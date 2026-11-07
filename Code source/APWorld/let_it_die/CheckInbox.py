"""Single-owner durable receipt stage. ACK here does NOT mean an AP check/save.

Research prototype; no automatic worker, game access, native source or network.
"""
import hashlib,json,os,tempfile
from pathlib import Path
from .CheckWire import Frame,Context,EVENT,FAULT,VISIT,CHEST,SIZE,receipt_ack

MAX_BYTES=32*1024*1024;MAX_RECEIVED=100000;MAX_OUTSTANDING=4096;MAX_BATCH=64
def unique_object(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise ValueError('duplicate receipt JSON key')
        result[key]=value
    return result
def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode('ascii')).hexdigest()
def atomic(path,data):
    fd,temp=tempfile.mkstemp(prefix='.death-receipt-',suffix='.tmp',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(temp,path)
    finally:
        if os.path.exists(temp):os.unlink(temp)

class DurableInbox:
    def __init__(self,directory,context:Context):
        context.validate();self.context=context;self.directory=Path(directory)
        self.path=self.directory/f'death_receipts_{context.nonce.hex()}.json'
        self.ack_path=self.directory/'death_client_received.bin'
        self.binding=hashlib.sha256(context.key()).hexdigest()
        self.state={'v':1,'binding':self.binding,'hashes':[],'consumed':0,'raw':{}}
        if self.path.exists():
            if self.path.stat().st_size>MAX_BYTES:raise ValueError('receipt size')
            raw=self.path.read_bytes()
            if len(raw)>MAX_BYTES:raise ValueError('receipt size')
            wrapper=json.loads(raw,object_pairs_hook=unique_object)
            if type(wrapper) is not dict or set(wrapper)!={'state','sha256'}:raise ValueError('receipt envelope')
            state=wrapper['state']
            if type(state) is not dict or set(state)!={'v','binding','hashes','consumed','raw'}:raise ValueError('receipt schema')
            if wrapper['sha256']!=digest(state) or type(state.get('v')) is not int or state['v']!=1 or state.get('binding')!=self.binding:raise ValueError('receipt integrity/context')
            hashes=state['hashes'];consumed=state['consumed'];pending=state['raw']
            if type(hashes) is not list or len(hashes)>MAX_RECEIVED or type(consumed) is not int or not 0<=consumed<=len(hashes) or type(pending) is not dict or len(pending)>MAX_OUTSTANDING:raise ValueError('receipt counters')
            if any(type(h) is not str or len(h)!=64 or any(c not in '0123456789abcdef' for c in h) for h in hashes):raise ValueError('receipt digests')
            if set(pending)!={str(i) for i in range(consumed+1,len(hashes)+1)}:raise ValueError('receipt holes')
            for seq,text in pending.items():
                if type(text) is not str or len(text)!=SIZE*2:raise ValueError('receipt frame size')
                data=bytes.fromhex(text);frame=Frame.decode(data)
                if frame.kind not in (EVENT,VISIT,CHEST) or frame.context!=context or frame.sequence!=int(seq) or hashlib.sha256(data).hexdigest()!=hashes[int(seq)-1]:raise ValueError('receipt frame mismatch')
            self.state=state
    def _store(self,state):
        raw=json.dumps({'state':state,'sha256':digest(state)},sort_keys=True,separators=(',',':')).encode('ascii')
        if len(raw)>MAX_BYTES:raise ValueError('receipt capacity; producer must retain')
        self.directory.mkdir(parents=True,exist_ok=True);atomic(self.path,raw);self.state=state
    @property
    def received(self):return len(self.state['hashes'])
    def ingest(self,batch):
        if type(batch) is not bytes or not batch or len(batch)%SIZE or len(batch)>MAX_BATCH*SIZE:raise ValueError('batch size')
        frames=[Frame.decode(batch[i:i+SIZE]) for i in range(0,len(batch),SIZE)]
        for i,frame in enumerate(frames):
            if frame.context!=self.context:raise ValueError('foreign producer/identity/config/writer')
            if frame.kind==FAULT:raise ValueError(f'native producer fault {frame.status}; pause, no ACK')
            if frame.kind not in (EVENT,VISIT,CHEST):raise ValueError('control frame in event batch')
            if i and frame.sequence!=frames[i-1].sequence+1:raise ValueError('non-contiguous batch')
        state=json.loads(json.dumps(self.state));changed=False
        for i,frame in enumerate(frames):
            raw=batch[i*SIZE:(i+1)*SIZE];sha=hashlib.sha256(raw).hexdigest();seq=frame.sequence
            if seq<=len(state['hashes']):
                if state['hashes'][seq-1]!=sha:raise ValueError('conflicting native replay')
                continue
            if seq!=len(state['hashes'])+1:raise ValueError('missing native event; no cumulative ACK')
            if seq>MAX_RECEIVED or len(state['raw'])>=MAX_OUTSTANDING:raise ValueError('receipt capacity; retain native records')
            state['hashes'].append(sha);state['raw'][str(seq)]=raw.hex();changed=True
        if changed:self._store(state)
        # The on-disk stage is durable BEFORE the producer receives this ack.
        return receipt_ack(self.context,self.received)
    def publish_ack(self):
        if not self.received:return None
        raw=receipt_ack(self.context,self.received);atomic(self.ack_path,raw);return raw
    def consume(self,callback,limit=MAX_BATCH):
        if type(limit) is not int or not 1<=limit<=MAX_BATCH:raise ValueError('consumer limit')
        count=0
        while self.state['consumed']<self.received and count<limit:
            seq=self.state['consumed']+1;frame=Frame.decode(bytes.fromhex(self.state['raw'][str(seq)]))
            callback(frame) # Must persist outbox outcome before returning; replay is possible.
            state=json.loads(json.dumps(self.state));state['consumed']=seq;del state['raw'][str(seq)];self._store(state);count+=1
        return count
