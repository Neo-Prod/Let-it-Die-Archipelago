#ifndef LID_SHOP_MOMOKA_HOST_H
#define LID_SHOP_MOMOKA_HOST_H
#include "shop_transaction_state.h"
#include "shop_momoka_payment.h"
/* Concrete resident C host, not a detour/installer. Never free or rebind a host
   while callbacks can refer to it. All APIs require the real native thread ID.
   BUSY must be handled by a future worker dispatcher BEFORE the corresponding
   native mutation; it is not permission to call the vanilla AP grant/debit.
   No engine locks, sleeps, file IO, native debit or AP send in this component. */
enum {LID_HM_BUSY=2,LID_HM_UI=1,LID_HM_REQUEST=2,LID_HM_SAVE=3};
typedef struct LidShopHostOps {
    void*context;uint64_t(*session)(void*);uint32_t(*thread)(void*);
    unsigned(*role)(void*);int(*copy)(void*,uintptr_t,void*,size_t);
    int(*owned_callsite)(void*,uintptr_t,const unsigned char*,unsigned);
} LidShopHostOps;
typedef struct LidShopPendingRow {LidMPRow row;unsigned used;} LidShopPendingRow;
typedef struct LidShopMomokaHost {
    unsigned initialized,refused,request_active,needs_cleanup;uint32_t request_owner;
    uint64_t session,request_ticket;unsigned char identity[96];LidShopHostOps native;
    LidShopAdmission gate;LidShopLease purchase;
    LidShopLedger ledger;LidShopPendingRow pending[128];LidMomokaPayment scope;
    LidMPRow prepare_rows[128]; /* resident scratch, never a 35KB worker stack frame */
} LidShopMomokaHost;
static int lid_hm_live(const LidShopMomokaHost*h){
    return h&&h->initialized&&h->native.session(h->native.context)==h->session;
}
static uint32_t lid_hm_thread(const LidShopMomokaHost*h){return h->native.thread(h->native.context);}
static int lid_hm_request(const LidShopMomokaHost*h,uint64_t ticket){
    return lid_hm_live(h)&&__atomic_load_n(&h->request_active,__ATOMIC_ACQUIRE)&&
      ticket==__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE)&&
      lid_hm_thread(h)==__atomic_load_n(&h->request_owner,__ATOMIC_ACQUIRE)&&h->native.role(h->native.context)==LID_HM_REQUEST;
}
static uint64_t lid_hm_mp_session(void*c){LidShopMomokaHost*h=c;return lid_hm_live(h)?h->session:0;}
static uint64_t lid_hm_mp_thread(void*c){return lid_hm_thread(c);}
static int lid_hm_mp_worker(void*c){LidShopMomokaHost*h=c;return lid_hm_request(h,__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE));}
static int lid_hm_mp_admission(void*c){
    LidShopMomokaHost*h=c;uint32_t tid=lid_hm_thread(h);
    if((__atomic_load_n(&h->gate.owner,__ATOMIC_ACQUIRE)>>32)!=tid)return 0;
    return h->purchase.kind==LID_ST_PURCHASE&&lid_st_held(&h->gate,&h->purchase,tid);
}
static int lid_hm_mp_copy(void*c,uintptr_t p,void*out,size_t n){
    LidShopMomokaHost*h=c;return h->native.copy(h->native.context,p,out,n);
}
static int lid_hm_mp_callsite(void*c,uintptr_t p,const unsigned char*original,unsigned size){
    LidShopMomokaHost*h=c;return h->native.owned_callsite&&h->native.owned_callsite(h->native.context,p,original,size)==1;
}
static int lid_hm_mp_receipt(void*c,const LidMPReceipt*r){
    LidShopMomokaHost*h=c;
    if(!r||!lid_hm_mp_admission(h)||r->offer>=h->ledger.count||h->ledger.price[r->offer]!=r->price)return 0;
    if(!r->paid){lid_st_uncertain(&h->ledger,r->offer);return 1;}
    return lid_st_paid(&h->ledger,r->offer);
}
static void lid_hm_mp_uncertain(void*c,unsigned offer){
    /* Scope owner still holds purchase admission, including lost-session abort.
       Only old-run C memory is touched; no current-world pointer is consulted. */
    LidShopMomokaHost*h=c;lid_st_uncertain(&h->ledger,offer);
}
static int lid_hm_prepare_context(LidShopMomokaHost*h,const LidShopHostOps*ops,
                              const unsigned char identity[96],unsigned count){
    uint64_t session;
    if(!h||h->initialized||h->refused||!ops||!ops->session||!ops->thread||!ops->role||!ops->copy||!identity||
       !count||count>LID_ST_MAX||!(session=ops->session(ops->context))||!ops->thread(ops->context))return 0;
    for(unsigned block=0;block<3;block++){
        unsigned nonzero=0;for(unsigned i=0;i<32;i++)nonzero|=identity[block*32+i];if(!nonzero)return 0;
    }
    __builtin_memset(h,0,sizeof(*h));h->native=*ops;h->session=session;
    __builtin_memcpy(h->identity,identity,96);return 1;
}
static int lid_shop_host_init(LidShopMomokaHost*h,const LidShopHostOps*ops,
                              const unsigned char identity[96],unsigned count){
    if(!lid_hm_prepare_context(h,ops,identity,count)||!lid_st_init(&h->ledger,count))return 0;
    h->initialized=1;return 1;
}
/* Failed/corrupt restore latches refusal; it must never fall back to an empty
   ledger. A fresh init is allowed only after the host's new-run eligibility
   check, not merely because a marker is absent. Input must not alias h. */
static int lid_shop_host_restore(LidShopMomokaHost*h,const LidShopHostOps*ops,
   const unsigned char identity[96],unsigned count,const unsigned char*marker,unsigned size){
    if(!h||h->initialized||h->refused)return 0;
    if(!lid_hm_prepare_context(h,ops,identity,count)){h->refused=1;return 0;}
    if(!lid_st_restore(&h->ledger,marker,size,identity,count)){h->refused=1;return 0;}
    h->initialized=1;return 1;
}
/* Call only under METADATA admission. Clean only owned C data; no dead API/root
   pointer is retained when an earlier handler has returned. */
static void lid_hm_cleanup(LidShopMomokaHost*h){
    if(!__atomic_load_n(&h->needs_cleanup,__ATOMIC_ACQUIRE))return;
    for(unsigned i=0;i<h->scope.count;i++)(void)lid_st_unclaim(&h->ledger,h->scope.rows[i].offer);
    for(unsigned i=0;i<128;i++)if(h->pending[i].used&&
      h->ledger.phase[h->pending[i].row.offer]>=LID_ST_PAID)h->pending[i].used=0;
    __atomic_store_n(&h->needs_cleanup,0,__ATOMIC_RELEASE);
}
/* Reservation occurs BEFORE the script's UI money mutation/history append. */
static int lid_shop_host_reserve(LidShopMomokaHost*h,const LidMPRow*row){
    LidShopLease lease={0};int slot=-1,result=LID_MP_INVALID;
    if(!lid_hm_live(h)||!row||h->native.role(h->native.context)!=LID_HM_UI||
       !lid_mp_id(row->skill,row->units)||!row->price||row->price>INT32_MAX||
       row->spirit_price||row->recycle_price||row->offer>=h->ledger.count)return LID_MP_INVALID;
    if(!lid_st_acquire(&h->gate,lid_hm_thread(h),LID_ST_METADATA,&lease))return LID_HM_BUSY;
    if(!__atomic_load_n(&h->request_active,__ATOMIC_ACQUIRE))lid_hm_cleanup(h);
    for(unsigned i=0;i<128;i++){
        if(h->pending[i].used&&(h->pending[i].row.offer==row->offer||
          lid_mp_equal(h->pending[i].row.skill,h->pending[i].row.units,row->skill,row->units)))goto done;
        if(!h->pending[i].used&&slot<0)slot=(int)i;
    }
    if(slot>=0&&lid_st_reserve(&h->ledger,row->offer,row->price)){
        h->pending[slot].row=*row;h->pending[slot].used=1;result=LID_MP_MATCH;
    }
done:
    if(!lid_st_release(&h->gate,&lease,lid_hm_thread(h)))return LID_MP_INVALID;return result;
}
/* ids must come from the exact current copied BUY history, resolved against
   the bound catalogue and reserved UI batch; this method does not parse JSON. */
static int lid_hm_begin_checked(LidShopMomokaHost*h,uintptr_t base,int sha,
                               const unsigned*ids,const LidMPRow*expected,unsigned count,uint64_t*ticket){
    LidShopLease lease={0};LidMPRow*rows;unsigned found=0;int result=LID_MP_INVALID;
    if(!lid_hm_live(h)||!ticket||!ids||!count||count>128||h->native.role(h->native.context)!=LID_HM_REQUEST)return LID_MP_INVALID;
    if(!lid_st_acquire(&h->gate,lid_hm_thread(h),LID_ST_METADATA,&lease))return LID_HM_BUSY;
    rows=h->prepare_rows;
    if(__atomic_load_n(&h->request_active,__ATOMIC_ACQUIRE)||__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE)==UINT64_MAX)goto done;
    lid_hm_cleanup(h);
    for(unsigned i=0;i<count;i++){
        unsigned matches=0;
        for(unsigned j=0;j<128;j++)if(h->pending[j].used&&h->pending[j].row.offer==ids[i]){rows[i]=h->pending[j].row;matches++;}
        if(matches!=1)goto done;
        if(expected&&(expected[i].offer!=ids[i]||!lid_mp_id(expected[i].skill,expected[i].units)||
           !lid_mp_equal(expected[i].skill,expected[i].units,rows[i].skill,rows[i].units)))goto done;
    }
    if(!lid_st_claim(&h->ledger,ids,count))goto done;found=1;
    __atomic_store_n(&h->request_owner,lid_hm_thread(h),__ATOMIC_RELEASE);
    __atomic_store_n(&h->request_ticket,__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE)+1,__ATOMIC_RELEASE);
    __builtin_memset(&h->scope,0,sizeof(h->scope));
    __atomic_store_n(&h->request_active,1,__ATOMIC_RELEASE);
    {LidMPOps ops={h,lid_hm_mp_session,lid_hm_mp_thread,lid_hm_mp_worker,lid_hm_mp_admission,
                  lid_hm_mp_copy,lid_hm_mp_receipt,lid_hm_mp_uncertain,lid_hm_mp_callsite};
     if(!lid_momoka_payment_prepare(&h->scope,base,sha,&ops,rows,count)){
         __atomic_store_n(&h->request_active,0,__ATOMIC_RELEASE);goto done;
     }}
    *ticket=__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE);result=LID_MP_MATCH;
done:
    if(result!=LID_MP_MATCH&&found)for(unsigned i=0;i<count;i++)(void)lid_st_unclaim(&h->ledger,ids[i]);
    if(!lid_st_release(&h->gate,&lease,lid_hm_thread(h)))return LID_MP_INVALID;return result;
}
static int lid_shop_host_begin(LidShopMomokaHost*h,uintptr_t base,int sha,
                               const unsigned*ids,unsigned count,uint64_t*ticket){
    return lid_hm_begin_checked(h,base,sha,ids,0,count,ticket);
}
static int lid_shop_host_grant(LidShopMomokaHost*h,uint64_t ticket,uintptr_t caller,
                                uintptr_t account,uintptr_t list,unsigned char flag){
    uint16_t skill[129];unsigned units=0,matched=0;int result;
    if(!lid_hm_request(h,ticket))return LID_MP_INVALID;
    if(caller!=h->scope.base+0x1157418)return LID_MP_NATIVE;
    if(account!=h->scope.account||flag!=1||!lid_mp_grant_id(&h->scope,list,skill,&units))return LID_MP_INVALID;
    for(unsigned i=0;i<h->scope.count;i++)if(lid_mp_equal(skill,units,h->scope.rows[i].skill,h->scope.rows[i].units))matched=1;
    if(!matched)return LID_MP_NATIVE;
    if(h->purchase.token)return LID_MP_INVALID; /* reentry is not retriable contention */
    if(!lid_st_acquire(&h->gate,lid_hm_thread(h),LID_ST_PURCHASE,&h->purchase))return LID_HM_BUSY;
    result=lid_momoka_payment_grant(&h->scope,caller,account,list,flag);
    if(result!=LID_MP_MATCH)(void)lid_st_release(&h->gate,&h->purchase,lid_hm_thread(h));return result;
}
static int lid_shop_host_before_debit(LidShopMomokaHost*h,uint64_t ticket,uintptr_t caller,
                                      uintptr_t account,int amount,uintptr_t reason){
    int result;if(!lid_hm_request(h,ticket))return LID_MP_INVALID;
    result=lid_momoka_payment_before_debit(&h->scope,caller,account,amount,reason);
    if(result!=LID_MP_MATCH)return result;
    unsigned i=(unsigned)h->scope.active;LidMPReceipt*r=&h->scope.evidence[i];
    if(!lid_st_begin(&h->ledger,r->offer,r->price,(uint32_t)r->before.free_money))return LID_MP_INVALID;
    return LID_MP_MATCH;
}
static int lid_shop_host_after_debit(LidShopMomokaHost*h,uint64_t ticket,unsigned char native_result){
    int result=LID_MP_INVALID;
    if(!h||!h->initialized||ticket!=__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE)||!lid_hm_mp_admission(h))return LID_MP_INVALID;
    if(lid_hm_request(h,ticket))result=lid_momoka_payment_after_debit(&h->scope,native_result);
    if(result!=LID_MP_MATCH)(void)lid_momoka_payment_abort(&h->scope);
    if(!lid_st_release(&h->gate,&h->purchase,lid_hm_thread(h)))return LID_MP_INVALID;return result;
}
/* Pop native TLS before end. Owned cleanup can be deferred while a save owns
   admission, without retaining the native API object or waiting on a runner. */
static int lid_shop_host_end(LidShopMomokaHost*h,uint64_t ticket){
    if(!h||!h->initialized||ticket!=__atomic_load_n(&h->request_ticket,__ATOMIC_ACQUIRE)||!__atomic_load_n(&h->request_active,__ATOMIC_ACQUIRE)||
       lid_hm_thread(h)!=__atomic_load_n(&h->request_owner,__ATOMIC_ACQUIRE))return 0;
    if(h->purchase.token){
        if(!lid_hm_mp_admission(h))return 0;
        (void)lid_momoka_payment_abort(&h->scope);
        if(!lid_st_release(&h->gate,&h->purchase,lid_hm_thread(h)))return 0;
    }
    __atomic_store_n(&h->needs_cleanup,1,__ATOMIC_RELEASE);
    __atomic_store_n(&h->request_active,0,__ATOMIC_RELEASE);return 1;
}
/* Save/verification leases are separate from request lifetime and do not
   borrow or change the existing reload save gate. BUSY is not a Save failure. */
static int lid_shop_host_save_begin(LidShopMomokaHost*h,LidShopLease*lease){
    if(!lid_hm_live(h)||h->native.role(h->native.context)!=LID_HM_SAVE)return LID_MP_INVALID;
    return lid_st_acquire(&h->gate,lid_hm_thread(h),LID_ST_SAVE,lease)?LID_MP_MATCH:LID_HM_BUSY;
}
static int lid_shop_host_snapshot(LidShopMomokaHost*h,const LidShopLease*lease,unsigned char*out,unsigned cap,unsigned*size){
    return lid_hm_live(h)&&lease&&lease->kind==LID_ST_SAVE&&lid_st_held(&h->gate,lease,lid_hm_thread(h))&&
      lid_st_encode(&h->ledger,h->identity,out,cap,size);
}
/* Apply only AFTER the BRG readback and authenticated immutable expectation
   have been verified by the bridge. This method does not authenticate a file. */
static int lid_shop_host_verified(LidShopMomokaHost*h,const unsigned char*marker,unsigned size,
                                   const unsigned*ids,unsigned count){
    LidShopLease lease={0};int result;
    if(!lid_hm_live(h)||h->native.role(h->native.context)!=LID_HM_UI)return LID_MP_INVALID;
    if(!lid_st_acquire(&h->gate,lid_hm_thread(h),LID_ST_METADATA,&lease))return LID_HM_BUSY;
    result=lid_st_verify(&h->ledger,h->identity,marker,size,ids,count)?LID_MP_MATCH:LID_MP_INVALID;
    if(!lid_st_release(&h->gate,&lease,lid_hm_thread(h)))return LID_MP_INVALID;return result;
}
#endif
