"""Tower payment journal/LocationChecks only, never immediate local rewards.

Generic persistence/send methods are inherited from untouched ShopPurchase.
Tower codecs, billing identity, state and fixed paths are independent.
"""
import json,struct
from dataclasses import asdict
from pathlib import Path
from .CheckInbox import atomic,unique_object
from .CheckWire import Context
from .ShopPurchase import ShopPurchaseService,_digest,MAX_BYTES
from .ShopTowerCatalogue import offers as public_offers,packet
from .ShopTowerNative import for_goal,NATIVE_DIGEST
from .ShopTowerPurchaseWire import Payment,PAYMENT,SIZE,purchase_state

EVENT_FILE='shop_tower_native_payment.bin'
STATE_FILE='shop_tower_client_state.bin'

class ShopTowerPurchaseService(ShopPurchaseService):
    def __init__(self,directory,context,offers,goal):
        if type(context) is not Context:raise ValueError('tower Context type')
        context.validate()
        if (type(offers) is not tuple or offers!=public_offers(goal) or
            _digest([asdict(row) for row in offers])!=packet(goal)['catalogue_sha256']):raise ValueError('tower generated offers')
        self.directory,self.context=Path(directory),context
        self.catalogue=bytes.fromhex(packet(goal)['catalogue_sha256'])
        self.offers={row.offer:row for row in for_goal(goal)}
        self.ids=frozenset(row.location for row in self.offers.values())
        self.path=self.directory/'shop_tower_purchase_outbox.json'
        self.ack_path=self.directory/'shop_tower_client_received.bin'
        self.binding=_digest(['LIDTSHP1',context.run.hex(),context.config.hex(),self.catalogue.hex(),NATIVE_DIGEST])
        self.last_send=self.last_event_file=self.last_state=None
        self.state=dict(schema=1,binding=self.binding,sessions=[],receipts={},confirmed=[])
        if self.path.exists():
            with self.path.open('rb') as stream:raw=stream.read(MAX_BYTES+1)
            if len(raw)>MAX_BYTES:raise ValueError('tower journal capacity')
            wrapper=json.loads(raw,object_pairs_hook=unique_object)
            if type(wrapper) is not dict or set(wrapper)!={'sha256','state'} or wrapper['sha256']!=_digest(wrapper['state']):
                raise ValueError('tower journal wrapper/checksum')
            self._validate_state(wrapper['state']);self.state=wrapper['state']

    def _match(self,event,current):
        if (event.kind!=PAYMENT or event.catalogue!=self.catalogue or event.context.run!=self.context.run or
            event.context.config!=self.context.config or (current and event.context!=self.context)):
            raise ValueError('foreign tower receipt')
        offer=self.offers.get(event.offer)
        if (offer is None or event.location!=offer.location or event.item!=offer.native_item or
            event.product!=offer.product or event.price!=offer.native_kc):raise ValueError('tower native offer mismatch')
        return offer

    def _validate_state(self,state):
        if (type(state) is not dict or set(state)!={'schema','binding','sessions','receipts','confirmed'} or
            type(state['schema']) is not int or state['schema']!=1 or state['binding']!=self.binding or
            type(state['receipts']) is not dict or len(state['receipts'])>len(self.offers) or type(state['confirmed']) is not list):
            raise ValueError('tower journal binding/shape')
        sessions=state['sessions']
        if (type(sessions) is not list or len(sessions)>32 or any(type(key) is not str or len(key)!=224 for key in sessions) or
            len(set(sessions))!=len(sessions)):raise ValueError('tower producer registry')
        for key in sessions:
            raw=bytes.fromhex(key)
            if (len(raw)!=112 or raw.hex()!=key or raw[48:80]!=self.context.run or raw[80:112]!=self.context.config or
                not all(struct.unpack_from('<IQI',raw)) or not any(raw[16:32]) or not any(raw[32:48])):
                raise ValueError('tower producer identity')
        self._checked(state['confirmed'])
        if state['confirmed']!=sorted(set(state['confirmed'])):raise ValueError('tower confirmation canonical form')
        sequences=set()
        for key,value in state['receipts'].items():
            if type(value) is not str or len(value)!=SIZE*2:raise ValueError('tower persisted receipt format')
            event=Payment.decode(bytes.fromhex(value))
            if key!=str(event.offer) or value!=event.encode().hex():raise ValueError('tower persisted offer/key')
            self._match(event,current=False)
            sequence=(event.context.key(),event.sequence)
            if sequence in sequences:raise ValueError('tower producer sequence reused')
            sequences.add(sequence)

    def _ingest(self,raw,recovered):
        event=Payment.decode(raw);offer=self._match(event,current=not recovered)
        if recovered:
            if event.context.key().hex() not in self.state['sessions']:raise ValueError('unregistered tower producer')
        else:self._register_session()
        old=self.state['receipts'].get(str(event.offer))
        if old is not None and old!=raw.hex():raise ValueError('second payment for same tower offer')
        if old is None:
            if offer.location in self.state['confirmed']:raise ValueError('tower offer already checked')
            self._commit({**self.state,'receipts':{**self.state['receipts'],str(event.offer):raw.hex()}})
        # Durable atomic journal precedes state/ACK. Neither ACK nor send()
        # proves a native wallet Save or server confirmation.
        self.publish_state();atomic(self.ack_path,event.acknowledgement());return old is None

    def publish_state(self):
        self._register_session();sold_out=self.sold_out
        raw=purchase_state(self.context,self.catalogue,tuple(row.offer for row in self.offers.values() if row.location in sold_out))
        if raw!=self.last_state:atomic(self.directory/STATE_FILE,raw);self.last_state=raw

    def poll(self):
        path=self.directory/EVENT_FILE
        try:info=path.stat()
        except FileNotFoundError:return False
        signature=(info.st_mtime_ns,info.st_size)
        if signature==self.last_event_file:return False
        with path.open('rb') as stream:raw=stream.read(SIZE+1)
        event=Payment.decode(raw)
        if event.context!=self.context:
            if event.context.key().hex() not in self.state['sessions']:return False
            changed=self._ingest(raw,recovered=True)
        else:changed=self.ingest(raw)
        self.last_event_file=signature;return changed
