"""Research model, NOT a runtime authority or a save writer.

Pure fixed-frame journal validation. An unresolved intent never authorizes a
replay. Native integration must supply real own-save/readback evidence; this
module neither manufactures nor verifies that external gameplay authority.
"""
from dataclasses import dataclass,replace
from .CharacterTransport import Message,TARGET,SAVED
from .BlueprintTransport import decode_blueprints,decode_blueprint_saved,BlueprintTransportError
from .DecalProgression import COUNT,FRAME_SIZE,CATALOGUE,counts_for_target,payload_for_target

class RecoveryRequired(ValueError):
    pass

@dataclass(frozen=True)
class View:
    committed:tuple[int,...]
    history_length:int
    generation:bytes|None
    cleanup_pending:bool
    identity:str
    config:str

def _saved(raw,snapshot):
    if type(raw) is not bytes or len(raw)!=FRAME_SIZE:
        raise BlueprintTransportError('Invalid decal ledger size')
    saved=Message.decode(raw[64:320])
    if saved.kind!=SAVED or saved.status!=1:
        raise BlueprintTransportError('Not a saved decal ledger')
    target=replace(saved,kind=TARGET,status=0)
    if saved.history_length>len(snapshot.history):
        raise BlueprintTransportError('Ledger history exceeds available receipts')
    prefix=type(snapshot)(snapshot.identity,snapshot.config,snapshot.history[:saved.history_length])
    expected=counts_for_target(prefix,target)
    if raw[8:12]!=b'\x01\x00\x00\x00':
        raise BlueprintTransportError('Decal ledger must use strict V1')
    counts=decode_blueprint_saved(raw,target,CATALOGUE,COUNT)
    if counts!=expected:
        raise BlueprintTransportError('Ledger differs from actual receipt prefix')
    return target,counts

def inspect(snapshot,ledger=None,pending=None):
    """Restart policy: only committed proof or exact committed-intent cleanup."""
    if ledger is None:
        committed=(0,)*COUNT;target=None
    else:
        target,committed=_saved(ledger,snapshot)
    if pending is not None:
        if type(pending) is not bytes or len(pending)!=FRAME_SIZE:
            raise BlueprintTransportError('Invalid pending frame')
        attempt=Message.decode(pending[64:320])
        if attempt.kind!=TARGET:
            raise BlueprintTransportError('Pending is not a TARGET')
        # Validate checksum/catalogue/session/actual prefix before reporting an
        # unresolved state; corrupt or foreign records are never discarded.
        if attempt.history_length>len(snapshot.history):
            raise BlueprintTransportError('Pending history exceeds available receipts')
        prefix=type(snapshot)(snapshot.identity,snapshot.config,snapshot.history[:attempt.history_length])
        expected=payload_for_target(prefix,attempt)
        if pending!=expected:
            raise BlueprintTransportError('Pending differs from actual receipt prefix')
        if target is None or attempt!=target:
            raise RecoveryRequired('Unresolved decal intent; no automatic grant or ACK')
        decode_blueprints(pending,target,CATALOGUE,COUNT)
    return View(committed,target.history_length if target else 0,target.generation if target else None,pending is not None,snapshot.identity,snapshot.config)

def plan(snapshot,target,view):
    """Return deltas from delivered receipts, irrespective of current stock."""
    counts=counts_for_target(snapshot,target)
    if view.identity!=snapshot.identity or view.config!=snapshot.config:
        raise BlueprintTransportError('Journal view belongs to another slot/config')
    if len(view.committed)!=COUNT or target.history_length<view.history_length:
        raise BlueprintTransportError('History rollback')
    if any(type(n) is not int or n<0 for n in view.committed):
        raise BlueprintTransportError('Invalid committed receipt vector')
    delta=tuple(new-old for new,old in zip(counts,view.committed))
    if any(n<0 for n in delta) or sum(delta)>target.history_length-view.history_length:
        raise BlueprintTransportError('Committed receipts are not a prefix')
    if target.history_length==view.history_length and view.generation not in (None,target.generation):
        raise BlueprintTransportError('Divergent receipt prefix')
    return delta

def validate_commit(snapshot,pending,native_saved):
    """Bind a caller-supplied proof to the exact armed attempt, no save calls."""
    target,counts=_saved(native_saved,snapshot)
    if pending!=payload_for_target(type(snapshot)(snapshot.identity,snapshot.config,snapshot.history[:target.history_length]),target):
        raise BlueprintTransportError('Native proof is not the armed attempt')
    return native_saved
