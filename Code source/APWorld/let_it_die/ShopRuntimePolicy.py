"""One explicit enabled/disabled policy after the existing verified GRANT.

Not a connection handshake or a reward source. Binds the same native Context
and complete validated check configuration; native polling is background only.
"""
import hashlib,struct
from pathlib import Path
from .CheckWire import Context,integer
from .CheckInbox import atomic
from . import ShopMomokaCatalogue as Momoka,ShopTowerCatalogue as Tower

FILE='shop_client_policy.bin'
MAGIC=b'LIDSHPM1'
SIZE=192

def checksum(raw):
    value=2166136261
    for index,byte in enumerate(raw):
        if not 168<=index<172:value=((value^byte)*16777619)&0xffffffff
    return value

def encode(context,config):
    if type(context) is not Context:raise ValueError('shop policy Context')
    context.validate();goal=integer(config.goal,10,40)
    if goal%10:raise ValueError('shop policy goal')
    enabled=bool(config.shop_offers or config.tower_offers)
    if enabled and (config.shop_offers!=Momoka.offers(goal) or config.tower_offers!=Tower.offers(goal)):
        raise ValueError('shop policy requires full catalogue')
    fingerprint=bytes.fromhex(config.fingerprint)
    if len(fingerprint)!=32 or not any(fingerprint):raise ValueError('shop policy fingerprint')
    raw=bytearray(SIZE);raw[:8]=MAGIC
    struct.pack_into('<IIIIQ',raw,8,1,SIZE,context.pid,context.client_pid,context.birth)
    raw[32:48]=context.nonce;raw[48:64]=context.writer
    raw[64:96]=context.run;raw[96:128]=context.config;raw[128:160]=fingerprint
    struct.pack_into('<II',raw,160,int(enabled),goal)
    struct.pack_into('<I',raw,168,checksum(raw))
    return bytes(raw)

def verify(raw,context,config):
    if type(raw) is not bytes or raw!=encode(context,config):raise ValueError('shop policy mismatch')
    return True

def publish(directory,context,config):
    raw=encode(context,config);atomic(Path(directory)/FILE,raw)
    return hashlib.sha256(raw).hexdigest()
