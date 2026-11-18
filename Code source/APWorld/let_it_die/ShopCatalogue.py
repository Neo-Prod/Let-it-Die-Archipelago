"""Full ShopSanity generation/consumer contract, not a native producer."""
from dataclasses import dataclass
from . import ShopMomokaCatalogue as Momoka,ShopTowerCatalogue as Tower
from .ShopPurchase import parse_catalogue
from .CheckWire import integer

@dataclass(frozen=True)
class ShopLocation:
    code:int
    name:str
    kind:str
    floor:int
    room:str

def build(goal):
    return (tuple(ShopLocation(row.code,row.name,'momoka_kc',row.unlock_floor,row.unlock_room) for row in Momoka.locations(goal))+
            tuple(ShopLocation(row.code,row.name,row.kind,row.floor,row.room) for row in Tower.offers(goal)))

def slot_packets(goal):
    return dict(shops=Momoka.packet(goal),tower_shops=Tower.packet(goal))

def validate(slot_data,goal):
    tracker=slot_data.get('tracker',{});version=tracker.get('shopsanity_version',0)
    integer(version,0,1)
    if not version:
        if 'tower_shops' in slot_data:raise ValueError('tower descriptor needs ShopSanity version')
        return parse_catalogue(slot_data.get('shops')),()
    options=tracker.get('options')
    if type(options) is not dict:raise ValueError('ShopSanity tracker options required')
    enabled=integer(options.get('shop_sanity'),0,1)
    if not enabled:
        if 'shops' in slot_data or 'tower_shops' in slot_data:raise ValueError('disabled ShopSanity has descriptors')
        return (),()
    expected=slot_packets(goal)
    if type(slot_data.get('shops')) is not dict or slot_data['shops']!=expected['shops']:raise ValueError('Momoka descriptor differs from local definitions')
    momoka=parse_catalogue(slot_data['shops'])
    tower=Tower.parse_packet(slot_data.get('tower_shops'),goal)
    return momoka,tower
