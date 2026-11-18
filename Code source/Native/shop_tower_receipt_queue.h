#ifndef LID_SHOP_TOWER_RECEIPT_QUEUE_H
#define LID_SHOP_TOWER_RECEIPT_QUEUE_H
#include "shop_tower_payment.h"
#include "shop_tower_receipt_port.h"
/* Only the existing IO worker calls these C callbacks. READY release from the
   actual payer makes its immutable receipt readable; ACK frees it under guard.
   Admission halt must never prevent draining an already proven paid receipt. */
static int lid_trq_state(void*host,const uint8_t*bought){
    LidTowerPayment*p=host;return p&&p->prepared&&
        lid_tq_client_state(p->queue,&p->queue->context,p->queue->catalogue,bought);
}
static int lid_trq_lock(LidTowerPayment*p,uint32_t*tid){
    if(!p||!p->prepared||!lid_tq_lock(p->queue,tid))return 0;
    if(*tid!=p->queue->worker||p->queue->ops.session(p->queue->ops.context)!=1){lid_tq_unlock(p->queue);return 0;}
    return 1;
}
static int lid_trq_peek(void*host,uint8_t*raw,uint64_t*token){
    LidTowerPayment*p=host;uint32_t tid;unsigned first=64;int ok=0;
    if(!raw||!token||!lid_trq_lock(p,&tid))return 0;
    LidTowerQueue*q=p->queue;
    for(unsigned i=0;i<64;i++){
        unsigned phase=__atomic_load_n(&q->slots[i].phase,__ATOMIC_ACQUIRE);
        if(phase==LID_TQ_PUBLISHED)goto done; /* one outstanding event */
        if(phase==LID_TQ_READY&&(first==64||q->slots[i].token<q->slots[first].token))first=i;
    }
    if(first<64){
        LidTowerQueueSlot*s=&q->slots[first];const LidTowerPaymentReceipt*r=&p->records[first].receipt;
        if(r->token!=s->token||r->offer!=s->observation.offer||r->location!=s->observation.location||
           r->price!=(unsigned)s->observation.price||!lid_tp_equation(r))goto done;
        const LidTowerStockData*d=&lid_tower_stock_data[r->offer];LidTowerPurchaseFrame f={0};
        f.context=q->context;r15_copy(f.catalogue,q->catalogue,32);f.kind=LID_TPW_PAYMENT;
        f.sequence=r->token;f.offer=r->offer;f.location=r->location;f.price=r->price;
        f.before_carried=r->before.carried;f.after_carried=r->after.carried;
        f.before_free=r->before.reserve;f.after_free=r->after.reserve;
        f.before_paid=r->before.paid_reserve;f.after_paid=r->after.paid_reserve;
        f.debit=r->debit_result;f.handler=r->handler_result;
        while(f.item_units<95&&d->item[f.item_units]){unsigned n=f.item_units++;f.item[n]=(uint16_t)d->item[n];}
        while(f.product_units<31&&d->product[f.product_units]){unsigned n=f.product_units++;f.product[n]=(uint16_t)d->product[n];}
        if(lid_tpw_encode(raw,LID_TPW_SIZE,&f)){*token=r->token;ok=1;}
    }
done:lid_tq_unlock(q);return ok;
}
static int lid_trq_published(void*host,uint64_t token){
    LidTowerPayment*p=host;uint32_t tid;int ok=0;if(!token||!lid_trq_lock(p,&tid))return 0;
    LidTowerQueue*q=p->queue;
    for(unsigned i=0;i<64;i++)if(q->slots[i].token==token&&
        __atomic_load_n(&q->slots[i].phase,__ATOMIC_ACQUIRE)==LID_TQ_READY){
        __atomic_store_n(&q->slots[i].phase,LID_TQ_PUBLISHED,__ATOMIC_RELEASE);ok=1;break;
    }
    lid_tq_unlock(q);return ok;
}
static int lid_trq_ack(void*host,const LidTowerPurchaseFrame*f){
    LidTowerPayment*p=host;uint32_t tid;int ok=0;
    if(!f||f->kind!=LID_TPW_ACK||!lid_tpw_valid(f)||!lid_trq_lock(p,&tid))return 0;
    LidTowerQueue*q=p->queue;
    if(!r15_same_context(&f->context,&q->context)||!r15_equal(f->catalogue,q->catalogue,32)||
       !(q->bought[f->offer/8]&(1u<<(f->offer%8))))goto done;
    for(unsigned i=0;i<64;i++){
        LidTowerQueueSlot*s=&q->slots[i];
        if(__atomic_load_n(&s->phase,__ATOMIC_ACQUIRE)==LID_TQ_PUBLISHED&&s->token==f->sequence&&
           s->observation.offer==f->offer&&s->observation.location==f->location){
            /* No UI/native borrower remains after actual handler completion. */
            r15_zero(&p->records[i],sizeof(p->records[i]));r15_zero(&s->observation,sizeof(s->observation));
            s->token=0;__atomic_store_n(&s->phase,LID_TQ_EMPTY,__ATOMIC_RELEASE);ok=1;break;
        }
    }
done:lid_tq_unlock(q);return ok;
}
static void lid_trq_halt(void*host){LidTowerPayment*p=host;if(p&&p->prepared)lid_tq_halt(p->queue);}
static int lid_trq_retire(void*host){
    LidTowerPayment*p=host;uint32_t tid;int ok=0;if(!lid_trq_lock(p,&tid))return 0;
    LidTowerQueue*q=p->queue;lid_tq_halt(q);
    if(q->pending)goto done;
    for(unsigned i=0;i<64;i++)if(__atomic_load_n(&q->slots[i].phase,__ATOMIC_ACQUIRE)!=LID_TQ_EMPTY)goto done;
    __atomic_store_n(&q->ready,0,__ATOMIC_RELEASE);ok=1;
done:lid_tq_unlock(q);return ok;
}
static int lid_trq_prepare(LidTowerReceiptPort*out,LidTowerPayment*p){
    if(!out||out->host||!p||!p->prepared||!p->queue||!p->queue->initialized||
       p->queue->ops.thread(p->queue->ops.context)!=p->queue->owner||
       p->queue->ops.session(p->queue->ops.context)!=1)return 0;
    out->context=p->queue->context;r15_copy(out->catalogue,p->queue->catalogue,32);
    out->ui=p->queue->owner;out->worker=p->queue->worker;out->host=p;
    out->state=lid_trq_state;out->peek=lid_trq_peek;out->published=lid_trq_published;
    out->ack=lid_trq_ack;out->retire=lid_trq_retire;out->halt=lid_trq_halt;return 1;
}
#endif
