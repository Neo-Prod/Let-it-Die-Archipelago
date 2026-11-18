"""Bounded session-bound native catalogue; no purchase/reward/native calls."""
import hashlib,struct
from . import ShopMomokaCatalogue as Momoka,ShopTowerCatalogue as Tower
from .CheckWire import Context,integer
from .CheckInbox import atomic

FILE='shop_client_catalogue.bin'
MAGIC=b'LIDSHPC1'
HEADER=256;MOMOKA_ROW=192;TOWER_ROW=256;MAX_SIZE=52352

def _text(raw,at,capacity,value):
    if type(value) is not str or len(value)>=capacity or any(not 33<=ord(c)<=126 for c in value):
        raise ValueError('native catalogue ASCII field')
    raw[at:at+len(value)]=value.encode('ascii')

def body(goal):
    selected=Momoka.locations(goal);tower=Tower.offers(goal)
    raw=bytearray(len(selected)*MOMOKA_ROW+len(tower)*TOWER_ROW)
    for index,row in enumerate(selected):
        at=index*MOMOKA_ROW
        struct.pack_into('<IIQII',raw,at,row.offer,0,row.code,row.price_kc,row.unlock_floor)
        _text(raw,at+24,129,row.native_skill)
    base=len(selected)*MOMOKA_ROW
    for index,row in enumerate(tower):
        at=base+index*TOWER_ROW
        struct.pack_into('<IIQIIII',raw,at,row.offer,1 if row.kind=='tower_material' else 2,row.code,row.floor,row.shopid,row.plan_slot,0)
        _text(raw,at+32,64,row.room_key);_text(raw,at+96,32,row.product);_text(raw,at+128,96,row.native_item)
    return bytes(raw)

def checksum(raw):
    value=2166136261
    for index,byte in enumerate(raw):
        if not 232<=index<236:value=((value^byte)*16777619)&0xffffffff
    return value

def encode(context,config):
    if type(context) is not Context:raise ValueError('native catalogue context type')
    context.validate();goal=integer(config.goal,10,40)
    if config.shop_offers!=Momoka.offers(goal) or config.tower_offers!=Tower.offers(goal):
        raise ValueError('native catalogue requires complete verified ShopSanity config')
    fingerprint=bytes.fromhex(config.fingerprint)
    if len(fingerprint)!=32 or not any(fingerprint):raise ValueError('native catalogue fingerprint')
    rows=body(goal);raw=bytearray(HEADER+len(rows));raw[:8]=MAGIC
    struct.pack_into('<IIIIQ',raw,8,1,len(raw),context.pid,context.client_pid,context.birth)
    raw[32:48]=context.nonce;raw[48:64]=context.writer;raw[64:96]=context.run;raw[96:128]=context.config
    raw[128:160]=fingerprint
    raw[160:192]=bytes.fromhex(Momoka.packet(goal)['catalogue_sha256'])
    raw[192:224]=bytes.fromhex(Tower.packet(goal)['catalogue_sha256'])
    struct.pack_into('<II',raw,224,len(config.shop_offers),len(config.tower_offers));struct.pack_into('<I',raw,236,goal)
    raw[HEADER:]=rows;struct.pack_into('<I',raw,232,checksum(raw))
    if len(raw)>MAX_SIZE:raise ValueError('native catalogue capacity')
    return bytes(raw)

def verify(raw,context,config):
    """Diagnostic Python verification, not proof that the native decoder ran."""
    if type(raw) is not bytes or raw!=encode(context,config):raise ValueError('native catalogue differs from expected session/config')
    return True

def publish(directory,context,config):
    from pathlib import Path
    raw=encode(context,config);atomic(Path(directory)/FILE,raw)
    return hashlib.sha256(raw).hexdigest()
