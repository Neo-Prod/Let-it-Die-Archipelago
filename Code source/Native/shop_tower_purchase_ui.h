#ifndef LID_SHOP_TOWER_PURCHASE_UI_H
#define LID_SHOP_TOWER_PURCHASE_UI_H
#include "shop_tower_ui_current.h"
#include "momoka_owned_slot.h"
#if !LID_SHOP_EXEC_SEH_ENABLED
#error "Tower UI wrappers require compiler finally"
#endif
/* Caller supplies zeroed final-address resident storage, with no later rebind.
   Tower namespace only. UI confirmation is NOT payment evidence. Never pass
   these slots to Momoka LIDSHOP3 reserve-only wallet receipts. All host/storage
   and wrapper addresses must stay resident after the first slot publication. */
enum {LID_TQ_EMPTY,LID_TQ_RESERVED,LID_TQ_CONFIRMED,LID_TQ_UNCERTAIN,
      LID_TQ_PAYING,LID_TQ_PAID,LID_TQ_READY,LID_TQ_PUBLISHED}; /* append: prior values unchanged */
typedef struct LidTowerQueueSlot {
    unsigned phase;uint64_t token;LidTowerUiSnapshot observation;
} LidTowerQueueSlot;
typedef struct LidTowerQueueOps {
    void*context;uint32_t(*thread)(void*);
    int(*session)(void*); /* C-only, also safe on transport worker */
    int(*before_charge)(void*,unsigned); /* UI owner only; exact1 readiness */
} LidTowerQueueOps;
typedef struct LidTowerQueue {
    unsigned initialized,guard,ready,halted,pending;uint32_t owner,worker;
    uint64_t next;R15Context context;uint8_t catalogue[32],allowed[23],bought[23];
    LidTowerQueueOps ops;LidTowerQueueSlot slots[64];
} LidTowerQueue;
static int lid_tq_lock(LidTowerQueue*q,uint32_t*tid){
    unsigned empty=0;
    return q&&q->initialized&&tid&&(*tid=q->ops.thread(q->ops.context))&&
        __atomic_compare_exchange_n(&q->guard,&empty,*tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void lid_tq_unlock(LidTowerQueue*q){__atomic_store_n(&q->guard,0,__ATOMIC_RELEASE);}
static void lid_tq_halt(LidTowerQueue*q){if(q)__atomic_store_n(&q->halted,1,__ATOMIC_RELEASE);}
static int lid_tq_prepare(LidTowerQueue*q,const LidShopRuntimeCatalogue*c,const LidTowerQueueOps*ops){
    if(!q||q->initialized||!c||!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||
       !r15_context_valid(&c->context)||!r15_nonzero(c->tower_hash,32)||!c->tower_count||c->tower_count>184||
       !c->ui||!c->worker||c->ui==c->worker||!ops||!ops->thread||!ops->session||!ops->before_charge||
       ops->thread(ops->context)!=c->ui||ops->session(ops->context)!=1)return 0;
    uint8_t allowed[23]={0};
    for(unsigned i=0;i<c->tower_count;i++){
        unsigned offer=c->tower[i].offer;
        if(offer>=184||(allowed[offer/8]&(1u<<(offer%8))))return 0;
        const LidTowerStockData*d=&lid_tower_stock_data[offer];const LidShopRuntimeTower*s=&c->tower[i];
        if(s->location!=d->location||s->floor!=d->floor||s->floor>c->goal||s->shopid!=d->shopid||
           s->plan_slot!=d->plan_slot||s->kind!=(d->plan_slot?2u:1u)||!lid_ts_room(s->room_key,d->room))return 0;
        allowed[offer/8]|=(uint8_t)(1u<<(offer%8));
    }
    q->context=c->context;r15_copy(q->catalogue,c->tower_hash,32);r15_copy(q->allowed,allowed,23);
    q->owner=c->ui;q->worker=c->worker;q->ops=*ops;q->initialized=1;return 1;
}
/* Only the real transport worker may authorize a same-context monotonic
   bought snapshot. No UObject read, wait, file access or engine call here. */
static int lid_tq_client_state(LidTowerQueue*q,const R15Context*context,const uint8_t digest[32],const uint8_t bought[23]){
    uint32_t tid;int ok=0;if(!context||!digest||!bought||!lid_tq_lock(q,&tid))return 0;
    if(tid!=q->worker||q->ops.session(q->ops.context)!=1)goto done;
    if(!r15_same_context(context,&q->context)||!r15_equal(digest,q->catalogue,32))goto done;
    for(unsigned i=0;i<23;i++)if((bought[i]&~q->allowed[i])||(q->bought[i]&~bought[i])){lid_tq_halt(q);goto done;}
    r15_copy(q->bought,bought,23);__atomic_store_n(&q->ready,1,__ATOMIC_RELEASE);ok=1;
done:lid_tq_unlock(q);return ok;
}
static int lid_tq_observation(const LidTowerUiSnapshot*s){
    if(!s||s->offer>=184||s->carrier!=&lid_tower_stock_data[s->offer]||!s->menu||!s->item_select||
       s->price<=0||s->price!=(int32_t)s->carrier->kc||s->location!=s->carrier->location||
       s->original>=36||s->logical>=36||s->products.count<=0||s->products.count>36||
       s->indices.count!=s->products.count||s->logical>=(unsigned)s->products.count)return 0;
    return 1;
}
static int lid_tq_reserve(LidTowerQueue*q,const LidTowerUiSnapshot*s,uint64_t*out){
    uint32_t tid;int ok=0;
    if(!q||!q->initialized||!out){r299_note(14,20,0);return 0;}
    if(!lid_tq_observation(s)){r299_note(14,21,0);return 0;}
    if(s->flag){r299_note(14,22,s->flag);return 0;}
    if(q->ops.thread(q->ops.context)!=q->owner){r299_note(14,23,0);return 0;}
    if(q->ops.before_charge(q->ops.context,s->offer)!=1){r299_note(14,24,s->offer);return 0;}
    if(!lid_tq_lock(q,&tid)){r299_note(14,25,0);return 0;}
    if(tid!=q->owner||!q->ready||__atomic_load_n(&q->halted,__ATOMIC_ACQUIRE)||q->pending||
       q->ops.session(q->ops.context)!=1||q->next==UINT64_MAX||
       !(q->allowed[s->offer/8]&(1u<<(s->offer%8)))||(q->bought[s->offer/8]&(1u<<(s->offer%8)))){
        unsigned detail=(tid!=q->owner?1u:0u)|(!q->ready?2u:0u)|
            (__atomic_load_n(&q->halted,__ATOMIC_ACQUIRE)?4u:0u)|(q->pending?8u:0u)|
            (q->next==UINT64_MAX?16u:0u)|(!(q->allowed[s->offer/8]&(1u<<(s->offer%8)))?32u:0u)|
            ((q->bought[s->offer/8]&(1u<<(s->offer%8)))?64u:0u);
        r299_note(14,26,detail);goto done;
    }
    unsigned free=64;
    for(unsigned i=0;i<64;i++){
        unsigned phase=__atomic_load_n(&q->slots[i].phase,__ATOMIC_ACQUIRE);
        if(phase!=LID_TQ_EMPTY&&q->slots[i].observation.offer==s->offer){r299_note(14,27,i);goto done;}
        if(phase==LID_TQ_EMPTY&&free==64)free=i;
    }
    if(free==64){r299_note(14,28,0);goto done;}
    LidTowerQueueSlot*slot=&q->slots[free];slot->observation=*s;slot->token=++q->next;
    __atomic_store_n(&slot->phase,LID_TQ_RESERVED,__ATOMIC_RELEASE);q->pending=free+1;*out=slot->token;ok=1;r299_note(14,29,s->offer);
done:lid_tq_unlock(q);return ok;
}
/* Compare saved identities/values only. Never dereference the old menu/arrays.
   The post-history collector freshly resolved them on the same owner thread. */
static int lid_tq_same_action(const LidTowerUiSnapshot*a,const LidTowerUiSnapshot*b){
    return a->menu==b->menu&&a->item_select==b->item_select&&a->offer==b->offer&&
        a->logical==b->logical&&a->original==b->original&&a->price==b->price&&a->location==b->location&&
        a->products.data==b->products.data&&a->products.count==b->products.count&&a->products.capacity==b->products.capacity&&
        a->indices.data==b->indices.data&&a->indices.count==b->indices.count&&a->indices.capacity==b->indices.capacity&&
        a->history_count<4096&&b->history_count==a->history_count+1&&!a->flag&&b->flag==1;
}
static int lid_tq_confirm(LidTowerQueue*q,const LidTowerUiSnapshot*s,uint64_t*out){
    uint32_t tid;int ok=0;if(!out||!lid_tq_observation(s)||!lid_tq_lock(q,&tid))return 0;
    if(tid!=q->owner||!q->pending||q->pending>64)goto done;
    LidTowerQueueSlot*slot=&q->slots[q->pending-1];
    if(__atomic_load_n(&slot->phase,__ATOMIC_ACQUIRE)==LID_TQ_RESERVED&&
       !__atomic_load_n(&q->halted,__ATOMIC_ACQUIRE)&&q->ops.session(q->ops.context)==1&&
       lid_tq_same_action(&slot->observation,s)){
        /* Backend gets only copied immutable carrier values, not deferred UI
           addresses. Confirmation does not release the slot for re-purchase. */
        slot->observation.menu=slot->observation.item_select=0;
        slot->observation.products=(LidTSArray){0};slot->observation.indices=(LidTSArray){0};
        __atomic_store_n(&slot->phase,LID_TQ_CONFIRMED,__ATOMIC_RELEASE);q->pending=0;*out=slot->token;ok=1;
    }else{__atomic_store_n(&slot->phase,LID_TQ_UNCERTAIN,__ATOMIC_RELEASE);lid_tq_halt(q);}
done:lid_tq_unlock(q);return ok;
}

typedef struct LidTowerUiExecOps {
    void*context;uint32_t(*thread)(void*);uint32_t(*error)(void*);void(*error_set)(void*,uint32_t);
    int(*quiet)(void*); /* actual independent engine guard held */
    int(*resident)(void*,uintptr_t);int(*protect)(void*,uintptr_t,unsigned,unsigned*);
    int(*coherent)(void*); /* real backend + display capability, never a stub */
} LidTowerUiExecOps;
typedef struct LidTowerUiExec {
    unsigned prepared,published,active,busy,stopped,protection_dirty;uint32_t owner;
    LidTowerUiCurrent*current;LidTowerQueue*queue;LidTowerUiExecOps ops;
    M207Slot ownership[2];uintptr_t wrappers[2];unsigned protection[2];
} LidTowerUiExec;
static LidTowerUiExec*g_lid_tower_ui_exec;
static void lid_tue_call(LidTowerUiExec*h,unsigned kind,void*receiver,void*frame,void*result){
    if(!h||!h->prepared||kind>1){r299_note(5,1,0);return;} /* unreachable for a published slot */
    const LidTowerUiMeta*m=h->current->meta;
    ((M204Exec)(uintptr_t)m->native[kind].original)(receiver,frame,result); /* exactly once, even inactive */
    uint32_t error=h->ops.error(h->ops.context);uint64_t node,owned;
    if(!__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)||h->ops.thread(h->ops.context)!=h->owner||
       !m195_read(&m->memory,(uint64_t)(uintptr_t)frame,0x14,&node,8)||node!=(kind?m->buy:m->tick)){
        h->ops.error_set(h->ops.context,error);{r299_note(5,2,0);return;}
    }
    volatile unsigned completed=0;unsigned entered=0;
    __try{
        LidTowerUiSnapshot s={0};uint64_t token=0;
        int context=lid_tuc_collect(h->current,kind,(uint64_t)(uintptr_t)receiver,(uint64_t)(uintptr_t)frame,&s);
        r299_note(5,40+(unsigned)(context+1)*2+kind,0);
        if(context){
            /* Proven malformed buy is refused. No general HUD/count override.
               Native cancellation is honored and never creates a reservation. */
            uint32_t value=*(uint32_t*)result;
            if(kind==LID_TUC_CANCEL&&value){completed=1;}
            else{
                int ok=context==1&&!h->stopped&&!h->busy&&
                    m195_read(&m->memory,m->native[kind].slot,0,&owned,8)&&owned==h->wrappers[kind];
                if(ok){h->busy=1;entered=1;}
                if(kind==LID_TUC_CANCEL){
                    if(!ok||!r299_flag(5,20,lid_tq_reserve(h->queue,&s,&token)))*(uint32_t*)result=1;
                }else{
                    if(!ok||value!=1||!r299_flag(5,22,lid_tq_confirm(h->queue,&s,&token))){
                        h->stopped=1;lid_tq_halt(h->queue);
                    }
                    /* Scope is only this exact AP tower buy, after history;
                       zero avoids BOTH phantom count and pending-ID appends. */
                    *(uint32_t*)result=0;
                }
                completed=1;
            }
        }else completed=1;
    }__finally{
        if(!completed){h->stopped=1;lid_tq_halt(h->queue);}
        if(entered)h->busy=0;h->ops.error_set(h->ops.context,error);
    }
}
static __attribute__((noinline)) void lid_tue_cancel(void*r,void*f,void*v){
    lid_tue_call(__atomic_load_n(&g_lid_tower_ui_exec,__ATOMIC_ACQUIRE),0,r,f,v);
}
static __attribute__((noinline)) void lid_tue_count(void*r,void*f,void*v){
    lid_tue_call(__atomic_load_n(&g_lid_tower_ui_exec,__ATOMIC_ACQUIRE),1,r,f,v);
}
static int lid_tue_prepare(LidTowerUiExec*h,LidTowerUiCurrent*c,LidTowerQueue*q,const LidTowerUiExecOps*ops){
    if(!h||h->prepared||!c||!c->prepared||!q||!q->initialized||!ops||!ops->thread||!ops->error||!ops->error_set||
       !ops->quiet||!ops->resident||!ops->protect||!ops->coherent||ops->thread(ops->context)!=q->owner||
       q->owner!=c->catalogue->ui||!r15_same_context(&q->context,&c->catalogue->context)||
       !r15_equal(q->catalogue,c->catalogue->tower_hash,32))return 0;
    h->current=c;h->queue=q;h->ops=*ops;h->owner=q->owner;
    h->wrappers[0]=(uintptr_t)lid_tue_cancel;h->wrappers[1]=(uintptr_t)lid_tue_count;h->prepared=1;return 1;
}
/* Two UFunction slots, never a detour of linker-pooled native C code. Caller
   owns the real guard. Original forwarding is available BEFORE the first CAS.
   Do not activate until backend/no-item payment/receipts/display are coherent. */
static int lid_tue_publish(LidTowerUiExec*h){
    if(!h||!h->prepared||h->published||h->stopped||h->protection_dirty||
       h->ops.thread(h->ops.context)!=h->owner||h->ops.quiet(h->ops.context)!=1)return 0;
    for(unsigned i=0;i<2;i++)if(!h->ops.resident(h->ops.context,h->wrappers[i]))return 0;
    LidTowerUiExec*empty=0;
    if(!__atomic_compare_exchange_n(&g_lid_tower_ui_exec,&empty,h,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    volatile unsigned completed=0;int ok=0;
    __try{
        for(unsigned i=0;i<2;i++){
            uint64_t slot=h->current->meta->native[i].slot;unsigned ignored;
            if(!h->ops.protect(h->ops.context,(uintptr_t)slot,4,&h->protection[i]))goto end;
            h->protection_dirty|=1u<<i;
            int installed=m207_publish(&h->ownership[i],(uintptr_t*)(uintptr_t)slot,
                (uintptr_t)h->current->meta->native[i].original,h->wrappers[i]);
            if(!h->ops.protect(h->ops.context,(uintptr_t)slot,h->protection[i],&ignored))goto end;
            h->protection_dirty&=~(1u<<i);if(!installed)goto end;
        }
        h->published=1;ok=1;
end:completed=1;
    }__finally{
        /* No exception-swallow, unowned overwrite or resident pointer release.
           Failure leaves inactive forwarding wrappers; no partial activation. */
        if(!completed||!ok)h->stopped=1;
    }
    return ok;
}
static int lid_tue_enable(LidTowerUiExec*h){
    if(!h||!h->published||h->stopped||h->protection_dirty||h->ops.thread(h->ops.context)!=h->owner||
       h->ops.quiet(h->ops.context)!=1||h->ops.coherent(h->ops.context)!=1||
       !__atomic_load_n(&h->queue->ready,__ATOMIC_ACQUIRE)||__atomic_load_n(&h->queue->halted,__ATOMIC_ACQUIRE))return 0;
    for(unsigned i=0;i<2;i++){
        uint64_t value;
        if(__atomic_load_n(&h->ownership[i].state,__ATOMIC_ACQUIRE)!=M207_INSTALLED||
           !m195_read(&h->current->meta->memory,h->current->meta->native[i].slot,0,&value,8)||value!=h->wrappers[i])return 0;
    }
    __atomic_store_n(&h->active,1,__ATOMIC_RELEASE);return 1;
}
#endif
