"""Candidate pure blueprint companion codec; not connected to runtime delivery.

No filesystem, network or game calls. Report encoding never infers a successful
save. The host must bind the character TARGET to the current session first.
"""
from __future__ import annotations

import struct
from dataclasses import replace
from .CharacterProgression import MAX_HISTORY
from .CharacterTransport import Message, TARGET, SAVED, checksum

MAGIC = b'LIDAPB01'
HEADER_SIZE = 64
COUNTS_OFFSET = 320
MAX_FRAME_SIZE = 4096
MAX_FAMILIES = (MAX_FRAME_SIZE - COUNTS_OFFSET) // 4


class BlueprintTransportError(ValueError):
    pass


def _catalog(digest: bytes, count: int) -> None:
    if type(digest) is not bytes or len(digest) != 32 or not any(digest):
        raise BlueprintTransportError('Invalid catalogue digest')
    if type(count) is not int or not 1 <= count <= MAX_FAMILIES:
        raise BlueprintTransportError('Invalid family count')


def _target(target: Message) -> bytes:
    if not isinstance(target, Message) or target.kind != TARGET:
        raise BlueprintTransportError('Expected character TARGET')
    return target.encode()


def _budget_version(target: Message, counts: tuple[int, ...]) -> int:
    stats, plans, history = sum(target.counts), sum(counts), target.history_length
    if stats > history or plans > 3 * (history - stats):
        raise BlueprintTransportError('Counts exceed three-plan set receipt budget')
    return 1 if stats + plans <= history else 2


def encode_blueprints(target: Message, counts: tuple[int, ...], catalogue: bytes, *, allow_sets: bool = False) -> bytes:
    if type(counts) is not tuple:
        raise BlueprintTransportError('Counts must be immutable')
    _catalog(catalogue, len(counts))
    character = _target(target)
    if any(type(n) is not int or not 0 <= n <= MAX_HISTORY for n in counts):
        raise BlueprintTransportError('Invalid blueprint counts')
    version = _budget_version(target, counts)
    if version == 2 and not allow_sets:
        raise BlueprintTransportError('Counts exceed shared receipt history')
    raw = bytearray(COUNTS_OFFSET + 4 * len(counts))
    raw[:8] = MAGIC
    struct.pack_into('<III', raw, 8, version, HEADER_SIZE, len(counts))
    raw[20:52] = catalogue
    raw[64:320] = character
    struct.pack_into(f'<{len(counts)}I', raw, COUNTS_OFFSET, *counts)
    struct.pack_into('<I', raw, 52, checksum(raw[:52] + raw[56:]))
    return bytes(raw)


def decode_blueprints(raw: bytes, target: Message, catalogue: bytes, family_count: int) -> tuple[int, ...]:
    _catalog(catalogue, family_count)
    character = _target(target)
    if type(raw) is not bytes or len(raw) != COUNTS_OFFSET + 4 * family_count:
        raise BlueprintTransportError('Invalid companion size')
    version, header, families = struct.unpack_from('<III', raw, 8)
    if raw[:8] != MAGIC or version not in (1, 2) or (header, families) != (HEADER_SIZE, family_count):
        raise BlueprintTransportError('Invalid companion version/header')
    if raw[20:52] != catalogue or any(raw[56:64]):
        raise BlueprintTransportError('Catalogue or reserved bytes mismatch')
    if struct.unpack_from('<I', raw, 52)[0] != checksum(raw[:52] + raw[56:]):
        raise BlueprintTransportError('Companion checksum mismatch')
    if raw[64:320] != character:
        raise BlueprintTransportError('Companion belongs to another TARGET')
    counts = struct.unpack_from(f'<{family_count}I', raw, COUNTS_OFFSET)
    if any(n > MAX_HISTORY for n in counts) or version != _budget_version(target, counts):
        raise BlueprintTransportError('Counts exceed shared receipt history')
    return counts


def encode_blueprint_saved(target: Message, counts: tuple[int, ...], catalogue: bytes, *, allow_sets: bool = False) -> bytes:
    """Native-save report codec only; caller must already hold its save proof."""
    raw = bytearray(encode_blueprints(target, counts, catalogue, allow_sets=allow_sets))
    raw[64:320] = replace(target, kind=SAVED, status=1).encode()
    struct.pack_into('<I', raw, 52, checksum(raw[:52] + raw[56:]))
    return bytes(raw)


def decode_blueprint_saved(raw: bytes, target: Message, catalogue: bytes, family_count: int) -> tuple[int, ...]:
    _catalog(catalogue, family_count)
    character = _target(target)
    if type(raw) is not bytes or len(raw) != COUNTS_OFFSET + 4 * family_count:
        raise BlueprintTransportError('Invalid saved report size')
    if raw[64:320] != replace(target, kind=SAVED, status=1).encode():
        raise BlueprintTransportError('Saved report belongs to another character save')
    if struct.unpack_from('<I', raw, 52)[0] != checksum(raw[:52] + raw[56:]):
        raise BlueprintTransportError('Saved report checksum mismatch')
    # Normalize only AFTER validating the original proof checksum and SAVED.
    normalized = bytearray(raw)
    normalized[64:320] = character
    struct.pack_into('<I', normalized, 52, checksum(normalized[:52] + normalized[56:]))
    return decode_blueprints(bytes(normalized), target, catalogue, family_count)
