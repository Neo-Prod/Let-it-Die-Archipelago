#ifndef LID_KILL_COIN_CORE_R138_H
#define LID_KILL_COIN_CORE_R138_H
#include "kill_coin_wire.h"
/* Owner-only logic. No game calls, polling, files or native-save claim here.
   Integration supplies verified current-wallet callbacks and durable IO. */
enum {R138_IDLE=0,R138_WAIT_DURABLE=1,R138_UNCERTAIN=2,R138_NATIVE_SAVE=3};
enum {R138_WAIT=0,R138_ADDED=1,R138_DUPLICATE=2,R138_PENDING=3,R138_INVALID=-1,R138_REFUSED=-2,R138_FAULT=-3,R138_STALE=-4};
typedef struct R138Ops {
 void*context;uint32_t(*thread)(void*);int(*ready)(void*);
 int(*read)(void*,uint64_t*subject,int32_t*money);
 int(*add)(void*,uint64_t subject,uint32_t amount);
 /* Optional: add returns 2 only after starting an owned native save.
    poll returns 0 while pending, 1 after save, negative on uncertainty. */
 int(*poll)(void*);
} R138Ops;
typedef struct R138Coins {uint32_t initialized,owner,busy,phase,has_last;R15Context context;R138Frame pending,last;uint64_t pending_subject;} R138Coins;
static int r138_same_slot(const R15Context*a,const R15Context*b){return r15_equal(a->run,b->run,32)&&r15_equal(a->config,b->config,32);}
static int r138_key(const R138Frame*a,const R138Frame*b){return a->index==b->index&&a->item==b->item&&a->amount==b->amount;}
static int r138_init(R138Coins*s,uint32_t owner,const R15Context*c,const R138Frame*restored){
 if(!s||s->initialized||!owner||!r15_context_valid(c))return 0;
 if(restored&&(!r138_valid(restored)||restored->kind!=R138_ACK||restored->status!=R138_APPLIED||!r138_same_slot(c,&restored->context)))return 0;
 R138Coins v={0};v.initialized=1;v.owner=owner;v.context=*c;if(restored){v.last=*restored;v.has_last=1;}*s=v;return 1;
}
static int r138_step(R138Coins*s,const R138Ops*o,const R138Frame*g,R138Frame*out){
 uint64_t subject,after_subject;int32_t before,after;int result;
 if(!s||s->initialized!=1||s->phase>R138_NATIVE_SAVE||s->has_last>1||!o||!o->thread||o->thread(o->context)!=s->owner||!out||!r138_valid(g)||g->kind!=R138_GRANT||!r15_same_context(&s->context,&g->context))return R138_INVALID;
 if(s->busy)return R138_WAIT;if(s->phase==R138_UNCERTAIN)return R138_FAULT;
 if(s->phase==R138_WAIT_DURABLE){if(!r138_key(g,&s->pending))return R138_WAIT;*out=s->pending;return R138_ADDED;}
 if(s->phase==R138_NATIVE_SAVE){
  if(!r138_key(g,&s->pending))return R138_WAIT;
  s->busy=1;result=o->poll?o->poll(o->context):-1;
  if(!result){result=R138_PENDING;goto done;}
  if(result!=1||!o->read||o->read(o->context,&after_subject,&after)!=1||
     after_subject!=s->pending_subject||after<0||(uint32_t)after!=s->pending.after){
   s->phase=R138_UNCERTAIN;result=R138_FAULT;goto done;
  }
  s->phase=R138_WAIT_DURABLE;*out=s->pending;result=R138_ADDED;goto done;
 }
 if(s->has_last&&g->index<=s->last.index){
  if(g->index<s->last.index)return R138_STALE;if(!r138_key(g,&s->last))return R138_INVALID;
  *out=s->last;out->context=s->context;out->status=R138_REPLAY;return R138_DUPLICATE;
 }
 if(!o->ready||!o->read||!o->add)return R138_WAIT;
 s->busy=1;result=R138_WAIT;
 if(o->ready(o->context)!=1)goto done;
 if(o->read(o->context,&subject,&before)!=1||!subject||before<0)goto done;
 if((uint32_t)before>2147483647u-g->amount){result=R138_REFUSED;goto done;}
 int called=o->add(o->context,subject,g->amount);
 if(called==2){
  if(!o->poll){s->phase=R138_UNCERTAIN;result=R138_FAULT;goto done;}
  s->pending=*g;s->pending.kind=R138_ACK;s->pending.status=R138_APPLIED;
  s->pending.before=(uint32_t)before;s->pending.after=(uint32_t)before+g->amount;
  s->pending_subject=subject;s->phase=R138_NATIVE_SAVE;result=R138_PENDING;goto done;
 }
 /* Multi-model native pickup can mutate authoritative rewards even when the
    frontend value is unchanged. Negative means ambiguous effect: NEVER retry. */
 if(called<0){s->phase=R138_UNCERTAIN;result=R138_FAULT;goto done;}
 if(o->read(o->context,&after_subject,&after)!=1||after_subject!=subject){s->phase=R138_UNCERTAIN;result=R138_FAULT;goto done;}
 if(called!=1){
  if(after!=before){s->phase=R138_UNCERTAIN;result=R138_FAULT;}else result=R138_REFUSED;
  goto done;
 }
 if(after<0||(uint32_t)after!=(uint32_t)before+g->amount){s->phase=R138_UNCERTAIN;result=R138_FAULT;goto done;}
 s->pending=*g;s->pending.kind=R138_ACK;s->pending.status=R138_APPLIED;s->pending.before=(uint32_t)before;s->pending.after=(uint32_t)after;
 s->phase=R138_WAIT_DURABLE;*out=s->pending;result=R138_ADDED;
done:s->busy=0;return result;
}
/* Call on owner only AFTER worker confirms durable receipt storage. The
   worker must not publish an application ACK before that durable write. */
static int r138_committed(R138Coins*s,const R138Ops*o){
 if(!s||s->initialized!=1||!o||!o->thread||o->thread(o->context)!=s->owner||s->busy||s->phase!=R138_WAIT_DURABLE||!r138_valid(&s->pending)||s->pending.kind!=R138_ACK||s->pending.status!=R138_APPLIED)return 0;
 s->last=s->pending;s->has_last=1;s->phase=R138_IDLE;r15_zero(&s->pending,sizeof(s->pending));return 1;
}
#endif
