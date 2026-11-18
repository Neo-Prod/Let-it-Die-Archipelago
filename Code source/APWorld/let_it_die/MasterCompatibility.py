"""One known master_item schema migration on the dedicated AP copy only.

No save or receipt access. Preserve every existing column value, index and
other table; obtain only the new sell_spirit values/schema from read-only game
masters. Transaction failure rolls the dedicated table back in full.
"""
from pathlib import Path
from contextlib import contextmanager
import sys

_COLUMNS=('itemid','itemtype','rarity','grp','name','desc','buy_money',
    'buy_rank_money','buy_spirit','buy_medal','buy_rank_spirit','buy_recycle_point',
    'buy_bloodnium','sell_money','sell_rank_rate','sell_recycle_point',
    'sell_spirit','is_hub_sale','is_usable','asset','sort_no','ol','platform')

def _literal(value):return "'"+str(value).replace("'","''")+"'"
def _identifier(value):return '"'+value.replace('"','""')+'"'

@contextmanager
def _open(path,readonly=False):
    if sys.platform=='win32':
        from .MasterPatch import _WindowsSQLite
        with _WindowsSQLite(path,read_only=readonly) as db:yield db
    else:
        import sqlite3
        with sqlite3.connect(path.as_uri()+'?mode=ro&immutable=1',uri=True) if readonly else sqlite3.connect(path) as db:
            try:yield db
            finally:db.close()

def _rows(db,sql):
    if not hasattr(db,'_lib'):return [tuple(None if v is None else str(v) for v in row) for row in db.execute(sql)]
    ctypes=db._ctypes;result=[];error=ctypes.c_char_p();failure=[]
    callback_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_int,ctypes.POINTER(ctypes.c_char_p),ctypes.POINTER(ctypes.c_char_p))
    @callback_type
    def callback(_user,count,values,_names):
        try:result.append(tuple(values[i].decode('utf-8',errors='strict') if values[i] is not None else None for i in range(count)));return 0
        except BaseException as exc:failure.append(exc);return 1
    status=db._lib.sqlite3_exec(db._db,sql.encode('utf-8'),ctypes.cast(callback,ctypes.c_void_p),None,ctypes.byref(error))
    if status:
        message=error.value.decode('utf-8',errors='replace') if error.value else db._error_message()
        if error:db._lib.sqlite3_free(ctypes.cast(error,ctypes.c_void_p))
        if failure:raise failure[0]
        raise RuntimeError('Master compatibility query failed: '+message)
    return result

def migrate_master_item(destination_db,*,slot_directory,source_db):
    destination=Path(destination_db).resolve(strict=True);directory=Path(slot_directory).resolve(strict=True);source=Path(source_db).resolve(strict=True)
    if destination.parent!=directory or destination.name!='masters.db' or directory.parent.name!='slots':raise ValueError('Dedicated AP slot master required')
    if destination==source or destination.samefile(source):raise ValueError('Original masters database is immutable')
    with _open(source,True) as current:
        schema=_rows(current,'PRAGMA table_info(master_item)')
        if tuple(row[1] for row in schema)!=_COLUMNS:raise ValueError('Unknown current master_item schema')
        create=_rows(current,"SELECT sql FROM sqlite_master WHERE type='table' AND name='master_item'")[0][0]
        spirit=_rows(current,'SELECT itemid,sell_spirit FROM master_item')
    with _open(destination) as db:
        existing=_rows(db,'PRAGMA table_info(master_item)')
        if existing==schema:return False
        legacy=[row for row in schema if row[1]!='sell_spirit']
        legacy=[(str(i),*row[1:]) for i,row in enumerate(legacy)]
        if existing!=legacy:raise ValueError('Unknown dedicated master_item schema; no migration performed')
        if _rows(db,"SELECT name FROM sqlite_master WHERE name='__lid_master_item_r310'"):raise ValueError('Migration table already exists')
        indexes=_rows(db,"SELECT type,sql FROM sqlite_master WHERE tbl_name='master_item' AND sql IS NOT NULL AND type<>'table'")
        if any(kind!='index' for kind,_ in indexes):raise ValueError('Unexpected master_item trigger')
        db.execute('BEGIN IMMEDIATE;')
        try:
            # Recheck inside the transaction; no overwrite of a concurrent edit.
            if _rows(db,'PRAGMA table_info(master_item)')!=existing:raise ValueError('Master schema changed during migration')
            keys={row[0] for row in _rows(db,'SELECT itemid FROM master_item')};values=dict(spirit)
            if not keys<=values.keys():raise ValueError('Dedicated item missing from current master')
            marker='CREATE TABLE "master_item"'
            if not create.startswith(marker):raise ValueError('Unexpected master_item creation SQL')
            db.execute(create.replace(marker,'CREATE TABLE "__lid_master_item_r310"',1))
            old_names=[row[1] for row in existing];names=','.join(map(_identifier,_COLUMNS))
            cases='CASE itemid '+''.join('WHEN '+_literal(key)+' THEN '+str(int(values[key]))+' ' for key in sorted(keys))+'ELSE 0 END'
            select=','.join(cases if name=='sell_spirit' else _identifier(name) for name in _COLUMNS)
            db.execute('INSERT INTO "__lid_master_item_r310"('+names+') SELECT '+select+' FROM master_item')
            old_columns=','.join(map(_identifier,old_names))
            mismatch=_rows(db,'SELECT COUNT(*) FROM (SELECT '+old_columns+' FROM master_item EXCEPT SELECT '+old_columns+' FROM __lid_master_item_r310)')[0][0]
            if int(mismatch) or int(_rows(db,'SELECT COUNT(*) FROM __lid_master_item_r310')[0][0])!=len(keys):raise ValueError('Master item values changed')
            db.execute('DROP TABLE master_item')
            db.execute('ALTER TABLE __lid_master_item_r310 RENAME TO master_item')
            for _,sql in indexes:db.execute(sql)
            if _rows(db,'PRAGMA table_info(master_item)')!=schema:raise ValueError('Master schema verification failed')
            db.execute('COMMIT;')
        except BaseException:
            db.execute('ROLLBACK;');raise
    return True
