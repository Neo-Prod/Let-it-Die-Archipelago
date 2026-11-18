#ifndef LID_SHOP_MOMOKA_PAYMENT_H
#define LID_SHOP_MOMOKA_PAYMENT_H
#include <stdint.h>
#include <stddef.h>
/* Passive scope for KC purchases in exact-profile141160F50. No hooks, waits,
   native calls, local grant suppression, saves or network operations here.
   Native wrappers must acquire admission BEFORE a matched grant and retain it
   through the original debit and receipt publication. Never use this scope as
   evidence that worker admission, UI reservation or durability already exists.
   Prepare on the owned request worker with stable copied catalogue rows whose
   price/currencies have been validated against the current native skill data.
   Static masters alone do not prove those current runtime values. */
enum {LID_MP_NATIVE=0,LID_MP_MATCH=1,LID_MP_INVALID=-1};
enum {LID_MP_WAIT,LID_MP_GRANT,LID_MP_DEBIT,LID_MP_PAID,LID_MP_UNCERTAIN};
typedef struct LidMPMoney {int32_t free_money,paid_money;} LidMPMoney;
typedef struct LidMPRow {uint16_t skill[129];unsigned units,offer,price,spirit_price,recycle_price;} LidMPRow;
typedef struct LidMPReceipt {
    unsigned offer,price;LidMPMoney before,after;
    unsigned after_valid,native_result,paid;
} LidMPReceipt;
typedef struct LidMPOps {
    /* Resident context bound to THIS old run until all callbacks have drained;
       never redirect this pointer to a newly connected session's ledger. */
    void*context;
    uint64_t(*session)(void*);
    uint64_t(*thread)(void*);
    int(*worker)(void*);
    int(*admission)(void*); /* own purchase lease, no engine lock */
    int(*copy)(void*,uintptr_t,void*,size_t);
    /* Metadata-only publication under admission, no game calls/allocations.
       Must return1 only for a complete publication to this exact run ledger.
       An uncertain receipt may reserve/block, never authorize an AP send. */
    int(*receipt)(void*,const LidMPReceipt*);
    /* Infallible/idempotent writes to resident OLD-run C metadata only. Must
       set this offer UNCERTAIN and block every send before admission release.
       No game pointers, file IO, allocation or network calls, also on abort. */
    void(*uncertain)(void*,unsigned offer);
    /* Optional strict proof of OUR installed CALL override, never a generic
       signature bypass. Caller verifies actual CALL+relay ownership. */
    int(*owned_callsite)(void*,uintptr_t,const unsigned char*,unsigned);
} LidMPOps;
typedef struct LidMomokaPayment {
    uintptr_t base,account;uint64_t session,owner;LidMPOps ops;
    unsigned initialized,count,blocked;int active;
    LidMPRow rows[128];unsigned char phase[128];
    LidMPReceipt evidence[128];
} LidMomokaPayment;
static int lid_mp_read(const LidMPOps*o,uintptr_t p,void*out,size_t n){
    return o&&o->copy&&p&&out&&n&&p<=UINTPTR_MAX-n&&o->copy(o->context,p,out,n)==1;
}
static int lid_mp_owner(const LidMomokaPayment*s){
    return s&&s->initialized&&s->ops.thread&&s->ops.thread(s->ops.context)==s->owner;
}
static int lid_mp_valid(const LidMomokaPayment*s){
    return lid_mp_owner(s)&&!s->blocked&&s->ops.session(s->ops.context)==s->session&&
      s->ops.worker(s->ops.context)==1;
}
static int lid_mp_id(const uint16_t*p,unsigned n){
    if(!n||n>=129||p[n])return 0;for(unsigned i=0;i<n;i++)if(p[i]<0x21||p[i]>0x7e)return 0;return 1;
}
static int lid_mp_equal(const uint16_t*a,unsigned n,const uint16_t*b,unsigned m){
    if(n!=m)return 0;for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;
}
/* Concrete origin predicate for the current native worker invocation.
   api is BORROWED only while its handler is on this thread's call stack.
   Does not retain it, identify arbitrary OS threads or acquire admission. */
static int lid_momoka_worker_entry(const LidMPOps*o,uintptr_t base,int exact_sha,
                                  uintptr_t api,uintptr_t caller){
    static const unsigned char thunk[10]={0x48,0x8b,0x01,0x48,0xff,0xa0,0x80,0,0,0};
    unsigned char bytes[10];uintptr_t vt,slot;
    if(!base||base>UINTPTR_MAX-0x212e0e8||exact_sha!=1||caller!=base+0x10f4fde||
       !lid_mp_read(o,api,&vt,8)||vt!=base+0x2112db0||
       !lid_mp_read(o,vt+0x18,&slot,8)||slot!=base+0x110d570||
       !lid_mp_read(o,vt+0x80,&slot,8)||slot!=base+0x1156d30||
       !lid_mp_read(o,base+0x10f4fdb,bytes,3)||bytes[0]!=0xff||bytes[1]!=0x50||bytes[2]!=0x18||
       !lid_mp_read(o,base+0x110d570,bytes,10))return 0;
    for(unsigned i=0;i<10;i++)if(bytes[i]!=thunk[i])return 0;return 1;
}
static int lid_momoka_payment_prepare(LidMomokaPayment*out,uintptr_t base,int exact_sha,
                                     const LidMPOps*ops,const LidMPRow*rows,unsigned count){
    static const unsigned char grant[5]={0xe8,0xf8,0x3d,0x1f,0};
    static const unsigned char debit[5]={0xe8,0x95,0xba,0x1d,0};
    unsigned char bytes[5];uint64_t session,owner;
    if(!out||out->initialized||!base||base>UINTPTR_MAX-0xf8b509c||exact_sha!=1||!ops||
      !ops->session||!ops->thread||!ops->worker||!ops->admission||!ops->copy||!ops->receipt||!ops->uncertain||
      !rows||!count||count>128||ops->worker(ops->context)!=1||
      !(session=ops->session(ops->context))||!(owner=ops->thread(ops->context)))return 0;
    if(!lid_mp_read(ops,base+0x1157413,bytes,5))return 0;
    {unsigned same=1;for(unsigned i=0;i<5;i++)if(bytes[i]!=grant[i])same=0;
     if(!same&&(!ops->owned_callsite||ops->owned_callsite(ops->context,base+0x1157413,grant,5)!=1))return 0;}
    if(!lid_mp_read(ops,base+0x1157456,bytes,5))return 0;
    {unsigned same=1;for(unsigned i=0;i<5;i++)if(bytes[i]!=debit[i])same=0;
     if(!same&&(!ops->owned_callsite||ops->owned_callsite(ops->context,base+0x1157456,debit,5)!=1))return 0;}
    for(unsigned i=0;i<count;i++){
        if(!lid_mp_id(rows[i].skill,rows[i].units)||!rows[i].price||rows[i].price>0x7fffffff||rows[i].offer>=4096||
           rows[i].spirit_price||rows[i].recycle_price)return 0;
        for(unsigned j=0;j<i;j++)if(rows[i].offer==rows[j].offer||
          lid_mp_equal(rows[i].skill,rows[i].units,rows[j].skill,rows[j].units))return 0;
    }
    /* All validation before publication. Caller owns all source row storage. */
    __builtin_memset(out,0,sizeof(*out));out->base=base;out->account=base+0xf8b5088;
    out->session=session;out->owner=owner;out->ops=*ops;out->count=count;out->active=-1;
    for(unsigned i=0;i<count;i++)out->rows[i]=rows[i];out->initialized=1;return 1;
}
/* Read one FString out of the native one-element grant TArray. No borrowed
   array/text pointer is retained past this call. */
static int lid_mp_grant_id(const LidMomokaPayment*s,uintptr_t array,uint16_t*out,unsigned*units){
    struct {uintptr_t data;int32_t num,capacity;} a,text;
    if(!lid_mp_read(&s->ops,array,&a,16)||a.num!=1||a.capacity<1||a.capacity>4096||
       !lid_mp_read(&s->ops,a.data,&text,16)||text.num<2||text.num>129||
       text.capacity<text.num||text.capacity>1048576||
       !lid_mp_read(&s->ops,text.data,out,(size_t)text.num*2)||!lid_mp_id(out,(unsigned)text.num-1))return 0;
    *units=(unsigned)text.num-1;return 1;
}
/* MATCH identifies the sole AP grant candidate. It is not permission to
   activate suppression without the complete host/debit/save pipeline. */
static int lid_momoka_payment_grant(LidMomokaPayment*s,uintptr_t caller,
                                   uintptr_t account,uintptr_t list,unsigned char flag){
    uint16_t skill[129];unsigned units=0;
    if(!s||!s->initialized)return LID_MP_NATIVE;
    if(caller!=s->base+0x1157418)return LID_MP_NATIVE;
    if(!lid_mp_valid(s)||account!=s->account||flag!=1||s->active!=-1||
       !lid_mp_grant_id(s,list,skill,&units))return LID_MP_INVALID;
    for(unsigned i=0;i<s->count;i++)if(lid_mp_equal(skill,units,s->rows[i].skill,s->rows[i].units)){
        if(s->phase[i]!=LID_MP_WAIT||s->ops.admission(s->ops.context)!=1)return LID_MP_INVALID;
        s->active=(int)i;s->phase[i]=LID_MP_GRANT;return LID_MP_MATCH;
    }
    return LID_MP_NATIVE;
}
static int lid_momoka_payment_before_debit(LidMomokaPayment*s,uintptr_t caller,
                   uintptr_t account,int amount,uintptr_t reason){
    LidMPMoney before;
    if(!s||!s->initialized||caller!=s->base+0x115745b)return LID_MP_NATIVE;
    if(s->active<0)return LID_MP_NATIVE;
    if(!lid_mp_valid(s)||account!=s->account||s->ops.admission(s->ops.context)!=1||
       s->phase[s->active]!=LID_MP_GRANT||amount<1||
       (unsigned)amount!=s->rows[s->active].price||reason!=s->base+0x2118650||
       !lid_mp_read(&s->ops,account+0xc,&before,sizeof(before))||
       before.free_money<amount||before.paid_money<0)return LID_MP_INVALID;
    unsigned i=(unsigned)s->active;s->phase[i]=LID_MP_DEBIT;
    s->evidence[i]=(LidMPReceipt){s->rows[i].offer,s->rows[i].price,before,{0,0},0,0,0};
    return LID_MP_MATCH;
}
/* Call after the ORIGINAL debit has returned exactly once. This method never
   invokes it, retries it, changes its result or repairs balances by raw writes.
   KC-only Momoka preflight requires free_money>=price, so paid_money must stay
   unchanged. A native false is uncertain even if the balances look unchanged. */
static int lid_momoka_payment_after_debit(LidMomokaPayment*s,unsigned char native_result){
    if(!lid_mp_valid(s)||s->active<0||s->phase[s->active]!=LID_MP_DEBIT||
       s->ops.admission(s->ops.context)!=1)return LID_MP_INVALID;
    unsigned i=(unsigned)s->active;LidMPReceipt*r=&s->evidence[i];
    r->native_result=native_result;
    r->after_valid=lid_mp_read(&s->ops,s->account+0xc,&r->after,sizeof(r->after));
    r->paid=native_result!=0&&r->after_valid&&
      r->after.free_money==r->before.free_money-(int)r->price&&r->after.paid_money==r->before.paid_money;
    int published=s->ops.receipt(s->ops.context,r)==1;
    if(r->paid&&published){s->phase[i]=LID_MP_PAID;s->active=-1;return LID_MP_MATCH;}
    s->phase[i]=LID_MP_UNCERTAIN;s->blocked=1;s->ops.uncertain(s->ops.context,s->rows[i].offer);return LID_MP_INVALID;
}
/* Cleanup/exception notification: only the infallible old-run metadata latch.
   Keeps owned evidence for later draining; host must retain the old run scope.
   Missing post-debit observation must never become a fresh retriable purchase. */
static int lid_momoka_payment_abort(LidMomokaPayment*s){
    if(!lid_mp_owner(s))return 0;
    if(s->active>=0&&(s->phase[s->active]==LID_MP_GRANT||s->phase[s->active]==LID_MP_DEBIT)){
        s->phase[s->active]=LID_MP_UNCERTAIN;s->blocked=1;
        s->ops.uncertain(s->ops.context,s->rows[s->active].offer);
    }
    return 1;
}
static int lid_momoka_payment_complete(const LidMomokaPayment*s){
    if(!lid_mp_valid(s)||s->active!=-1)return 0;
    for(unsigned i=0;i<s->count;i++)if(s->phase[i]!=LID_MP_PAID)return 0;return 1;
}
#endif
