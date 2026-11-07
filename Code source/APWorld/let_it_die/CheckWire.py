"""Research codec, no process/game/network access. Fixed little-endian frames."""
from dataclasses import dataclass,replace
import hashlib,re,struct
from .CheckMapIds import physical_floor

SIZE=1024;EVENT=1;ACK=2;FAULT=3;VISIT=4;CHEST=5;MAGIC=b'LIDKIL01'
GAME=bytes.fromhex('f33e91e6e8d58566bfabad8d01cd0486586a0d43fe1c58e65fc6485a417bddfd')
CLASS_NAMES=('BrgPawn_CustomCharaAI','BrgPawn_CustomCharaAIZombie','BrgPawn_NormalEnemyBase','BrgPawn_Scratch','BrgPawn_Hovering','BrgPawn_Turret','BrgPawn_Bone','BrgPawn_Reversal','BrgPawn_Treasure','BrgPawn_MiddleBossHunk','BrgPawn_MiddleBossJindie','BrgPawn_MiddleBossGoto09','BrgPawn_MiddleBossU10','BrgPawn_BossMax','BrgPawn_BossJackson','BrgPawn_BossCrowley','BrgPawn_BossGunkan','BrgPawn_MiddleBossU10Tail','BrgPawn_Jackal','BrgPawn_FourForcemenBase')
def integer(v,lo,hi):
    if type(v) is not int or not lo<=v<=hi:raise ValueError('integer range/type')
    return v
def nonzero(v,n):
    if type(v) is not bytes or len(v)!=n or not any(v):raise ValueError('missing identity bytes')
def checksum(data):
    h=2166136261
    for i,b in enumerate(data):
        if not 848<=i<852:h=((h^b)*16777619)&0xffffffff
    return h
def ascii_bytes(s,n):
    if type(s) is not str or len(s)>=n or any(not 32<=ord(c)<=126 for c in s):raise ValueError('ASCII field')
    return s.encode('ascii').ljust(n,b'\0')
def unicode_bytes(s):
    if type(s) is not str or any(ord(c)<32 or ord(c)==127 for c in s):raise ValueError('UTF16 field')
    data=s.encode('utf-16-le')
    if len(data)>190:raise ValueError('UTF16 length')
    return data.ljust(192,b'\0')
def read_ascii(data):
    end=data.find(b'\0')
    if end<0 or any(data[end:]):raise ValueError('ASCII terminator/padding')
    return data[:end].decode('ascii')
def read_unicode(data):
    units=struct.unpack('<96H',data)
    try:end=units.index(0)
    except ValueError:raise ValueError('UTF16 terminator')
    if any(units[end:]):raise ValueError('UTF16 padding')
    return data[:end*2].decode('utf-16-le')
def mask_valid(frame):
    family=(frame.classes>>3)&63;mini=(frame.classes>>9)&15;major=(frame.classes>>13)&15;ai=frame.classes&3
    if any(n&(n-1) for n in (family,mini,major)):return False
    if family and (mini or major or ai or frame.classes&(1<<17)):return False
    if (mini or major) and (ai or frame.classes&((1<<18)|(1<<19))):return False
    if major and mini and major!=mini:return False
    if frame.classes&(1<<17) and major:return False
    if frame.classes&4 and ai:return False
    if frame.family and family!=1<<(frame.family-1):return False
    if frame.version and frame.label.upper()!=f'V-{frame.version}':return False
    return True

@dataclass(frozen=True)
class Context:
    pid:int
    birth:int
    nonce:bytes
    run:bytes
    config:bytes
    client_pid:int
    writer:bytes
    def validate(self):
        integer(self.pid,1,2**32-1);integer(self.birth,1,2**64-1);integer(self.client_pid,1,2**32-1)
        for data,n in ((self.nonce,16),(self.run,32),(self.config,32),(self.writer,16)):nonzero(data,n)
    def key(self):
        self.validate();return struct.pack('<IQI',self.pid,self.birth,self.client_pid)+self.nonce+self.writer+self.run+self.config

@dataclass(frozen=True)
class Frame:
    kind:int
    context:Context
    sequence:int
    epoch:int=0
    status:int=0
    flags:int=0
    classes:int=0
    name_index:int=0
    name_number:int=0
    raw_kind:int=0
    health:int=0
    max_health:int=0
    floor:str=''
    area:str=''
    rotation:str=''
    damage:str=''
    pawn_class:str=''
    name:str=''
    label:str=''
    family:int=0
    version:int=0
    def validate(self):
        if type(self.context) is not Context:raise ValueError('context type')
        self.context.validate();integer(self.kind,1,5);integer(self.sequence,1,2**64-1);integer(self.epoch,0,2**64-1)
        for key in ('status','flags','classes','name_index','name_number','raw_kind','family','version'):integer(getattr(self,key),0,2**32-1)
        for key in ('health','max_health'):integer(getattr(self,key),-2**31,2**31-1)
        for key,n in (('floor',16),('area',32),('rotation',16),('damage',96),('pawn_class',96)):ascii_bytes(getattr(self,key),n)
        unicode_bytes(self.name);unicode_bytes(self.label)
        if self.kind==CHEST:
            if self.status or not self.epoch or self.rotation!='4HMA' or not 1<=self.raw_kind<=3 or any(getattr(self,k) for k in ('flags','classes','health','max_health','damage','pawn_class','name','label','family','version')):raise ValueError('chest payload')
            physical_floor(self.floor,self.area);return
        if self.kind==VISIT:
            if self.status or not self.epoch or self.rotation!='4HMA' or any(getattr(self,k) for k in ('flags','classes','name_index','name_number','raw_kind','health','max_health','damage','pawn_class','name','label','family','version')):raise ValueError('visit payload')
            physical_floor(self.floor,self.area);return
        if self.kind!=EVENT:
            if not (self.status==1 if self.kind==ACK else 1<=self.status<=4):raise ValueError('receipt/fault status')
            if any(getattr(self,k) for k in ('epoch','flags','classes','name_index','name_number','raw_kind','health','max_health','floor','area','rotation','damage','pawn_class','name','label','family','version')):raise ValueError('control payload must be empty')
            return
        if self.status or not self.epoch or self.flags&~7 or self.flags&3!=3 or self.classes&~0xfffff or self.raw_kind>255 or self.health!=1 or self.max_health<1 or self.family>6 or self.version>5 or (not self.family and self.version):raise ValueError('event flags/ranges')
        if not mask_valid(self):raise ValueError('contradictory class/version hints')
        physical_floor(self.floor,self.area)
        if self.rotation not in ('','4HMA') or re.fullmatch(r'BrgPawn_[A-Za-z0-9_]+',self.pawn_class) is None:raise ValueError('rotation/class')
    def encode(self):
        self.validate();p=bytearray(SIZE);p[:8]=MAGIC
        struct.pack_into('<IIIIQ',p,8,1,self.kind,self.context.pid,self.status,self.context.birth)
        p[32:48]=self.context.nonce;p[48:80]=self.context.run;p[80:112]=self.context.config
        struct.pack_into('<QQ',p,112,self.sequence,self.epoch);p[128:160]=GAME
        struct.pack_into('<IIIII',p,160,self.flags,self.classes,self.name_index,self.name_number,self.raw_kind);struct.pack_into('<ii',p,184,self.health,self.max_health)
        for key,at,n in (('floor',192,16),('area',208,32),('rotation',240,16),('damage',256,96),('pawn_class',352,96)):p[at:at+n]=ascii_bytes(getattr(self,key),n)
        p[448:640]=unicode_bytes(self.name);p[640:832]=unicode_bytes(self.label)
        struct.pack_into('<II',p,832,self.family,self.version);p[852:868]=self.context.writer;struct.pack_into('<I',p,868,self.context.client_pid)
        struct.pack_into('<I',p,848,checksum(p));return bytes(p)
    @classmethod
    def decode(cls,p):
        if type(p) is not bytes or len(p)!=SIZE or p[:8]!=MAGIC or struct.unpack_from('<I',p,8)[0]!=1 or p[128:160]!=GAME or any(p[180:184]+p[840:848]+p[872:]) or struct.unpack_from('<I',p,848)[0]!=checksum(p):raise ValueError('frame size/version/build/checksum/reserved')
        version,kind,pid,status,birth=struct.unpack_from('<IIIIQ',p,8)
        context=Context(pid,birth,p[32:48],p[48:80],p[80:112],struct.unpack_from('<I',p,868)[0],p[852:868])
        sequence,epoch=struct.unpack_from('<QQ',p,112);flags,classes,name_index,name_number,raw_kind=struct.unpack_from('<IIIII',p,160);health,max_health=struct.unpack_from('<ii',p,184)
        result=cls(kind,context,sequence,epoch,status,flags,classes,name_index,name_number,raw_kind,health,max_health,
            read_ascii(p[192:208]),read_ascii(p[208:240]),read_ascii(p[240:256]),read_ascii(p[256:352]),read_ascii(p[352:448]),read_unicode(p[448:640]),read_unicode(p[640:832]),*struct.unpack_from('<II',p,832))
        result.validate()
        if result.encode()!=p:raise ValueError('noncanonical frame')
        return result
    def event_id(self):return hashlib.sha256(b'LID-DEATH-EVENT-1'+self.context.key()+struct.pack('<Q',self.sequence)).hexdigest()
    def to_death(self,death_type):
        if self.kind==CHEST:
            self.validate()
            from .CheckOutbox import Chest
            return Chest(self.context.run.hex(),self.event_id(),physical_floor(self.floor,self.area),self.area,self.rotation,('Red','Silver','Gold')[self.raw_kind-1])
        if self.kind==VISIT:
            from .CheckOutbox import Visit
            return Visit(self.context.run.hex(),self.event_id(),physical_floor(self.floor,self.area),self.area,self.rotation)
        if self.kind!=EVENT or self.rotation!='4HMA':raise ValueError('not a verified rotated death')
        chain=tuple(dict.fromkeys((self.pawn_class,)+tuple(c for bit,c in enumerate(CLASS_NAMES) if self.classes&(1<<bit))))
        return death_type(self.context.run.hex(),self.event_id(),physical_floor(self.floor,self.area),self.area,chain,self.rotation,True,bool(self.flags&4),self.label,self.damage)

def receipt_ack(context,through):return Frame(ACK,context,through,status=1).encode()
def validate_ack(raw,context,previous,published):
    integer(previous,0,2**64-1);integer(published,0,2**64-1)
    frame=Frame.decode(raw)
    if frame.kind!=ACK or frame.context!=context or not 0<=previous<=frame.sequence<=published:raise ValueError('stale/foreign/future receipt acknowledgement')
    return frame.sequence
