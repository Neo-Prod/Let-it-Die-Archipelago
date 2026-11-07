"""Candidate blueprint save-report validation; never edits a game save.

NativeTransport must first prove current session/writer and pending TARGET.
The report is a native save claim, not independent disk/reload validation.
"""
from __future__ import annotations

from dataclasses import replace
import json
import os
from pathlib import Path
import tempfile
from .BlueprintCatalog import FAMILIES, CATALOGUE_HEX
from .BlueprintProgression import counts_for_target
from .BlueprintTransport import decode_blueprint_saved, BlueprintTransportError
from .CharacterProgression import ReceiptSnapshot
from .CharacterTransport import Message, TARGET, SAVED, GAME_SHA

SAVED_FILE = 'blueprint_native_saved.bin'
APPLIED_FILE = 'blueprint_applied.json'


def validate_saved_report(snapshot: ReceiptSnapshot, saved: Message, raw: bytes) -> dict:
    if not isinstance(snapshot, ReceiptSnapshot) or not isinstance(saved, Message):
        raise BlueprintTransportError('Invalid receipt snapshot/save report')
    saved.validate()
    if saved.kind != SAVED or saved.history_length > len(snapshot.history):
        raise BlueprintTransportError('Not a saved receipt prefix')
    prefix = ReceiptSnapshot(snapshot.identity, snapshot.config, snapshot.history[:saved.history_length])
    target = replace(saved, kind=TARGET, status=0)
    expected = counts_for_target(prefix, target)
    counts = decode_blueprint_saved(raw, target, bytes.fromhex(CATALOGUE_HEX), len(FAMILIES))
    if counts != expected:
        raise BlueprintTransportError('Saved blueprint counts differ from receipt prefix')
    return {'schema': 1, 'status': 'native_save_reported', 'scope': 'blueprint_progressives_only',
            'identity': prefix.identity, 'config': prefix.config,
            'catalogue': CATALOGUE_HEX, 'receipt_generation': prefix.generation,
            'history_length': len(prefix.history), 'item_ids': [item_id for item_id, _ in FAMILIES],
            'counts': counts, 'game_sha256': GAME_SHA}


def read_saved_report(directory: Path, snapshot: ReceiptSnapshot, saved: Message) -> dict:
    with (Path(directory) / SAVED_FILE).open('rb') as stream:
        raw = stream.read(320 + 4 * len(FAMILIES) + 1)
    return validate_saved_report(snapshot, saved, raw)


def write_saved_report(directory: Path, report: dict) -> None:
    data = (json.dumps(report, sort_keys=True) + '\n').encode('ascii')
    fd, temporary = tempfile.mkstemp(prefix='.blueprint-report-', suffix='.tmp', dir=directory)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, Path(directory) / APPLIED_FILE)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
