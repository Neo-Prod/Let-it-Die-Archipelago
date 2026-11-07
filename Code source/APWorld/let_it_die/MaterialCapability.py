"""Catalogue-5 capability sidecar; does not alter character connection/GRANT.

This module is a private candidate. A decoded READY is evidence of a matching
host reply, not an independent proof that hooks are installed or game-tested.
"""
from dataclasses import dataclass
import os
from pathlib import Path
import struct
from .CheckWire import Context

SIZE=256
REQUEST,READY=1,2
MAGIC=b'LIDMAT01'
BUILD=bytes.fromhex('f33e91e6e8d58566bfabad8d01cd0486586a0d43fe1c58e65fc6485a417bddfd')
REQUEST_FILE='material_client_capability.bin'
READY_FILE='material_native_capability.bin'

def checksum(raw):
    value=2166136261
    for byte in raw[:252]:value=((value^byte)*16777619)&0xffffffff
    return value

@dataclass(frozen=True)
class Capability:
    context:Context
    kind:int
    catalogue:bytes
    def encode(self):
        self.context.validate()
        if type(self.kind) is not int or self.kind not in (REQUEST,READY):raise ValueError('capability kind')
        if type(self.catalogue) is not bytes or len(self.catalogue)!=32 or not any(self.catalogue):raise ValueError('catalogue digest')
        c=self.context;raw=bytearray(SIZE)
        raw[:8]=MAGIC
        struct.pack_into('<IIIIQ',raw,8,1,self.kind,c.pid,c.client_pid,c.birth)
        raw[32:48]=c.nonce;raw[48:80]=c.run;raw[80:112]=c.config;raw[112:128]=c.writer
        raw[128:160]=self.catalogue;raw[160:192]=BUILD
        struct.pack_into('<I',raw,192,5)
        struct.pack_into('<I',raw,252,checksum(raw))
        return bytes(raw)

def decode(raw):
    if type(raw) is not bytes or len(raw)!=SIZE:raise ValueError('capability size')
    version,kind,pid,client_pid,birth=struct.unpack_from('<IIIIQ',raw,8)
    if raw[:8]!=MAGIC or version!=1 or raw[160:192]!=BUILD or struct.unpack_from('<I',raw,192)[0]!=5:raise ValueError('capability header/build')
    if struct.unpack_from('<I',raw,252)[0]!=checksum(raw):raise ValueError('capability checksum')
    m=Capability(Context(pid,birth,raw[32:48],raw[48:80],raw[80:112],client_pid,raw[112:128]),kind,raw[128:160])
    if m.encode()!=raw:raise ValueError('noncanonical capability')
    return m

def request(context,catalogue_version,fingerprint):
    if type(catalogue_version) is not int or catalogue_version!=5:raise ValueError('materials require catalogue 5')
    if type(fingerprint) is not str or len(fingerprint)!=64 or any(ch not in '0123456789abcdef' for ch in fingerprint):raise ValueError('catalogue fingerprint')
    m=Capability(context,REQUEST,bytes.fromhex(fingerprint));m.encode();return m

def publish_request(directory,message):
    if message.kind!=REQUEST:raise ValueError('request required')
    raw=message.encode();directory=Path(directory)
    # Caller owns the existing SlotSession lease. No directory creation here.
    temporary=directory/f'.{REQUEST_FILE}.{message.context.client_pid}.{message.context.writer.hex()}.tmp'
    created=False
    try:
        with temporary.open('xb') as stream:
            created=True
            stream.write(raw);stream.flush();os.fsync(stream.fileno())
        os.replace(temporary,directory/REQUEST_FILE)
    finally:
        if created:
            try:temporary.unlink()
            except FileNotFoundError:pass

def matching_ready(directory,message):
    if message.kind!=REQUEST:raise ValueError('request required')
    message.encode()
    try:
        with (Path(directory)/READY_FILE).open('rb') as stream:raw=stream.read(SIZE+1)
    except FileNotFoundError:return False
    reply=decode(raw)
    if reply.kind!=READY or reply.context!=message.context or reply.catalogue!=message.catalogue:raise ValueError('foreign capability reply')
    return True
