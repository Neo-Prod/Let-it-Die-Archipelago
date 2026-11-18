#ifndef LID_SHOP_PURCHASE_TRANSPORT_H
#define LID_SHOP_PURCHASE_TRANSPORT_H
/* Included after actual R26Transport definition. UI only publishes private C
   bridge; the EXISTING worker alone owns purchase files/lease. No new thread.
   The provider's live callback must be immutable C Context/lease validation,
   never a UI UObject/session read on this worker or a native payment worker. */
enum {R254_IO_WAIT=0,R254_IO_ACTIVE=1,R254_IO_FAILED=2,R254_IO_CLOSED=3};
static int r254_shop_bind(R26Transport*t,void*provider,uint32_t(*thread)(void*),int(*live)(void*)){
    if(!t||!thread||thread(provider)!=t->owner||
       __atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||
       __atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&t->shop_bridge_published,__ATOMIC_ACQUIRE)||
       lid_sm_mode(&t->shop_policy)!=LID_SM_ENABLED)return 0;
    const LidShopRuntimeCatalogue*c=&t->shop_catalogue.catalogue;
    if(!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||
       !r15_same_context(&c->context,&t->queue.context)||c->goal!=t->shop_policy.goal||
       !r15_equal(c->full,t->shop_policy.full,32)||
       !lid_sc_prepare_bridge(c,&t->shop_bridge,t->owner,provider,thread,live))return 0;
    __atomic_store_n(&t->shop_bridge_published,1,__ATOMIC_RELEASE);return 1;
}
/* Owner-only, no IO, no naked cross-thread ready flag reads. 0 waits, -1
   terminal admission failure, 1 current bought state has actually been read. */
static int r254_shop_ready(R26Transport*t){
    uint32_t tid;int ready=0;
    if(!t||!__atomic_load_n(&t->shop_bridge_published,__ATOMIC_ACQUIRE)||
       t->disk.ops.thread_id()!=t->owner)return 0;
    unsigned state=__atomic_load_n(&t->shop_io_state,__ATOMIC_ACQUIRE);
    if(state==R254_IO_FAILED||state==R254_IO_CLOSED)return -1;
    if(state!=R254_IO_ACTIVE)return 0; /* no UI lock contention during IO bind */
    if(!lid_pb_lock(&t->shop_bridge,&tid))return 0;
    LidShopPurchaseBridge*b=&t->shop_bridge;
    if(tid==b->ui)ready=(b->blocked||__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE))?-1:(b->ready&&b->io?1:0);
    lid_pb_unlock(b);return ready;
}
static void r254_shop_io_step(R26Transport*t,uint64_t now){
    if(!t||!__atomic_load_n(&t->shop_bridge_published,__ATOMIC_ACQUIRE))return;
    LidShopPurchaseBridge*b=&t->shop_bridge;
    if(__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE))__atomic_store_n(&b->halted,1,__ATOMIC_RELEASE);
    unsigned state=__atomic_load_n(&t->shop_io_state,__ATOMIC_ACQUIRE);
    if(state==R254_IO_FAILED||state==R254_IO_CLOSED)return;
    if(state==R254_IO_WAIT){
        if(!lid_pd_start(&t->shop_disk,b,&t->disk.ops,t->shop_directory)){
            __atomic_store_n(&b->halted,1,__ATOMIC_RELEASE);
            __atomic_store_n(&t->shop_io_state,R254_IO_FAILED,__ATOMIC_RELEASE);return;
        }
        __atomic_store_n(&t->shop_io_state,R254_IO_ACTIVE,__ATOMIC_RELEASE);
    }
    if(t->shop_next_poll&&now<t->shop_next_poll)return;
    t->shop_next_poll=now>UINT64_MAX-500?UINT64_MAX:now+500;
    /* 0 can be a busy metadata claim or transient write failure; preserve and
       retry the same owned receipt, never manufacture another payment. */
    (void)lid_pd_pump(&t->shop_disk);
}
static int r254_shop_io_drained(R26Transport*t){
    if(!t||!__atomic_load_n(&t->shop_bridge_published,__ATOMIC_ACQUIRE))return 1;
    unsigned state=__atomic_load_n(&t->shop_io_state,__ATOMIC_ACQUIRE);
    if(state==R254_IO_CLOSED)return 1;
    if(state!=R254_IO_ACTIVE)return 0;
    /* A reserved/paying/uncertain/unacknowledged receipt keeps lease/storage
       resident through the existing R26 timeout; never silently discard it. */
    if(t->shop_disk.awaiting_mark||!lid_pb_retire_empty(&t->shop_bridge))return 0;
    if(!t->shop_disk.ops.close(t->shop_disk.lease)){
        __atomic_store_n(&t->shop_io_state,R254_IO_FAILED,__ATOMIC_RELEASE);return 0;
    }
    t->shop_disk.lease=0;
    __atomic_store_n(&t->shop_io_state,R254_IO_CLOSED,__ATOMIC_RELEASE);return 1;
}
#endif
