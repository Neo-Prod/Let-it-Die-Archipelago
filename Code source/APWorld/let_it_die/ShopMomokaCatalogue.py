"""Frozen permanent Momoka KC location identities and generated descriptors.

Local package/Kismet/master provenance is embedded in the JSON. Static evidence
is not an in-game stock test. No active world, native hook, wallet or delivery
changes; the complete ShopSanity generator must compose tower locations too.
Master prices are descriptive; real UI/native debit prices stay authoritative.
"""
from dataclasses import dataclass
import json
import pkgutil
from .CheckWire import GAME,integer
from .ShopPurchase import Offer,catalogue_fingerprint
from .ShopCatalogueBuild import catalogue_build_valid

BASE=7500001
UNLOCKS={
    'KGF_FIRST_PLAY':(1,'Waiting Room'),
    'KGF_ARC_FIRSTTIME':(11,'11F MAKA-TSUTSUMI'),
    'KGF_AMS_FIRST_MATINEE':(21,'21F AKAMI'),
    'KGF_RFT_FIRST_TIME':(31,'31F OBARA'),
}
@dataclass(frozen=True)
class MomokaLocation:
    offer:int
    code:int
    name:str
    native_skill:str
    unlock_flag:str
    unlock_floor:int
    unlock_room:str
    price_kc:int

def _load():
    raw=pkgutil.get_data(__package__,'ShopMomokaCatalogue.json')
    if raw is None:raise ValueError('Momoka catalogue resource missing')
    data=json.loads(raw)
    if not catalogue_build_valid(data):
        raise ValueError('Momoka catalogue build identity')
    rows=data.get('rows')
    if type(rows) is not list or len(rows)!=26:raise ValueError('Momoka permanent catalogue capacity')
    result=tuple(MomokaLocation(**row) for row in rows)
    for index,row in enumerate(result):
        integer(row.offer,0,25);integer(row.code,BASE,BASE+25);integer(row.price_kc,1,0x7fffffff)
        if row.offer!=index or row.code!=BASE+index or row.name!='Momoka KC - '+row.native_skill:
            raise ValueError('Momoka stable identity/order')
        integer(row.unlock_floor,1,31)
        if (row.unlock_floor,row.unlock_room)!=UNLOCKS.get(row.unlock_flag):
            raise ValueError('Momoka unlock witness')
    catalogue_fingerprint(tuple(Offer(row.offer,row.code,row.native_skill) for row in result))
    if [row.native_skill for row in result]!=sorted(row.native_skill for row in result):
        raise ValueError('Momoka SKU ordering')
    return result

ALL_LOCATIONS=_load()
LOCATION_NAME_TO_ID={row.name:row.code for row in ALL_LOCATIONS}

def locations(goal):
    integer(goal,10,40)
    if goal not in (10,20,30,40):raise ValueError('Momoka goal')
    return tuple(row for row in ALL_LOCATIONS if row.unlock_floor<=goal)

def offers(goal):
    return tuple(Offer(row.offer,row.code,row.native_skill) for row in locations(goal))

def packet(goal):
    """Exact R248 client contract; not automatically enabled by this module."""
    selected=offers(goal)
    return dict(schema=1,offers=[dict(offer=row.offer,location=row.location,skill=row.skill) for row in selected],
                catalogue_sha256=catalogue_fingerprint(selected).hex())
