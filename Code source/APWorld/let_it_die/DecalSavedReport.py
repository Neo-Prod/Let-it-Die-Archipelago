"""Candidate runtime proof validation, never a game-save writer.

The common transport must first authenticate the current native SAVED against
its pending TARGET/session. A matching durable ledger is required as well.
"""
from dataclasses import replace
import json
from pathlib import Path
from .CharacterTransport import Message,TARGET,SAVED,atomic_write
from .DecalProgression import CATALOGUE,COUNT,FRAME_SIZE,counts_for_target
from .DecalCatalog import FAMILIES
from .BlueprintTransport import decode_blueprint_saved,BlueprintTransportError

SAVED_FILE='decal_native_saved.bin'
LEDGER_FILE='decal_ledger.bin'
APPLIED_FILE='decal_applied.json'

def _bounded(path):
    with Path(path).open('rb') as stream:
        raw=stream.read(FRAME_SIZE+1)
    if len(raw)!=FRAME_SIZE:
        raise BlueprintTransportError('Invalid decal report/ledger size')
    return raw

def validate_saved_report(snapshot,saved,raw,ledger):
    if not isinstance(saved,Message):
        raise BlueprintTransportError('Invalid native decal save message')
    saved.validate()
    if saved.kind!=SAVED or saved.status!=1 or saved.history_length>len(snapshot.history):
        raise BlueprintTransportError('Not a saved decal receipt prefix')
    if raw!=ledger:
        raise BlueprintTransportError('Native decal report differs from durable ledger')
    prefix=type(snapshot)(snapshot.identity,snapshot.config,snapshot.history[:saved.history_length])
    target=replace(saved,kind=TARGET,status=0)
    expected=counts_for_target(prefix,target)
    if type(raw) is not bytes or raw[8:12]!=b'\x01\0\0\0':
        raise BlueprintTransportError('Decal report must be strict V1')
    counts=decode_blueprint_saved(raw,target,CATALOGUE,COUNT)
    if counts!=expected:
        raise BlueprintTransportError('Saved decal counts differ from actual receipt prefix')
    return {'schema':1,'status':'native_save_and_durable_ledger_reported','scope':'premium_decals_only',
            'identity':prefix.identity,'config':prefix.config,'receipt_generation':prefix.generation,
            'history_length':len(prefix.history),'catalogue':CATALOGUE.hex(),
            'item_ids':[i for i,_ in FAMILIES],'counts':counts}

def read_saved_report(directory,snapshot,saved):
    directory=Path(directory)
    return validate_saved_report(snapshot,saved,_bounded(directory/SAVED_FILE),_bounded(directory/LEDGER_FILE))

def write_saved_report(directory,report):
    atomic_write(Path(directory)/APPLIED_FILE,(json.dumps(report,sort_keys=True)+'\n').encode('ascii'))
