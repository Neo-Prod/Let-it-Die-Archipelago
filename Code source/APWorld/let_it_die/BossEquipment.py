"""Pure boss loadout feasibility; shares MasterySanity's native stat arithmetic."""
from dataclasses import dataclass
from functools import lru_cache
import hashlib,json,pkgutil
from . import Items
from .ArmorSets import SETS
from .EquipmentProgression import stages_for,option_steps
from .MasteryLogic import MasteryLogic,requirement

DATA_HASH = '57767aaa85cf5ffff6ea67432e7c06e63ab3e466f8cebc63eea86c1ce4032d1b'

@lru_cache(maxsize=1)
def data():
    raw=pkgutil.get_data(__package__,'BossEquipment.json')
    if raw is None or hashlib.sha256(raw).hexdigest()!=DATA_HASH:
        raise ValueError('Boss equipment requirement catalogue identity')
    result=json.loads(raw)
    if result.get('schema')!=1:raise ValueError('Boss equipment requirement schema')
    return result['parts']

@dataclass(frozen=True)
class Piece:
    item:str
    copies:int
    tier:int
    stat:str
    threshold:int
    ceiling:int
    next_copy:int

def slot_for(part):
    if part.startswith('PT_ARM_'):return 'weapon'
    for token,slot in (('_HEAD_','head'),('_TOPS_','body'),('_BTM_','legs')):
        if token in part:return slot
    raise ValueError('Unknown equipment slot: '+part)

class BossEquipment:
    def __init__(self,world):
        self.player=world.player;self.options=world.options
        self.tracker_mode=world.game in getattr(world.multiworld,'re_gen_passthrough',{})
        self.slots={s:[]for s in ('weapon','head','body','legs')};self.sets=[]
        quantities=Items.get_equipment_quantities(self.options)
        steps=option_steps(self.options)
        def pieces(item,member,count):
            stages=stages_for(member,steps)[:count];by_part={};values=[]
            for copies,stage in enumerate(stages,1):
                row=data()[stage.part]
                value=requirement(stage.native_level,row['base'],row['coefficient'])
                by_part.setdefault(stage.part,[]).append((copies,value))
                values.append((copies,stage,row,value))
            result=[]
            for copies,stage,row,value in values:
                sequence=by_part[stage.part]
                next_copy=next((n for n,_ in sequence if n>copies),0)
                result.append((slot_for(stage.part),Piece(item,copies,stage.tier,row['stat'],value,
                              max(v for _,v in sequence),next_copy)))
            return result
        for name,quantity in quantities.items():
            if quantity and name in Items.EQUIPMENT_ITEMS:
                for slot,piece in pieces(name,name,quantity):self.slots[slot].append(piece)
        for spec in SETS:
            count=quantities.get(spec['name'],0)
            if not count:continue
            slots={s:[]for s in ('head','body','legs')}
            for member in spec['members']:
                for slot,piece in pieces(spec['name'],member,count):slots[slot].append(piece)
            self.sets.append(slots)

    def stat(self,state,stat):
        # Exactly the existing mastery rule, without constructing mastery policy.
        return MasteryLogic.stat(self,state,stat)

    def has_piece(self,state,tier,pieces):
        for p in pieces:
            if p.tier<tier:continue
            copies=state.count(p.item,self.player)
            if copies<p.copies:continue
            if self.tracker_mode and p.next_copy and copies>=p.next_copy:continue
            threshold=p.threshold if self.tracker_mode else p.ceiling
            if self.stat(state,p.stat)>=threshold:return True
        return False

    def has_slot(self,state,tier,slot):
        if slot=='set':
            return any(all(self.has_piece(state,tier,slots[s])for s in ('head','body','legs'))
                       for slots in self.sets)
        return self.has_piece(state,tier,self.slots[slot])
