"""Completion rules for LET IT DIE."""

from BaseClasses import Item, ItemClassification
from .TuberRooms import witness_rooms
from .ChestRooms import witness_rooms as chest_witness_rooms
from .MaterialRooms import witness_rooms as material_witness_rooms


VICTORY_ITEM_NAME = "Victory"


def set_rules(world) -> None:
    """Complete the generated world at its selected boss event."""

    logic = world.tower_logic
    material_cache={}
    for check in world.check_catalogue['checks']:
        if check['kind'] == 'miniboss':
            name = logic.map_keys[check['floor'], check['area']]
            world.multiworld.get_location(check['name'], world.player).access_rule = logic.rule(logic.boss_routes[name])
        elif check['kind']=='tuber' and world.hater_catalogue_version>=3:
            rooms=tuple(world.multiworld.get_region(name,world.player)
                        for name in witness_rooms(check['tag'],check['version'],world.check_catalogue['goal_floor']))
            if not rooms:raise ValueError('Tuber location has no witness room')
            world.multiworld.get_location(check['name'],world.player).access_rule = (
                lambda state, rooms=rooms: any(region.can_reach(state) for region in rooms))
        elif check['kind']=='material':
            key=check['tag']
            if key not in material_cache:
                witnesses=tuple((world.multiworld.get_region(room,world.player),
                                  logic.boss_routes[room] if boss else None)
                                 for room,boss in material_witness_rooms(key,world.check_catalogue['goal_floor']))
                if not witnesses:raise ValueError('Material location has no valid Wednesday witness')
                material_cache[key]=witnesses
            witnesses=material_cache[key]
            world.multiworld.get_location(check['name'],world.player).access_rule=(
                lambda state,witnesses=witnesses:any(region.can_reach(state) and
                    (boss is None or logic.ready(state,boss)) for region,boss in witnesses))
        elif check['kind']=='chest':
            witnesses=tuple((world.multiworld.get_region(room,world.player),
                             logic.boss_routes[room] if boss else None)
                            for room,boss in chest_witness_rooms(check['tag'],check['zone'],world.check_catalogue['goal_floor']))
            if not witnesses:raise ValueError('Chest location has no placed witness room')
            world.multiworld.get_location(check['name'],world.player).access_rule = (
                lambda state,witnesses=witnesses: any(region.can_reach(state) and
                    (boss is None or logic.ready(state,boss)) for region,boss in witnesses))
    goal = world.multiworld.get_location(
        world.goal_location_name,
        world.player,
    )
    goal.access_rule = logic.rule(len(logic.routes) - 1)

    goal.place_locked_item(
        Item(
            VICTORY_ITEM_NAME,
            ItemClassification.progression,
            None,
            world.player,
        )
    )

    world.multiworld.completion_condition[world.player] = (
        lambda state: state.has(VICTORY_ITEM_NAME, world.player)
    )
