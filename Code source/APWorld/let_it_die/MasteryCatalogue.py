"""Deterministic optional weapon mastery checks and generation policy."""
from functools import lru_cache
import hashlib,json,pkgutil
from types import SimpleNamespace

SCHEMA=1
BASE=7800000
DATA_HASH='c8efe967d1e3ad276147f7b1f61ff2b9358ba30c3636cef843ab5068fe21379b'
OPTION_NAMES=frozenset(('mastery_sanity','mastery_checks','mastery_include_fists','mastery_xp_multiplier'))

@lru_cache(maxsize=1)
def data():
    raw=pkgutil.get_data(__package__,'MasteryEquipment.json')
    if raw is None or hashlib.sha256(raw).hexdigest()!=DATA_HASH:raise ValueError('Mastery equipment catalogue identity')
    result=json.loads(raw)
    if result['schema']!=SCHEMA:raise ValueError('Mastery equipment schema')
    return result

def levels(count):
    if type(count)is not int or not 1<=count<=20:raise ValueError('Mastery checks must be1..20')
    # Nearest integer, .5 rounds upward; no random selection or float error.
    return tuple((40*i+count)//(2*count) for i in range(1,count+1))

def code(family,level):
    if family not in data()['families'] or type(level)is not int or not 1<=level<=20:raise ValueError('Mastery family/level')
    return BASE+int(family[8:])*100+level

def name(family,level):return f"Mastery {data()['families'][family]['name']} - Level {level:02d}"

def from_options(options):
    from .Items import get_equipment_quantities
    from .EquipmentProgression import stages_for,option_steps
    enabled=bool(options.mastery_sanity.value);count=int(options.mastery_checks.value)
    fists=bool(options.mastery_include_fists.value);multiplier=int(options.mastery_xp_multiplier.value)
    levels(count)
    if not 1<=multiplier<=100:raise ValueError('Mastery XP multiplier must be1..100')
    families=set()
    if enabled:
        for item,quantity in get_equipment_quantities(options).items():
            if quantity and item in data()['weapons']:
                families.update(data()['weapons'][item][stage.part]['family']
                                for stage in stages_for(item,option_steps(options))[:quantity])
        if fists:families.add('PTARMTP_00')
        else:families.discard('PTARMTP_00')
    return dict(schema=SCHEMA,catalogue=DATA_HASH,enabled=enabled,checks=count,include_fists=fists,
                xp_multiplier=multiplier,families=sorted(families))

def from_slot(slot):
    policy=slot.get('mastery')
    if policy is None:return None
    options=slot.get('tracker',{}).get('options')
    if slot.get('tracker',{}).get('mastery_schema')!=1 or not isinstance(options,dict) or not OPTION_NAMES<=options.keys():raise ValueError('Mastery tracker options missing')
    proxy=SimpleNamespace(**{n:SimpleNamespace(value=v)for n,v in options.items()})
    expected=from_options(proxy)
    if (type(policy)is not dict or policy!=expected or any(type(policy[n])is not bool for n in ('enabled','include_fists'))
        or any(type(policy[n])is not int for n in ('schema','checks','xp_multiplier'))):raise ValueError('Mastery generation policy mismatch')
    return expected

def checks(policy):
    from .CheckCatalogue import Check
    if policy is None or not policy['enabled']:return ()
    return tuple(Check(code(family,level),name(family,level),'mastery',tag=family,ordinal=level)
                 for family in policy['families']for level in levels(policy['checks']))
