"""Research-only single-owner slot lifecycle. No game/OS identity discovery."""
from pathlib import Path
import msvcrt
from .CheckWire import Context,integer
from .MaterialInbox import DurableMaterialInbox
from .CheckInbox import DurableInbox

class SlotLease:
    """Windows byte lock: nonblocking, released by OS on process termination.

    The persistent lock file is never removed (removal can split ownership).
    Assumes an existing trusted slot directory; no network filesystem guarantee.
    """
    def __init__(self,directory):
        self.file=None
        file=(Path(directory)/'death_transport.owner').open('a+b')
        try:
            file.seek(0,2)
            if file.tell()==0:file.write(b'\0');file.flush()
            file.seek(0)
            msvcrt.locking(file.fileno(),msvcrt.LK_NBLCK,1)
            self.file=file
        except BaseException:
            file.close();raise
    def require(self):
        if self.file is None:raise RuntimeError('slot lease closed')
    def close(self):
        if self.file is not None:
            file,self.file=self.file,None
            try:file.seek(0);msvcrt.locking(file.fileno(),msvcrt.LK_UNLCK,1)
            finally:file.close()
    def __enter__(self):self.require();return self
    def __exit__(self,*unused):self.close()

class SlotSession:
    """All receiver journal operations must go through the same slot lease.

    Old receipts can be consumed, but only the active session may publish ACK.
    Does not validate real process lifetime or authorize native context rotation.
    """
    def __init__(self,directory,current:Context,catalogue_version=4):
        integer(catalogue_version,1,5)
        self.catalogue_version=catalogue_version
        current.validate();self.directory=Path(directory);self.current=current
        self.lease=SlotLease(self.directory)
        try:self._inbox=self._make_inbox(current)
        except BaseException:self.lease.close();raise
    def _make_inbox(self,context):
        return DurableMaterialInbox(self.directory,context,5) if self.catalogue_version==5 else DurableInbox(self.directory,context)
    def ingest(self,batch):
        self.lease.require();return self._inbox.ingest(batch)
    @property
    def received(self):self.lease.require();return self._inbox.received
    @property
    def consumed(self):self.lease.require();return self._inbox.state['consumed']
    def receipt_bytes(self):self.lease.require();return self._inbox.path.read_bytes()
    def publish_ack(self):
        self.lease.require();return self._inbox.publish_ack()
    def consume(self,callback,limit=64):
        self.lease.require();return self._inbox.consume(callback,limit)
    def drain_old(self,old:Context,callback,limit=64):
        self.lease.require();old.validate()
        if old==self.current or old.nonce==self.current.nonce:raise ValueError('old session must have a distinct nonce')
        if old.run!=self.current.run or old.config!=self.current.config:raise ValueError('foreign run/config requires separate handling')
        inbox=self._make_inbox(old)
        # Never ingest old native batches or call old publish_ack here.
        return inbox.consume(callback,limit)
    def close(self):self.lease.close()
    def __enter__(self):self.lease.require();return self
    def __exit__(self,*unused):self.close()
