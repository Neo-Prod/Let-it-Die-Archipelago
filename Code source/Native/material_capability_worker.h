#ifndef LID_MATERIAL_CAPABILITY_WORKER_R100_H
#define LID_MATERIAL_CAPABILITY_WORKER_R100_H
#include "material_capability_mailbox.h"
/* Called from the EXISTING transport worker iteration, not a new thread.
   Only these worker callbacks may touch capability files. Owner event only
   uses mailbox. No calls to game and no waiting inside this step. */
enum {R100_WAIT=0,R100_REQUESTED=1,R100_PUBLISHED=2,R100_EXPIRED=3,R100_IO_ERROR=-1,R100_INVALID=-2,R100_CLOCK_ERROR=-3};
typedef struct R100WorkerOps {
 void*context;
 int(*read)(void*,uint8_t*,size_t); /* 0 absent,-1 I/O, else actual bytes */
 int(*publish)(void*,const uint8_t*,size_t); /* 1 success,0 retry */
} R100WorkerOps;
typedef struct R100Worker {
 uint32_t tid,started,attempted,expired,published,fault;
 uint64_t start,last,seen_time;
} R100Worker;
static int r100_worker_step(R100Worker*w,R100Mailbox*b,const R100WorkerOps*ops,uint32_t tid,uint64_t now,int stopping){
 uint8_t raw[R99_SIZE+1];int result;
 if(!w||!b||!b->owner||!ops||!ops->read||!ops->publish||!tid||tid==b->owner||!w->tid||tid!=w->tid)return R100_INVALID;
 if(stopping||w->published||w->expired)return w->published?R100_PUBLISHED:w->expired?R100_EXPIRED:R100_WAIT;
 if(w->fault)return R100_CLOCK_ERROR;
 if(!w->started){w->started=1;w->start=now;w->seen_time=now;}
 if(now<w->seen_time){w->fault=1;return R100_CLOCK_ERROR;}w->seen_time=now;
 if(__atomic_load_n(&b->request_set,__ATOMIC_ACQUIRE)){
  if(!r100_worker_reply(b,raw,R99_SIZE))return R100_REQUESTED;
  if(w->attempted&&now-w->last<1000)return R100_REQUESTED;
  w->attempted=1;w->last=now;
  if(ops->publish(ops->context,raw,R99_SIZE)!=1)return R100_IO_ERROR;
  w->published=1;return R100_PUBLISHED;
 }
 /* Bound absent/legacy/malformed file probing to this startup window.
    Once accepted no more request reads; waiting for owner is RAM-only. */
 if(now-w->start>=120000){w->expired=1;return R100_EXPIRED;}
 if(w->attempted&&now-w->last<1000)return R100_WAIT;
 w->attempted=1;w->last=now;result=ops->read(ops->context,raw,sizeof(raw));
 if(result==0)return R100_WAIT;
 if(result<0)return R100_IO_ERROR;
 if(result!=R99_SIZE||r100_worker_request(b,raw,(size_t)result)!=1)return R100_INVALID;
 /* Request-read timing must not delay the first READY publication. */
 w->attempted=0;return R100_REQUESTED;
}
#endif
