"""Research prototype: eligible deaths -> durable pending AP checks.

No game access, connection changes, save edits, log scraping or automatic
network task. Native production evidence/transport and generation remain separate.
"""
from __future__ import annotations
from dataclasses import asdict, dataclass
import hashlib,json,os,re,tempfile
from .CheckMapIds import area_for_floor
from pathlib import Path

TUBER_DEFAULTS={"Scratch":20,"Hovering":5,"Bullet":10,"Bone":3,"Reversal":1,"Treasure":1}
TUBER_CLASSES={"BrgPawn_"+name:name for name in ("Scratch","Hovering","Bone","Reversal","Treasure")}
TUBER_CLASSES["BrgPawn_Turret"]="Bullet"
MAJORS={"BrgPawn_BossMax":"MAX_SHARP","BrgPawn_BossJackson":"JACKSON","BrgPawn_BossCrowley":"CROWLEY","BrgPawn_BossGunkan":"GUNKANYAMA"}
MINIS={"BrgPawn_MiddleBossHunk":"COEN","BrgPawn_MiddleBossJindie":"JIN_DIE","BrgPawn_MiddleBossGoto09":"GOTO9","BrgPawn_MiddleBossU10":"U10"}
GOALS={10:"MAX_SHARP",20:"JACKSON",30:"CROWLEY",40:"GUNKANYAMA"}
VERSION_GOAL={1:10,2:20,3:30,4:30,5:40} # User correction: only V-3 moved to 30.
MAX_BYTES=16*1024*1024;MAX_EVENTS=100000

def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode('ascii')).hexdigest()
def integer(value,lo,hi):
    if type(value) is not int or not lo<=value<=hi:raise ValueError('invalid integer')
    return value
def hexid(value):
    if type(value) is not str or re.fullmatch('[0-9a-f]{64}',value) is None:raise ValueError('invalid identity')
    return value
def version(label):
    if type(label) is not str:return 0
    m=re.fullmatch(r'V-([1-5])',label.strip().upper());return int(m[1]) if m else 0

@dataclass(frozen=True)
class Death:
    run:str
    event_id:str # Durable producer-session + map incarnation + sequence digest, not a raw pointer.
    floor:int
    area:str
    classes:tuple[str,...]
    rotation:str
    accepted:bool
    removing:bool
    label:str=''
    damage:str=''

@dataclass(frozen=True)
class Visit:
    run:str
    event_id:str
    floor:int
    area:str
    rotation:str

@dataclass(frozen=True)
class Chest:
    run:str
    event_id:str
    floor:int
    area:str
    rotation:str
    color:str


def classify(event):
    chain=event.classes
    for cls,boss in MAJORS.items():
        if cls in chain:return 'major',boss,0
    if any('U10Tail' in c or c.startswith(('BrgPawn_Jackal','BrgPawn_FourForcemen')) for c in chain):return 'unknown','',0
    for cls,boss in MINIS.items():
        if cls in chain:return 'miniboss',boss,0
    for cls,family in TUBER_CLASSES.items():
        if cls in chain:return 'tuber',family,version(event.label)
    if 'BrgPawn_CustomCharaAIZombie' in chain:return 'screamer','',0
    if 'BrgPawn_CustomCharaAI' in chain:return 'hater','',0
    return 'unknown','',0

@dataclass(frozen=True)
class Location:
    code:int
    kind:str
    floor:int=0
    area:str=''
    tag:str=''
    version:int=0
    ordinal:int=0
    zone:int=0

def tuber_slots(quantities,available_versions):
    """No geography guessed. Caller supplies the versions valid for this run."""
    if set(quantities)!=set(TUBER_DEFAULTS) or set(available_versions)!=set(TUBER_DEFAULTS):raise ValueError('six families required')
    slots=[]
    for family in TUBER_DEFAULTS:
        quantity=integer(quantities[family],0,1000)
        versions=available_versions[family]
        if type(versions) is not tuple or len(set(versions))!=len(versions):raise ValueError('invalid version list')
        for v in sorted(versions):
            integer(v,1,5);slots.extend((family,v,index) for index in range(1,quantity+1))
    return tuple(slots)

def tuber_slots_for_goal(quantities,goal_floor):
    if goal_floor not in GOALS:raise ValueError('invalid goal')
    versions=tuple(v for v,minimum in VERSION_GOAL.items() if minimum<=goal_floor)
    return tuber_slots(quantities,{family:versions for family in TUBER_DEFAULTS})

def screamer_counts(item_count,fixed_count,goal_floor):
    integer(item_count,0,100000);integer(fixed_count,0,100000)
    if goal_floor not in GOALS:raise ValueError('invalid goal')
    bands=goal_floor//10;base,remainder=divmod(max(0,item_count-fixed_count),bands)
    return tuple(base+(i<remainder) for i in range(bands))

class CheckOutbox:
    def __init__(self,path:Path,run:str,goal_floor:int,active_ids,locations,chance=25,pity=False):
        self.path=Path(path);self.run=hexid(run)
        if goal_floor not in GOALS:raise ValueError('invalid goal')
        self.goal_floor=goal_floor;self.chance=integer(chance,1,100)
        if type(pity) is not bool:raise ValueError('pity must be boolean')
        self.pity=pity;self.pity_limit=(100+self.chance-1)//self.chance
        self.active=frozenset(integer(i,1,2**53-1) for i in active_ids)
        self.locations=tuple(sorted(locations,key=lambda s:s.code));keys=set();codes=set()
        for s in self.locations:
            integer(s.code,1,2**53-1)
            if s.code not in self.active or s.code in codes:raise ValueError('inactive/duplicate location')
            codes.add(s.code)
            if s.kind in ('hater','miniboss','room_visit'):
                integer(s.floor,1,40)
                if not area_for_floor(s.area,s.floor):raise ValueError('invalid map')
                if s.kind in ('hater','room_visit') and s.tag:raise ValueError('unexpected map subtype')
                if s.kind=='miniboss' and s.tag not in MINIS.values():raise ValueError('invalid boss')
                key=(s.kind,s.floor,s.area,s.tag)
            elif s.kind=='screamer':
                integer(s.zone,0,3);integer(s.ordinal,1,2500);key=(s.kind,s.zone,s.ordinal)
            elif s.kind=='tuber':
                if s.tag not in TUBER_DEFAULTS:raise ValueError('invalid family')
                integer(s.version,1,5);integer(s.ordinal,1,1000);key=(s.kind,s.tag,s.version,s.ordinal)
            elif s.kind=='chest':
                if s.tag not in ('Red','Silver','Gold') or s.floor or s.area or s.version:raise ValueError('invalid chest category')
                integer(s.zone,0,goal_floor//10-1);integer(s.ordinal,1,1000)
                key=(s.kind,s.tag,s.zone,s.ordinal)
            else:raise ValueError('unsupported category')
            if key in keys:raise ValueError('ambiguous location')
            keys.add(key)
        descriptor={'goal':goal_floor,'chance':chance,'active':sorted(self.active),'locations':[asdict(s) for s in self.locations]}
        if pity:descriptor['pity']=True
        self.config=digest(descriptor)
        self.state={'v':1,'run':run,'config':self.config,'events':{},'pending':[],'confirmed':[],'goal':False}
        if pity:self.state['pity']={str(zone):0 for zone in range(4)}
        if self.path.exists():
            if self.path.stat().st_size>MAX_BYTES:raise ValueError('journal too large')
            raw=self.path.read_bytes()
            if len(raw)>MAX_BYTES:raise ValueError('journal too large')
            wrapper=json.loads(raw);state=wrapper['state']
            if wrapper['sha256']!=digest(state):raise ValueError('journal checksum')
            if state.get('v')!=1 or state.get('run')!=run or state.get('config')!=self.config:raise ValueError('journal context mismatch')
            if type(state.get('events')) is not dict or len(state['events'])>MAX_EVENTS or type(state.get('goal')) is not bool:raise ValueError('journal state')
            for event_id,entry in state['events'].items():
                hexid(event_id);hexid(entry['payload'])
                if any(integer(i,1,2**53-1) not in self.active for i in entry['locations']):raise ValueError('invalid event result')
            for key in ('pending','confirmed'):
                ids=state[key]
                if type(ids) is not list or len(set(ids))!=len(ids) or any(integer(i,1,2**53-1) not in self.active for i in ids):raise ValueError('invalid journal locations')
            if set(state['pending'])&set(state['confirmed']):raise ValueError('journal overlap')
            if pity:
                counters=state.get('pity')
                if type(counters) is not dict or set(counters)!={'0','1','2','3'}:raise ValueError('pity journal')
                for value in counters.values():integer(value,0,self.pity_limit-1)
            elif 'pity' in state:raise ValueError('unexpected pity state')
            self.state=state
    def _store(self,state):
        raw=json.dumps({'state':state,'sha256':digest(state)},sort_keys=True,separators=(',',':')).encode('ascii')
        if len(raw)>MAX_BYTES:raise ValueError('journal capacity; producer must retain batch')
        self.path.parent.mkdir(parents=True,exist_ok=True)
        fd,name=tempfile.mkstemp(dir=self.path.parent,prefix='checks-',suffix='.tmp')
        try:
            with os.fdopen(fd,'wb') as f:f.write(raw);f.flush();os.fsync(f.fileno())
            os.replace(name,self.path)
        finally:
            if os.path.exists(name):os.unlink(name)
        self.state=state
    def record(self,event:Death):
        if type(event) is Visit:return self.record_visit(event)
        if type(event) is Chest:return self.record_chest(event)
        if type(event) is not Death or hexid(event.run)!=self.run:raise ValueError('foreign run')
        hexid(event.event_id);integer(event.floor,1,40)
        if type(event.classes) is not tuple or not 1<=len(event.classes)<=32 or any(type(c) is not str or not re.fullmatch(r'BrgPawn_[A-Za-z0-9_]+',c) for c in event.classes):raise ValueError('invalid classes')
        if type(event.accepted) is not bool or type(event.removing) is not bool or not area_for_floor(event.area,event.floor):raise ValueError('invalid event')
        if type(event.label) is not str or len(event.label)>96 or type(event.damage) is not str or len(event.damage)>128 or type(event.rotation) is not str or len(event.rotation)>32:raise ValueError('invalid event text')
        payload=digest(asdict(event));old=self.state['events'].get(event.event_id)
        if old:
            if old['payload']!=payload:raise ValueError('conflicting event identity')
            return tuple(old['locations'])
        if len(self.state['events'])==MAX_EVENTS:raise ValueError('event capacity; producer must retain batch')
        state=json.loads(json.dumps(self.state));result=[];kind,tag,v=classify(event)
        eligible=event.accepted and not event.removing and event.damage!='BrgDamageType_Destroy' and event.rotation=='4HMA' and event.floor<=self.goal_floor
        # Active catalogue membership below owns family/version eligibility.
        # Legacy catalogues naturally contain only their legacy versions.
        if kind=='tuber' and not v:eligible=False
        if eligible:
            used=set(state['pending'])|set(state['confirmed'])
            screamer_win=False
            if kind=='screamer':
                zone=(event.floor-1)//10
                available=any(s.kind=='screamer' and s.zone==zone and s.code not in used for s in self.locations)
                if available:
                    roll=int(hashlib.sha256((self.run+event.event_id).encode('ascii')).hexdigest()[:16],16)%100
                    streak=state['pity'][str(zone)]+1 if self.pity else 0
                    screamer_win=roll<self.chance or (self.pity and streak>=self.pity_limit)
                    if self.pity:state['pity'][str(zone)]=0 if screamer_win else streak
            if kind=='major' and event.floor==self.goal_floor and tag==GOALS[self.goal_floor]:state['goal']=True
            candidates=[]
            for s in self.locations:
                if s.code in used:continue
                if s.kind in ('hater','miniboss') and s.floor>self.goal_floor:continue
                if s.kind==kind=='hater' and s.floor==event.floor and s.area==event.area:candidates.append(s)
                elif s.kind==kind=='miniboss' and s.floor==event.floor and s.area==event.area and s.tag==tag:candidates.append(s)
                elif s.kind==kind=='tuber' and s.tag==tag and s.version==v:candidates.append(s)
                elif s.kind==kind=='screamer' and s.zone==(event.floor-1)//10:
                    if screamer_win:candidates.append(s)
            if candidates:result=[min(candidates,key=lambda s:(s.ordinal,s.code)).code]
        state['pending']=sorted(set(state['pending'])|set(result));state['events'][event.event_id]={'payload':payload,'locations':result}
        self._store(state);return tuple(result)
    def record_visit(self,event:Visit):
        if type(event) is not Visit or hexid(event.run)!=self.run:raise ValueError('foreign visit run')
        hexid(event.event_id);integer(event.floor,1,40)
        if not area_for_floor(event.area,event.floor) or type(event.rotation) is not str or len(event.rotation)>32:raise ValueError('invalid visit')
        payload=digest(asdict(event));old=self.state['events'].get(event.event_id)
        if old:
            if old['payload']!=payload:raise ValueError('conflicting event identity')
            return tuple(old['locations'])
        if len(self.state['events'])==MAX_EVENTS:raise ValueError('event capacity; producer must retain batch')
        state=json.loads(json.dumps(self.state));result=[]
        if event.rotation=='4HMA' and event.floor<=self.goal_floor:
            used=set(state['pending'])|set(state['confirmed'])
            result=[s.code for s in self.locations if s.kind=='room_visit' and s.floor==event.floor and s.area==event.area and s.code not in used]
            if len(result)>1:raise ValueError('ambiguous visit')
        state['pending']=sorted(set(state['pending'])|set(result));state['events'][event.event_id]={'payload':payload,'locations':result}
        self._store(state);return tuple(result)
    def record_chest(self,event:Chest):
        if type(event) is not Chest or hexid(event.run)!=self.run:raise ValueError('foreign chest run')
        hexid(event.event_id);integer(event.floor,1,40)
        if not area_for_floor(event.area,event.floor) or event.color not in ('Red','Silver','Gold'):raise ValueError('invalid chest')
        if type(event.rotation) is not str or len(event.rotation)>32:raise ValueError('invalid chest rotation')
        payload=digest(asdict(event));old=self.state['events'].get(event.event_id)
        if old:
            if old['payload']!=payload:raise ValueError('conflicting event identity')
            return tuple(old['locations'])
        if len(self.state['events'])==MAX_EVENTS:raise ValueError('event capacity; producer must retain batch')
        state=json.loads(json.dumps(self.state));result=[]
        if event.rotation=='4HMA' and event.floor<=self.goal_floor:
            used=set(state['pending'])|set(state['confirmed'])
            candidates=[s for s in self.locations if s.kind=='chest' and s.tag==event.color
                        and s.zone==(event.floor-1)//10 and s.code not in used]
            if candidates:result=[min(candidates,key=lambda s:(s.ordinal,s.code)).code]
        # An observed opening is authoritative even outside tracker witness rooms.
        state['pending']=sorted(set(state['pending'])|set(result))
        state['events'][event.event_id]={'payload':payload,'locations':result}
        self._store(state);return tuple(result)
    def server_confirmed(self,locations):
        ids={integer(i,1,2**53-1) for i in locations}
        if not ids<=self.active:raise ValueError('server catalog mismatch')
        state=json.loads(json.dumps(self.state));state['confirmed']=sorted(set(state['confirmed'])|ids);state['pending']=sorted(set(state['pending'])-ids);self._store(state)
    def packet(self):
        return {'cmd':'LocationChecks','locations':list(self.state['pending'])} if self.state['pending'] else None
    async def publish(self,send_msgs):
        packet=self.packet()
        if packet:await send_msgs([packet])
        # Successful socket send is NOT a checked_locations acknowledgement.
