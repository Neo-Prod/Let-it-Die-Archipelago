"""Hints for native-observed BUY offers only; no checks, purchases or rewards.

LocationInfo is already populated by display-only scouting, so it cannot
acknowledge hint creation. Confirm from the server's read-only hint set instead.
"""
import hashlib,json,struct
from pathlib import Path
from .CheckWire import Context,integer
from .CheckInbox import atomic,unique_object
from .ShopDisplay import offers

FILE='shop_native_hints.bin'
JOURNAL='shop_hint_history.json'
SIZE=256

class StaleObservation(ValueError):
    """A previous native process's fixed-path file, not new observations."""

def checksum(raw):
    value=2166136261
    for i,byte in enumerate(raw):
        if not 196<=i<200:value=((value^byte)*16777619)&0xffffffff
    return value

def indices(config):
    return {(offer if scope==1 else offer+26):location for location,(scope,offer)in offers(config).items()}

def decode(raw,context,config):
    if type(raw)is not bytes or len(raw)!=SIZE or raw[:8]!=b'LIDSHNT1':raise ValueError('shop hint size/magic')
    if struct.unpack_from('<II',raw,8)!=(1,SIZE)or any(raw[200:]):raise ValueError('shop hint version/padding')
    if struct.unpack_from('<I',raw,196)[0]!=checksum(raw):raise ValueError('shop hint checksum')
    pid,client,birth=struct.unpack_from('<IIQ',raw,16)
    current=Context(pid,birth,raw[32:48],raw[64:96],raw[96:128],client,raw[48:64]);current.validate()
    if current!=context:raise StaleObservation('shop hint foreign context')
    if raw[128:160]!=bytes.fromhex(config.fingerprint):raise ValueError('shop hint foreign catalogue')
    sequence=struct.unpack_from('<Q',raw,188)[0]
    if not sequence:raise ValueError('shop hint sequence')
    mask=int.from_bytes(raw[160:188],'little');selected=indices(config)
    allowed=sum(1<<index for index in selected)
    if not mask or mask&~allowed:raise ValueError('shop hint unknown offer')
    return sequence,frozenset(location for index,location in selected.items()if mask&(1<<index))

def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False).encode()).hexdigest()

class ShopHintService:
    def __init__(self,directory,context,config):
        if type(context)is not Context:raise ValueError('shop hints Context')
        context.validate();self.directory=Path(directory);self.context=context;self.config=config
        self.ids=frozenset(indices(config).values());self.path=self.directory/JOURNAL
        self.binding=digest([context.run.hex(),context.config.hex(),config.fingerprint])
        self.state=dict(schema=1,binding=self.binding,observed=[],hinted=[])
        self.last_raw=None;self.sequence=0;self.server=None;self.socket=None
        self.next_request=0.;self.slot=None;self.team=None;self.subscribed=False
        if self.path.exists():
            with self.path.open('rb')as f:raw=f.read(131073)
            if len(raw)>131072:raise ValueError('shop hint journal capacity')
            wrapper=json.loads(raw,object_pairs_hook=unique_object)
            if type(wrapper)is not dict or set(wrapper)!={'state','sha256'}or digest(wrapper['state'])!=wrapper['sha256']:
                raise ValueError('shop hint journal checksum')
            self.validate(wrapper['state']);self.state=wrapper['state']

    def validate(self,state):
        if type(state)is not dict or set(state)!={'schema','binding','observed','hinted'}or type(state['schema'])is not int or state['schema']!=1 or state['binding']!=self.binding:
            raise ValueError('shop hint journal binding/schema')
        for key in ('observed','hinted'):
            values=state[key]
            if type(values)is not list or len(values)>len(self.ids)or any(type(v)is not int for v in values)or values!=sorted(set(values))or not set(values)<=self.ids:
                raise ValueError('shop hint journal location set')
        if not set(state['hinted'])<=set(state['observed']):raise ValueError('shop hint unobserved confirmation')

    def commit(self,observed,hinted):
        state=dict(schema=1,binding=self.binding,observed=sorted(observed),hinted=sorted(hinted));self.validate(state)
        if state==self.state:return
        atomic(self.path,json.dumps(dict(state=state,sha256=digest(state)),sort_keys=True,separators=(',',':')).encode())
        self.state=state

    def poll(self):
        try:
            with (self.directory/FILE).open('rb')as f:raw=f.read(SIZE+1)
        except FileNotFoundError:return
        if raw==self.last_raw:return
        try:sequence,locations=decode(raw,self.context,self.config)
        except StaleObservation:return
        if sequence<self.sequence:raise ValueError('shop hint sequence regressed')
        if sequence==self.sequence and self.last_raw is not None:raise ValueError('shop hint sequence changed payload')
        fresh=locations-set(self.state['observed'])
        self.commit(set(self.state['observed'])|locations,self.state['hinted'])
        if fresh:self.next_request=0.
        self.sequence=sequence;self.last_raw=raw

    async def step(self,ctx,now):
        self.poll()
        observed=set(self.state['observed']);hinted=set(self.state['hinted'])
        if not observed-hinted:return 'idle'
        server=ctx.server;socket=getattr(server,'socket',None)
        if server is None or not getattr(socket,'open',False)or getattr(socket,'closed',True):return 'waiting_server'
        integer(ctx.slot,1,2**31-1);integer(ctx.team,0,2**31-1)
        if self.slot is None:self.slot=ctx.slot;self.team=ctx.team
        if (ctx.slot,ctx.team)!=(self.slot,self.team):raise ValueError('shop hint player/team changed')
        if self.server is not server or self.socket is not socket:
            self.server=server;self.socket=socket;self.subscribed=False;self.next_request=0.
        key=f'_read_hints_{ctx.team}_{ctx.slot}'
        known=ctx.stored_data.get(key,())
        if known is None:known=()
        if type(known)not in (list,tuple,set,frozenset):raise ValueError('shop hint server set')
        confirmed=set()
        for hint in known:
            field=lambda name:hint.get(name)if isinstance(hint,dict)else getattr(hint,name,None)
            location=field('location');finding=field('finding_player')
            if type(location)is int and type(finding)is int and finding==ctx.slot and location in observed:confirmed.add(location)
        self.commit(observed,hinted|confirmed);pending=sorted(observed-set(self.state['hinted']))
        if not pending:return 'idle'
        if not self.subscribed:
            ctx.stored_data_notification_keys.add(key)
            await ctx.send_msgs([dict(cmd='SetNotify',keys=[key]),dict(cmd='Get',keys=[key])])
            if ctx.server is not server or getattr(server,'socket',None)is not socket:return 'waiting_server'
            self.subscribed=True
            # First obtain existing server hints before creating anything.
            self.next_request=now+.5;return 'waiting_server_hints'
        if now>=self.next_request:
            self.next_request=now+10.
            await ctx.send_msgs([dict(cmd='LocationScouts',locations=pending[i:i+64],create_as_hint=2)
                                 for i in range(0,len(pending),64)]+[dict(cmd='Get',keys=[key])])
        return 'waiting_confirmation'
