"""Research candidate only: exact receipts, no stock, file or game calls."""
from collections import Counter
from .DecalCatalog import FAMILIES,CATALOGUE_HEX
from .BlueprintProgression import counts_for_target as validate_shared_target
from .BlueprintTransport import encode_blueprints,BlueprintTransportError

CATALOGUE=bytes.fromhex(CATALOGUE_HEX)
COUNT=len(FAMILIES)
FRAME_SIZE=320+4*COUNT

def counts_for_target(snapshot,target):
    # Reuse the complete immutable snapshot/session/TARGET comparison. This
    # validates the shared receipt prefix, rather than inventing extra receipts.
    validate_shared_target(snapshot,target)
    counter=Counter(row[0] for row in snapshot.history)
    return tuple(counter[apid] for apid,_ in FAMILIES)

def payload_for_target(snapshot,target):
    counts=counts_for_target(snapshot,target)
    if sum(target.counts)+sum(counts)>target.history_length:
        raise BlueprintTransportError('Decal receipts exceed shared history')
    return encode_blueprints(target,counts,CATALOGUE)
