"""Typed mixed-frame -> durable candidate Outbox; no socket/send operation."""
from .CheckWire import Frame,EVENT,VISIT,CHEST
from .MaterialWire import MaterialFrame
from .MaterialEvents import from_frame
from .MaterialOutbox import Death,Visit,Chest,MaterialCheckOutbox
from .CheckMapIds import physical_floor
def record_frame(outbox,frame):
    if type(outbox) is not MaterialCheckOutbox:raise ValueError('material candidate Outbox required')
    if type(frame) is MaterialFrame:return outbox.record(from_frame(frame))
    if type(frame) is not Frame:raise ValueError('validated typed wire frame required')
    frame.validate()
    if frame.kind==EVENT:return outbox.record(frame.to_death(Death))
    if frame.kind==VISIT:return outbox.record(Visit(frame.context.run.hex(),frame.event_id(),physical_floor(frame.floor,frame.area),frame.area,frame.rotation))
    if frame.kind==CHEST:return outbox.record(Chest(frame.context.run.hex(),frame.event_id(),physical_floor(frame.floor,frame.area),frame.area,frame.rotation,('Red','Silver','Gold')[frame.raw_kind-1]))
    raise ValueError('control frames cannot allocate checks')
