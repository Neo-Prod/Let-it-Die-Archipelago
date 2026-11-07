"""Research only: bounded BRG decoder and PRIMARY schema checks.

No file access, native apply, save writes or acknowledgement. This is NOT a
complete validator of all nested native domains and must not arm delivery.
"""
from __future__ import annotations
from dataclasses import dataclass
import hashlib
import json
import math
import struct
import zlib

MAX_FILE=12*1024*1024
MAX_JSON=32*1024*1024
MAX_CHUNKS=256
MAX_NODES=500000
MAX_DEPTH=128
# First root dependency of each of the17 native readers, plus Soul.
# gi is DAT142146AE8 in reader141295C70 (not a rendered L literal).
ROOT_TYPES={name:dict for name in ('user','soul','gameflg','bodyuser','floor','diedchara',
    'zombie','part','item','mushroom','beast','playlog','gi','dummy','ranking','warteam','fort')}
ROOT_TYPES['team']=list
FIELDS=('hp','stm','str','dex','vit','luk','bag','skill','rage')

class SnapshotError(ValueError):
    """Fixed descriptions only; never include save values or identities."""

@dataclass(frozen=True)
class PreparedSnapshot:
    canonical_ascii:bytes
    source_sha256:str
    canonical_sha256:str
    chunk_count:int
    declared_bytes:int
    primary_schema_only:bool=True

def _reject_constant(_value):
    raise SnapshotError('non-finite JSON constant')

def _object(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise SnapshotError('duplicate JSON member')
        result[key]=value
    return result

def _bounded_tree(root):
    todo=[(root,0)];seen=0
    while todo:
        value,depth=todo.pop();seen+=1
        if seen>MAX_NODES or depth>MAX_DEPTH:raise SnapshotError('JSON complexity limit')
        if isinstance(value,dict):
            todo.extend((v,depth+1) for v in value.values())
            todo.extend((k,depth+1) for k in value)
        elif isinstance(value,list):todo.extend((v,depth+1) for v in value)
        elif isinstance(value,str):
            if '\0' in value or any(0xD800<=ord(c)<=0xDFFF for c in value):raise SnapshotError('unsupported JSON string')
        elif isinstance(value,float) and not math.isfinite(value):raise SnapshotError('non-finite JSON number')

def decode_brg(data:bytes)->tuple[dict,int,int]:
    if type(data) is not bytes or len(data)<24 or len(data)>MAX_FILE:raise SnapshotError('BRG file size')
    if data[:4]!=b'BRG\0' or data[12:16]!=b'ZLIB':raise SnapshotError('BRG magic')
    version,declared=struct.unpack_from('<II',data,4)
    if version!=2 or not 0<declared<=MAX_JSON:raise SnapshotError('BRG version or declared size')
    offset=16;pieces=[];total=0
    while offset+8<=len(data):
        unpacked,packed=struct.unpack_from('<II',data,offset)
        if not unpacked or not packed:raise SnapshotError('empty BRG chunk')
        if len(pieces)>=MAX_CHUNKS or unpacked>declared-total:raise SnapshotError('BRG chunk budget')
        offset+=8;end=offset+packed
        if end>len(data):raise SnapshotError('truncated BRG chunk')
        try:
            decoder=zlib.decompressobj();raw=decoder.decompress(data[offset:end],unpacked+1)
        except zlib.error as exc:raise SnapshotError('invalid zlib stream') from exc
        if len(raw)!=unpacked or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
            raise SnapshotError('zlib boundary or size mismatch')
        pieces.append(raw);total+=len(raw);offset=end
    # Native saves observed: four chunks, trailing uint32 zero.
    # Existing APWorld encoder: one chunk, no suffix. Neither is inferred.
    if data[offset:] not in (b'',b'\0'*4) or total!=declared or not pieces:raise SnapshotError('BRG trailer or total size')
    try:
        root=json.loads(b''.join(pieces).decode('utf-8-sig'),strict=False,
                        object_pairs_hook=_object,parse_constant=_reject_constant)
    except (UnicodeError,json.JSONDecodeError,RecursionError) as exc:raise SnapshotError('invalid JSON payload') from exc
    if type(root) is not dict:raise SnapshotError('JSON root type')
    _bounded_tree(root)
    return root,len(pieces),declared

def _id(value):
    if type(value) not in (str,int):raise SnapshotError('identity type')
    value=str(value)
    if not value or len(value)>128:raise SnapshotError('identity size')
    return value

def validate_primary_schema(root:dict,expected_uid:str)->None:
    if type(root) is not dict or type(expected_uid) is not str or not expected_uid:raise SnapshotError('expected identity missing')
    for key,kind in ROOT_TYPES.items():
        if type(root.get(key)) is not kind:raise SnapshotError('required root domain absent or wrong type')
    uid=_id(root['user'].get('uid'))
    if uid!=expected_uid or _id(root['soul'].get('uid'))!=uid:raise SnapshotError('user and soul identity mismatch')
    soul=root['soul'];chr_table=soul.get('chr')
    if type(chr_table) is not dict or type(chr_table.get('chrs')) is not dict:raise SnapshotError('fighter table shape')
    fighters=chr_table['chrs'].get(uid)
    if type(fighters) is not list or not 0<len(fighters)<=4096:raise SnapshotError('fighter table size')
    ids=set();selected=[]
    for row in fighters:
        if type(row) is not dict:raise SnapshotError('fighter record type')
        cid=_id(row.get('cid'))
        if cid in ids:raise SnapshotError('duplicate fighter identity')
        ids.add(cid)
        if row.get('state')=='USE':selected.append(cid)
    if len(selected)!=1:raise SnapshotError('selected fighter ambiguity')
    bodies=root['bodyuser'].get(uid)
    if type(bodies) is not list or len(bodies)>4096:raise SnapshotError('body table shape')
    body_ids=set();chosen=[]
    for row in bodies:
        if type(row) is not dict:raise SnapshotError('body record type')
        cid=_id(row.get('cid'))
        if cid in body_ids:raise SnapshotError('duplicate body identity')
        body_ids.add(cid)
        if cid==selected[0]:chosen.append(row)
    if len(chosen)!=1:raise SnapshotError('selected fighter body missing')
    for field in FIELDS:
        value=chosen[0].get(field)
        if type(value) is not int or not 0<=value<=0x7fffffff:raise SnapshotError('progression counter shape')
    for field in ('skl','deathbag','partresearch'):
        if type(soul.get(field)) is not dict:raise SnapshotError('character domain shape')

def prepare_snapshot(data:bytes,expected_uid:str)->PreparedSnapshot:
    root,chunks,declared=decode_brg(data);validate_primary_schema(root,expected_uid)
    canonical=json.dumps(root,ensure_ascii=True,separators=(',',':'),allow_nan=False).encode('ascii')
    if len(canonical)>MAX_JSON:raise SnapshotError('canonical JSON size')
    return PreparedSnapshot(canonical,hashlib.sha256(data).hexdigest(),hashlib.sha256(canonical).hexdigest(),chunks,declared)
