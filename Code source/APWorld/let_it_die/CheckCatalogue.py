"""Read-only AST import and isolated candidate catalogue. No AP/game imports."""
import ast,re
from dataclasses import dataclass,asdict
from pathlib import Path
from .TuberRooms import witness_rooms

GOALS={10:'Max Sharp',20:'Jackson',30:'Crowley',40:'Taro Gunkanyama'}
FAMILIES=('Scratch','Hovering','Bullet','Bone','Reversal','Treasure')
DEFAULTS=dict(zip(FAMILIES,(20,5,10,3,1,1)))
VERSION_GOALS=(10,20,30,30,40)
PREFIX_BASE={'MET':0,'ARC':10,'AMS':20,'RFT':30}
SCREAMER_BASE=7206000;SCREAMER_CAP=2500;TUBER_BASE=7300000;TUBER_CAP=1000
CHEST_COLORS=('Red','Silver','Gold')
CHEST_DEFAULTS=dict(zip(CHEST_COLORS,(30,15,3)))
CHEST_BASE=7340000;CHEST_CAP=1000
MINIBOSS={'COEN':'COEN','GOTO-9':'GOTO9','JIN-DIE':'JIN_DIE','U-10':'U10'}
# Exact SMALLBOSS rooms from master_area_setting_unit + stage_fourforcemen.
# Keep legacy location IDs reserved; only new catalogues omit these Haters.
FORCEMEN_AREAS=frozenset(('MET_AREA_V440','MET_AREA_V450','MET_AREA_V480',
    'ARC_AREA_V420','ARC_AREA_V440','ARC_AREA_V470',
    'AMS_AREA_V032','AMS_AREA_V033','AMS_AREA_V034',
    'RFT_AREA_V029','RFT_AREA_V030','RFT_AREA_V031'))
def integer(value,low,high):
    if type(value) is not int or not low<=value<=high:raise ValueError('integer range/type')
    return value
def map_floor(floor_id,area_id):
    match=re.fullmatch(r'(MET|ARC|AMS|RFT)_FLR_(0[1-9]|10)',floor_id)
    if not match or not re.fullmatch(match[1]+r'_AREA_(?:V)?[0-9]{1,12}',area_id):raise ValueError('invalid tower map')
    return PREFIX_BASE[match[1]]+int(match[2])
def extract(path):
    tree=ast.parse(Path(path).read_text(encoding='utf-8-sig'));rows=[]
    for node in tree.body:
        if not isinstance(node,ast.Assign) or not isinstance(node.value,ast.Dict):continue
        names=[t.id for t in node.targets if isinstance(t,ast.Name)]
        if not names or not names[0].endswith('_LOCATIONS'):continue
        for key,value in zip(node.value.keys,node.value.values):
            if not isinstance(value,ast.Call) or not isinstance(value.func,ast.Name) or value.func.id!='LetItDieLocationData':continue
            code,category,zone,game_id=(ast.literal_eval(x) for x in value.args)
            rows.append({'name':ast.literal_eval(key),'code':code,'category':category,'zone':zone,'game_id':game_id,'source_line':value.lineno,'source_table':names[0]})
    return rows
@dataclass(frozen=True)
class Check:
    code:int;name:str;kind:str;floor:int=0;area:str='';tag:str='';version:int=0;ordinal:int=0;zone:int=0
def build(rows,goal,item_count,quotas=None,include_visits=False,exclude_forcemen=True,room_tubers=False,chest_quotas=None,material_quotas=None,minimum_screamer_checks_per_band=0):
    if type(goal) is not int or goal not in GOALS:raise ValueError('goal')
    integer(item_count,0,1000000)
    quotas=DEFAULTS if quotas is None else quotas
    if type(quotas) is not dict or set(quotas)!=set(FAMILIES):raise ValueError('six family quotas required')
    for n in quotas.values():integer(n,0,TUBER_CAP)
    if chest_quotas is not None:
        if type(chest_quotas) is not dict or set(chest_quotas)!=set(CHEST_COLORS):raise ValueError('three chest quotas required')
        for n in chest_quotas.values():integer(n,0,CHEST_CAP)
    checks=[];reserved=set();keys=set()
    for row in rows:
        integer(row['code'],1,2**53-1)
        if row['code'] in reserved:raise ValueError('duplicate existing ID')
        reserved.add(row['code'])
        if row['category'] not in (('hater','miniboss','room_visit') if include_visits else ('hater','miniboss')):continue
        parts=row['game_id'].split('|');expected=3 if row['category']=='miniboss' else 2
        if len(parts)!=expected:raise ValueError('map arity')
        floor=map_floor(*parts[:2]);label=re.match(r'([0-9]+)F ',row['name'])
        if not label or int(label[1])!=floor:raise ValueError('display/map floor mismatch')
        decade=(floor-1)//10;zone=f'{decade*10+1:02d}-{decade*10+10:02d}'
        if row['zone']!=zone:raise ValueError('zone mismatch')
        tag=MINIBOSS[parts[2]] if expected==3 else ''
        key=(row['category'],parts[0],parts[1],tag)
        if key in keys:raise ValueError('duplicate encounter/map')
        keys.add(key)
        if floor<=goal and not (exclude_forcemen and row['category']=='hater' and parts[1] in FORCEMEN_AREAS):
            checks.append(Check(row['code'],row['name'],row['category'],floor,parts[1],tag,zone=decade))
    for family_index,family in enumerate(FAMILIES):
        for version,min_goal in enumerate(VERSION_GOALS,1):
            rooms=witness_rooms(family,version,goal) if room_tubers else ()
            if (not rooms if room_tubers else min_goal>goal):continue
            zone_floor=min(int(r.split('F')[0]) for r in rooms) if room_tubers else min_goal
            for ordinal in range(1,quotas[family]+1):
                code=TUBER_BASE+family_index*5000+(version-1)*1000+ordinal
                checks.append(Check(code,f'{family} V-{version} #{ordinal:04d}','tuber',tag=family,version=version,ordinal=ordinal,zone=(zone_floor-1)//10))
    if chest_quotas is not None:
        for color_index,color in enumerate(CHEST_COLORS):
            for zone in range(goal//10):
                for ordinal in range(1,chest_quotas[color]+1):
                    code=CHEST_BASE+color_index*4000+zone*1000+ordinal
                    checks.append(Check(code,f'{color} Chest {zone*10+1:02d}-{zone*10+10:02d} #{ordinal:04d}','chest',tag=color,ordinal=ordinal,zone=zone))
    if material_quotas is not None:
        from .MaterialCatalogue import build as build_materials
        if type(material_quotas) is not dict or set(material_quotas)!={'normal','rare','metal'}:raise ValueError('three material quotas required')
        checks.extend(Check(**asdict(check)) for check in build_materials(goal,**material_quotas))
    minimum=integer(minimum_screamer_checks_per_band,0,SCREAMER_CAP)
    fixed=len(checks);remaining=max(0,item_count-fixed,minimum*(goal//10));base,extra=divmod(remaining,goal//10)
    counts=[]
    for zone in range(goal//10):
        count=base+(zone<extra)
        if count>SCREAMER_CAP:raise ValueError('Screamer capacity')
        counts.append(count)
        for ordinal in range(1,count+1):
            checks.append(Check(SCREAMER_BASE+zone*SCREAMER_CAP+ordinal,f'Screamer {zone*10+1:02d}-{zone*10+10:02d} #{ordinal:04d}','screamer',ordinal=ordinal,zone=zone))
    codes=[c.code for c in checks]
    if len(codes)!=len(set(codes)) or any(c.code in reserved for c in checks if c.kind in ('tuber','screamer','chest','material')):raise ValueError('ID collision')
    if material_quotas is not None and len(checks)>50000:raise ValueError('Check catalogue exceeds client capacity')
    return {'goal_floor':goal,'goal_name':GOALS[goal],'victory_network_address':None,'fixed_count':fixed,'screamer_counts':counts,'filler_count':max(0,len(checks)-item_count),'item_count':item_count,'checks':[asdict(c) for c in checks]}
