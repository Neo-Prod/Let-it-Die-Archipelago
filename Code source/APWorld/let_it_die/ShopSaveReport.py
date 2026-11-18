"""Passive ShopSanity BRG readback; no save writes, payment, network or ACK.

The host must authenticate the expected snapshot against the current request,
run/config/catalogue and native payment scope, and choose the verified AP path.
Matching these bytes does not validate every native gameplay/save domain.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct

from .ReloadValidation import MAX_FILE, SnapshotError, decode_brg

MARKER_KEY = "lid_ap_shop_v2"
MARKER_MAGIC = b"LIDSHOP2"
MAX_OFFERS = 4096
MAX_MARKER = 116 + MAX_OFFERS


@dataclass(frozen=True)
class ShopMarker:
    binding: bytes  # full run/config/catalogue digests, in this order
    revision: int
    states: bytes  # 0 empty, 1 paid, 2 uncertain; no transient PAYING state

    @property
    def paid(self) -> tuple[int, ...]:
        return tuple(i for i, state in enumerate(self.states) if state == 1)

    @property
    def uncertain(self) -> tuple[int, ...]:
        return tuple(i for i, state in enumerate(self.states) if state == 2)


def decode_marker(payload: bytes) -> ShopMarker:
    if (type(payload) is not bytes or not 117 <= len(payload) <= MAX_MARKER
            or payload[:8] != MARKER_MAGIC):
        raise SnapshotError("shop marker size or version")
    revision, count = struct.unpack_from("<QI", payload, 104)
    states = payload[116:]
    if (not 1 <= count <= MAX_OFFERS or len(states) != count
            or any(state > 2 for state in states)
            or (not revision and 1 in states)):
        raise SnapshotError("shop marker state or catalogue size")
    return ShopMarker(payload[8:104], revision, states)


def marker_from_text(value: object) -> bytes:
    if (type(value) is not str or not 234 <= len(value) <= MAX_MARKER * 2
            or len(value) % 2 or any(c not in "0123456789ABCDEF" for c in value)):
        raise SnapshotError("shop marker text representation")
    payload = bytes.fromhex(value)
    decode_marker(payload)
    return payload


def _money(value: object) -> int:
    if type(value) is not int or not 0 <= value <= 0x7FFFFFFF:
        raise SnapshotError("shop wallet value")
    return value


def _uid(value: object) -> str:
    if type(value) not in (str, int):
        raise SnapshotError("shop save identity type")
    text = str(value)
    if not 1 <= len(text) <= 128 or any(c not in "0123456789" for c in text):
        raise SnapshotError("shop save identity format")
    return text


@dataclass(frozen=True)
class ShopSaveExpectation:
    marker: bytes
    uid: str
    free_money: int
    paid_money: int

    def validate(self) -> ShopMarker:
        marker = decode_marker(self.marker)
        if type(self.uid) is not str or _uid(self.uid) != self.uid:
            raise SnapshotError("expected shop identity")
        _money(self.free_money)
        _money(self.paid_money)
        return marker


@dataclass(frozen=True)
class ShopSaveProof:
    source_sha256: str
    marker: ShopMarker
    chunk_count: int

    @property
    def sendable_offers(self) -> tuple[int, ...]:
        # Same fail-closed recovery policy as the C ledger, not an AP send.
        return () if self.marker.uncertain else self.marker.paid


def verify_saved_bytes(raw: bytes, expected: ShopSaveExpectation) -> ShopSaveProof:
    if not isinstance(expected, ShopSaveExpectation):
        raise SnapshotError("expected shop snapshot missing")
    marker = expected.validate()
    root, chunks, _declared = decode_brg(raw)
    user, soul = root.get("user"), root.get("soul")
    if type(user) is not dict or type(soul) is not dict:
        raise SnapshotError("shop save identity domains")
    if _uid(user.get("uid")) != expected.uid or _uid(soul.get("uid")) != expected.uid:
        raise SnapshotError("shop save identity mismatch")
    if MARKER_KEY not in root:
        # Absence is NOT an empty purchase history and never authorizes replay.
        raise SnapshotError("shop marker absent from saved snapshot")
    if marker_from_text(root[MARKER_KEY]) != expected.marker:
        raise SnapshotError("saved shop marker differs from expected snapshot")
    if (_money(soul.get("free_money")) != expected.free_money
            or _money(soul.get("paid_money")) != expected.paid_money):
        raise SnapshotError("saved shop wallet differs from expected snapshot")
    return ShopSaveProof(hashlib.sha256(raw).hexdigest(), marker, chunks)


def read_saved_report(path: str | Path, expected: ShopSaveExpectation) -> ShopSaveProof:
    """Read one bounded file snapshot. Caller authenticates path/session first.

    Native file completion must precede this call; a missing, locked, changing
    or mismatching file is not a successful purchase and must not trigger debit.
    No sleep, polling loop, save mutation or persistent plaintext report here.
    """
    with Path(path).open("rb") as stream:
        raw = stream.read(MAX_FILE + 1)
    return verify_saved_bytes(raw, expected)
