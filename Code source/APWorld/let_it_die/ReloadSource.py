"""Read-only BRG preflight for the existing item-ownership handoff.

Publishes metadata only, never plaintext save contents, save writes or an ACK.
The native reader must independently hold/hash the actual source file.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

from .ReloadValidation import MAX_FILE, SnapshotError, decode_brg, validate_primary_schema
from .ReloadCompatibility import validate_native_compatibility

SOURCE_FILE = "character_reload_source.bin"
SOURCE_MAGIC = b"LIDAPRS1"
SOURCE_SIZE = 640


def prepare_source_proof(raw: bytes, quiesced) -> bytes:
    from .CharacterTransport import QUIESCED, TransportError

    quiesced.validate()
    if quiesced.kind != QUIESCED:
        raise TransportError("Reload source requires a quiesced session")
    root, _chunks, _declared = decode_brg(raw)
    user = root.get("user")
    uid = user.get("uid") if type(user) is dict else None
    if type(uid) not in (str, int):
        raise SnapshotError("Source identity absent")
    uid = str(uid)
    if not 1 <= len(uid) <= 128 or any(c not in "0123456789" for c in uid):
        raise SnapshotError("Source identity format")
    validate_primary_schema(root, uid)
    validate_native_compatibility(root)
    proof = bytearray(SOURCE_SIZE)
    proof[:8] = SOURCE_MAGIC
    # flags3: primary schema and native comparison representation checked.
    # This does not claim validation of every nested gameplay domain.
    struct.pack_into("<6I", proof, 8, 1, SOURCE_SIZE, len(raw), 3, len(uid), 0)
    proof[32:288] = quiesced.encode()
    proof[288:320] = hashlib.sha256(raw).digest()
    encoded_uid = uid.encode("utf-16-le")
    proof[320:320 + len(encoded_uid)] = encoded_uid
    return bytes(proof)


def publish_source_proof(directory: Path, quiesced, publish) -> None:
    """Called once after legacy quiescence and before publishing QUIESCED."""
    directory = Path(directory)
    with (directory / "brggame.sav").open("rb") as source:
        raw = source.read(MAX_FILE + 1)
    proof = prepare_source_proof(raw, quiesced)
    publish(directory / SOURCE_FILE, proof)
