"""Optional, non-consumable numbered-floor progression shared with the tracker."""
from dataclasses import dataclass

ITEM_NAME = 'Progressive Floor'
ITEM_ID = 7100010
OPTION_NAMES = frozenset(('progressive_floor', 'progressive_floor_quantity'))
SCHEMA = 1


def required_for_floor(floor):
    if type(floor) is not int or not 1 <= floor <= 40:
        raise ValueError('Tower floor must be between 1 and 40')
    return floor - 1 - (floor - 1) // 10


@dataclass(frozen=True)
class FloorPolicy:
    enabled: bool
    quantity: int
    goal: int

    def __post_init__(self):
        if type(self.enabled) is not bool or type(self.quantity) is not int or not 0 <= self.quantity <= 1000:
            raise ValueError('Invalid Progressive Floor configuration')
        if type(self.goal) is not int or self.goal not in (10, 20, 30, 40):
            raise ValueError('Invalid Progressive Floor goal')
        if self.enabled and self.quantity < required_for_floor(self.goal):
            raise ValueError(f'LET IT DIE goal {self.goal}F requires at least {required_for_floor(self.goal)} Progressive Floor items; YAML provides {self.quantity}.')

    @property
    def pool_count(self):
        return self.quantity if self.enabled else 0

    def can_enter(self, state, player, floor):
        return not self.enabled or state.has(ITEM_NAME, player, required_for_floor(floor))

    def slot_data(self):
        return dict(schema=SCHEMA, enabled=self.enabled, quantity=self.quantity, goal=self.goal)


def from_options(options):
    return FloorPolicy(bool(options.progressive_floor.value), int(options.progressive_floor_quantity.value), int(options.goal.value))


def from_slot_data(slot_data):
    value = slot_data.get('floor_progression')
    if value is None:
        return FloorPolicy(False, 0, int(slot_data.get('goal', 40)))
    if type(value) is not dict or set(value) != {'schema', 'enabled', 'quantity', 'goal'} or type(value['schema']) is not int or value['schema'] != SCHEMA:
        raise ValueError('Unsupported Floor progression slot policy')
    policy = FloorPolicy(value['enabled'], value['quantity'], value['goal'])
    if policy.goal != slot_data.get('goal'):
        raise ValueError('Floor progression goal mismatch')
    return policy
