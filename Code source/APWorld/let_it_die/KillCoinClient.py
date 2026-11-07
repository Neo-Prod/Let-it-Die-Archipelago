"""Independent currency task; not enabled by Client until native integration exists."""
import asyncio
import os
from pathlib import Path
import tempfile

from .CheckWire import Context
from .KillCoinDelivery import next_grant, accept_ack, read_cursor, write_cursor, prefix_digest
from .KillCoinWire import ACK, SIZE, decode

GRANT_FILE = "kill_coin_grant.bin"
ACK_FILE = "kill_coin_ack.bin"
CURSOR_FILE = "kill_coin_applied.json"


def read_frame(path):
    with Path(path).open("rb") as stream:
        return decode(stream.read(SIZE + 1))


def publish_grant(directory, frame):
    """Fixed bridge filename only; never derive a game-save path."""
    data = frame.encode()
    fd, temporary = tempfile.mkstemp(prefix=".kill-coins-", suffix=".tmp", dir=directory)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, Path(directory) / GRANT_FILE)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


class CoinService:
    """Serial polling by one task; caller owns synchronized snapshot readiness."""
    def __init__(self, directory, context):
        context.validate()
        self.directory = Path(directory)
        self.context = context
        self.loaded = False
        self.cursor = None
        self.pending = None
        self.pending_prefix = None
        self.published = False

    def step(self, snapshot):
        if (snapshot.identity, snapshot.config) != (self.context.run.hex(), self.context.config.hex()):
            raise ValueError("KC snapshot/native slot mismatch")
        if not self.loaded:
            cursor = read_cursor(self.directory / CURSOR_FILE, snapshot)
            # Also checks restored native run/config before any publication.
            next_grant(snapshot, self.context, cursor)
            self.cursor, self.loaded = cursor, True
        expected = next_grant(snapshot, self.context, self.cursor)
        if self.pending is not None and (self.pending != expected or self.pending_prefix !=
                prefix_digest(snapshot.history[:self.pending.index + 1])):
            raise ValueError("KC outstanding history changed")
        if expected is None:
            return "caught_up"
        if self.pending is None:
            self.pending, self.published = expected, False
            self.pending_prefix = prefix_digest(snapshot.history[:expected.index + 1])
        if not self.published:
            publish_grant(self.directory, self.pending)
            self.published = True
        try:
            ack = read_frame(self.directory / ACK_FILE)
        except FileNotFoundError:
            return "waiting_native_ack"
        if ack.context != self.context:
            return "waiting_native_ack"
        if ack.kind != ACK:
            raise ValueError("KC outcome file is not an ACK")
        if ack.index < self.pending.index:
            return "waiting_native_ack"
        cursor = accept_ack(snapshot, self.pending, ack.encode(), self.cursor)
        # Do not change memory or publish the next grant if durable publication fails.
        write_cursor(self.directory / CURSOR_FILE, cursor, snapshot)
        self.cursor, self.pending, self.published = cursor, None, False
        return "applied"


def delivery_inputs(ctx):
    """Use existing verified handshake and receipt barrier; never change either."""
    from .CharacterTransport import read_message, OFFER_FILE, GRANT
    if ctx.server is None or not ctx.slot or not ctx.prepared_slot_key:
        return "waiting_server", None
    if not ctx.character_history_ready or ctx.character_receipt_error is not None:
        return "waiting_full_history", None
    journal = ctx.character_receipts
    if journal is None or journal.snapshot is None:
        return "waiting_receipts", None
    native = ctx.native_character_transport
    if native is None:
        return "waiting_native_transport", None
    directory = ctx.request.bridge_dir / "slots" / ctx.prepared_slot_key
    if native.directory != directory:
        return "waiting_native_slot", None
    grant = read_message(directory / OFFER_FILE)
    if grant.kind != GRANT or not native.matches(grant):
        return "waiting_verified_native_grant", None
    context = Context(grant.pid, grant.creation_time, grant.nonce, grant.identity,
                      grant.config, grant.writer_pid, grant.writer)
    return "ready", (directory, context, journal.snapshot)


async def watch_coins(ctx, log):
    service = None
    previous = None
    while not ctx.exit_event.is_set():
        try:
            state, inputs = delivery_inputs(ctx)
            if inputs is not None:
                directory, context, snapshot = inputs
                if service is None or (service.directory, service.context) != (directory, context):
                    service = CoinService(directory, context)
                state = service.step(snapshot)
                if state == "applied":
                    applied = decode(service.cursor.evidence)
                    log(ctx.request.bridge_dir,
                        f"AP KC: applied index={applied.index} amount={applied.amount} "
                        f"wallet={applied.before}->{applied.after} status={applied.status}; "
                        "evidence=native_ack; game_save=unconfirmed")
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            state = "paused_" + type(exc).__name__
        if state != previous:
            log(ctx.request.bridge_dir, "AP KC: state=" + state)
            previous = state
        await asyncio.sleep(.25)


def ensure_coin_task(ctx, log):
    """Future activation point; failures never propagate into connection setup."""
    try:
        task = getattr(ctx, "_coin_task", None)
        if task is None:
            ctx._coin_task = asyncio.create_task(watch_coins(ctx, log), name="LET IT DIE KC delivery")
    except Exception as exc:
        log(ctx.request.bridge_dir, "AP KC: setup paused_" + type(exc).__name__)


async def stop_coin_task(ctx):
    task = getattr(ctx, "_coin_task", None)
    if task is not None:
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass
        ctx._coin_task = None
