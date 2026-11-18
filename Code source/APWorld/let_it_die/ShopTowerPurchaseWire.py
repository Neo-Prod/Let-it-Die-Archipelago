"""Tower-only receipts. Pure codec, no game access, persistence or AP send."""
from dataclasses import dataclass
import struct
from .CheckWire import Context,integer,nonzero
from .ShopTowerNative import ALL_NATIVE

MAGIC=b'LIDTSHP1';SIZE=512;STATE_SIZE=224;PAYMENT=1;RECEIVED=2;STATE=3;MAX_OFFERS=184

def checksum(raw,offset):
    result=2166136261
    for i,byte in enumerate(raw):
        if not offset<=i<offset+4:result=((result^byte)*16777619)&0xffffffff
    return result

def _header(raw,context,catalogue,kind):
    context.validate();nonzero(catalogue,32)
    raw[:8]=MAGIC
    struct.pack_into('<IIIIQ',raw,8,1,kind,context.pid,context.client_pid,context.birth)
    raw[32:48],raw[48:64]=context.nonce,context.writer
    raw[64:96],raw[96:128],raw[128:160]=context.run,context.config,catalogue

def _context(raw):
    _,_,pid,client_pid,birth=struct.unpack_from('<IIIIQ',raw,8)
    return Context(pid,birth,raw[32:48],raw[64:96],raw[96:128],client_pid,raw[48:64])

def purchase_state(context,catalogue,bought):
    if type(bought) not in (tuple,list,set,frozenset) or len(bought)>MAX_OFFERS or len(set(bought))!=len(bought):
        raise ValueError('tower bought bitmap shape')
    raw=bytearray(STATE_SIZE);_header(raw,context,catalogue,STATE)
    for offer in bought:
        integer(offer,0,MAX_OFFERS-1);raw[160+offer//8]|=1<<(offer%8)
    struct.pack_into('<I',raw,220,checksum(raw,220));return bytes(raw)

def decode_state(raw,context,catalogue):
    if type(raw) is not bytes or len(raw)!=STATE_SIZE:raise ValueError('tower bitmap size')
    bought=tuple(i for i in range(MAX_OFFERS) if raw[160+i//8]&(1<<(i%8)))
    if purchase_state(context,catalogue,bought)!=raw:raise ValueError('foreign/noncanonical tower bitmap')
    return bought

@dataclass(frozen=True)
class Payment:
    context:Context
    catalogue:bytes
    sequence:int
    offer:int
    location:int
    item:str=''
    product:str=''
    price:int=0
    before_carried:int=0
    after_carried:int=0
    before_free:int=0
    after_free:int=0
    before_paid:int=0
    after_paid:int=0
    debit_result:int=0
    handler_result:int=0
    kind:int=PAYMENT

    def validate(self):
        if type(self.context) is not Context:raise ValueError('tower context type')
        self.context.validate();nonzero(self.catalogue,32)
        integer(self.sequence,1,2**64-1);integer(self.offer,0,MAX_OFFERS-1);integer(self.kind,PAYMENT,RECEIVED)
        integer(self.location,1,2**53-1)
        native=ALL_NATIVE[self.offer]
        if self.location!=native.location:raise ValueError('tower offer/location')
        fields=('price','before_carried','after_carried','before_free','after_free','before_paid','after_paid','debit_result','handler_result')
        for field in fields:integer(getattr(self,field),0,1 if field.endswith('_result') else 0x7fffffff)
        if type(self.item) is not str or type(self.product) is not str:raise ValueError('tower text type')
        if self.kind==RECEIVED:
            if self.item or self.product or any(getattr(self,key) for key in fields):raise ValueError('tower ACK payload')
            return
        if type(self.item) is not str or type(self.product) is not str or self.item!=native.native_item or self.product!=native.product or self.price!=native.native_kc:
            raise ValueError('tower native billing identity')
        used=min(self.before_carried,self.price)
        if (not self.price or self.before_free<self.price-used or self.after_carried!=self.before_carried-used or
            self.after_free!=self.before_free-(self.price-used) or self.after_paid!=self.before_paid or
            self.debit_result!=1 or self.handler_result!=1):raise ValueError('tower payment not completed')

    def encode(self):
        self.validate();raw=bytearray(SIZE);_header(raw,self.context,self.catalogue,self.kind)
        struct.pack_into('<QIIQiiiiiiBBHH',raw,160,self.sequence,self.offer,self.price,self.location,
            self.before_carried,self.after_carried,self.before_free,self.after_free,self.before_paid,self.after_paid,
            self.debit_result,self.handler_result,len(self.item),len(self.product))
        raw[216:216+len(self.item)*2]=self.item.encode('utf-16-le')
        raw[408:408+len(self.product)*2]=self.product.encode('utf-16-le')
        struct.pack_into('<I',raw,508,checksum(raw,508));return bytes(raw)

    @classmethod
    def decode(cls,raw):
        if (type(raw) is not bytes or len(raw)!=SIZE or raw[:8]!=MAGIC or struct.unpack_from('<I',raw,8)[0]!=1 or
            struct.unpack_from('<I',raw,508)[0]!=checksum(raw,508)):raise ValueError('tower frame size/version/checksum')
        seq,offer,price,location,carried,carried_after,free,free_after,paid,paid_after,debit,handler,units,product_units=struct.unpack_from('<QIIQiiiiiiBBHH',raw,160)
        if units>95 or product_units>31:raise ValueError('tower text length')
        item=raw[216:216+units*2].decode('utf-16-le');product=raw[408:408+product_units*2].decode('utf-16-le')
        event=cls(_context(raw),raw[128:160],seq,offer,location,item,product,price,carried,carried_after,
                  free,free_after,paid,paid_after,debit,handler,struct.unpack_from('<I',raw,12)[0])
        event.validate()
        if event.encode()!=raw:raise ValueError('noncanonical tower receipt')
        return event

    def acknowledgement(self):
        return Payment(self.context,self.catalogue,self.sequence,self.offer,self.location,kind=RECEIVED).encode()
