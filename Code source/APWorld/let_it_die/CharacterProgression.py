"""Receipt bookkeeping for the nine character progressives.

Used by the editable client sources for receipts only; not installed yet.
Receipt persistence is NEVER proof of a game save. No process access, native
calls, save editing or connection changes here.
Source of IDs: Items.py STAT_ITEMS; source of amounts: server slot_data.
"""
from __future__ import annotations

from collections import Counter
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import tempfile
from typing import Iterable, Mapping, Any


# AP order, distinct from the native BodyLvl order (STM is native index 5).
PROGRESSIVES = (
    (7100001, "hp", "hp"), (7100002, "stm", "stm"),
    (7100003, "str", "str"), (7100004, "dex", "dex"),
    (7100005, "vit", "vit"), (7100006, "luk", "luck"),
    (7100007, "death_bag", "death_bag"),
    (7100008, "skill_slots", "skill_slots"),
    (7100009, "rage_gauge", "rage_gauge"),
)
MAX_HISTORY = 100_000
MAX_FILE_BYTES = 16 * 1024 * 1024
INT_MAX = 2_147_483_647


class ReceiptError(ValueError):
    pass


class IncompleteHistory(ReceiptError):
    """Wait for CommonClient Sync; do not lower the previously received target."""


def _integer(value: Any, label: str, minimum: int = 0, maximum: int = INT_MAX) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ReceiptError(f"Invalid integer: {label}")
    return value


def _digest(value: Any) -> str:
    data = json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True).encode("ascii")
    return hashlib.sha256(data).hexdigest()


@dataclass(frozen=True)
class CharacterConfig:
    # Each tuple is (starting value, increase per receipt, quantity cap).
    tracks: tuple[tuple[int, int, int], ...]

    def __post_init__(self) -> None:
        if type(self.tracks) is not tuple or len(self.tracks) != 9:
            raise ReceiptError("Expected nine character tracks")
        for track in self.tracks:
            if type(track) is not tuple or len(track) != 3:
                raise ReceiptError("Invalid character track")
            start, increase, cap = track
            for value in track:
                _integer(value, "track")
            if start + increase * cap > INT_MAX or cap >= INT_MAX:
                raise ReceiptError("Progressive target overflows native int32")
        if 1 + sum(track[2] for track in self.tracks) > INT_MAX:
            raise ReceiptError("Combined native character level overflows int32")

    @classmethod
    def from_slot_data(cls, slot_data: Mapping[str, Any]) -> CharacterConfig:
        tracks = []
        for _, starting, progressive in PROGRESSIVES:
            keys = (f"starting_{starting}", f"progressive_{progressive}_increase",
                    f"progressive_{progressive}_quantity")
            try:
                tracks.append(tuple(_integer(slot_data[key], key) for key in keys))
            except KeyError as exc:
                raise ReceiptError(f"Missing character configuration: {exc.args[0]}") from exc
        return cls(tuple(tracks))

    @property
    def fingerprint(self) -> str:
        return _digest(self.tracks)

    def targets(self, counts: tuple[int, ...]) -> tuple[int, ...]:
        if len(counts) != 9:
            raise ReceiptError("Expected nine receipt counts")
        return tuple(start + increase * min(_integer(count, "count"), cap)
                     for count, (start, increase, cap) in zip(counts, self.tracks))

    def native_levels(self, counts: tuple[int, ...]) -> tuple[int, ...]:
        """Pure layout conversion, not a write to the game's BodyLvl structure."""
        if len(counts) != 9:
            raise ReceiptError("Expected nine receipt counts")
        c = [min(_integer(n, "count"), t[2]) for n, t in zip(counts, self.tracks)]
        # lvl, hp, str, dex, vit, stm, luk, skill, bag, rage
        return (1 + sum(c), 1 + c[0], 1 + c[2], 1 + c[3], 1 + c[4],
                1 + c[1], 1 + c[5], c[7], c[6], c[8])


def receipt_identity(*, server: str, seed: str, team: int, slot: int) -> str:
    if not server or not seed or type(server) is not str or type(seed) is not str:
        raise ReceiptError("Missing AP receipt identity")
    _integer(team, "team")
    _integer(slot, "slot", 1)
    # Store only a fingerprint. This does not replace the existing slot/save key.
    return _digest((server, seed, team, slot))


def _entry(item: Any) -> tuple[int, int, int, int]:
    if hasattr(item, "item"):
        values = (item.item, item.location, item.player, item.flags)
    elif type(item) in (list, tuple) and len(item) == 4:
        values = tuple(item)
    else:
        raise ReceiptError("Invalid NetworkItem shape")
    item_id, location, player, flags = values
    _integer(item_id, "item", 1, (1 << 63) - 1)
    _integer(location, "location", -(1 << 63), (1 << 63) - 1)
    _integer(player, "player")
    _integer(flags, "flags")
    return values


@dataclass(frozen=True)
class ReceiptSnapshot:
    identity: str
    config: str
    history: tuple[tuple[int, int, int, int], ...]

    def __post_init__(self) -> None:
        for key in (self.identity, self.config):
            if type(key) is not str or len(key) != 64 or any(c not in "0123456789abcdef" for c in key):
                raise ReceiptError("Invalid receipt fingerprint")
        if type(self.history) is not tuple or len(self.history) > MAX_HISTORY:
            raise ReceiptError("Invalid receipt history")
        for item in self.history:
            if type(item) is not tuple:
                raise ReceiptError("Receipt history must be immutable")
            _entry(item)

    @property
    def counts(self) -> tuple[int, ...]:
        counter = Counter(item[0] for item in self.history)
        return tuple(counter[item_id] for item_id, _, _ in PROGRESSIVES)

    @property
    def generation(self) -> str:
        return _digest((self.identity, self.config, self.history))

    def to_document(self) -> dict[str, Any]:
        return {"schema": 1, "status": "received_only", "identity": self.identity,
                "config": self.config, "history": self.history,
                "counts": self.counts, "generation": self.generation}


def observe_history(*, identity: str, config: CharacterConfig,
                    items: Iterable[Any], previous: ReceiptSnapshot | None = None) -> ReceiptSnapshot:
    """Consume CommonClient's cumulative items_received, never a packet delta."""
    history = []
    for item in items:
        if len(history) >= MAX_HISTORY:
            raise ReceiptError("Receipt history exceeds the supported bound")
        history.append(_entry(item))
    candidate = ReceiptSnapshot(identity, config.fingerprint, tuple(history))
    if previous is not None:
        if (previous.identity, previous.config) != (candidate.identity, candidate.config):
            raise ReceiptError("AP identity or character configuration changed")
        prefix_length = min(len(previous.history), len(candidate.history))
        if previous.history[:prefix_length] != candidate.history[:prefix_length]:
            raise ReceiptError("AP receipt history diverged; manual diagnosis required")
        if len(candidate.history) < len(previous.history):
            raise IncompleteHistory("Wait for complete AP history before publishing a target")
    return candidate


def write_receipts(path: Path, snapshot: ReceiptSnapshot) -> None:
    """Atomic receipt publication. Never call this path a game-save commit."""
    path = Path(path)
    data = (json.dumps(snapshot.to_document(), sort_keys=True, ensure_ascii=True) + "\n").encode("ascii")
    if len(data) > MAX_FILE_BYTES:
        raise ReceiptError("Receipt file exceeds the supported bound")
    # Parent is deliberately not created: the caller must already have selected
    # the dedicated AP slot. No traversal or save-path derivation occurs here.
    fd, temporary = tempfile.mkstemp(prefix=".character-receipts-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def read_receipts(path: Path, *, identity: str, config: CharacterConfig) -> ReceiptSnapshot:
    with Path(path).open("rb") as stream:
        raw = stream.read(MAX_FILE_BYTES + 1)
    if len(raw) > MAX_FILE_BYTES:
        raise ReceiptError("Receipt file exceeds the supported bound")
    try:
        doc = json.loads(raw)
        if type(doc["schema"]) is not int or doc["schema"] != 1 or doc["status"] != "received_only":
            raise ReceiptError("Unsupported receipt schema/status")
        if type(doc["history"]) is not list:
            raise ReceiptError("Invalid receipt history")
        snapshot = ReceiptSnapshot(doc["identity"], doc["config"],
                                   tuple(_entry(item) for item in doc["history"]))
        if (snapshot.identity, snapshot.config) != (identity, config.fingerprint):
            raise ReceiptError("Receipt file belongs to another identity/configuration")
        if doc["generation"] != snapshot.generation or doc["counts"] != list(snapshot.counts):
            raise ReceiptError("Receipt content verification failed")
        return snapshot
    except (KeyError, TypeError, json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ReceiptError("Malformed receipt file; refusing to reset progress") from exc


class ReceiptJournal:
    """A failed publication leaves the last confirmed receipt snapshot intact."""

    def __init__(self, path: Path, *, identity: str, config: CharacterConfig):
        self.path = Path(path)
        self.identity = identity
        self.config = config
        self.snapshot: ReceiptSnapshot | None = None
        # Missing is recoverable from AP. Corruption/permission errors are NOT
        # treated as an empty journal and must suspend reception publication.
        try:
            self.snapshot = read_receipts(self.path, identity=identity, config=config)
        except FileNotFoundError:
            pass

    def observe(self, items: Iterable[Any]) -> tuple[ReceiptSnapshot, bool]:
        candidate = observe_history(identity=self.identity, config=self.config,
                                    items=items, previous=self.snapshot)
        changed = self.snapshot is None or candidate.generation != self.snapshot.generation
        if changed:
            write_receipts(self.path, candidate)
            self.snapshot = candidate
        return candidate, changed
