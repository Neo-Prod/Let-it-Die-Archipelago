"""User-reviewed Universal.txt routes and generation/UT reachability.

Numbers refer to EquipmentTiers, never receipt indices or native star rarity.
This module has no game/process, transport, or save access.
"""
from dataclasses import dataclass

from . import Items
from .ArmorSets import SETS
from .EquipmentPool import _catalogue


@dataclass(frozen=True)
class Route:
    name: str
    maps: tuple[str, ...]
    bosses: tuple[str, ...]
    weapon: int
    armor: int
    stats: int

    @property
    def last_floor(self):
        return max(int(name.split('F', 1)[0]) for name in self.maps)


# Canonical map labels from ROOM_VISIT_LOCATIONS. Aliases in Universal.txt:
# 10F "Jackson" = Max Sharp; Hachiyama/Hachiyanma, Sumikyo/Sumikiyo,
# Katsumi/Katsuma, Ntanda/Nitanda. Slashes describe alternative branches.
ROUTES = (
    Route('03F Coen', ('1F IKEGARA', '2F IMOKAWA-CHO', '3F TAMATA'),
          ('3F TAMATA',), 1, 0, 5),
    Route('06F Jin-Die', ('4F TAGAHARA', '5F JOTEN-JIMA', '6F MOKA-MAGOME'),
          ('6F MOKA-MAGOME',), 1, 1, 10),
    Route('10F Max Sharp', ('7F DANEDA', '8F META-KOYA', '9F NASHIYOGUCHI', '10F OMENO-SOTO'),
          ('10F OMENO-SOTO',), 2, 2, 15),
    Route('11F Goto-9', ('11F MAKA-TSUTSUMI', '12F SEYANE', '13F MOMOHARA', '12F UKONGAOKA'),
          ('11F MAKA-TSUTSUMI',), 2, 2, 20),
    Route('16F Jin-Die', ('13F KAMOTO', '14F YAMADAI', '15F USOGAYA', '16F KAGA'),
          ('16F KAGA',), 2, 2, 25),
    Route('20F Jackson', ('17F KINUTAMIN', '18F TEMO-UMA', '19F DAIDA', '20F NORA', '19F CHITOSE-MEI', '20F HACHIYANMA'),
          ('20F HACHIYANMA',), 3, 3, 35),
    Route('23F Goto-9', ('21F AKAMI', '22F KAWABE', '23F ECHIZEN-JIMA'),
          ('23F ECHIZEN-JIMA',), 3, 3, 45),
    Route('26F Coen and Jin-Die', ('24F SUMIKIYO', '24F AGASU', '25F KANIE', '26F SHIMEMENO', '26F OSHIMI'),
          ('26F OSHIMI', '26F SHIMEMENO'), 3, 3, 60),
    Route('30F Crowley', ('27F SHINKO-BASHI', '28F HYAKUDA', '28F HYAKKOKU', '29F HASHIDAKA', '30F SAIGETSU'),
          ('30F SAIGETSU',), 4, 4, 80),
    Route('32F U-10', ('31F OBARA', '32F OSUKI', '31F KOI', '32F KATSUMA'),
          ('32F KATSUMA',), 4, 4, 100),
    Route('34F Coen', ('33F UCHINOBU', '34F NITANDA'),
          ('34F NITANDA',), 4, 4, 110),
    Route('36F Goto-9', ('35F SHINA-YAMA', '36F ITANODAI'),
          ('36F ITANODAI',), 4, 4, 120),
    Route('38F Jin-Die', ('37F HIROMATSU', '38F YATAKA-CHO', '37F MITSUBA', '38F EBATA'),
          ('38F EBATA',), 4, 4, 130),
    Route('40F Taro Gunkanyama', ('39F HARATSUKA', '40F FUTAGI'),
          ('40F FUTAGI',), 5, 5, 150),
)
GOAL_ROUTE = {10: 2, 20: 5, 30: 8, 40: 13}


class TowerLogic:
    def __init__(self, world):
        from .Locations import ROOM_VISIT_LOCATIONS
        self.world = world
        self.player = world.player
        self.grouped = bool(world.options.group_armor_sets.value)
        self.routes = ROUTES[:GOAL_ROUTE[int(world.options.goal.value)] + 1]
        self.maps = {name.removesuffix(' - Visit'): data for name, data in ROOM_VISIT_LOCATIONS.items()
                     if int(name.split('F', 1)[0]) <= world.check_catalogue['goal_floor']}
        self.map_keys = {}
        for name, data in self.maps.items():
            floor = int(name.split('F', 1)[0])
            self.map_keys[floor, data.game_id.split('|')[1]] = name
        self.boss_routes = {}
        for index, route in enumerate(self.routes):
            for name in route.maps:
                if name not in self.maps:
                    raise ValueError(f'Unknown LET IT DIE logic map: {name}')
            for name in route.bosses:
                self.boss_routes[name] = index

        quantities = Items.get_equipment_quantities(world.options)
        tiers = _catalogue()
        # Each tuple is (item name, received copies, slots granted, actual tier).
        self.equipment = []
        for name, item in Items.EQUIPMENT_ITEMS.items():
            count = quantities.get(name, 0)
            if not count:
                continue
            game_id = item.game_ids[0]
            slot = ('weapon' if game_id.startswith('PT_ARM_') else
                    next((slot for token, slot in (('_HEAD_', 'head'), ('_TOPS_', 'body'), ('_BTM_', 'legs'))
                          if token in game_id), None))
            if slot is None:
                raise ValueError(f'Unclassified equipment slot: {name}')
            for copies, tier in enumerate(tiers[name]['tiers'][:count], 1):
                self.equipment.append((name, copies, (slot,), tier))
        for spec in SETS:
            count = quantities.get(spec['name'], 0)
            for copies, tier in enumerate(spec['tiers'][:count], 1):
                self.equipment.append((spec['name'], copies, ('head', 'body', 'legs'), tier))
        self.alternatives = {}
        for tier in range(1, 7):
            for slot in ('weapon', 'head', 'body', 'legs', 'set'):
                best = {}
                for name, copies, slots, actual in self.equipment:
                    if (slot in slots or (slot == 'set' and len(slots) == 3)) and actual >= tier:
                        best[name] = min(copies, best.get(name, copies))
                self.alternatives[tier, slot] = tuple(best.items())

    def has_slot(self, state, tier, slot):
        return any(state.has(name, self.player, copies)
                   for name, copies in self.alternatives[tier, slot])

    def ready(self, state, index):
        route = self.routes[index]
        return (sum(state.count(name, self.player) for name in Items.STAT_ITEMS) >= route.stats
                and self.has_slot(state, route.weapon, 'weapon')
                and (not route.armor or (self.has_slot(state, route.armor, 'set') if self.grouped else
                     all(self.has_slot(state, route.armor, slot) for slot in ('head', 'body', 'legs')))))

    def rule(self, index):
        # A later requirement implies all preceding ones (monotonic minima).
        return lambda state: self.ready(state, index)

    def validate_pool(self):
        required = self.routes[-1]
        option_names = ('hp', 'stm', 'str', 'dex', 'vit', 'luck', 'death_bag', 'skill_slots', 'rage_gauge')
        total = sum(int(getattr(self.world.options, f'progressive_{name}_quantity').value) for name in option_names)
        total += sum(item.name in Items.STAT_ITEMS
                     for item in self.world.multiworld.precollected_items.get(self.player, []))
        if total < required.stats:
            raise ValueError(f'LET IT DIE {required.name} needs {required.stats} character progressives; YAML provides {total}.')
        needed = [('weapon', required.weapon)]
        if required.armor:
            needed.extend((slot, required.armor) for slot in (('set',) if self.grouped else ('head', 'body', 'legs')))
        for slot, tier in needed:
            if not self.alternatives[tier, slot]:
                raise ValueError(f'LET IT DIE {required.name} needs a tier {tier}+ {slot}; faction/tier options exclude all candidates.')


MATERIAL_OPTION_NAMES = frozenset(('material_checks','rare_material_checks','faction_metal_checks'))

CHEST_OPTION_NAMES = frozenset(('red_chest_checks','silver_chest_checks','gold_chest_checks'))


from .Fillers import NEW_OPTION_NAMES


def restore_tracker_options(world):
    payload = getattr(world.multiworld, 're_gen_passthrough', {}).get(world.game)
    if payload is None:
        return
    tracker = payload.get('tracker', {})
    schema = tracker.get('schema')
    if type(schema) is not int or schema not in (1, 2):
        raise ValueError('LET IT DIE Universal Tracker requires tracker schema 1 or 2.')
    world.generation_schema = schema
    version = tracker.get('catalogue_version', 1)
    if type(version) is not int or version not in (1, 2, 3, 4, 5):
        raise ValueError('Unsupported LET IT DIE tracker catalogue version.')
    world.hater_catalogue_version = version
    options = tracker.get('options')
    names = world.options_dataclass.__annotations__
    expected = set(names) - (CHEST_OPTION_NAMES if version < 4 else set()) - (MATERIAL_OPTION_NAMES if version < 5 else set())
    if schema < 2:
        expected -= NEW_OPTION_NAMES
    if not isinstance(options, dict) or set(options) != expected:
        raise ValueError('LET IT DIE tracker generation options are incomplete.')
    for name in names:
        option = getattr(world.options, name)
        setattr(world.options, name, type(option).from_any(options[name] if name in expected else 0))


def tracker_slot_data(world):
    return {'schema': world.generation_schema, 'catalogue_version': world.hater_catalogue_version,
            'options': {name: getattr(world.options, name).value
                                  for name in world.options_dataclass.__annotations__
                                  if (world.hater_catalogue_version >= 4 or name not in CHEST_OPTION_NAMES)
                                  and (world.hater_catalogue_version >= 5 or name not in MATERIAL_OPTION_NAMES)
                                  and (world.generation_schema >= 2 or name not in NEW_OPTION_NAMES)}}
