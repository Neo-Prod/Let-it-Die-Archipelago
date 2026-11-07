#ifndef LID_CHECKS_CHEST_OBSERVER_R74_H
#define LID_CHECKS_CHEST_OBSERVER_R74_H
#include "checks_chest_capture.h"
typedef void (*R74Dispatch)(void*,void*,void*,void*);
typedef struct R74Stamp {R15Context context;uint64_t epoch,revision;char floor[16],area[32];} R74Stamp;
typedef struct R74Ops {
    R6Ops memory;void*context;uint32_t(*thread)(void*);int(*snapshot)(void*,R74Stamp*);
    int(*scope_begin)(void*);void(*scope_end)(void*,int);
    R15Queue*queue;uint64_t vtable;
} R74Ops;
typedef struct R74Observer {
    R74Dispatch original;uint32_t owner,armed,busy;unsigned count;uint64_t function;
    R73ChestLayout cache[4];unsigned queued,refused,seen,binding_refused,context_refused;
} R74Observer;
static int r74_same_stamp(const R74Stamp*a,const R74Stamp*b){
    return a->epoch&&a->epoch==b->epoch&&a->revision==b->revision&&r15_same_context(&a->context,&b->context)&&
      r15_equal(a->floor,b->floor,16)&&r15_equal(a->area,b->area,32);
}
/* The original is always forwarded, including when the observer is unarmed.
   Only owner-thread code touches the cache/counters/busy flag. */
static void r74_observe(R74Observer*s,const R74Ops*ops,void*object,void*function,void*params,void*result){
    R74Dispatch original=__atomic_load_n(&s->original,__ATOMIC_ACQUIRE);
    if(!original)return; /* Never reachable through a successfully installed slot. */
    if(!__atomic_load_n(&s->armed,__ATOMIC_ACQUIRE)||!ops||!ops->thread||!ops->snapshot||!ops->queue||
       !s->owner||ops->thread(ops->context)!=s->owner||params||result||s->busy||
       (s->function&&s->function!=(uint64_t)(uintptr_t)function)){
        original(object,function,params,result);return;
    }
    if(!s->function&&!r6_name(&ops->memory,(uint64_t)(uintptr_t)function,"TreasureBoxOpen")){
        original(object,function,params,result);return;
    }
    s->busy=1;s->seen++;R74Stamp before={0},after={0};R73ChestSample a={0},b={0};R73ChestLayout*l=0;
    uint64_t cls=0;uint8_t count=1;int observe=0;
    if(ops->snapshot(ops->context,&before)&&r6_at(&ops->memory,(uint64_t)(uintptr_t)object,0x50,&cls,8)&&
       r6_at(&ops->memory,(uint64_t)(uintptr_t)function,0xe1,&count,1)&&!count){
        for(unsigned i=0;i<s->count;i++)if(s->cache[i].object_class==cls){l=&s->cache[i];break;}
        if(!l&&s->count<4){
            R73ChestLayout candidate;int scoped=ops->scope_begin?ops->scope_begin(ops->context):0;
            int bound=r73_bind(&ops->memory,(uint64_t)(uintptr_t)object,ops->vtable,(uint64_t)(uintptr_t)function,&candidate);
            if(ops->scope_end)ops->scope_end(ops->context,scoped);
            if(bound){s->cache[s->count]=candidate;l=&s->cache[s->count++];s->function=candidate.function;}
            else s->binding_refused++;
        }
        observe=l&&l->function==(uint64_t)(uintptr_t)function&&r73_sample(&ops->memory,(uint64_t)(uintptr_t)object,l,&a)&&!a.open;
    }else s->context_refused++;
    original(object,function,params,result);
    if(observe&&ops->thread(ops->context)==s->owner&&r73_sample(&ops->memory,(uint64_t)(uintptr_t)object,l,&b)&&
       ops->snapshot(ops->context,&after)&&r74_same_stamp(&before,&after)){
        int captured=r73_capture(ops->queue,s->owner,&after.context,after.epoch,after.floor,after.area,&a,&b,1);
        if(captured==1)s->queued++;else if(captured<0)s->refused++;
    }
    s->busy=0;
}
typedef int(*R74Protect)(void*,void*,size_t,uint32_t,uint32_t*);
static int r74_patch(R74Observer*s,const R6Ops*memory,R74Protect protect,void*context,
    R74Dispatch*slot,R74Dispatch expected,R74Dispatch hook){
    static const uint8_t signature[16]={0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
    R74Dispatch prior=0;uint8_t prologue[16];uint32_t old=0,ignored=0;
    if(!s||!memory||!protect||!slot||!expected||!hook||!s->owner||((uintptr_t)slot&7)||
       !r6_copy(memory,(uint64_t)(uintptr_t)slot,&prior,8)||prior!=expected||
       !r6_copy(memory,(uint64_t)(uintptr_t)expected,prologue,16)||!r15_equal(prologue,signature,16))return 0;
    __atomic_store_n(&s->original,expected,__ATOMIC_RELEASE);
    if(!protect(context,slot,8,4,&old))return 0;
    int swapped=__atomic_compare_exchange_n(slot,&prior,hook,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    int restored=protect(context,slot,8,old,&ignored);
    __atomic_store_n(&s->armed,swapped&&restored,__ATOMIC_RELEASE);
    return swapped&&restored;
}
#endif
