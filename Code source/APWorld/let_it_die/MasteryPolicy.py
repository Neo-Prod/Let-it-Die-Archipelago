"""Session-bound fixed native mastery policy; no saves or network access."""
import struct,os,tempfile
from pathlib import Path
from .MasteryCatalogue import data,DATA_HASH

FAMILIES=tuple(sorted(data()['families']))
def checksum(raw):
    value=2166136261
    for b in raw[:188]:value=((value^b)*16777619)&0xffffffff
    return value
def encode(context,policy):
    context.validate();raw=bytearray(192);raw[:8]=b'LIDMST01'
    struct.pack_into('<IIIIQ',raw,8,1,192,context.pid,context.client_pid,context.birth)
    raw[32:48]=context.nonce;raw[48:80]=context.run;raw[80:112]=context.config;raw[112:128]=context.writer
    mask=sum(1<<FAMILIES.index(f)for f in policy['families'])
    struct.pack_into('<IIIIQ',raw,128,int(policy['enabled']),policy['checks'],int(policy['include_fists']),policy['xp_multiplier'],mask)
    raw[152:184]=bytes.fromhex(DATA_HASH)
    struct.pack_into('<I',raw,188,checksum(raw));return bytes(raw)
def publish(directory,context,policy):
    if policy is None:return
    path=Path(directory)/'mastery_policy.bin';raw=encode(context,policy)
    fd,temp=tempfile.mkstemp(prefix='.mastery-',suffix='.tmp',dir=path.parent)
    try:
        with os.fdopen(fd,'wb')as f:f.write(raw);f.flush();os.fsync(f.fileno())
        os.replace(temp,path)
    finally:
        if os.path.exists(temp):os.unlink(temp)
