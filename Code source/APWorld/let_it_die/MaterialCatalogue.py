"""Research material locations; ordinals remain stable across goals/options."""
from dataclasses import dataclass
from .MaterialIdentity import MATERIALS,BY_ID
from .MaterialNames import MATERIAL_NAMES

BASE=7360000
CAP=1000
DEFAULTS={'normal':10,'rare':2,'metal':1}
def integer(value,lo,hi):
    if type(value) is not int or not lo<=value<=hi:raise ValueError('integer range/type')
    return value
def eligible(goal):
    if type(goal) is not int or goal not in (10,20,30,40):raise ValueError('goal')
    bands=goal//10;result=[]
    for ordinal,(item,family,rank,faction) in enumerate(MATERIALS,1):
        category=None
        if family==7 and rank<=bands:category='metal'
        elif family==6:category='normal'
        elif family<=5 and rank<=bands:category='normal'
        elif family<=5 and rank==bands+1:category='rare'
        if category:result.append((ordinal,item,category))
    return tuple(result)
@dataclass(frozen=True)
class MaterialCheck:
    code:int
    name:str
    kind:str='material'
    floor:int=0
    area:str=''
    tag:str=''
    version:int=0
    ordinal:int=0
    zone:int=0
def build(goal,normal=10,rare=2,metal=1):
    quantities=dict(normal=integer(normal,0,CAP),rare=integer(rare,0,CAP),metal=integer(metal,0,CAP));result=[]
    for material,item,category in eligible(goal):
        for index in range(1,quantities[category]+1):
            result.append(MaterialCheck(BASE+(material-1)*CAP+index,f'Material - {MATERIAL_NAMES[item]} #{index:04d}',tag=item,ordinal=index))
    return tuple(result)
