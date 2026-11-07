"""Candidate check delivery; no networking until the caller supplies send_msgs."""
import hashlib,json
from dataclasses import dataclass
from pathlib import Path
from . import CheckCatalogue,Locations
from .CheckWire import Context,Frame,SIZE
from .MaterialWire import decode_frame
from .MaterialOutbox import MaterialCheckOutbox
from .MaterialDispatch import record_frame
from .CheckInbox import MAX_BYTES,unique_object
from .CheckSessions import SlotSession
from .CheckOutbox import CheckOutbox,Location,Death
from .CheckStaleBatch import old_batch_received
from .MaterialCapabilityState import MaterialCapabilityState

def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False)
def id_set(values):
    if type(values) not in (set,frozenset,list,tuple):raise ValueError('location set type')
    if len(values)>50000:raise ValueError('location set capacity')
    for i in values:CheckCatalogue.integer(i,1,2**53-1)
    result=frozenset(values)
    if len(result)!=len(values):raise ValueError('duplicate location ID')
    return result
@dataclass(frozen=True)
class CheckConfig:
    goal:int
    chance:int
    locations:tuple
    ids:frozenset
    fingerprint:str
    pity:bool=False
    catalogue_version:int=1

def validate_catalogue(slot_data,checked,missing):
    if type(slot_data) is not dict:raise ValueError('slot data')
    packet=slot_data.get('checks')
    tracker=slot_data.get('tracker',{})
    if type(tracker) is not dict:raise ValueError('tracker catalogue metadata')
    version=tracker.get('catalogue_version',1)
    generation_schema=tracker.get('schema',1)
    if type(generation_schema) is not int or generation_schema not in (1,2):raise ValueError('unsupported tracker schema')
    if type(version) is not int or version not in (1,2,3,4,5):raise ValueError('unsupported catalogue version')
    keys={'schema','required_rotation','goal_floor','goal_name','tuber_checks_per_version','locations'}
    if version>=4:keys.add('chest_checks_per_band')
    if version>=5:keys.add('material_checks_per_type')
    if generation_schema>=2:keys.add('minimum_screamer_checks_per_band')
    if type(packet) is not dict or set(packet)!=keys:raise ValueError('check catalogue schema')
    if type(packet['schema']) is not int or packet['schema'] not in (1,2) or packet['required_rotation']!='4HMA':raise ValueError('unsupported catalogue/rotation')
    goal=CheckCatalogue.integer(packet['goal_floor'],10,40)
    if type(slot_data.get('goal')) is not int or slot_data['goal']!=goal:raise ValueError('goal mismatch')
    item_count=CheckCatalogue.integer(slot_data.get('generated_item_count'),0,1000000)
    chance=CheckCatalogue.integer(slot_data.get('screamer_check_chance'),1,100)
    pity=slot_data.get('screamer_pity',False)
    if type(pity) is not bool:raise ValueError('pity must be boolean')
    if packet['schema']==1 and pity:raise ValueError('legacy catalogue cannot enable pity')
    rows=[dict(name=name,code=data.code,category=data.category,zone=data.zone,game_id=data.game_id)
          for table in ((Locations.ROOM_VISIT_LOCATIONS,Locations.HATER_LOCATIONS,Locations.MINIBOSS_LOCATIONS) if packet['schema']==2 else (Locations.HATER_LOCATIONS,Locations.MINIBOSS_LOCATIONS)) for name,data in table.items()]
    # The server owns this run's catalogue. Old R43 runs omit this marker;
    # R45 generation writes version 2 to remove Forcemen Haters. Select the
    # matching definition BEFORE comparing, never guess from a failed match.
    if version>=5:
        material_options=tracker.get('options')
        if type(material_options) is not dict:raise ValueError('material tracker options required')
        names={'normal':'material_checks','rare':'rare_material_checks','metal':'faction_metal_checks'}
        values={key:CheckCatalogue.integer(material_options.get(name),0,1000) for key,name in names.items()}
        if values!=packet['material_checks_per_type']:raise ValueError('material option/catalogue mismatch')
    minimum=0
    if generation_schema>=2:
        minimum=CheckCatalogue.integer(packet['minimum_screamer_checks_per_band'],0,CheckCatalogue.SCREAMER_CAP)
        generation_options=tracker.get('options')
        if type(generation_options) is not dict or type(generation_options.get('minimum_screamer_checks_per_band')) is not int or generation_options['minimum_screamer_checks_per_band']!=minimum:raise ValueError('Screamer minimum option/catalogue mismatch')
    expected=CheckCatalogue.build(rows,goal,item_count,packet['tuber_checks_per_version'],
        include_visits=packet['schema']==2,exclude_forcemen=version>=2,room_tubers=version>=3,
        chest_quotas=packet['chest_checks_per_band'] if version>=4 else None,
        material_quotas=packet['material_checks_per_type'] if version>=5 else None,
        minimum_screamer_checks_per_band=minimum)
    if type(packet['locations']) is not list or len(packet['locations'])>50000:raise ValueError('catalogue capacity')
    if canonical(packet['locations'])!=canonical(expected['checks']) or packet['goal_name']!=expected['goal_name']:raise ValueError('catalogue differs from local definitions')
    for key,value in (('fixed_location_count',expected['fixed_count']),('screamer_location_count',sum(expected['screamer_counts']))):
        if CheckCatalogue.integer(slot_data.get(key),0,50000)!=value:raise ValueError('catalogue count mismatch')
    names=Location.__dataclass_fields__
    locations=tuple(Location(**{k:v for k,v in check.items() if k in names}) for check in expected['checks'])
    ids=frozenset(loc.code for loc in locations);done=id_set(checked);todo=id_set(missing)
    if done&todo or done|todo!=ids:raise ValueError('server locations differ from active catalogue')
    descriptor={'checks':packet,'chance':chance}
    if packet['schema']==2:descriptor['pity']=pity
    fingerprint=hashlib.sha256(canonical(descriptor).encode()).hexdigest()
    return CheckConfig(goal,chance,locations,ids,fingerprint,pity,version)

class CheckService:
    def __init__(self,directory,context:Context,config:CheckConfig):
        self.directory=Path(directory);self.context=context;self.config=config
        self.session=SlotSession(self.directory,context,config.catalogue_version)
        factory=MaterialCheckOutbox if config.catalogue_version==5 else CheckOutbox
        try:self.outbox=factory(self.directory/'death_checks_outbox.json',context.run.hex(),config.goal,config.ids,config.locations,config.chance,config.pity)
        except BaseException:self.session.close();raise
        try:self.material_capability=MaterialCapabilityState(self.directory,context,config) if config.catalogue_version==5 else None
        except BaseException:self.session.close();raise
        self.last_send=None;self.last_file=None
    def _record(self,frame):
        return record_frame(self.outbox,frame) if self.config.catalogue_version==5 else self.outbox.record(frame.to_death(Death))
    def recover_old(self):
        paths=sorted(self.directory.glob('death_receipts_*.json'))
        if len(paths)>32:raise ValueError('session recovery capacity')
        recovered=0
        for path in paths:
            if path.name==f'death_receipts_{self.context.nonce.hex()}.json':continue
            if path.stat().st_size>MAX_BYTES:raise ValueError('old receipt size')
            raw=path.read_bytes()
            if len(raw)>MAX_BYTES:raise ValueError('old receipt size')
            data=json.loads(raw,object_pairs_hook=unique_object);pending=data['state']['raw']
            if not pending:continue
            frame=decode_frame(bytes.fromhex(next(iter(pending.values()))),self.config.catalogue_version);old=frame.context
            if path.name!=f'death_receipts_{old.nonce.hex()}.json':raise ValueError('old receipt filename/context')
            while True:
                count=self.session.drain_old(old,self._record)
                recovered+=count
                if count==0:break
        return recovered
    def confirm(self,checked):
        ids=id_set(checked)
        if not ids<=self.config.ids:raise ValueError('foreign server confirmation')
        if not ids<=set(self.outbox.state['confirmed']):self.outbox.server_confirmed(ids)
    def poll(self):
        if self.material_capability is not None:self.material_capability.poll()
        path=self.directory/'death_native_events.bin'
        try:stat=path.stat()
        except FileNotFoundError:stat=None
        if stat is not None:
            signature=(stat.st_mtime_ns,stat.st_size)
            if signature!=self.last_file:
                with path.open('rb') as f:batch=f.read(64*SIZE+1)
                if not old_batch_received(self.directory,self.context,batch,self.config.catalogue_version):
                    self.session.ingest(batch);self.session.publish_ack()
                self.last_file=signature
        # Also drains after a previous callback failure, even if file unchanged.
        return self.session.consume(self._record)
    async def publish(self,now,send_msgs,goal_status):
        packet=self.outbox.packet();goal=self.outbox.state['goal']
        if not packet and not goal:return False
        if self.last_send is not None and now-self.last_send<5:return False
        self.last_send=now
        messages=[]
        if packet:messages.append({'cmd':'LocationChecks','locations':packet['locations'][:256]})
        if goal:messages.append({'cmd':'StatusUpdate','status':goal_status})
        await send_msgs(messages)
        # Successful send never removes pending checks or makes a server ACK.
        return True
    def close(self):self.session.close()
