"""Ignore an old-session batch only after verifying its durable receipt.
No writes, deletion, or acknowledgement for the current producer.
"""
import hashlib
from .CheckWire import Frame,EVENT,VISIT,CHEST,SIZE,integer
from .MaterialWire import decode_frame,MATERIAL
from .MaterialInbox import DurableMaterialInbox
from .CheckInbox import DurableInbox,MAX_BATCH

def old_batch_received(directory,current,batch,catalogue_version=4):
    integer(catalogue_version,1,5)
    if type(batch) is not bytes or not batch or len(batch)%SIZE or len(batch)>MAX_BATCH*SIZE:
        raise ValueError('batch size')
    frames=[decode_frame(batch[i:i+SIZE],catalogue_version) for i in range(0,len(batch),SIZE)]
    old=frames[0].context
    if old==current:return False
    if old.nonce==current.nonce or old.run!=current.run or old.config!=current.config:
        raise ValueError('foreign stale context')
    for i,frame in enumerate(frames):
        if frame.context!=old or frame.kind not in ((EVENT,VISIT,CHEST,MATERIAL) if catalogue_version==5 else (EVENT,VISIT,CHEST)) or (i and frame.sequence!=frames[i-1].sequence+1):
            raise ValueError('mixed stale batch')
    path=directory/f'death_receipts_{old.nonce.hex()}.json'
    if not path.is_file():raise ValueError('stale batch has no durable receipt')
    inbox=DurableMaterialInbox(directory,old,5) if catalogue_version==5 else DurableInbox(directory,old)
    for i,frame in enumerate(frames):
        if frame.sequence>inbox.received or inbox.state['hashes'][frame.sequence-1]!=hashlib.sha256(batch[i*SIZE:(i+1)*SIZE]).hexdigest():
            raise ValueError('stale batch not durably received')
    return True
