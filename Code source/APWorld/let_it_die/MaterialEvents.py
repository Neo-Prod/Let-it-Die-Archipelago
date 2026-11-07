"""Typed conversion from a validated material wire frame to Outbox input."""
from dataclasses import dataclass
import hashlib
from .MaterialWire import MaterialFrame
from .CheckMapIds import physical_floor
@dataclass(frozen=True)
class Material:
    run:str
    event_id:str
    floor:int
    area:str
    rotation:str
    item_id:str
    material_ordinal:int
    wire_payload:str
def from_frame(frame):
    if type(frame) is not MaterialFrame:raise ValueError('validated material frame required')
    frame.validate()
    return Material(frame.context.run.hex(),frame.event_id(),physical_floor(frame.floor,frame.area),frame.area,frame.rotation,frame.item_id,frame.ordinal,hashlib.sha256(frame.encode()).hexdigest())
