"""Ordered currency application cursor; inactive until the native worker is wired.

This journal records native application evidence, never game-save completion.
The caller must supply a complete, synchronized ReceiptSnapshot.
"""
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import tempfile

from .CharacterProgression import ReceiptSnapshot
from .Fillers import AMOUNTS_BY_ID
from .KillCoinWire import CoinFrame, ACK, decode


def prefix_digest(history):
    return hashlib.sha256(json.dumps(history, separators=(",", ":"),
                                    ensure_ascii=True).encode("ascii")).hexdigest()


@dataclass(frozen=True)
class AppliedCursor:
    identity: str
    config: str
    index: int
    prefix: str
    evidence: bytes

    def validate(self, snapshot):
        if not isinstance(snapshot, ReceiptSnapshot):
            raise ValueError("Complete receipt snapshot required")
        if (self.identity, self.config) != (snapshot.identity, snapshot.config):
            raise ValueError("KC journal belongs to another receipt identity/config")
        if type(self.index) is not int or not 0 <= self.index < len(snapshot.history):
            raise ValueError("KC history truncated or cursor invalid")
        if self.prefix != prefix_digest(snapshot.history[:self.index + 1]):
            raise ValueError("KC receipt history changed")
        frame = decode(self.evidence)
        if (frame.kind != ACK or frame.index != self.index or
                frame.item != snapshot.history[self.index][0]):
            raise ValueError("KC journal evidence mismatch")
        return self


def next_grant(snapshot, context, cursor=None):
    if not isinstance(snapshot, ReceiptSnapshot):
        raise ValueError("Complete receipt snapshot required")
    context.validate()
    start = 0
    if cursor is not None:
        cursor.validate(snapshot)
        previous = decode(cursor.evidence).context
        if (previous.run, previous.config) != (context.run, context.config):
            raise ValueError("KC native slot changed")
        start = cursor.index + 1
    for index in range(start, len(snapshot.history)):
        item = snapshot.history[index][0]
        if item in AMOUNTS_BY_ID:
            return CoinFrame(context, index, item, AMOUNTS_BY_ID[item])
    return None


def accept_ack(snapshot, grant, raw_ack, cursor=None):
    expected = next_grant(snapshot, grant.context, cursor)
    if expected is None or grant != expected:
        raise ValueError("KC grant is not the next receipt")
    ack = decode(raw_ack)
    if (ack.kind != ACK or ack.context != grant.context or
            (ack.index, ack.item, ack.amount) != (grant.index, grant.item, grant.amount)):
        raise ValueError("KC acknowledgement does not match outstanding grant")
    return AppliedCursor(snapshot.identity, snapshot.config, grant.index,
                         prefix_digest(snapshot.history[:grant.index + 1]), raw_ack).validate(snapshot)


def write_cursor(path, cursor, snapshot):
    cursor.validate(snapshot)
    document = dict(schema=1, identity=cursor.identity, config=cursor.config,
                    index=cursor.index, prefix=cursor.prefix, evidence=cursor.evidence.hex())
    data = (json.dumps(document, sort_keys=True) + "\n").encode("ascii")
    path = Path(path)
    fd, temporary = tempfile.mkstemp(prefix=".kill-coins-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def read_cursor(path, snapshot):
    # Only absence means no cursor. Corruption must never trigger a replay.
    try:
        with Path(path).open("rb") as stream:
            raw = stream.read(4097)
    except FileNotFoundError:
        return None
    if len(raw) > 4096:
        raise ValueError("KC journal exceeds size limit")
    def pairs(entries):
        result = {}
        for key, value in entries:
            if key in result:
                raise ValueError("Duplicate KC journal key")
            result[key] = value
        return result
    doc = json.loads(raw, object_pairs_hook=pairs)
    if (type(doc) is not dict or set(doc) !=
            {"schema", "identity", "config", "index", "prefix", "evidence"} or
            type(doc["schema"]) is not int or doc["schema"] != 1 or
            type(doc["evidence"]) is not str or len(doc["evidence"]) != 512):
        raise ValueError("Invalid KC journal schema")
    return AppliedCursor(doc["identity"], doc["config"], doc["index"], doc["prefix"],
                         bytes.fromhex(doc["evidence"])).validate(snapshot)
