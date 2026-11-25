"""Floor companion for the existing frozen Waiting Room receipt batch."""
import struct
from .CharacterTransport import TARGET, checksum
from .FloorProgression import FloorPolicy, ITEM_ID, required_for_floor

MAGIC = b'LIDAPF01'
SIZE = 320


def encode_floor_target(snapshot, target, policy):
    if not isinstance(policy, FloorPolicy) or target.kind != TARGET:
        raise ValueError('Invalid Floor target policy')
    if (target.identity.hex() != snapshot.identity or target.config.hex() != snapshot.config
            or target.generation.hex() != snapshot.generation or target.counts != snapshot.counts
            or target.history_length != len(snapshot.history)):
        raise ValueError('Floor companion belongs to another receipt batch')
    received = sum(entry[0] == ITEM_ID for entry in snapshot.history)
    if not policy.enabled and received:
        # Console-injected items do not silently enable a disabled option.
        received = 0
    raw = bytearray(SIZE)
    raw[:8] = MAGIC
    struct.pack_into('<7I', raw, 8, 1, SIZE, int(policy.enabled), policy.goal,
                     policy.quantity, received, min(received, required_for_floor(policy.goal)))
    raw[64:] = target.encode()
    struct.pack_into('<I', raw, 60, checksum(raw[:60] + raw[64:]))
    return bytes(raw)


def decode_floor_target(raw, target):
    if type(raw) is not bytes or len(raw) != SIZE or raw[:8] != MAGIC:
        raise ValueError('Invalid Floor companion')
    version, size, enabled, goal, quantity, received, applied = struct.unpack_from('<7I', raw, 8)
    if (version != 1 or size != SIZE or enabled not in (0, 1) or raw[36:60] != bytes(24)
            or raw[64:] != target.encode()
            or struct.unpack_from('<I', raw, 60)[0] != checksum(raw[:60] + raw[64:])):
        raise ValueError('Floor companion identity/layout mismatch')
    policy = FloorPolicy(bool(enabled), quantity, goal)
    if (received > target.history_length - sum(target.counts) or (not enabled and received)
            or applied != min(received, required_for_floor(goal))):
        raise ValueError('Floor companion count mismatch')
    return policy, applied
