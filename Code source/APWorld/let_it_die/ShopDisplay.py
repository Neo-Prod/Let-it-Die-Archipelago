"""Authoritative shop reward scouting/display; never delivers a reward.

Use CommonClient's normal LocationInfo/name tables after verified native GRANT.
AP supplies IDs/names/receiver, not image URLs. Native icons are representative
base-family keys, NEVER a remote player's inferred next progressive tier.
"""
import struct
from dataclasses import dataclass
from pathlib import Path
from collections.abc import Mapping
from .CheckWire import Context,integer
from .CheckInbox import atomic
from . import ShopMomokaCatalogue as Momoka,ShopTowerCatalogue as Tower
from .BlueprintCatalog import FAMILIES as BLUEPRINTS
from .DecalCatalog import FAMILIES as DECALS
from .ArmorSets import SETS

GAME='LET IT DIE'
FILE='shop_client_display.bin'
HEADER=256;ROW=1792;MAX_ROWS=210;MAX_SIZE=HEADER+ROW*MAX_ROWS
ICONS={code:(1,tiers[0]) for code,tiers in BLUEPRINTS}
ICONS.update({code:(2,skill) for code,skill in DECALS})
for _set in SETS:
    ICONS[_set['ap_id']]=(3,dict(BLUEPRINTS)[_set['member_ids'][0]][0])

@dataclass(frozen=True)
class DisplayRow:
    location:int
    item:int
    player:int
    flags:int
    name:str
    recipient:str
    game:str
    scope:int
    offer:int

def icon(item,game):
    return ICONS.get(item,(0,'')) if game==GAME else (0,'')

def _wide(raw,at,capacity,value):
    if type(value) is not str or not value or '\0' in value:raise ValueError('shop display text')
    encoded=value.encode('utf-16-le',errors='strict');units=len(encoded)//2
    if units>=capacity:raise ValueError('shop display text capacity')
    raw[at:at+len(encoded)]=encoded
    return units

def offers(config):
    if config.shop_offers!=Momoka.offers(config.goal) or config.tower_offers!=Tower.offers(config.goal):
        raise ValueError('shop display requires complete verified catalogue')
    selected={row.location:(1,row.offer) for row in config.shop_offers}
    for row in config.tower_offers:
        if row.code in selected:raise ValueError('shop display duplicate location')
        selected[row.code]=(2,row.offer)
    if not selected or len(selected)>MAX_ROWS:raise ValueError('shop display row count')
    return selected

def checksum(raw):
    value=2166136261
    for index,byte in enumerate(raw):
        if not 168<=index<172:value=((value^byte)*16777619)&0xffffffff
    return value

def encode(context,config,slot,rows):
    if type(context) is not Context:raise ValueError('shop display Context')
    context.validate();integer(slot,1,2**31-1);selected=offers(config)
    if type(rows) is not tuple or len(rows)!=len(selected):raise ValueError('shop display complete rows required')
    by_id={}
    for row in rows:
        if type(row) is not DisplayRow or row.location in by_id:raise ValueError('shop display duplicate/type')
        integer(row.location,1,2**53-1);integer(row.scope,1,2);integer(row.offer,0,4095)
        if (row.scope,row.offer)!=selected.get(row.location):raise ValueError('shop display carrier mismatch')
        integer(row.item,-2**63,2**63-1);integer(row.player,1,2**31-1);integer(row.flags,0,7)
        by_id[row.location]=row
    if set(by_id)!=set(selected):raise ValueError('shop display location set')
    raw=bytearray(HEADER+len(rows)*ROW);raw[:8]=b'LIDSHUI1'
    struct.pack_into('<IIIIQ',raw,8,1,len(raw),context.pid,context.client_pid,context.birth)
    raw[32:48]=context.nonce;raw[48:64]=context.writer;raw[64:96]=context.run;raw[96:128]=context.config
    fingerprint=bytes.fromhex(config.fingerprint)
    if len(fingerprint)!=32 or not any(fingerprint):raise ValueError('shop display fingerprint')
    raw[128:160]=fingerprint;struct.pack_into('<III',raw,160,len(rows),ROW,0);struct.pack_into('<I',raw,172,slot)
    for index,location in enumerate(sorted(by_id)):
        row=by_id[location];at=HEADER+index*ROW;kind,key=icon(row.item,row.game)
        struct.pack_into('<QqIIIII',raw,at,row.location,row.item,row.player,row.flags,kind,row.scope,row.offer)
        name=_wide(raw,at+160,512,row.name);player=_wide(raw,at+1184,128,row.recipient);game=_wide(raw,at+1440,128,row.game)
        struct.pack_into('<III',raw,at+36,name,player,game)
        key_bytes=key.encode('ascii')
        if len(key_bytes)>=96:raise ValueError('shop display icon key')
        raw[at+64:at+64+len(key_bytes)]=key_bytes
    struct.pack_into('<I',raw,168,checksum(raw));return bytes(raw)

class ShopDisplayService:
    def __init__(self,directory,context,config):
        if type(context) is not Context:raise ValueError('shop display Context')
        context.validate();self.directory=Path(directory);self.context=context;self.config=config
        self.selected=offers(config);self.next_scout=0.;self.attempts=0;self.published=False;self.slot=None
        self.server=None;self.socket=None

    async def step(self,ctx,now):
        if self.published:return 'ready'
        if ctx.server is None:return 'waiting_server'
        socket=getattr(ctx.server,'socket',None)
        if self.server is not ctx.server or self.socket is not socket:
            self.server=ctx.server;self.socket=socket;self.attempts=0;self.next_scout=0.
        integer(ctx.slot,1,2**31-1)
        if self.slot is None:self.slot=ctx.slot
        if ctx.slot!=self.slot:raise ValueError('shop display player changed; restart required')
        cache=ctx.locations_info;slots=ctx.slot_info;names=ctx.item_names;aliases=ctx.player_names
        if not isinstance(cache,Mapping) or not isinstance(slots,Mapping) or not isinstance(aliases,Mapping):
            raise ValueError('shop display client tables unavailable')
        if ctx.slot not in slots or slots[ctx.slot].game!=GAME:raise ValueError('shop display source slot game')
        missing=[location for location in sorted(self.selected) if location not in cache]
        if missing:
            if self.attempts<12 and now>=self.next_scout:
                # No hint creation, chat, ReceivedItems or checked-state edits.
                self.attempts+=1;self.next_scout=now+5.
                await ctx.send_msgs([dict(cmd='LocationScouts',locations=missing[i:i+64],create_as_hint=0)
                                     for i in range(0,len(missing),64)])
            return 'waiting_scouts'
        rows=[]
        for location in sorted(self.selected):
            item=cache[location]
            if item.location!=location:raise ValueError('shop display LocationInfo identity')
            integer(item.item,-2**63,2**63-1);integer(item.player,1,2**31-1);integer(item.flags,0,7)
            if item.player not in slots:return 'waiting_names'
            game=slots[item.player].game
            if type(game) is not str or not game:raise ValueError('shop display recipient game')
            # Check the real mapping before lookup: do not freeze its fallback
            # "Unknown item" string while DataPackage is still arriving.
            if game not in names or item.item not in names[game]:return 'waiting_names'
            name=names.lookup_in_game(item.item,game)
            recipient=aliases.get(item.player,slots[item.player].name)
            scope,offer=self.selected[location]
            rows.append(DisplayRow(location,item.item,item.player,item.flags,name,recipient,game,scope,offer))
        raw=encode(self.context,self.config,self.slot,tuple(rows))
        atomic(self.directory/FILE,raw);self.published=True;return 'ready'
