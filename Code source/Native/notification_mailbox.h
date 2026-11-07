#ifndef LID_NOTIFICATION_MAILBOX_R154_H
#define LID_NOTIFICATION_MAILBOX_R154_H
#include "notification_wire.h"
enum {R154_IDLE=0,R154_PENDING=1,R154_ACCEPTED=2,R154_DISPLAYING=3};
typedef struct R154Mailbox {
 R15Context context;R154Message pending;uint64_t accepted_sequence,retry_at;
 uint32_t state,owner,worker;
} R154Mailbox;
/* Zero-initialized, long-lived storage; initialize before publishing to threads. */
static int r154_mailbox_init(R154Mailbox*m,const R15Context*c,uint32_t owner,uint32_t worker){
 if(!m||!c||!r15_context_valid(c)||!owner||!worker||owner==worker||m->owner||m->worker)return 0;
 r15_zero(m,sizeof(*m));m->context=*c;m->owner=owner;m->worker=worker;return 1;
}
/* Worker-only publication. 2 means duplicate already accepted: repeat ACK only. */
static int r154_worker_offer(R154Mailbox*m,uint32_t tid,const R154Message*request){
 if(!m||!tid||tid!=m->worker||!request||!r154_valid(request)||request->kind!=R154_REQUEST||!r15_same_context(&m->context,&request->context))return -1;
 uint32_t state=__atomic_load_n(&m->state,__ATOMIC_ACQUIRE);
 if(state!=R154_IDLE)return 0;
 if(request->sequence==m->accepted_sequence&&m->accepted_sequence)return 2;
 if(m->accepted_sequence==UINT64_MAX||request->sequence!=m->accepted_sequence+1)return -1;
 m->pending=*request;__atomic_store_n(&m->state,R154_PENDING,__ATOMIC_RELEASE);return 1;
}
/* Owner-only: absolutely no callbacks on empty/wrong-thread/throttled paths.
   Callback returns1 only when the native popup queue has accepted ownership. */
static int r154_owner_deliver(R154Mailbox*m,uint32_t tid,uint64_t now,void*opaque,int(*display)(void*,const R154Message*)){
 if(!m||!tid||tid!=m->owner||__atomic_load_n(&m->state,__ATOMIC_ACQUIRE)!=R154_PENDING)return 0;
 if(!display||now<m->retry_at)return 0;
 m->retry_at=now>UINT64_MAX-250?UINT64_MAX:now+250;
 __atomic_store_n(&m->state,R154_DISPLAYING,__ATOMIC_RELEASE);
 if(display(opaque,&m->pending)!=1){__atomic_store_n(&m->state,R154_PENDING,__ATOMIC_RELEASE);return 0;}
 __atomic_store_n(&m->state,R154_ACCEPTED,__ATOMIC_RELEASE);return 1;
}
/* Worker-only, preserves accepted state across any failed ACK disk publication. */
static int r154_worker_ack(R154Mailbox*m,uint32_t tid,R154Message*out){
 if(!m||!out||!tid||tid!=m->worker||__atomic_load_n(&m->state,__ATOMIC_ACQUIRE)!=R154_ACCEPTED)return 0;
 r15_zero(out,sizeof(*out));out->context=m->context;out->kind=R154_ACK;out->sequence=m->pending.sequence;return 1;
}
/* Call ONLY after publishing the independent presentation ACK successfully. */
static int r154_worker_ack_published(R154Mailbox*m,uint32_t tid,uint64_t sequence){
 if(!m||!tid||tid!=m->worker||__atomic_load_n(&m->state,__ATOMIC_ACQUIRE)!=R154_ACCEPTED||sequence!=m->pending.sequence)return 0;
 m->accepted_sequence=sequence;__atomic_store_n(&m->state,R154_IDLE,__ATOMIC_RELEASE);return 1;
}
#endif
