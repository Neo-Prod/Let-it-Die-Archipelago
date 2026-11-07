"""Map reachability, minimum boss routes, and goal-bounded farming bands."""
from BaseClasses import Region

def create_regions(world) -> None:
    catalogue = world.check_catalogue
    regions = {'Waiting Room': Region('Waiting Room', world.player, world.multiworld)}
    logic = world.tower_logic
    stages = [Region(f'Route {r.name}', world.player, world.multiworld) for r in logic.routes]
    regions['Waiting Room'].connect(stages[0])
    for index in range(1, len(stages)):
        stages[index - 1].connect(stages[index], rule=logic.rule(index - 1))
    for name in logic.maps:
        region = Region(name, world.player, world.multiworld)
        regions[name] = region
        floor = int(name.split('F', 1)[0])
        default = next(i for i, route in enumerate(logic.routes) if floor <= route.last_floor)
        for index, route in enumerate(logic.routes):
            if name in route.maps:
                stages[index].connect(region, name=f'{route.name}: path to {name}')
        if name not in logic.routes[default].maps:
            stages[default].connect(region, name=f'{logic.routes[default].name}: other map {name}', rule=logic.rule(default))
    for zone in range(catalogue['goal_floor'] // 10):
        region = Region(f'Tower {zone * 10 + 1:02d}-{zone * 10 + 10:02d}', world.player, world.multiworld)
        regions[zone] = region
        first = next(i for i, route in enumerate(logic.routes) if route.last_floor > zone * 10)
        stages[first].connect(region)
    world.multiworld.regions.extend(regions.values())
    world.multiworld.regions.extend(stages)
    for check in catalogue['checks']:
        parent = (regions[logic.map_keys[check['floor'], check['area']]]
                  if check['kind'] in ('room_visit', 'hater', 'miniboss') else regions['Waiting Room']
                  if (check['kind']=='tuber' and world.hater_catalogue_version>=3) or check['kind'] in ('chest','material')
                  else regions[check['zone']])
        parent.add_locations({check['name']: check['code']}, world.location_type)
    world.goal_location_name = f"{catalogue['goal_floor']}F - Defeat {catalogue['goal_name']}"
    parent = regions[logic.routes[-1].bosses[0]]
    goal = world.location_type(world.player, world.goal_location_name, None, parent)
    parent.locations.append(goal)
