#ifndef LID_SHOP_TOWER_RECEIPT_TRANSPORT_H
#define LID_SHOP_TOWER_RECEIPT_TRANSPORT_H
/* Included after R26Transport. Publish immutable provider once on UI owner;
   the SAME existing R26 worker performs every disk operation, also on drain. */
static int r276_tower_bind(R26Transport*t,LidTowerReceiptPort*p){
    if(!t||!p||!p->host||!p->state||!p->peek||!p->published||!p->ack||!p->retire||!p->halt||
       t->disk.ops.thread_id()!=t->owner||p->ui!=t->owner||
       __atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)||
       lid_sm_mode(&t->shop_policy)!=LID_SM_ENABLED)return 0;
    const LidShopRuntimeCatalogue*c=&t->shop_catalogue.catalogue;
    if(!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||p->worker!=c->worker||p->ui!=c->ui||
       !r15_same_context(&p->context,&t->queue.context)||!r15_same_context(&p->context,&c->context)||
       !r15_equal(p->catalogue,c->tower_hash,32))return 0;
    LidTowerReceiptPort*empty=0;
    return __atomic_compare_exchange_n(&t->tower_receipts,&empty,p,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static int r276_tower_ready(R26Transport*t){
    return t&&__atomic_load_n(&t->tower_receipts,__ATOMIC_ACQUIRE)&&
        __atomic_load_n(&t->tower_io_state,__ATOMIC_ACQUIRE)==R254_IO_ACTIVE;
    /* The UI queue separately requires actual monotonic client-state ready. */
}
static void r276_tower_io_step(R26Transport*t,uint64_t now){
    LidTowerReceiptPort*p=t?__atomic_load_n(&t->tower_receipts,__ATOMIC_ACQUIRE):0;if(!p)return;
    if(__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE))p->halt(p->host);
    unsigned state=__atomic_load_n(&t->tower_io_state,__ATOMIC_ACQUIRE);
    if(state==R254_IO_FAILED||state==R254_IO_CLOSED)return;
    if(state==R254_IO_WAIT){
        if(!lid_trd_start(&t->tower_disk,p,&t->disk.ops,t->shop_directory)){
            p->halt(p->host);__atomic_store_n(&t->tower_io_state,R254_IO_FAILED,__ATOMIC_RELEASE);return;
        }
        __atomic_store_n(&t->tower_io_state,R254_IO_ACTIVE,__ATOMIC_RELEASE);
    }
    if(t->tower_next_poll&&now<t->tower_next_poll)return;
    t->tower_next_poll=now>UINT64_MAX-500?UINT64_MAX:now+500;
    (void)lid_trd_pump(&t->tower_disk);
}
static int r276_tower_io_drained(R26Transport*t){
    if(!t||!__atomic_load_n(&t->tower_receipts,__ATOMIC_ACQUIRE))return 1;
    unsigned state=__atomic_load_n(&t->tower_io_state,__ATOMIC_ACQUIRE);
    if(state==R254_IO_CLOSED)return 1;
    if(state!=R254_IO_ACTIVE||!lid_trd_close(&t->tower_disk))return 0;
    __atomic_store_n(&t->tower_io_state,R254_IO_CLOSED,__ATOMIC_RELEASE);return 1;
}
#endif
