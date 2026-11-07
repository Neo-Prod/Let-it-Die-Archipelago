"""Candidate paired publication; not wired into CharacterTransport/client yet."""
from __future__ import annotations

import os
from pathlib import Path
import tempfile
from .BlueprintProgression import payload_for_target
from .CharacterProgression import ReceiptSnapshot
from .CharacterTransport import Message, TARGET_FILE, atomic_write

BLUEPRINT_TARGET_FILE = 'blueprint_target.bin'


def _publish_companion(directory: Path, payload: bytes) -> None:
    # Only one fixed basename. Directory is the already selected bridge slot.
    # Never create a directory or derive a save path from AP data here.
    fd, temporary = tempfile.mkstemp(prefix='.blueprint-bridge-', suffix='.tmp', dir=directory)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, directory / BLUEPRINT_TARGET_FILE)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def publish_blueprint_target(directory: Path, snapshot: ReceiptSnapshot, target: Message) -> None:
    """Return only after both publications; this is NOT a game save/ACK.

    A failed TARGET publication can leave an unmatched companion, which the
    paired native reader must refuse before retaining the entry batch.
    """
    payload = payload_for_target(snapshot, target)
    character = target.encode()
    directory = Path(directory)
    from .DecalProgression import payload_for_target as decal_payload
    from .DecalJournal import inspect as inspect_decal_ledger
    from .DecalSavedReport import LEDGER_FILE, _bounded
    try:
        ledger = _bounded(directory / LEDGER_FILE)
    except FileNotFoundError:
        ledger = None
    # Verify any durable previous delivery against actual full AP history.
    # An in-flight pending intent is owned by native, never replayed by Python.
    inspect_decal_ledger(snapshot, ledger)
    decals = decal_payload(snapshot, target)
    atomic_write(directory / 'decal_target.bin', decals)
    _publish_companion(directory, payload)
    atomic_write(directory / TARGET_FILE, character)
