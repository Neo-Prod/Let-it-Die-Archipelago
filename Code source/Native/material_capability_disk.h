#ifndef LID_MATERIAL_CAPABILITY_DISK_R101_H
#define LID_MATERIAL_CAPABILITY_DISK_R101_H
#include "checks_disk_io.h"
#include "material_capability_mailbox.h"
/* Worker-only 256-byte sidecar. The validated existing R26 lease remains the
   lifetime authority. Never close/release it here or loosen legacy publisher. */
typedef struct R101Disk {
 R26Disk*transport;const R100Mailbox*mailbox;uint32_t worker,fault;uint64_t attempt;HANDLE uncertain_handle;
 wchar_t request[1120],ready[1120],stem[1180],temporary[1220];
} R101Disk;
static int r101_same_path(const wchar_t*a,const wchar_t*b){for(unsigned i=0;i<1120;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}return 0;}
/* Called at startup with zero initialized storage, before worker exposure.
   dir must be EXACTLY the directory already validated/leased by R26. */
static int r101_init(R101Disk*out,R26Disk*d,const R100Mailbox*b,const wchar_t*dir){
 R101Disk n={0};wchar_t events[1120];
 if(!out||out->transport||!d||!d->lease||!r26_ops_valid(&d->ops)||!b||!b->owner||!r15_context_valid(&b->context)||!dir||
 !r26_path(events,1120,dir,L"death_native_events.bin")||!r101_same_path(events,d->events))return 0;
 n.transport=d;n.mailbox=b;
 if(!r26_path(n.request,1120,dir,L"material_client_capability.bin")||!r26_path(n.ready,1120,dir,L"material_native_capability.bin")||
 !r26_path(n.stem,1180,dir,L".material-")||!r26_hex(n.stem,1180,b->context.pid,8)||!r26_append(n.stem,1180,L"-")||
 !r26_hex(n.stem,1180,b->context.birth,16)||!r26_append(n.stem,1180,L"-")||!r26_nonce(n.stem,1180,&b->context)||
 !r26_append(n.stem,1180,L".tmp."))return 0;
 *out=n;return 1;
}
static int r101_worker_bind(R101Disk*d,uint32_t tid){
 if(!d||!d->transport||!d->transport->lease||d->fault||!tid||tid==d->mailbox->owner||d->transport->ops.thread_id()!=tid)return 0;
 if(d->worker)return d->worker==tid;d->worker=tid;return 1;
}
static int r101_live(R101Disk*d){return d&&d->transport&&d->transport->lease&&!d->fault&&d->worker&&d->transport->ops.thread_id()==d->worker;}
static int r101_read(void*opaque,uint8_t*out,size_t capacity){
 R101Disk*d=opaque;DWORD got=0;
 if(!out||capacity!=R99_SIZE+1||!r101_live(d))return -1;
 R26Ops*o=&d->transport->ops;
 HANDLE f=o->open(d->request,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE){DWORD e=o->error();return e==2||e==3?0:-1;}
 int ok=o->read(f,out,(DWORD)capacity,&got,0)!=0;
 if(!o->close(f)){d->uncertain_handle=f;d->fault=1;return -1;}
 if(!ok||got>capacity)return -1;
 return got?(int)got:R99_SIZE+1; /* empty is malformed, not absent */
}
static int r101_publish(void*opaque,const uint8_t*raw,size_t n){
 R101Disk*d=opaque;R99Capability reply,request;HANDLE f=INVALID_HANDLE_VALUE;
 if(!r101_live(d)||!r99_decode(&reply,raw,n)||reply.kind!=R99_READY||
 !__atomic_load_n(&d->mailbox->request_set,__ATOMIC_ACQUIRE)||!r99_decode(&request,d->mailbox->request,R99_SIZE)||
 !r15_same_context(&reply.context,&d->mailbox->context)||!r15_equal(reply.catalogue,request.catalogue,32))return 0;
 R26Ops*o=&d->transport->ops;
 for(unsigned i=0;i<8;i++){
  if(d->attempt==UINT64_MAX)return 0;d->temporary[0]=0;
  if(!r26_append(d->temporary,1220,d->stem)||!r26_hex(d->temporary,1220,++d->attempt,16))return 0;
  f=o->open(d->temporary,GENERIC_WRITE,0,0,1 /* CREATE_NEW */,FILE_ATTRIBUTE_NORMAL,0);
  if(f!=INVALID_HANDLE_VALUE)break;DWORD e=o->error();if(e!=80&&e!=183)return 0;
 }
 if(f==INVALID_HANDLE_VALUE)return 0;
 DWORD wrote=0;int ok=o->write(f,raw,R99_SIZE,&wrote,0)&&wrote==R99_SIZE;
 if(ok)ok=o->flush(f)!=0;
 if(!o->close(f)){d->uncertain_handle=f;d->fault=1;return 0;} /* retain uncertain handle, never blindly retry close */
 if(ok)ok=o->replace(d->temporary,d->ready,9 /* REPLACE|WRITE_THROUGH */)!=0;
 if(!ok)o->remove(d->temporary); /* only a file we created, never old READY */
 return ok;
}
#endif
