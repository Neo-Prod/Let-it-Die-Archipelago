"""Momoka payment receipts -> durable AP checks; no game-save or wallet writes.

Caller supplies an authenticated native Context, immutable generated offers,
and the current slot's bridge directory. A checksum is corruption detection,
not independent authentication. Native producer must prove scoped debit and
handler success, suppress only its AP grant and retain receipt until ACK.
AP purchase persistence is independent of the game's usual wallet Save.
The SKU identifies the vanilla shop carrier, NEVER the AP reward to grant.
This service emits LocationChecks only. ReceivedItems and gameplay delivery
remain on the existing login/actual-waiting-room-return entry dispatcher.
Buying while already in the waiting room must NOT open a new delivery entry.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
from dataclasses import dataclass

from .CheckWire import Context, integer, nonzero
from .CheckInbox import atomic, unique_object

MAGIC = b"LIDSHOP3"
SIZE = 512
PAYMENT, RECEIVED = 1, 2
MAX_OFFERS = 4096
MAX_BYTES = 8 * 1024 * 1024
STATE_SIZE = 704
STATE = 3
EVENT_FILE = "shop_native_payment.bin"
STATE_FILE = "shop_client_state.bin"


def purchase_state(context, catalogue, bought):
    """Current authenticated process + catalogue bitmap, not a native Save."""
    context.validate()
    nonzero(catalogue, 32)
    if type(bought) not in (tuple, list, set, frozenset) or len(bought) > MAX_OFFERS:
        raise ValueError("shop bought bitmap type/capacity")
    raw = bytearray(STATE_SIZE)
    raw[:8] = MAGIC
    struct.pack_into("<IIIIQ", raw, 8, 1, STATE, context.pid, context.client_pid, context.birth)
    raw[32:48], raw[48:64] = context.nonce, context.writer
    raw[64:96], raw[96:128], raw[128:160] = context.run, context.config, catalogue
    for offer in bought:
        integer(offer, 0, MAX_OFFERS - 1)
        raw[160 + offer // 8] |= 1 << (offer % 8)
    value = 2166136261
    for i, byte in enumerate(raw):
        if not 672 <= i < 676:
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    struct.pack_into("<I", raw, 672, value)
    return bytes(raw)


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                     allow_nan=False).encode("ascii")).hexdigest()


def _skill(value):
    if (type(value) is not str or not 1 <= len(value) <= 128
            or any(not 33 <= ord(c) <= 126 for c in value)):
        raise ValueError("shop SKU format")
    return value


def _checksum(raw):
    value = 2166136261
    for i, byte in enumerate(raw):
        if not 476 <= i < 480:
            value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


@dataclass(frozen=True)
class Offer:
    """Native purchase identity -> AP location, not an immediate reward."""
    offer: int
    location: int
    skill: str

    def validate(self):
        integer(self.offer, 0, MAX_OFFERS - 1)
        integer(self.location, 1, 2**53 - 1)
        _skill(self.skill)


def catalogue_fingerprint(offers):
    if type(offers) is not tuple or not 1 <= len(offers) <= MAX_OFFERS:
        raise ValueError("shop catalogue capacity/type")
    for offer in offers:
        if type(offer) is not Offer:
            raise ValueError("shop catalogue row type")
        offer.validate()
    for field in ("offer", "location", "skill"):
        if len({getattr(o, field) for o in offers}) != len(offers):
            raise ValueError("duplicate shop catalogue identity")
    rows = [[o.offer, o.location, o.skill] for o in sorted(offers, key=lambda o: o.offer)]
    return bytes.fromhex(_digest(rows))


def parse_catalogue(packet):
    """Strict generated descriptor; server/native handshake supplies identity.

    Missing descriptor is the unchanged legacy run. A checksum does not prove
    the SKU's native unlock conditions or that the runtime producer is active.
    """
    if packet is None:
        return ()
    if (type(packet) is not dict or set(packet) != {"schema", "offers", "catalogue_sha256"}
            or type(packet["schema"]) is not int or packet["schema"] != 1
            or type(packet["offers"]) is not list or not 1 <= len(packet["offers"]) <= MAX_OFFERS):
        raise ValueError("generated shop catalogue schema")
    offers = []
    for row in packet["offers"]:
        if type(row) is not dict or set(row) != {"offer", "location", "skill"}:
            raise ValueError("generated shop offer schema")
        offers.append(Offer(**row))
    offers = tuple(offers)
    expected = catalogue_fingerprint(offers).hex()
    if type(packet["catalogue_sha256"]) is not str or packet["catalogue_sha256"] != expected:
        raise ValueError("generated shop catalogue checksum")
    return offers


@dataclass(frozen=True)
class Payment:
    context: Context
    catalogue: bytes
    sequence: int
    offer: int
    skill: str = ""
    price: int = 0
    before_free: int = 0
    after_free: int = 0
    before_paid: int = 0
    after_paid: int = 0
    debit_result: int = 0
    handler_result: int = 0
    kind: int = PAYMENT

    def validate(self):
        if type(self.context) is not Context:
            raise ValueError("shop receipt context type")
        self.context.validate()
        nonzero(self.catalogue, 32)
        integer(self.sequence, 1, 2**64 - 1)
        integer(self.offer, 0, MAX_OFFERS - 1)
        integer(self.kind, PAYMENT, RECEIVED)
        for field in ("price", "before_free", "after_free", "before_paid", "after_paid"):
            integer(getattr(self, field), 0, 0x7FFFFFFF)
        integer(self.debit_result, 0, 1)
        integer(self.handler_result, 0, 1)
        if self.kind == RECEIVED:
            if self.skill or any(getattr(self, f) for f in (
                    "price", "before_free", "after_free", "before_paid", "after_paid",
                    "debit_result", "handler_result")):
                raise ValueError("shop ACK payload must be empty")
            return
        _skill(self.skill)
        if (not self.price or self.before_free < self.price
                or self.after_free != self.before_free - self.price
                or self.after_paid != self.before_paid
                or self.debit_result != 1 or self.handler_result != 1):
            raise ValueError("shop payment not completed")

    def encode(self):
        self.validate()
        raw = bytearray(SIZE)
        raw[:8] = MAGIC
        c = self.context
        struct.pack_into("<IIIIQ", raw, 8, 1, self.kind, c.pid, c.client_pid, c.birth)
        raw[32:48], raw[48:64] = c.nonce, c.writer
        raw[64:96], raw[96:128], raw[128:160] = c.run, c.config, self.catalogue
        struct.pack_into("<QIIiiiiBBH", raw, 160, self.sequence, self.offer, self.price,
                         self.before_free, self.after_free, self.before_paid, self.after_paid,
                         self.debit_result, self.handler_result, len(self.skill))
        raw[196:196 + len(self.skill) * 2] = self.skill.encode("utf-16-le")
        struct.pack_into("<I", raw, 476, _checksum(raw))
        return bytes(raw)

    @classmethod
    def decode(cls, raw):
        if (type(raw) is not bytes or len(raw) != SIZE or raw[:8] != MAGIC
                or struct.unpack_from("<I", raw, 8)[0] != 1
                or struct.unpack_from("<I", raw, 476)[0] != _checksum(raw)):
            raise ValueError("shop receipt size/version/checksum")
        _, kind, pid, client_pid, birth = struct.unpack_from("<IIIIQ", raw, 8)
        c = Context(pid, birth, raw[32:48], raw[64:96], raw[96:128], client_pid, raw[48:64])
        sequence, offer, price, before, after, paid, paid_after, debit, handler, units = struct.unpack_from(
            "<QIIiiiiBBH", raw, 160)
        if units > 128 or any(raw[196 + units * 2:476] + raw[480:]):
            raise ValueError("shop receipt text/padding")
        skill = raw[196:196 + units * 2].decode("utf-16-le")
        result = cls(c, raw[128:160], sequence, offer, skill, price, before, after,
                     paid, paid_after, debit, handler, kind)
        result.validate()
        if result.encode() != raw:
            raise ValueError("noncanonical shop receipt")
        return result

    def acknowledgement(self):
        return Payment(self.context, self.catalogue, self.sequence, self.offer, kind=RECEIVED).encode()


class ShopPurchaseService:
    """One owning client task. Server confirmation, not send(), completes delivery.

    No automatic task activation or paths derived from native receipt contents.
    Existing authenticated handshake and generated catalogue must precede setup.
    """
    def __init__(self, directory, context, offers):
        context.validate()
        self.directory, self.context = Path(directory), context
        self.catalogue = catalogue_fingerprint(offers)
        self.offers = {o.offer: o for o in offers}
        self.ids = frozenset(o.location for o in offers)
        self.path = self.directory / "shop_purchase_outbox.json"
        self.ack_path = self.directory / "shop_client_received.bin"
        self.binding = _digest([context.run.hex(), context.config.hex(), self.catalogue.hex()])
        self.last_send = None
        self.last_event_file = None
        self.last_state = None
        self.state = {"schema": 1, "binding": self.binding, "sessions": [], "receipts": {}, "confirmed": []}
        if self.path.exists():
            with self.path.open("rb") as stream:
                raw = stream.read(MAX_BYTES + 1)
            if len(raw) > MAX_BYTES:
                raise ValueError("shop journal capacity")
            wrapper = json.loads(raw, object_pairs_hook=unique_object)
            if type(wrapper) is not dict or set(wrapper) != {"sha256", "state"}:
                raise ValueError("shop journal wrapper")
            if wrapper["sha256"] != _digest(wrapper["state"]):
                raise ValueError("shop journal checksum")
            self._validate_state(wrapper["state"])
            self.state = wrapper["state"]

    def _match(self, event, current):
        if (event.kind != PAYMENT or event.catalogue != self.catalogue
                or event.context.run != self.context.run or event.context.config != self.context.config
                or (current and event.context != self.context)):
            raise ValueError("foreign shop receipt")
        offer = self.offers.get(event.offer)
        if offer is None or offer.skill != event.skill:
            raise ValueError("unknown shop offer/SKU")
        return offer

    def _validate_state(self, state):
        if (type(state) is not dict or set(state) != {"schema", "binding", "sessions", "receipts", "confirmed"}
                or type(state["schema"]) is not int or state["schema"] != 1
                or state["binding"] != self.binding or type(state["receipts"]) is not dict
                or len(state["receipts"]) > len(self.offers)
                or type(state["confirmed"]) is not list):
            raise ValueError("shop journal binding/shape")
        sessions = state["sessions"]
        if (type(sessions) is not list or len(sessions) > 32
                or any(type(key) is not str or len(key) != 224 for key in sessions)
                or len(set(sessions)) != len(sessions)):
            raise ValueError("shop producer session registry")
        for key in sessions:
            raw = bytes.fromhex(key)
            if (len(raw) != 112 or raw.hex() != key or raw[48:80] != self.context.run
                    or raw[80:112] != self.context.config
                    or not all(struct.unpack_from('<IQI', raw))
                    or not any(raw[16:32]) or not any(raw[32:48])):
                raise ValueError("shop registered producer identity")
        self._checked(state["confirmed"])
        if state["confirmed"] != sorted(set(state["confirmed"])):
            raise ValueError("shop confirmations must be canonical")
        sequences = set()
        for key, value in state["receipts"].items():
            if type(value) is not str or len(value) != SIZE * 2:
                raise ValueError("shop persisted receipt format")
            event = Payment.decode(bytes.fromhex(value))
            if key != str(event.offer) or value != event.encode().hex():
                raise ValueError("shop persisted offer/key")
            self._match(event, current=False)
            sequence_key = (event.context.key(), event.sequence)
            if sequence_key in sequences:
                raise ValueError("shop producer sequence reused")
            sequences.add(sequence_key)

    def _checked(self, checked):
        if type(checked) not in (list, tuple, set, frozenset) or len(checked) > MAX_OFFERS:
            raise ValueError("shop server confirmation type/capacity")
        for location in checked:
            integer(location, 1, 2**53 - 1)
        if len(set(checked)) != len(checked) or not set(checked) <= self.ids:
            raise ValueError("foreign/duplicate shop server confirmation")
        return set(checked)

    def _commit(self, state):
        self._validate_state(state)
        raw = json.dumps({"sha256": _digest(state), "state": state}, sort_keys=True,
                         separators=(",", ":")).encode("ascii")
        if len(raw) > MAX_BYTES:
            raise ValueError("shop journal capacity")
        atomic(self.path, raw)
        self.state = state

    def ingest(self, raw):
        return self._ingest(raw, recovered=False)

    def _register_session(self):
        key = self.context.key().hex()
        if key not in self.state["sessions"]:
            self._commit({**self.state, "sessions": (self.state["sessions"] + [key])[-32:]})

    def _ingest(self, raw, recovered):
        event = Payment.decode(raw)
        offer = self._match(event, current=not recovered)
        if recovered:
            if event.context.key().hex() not in self.state["sessions"]:
                raise ValueError("unregistered old shop producer")
        else:
            self._register_session()
        old = self.state["receipts"].get(str(event.offer))
        if old is not None and old != raw.hex():
            raise ValueError("second payment for same shop offer")
        if old is None:
            if offer.location in self.state["confirmed"]:
                raise ValueError("new payment for already checked shop offer")
            state = {**self.state, "receipts": {**self.state["receipts"], str(event.offer): raw.hex()}}
            self._commit(state)
        # Durable journal precedes native receipt ACK. ACK is NOT server ACK or
        # proof of native wallet persistence. Failed ACK safely repeats ingest.
        self.publish_state()
        atomic(self.ack_path, event.acknowledgement())
        return old is None

    def confirm(self, checked):
        checked = self._checked(checked)
        previous = set(self.state["confirmed"])
        if not previous <= checked:
            raise ValueError("server shop confirmation regressed")
        if checked != previous:
            self._commit({**self.state, "confirmed": sorted(checked)})

    @property
    def sold_out(self):
        # Local durable receipts close the resend window before server ACK.
        return frozenset(self.state["confirmed"]) | frozenset(
            self.offers[int(key)].location for key in self.state["receipts"])

    @property
    def pending(self):
        return tuple(sorted(self.sold_out - set(self.state["confirmed"])))

    def publish_state(self):
        # Register the verified native handshake BEFORE publishing buy-ready
        # state. A later client can recover a persisted, not-yet-ACKed event
        # from that known producer without adopting arbitrary old processes.
        self._register_session()
        sold_out = self.sold_out
        bought = tuple(o.offer for o in self.offers.values() if o.location in sold_out)
        raw = purchase_state(self.context, self.catalogue, bought)
        if raw != self.last_state:
            atomic(self.directory / STATE_FILE, raw)
            self.last_state = raw

    def poll(self):
        """One bounded receipt; unchanged files do not rewrite journal or ACK."""
        path = self.directory / EVENT_FILE
        try:
            info = path.stat()
        except FileNotFoundError:
            return False
        signature = (info.st_mtime_ns, info.st_size)
        if signature == self.last_event_file:
            return False
        with path.open("rb") as stream:
            raw = stream.read(SIZE + 1)
        event = Payment.decode(raw)
        if event.context != self.context:
            if event.context.key().hex() not in self.state["sessions"]:
                return False
            changed = self._ingest(raw, recovered=True)
        else:
            changed = self.ingest(raw)
        self.last_event_file = signature
        return changed

    async def step(self, checked, now, send_msgs):
        """Authenticated worker supplies this shop's checked IDs only."""
        self.confirm(checked)
        self._register_session()
        self.poll()
        self.publish_state()  # Recover old persisted event before new buys.
        return await self.publish(now, send_msgs)

    async def publish(self, now, send_msgs):
        if type(now) not in (float, int) or not 0 <= now < float("inf"):
            raise ValueError("shop publish clock")
        if not self.pending or (self.last_send is not None and now - self.last_send < 5):
            return False
        await send_msgs([{"cmd": "LocationChecks", "locations": list(self.pending[:256])}])
        self.last_send = now
        return True
