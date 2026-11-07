#ifndef LID_NOTIFICATION_DISK_R154_H
#define LID_NOTIFICATION_DISK_R154_H
#include "checks_disk_io.h"
#include "notification_mailbox.h"
typedef struct R154Disk {
 R26Disk*parent;R154Mailbox mailbox;R154Message scratch;
 uint8_t input[R154_SIZE+1],output[R154_SIZE];
 wchar_t directory[1000],request[1120],ack[1120],temporary[1280];
 uint64_t attempt;uint32_t ready;
} R154Disk;
static int r154_disk_init(R154Disk*d,R26Disk*parent,const R15Context*c,uint32_t owner,uint32_t worker){
 if(!d||!parent||!parent->lease||!r26_path(d->request,1120,d->directory,L"notification_request.bin")||
 !r26_path(d->ack,1120,d->directory,L"notification_ack.bin")||!r154_mailbox_init(&d->mailbox,c,owner,worker))return 0;
 d->parent=parent;__atomic_store_n(&d->ready,1,__ATOMIC_RELEASE);return 1;
}
static int r154_disk_publish(R154Disk*d){
 R26Ops*o=&d->parent->ops;if(d->attempt==UINT64_MAX)return 0;
 d->temporary[0]=0;
 if(!r26_append(d->temporary,1280,d->parent->stem)||!r26_append(d->temporary,1280,L"notify-")||!r26_hex(d->temporary,1280,++d->attempt,16))return 0;
 HANDLE f=o->open(d->temporary,GENERIC_WRITE,0,0,1 /* CREATE_NEW */,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE)return 0;
 DWORD wrote=0;int ok=o->write(f,d->output,R154_SIZE,&wrote,0)&&wrote==R154_SIZE;
 if(ok)ok=o->flush(f)!=0;int closed=o->close(f)!=0;if(!closed)ok=0;
 if(ok)ok=o->replace(d->temporary,d->ack,9)!=0;
 if(!ok&&closed)o->remove(d->temporary);return ok;
}
/* Existing background worker only. No owner/game-thread filesystem access. */
static int r154_disk_step(R154Disk*d,uint32_t tid){
 if(!d||!__atomic_load_n(&d->ready,__ATOMIC_ACQUIRE)||tid!=d->mailbox.worker||d->parent->ops.thread_id()!=tid)return -1;
 uint32_t state=__atomic_load_n(&d->mailbox.state,__ATOMIC_ACQUIRE);
 if(state==R154_ACCEPTED){
  if(!r154_worker_ack(&d->mailbox,tid,&d->scratch)||!r154_encode(&d->scratch,d->output)||!r154_disk_publish(d))return 0;
  return r154_worker_ack_published(&d->mailbox,tid,d->scratch.sequence);
 }
 if(state!=R154_IDLE)return 0;
 R26Ops*o=&d->parent->ops;HANDLE f=o->open(d->request,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE)return 0;
 DWORD got=0;int ok=o->read(f,d->input,R154_SIZE+1,&got,0)!=0;int closed=o->close(f)!=0;
 if(!ok||!closed||!r154_decode(d->input,got,&d->scratch))return 0;
 /* Duplicate accepted request needs no new write: its ACK is already present.
    An unpublished ACK remains ACCEPTED and retries the branch above. */
 return r154_worker_offer(&d->mailbox,tid,&d->scratch)==1;
}
#endif
