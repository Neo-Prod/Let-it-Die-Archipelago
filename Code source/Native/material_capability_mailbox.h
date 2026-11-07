#ifndef LID_MATERIAL_CAPABILITY_MAILBOX_R100_H
#define LID_MATERIAL_CAPABILITY_MAILBOX_R100_H
#include "material_capability.h"
/* One exchange per immutable native transport session. Initialize before
   starting worker; never reset/reuse until worker joined and owner quiescent.
   Each payload is written once, then release-published and never overwritten.
   Only state words cross threads after initialization: no seqlock data race. */
typedef struct R100Mailbox {
 R15Context context;uint32_t owner;
 uint32_t request_set,reply_set;
 uint8_t request[R99_SIZE],reply[R99_SIZE];
} R100Mailbox;
static int r100_init(R100Mailbox*b,const R15Context*c,uint32_t owner){
 if(!b||!r15_context_valid(c)||!owner)return 0;
 /* Fresh zero-initialized storage is a caller lifetime precondition. */
 if(b->owner||b->request_set||b->reply_set)return 0;
 b->context=*c;b->owner=owner;return 1;
}
/* Exactly one worker caller, as with the existing transport's disk service.
   1=first request;2=identical retry;0=invalid;-1=conflicting request.
   Validated catalogue must originate from client's validated CheckConfig. */
static int r100_worker_request(R100Mailbox*b,const uint8_t*raw,size_t n){
 R99Capability m;
 if(!b||!b->owner||!r99_decode(&m,raw,n)||m.kind!=R99_REQUEST||!r15_same_context(&m.context,&b->context))return 0;
 if(__atomic_load_n(&b->request_set,__ATOMIC_ACQUIRE))return r15_equal(b->request,raw,R99_SIZE)?2:-1;
 r15_copy(b->request,raw,R99_SIZE);__atomic_store_n(&b->request_set,1,__ATOMIC_RELEASE);return 1;
}
/* Bounded, no I/O/wait/allocation. Does NOT require next hub entry. The host
   calls on an existing owner event and supplies fresh real proofs. */
static int r100_owner_ready(R100Mailbox*b,const R99Proof*p){
 R99Capability request,reply;uint8_t raw[R99_SIZE];
 if(!b||!p||p->owner!=b->owner||p->current_thread!=b->owner||
 !__atomic_load_n(&b->request_set,__ATOMIC_ACQUIRE)||
 !r99_decode(&request,b->request,R99_SIZE)||!r99_ready(&reply,&request,&b->context,p)||!r99_encode(raw,sizeof(raw),&reply))return 0;
 if(__atomic_load_n(&b->reply_set,__ATOMIC_ACQUIRE))return r15_equal(raw,b->reply,R99_SIZE)?2:-1;
 r15_copy(b->reply,raw,R99_SIZE);__atomic_store_n(&b->reply_set,1,__ATOMIC_RELEASE);return 1;
}
static int r100_owner_request(const R100Mailbox*b,uint32_t tid,R99Capability*out){
 if(!b||tid!=b->owner||!out||!__atomic_load_n(&b->request_set,__ATOMIC_ACQUIRE))return 0;
 return r99_decode(out,b->request,R99_SIZE);
}
/* Worker retries actual disk publication until successful. This read never
   consumes the reply; I/O failure must not require another owner event. */
static int r100_worker_reply(const R100Mailbox*b,uint8_t*out,size_t n){
 if(!b||!out||n!=R99_SIZE||!__atomic_load_n(&b->reply_set,__ATOMIC_ACQUIRE))return 0;
 r15_copy(out,b->reply,R99_SIZE);return 1;
}
#endif
