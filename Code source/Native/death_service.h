#ifndef LID_RESEARCH_DEATH_SERVICE_H
#define LID_RESEARCH_DEATH_SERVICE_H
#include "death_queue.h"
/* Research-only worker step. Callbacks run on the bound worker, never in push.
   publish MUST flush and atomically replace before returning success.
   read_ack: 0 absent, -1 I/O error, positive number of bytes (max 1025). */
typedef struct R16IO {
    void *opaque;
    int (*read_ack)(void*,uint8_t*,size_t);
    int (*publish)(void*,const uint8_t*,size_t);
} R16IO;
typedef struct R16Service {
    R15Queue *queue;R16IO io;uint32_t worker;
    uint64_t first,last,last_ms;int have_publication,last_fault;
    uint8_t batch[R15_BATCH*R15_SIZE],ack[R15_SIZE+1];
} R16Service;
enum {R16_PUBLISHED=1,R16_RECEIVED=2,R16_FAULT=4,R16_READ_RETRY=8,R16_WRITE_RETRY=16,R16_BAD_ACK=32};
static int r16_service_init(R16Service*s,R15Queue*q,uint32_t tid,R16IO io){
    if(!s||!io.read_ack||!io.publish||!r15_queue_bind_worker(q,tid))return 0;
    r15_zero(s,sizeof(*s));s->queue=q;s->worker=tid;s->io=io;return 1;
}
static unsigned r16_service_step(R16Service*s,uint32_t tid,uint64_t now_ms){
    if(!s||tid!=s->worker||!r15_queue_is_worker(s->queue,tid))return R16_FAULT;
    unsigned result=0;R15Queue*q=s->queue;
    int count=s->io.read_ack(s->io.opaque,s->ack,sizeof(s->ack));
    uint64_t before=__atomic_load_n(&q->received,__ATOMIC_ACQUIRE);
    if(count<0)result|=R16_READ_RETRY;
    else if(count>0){
        if(count!=R15_SIZE||!r15_queue_received(q,tid,s->ack,(size_t)count))result|=R16_BAD_ACK;
        else if(__atomic_load_n(&q->received,__ATOMIC_ACQUIRE)>before)result|=R16_RECEIVED;
    }
    size_t bytes=0;uint64_t through=0;
    if(!r15_queue_copy(q,tid,s->batch,sizeof(s->batch),&bytes,&through))return result;
    uint64_t first=r15_r64(s->batch+112);int fault=q->staged_fault;
    if(fault)result|=R16_FAULT;
    if(s->have_publication&&s->first==first&&s->last==through&&s->last_fault==fault&&now_ms>=s->last_ms&&now_ms-s->last_ms<1000)return result;
    if(!s->io.publish(s->io.opaque,s->batch,bytes))return result|R16_WRITE_RETRY;
    if(!r15_queue_published(q,tid,through))return result|R16_FAULT;
    s->first=first;s->last=through;s->last_fault=fault;s->last_ms=now_ms;s->have_publication=1;
    return result|R16_PUBLISHED;
}
#endif
