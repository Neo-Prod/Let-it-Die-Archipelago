"""Research material codec. No game, network or filesystem operations."""
from dataclasses import dataclass
import hashlib,struct
from .CheckWire import Context,Frame,SIZE,MAGIC,GAME,integer,checksum,ascii_bytes,read_ascii
from .CheckMapIds import physical_floor
from .MaterialIdentity import MATERIALS,BY_ID
MATERIAL=6

@dataclass(frozen=True)
class MaterialFrame:
    context:Context
    sequence:int
    epoch:int
    slot:int
    unique:int
    actor_name:int
    actor_number:int
    ordinal:int
    gettime:int
    source_kind:int
    floor:str
    area:str
    rotation:str
    entity_id:str
    item_id:str
    @property
    def kind(self):return MATERIAL
    @property
    def identity(self):
        self.validate();return MATERIALS[self.ordinal-1]
    def validate(self):
        if type(self.context) is not Context:raise ValueError('context type')
        self.context.validate();integer(self.sequence,1,2**64-1);integer(self.epoch,1,2**64-1)
        integer(self.slot,0,495);integer(self.unique,0,2**31-1);integer(self.actor_name,0,2**32-1);integer(self.actor_number,0,2**32-1)
        integer(self.ordinal,1,46);integer(self.gettime,-2**31,2**31-1);integer(self.source_kind,0,4)
        for key,n in (('floor',16),('area',32),('rotation',16),('entity_id',96),('item_id',96)):ascii_bytes(getattr(self,key),n)
        physical_floor(self.floor,self.area)
        if self.rotation!='4HMA' or BY_ID.get(self.item_id)!=self.ordinal:raise ValueError('material identity/rotation')
    def encode(self):
        self.validate();p=bytearray(SIZE);p[:8]=MAGIC
        struct.pack_into('<IIIIQ',p,8,1,MATERIAL,self.context.pid,0,self.context.birth)
        p[32:48]=self.context.nonce;p[48:80]=self.context.run;p[80:112]=self.context.config
        struct.pack_into('<QQ',p,112,self.sequence,self.epoch);p[128:160]=GAME
        struct.pack_into('<IiIII',p,160,self.slot,self.unique,self.actor_name,self.actor_number,self.ordinal)
        struct.pack_into('<iI',p,184,self.gettime,self.source_kind)
        for key,at,n in (('floor',192,16),('area',208,32),('rotation',240,16),('entity_id',256,96),('item_id',352,96)):p[at:at+n]=ascii_bytes(getattr(self,key),n)
        p[852:868]=self.context.writer;struct.pack_into('<I',p,868,self.context.client_pid);struct.pack_into('<I',p,848,checksum(p));return bytes(p)
    @classmethod
    def decode(cls,p):
        if type(p) is not bytes or len(p)!=SIZE or p[:8]!=MAGIC or struct.unpack_from('<II',p,8)!=(1,MATERIAL) or p[128:160]!=GAME or struct.unpack_from('<I',p,848)[0]!=checksum(p):raise ValueError('material frame size/kind/version/build/checksum')
        pid=struct.unpack_from('<I',p,16)[0];birth=struct.unpack_from('<Q',p,24)[0]
        context=Context(pid,birth,p[32:48],p[48:80],p[80:112],struct.unpack_from('<I',p,868)[0],p[852:868])
        sequence,epoch=struct.unpack_from('<QQ',p,112);slot,unique,actor_name,actor_number,ordinal=struct.unpack_from('<IiIII',p,160);gettime,source_kind=struct.unpack_from('<iI',p,184)
        result=cls(context,sequence,epoch,slot,unique,actor_name,actor_number,ordinal,gettime,source_kind,
                   read_ascii(p[192:208]),read_ascii(p[208:240]),read_ascii(p[240:256]),read_ascii(p[256:352]),read_ascii(p[352:448]))
        result.validate()
        if result.encode()!=p:raise ValueError('noncanonical/reserved material payload')
        return result
    def event_id(self):
        self.validate()
        # Same queue-wide sequence/context namespace as legacy event kinds.
        return hashlib.sha256(b'LID-DEATH-EVENT-1'+self.context.key()+struct.pack('<Q',self.sequence)).hexdigest()

def decode_frame(raw,catalogue_version):
    integer(catalogue_version,1,5)
    if type(raw) is not bytes or len(raw)!=SIZE:raise ValueError('frame bytes/size')
    if struct.unpack_from('<I',raw,12)[0]==MATERIAL:
        if catalogue_version!=5:raise ValueError('materials require catalogue5')
        return MaterialFrame.decode(raw)
    return Frame.decode(raw)
