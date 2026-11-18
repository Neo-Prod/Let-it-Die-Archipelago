#ifndef LID_SHOP_TOWER_PAYMENT_H
#define LID_SHOP_TOWER_PAYMENT_H
#include "shop_tower_purchase_ui.h"
/* Tower-specific carried/free-reserve/paid-reserve evidence. No receipt wire,
   fake entity ID, native repository creation/attachment, refund or AP send.
   No default request admission: complete existing-handler host is mandatory.
   Prepare final-address zeroed resident storage once; do not rebind a run. */
typedef struct LidTowerMoney {int32_t carried,reserve,paid_reserve;} LidTowerMoney;
typedef struct LidTowerPaymentReceipt {
    uint64_t token,location;unsigned offer,price;LidTowerMoney before,after;
    unsigned after_valid,debit_result,handler_result;
} LidTowerPaymentReceipt;
typedef struct LidTowerPaymentNative {
    unsigned prepared;uintptr_t image,account;
    uintptr_t(*fighter)(int,unsigned char);unsigned char(*valid)(uintptr_t);
    uintptr_t(*empty_slot)(uintptr_t);
    unsigned char(*spend)(uintptr_t,int,const wchar_t*,const wchar_t*);
} LidTowerPaymentNative;
typedef struct LidTowerPaymentOps {
    void*context;
    /* Must prove actual owned1167730 request/callsite, exact current floor/shop
       master membership/native item/current native KC price and zero rank/SP
       coefficients, adopted token/owner and unchanged run.
       NOT just readable memory, a thread ID or the UI history reservation. */
    int(*window)(void*,uint64_t,unsigned,int);
    int(*copy)(void*,uintptr_t,void*,size_t);
} LidTowerPaymentOps;
typedef struct LidTowerPaymentRecord {uint32_t payer;LidTowerPaymentReceipt receipt;} LidTowerPaymentRecord;
typedef struct LidTowerPayment {
    unsigned prepared;LidTowerQueue*queue;LidTowerPaymentNative native;LidTowerPaymentOps ops;
    LidTowerPaymentRecord records[64];
} LidTowerPayment;
static int lid_tp_money_valid(const LidTowerMoney*m){return m&&m->carried>=0&&m->reserve>=0&&m->paid_reserve>=0;}
static int lid_tp_debit_equation(const LidTowerPaymentReceipt*r){
    if(!r||!r->token||r->offer>=184||!r->price||r->price>INT32_MAX||
       r->price!=lid_tower_stock_data[r->offer].kc||r->location!=lid_tower_stock_data[r->offer].location||
       !lid_tp_money_valid(&r->before)||!lid_tp_money_valid(&r->after)||
       !r->after_valid||r->debit_result!=1)return 0;
    int64_t carried=r->before.carried,price=r->price,used=carried<price?carried:price;
    return (int64_t)r->before.reserve>=price-used&&r->after.carried==carried-used&&
        r->after.reserve==(int64_t)r->before.reserve-(price-used)&&r->after.paid_reserve==r->before.paid_reserve;
}
static int lid_tp_equation(const LidTowerPaymentReceipt*r){return r&&r->handler_result==1&&lid_tp_debit_equation(r);}
static int lid_tp_native_prepare(LidTowerPaymentNative*out,const M195Ops*memory,uintptr_t image,int exact_sha){
    if(!out||out->prepared||exact_sha!=1||!image||image>UINTPTR_MAX-0xf8b50a0)return 0;
    static const uint32_t rvas[4]={0x1377420,0x1384f30,0x1377b60,0x1350b20};
    static const uint8_t profiles[4][24]={
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x0f,0xb6,0xfa,0xe8,0xee,0xb5,0xd8,0xff,0x4c,0x8b,0x00,0x48,0x8b,0xc8},
        {0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x8b,0x41,0x44,0x8b},
        {0x40,0x53,0x48,0x83,0xec,0x20,0x8b,0x41,0x14,0x48,0x8b,0xd9,0x85,0xc0,0x74,0x28,0x7f,0x20,0x4c,0x8d,0x0d,0x7f,0xf5,0x82},
        {0x48,0x89,0x5c,0x24,0x20,0x57,0x41,0x54,0x41,0x57,0x48,0x83,0xec,0x30,0x4d,0x8b,0xe1,0x4d,0x8b,0xf8,0x8b,0xfa,0x48,0x8b}};
    uint8_t actual[24];
    for(unsigned i=0;i<4;i++){
        if(!m195_read(memory,image,rvas[i],actual,24))return 0;
        for(unsigned j=0;j<24;j++)if(actual[j]!=profiles[i][j])return 0;
    }
    out->image=image;out->account=image+0xf8b5088;
    out->fighter=(__typeof__(out->fighter))(image+rvas[0]);out->valid=(__typeof__(out->valid))(image+rvas[1]);
    out->empty_slot=(__typeof__(out->empty_slot))(image+rvas[2]);out->spend=(__typeof__(out->spend))(image+rvas[3]);
    out->prepared=1;return 1;
}
static int lid_tp_prepare(LidTowerPayment*out,LidTowerQueue*q,const LidTowerPaymentNative*n,const LidTowerPaymentOps*ops){
    if(!out||out->prepared||!q||!q->initialized||q->ops.thread(q->ops.context)!=q->owner||
       !n||!n->prepared||!n->fighter||!n->valid||!n->empty_slot||!n->spend||!ops||!ops->window||!ops->copy)return 0;
    out->queue=q;out->native=*n;out->ops=*ops;out->prepared=1;return 1;
}
static int lid_tp_item(const uint16_t*item,unsigned units,const wchar_t*expected){
    if(!item||!expected||!units||units>=96||item[units])return 0;
    for(unsigned i=0;i<units;i++)if(item[i]<0x21||item[i]>0x7e||item[i]!=(uint16_t)expected[i])return 0;
    return !expected[units];
}
/* Called only after owned history decoder resolved room/product -> offer.
   No native call or wait under this metadata guard. Payload copied on claim. */
static int lid_tp_claim(LidTowerPayment*p,unsigned offer,const uint16_t*item,unsigned units,uint64_t*out){
    if(!p||!p->prepared||!out||offer>=184||!lid_tp_item(item,units,lid_tower_stock_data[offer].item))return 0;
    LidTowerQueue*q=p->queue;uint32_t tid;unsigned match=64;int ok=0;
    if(!lid_tq_lock(q,&tid))return 0;
    if(tid==q->owner||tid==q->worker||q->ops.session(q->ops.context)!=1||
       __atomic_load_n(&q->halted,__ATOMIC_ACQUIRE)||!q->ready)goto done;
    for(unsigned i=0;i<64;i++)if(__atomic_load_n(&q->slots[i].phase,__ATOMIC_ACQUIRE)==LID_TQ_CONFIRMED&&q->slots[i].observation.offer==offer){
        if(match!=64){lid_tq_halt(q);goto done;}match=i;
    }
    if(match==64||(q->bought[offer/8]&(1u<<(offer%8))))goto done;
    LidTowerQueueSlot*s=&q->slots[match];LidTowerPaymentRecord*r=&p->records[match];
    r->receipt=(LidTowerPaymentReceipt){s->token,s->observation.location,offer,(unsigned)s->observation.price,{0},{0},0,0,0};
    __atomic_store_n(&r->payer,tid,__ATOMIC_RELEASE);
    __atomic_store_n(&s->phase,LID_TQ_PAYING,__ATOMIC_RELEASE);*out=s->token;ok=1;
done:lid_tq_unlock(q);return ok;
}
static int lid_tp_owned(LidTowerPayment*p,uint64_t token,unsigned phase,unsigned*out){
    if(!p||!p->prepared||!token||!out)return 0;LidTowerQueue*q=p->queue;uint32_t tid=q->ops.thread(q->ops.context);
    if(!tid||tid==q->owner||tid==q->worker)return 0;
    for(unsigned i=0;i<64;i++)if(__atomic_load_n(&q->slots[i].phase,__ATOMIC_ACQUIRE)==phase&&
       q->slots[i].token==token&&__atomic_load_n(&p->records[i].payer,__ATOMIC_ACQUIRE)==tid){*out=i;return 1;}
    return 0;
}
static void lid_tp_fail(LidTowerPayment*p,uint64_t token){
    unsigned i;if(!p||!p->prepared)return;lid_tq_halt(p->queue);
    if(lid_tp_owned(p,token,LID_TQ_PAYING,&i)||lid_tp_owned(p,token,LID_TQ_PAID,&i))
        __atomic_store_n(&p->queue->slots[i].phase,LID_TQ_UNCERTAIN,__ATOMIC_RELEASE);
}
static int lid_tp_copy(LidTowerPayment*p,uintptr_t at,void*out,size_t size){
    return at&&size&&at<=UINTPTR_MAX-size&&p->ops.copy(p->ops.context,at,out,size)==1;
}
static int lid_tp_balance(LidTowerPayment*p,uintptr_t fighter,LidTowerMoney*out){
    return fighter&&fighter<=UINTPTR_MAX-0x58&&
        lid_tp_copy(p,fighter+0x54,&out->carried,4)&&
        lid_tp_copy(p,p->native.account+0xc,&out->reserve,4)&&
        lid_tp_copy(p,p->native.account+0x10,&out->paid_reserve,4)&&lid_tp_money_valid(out);
}
/* Native no-item payment core. Caller still MUST prove original master sale
   membership/current floor/request and owned helper callsite via window.
   Never invoke on arbitrary KC pickup/hub item delivery. No native pointer is
   retained across this invocation; no repository item is created or attached. */
static int lid_tp_pay(LidTowerPayment*p,uint64_t token,int uid){
    unsigned i;if(!lid_tp_owned(p,token,LID_TQ_PAYING,&i)){r299_note(8,1,0);return 0;}
    LidTowerQueue*q=p->queue;LidTowerPaymentReceipt*r=&p->records[i].receipt;
    volatile unsigned completed=0;int ok=0;uintptr_t fighter=0;int actual_uid;
    __try{
        if(__atomic_load_n(&q->halted,__ATOMIC_ACQUIRE)||q->ops.session(q->ops.context)!=1||
           p->ops.window(p->ops.context,token,r->offer,uid)!=1||
           !lid_tp_copy(p,p->native.account+8,&actual_uid,4)||actual_uid!=uid)goto end;
        fighter=p->native.fighter(uid,0);
        if(!fighter||!p->native.valid(fighter)||!p->native.empty_slot(fighter)||
           !lid_tp_balance(p,fighter,&r->before)||
           (int64_t)r->before.carried+r->before.reserve<r->price||
           p->ops.window(p->ops.context,token,r->offer,uid)!=1)goto end;
        /* Exactly one call to ORIGINAL native spender. It handles carried KC
           first then free reserve, native notifications and mutation hooks.
           No Soul/raw-money writes, retry, refund or local-item side effect. */
        r->debit_result=p->native.spend(p->native.account,(int)r->price,L"floor_shop_use_money",lid_tower_stock_data[r->offer].item);
        if(r->debit_result!=1||p->native.fighter(uid,0)!=fighter||
           p->ops.window(p->ops.context,token,r->offer,uid)!=1||!lid_tp_balance(p,fighter,&r->after))goto end;
        r->after_valid=1;
        /* handler_result remains0 until actual outer handler returns. */
        if(!lid_tp_debit_equation(r))goto end;
        __atomic_store_n(&q->slots[i].phase,LID_TQ_PAID,__ATOMIC_RELEASE);ok=1;
end:completed=1;
    }__finally{
        if(!completed||!ok)lid_tp_fail(p,token);
    }
    {r299_note(8,2,0);return ok;}
}
/* ONLY after actual owning original1167730 handler has returned successfully.
   This READY release is native evidence in RAM, NOT durable journal/AP send. */
static int lid_tp_finish(LidTowerPayment*p,uint64_t token,unsigned handler_result){
    unsigned i;if(!lid_tp_owned(p,token,LID_TQ_PAID,&i)){lid_tp_fail(p,token);{r299_note(11,1,0);return 0;}}
    LidTowerPaymentReceipt*r=&p->records[i].receipt;
    if(handler_result!=1||p->queue->ops.session(p->queue->ops.context)!=1){lid_tp_fail(p,token);{r299_note(11,2,0);return 0;}}
    r->handler_result=handler_result;
    if(!lid_tp_equation(r)){lid_tp_fail(p,token);{r299_note(11,3,0);return 0;}}
    __atomic_store_n(&p->queue->slots[i].phase,LID_TQ_READY,__ATOMIC_RELEASE);{r299_note(11,4,0);return 1;}
}
#endif
