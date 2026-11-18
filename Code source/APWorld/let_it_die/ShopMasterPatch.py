"""Wednesday merchant policy on the dedicated AP masters copy only.

No save, wallet, price, clock, spawn hook or connection-protocol operation.
The native SHOP_APPEARANCE_TIME unit is minutes; one means sixty seconds.
"""
from dataclasses import dataclass
from pathlib import Path
import sys

@dataclass(frozen=True)
class ShopMasterPolicy:
    goal:int
    rooms:tuple[tuple[str,str],...]

def policy(slot_data):
    """Validate the existing generated descriptors; legacy/off does no DB IO."""
    if type(slot_data) is not dict:raise ValueError('Shop master slot data')
    tracker=slot_data.get('tracker',{})
    if type(tracker) is not dict:raise ValueError('Shop master tracker')
    if not tracker.get('shopsanity_version',0):return None
    from .CheckWire import integer
    from .ShopCatalogue import validate
    goal=integer(slot_data.get('goal'),10,40)
    momoka,tower=validate(slot_data,goal)
    if not momoka and not tower:return None
    checks=slot_data.get('checks')
    if type(checks) is not dict or checks.get('required_rotation')!='4HMA' or type(checks.get('goal_floor')) is not int or checks['goal_floor']!=goal:
        raise ValueError('Shop master requires current Wednesday goal')
    rooms=tuple(sorted({tuple(row.room_key.split('|')) for row in tower}))
    if len(rooms)!={10:2,20:3,30:5,40:7}.get(goal) or any(len(r)!=2 for r in rooms):raise ValueError('Shop master merchant rooms')
    return ShopMasterPolicy(goal,rooms)

def _literal(value):
    return "'"+value.replace("'","''")+"'"

def _scalar(db,sql):
    if hasattr(db,'scalar_int'):return db.scalar_int(sql)
    row=db.execute(sql).fetchone()
    if row is None:raise ValueError('Shop master missing query result')
    return int(row[0])

def _where(floor,area):
    return 'templateid=\'4HMA\' AND flrid='+_literal(floor)+' AND areaid='+_literal(area)

def _verify(db,p):
    if _scalar(db,"SELECT COUNT(*) FROM master_const_int WHERE id='SHOP_APPEARANCE_TIME'")!=1:raise ValueError('Shop master timer missing/ambiguous')
    if _scalar(db,"SELECT COUNT(*) FROM master_const_int WHERE id='SHOP_APPEARANCE_TIME' AND value=1")!=1:return False
    for floor,area in p.rooms:
        where=_where(floor,area)
        if _scalar(db,'SELECT COUNT(*) FROM master_shop_appearance WHERE '+where)!=1:return False
        if _scalar(db,'SELECT COUNT(*) FROM master_shop_appearance WHERE '+where+' AND rate=100')!=1:return False
    return True

def _apply(db,p):
    if _verify(db,p):return False
    db.execute('BEGIN IMMEDIATE;')
    try:
        # Check all logical keys before writing; primary id alone is insufficient.
        for floor,area in p.rooms:
            count=_scalar(db,'SELECT COUNT(*) FROM master_shop_appearance WHERE '+_where(floor,area))
            if count>1:raise ValueError('Ambiguous Wednesday merchant rule')
            if not count:
                identifier='AP_SHOP_4HMA_'+floor+'_'+area
                if len(identifier)>64 or _scalar(db,'SELECT COUNT(*) FROM master_shop_appearance WHERE id='+_literal(identifier)):
                    raise ValueError('Shop master appearance id collision')
        db.execute("UPDATE master_const_int SET value=1 WHERE id='SHOP_APPEARANCE_TIME';")
        for floor,area in p.rooms:
            where=_where(floor,area)
            if _scalar(db,'SELECT COUNT(*) FROM master_shop_appearance WHERE '+where):
                db.execute('UPDATE master_shop_appearance SET rate=100 WHERE '+where+';')
            else:
                values=','.join(map(_literal,('AP_SHOP_4HMA_'+floor+'_'+area,'4HMA',floor,area)))
                db.execute('INSERT INTO master_shop_appearance(id,templateid,flrid,areaid,rate) VALUES('+values+',100);')
        if not _verify(db,p):raise ValueError('Shop master policy verification failed')
        db.execute('COMMIT;')
    except BaseException:
        db.execute('ROLLBACK;')
        raise
    return True

def apply_shop_master_policy(destination_db,*,slot_directory,source_db,slot_data):
    """Called at master-copy preparation before native overlay handoff.

    Original game DB is used only for a same-file check. Reconnects reuse the
    existing copy; a current policy never updates it. No backup/save operation.
    """
    p=policy(slot_data)
    if p is None:return None
    destination=Path(destination_db);directory=Path(slot_directory);source=Path(source_db)
    resolved=destination.resolve(strict=True);root=directory.resolve(strict=True)
    if resolved.parent!=root or resolved.name!='masters.db' or directory.parent.name!='slots' or not directory.name:
        raise ValueError('Shop policy requires dedicated slots/<key>/masters.db')
    if not source.is_file() or not resolved.is_file() or resolved==source.resolve() or resolved.samefile(source):
        raise ValueError('Refusing to patch original/same-file masters database')
    if sys.platform=='win32':
        from .MasterPatch import _WindowsSQLite
        with _WindowsSQLite(resolved) as db:_apply(db,p)
    else:
        import sqlite3
        from contextlib import closing
        with closing(sqlite3.connect(resolved)) as db:_apply(db,p)
    return p
