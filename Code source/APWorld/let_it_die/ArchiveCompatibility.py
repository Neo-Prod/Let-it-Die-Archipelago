"""Private interpretation of durable former-build evidence, never live input.

Original bytes/checksums are verified and kept on disk. All existing strict
codecs and receipt/context/pending rules still run on the private copy.
"""
import struct

OLD_BUILD=bytes.fromhex('f33e91e6e8d58566bfabad8d01cd0486586a0d43fe1c58e65fc6485a417bddfd')
CURRENT_BUILD=bytes.fromhex('6cfadbba9da3e2f3b3a4463194d247a50d2537a3ad704174730a0bbf899e42ca')

def _sum(raw,at):
    value=2166136261
    for i,b in enumerate(raw):
        if not at<=i<at+4:value=((value^b)*16777619)&0xffffffff
    return value

def _rebind(raw,build,checksum):
    if type(raw) is not bytes or len(raw) not in (256,1024):raise ValueError('archive frame size/type')
    if raw[build:build+32]==CURRENT_BUILD:return raw # existing strict decoder verifies it
    if raw[build:build+32] not in (OLD_BUILD,CURRENT_BUILD):raise ValueError('unknown archive build')
    if struct.unpack_from('<I',raw,checksum)[0]!=_sum(raw,checksum):raise ValueError('original archive checksum')
    private=bytearray(raw);private[build:build+32]=CURRENT_BUILD
    struct.pack_into('<I',private,checksum,_sum(private,checksum));return bytes(private)

def archived_check(raw,catalogue_version=4):
    from .MaterialWire import decode_frame
    return decode_frame(_rebind(raw,128,848),catalogue_version)

def archived_coin(raw):
    from .KillCoinWire import decode
    return decode(_rebind(raw,192,252))

def archived_decal(raw):
    if type(raw) is not bytes or len(raw)!=1152:raise ValueError('archive decal size/type')
    if raw[256:288]==CURRENT_BUILD:return raw # existing journal/companion checks remain intact
    if struct.unpack_from('<I',raw,52)[0]!=_sum(raw,52):raise ValueError('original companion checksum')
    private=bytearray(raw);private[64:320]=_rebind(raw[64:320],192,224)
    struct.pack_into('<I',private,52,_sum(private,52));return bytes(private)
