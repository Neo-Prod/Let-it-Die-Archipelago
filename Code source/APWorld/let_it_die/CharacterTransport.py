"""Fixed-size character-only bridge. No game calls or save-file editing.

Checksums detect corruption, not hostile local code. A SAVED packet is a native
save report, never by itself evidence of an independently validated game test.
"""
from __future__ import annotations

from collections import OrderedDict
from dataclasses import dataclass, replace
import json
import os
from pathlib import Path
import struct
import tempfile
import secrets
from typing import Callable

from .CharacterProgression import MAX_HISTORY, PROGRESSIVES, ReceiptJournal, ReceiptSnapshot
from .RuntimeControl import RebuildControl

GAME_SHA = "f33e91e6e8d58566bfabad8d01cd0486586a0d43fe1c58e65fc6485a417bddfd"
GAME_DIGEST = bytes.fromhex(GAME_SHA)
MAGIC = b"LIDAPC01"
OFFER, QUIESCED, TARGET, SAVED, GRANT = 1, 2, 3, 4, 5
OFFER_FILE = "character_native_offer.bin"
QUIESCED_FILE = "character_client_quiesced.bin"
TARGET_FILE = "character_target.bin"
SAVED_FILE = "character_native_saved.bin"
APPLIED_FILE = "character_applied.json"
ZERO32 = bytes(32)
ZERO_COUNTS = (0,) * 9


class TransportError(ValueError):
    pass


def checksum(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


@dataclass(frozen=True)
class Message:
    kind: int
    pid: int
    nonce: bytes
    creation_time: int
    writer: bytes = bytes(16)
    writer_pid: int = 0
    identity: bytes = ZERO32
    config: bytes = ZERO32
    history_length: int = 0
    counts: tuple[int, ...] = ZERO_COUNTS
    generation: bytes = ZERO32
    sequence: int = 0
    status: int = 0

    def validate(self) -> None:
        if type(self.kind) is not int or self.kind not in (OFFER, QUIESCED, TARGET, SAVED, GRANT):
            raise TransportError("Invalid message kind")
        if type(self.pid) is not int or not 1 <= self.pid <= 0xffffffff:
            raise TransportError("Invalid parent PID")
        if type(self.nonce) is not bytes or len(self.nonce) != 16 or not any(self.nonce):
            raise TransportError("Invalid session nonce")
        if type(self.creation_time) is not int or not 0 < self.creation_time <= 0xffffffffffffffff:
            raise TransportError("Invalid parent creation time")
        if type(self.writer) is not bytes or len(self.writer) != 16 or type(self.writer_pid) is not int:
            raise TransportError("Invalid runtime writer")
        for value in (self.identity, self.config, self.generation):
            if type(value) is not bytes or len(value) != 32:
                raise TransportError("Invalid bridge digest")
        if type(self.history_length) is not int or not 0 <= self.history_length <= MAX_HISTORY:
            raise TransportError("Invalid history length")
        if (type(self.counts) is not tuple or len(self.counts) != 9
                or any(type(n) is not int or not 0 <= n <= MAX_HISTORY for n in self.counts)
                or sum(self.counts) > self.history_length):
            raise TransportError("Invalid character counts")
        if type(self.sequence) is not int or not 0 <= self.sequence <= 0xffffffffffffffff:
            raise TransportError("Invalid bridge sequence")
        if type(self.status) is not int or self.status != (1 if self.kind == SAVED else 0):
            raise TransportError("Invalid bridge status")
        if self.kind == OFFER:
            if self.identity != ZERO32 or self.config != ZERO32 or any(self.writer) or self.writer_pid:
                raise TransportError("Offer must not bind unconfirmed client identity")
        elif self.identity == ZERO32 or self.config == ZERO32 or not any(self.writer) or not 0 < self.writer_pid <= 0xffffffff:
            raise TransportError("Missing client identity/configuration")
        if self.kind in (OFFER, QUIESCED, GRANT):
            if self.history_length or self.counts != ZERO_COUNTS or self.generation != ZERO32 or self.sequence:
                raise TransportError("Handshake cannot report item delivery")
        elif self.generation == ZERO32 or not self.sequence:
            raise TransportError("Target/save report requires a receipt generation")

    def encode(self) -> bytes:
        self.validate()
        data = bytearray(256)
        data[:8] = MAGIC
        struct.pack_into("<IIII", data, 8, 1, self.kind, self.pid, self.writer_pid)
        data[24:40], data[40:72], data[72:104] = self.nonce, self.identity, self.config
        struct.pack_into("<I9I", data, 104, self.history_length, *self.counts)
        data[144:176] = self.generation
        struct.pack_into("<QII", data, 176, self.sequence, self.status, 0)
        data[192:224] = GAME_DIGEST
        data[228:244] = self.writer
        struct.pack_into("<Q", data, 244, self.creation_time)
        struct.pack_into("<I", data, 224, checksum(data[:224] + data[228:]))
        return bytes(data)

    @classmethod
    def decode(cls, raw: bytes) -> Message:
        if (len(raw) != 256 or raw[:8] != MAGIC or struct.unpack_from("<I", raw, 8)[0] != 1
                or any(raw[188:192]) or any(raw[252:])
                or raw[192:224] != GAME_DIGEST
                or struct.unpack_from("<I", raw, 224)[0] != checksum(raw[:224] + raw[228:])):
            raise TransportError("Invalid bridge frame/version/build/checksum")
        kind, pid, writer_pid = struct.unpack_from("<III", raw, 12)
        history_length, *counts = struct.unpack_from("<I9I", raw, 104)
        sequence, status = struct.unpack_from("<QI", raw, 176)
        result = cls(kind=kind, pid=pid, nonce=raw[24:40], creation_time=struct.unpack_from("<Q", raw, 244)[0],
                     writer=raw[228:244], writer_pid=writer_pid, identity=raw[40:72], config=raw[72:104],
                     history_length=history_length, counts=tuple(counts), generation=raw[144:176],
                     sequence=sequence, status=status)
        result.validate()
        return result


def atomic_write(path: Path, data: bytes) -> None:
    """Only bridge-owned filenames; refuse accidental writes to a game save."""
    path = Path(path)
    if path.name not in {OFFER_FILE, QUIESCED_FILE, TARGET_FILE, SAVED_FILE, APPLIED_FILE, 'decal_target.bin', 'decal_applied.json', 'character_reload_source.bin'}:
        raise TransportError("Not a character bridge filename")
    fd, temporary = tempfile.mkstemp(prefix=".character-bridge-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def read_message(path: Path) -> Message:
    with Path(path).open("rb") as stream:
        return Message.decode(stream.read(257))


class NativeTransport:
    """Called serially by one client task. Requires actual watcher completion."""
    def __init__(self, directory: Path, pid: int, control: RebuildControl,
                 verify_game: Callable[[], bool], *, blueprints: bool = False, reload_source: bool = False):
        if type(blueprints) is not bool:
            raise TransportError('Invalid blueprint transport mode')
        if type(reload_source) is not bool:
            raise TransportError('Invalid reload source transport mode')
        self.blueprints = blueprints
        self.reload_source = reload_source
        self.directory, self.pid, self.control = Path(directory), pid, control
        self.verify_game = verify_game
        self.writer = secrets.token_bytes(16)
        self.writer_pid = os.getpid()
        self.offer: Message | None = None
        self.identity = self.config = ZERO32
        self.quiesced_published = False
        self.sequence = self.saved_sequence = 0
        self.last_generation: bytes | None = None
        self.pending: OrderedDict[int, Message] = OrderedDict()

    def matches(self, message: Message) -> bool:
        return bool(self.offer and message.pid == self.pid and message.nonce == self.offer.nonce
                    and message.creation_time == self.control.creation_time
                    and message.writer == self.writer and message.writer_pid == self.writer_pid
                    and message.identity == self.identity and message.config == self.config)

    def step(self, journal: ReceiptJournal | None, history_ready: bool) -> str:
        if journal is None:
            return "receipt_unavailable"
        try:
            offer = read_message(self.directory / OFFER_FILE)
        except FileNotFoundError:
            return "legacy"
        if offer.kind not in (OFFER, GRANT) or offer.pid != self.pid or offer.creation_time != self.control.creation_time:
            raise TransportError("Native offer does not match parent process")
        identity, config = bytes.fromhex(journal.identity), bytes.fromhex(journal.config.fingerprint)
        if self.offer is None or offer.nonce != self.offer.nonce:
            if not self.verify_game():
                raise TransportError("Current executable hash is unsupported")
            self.offer, self.identity, self.config = offer, identity, config
            self.quiesced_published = False
            self.sequence = self.saved_sequence = 0
            self.pending.clear()
            self.last_generation = None
        elif (identity, config) != (self.identity, self.config):
            raise TransportError("Client identity changed within a native session")
        self.control.request_stop()
        if self.control.unsafe:
            return "unsafe_remote_call"
        if not self.control.quiesced:
            return "stopping_legacy_watcher"
        if offer.kind == GRANT:
            if not self.matches(offer):
                return "another_runtime_client_owns_delivery"
        else:
            if not self.quiesced_published:
                reply = replace(offer, kind=QUIESCED, identity=self.identity, config=self.config,
                                writer=self.writer, writer_pid=self.writer_pid)
                if self.reload_source:
                    from .ReloadSource import publish_source_proof
                    publish_source_proof(self.directory, reply, atomic_write)
                atomic_write(self.directory / QUIESCED_FILE, reply.encode())
                self.quiesced_published = True
            return "waiting_native_grant"
        snapshot = journal.snapshot
        if not history_ready or snapshot is None:
            return "waiting_full_history"
        generation = bytes.fromhex(snapshot.generation)
        if generation != self.last_generation:
            if self.sequence == 0xffffffffffffffff:
                raise TransportError("Bridge sequence exhausted")
            target = Message(kind=TARGET, pid=self.pid, nonce=offer.nonce, creation_time=offer.creation_time,
                             writer=self.writer, writer_pid=self.writer_pid, identity=self.identity, config=self.config,
                             history_length=len(snapshot.history), counts=snapshot.counts,
                             generation=generation, sequence=self.sequence + 1)
            if self.blueprints:
                from .BlueprintPublication import publish_blueprint_target
                publish_blueprint_target(self.directory, snapshot, target)
            else:
                atomic_write(self.directory / TARGET_FILE, target.encode())
            self.sequence = target.sequence
            self.last_generation = generation
            self.pending[target.sequence] = target
            while len(self.pending) > 128:
                self.pending.popitem(last=False)
        try:
            saved = read_message(self.directory / SAVED_FILE)
        except FileNotFoundError:
            return self._delivery_state()
        self._accept_saved(saved, snapshot)
        return self._delivery_state()

    def _delivery_state(self) -> str:
        if self.saved_sequence:
            return "native_save_reported_pending" if self.pending else "native_save_reported"
        return "target_published"

    def _accept_saved(self, saved: Message, snapshot: ReceiptSnapshot) -> bool:
        if saved.kind != SAVED or not self.matches(saved) or saved.sequence <= self.saved_sequence:
            return False
        target = self.pending.get(saved.sequence)
        if target is None or replace(saved, kind=TARGET, status=0) != target:
            return False
        if saved.history_length > len(snapshot.history):
            return False
        prefix = ReceiptSnapshot(snapshot.identity, snapshot.config, snapshot.history[:saved.history_length])
        if prefix.generation != saved.generation.hex() or prefix.counts != saved.counts:
            return False
        if self.blueprints:
            from .BlueprintSavedReport import read_saved_report, write_saved_report
            try:
                blueprint_report = read_saved_report(self.directory, snapshot, saved)
            except FileNotFoundError:
                return False
            from .DecalSavedReport import read_saved_report as read_decals, write_saved_report as write_decals
            try:
                decal_report = read_decals(self.directory, snapshot, saved)
            except FileNotFoundError:
                return False
            # Validate BOTH domains before publishing either client report.
            write_decals(self.directory, decal_report)
            write_saved_report(self.directory, blueprint_report)
        doc = {"schema": 1, "status": "native_save_reported", "scope": "character_progressives_only",
               "item_ids": [item_id for item_id, _, _ in PROGRESSIVES],
               "identity": snapshot.identity, "config": snapshot.config,
               "receipt_generation": prefix.generation, "history_length": len(prefix.history),
               "counts": saved.counts, "game_sha256": GAME_SHA}
        atomic_write(self.directory / APPLIED_FILE,
                     (json.dumps(doc, sort_keys=True) + "\n").encode("ascii"))
        self.saved_sequence = saved.sequence
        for sequence in list(self.pending):
            if sequence <= saved.sequence:
                del self.pending[sequence]
        return True
