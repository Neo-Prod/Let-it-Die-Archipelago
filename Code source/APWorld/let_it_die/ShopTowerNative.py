"""Pinned billing identities only; public AP blueprint slots stay abstract."""
from dataclasses import dataclass
import hashlib,json,pkgutil
from .CheckInbox import unique_object
from .CheckWire import GAME
from .ShopTowerCatalogue import ALL_OFFERS,offers
from .ShopCatalogueBuild import catalogue_build_valid

NATIVE_DIGEST='f0fe99dddf7513c086a635acf6df2eb24124e74a8c295be73df48e30b527287b'

@dataclass(frozen=True)
class NativeOffer:
    offer:int
    location:int
    floor:int
    room_key:str
    shopid:int
    plan_slot:int
    product:str
    native_item:str
    native_kc:int

def _load():
    raw=pkgutil.get_data(__package__,'ShopTowerNative.json')
    if raw is None:raise ValueError('missing tower native bindings')
    value=json.loads(raw,object_pairs_hook=unique_object)
    canonical=json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False).encode('ascii')
    if hashlib.sha256(canonical).hexdigest()!=NATIVE_DIGEST or not catalogue_build_valid(value):
        raise ValueError('tower native binding/build mismatch')
    rows=tuple(NativeOffer(**row) for row in value['rows'])
    if len(rows)!=184:raise ValueError('tower native binding capacity')
    for index,(native,public) in enumerate(zip(rows,ALL_OFFERS)):
        if (native.offer!=index or native.offer!=public.offer or native.location!=public.code or
            native.floor!=public.floor or native.room_key!=public.room_key or native.shopid!=public.shopid or
            native.plan_slot!=public.plan_slot or not 1<=native.native_kc<=0x7fffffff):
            raise ValueError('tower native/public identity mismatch')
        for text,maximum in ((native.native_item,95),(native.product,31)):
            if not 1<=len(text)<=maximum or any(not 33<=ord(c)<=126 for c in text):raise ValueError('tower native text')
        if not public.plan_slot and (public.product!=native.product or public.native_item!=native.native_item):
            raise ValueError('tower material native identity mismatch')
    return rows

ALL_NATIVE=_load()

def for_goal(goal):
    return tuple(ALL_NATIVE[row.offer] for row in offers(goal))
