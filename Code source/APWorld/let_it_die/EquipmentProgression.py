"""Shared generation/tracker policy for native equipment upgrade targets."""
from functools import lru_cache
import hashlib
import json
import pkgutil
from typing import NamedTuple

CATALOGUE_HEX = '5d2ce385d63129e958d91fd34d277a9c657e4265ab45b237eff9cbcbbd361845'
SCHEMA = 2


class EquipmentStage(NamedTuple):
    tier: int
    part: str
    native_level: int
    enhancement: int


@lru_cache(maxsize=1)
def catalogue():
    raw = pkgutil.get_data(__package__, 'EquipmentProgression.json')
    if raw is None:
        raise ValueError('Equipment progression catalogue is missing')
    data = json.loads(raw)
    actual = hashlib.sha256(json.dumps(data, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    if data.get('schema') != SCHEMA or actual != CATALOGUE_HEX:
        raise ValueError('Equipment progression catalogue identity mismatch')
    return data['families']


def stages_for(name, intermediate_steps=0):
    if type(intermediate_steps) is not int or not 0 <= intermediate_steps <= 4:
        raise ValueError('equipment_upgrade_steps must be between 0 and 4')
    entry = catalogue().get(name)
    if entry is None:
        return ()
    stages = tuple(EquipmentStage(*step) for step in entry['policies'][str(intermediate_steps)])
    if (not stages or any(not 1 <= s.tier <= 9 or not 1 <= s.native_level <= 25
                          or not 0 <= s.enhancement <= 20 for s in stages)
            or tuple(s.tier for s in stages) != tuple(sorted(s.tier for s in stages))):
        raise ValueError('Invalid equipment progression sequence')
    return stages


def option_steps(options):
    return int(getattr(getattr(options, 'equipment_upgrade_steps', None), 'value', 0))


def slot_policy(options):
    return dict(schema=SCHEMA, catalogue=CATALOGUE_HEX, intermediate_steps=option_steps(options))


def steps_from_slot(slot_data):
    """No marker means the exact legacy +0 receipt semantics, never migration."""
    policy = slot_data.get('equipment_progression')
    if policy is None:
        return None
    if (not isinstance(policy, dict) or set(policy) != {'schema', 'catalogue', 'intermediate_steps'}
            or type(policy['schema']) is not int or policy['schema'] != SCHEMA or policy['catalogue'] != CATALOGUE_HEX
            or type(policy['intermediate_steps']) is not int or not 0 <= policy['intermediate_steps'] <= 4):
        raise ValueError('Unsupported equipment progression slot policy')
    if 'equipment_upgrade_steps' in slot_data and (type(slot_data['equipment_upgrade_steps']) is not int or slot_data['equipment_upgrade_steps'] != policy['intermediate_steps']):
        raise ValueError('Equipment progression option/policy mismatch')
    return policy['intermediate_steps']
