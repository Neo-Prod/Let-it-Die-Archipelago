"""Candidate pure blueprint receipt calculation. No gameplay, save or file I/O."""
from __future__ import annotations

from collections import Counter
from .BlueprintCatalog import CATALOGUE_HEX, FAMILIES
from .BlueprintTransport import encode_blueprints, BlueprintTransportError
from .CharacterProgression import ReceiptSnapshot, PROGRESSIVES
from .CharacterTransport import Message, TARGET
from .ArmorSets import SET_MEMBERS

CATALOGUE = bytes.fromhex(CATALOGUE_HEX)
if set(item_id for item_id, _ in FAMILIES) & set(item_id for item_id, _, _ in PROGRESSIVES):
    raise RuntimeError('Blueprint and character AP IDs overlap')


def counts_for_target(snapshot: ReceiptSnapshot, target: Message) -> tuple[int, ...]:
    if not isinstance(snapshot, ReceiptSnapshot) or not isinstance(target, Message):
        raise BlueprintTransportError('Expected immutable receipt snapshot and TARGET')
    target.validate()
    if (target.kind != TARGET or target.identity.hex() != snapshot.identity
            or target.config.hex() != snapshot.config
            or target.generation.hex() != snapshot.generation
            or target.history_length != len(snapshot.history)
            or target.counts != snapshot.counts):
        raise BlueprintTransportError('Blueprint and character receipt snapshots differ')
    counter = Counter(item[0] for item in snapshot.history)
    for item_id, members in SET_MEMBERS.items():
        for member_id in members:
            counter[member_id] += counter[item_id]
    return tuple(counter[item_id] for item_id, _ in FAMILIES)


def payload_for_target(snapshot: ReceiptSnapshot, target: Message) -> bytes:
    return encode_blueprints(target, counts_for_target(snapshot, target), CATALOGUE,
                             allow_sets=any(item[0] in SET_MEMBERS for item in snapshot.history))


def target_part_ids(counts: tuple[int, ...]) -> tuple[str, ...]:
    """Fixed AP tiers only, not first missing vanilla type; no locking policy."""
    if (type(counts) is not tuple or len(counts) != len(FAMILIES)
            or any(type(n) is not int or not 0 <= n <= 100000 for n in counts)):
        raise BlueprintTransportError('Invalid blueprint receipt counts')
    return tuple(pid for count, (_, tiers) in zip(counts, FAMILIES) for pid in tiers[:count])
