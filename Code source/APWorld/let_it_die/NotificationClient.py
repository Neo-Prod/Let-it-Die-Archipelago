"""Independent, ephemeral AP item-message presentation. Never grants items."""
import asyncio
from collections import deque
from copy import deepcopy
from pathlib import Path
import os
import tempfile

from .NotificationWire import ACK, REQUEST, SIZE, MAX_UNITS, Message, decode
from .NotificationLayout import display_pages

REQUEST_FILE = 'notification_request.bin'
ACK_FILE = 'notification_ack.bin'
MAX_PENDING = 256


def item_message_text(context, packet):
    message_type = packet.get('type')
    if message_type not in ('ItemSend', 'ItemCheat'):
        return None
    item = packet.get('item')
    if hasattr(item, 'player'):
        sender = item.player
    elif isinstance(item, (tuple, list)) and len(item) == 4:
        sender = item[2]
    else:
        return None
    receiver = packet.get('receiving')
    if type(sender) is not int or type(receiver) is not int:
        return None
    if message_type == 'ItemCheat':
        # Server console/getitem messages are broadcast across teams.
        if packet.get('team') != getattr(context, 'team', None) or not context.slot_concerns_self(receiver):
            return None
    elif not (context.slot_concerns_self(sender) or context.slot_concerns_self(receiver)):
        return None
    data = packet.get('data')
    if not isinstance(data, list):
        return None
    return context.rawjsontotextparser(deepcopy(data))


def atomic_write(path, data):
    # Runs on an asyncio executor worker, never on the game's owner thread.
    fd, name = tempfile.mkstemp(prefix='.notification-', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        try:
            os.unlink(name)
        except FileNotFoundError:
            pass


class NotificationService:
    def __init__(self, directory, context):
        context.validate()
        self.directory, self.context = Path(directory), context
        self.sequence = 0
        self.pending = None
        self.published = False

    def step(self, text):
        if self.pending is None:
            if text is None:
                return False
            candidate = Message(REQUEST, self.context, self.sequence + 1, text)
            candidate.encode()  # Validate before publishing or changing sequence.
            self.pending = candidate
            self.published = False
        if not self.published:
            atomic_write(self.directory / REQUEST_FILE, self.pending.encode())
            self.published = True
        try:
            with (self.directory / ACK_FILE).open('rb') as stream:
                raw = stream.read(SIZE + 1)
        except FileNotFoundError:
            return False
        ack = decode(raw)
        if ack.context != self.context or ack.kind != ACK or ack.sequence != self.pending.sequence:
            return False
        self.sequence, self.pending, self.published = self.pending.sequence, None, False
        return True


async def watch_notifications(ctx, log):
    from .KillCoinClient import delivery_inputs
    service = None
    previous = None
    while not ctx.exit_event.is_set():
        try:
            outbox = ctx._notification_outbox
            if not outbox:
                state = 'idle'
            else:
                state, inputs = delivery_inputs(ctx)
                if inputs is not None:
                    directory, context, _ = inputs
                    if service is None:
                        service = NotificationService(directory, context)
                    elif (service.directory, service.context) != (directory, context):
                        # Old-session presentation is ephemeral, never transfer across runs.
                        lost = len(outbox)
                        outbox.clear()
                        service = NotificationService(directory, context)
                        log(ctx.request.bridge_dir, f'AP NOTIFY: session changed; discarded={lost}; original AP messages remain in client log.')
                    if outbox:
                        accepted = await asyncio.to_thread(service.step, outbox[0])
                        if accepted:
                            outbox.popleft()
                            log(ctx.request.bridge_dir, f'AP NOTIFY: native queue accepted sequence={service.sequence}; rendered=unconfirmed')
                        state = 'waiting_popup_ack' if outbox else 'idle'
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            state = 'paused_' + type(exc).__name__
        if state != previous:
            log(ctx.request.bridge_dir, 'AP NOTIFY: state=' + state)
            previous = state
        await asyncio.sleep(.25)


def capture_notification(ctx, packet, log):
    """Called after normal CommonContext logging. Failure cannot affect receipts."""
    text = item_message_text(ctx, packet)
    if text is None:
        return
    if type(text) is not str or not text or '\0' in text or len(text.encode('utf-16-le')) // 2 > MAX_UNITS:
        log(ctx.request.bridge_dir, 'AP NOTIFY: text rejected; original AP message remains in client log.')
        return
    if not hasattr(ctx, '_notification_outbox'):
        ctx._notification_outbox = deque()
    pages = display_pages(text)
    if not pages:
        log(ctx.request.bridge_dir, 'AP NOTIFY: blank text omitted; original AP message remains in client log.')
        return
    if len(ctx._notification_outbox) + len(pages) > MAX_PENDING:
        log(ctx.request.bridge_dir, 'AP NOTIFY: queue full; popup omitted; original AP message remains in client log.')
        return
    # Reserve the complete message before adding pages; never enqueue half of it.
    ctx._notification_outbox.extend(pages)
    task = getattr(ctx, '_notification_task', None)
    if task is None or task.done():
        ctx._notification_task = asyncio.create_task(watch_notifications(ctx, log), name='LET IT DIE item notifications')


async def stop_notification_task(ctx):
    task = getattr(ctx, '_notification_task', None)
    if task is not None:
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass
        ctx._notification_task = None
