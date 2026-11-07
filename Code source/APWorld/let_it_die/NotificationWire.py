"""Presentation-only native popup protocol. No I/O or gameplay mutations."""
from dataclasses import dataclass
import struct

from .CheckWire import Context, GAME, integer

SIZE = 8448
HEADER = 256
MAX_UNITS = 4095
MAGIC = b'LIDNOTE1'
REQUEST, ACK = 1, 2


def checksum(data):
    value = 2166136261
    for offset, byte in enumerate(data):
        if not 176 <= offset < 180:
            value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


@dataclass(frozen=True)
class Message:
    kind: int
    context: Context
    sequence: int
    text: str = ''

    def encode(self):
        self.context.validate()
        integer(self.kind, REQUEST, ACK)
        integer(self.sequence, 1, 2**64 - 1)
        if type(self.text) is not str or '\0' in self.text:
            raise ValueError('Notification text type/NUL')
        data = self.text.encode('utf-16-le', errors='strict')
        units = len(data) // 2
        if units > MAX_UNITS or (self.kind == REQUEST and not units) or (self.kind == ACK and units):
            raise ValueError('Notification text length/kind')
        packet = bytearray(SIZE)
        packet[:8] = MAGIC
        c = self.context
        struct.pack_into('<IIIIQ', packet, 8, 1, self.kind, c.pid, c.client_pid, c.birth)
        packet[32:48], packet[48:64] = c.nonce, c.writer
        packet[64:96], packet[96:128] = c.run, c.config
        struct.pack_into('<QI', packet, 128, self.sequence, units)
        packet[144:176] = GAME
        packet[HEADER:HEADER + len(data)] = data
        struct.pack_into('<I', packet, 176, checksum(packet))
        return bytes(packet)


def decode(packet):
    if type(packet) is not bytes or len(packet) != SIZE or packet[:8] != MAGIC:
        raise ValueError('Notification frame length/magic')
    version, kind, pid, client_pid, birth = struct.unpack_from('<IIIIQ', packet, 8)
    sequence, units = struct.unpack_from('<QI', packet, 128)
    if version != 1 or packet[144:176] != GAME or units > MAX_UNITS:
        raise ValueError('Notification version/build/text length')
    if any(packet[140:144]) or any(packet[180:HEADER]) or any(packet[HEADER + units*2:]):
        raise ValueError('Notification reserved bytes/terminator/padding')
    if struct.unpack_from('<I', packet, 176)[0] != checksum(packet):
        raise ValueError('Notification checksum')
    context = Context(pid, birth, packet[32:48], packet[64:96], packet[96:128], client_pid, packet[48:64])
    message = Message(kind, context, sequence, packet[HEADER:HEADER + units*2].decode('utf-16-le', errors='strict'))
    if message.encode() != packet:
        raise ValueError('Noncanonical notification frame')
    return message
