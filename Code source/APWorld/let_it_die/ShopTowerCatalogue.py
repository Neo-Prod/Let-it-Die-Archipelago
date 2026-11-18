"""Room-specific tower shop identities. No native payment/UI activation.

Plans are abstract three-per-merchant slots: no guessed vanilla blueprint is
used as their native carrier. Quantities and prices remain runtime concerns.
"""
from dataclasses import asdict,dataclass
import hashlib,json,pkgutil
from .CheckWire import GAME,integer
from .MaterialNames import MATERIAL_NAMES
from .ShopCatalogueBuild import catalogue_build_valid
TUBER_MATERIALS=frozenset(f'ITMT_STONE_TBR_{rank}' for rank in range(1,6))

@dataclass(frozen=True)
class TowerOffer:
    offer:int
    code:int
    kind:str
    room_key:str
    room:str
    floor:int
    shopid:int
    product:str
    native_item:str
    plan_slot:int

    @property
    def name(self):
        label=MATERIAL_NAMES.get(self.native_item,self.native_item) if self.kind=='tower_material' else f'Blueprint Slot {self.plan_slot}'
        return f'Shop - {self.room} - {label}'

def _digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False).encode('ascii')).hexdigest()

def _load():
    raw=pkgutil.get_data(__package__,'ShopTowerCatalogue.json')
    if raw is None:raise ValueError('tower catalogue resource missing')
    data=json.loads(raw)
    if not catalogue_build_valid(data):raise ValueError('tower catalogue build')
    rows=tuple(TowerOffer(**row) for row in data['rows'])
    if len(rows)!=184:raise ValueError('tower catalogue capacity')
    keys=set();names=set();codes=set();plans={}
    for index,row in enumerate(rows):
        integer(row.offer,0,183);integer(row.code,1,2**53-1);integer(row.floor,1,40);integer(row.shopid,1,1000);integer(row.plan_slot,0,3)
        if row.offer!=index or not row.room_key or not row.room.startswith(str(row.floor)+'F '):raise ValueError('tower stable identity')
        if row.kind=='tower_material':
            if index>=163 or row.code!=7501001+index or not row.product or row.native_item not in MATERIAL_NAMES.keys()|TUBER_MATERIALS or row.plan_slot:raise ValueError('tower material row')
            key=(row.room_key,row.shopid,row.product)
        elif row.kind=='tower_plan':
            if index<163 or row.code!=7502001+index-163 or row.product or row.native_item or row.plan_slot not in (1,2,3):raise ValueError('tower abstract plan slot')
            key=(row.room_key,row.shopid,row.plan_slot);plans.setdefault((row.room_key,row.shopid),set()).add(row.plan_slot)
        else:raise ValueError('tower offer kind')
        if key in keys or row.name in names or row.code in codes:raise ValueError('tower duplicate')
        keys.add(key);names.add(row.name);codes.add(row.code)
    if len(plans)!=7 or any(slots!={1,2,3} for slots in plans.values()):raise ValueError('three plan slots per merchant required')
    return rows

ALL_OFFERS=_load()
LOCATION_NAME_TO_ID={row.name:row.code for row in ALL_OFFERS}

def offers(goal):
    integer(goal,10,40)
    if goal not in (10,20,30,40):raise ValueError('tower goal')
    return tuple(row for row in ALL_OFFERS if row.floor<=goal)

def packet(goal):
    rows=[asdict(row) for row in offers(goal)]
    return dict(schema=1,offers=rows,catalogue_sha256=_digest(rows))

def parse_packet(value,goal):
    if type(value) is not dict or value!=packet(goal):raise ValueError('tower descriptor differs from local definitions')
    # Canonical JSON also distinguishes bools from integer IDs/fields.
    if _digest(value)!=_digest(packet(goal)):raise ValueError('tower descriptor type mismatch')
    return offers(goal)
