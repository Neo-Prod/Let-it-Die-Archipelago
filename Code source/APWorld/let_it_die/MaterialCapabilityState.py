"""Nonblocking capability status, independent of check receipt/confirmation."""
import math
import time
from .MaterialCapability import request,publish_request,matching_ready

class MaterialCapabilityState:
    def __init__(self,directory,context,config):
        self.directory=directory
        self.request=request(context,config.catalogue_version,config.fingerprint)
        self.status='waiting';self.published=False;self.started=None;self.last=None;self.clock_fault=False

    def poll(self):
        if self.status=='ready' or self.clock_fault:return self.status
        now=time.monotonic()
        if not math.isfinite(now) or (self.last is not None and now<self.last):
            self.clock_fault=True;self.status='clock_error';return self.status
        if self.started is None:self.started=now
        if self.last is not None and now-self.last<1:return self.status
        self.last=now
        expired=now-self.started>=120
        if not self.published:
            if expired:self.status='unconfirmed';return self.status
            try:publish_request(self.directory,self.request)
            except OSError:self.status='request_io_error';return self.status
            self.published=True
        try:ready=matching_ready(self.directory,self.request)
        except OSError:self.status='reply_io_error';return self.status
        except (ValueError,TypeError):self.status='reply_invalid';return self.status
        self.status='ready' if ready else 'unconfirmed' if expired else 'waiting'
        return self.status
