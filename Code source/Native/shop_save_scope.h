#ifndef LID_SHOP_SAVE_SCOPE_H
#define LID_SHOP_SAVE_SCOPE_H
#include "momoka_native_dispatch.h"
/* Observer ONLY, no gate/wait/marker/payment/save scheduling. One reserved OS
   TLS slot and every context must remain resident, distinct from other slots.
   Native exceptions are not intercepted; do not enable AP mutation based on
   this metadata lifetime helper. Observer callbacks must not raise or mutate
   native state. Original Save/bridge is always invoked exactly once. */
typedef struct LidShopSaveScope LidShopSaveScope;
typedef struct LidShopSaveFrame {
    const LidShopSaveScope*scope;struct LidShopSaveFrame*previous;
    uintptr_t runner,text,caller;uint64_t sequence;uint32_t owner;unsigned live;
} LidShopSaveFrame;
typedef struct LidShopSaveScopeOps {
    LidMomokaTlsOps tls;
    void*original_context;unsigned char(*original)(void*,void*,void*);
    void*observer_context;void(*observe)(void*,const LidShopSaveScope*,const LidShopSaveFrame*,unsigned,unsigned char);
} LidShopSaveScopeOps;
struct LidShopSaveScope {
    unsigned initialized,slot,active,fault;uint32_t writer;uint64_t process_epoch,next;
    LidShopSaveScopeOps ops;LidShopSaveFrame frames[64];
};
static int lid_shop_save_scope_prepare(LidShopSaveScope*s,uint64_t process_epoch,uint32_t writer,
                                     unsigned slot,const LidShopSaveScopeOps*ops){
    if(!s||s->initialized||!process_epoch||!writer||slot==~0u||!ops||!ops->original||
       !ops->tls.get||!ops->tls.set||!ops->tls.thread||!ops->tls.error_get||!ops->tls.error_set)return 0;
    s->ops=*ops;s->process_epoch=process_epoch;s->writer=writer;s->slot=slot;s->initialized=1;return 1;
}
static LidShopSaveFrame*lid_shop_save_frame(const LidShopSaveScope*s){
    if(!s||!s->initialized)return 0;const LidMomokaTlsOps*t=&s->ops.tls;unsigned error=t->error_get(t->context);
    LidShopSaveFrame*f=t->get(t->context,s->slot);
    if(f&&(f->scope!=s||!__atomic_load_n(&f->live,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&f->owner,__ATOMIC_ACQUIRE)!=t->thread(t->context)))f=0;
    t->error_set(t->context,error);return f;
}
/* Called at ORIGINAL hook entry, with its own captured caller, not from a
   nested proxy's return address. Native input pointers valid only during call.
   On no slot/fault/reentry exhaustion, forward original without observation. */
static unsigned char lid_shop_save_scope_call(LidShopSaveScope*s,void*runner,void*text,uintptr_t caller){
    const LidMomokaTlsOps*t=&s->ops.tls;unsigned error=t->error_get(t->context);LidShopSaveFrame*f=0;
    uint32_t tid=t->thread(t->context);uint64_t next=__atomic_load_n(&s->next,__ATOMIC_ACQUIRE);
    __atomic_add_fetch(&s->active,1,__ATOMIC_ACQ_REL);
    if(tid&&!__atomic_load_n(&s->fault,__ATOMIC_ACQUIRE)){
        if(next==UINT64_MAX)__atomic_store_n(&s->fault,1,__ATOMIC_RELEASE);
        else if(__atomic_compare_exchange_n(&s->next,&next,next+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))for(unsigned i=0;i<64;i++){
            uint32_t empty=0;if(__atomic_compare_exchange_n(&s->frames[i].owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
                f=&s->frames[i];break;
            }
        }
    }
    if(f){
        f->scope=s;f->runner=(uintptr_t)runner;f->text=(uintptr_t)text;f->caller=caller;f->sequence=next+1;
        f->previous=t->get(t->context,s->slot);__atomic_store_n(&f->live,1,__ATOMIC_RELEASE);
        if(t->set(t->context,s->slot,f)!=1){
            f->runner=f->text=f->caller=0;__atomic_store_n(&f->live,0,__ATOMIC_RELEASE);
            __atomic_store_n(&f->owner,0,__ATOMIC_RELEASE);f=0;
        }
    }
    if(f&&s->ops.observe)s->ops.observe(s->ops.observer_context,s,f,1,0);
    t->error_set(t->context,error);
    unsigned char result=s->ops.original(s->ops.original_context,runner,text);
    error=t->error_get(t->context);
    if(f){
        if(s->ops.observe)s->ops.observe(s->ops.observer_context,s,f,2,result);
        int restored=t->set(t->context,s->slot,f->previous)==1;
        f->runner=f->text=f->caller=0;__atomic_store_n(&f->live,0,__ATOMIC_RELEASE);
        if(restored)__atomic_store_n(&f->owner,0,__ATOMIC_RELEASE);
        else __atomic_store_n(&s->fault,1,__ATOMIC_RELEASE);
    }
    __atomic_sub_fetch(&s->active,1,__ATOMIC_ACQ_REL);t->error_set(t->context,error);return result;
}
#endif
