"""LET IT DIE Offline BRG save codec.

The PC Offline save is a BRG container whose payload is JSON compressed with
zlib. This module deliberately contains no Archipelago networking code: it is
the low-level, testable layer used by the future client.

Always operate on a copy of brggame.sav while developing.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import shutil
import struct
import time
from typing import Any
import zlib


BRG_MAGIC = b"BRG\x00"
ZLIB_MAGIC = b"ZLIB"
SUPPORTED_CONTAINER_VERSION = 2


class LetItDieSaveError(RuntimeError):
    """Raised when a file is not a supported LET IT DIE Offline save."""


@dataclass(frozen=True)
class BrgSaveMetadata:
    """Container metadata read from a BRG save."""

    container_version: int
    declared_uncompressed_size: int
    chunk_count: int
    actual_uncompressed_size: int


@dataclass
class BrgSave:
    """Parsed LET IT DIE save and its BRG container metadata."""

    root: dict[str, Any]
    metadata: BrgSaveMetadata


# Reads a BRG save and returns its decompressed JSON object.
def read_brg_save(path: str | Path) -> BrgSave:
    path = Path(path)
    data = path.read_bytes()

    if len(data) < 24:
        raise LetItDieSaveError("Save is too small to be a LET IT DIE BRG save.")

    if data[:4] != BRG_MAGIC:
        raise LetItDieSaveError(
            f"Unexpected BRG magic {data[:4]!r}; expected {BRG_MAGIC!r}."
        )

    version, declared_size = struct.unpack_from("<II", data, 4)

    if data[12:16] != ZLIB_MAGIC:
        raise LetItDieSaveError(
            f"Unexpected inner magic {data[12:16]!r}; expected {ZLIB_MAGIC!r}."
        )

    offset = 16
    chunks: list[bytes] = []

    while offset + 8 <= len(data):
        raw_len, compressed_len = struct.unpack_from("<II", data, offset)
        offset += 8

        if compressed_len <= 0:
            break

        end = offset + compressed_len
        if end > len(data):
            raise LetItDieSaveError(
                "Compressed chunk extends beyond the end of the save."
            )

        compressed = data[offset:end]
        offset = end

        try:
            raw = zlib.decompress(compressed)
        except zlib.error as exc:
            raise LetItDieSaveError(
                f"Could not decompress BRG chunk {len(chunks) + 1}: {exc}"
            ) from exc

        if raw_len and len(raw) != raw_len:
            raise LetItDieSaveError(
                "BRG chunk size mismatch: "
                f"header={raw_len}, decompressed={len(raw)}."
            )

        chunks.append(raw)

    if not chunks:
        raise LetItDieSaveError("No zlib payload was found in the BRG save.")

    payload = b"".join(chunks)

    try:
        text = payload.decode("utf-8-sig")
    except UnicodeDecodeError as exc:
        raise LetItDieSaveError("Save JSON is not valid UTF-8.") from exc

    # The community editor repairs raw control characters inside JSON strings.
    # Python's strict=False accepts those strings without rewriting unrelated data.
    try:
        root = json.loads(text, strict=False)
    except json.JSONDecodeError as exc:
        raise LetItDieSaveError(
            f"Decompressed BRG payload is not valid JSON: {exc}"
        ) from exc

    if not isinstance(root, dict) or not isinstance(root.get("soul"), dict):
        raise LetItDieSaveError(
            "JSON payload does not look like a LET IT DIE full save dump."
        )

    metadata = BrgSaveMetadata(
        container_version=version,
        declared_uncompressed_size=declared_size,
        chunk_count=len(chunks),
        actual_uncompressed_size=len(payload),
    )

    return BrgSave(root=root, metadata=metadata)


# Builds a valid single-chunk BRG save from a parsed JSON object.
def build_brg_save_bytes(root: dict[str, Any]) -> bytes:
    payload = json.dumps(
        root,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")

    compressed = zlib.compress(payload)

    header = bytearray(24)
    header[0:4] = BRG_MAGIC
    struct.pack_into("<I", header, 4, SUPPORTED_CONTAINER_VERSION)
    struct.pack_into("<I", header, 8, len(payload))
    header[12:16] = ZLIB_MAGIC
    struct.pack_into("<I", header, 16, len(payload))
    struct.pack_into("<I", header, 20, len(compressed))

    return bytes(header) + compressed


# Writes a BRG save, optionally backing up an existing destination first.
def write_brg_save(
    root: dict[str, Any],
    destination: str | Path,
    *,
    backup_existing: bool = True,
) -> Path:
    destination = Path(destination)

    if destination.exists() and backup_existing:
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        backup = destination.with_name(
            f"{destination.stem}.backup_{timestamp}{destination.suffix}"
        )
        shutil.copy2(destination, backup)

    destination.write_bytes(build_brg_save_bytes(root))
    return destination


# Returns the player UID used as the key for fighter/account-owned tables.
def get_main_uid(root: dict[str, Any]) -> str:
    soul = root.get("soul") or {}
    user = root.get("user") or {}

    uid = soul.get("uid")
    if uid in (None, ""):
        uid = user.get("uid")

    if uid in (None, ""):
        raise LetItDieSaveError("Could not determine the save's main UID.")

    return str(uid)


# Returns the raw fighter records stored in soul.chr.chrs[uid].
def get_fighters(root: dict[str, Any]) -> list[dict[str, Any]]:
    uid = get_main_uid(root)
    soul = root.get("soul") or {}
    chr_table = soul.get("chr") or {}
    chrs_table = chr_table.get("chrs") or {}

    fighters = chrs_table.get(uid, [])

    if not isinstance(fighters, list):
        raise LetItDieSaveError("Fighter table has an unexpected format.")

    return fighters


# Returns the body/stat records associated with the current player's fighters.
def get_bodyuser_records(root: dict[str, Any]) -> list[dict[str, Any]]:
    uid = get_main_uid(root)
    table = root.get("bodyuser") or {}
    records = table.get(uid, [])

    if not isinstance(records, list):
        raise LetItDieSaveError("bodyuser table has an unexpected format.")

    return records


# Returns the raw R&D / blueprint research records.
def get_research_records(root: dict[str, Any]) -> list[dict[str, Any]]:
    soul = root.get("soul") or {}
    partresearch = soul.get("partresearch") or {}
    records = partresearch.get("user", [])

    if not isinstance(records, list):
        raise LetItDieSaveError("Research table has an unexpected format.")

    return records


# Returns the player's decal stock records.
def get_decal_stock(root: dict[str, Any]) -> list[dict[str, Any]]:
    soul = root.get("soul") or {}
    skl = soul.get("skl") or {}
    records = skl.get("psskl", [])

    if not isinstance(records, list):
        raise LetItDieSaveError("Decal stock table has an unexpected format.")

    return records


# Performs a non-mutating structural inspection useful for client development.
def inspect_save(root: dict[str, Any]) -> dict[str, Any]:
    uid = get_main_uid(root)
    fighters = get_fighters(root)
    body_records = get_bodyuser_records(root)
    research = get_research_records(root)
    decals = get_decal_stock(root)

    floor = root.get("floor")
    floor_keys = sorted(floor.keys()) if isinstance(floor, dict) else []

    return {
        "uid_present": bool(uid),
        "fighter_count": len(fighters),
        "bodyuser_count": len(body_records),
        "research_record_count": len(research),
        "decal_stock_record_count": len(decals),
        "root_keys": sorted(root.keys()),
        "soul_keys": sorted((root.get("soul") or {}).keys()),
        "floor_keys": floor_keys,
    }
