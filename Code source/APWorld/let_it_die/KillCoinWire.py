"""Separate native currency channel. An ACK proves application, not a game save."""
from dataclasses import dataclass
import struct
from .CheckWire import Context, GAME, integer
from .Fillers import AMOUNTS_BY_ID

SIZE = 256
MAGIC = b"LIDKCN01"
GRANT, ACK = 1, 2
APPLIED, REPLAY = 1, 2
INT_MAX = 2_147_483_647


def checksum(raw):
    value = 2166136261
    for byte in raw[:252]:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


@dataclass(frozen=True)
class CoinFrame:
    context: Context
    index: int
    item: int
    amount: int
    kind: int = GRANT
    status: int = 0
    before: int = 0
    after: int = 0

    def encode(self):
        self.context.validate()
        integer(self.kind, GRANT, ACK)
        integer(self.index, 0, 99_999)
        integer(self.item, 1, 2**32 - 1)
        integer(self.amount, 1, 50_000)
        integer(self.status, 0, REPLAY)
        integer(self.before, 0, INT_MAX)
        integer(self.after, 0, INT_MAX)
        if AMOUNTS_BY_ID.get(self.item) != self.amount:
            raise ValueError("KC denomination mismatch")
        if self.kind == GRANT:
            if self.status or self.before or self.after:
                raise ValueError("KC grant has outcome fields")
        elif self.status not in (APPLIED, REPLAY) or self.after - self.before != self.amount:
            raise ValueError("KC application outcome mismatch")
        raw = bytearray(SIZE)
        raw[:8] = MAGIC
        c = self.context
        struct.pack_into('<IIIIQ', raw, 8, 1, self.kind, c.pid, c.client_pid, c.birth)
        raw[32:48], raw[48:80], raw[80:112], raw[112:128] = c.nonce, c.run, c.config, c.writer
        struct.pack_into('<6I', raw, 128, self.index, self.item, self.amount, self.status, self.before, self.after)
        raw[192:224] = GAME
        struct.pack_into('<I', raw, 252, checksum(raw))
        return bytes(raw)


def decode(raw):
    if type(raw) is not bytes or len(raw) != SIZE:
        raise ValueError("KC packet size")
    version, kind, pid, client_pid, birth = struct.unpack_from('<IIIIQ', raw, 8)
    if raw[:8] != MAGIC or version != 1 or raw[192:224] != GAME or struct.unpack_from('<I', raw, 252)[0] != checksum(raw):
        raise ValueError("KC header, build or checksum")
    index, item, amount, status, before, after = struct.unpack_from('<6I', raw, 128)
    frame = CoinFrame(Context(pid, birth, raw[32:48], raw[48:80], raw[80:112], client_pid, raw[112:128]),
                      index, item, amount, kind, status, before, after)
    if frame.encode() != raw:
        raise ValueError("KC noncanonical padding")
    return frame
