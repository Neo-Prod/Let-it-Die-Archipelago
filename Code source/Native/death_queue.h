#ifndef LID_RESEARCH_DEATH_QUEUE_H
#define LID_RESEARCH_DEATH_QUEUE_H
#include "death_wire.h"
enum {R15_CAPACITY=256,R15_BATCH=64};
/* One validated game-thread producer; one independent I/O worker. All payloads
   copied before release publication, no pointer, allocation, disk or waiting.
   The worker alone stages/publishes/reads durable receipts. Not MPMC. */
typedef struct R15Queue {
    R15Context context;uint32_t owner,worker,initialized,fault;
    uint64_t tail,received,published,fault_sequence,staged;int staged_fault;
    uint8_t data[R15_CAPACITY][R15_SIZE];
} R15Queue;
static int r15_queue_init(R15Queue*q,const R15Context*c,uint32_t owner){
    if(!q||!owner||!r15_context_valid(c)||q->initialized)return 0;
    r15_zero(q,sizeof(*q));q->context=*c;q->owner=owner;__atomic_store_n(&q->initialized,1,__ATOMIC_RELEASE);return 1;
}
static int r15_queue_bind_worker(R15Queue*q,uint32_t tid){
    uint32_t expected=0;
    if(!q||!tid||!__atomic_load_n(&q->initialized,__ATOMIC_ACQUIRE))return 0;
    return __atomic_compare_exchange_n(&q->worker,&expected,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)||expected==tid;
}
static int r15_queue_is_worker(R15Queue*q,uint32_t tid){return q&&tid&&__atomic_load_n(&q->initialized,__ATOMIC_ACQUIRE)&&__atomic_load_n(&q->worker,__ATOMIC_ACQUIRE)==tid;}
static int r15_queue_fault(R15Queue*q,uint32_t tid,uint32_t reason){
    if(!q||tid!=q->owner||!q->initialized||reason<1||reason>4)return 0;
    if(__atomic_load_n(&q->fault,__ATOMIC_ACQUIRE))return 1;
    uint64_t tail=__atomic_load_n(&q->tail,__ATOMIC_ACQUIRE);q->fault_sequence=tail==UINT64_MAX?tail:tail+1;
    __atomic_store_n(&q->fault,reason,__ATOMIC_RELEASE);return 1;
}
/* 1=accepted, -1=wrong thread/uninitialized, -2=sticky fault. Caller must not
   interpret any refusal as a delivered event. Source sequence must be zero. */
static int r15_queue_push(R15Queue*q,uint32_t tid,const R15Frame*source){
    if(!q||!tid||tid!=q->owner||!__atomic_load_n(&q->initialized,__ATOMIC_ACQUIRE))return -1;
    if(__atomic_load_n(&q->fault,__ATOMIC_ACQUIRE))return -2;
    uint64_t tail=__atomic_load_n(&q->tail,__ATOMIC_RELAXED),received=__atomic_load_n(&q->received,__ATOMIC_ACQUIRE);
    if(tail==UINT64_MAX||tail-received>=R15_CAPACITY){r15_queue_fault(q,tid,1);return -2;}
    if(!source||(source->kind!=R15_EVENT&&source->kind!=R39_VISIT&&source->kind!=R72_CHEST)||source->sequence||!r15_same_context(&source->context,&q->context)){r15_queue_fault(q,tid,3);return -2;}
    R15Frame frame=*source;frame.sequence=tail+1;
    if(!r15_encode(q->data[tail%R15_CAPACITY],R15_SIZE,&frame)){r15_queue_fault(q,tid,4);return -2;}
    __atomic_store_n(&q->tail,tail+1,__ATOMIC_RELEASE);return 1;
}
/* Return a copied immutable batch. This does NOT free slots or confirm I/O. */
static int r15_queue_copy(R15Queue*q,uint32_t tid,uint8_t*out,size_t capacity,size_t*bytes,uint64_t*through){
    if(bytes)*bytes=0;if(through)*through=0;
    if(!r15_queue_is_worker(q,tid)||!out||!bytes||!through||capacity<R15_SIZE)return 0;
    uint64_t received=__atomic_load_n(&q->received,__ATOMIC_ACQUIRE),tail=__atomic_load_n(&q->tail,__ATOMIC_ACQUIRE);
    q->staged=0;q->staged_fault=0;
    if(tail>received){
        uint64_t count=tail-received;if(count>R15_BATCH)count=R15_BATCH;if(count>capacity/R15_SIZE)count=capacity/R15_SIZE;
        for(uint64_t i=0;i<count;i++)r15_copy(out+i*R15_SIZE,q->data[(received+i)%R15_CAPACITY],R15_SIZE);
        *bytes=(size_t)count*R15_SIZE;*through=received+count;q->staged=*through;return 1;
    }
    uint32_t fault=__atomic_load_n(&q->fault,__ATOMIC_ACQUIRE);
    if(fault){
        R15Frame frame;r15_zero(&frame,sizeof(frame));frame.context=q->context;frame.kind=R15_FAULT;frame.status=fault;frame.sequence=q->fault_sequence;
        if(!r15_encode(out,R15_SIZE,&frame))return 0;
        *bytes=R15_SIZE;*through=frame.sequence;q->staged=*through;q->staged_fault=1;return 1;
    }
    return 0;
}
/* Called ONLY after the staged batch file has been flushed and atomically
   replaced. Failed I/O means do not call; a socket send is not relevant. */
static int r15_queue_published(R15Queue*q,uint32_t tid,uint64_t through){
    if(!r15_queue_is_worker(q,tid)||!through||through!=q->staged)return 0;
    if(q->staged_fault)return 1; /* Fault notification can never free a slot. */
    uint64_t published=__atomic_load_n(&q->published,__ATOMIC_ACQUIRE);
    if(through<published||through>__atomic_load_n(&q->tail,__ATOMIC_ACQUIRE))return 0;
    __atomic_store_n(&q->published,through,__ATOMIC_RELEASE);return 1;
}
static int r15_queue_received(R15Queue*q,uint32_t tid,const uint8_t*raw,size_t size){
    R15Frame frame;uint64_t through;
    if(!r15_queue_is_worker(q,tid)||!r15_decode(&frame,raw,size)||
       !r15_ack_through(&frame,&q->context,__atomic_load_n(&q->received,__ATOMIC_ACQUIRE),__atomic_load_n(&q->published,__ATOMIC_ACQUIRE),&through))return 0;
    __atomic_store_n(&q->received,through,__ATOMIC_RELEASE);return 1;
}
#endif
