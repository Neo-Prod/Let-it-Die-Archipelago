#ifndef LID_KILL_COIN_WORKER_R141_H
#define LID_KILL_COIN_WORKER_R141_H
#include "kill_coin_core.h"
/* Isolated protocol. Worker alone calls IO; owner alone calls wallet/core.
   IO put returns success only after durable atomic publication. Missing is 0,
   malformed/IO failure is -1, exact canonical read is 1. No game-save claim. */
enum {R141_GRANT_FILE=1,R141_ACK_FILE=2,R141_LEDGER_FILE=3,R141_INTENT_FILE=4};
enum {R141_BOOT=0,R141_EMPTY,R141_PROBE,R141_NEED_INTENT,R141_AUTHORIZED,
 R141_EXECUTING,R141_APPLIED,R141_DURABLE,R141_CANCEL,R141_UNCERTAIN,R141_STOPPED,R141_NATIVE_SAVE};
typedef struct R141IO {
 void*context;int(*read)(void*,unsigned,R138Frame*);
 int(*put)(void*,unsigned,const R138Frame*);int(*remove_intent)(void*);
} R141IO;
typedef struct R141Coins {
 uint32_t initialized,owner,worker,state,owner_busy,owner_initialized;
 uint32_t has_restored,has_last,saved,cleaned,acked;R15Context context;
 R138Frame restored,last,request,outcome;R138Coins core;
} R141Coins;
static uint32_t r141_state(const R141Coins*s){return __atomic_load_n(&s->state,__ATOMIC_ACQUIRE);}
static void r141_publish(R141Coins*s,uint32_t v){__atomic_store_n(&s->state,v,__ATOMIC_RELEASE);}
static int r141_cas(R141Coins*s,uint32_t from,uint32_t to){return __atomic_compare_exchange_n(&s->state,&from,to,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
static int r141_init(R141Coins*s,const R15Context*c,uint32_t owner,uint32_t worker){
 if(!s||s->initialized||!r15_context_valid(c)||!owner||!worker||owner==worker)return 0;
 R141Coins v={0};v.initialized=1;v.owner=owner;v.worker=worker;v.context=*c;*s=v;return 1;
}
static int r141_ledger_valid(const R141Coins*s,const R138Frame*f){return r138_valid(f)&&f->kind==R138_ACK&&f->status==R138_APPLIED&&r138_same_slot(&s->context,&f->context);}
/* Worker callback only. No access to owner core, no native effects. */
static int r141_worker(R141Coins*s,const R141IO*io,uint32_t tid,int stop){
 if(!s||s->initialized!=1||tid!=s->worker||!io||!io->read||!io->put||!io->remove_intent)return -1;
 uint32_t state=r141_state(s);R138Frame f={0};int result;
 if(state==R141_UNCERTAIN||state==R141_STOPPED)return 0;
 if(state==R141_BOOT){
  if(stop){r141_publish(s,R141_STOPPED);return 1;}
  result=io->read(io->context,R141_LEDGER_FILE,&f);
  if(result<0||(result>0&&!r141_ledger_valid(s,&f)))goto uncertain;
  if(result>0){s->last=f;s->restored=f;s->has_last=s->has_restored=1;}
  result=io->read(io->context,R141_INTENT_FILE,&f);
  if(result<0)goto uncertain;
  if(result>0){
   if(!r138_valid(&f)||f.kind!=R138_GRANT||!r138_same_slot(&s->context,&f.context)||!s->has_last||!r138_key(&f,&s->last))goto uncertain;
   if(!io->remove_intent(io->context))return 0;
  }
  r141_publish(s,R141_EMPTY);return 1;
 }
 if(state==R141_EMPTY){
  if(stop){r141_publish(s,R141_STOPPED);return 1;}
  result=io->read(io->context,R141_GRANT_FILE,&f);if(result<0)goto uncertain;if(!result)return 0;
  if(!r138_valid(&f)||f.kind!=R138_GRANT)goto uncertain;
  if(!r15_same_context(&f.context,&s->context))return 0;
  if(s->has_last&&f.index<=s->last.index){
   if(f.index<s->last.index)return 0;if(!r138_key(&f,&s->last))goto uncertain;
   if(s->acked)return 0;
   f=s->last;f.context=s->context;f.status=R138_REPLAY;
   if(!io->put(io->context,R141_ACK_FILE,&f))return 0;s->acked=1;return 1;
  }
  s->request=f;s->saved=s->cleaned=s->acked=0;r141_publish(s,R141_PROBE);return 1;
 }
 if(state==R141_PROBE){if(stop)return r141_cas(s,R141_PROBE,R141_STOPPED);return 0;}
 if(state==R141_NEED_INTENT){
  if(stop){r141_publish(s,R141_STOPPED);return 1;}
  if(!io->put(io->context,R141_INTENT_FILE,&s->request))return 0;
  r141_publish(s,R141_AUTHORIZED);return 1;
 }
 if(state==R141_AUTHORIZED){if(stop)return r141_cas(s,R141_AUTHORIZED,R141_CANCEL);return 0;}
 if(state==R141_EXECUTING||state==R141_NATIVE_SAVE)return 0;
 if(state==R141_APPLIED){
  if(!r138_valid(&s->outcome)||s->outcome.kind!=R138_ACK||!r138_key(&s->outcome,&s->request)||!r15_same_context(&s->outcome.context,&s->context))goto uncertain;
  if(!s->saved){
   f=s->outcome;f.status=R138_APPLIED;
   if(!io->put(io->context,R141_LEDGER_FILE,&f))return 0;
   s->last=f;s->has_last=1;s->saved=1;
  }
  if(!s->cleaned){if(!io->remove_intent(io->context))return 0;s->cleaned=1;}
  if(!io->put(io->context,R141_ACK_FILE,&s->outcome))return 0;
  s->acked=1;
  r141_publish(s,stop?R141_STOPPED:R141_DURABLE);return 1;
 }
 if(state==R141_CANCEL){
  if(!io->remove_intent(io->context))return 0;
  r141_publish(s,stop?R141_STOPPED:R141_EMPTY);return 1;
 }
 if(state==R141_DURABLE){if(stop){r141_publish(s,R141_STOPPED);return 1;}return 0;}
uncertain:r141_publish(s,R141_UNCERTAIN);return -1;
}
/* Owner callback only. Never calls IO. Busy guard includes preflight callbacks. */
static int r141_owner(R141Coins*s,const R138Ops*ops){
 if(!s||s->initialized!=1||!ops||!ops->thread||ops->thread(ops->context)!=s->owner)return R138_INVALID;
 if(s->owner_busy)return R138_WAIT;s->owner_busy=1;int result=R138_WAIT;
 uint32_t state=r141_state(s);
 if(state==R141_BOOT||state==R141_UNCERTAIN||state==R141_STOPPED)goto done;
 if(!s->owner_initialized){
  if(!r138_init(&s->core,s->owner,&s->context,s->has_restored?&s->restored:0)){result=R138_FAULT;r141_publish(s,R141_UNCERTAIN);goto done;}
  s->owner_initialized=1;
 }
 if(state==R141_DURABLE){
  if(s->core.phase==R138_WAIT_DURABLE&&!r138_committed(&s->core,ops)){result=R138_FAULT;r141_publish(s,R141_UNCERTAIN);goto done;}
  (void)r141_cas(s,R141_DURABLE,R141_EMPTY);goto done;
 }
 if(state==R141_PROBE){
  uint64_t subject=0;int32_t money=-1;
  if(!ops->ready||!ops->read||!ops->add||ops->ready(ops->context)!=1||ops->read(ops->context,&subject,&money)!=1||!subject||money<0)goto done;
  if((uint32_t)money>2147483647u-s->request.amount){result=R138_REFUSED;goto done;}
  (void)r141_cas(s,R141_PROBE,R141_NEED_INTENT);goto done;
 }
 if(state!=R141_NATIVE_SAVE&&(state!=R141_AUTHORIZED||!r141_cas(s,R141_AUTHORIZED,R141_EXECUTING)))goto done;
 result=r138_step(&s->core,ops,&s->request,&s->outcome);
 if(result==R138_ADDED||result==R138_DUPLICATE)r141_publish(s,R141_APPLIED);
 else if(result==R138_PENDING)r141_publish(s,R141_NATIVE_SAVE);
 else if(result==R138_WAIT||result==R138_REFUSED)r141_publish(s,R141_CANCEL);
 else r141_publish(s,R141_UNCERTAIN); /* Durable intent stays, preventing blind replay. */
done:s->owner_busy=0;return result;
}
static int r141_drained(const R141Coins*s){if(!s||!s->initialized)return 0;uint32_t v=r141_state(s);return v==R141_STOPPED||v==R141_UNCERTAIN;}
#endif
