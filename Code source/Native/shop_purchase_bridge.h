#ifndef LID_SHOP_PURCHASE_BRIDGE_H
#define LID_SHOP_PURCHASE_BRIDGE_H
#include "shop_purchase_wire.h"
#include "checks_disk_io.h"
/* Own C state only. Real native request/UI wrappers prove callsites, copied
   catalogue SKU, prices and lifetime BEFORE invoking these methods. Never call
   a native grant or item-entry dispatcher here. Reserve BEFORE charging UI.
   No metadata lock spans native calls, async ticks or any file operation. */
enum {LID_PB_CAP=64};
enum {LID_PB_EMPTY,LID_PB_RESERVED,LID_PB_PAYING,LID_PB_READY,LID_PB_PUBLISHED,LID_PB_UNCERTAIN};
typedef struct LidShopPurchaseSlot {
    unsigned phase,ui_confirmed;uint32_t payer;uint64_t token,ui_frame;LidMPRow row;
    uint8_t raw[LID_SP_SIZE];
} LidShopPurchaseSlot;
typedef struct LidShopPurchaseBridge {
    unsigned initialized,guard,ready,blocked,published,halted;uint32_t ui,io;
    uint64_t next;R15Context context;uint8_t catalogue[32],allowed[512],bought[512];
    void*provider;uint32_t(*thread)(void*);int(*live)(void*);
    LidShopPurchaseSlot slots[LID_PB_CAP];
} LidShopPurchaseBridge;
static unsigned lid_pb_phase(const LidShopPurchaseSlot*s){return __atomic_load_n(&s->phase,__ATOMIC_ACQUIRE);}
static void lid_pb_phase_set(LidShopPurchaseSlot*s,unsigned phase){__atomic_store_n(&s->phase,phase,__ATOMIC_RELEASE);}
static uint64_t lid_pb_token(const LidShopPurchaseSlot*s){return __atomic_load_n(&s->token,__ATOMIC_ACQUIRE);}
static void lid_pb_token_set(LidShopPurchaseSlot*s,uint64_t value){__atomic_store_n(&s->token,value,__ATOMIC_RELEASE);}
static uint32_t lid_pb_payer(const LidShopPurchaseSlot*s){return __atomic_load_n(&s->payer,__ATOMIC_ACQUIRE);}
static void lid_pb_payer_set(LidShopPurchaseSlot*s,uint32_t value){__atomic_store_n(&s->payer,value,__ATOMIC_RELEASE);}
static unsigned lid_pb_confirmed(const LidShopPurchaseSlot*s){return __atomic_load_n(&s->ui_confirmed,__ATOMIC_ACQUIRE);}
static void lid_pb_confirmed_set(LidShopPurchaseSlot*s,unsigned value){__atomic_store_n(&s->ui_confirmed,value,__ATOMIC_RELEASE);}
static int lid_pb_lock(LidShopPurchaseBridge*b,uint32_t*tid){
    unsigned empty=0;
    if(!b||!b->initialized||!tid||!(*tid=b->thread(b->provider)))return 0;
    return __atomic_compare_exchange_n(&b->guard,&empty,*tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void lid_pb_unlock(LidShopPurchaseBridge*b){__atomic_store_n(&b->guard,0,__ATOMIC_RELEASE);}
static int lid_pb_bit(const uint8_t*bits,unsigned offer){return offer<4096&&(bits[offer/8]&(1u<<(offer%8)))!=0;}
static LidShopPurchaseSlot*lid_pb_slot(LidShopPurchaseBridge*b,uint64_t token){
    if(!token)return 0;for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])!=LID_PB_EMPTY&&lid_pb_token(&b->slots[i])==token)return &b->slots[i];return 0;
}
static int lid_pb_prepare(LidShopPurchaseBridge*b,const R15Context*context,const uint8_t catalogue[32],
    const uint8_t allowed[512],uint32_t ui,void*provider,uint32_t(*thread)(void*),int(*live)(void*)){
    if(!b||b->initialized||!r15_context_valid(context)||!catalogue||!r15_nonzero(catalogue,32)||
       !allowed||!r15_nonzero(allowed,512)||!ui||!thread||!live||thread(provider)!=ui||!live(provider))return 0;
    b->context=*context;r15_copy(b->catalogue,catalogue,32);r15_copy(b->allowed,allowed,512);
    b->ui=ui;b->provider=provider;b->thread=thread;b->live=live;
    /* Caller supplies zeroed resident storage, never copied/rebound later. */
    for(unsigned i=0;i<LID_PB_CAP;i++)lid_pb_phase_set(&b->slots[i],LID_PB_EMPTY);
    b->initialized=1;return 1;
}
static int lid_pb_bind_io(LidShopPurchaseBridge*b,uint32_t worker){
    uint32_t tid;int ok=0;if(!worker||!lid_pb_lock(b,&tid))return 0;
    if(tid==worker&&tid!=b->ui&&!b->io&&!b->ready&&!b->published){
        unsigned empty=1;
        for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])!=LID_PB_EMPTY)empty=0;
        if(empty){b->io=worker;ok=1;}
    }
    lid_pb_unlock(b);return ok;
}
/* 1 reserved,0 busy/unready/sold out. No mutation of native/cache balances.
   row is an explicit catalogue-bound AP carrier, not the item AP will send. */
static int lid_pb_reserve(LidShopPurchaseBridge*b,const LidMPRow*row,uint64_t ui_frame,uint64_t*out){
    uint32_t tid;LidShopPurchaseSlot*free=0;int ok=0;
    if(!row||!out||!ui_frame||!lid_mp_id(row->skill,row->units)||row->offer>=4096||!row->price||
       row->price>INT32_MAX||row->spirit_price||row->recycle_price||!lid_pb_lock(b,&tid))return 0;
    if(tid!=b->ui||!b->ready||b->blocked||__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE)||!b->live(b->provider)||
       !lid_pb_bit(b->allowed,row->offer)||lid_pb_bit(b->bought,row->offer)||b->next==UINT64_MAX)goto done;
    for(unsigned i=0;i<LID_PB_CAP;i++){
        LidShopPurchaseSlot*s=&b->slots[i];
        if(lid_pb_phase(s)!=LID_PB_EMPTY&&s->row.offer==row->offer)goto done;
        if(lid_pb_phase(s)==LID_PB_EMPTY&&!free)free=s;
    }
    if(!free)goto done;
    free->row=*row;lid_pb_token_set(free,++b->next);lid_pb_payer_set(free,0);free->ui_frame=ui_frame;
    lid_pb_confirmed_set(free,0);lid_pb_phase_set(free,LID_PB_RESERVED);*out=lid_pb_token(free);ok=1;
done:lid_pb_unlock(b);return ok;
}
/* Cancellation allowed only BEFORE local UI mutation/worker adoption; caller
   proves that precondition. Do not cancel after a debit to permit re-purchase. */
static int lid_pb_cancel(LidShopPurchaseBridge*b,uint64_t token){
    uint32_t tid;int ok=0;if(!lid_pb_lock(b,&tid))return 0;
    LidShopPurchaseSlot*s=lid_pb_slot(b,token);
    if(tid==b->ui&&s&&lid_pb_phase(s)==LID_PB_RESERVED&&!lid_pb_confirmed(s)){
        lid_pb_token_set(s,0);s->ui_frame=0;lid_pb_phase_set(s,LID_PB_EMPTY);ok=1;
    }
    lid_pb_unlock(b);return ok;
}
/* ADD's current live parent frame must be the SAME first-price BuySticker
   frame. It is compared only as identity, never dereferenced or deferred.
   Success authorizes future wrapper to skip UI owned-decal addition, not give
   any AP item. Failure after charge is uncertain and must not be retried. */
static int lid_pb_confirm_ui(LidShopPurchaseBridge*b,const LidMPRow*row,uint64_t parent){
    uint32_t tid;int ok=0;if(!row||!parent||!lid_pb_lock(b,&tid))return 0;
    if(tid!=b->ui||b->blocked||__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE)||!b->live(b->provider))goto done;
    for(unsigned i=0;i<LID_PB_CAP;i++){
        LidShopPurchaseSlot*s=&b->slots[i];
        if(lid_pb_phase(s)==LID_PB_RESERVED&&s->row.offer==row->offer){
            if(!lid_pb_confirmed(s)&&s->ui_frame==parent&&s->row.price==row->price&&
               lid_mp_equal(s->row.skill,s->row.units,row->skill,row->units)){
                s->ui_frame=0;lid_pb_confirmed_set(s,1);ok=1;
            }else{lid_pb_phase_set(s,LID_PB_UNCERTAIN);s->ui_frame=0;b->blocked=1;b->ready=0;}
            break;
        }
    }
done:lid_pb_unlock(b);return ok;
}
/* Owned request decoder may recover a token ONLY for an exact confirmed SKU.
   This lookup does not adopt a native API/root pointer or change phase. */
static int lid_pb_find(LidShopPurchaseBridge*b,unsigned offer,const uint16_t*skill,unsigned units,uint64_t*out){
    uint32_t tid;int ok=0;if(!out||!lid_mp_id(skill,units)||!lid_pb_lock(b,&tid))return 0;
    if(tid!=b->ui&&tid!=b->io&&!b->blocked&&!__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE)&&b->live(b->provider))for(unsigned i=0;i<LID_PB_CAP;i++){
        LidShopPurchaseSlot*s=&b->slots[i];
        if(lid_pb_phase(s)==LID_PB_RESERVED&&lid_pb_confirmed(s)&&s->row.offer==offer&&
           lid_mp_equal(skill,units,s->row.skill,s->row.units)){*out=lid_pb_token(s);ok=1;break;}
    }
    lid_pb_unlock(b);return ok;
}
static int lid_pb_uncertain_ui(LidShopPurchaseBridge*b,unsigned offer){
    uint32_t tid;if(!lid_pb_lock(b,&tid))return 0;
    if(tid!=b->ui){lid_pb_unlock(b);return 0;}
    for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])==LID_PB_RESERVED&&b->slots[i].row.offer==offer){
        lid_pb_phase_set(&b->slots[i],LID_PB_UNCERTAIN);b->slots[i].ui_frame=0;
    }
    b->blocked=1;b->ready=0;lid_pb_unlock(b);return 1;
}
/* Exact owned request wrapper calls immediately before its matched grant.
   Copies row for existing native payment observer; lock is released BEFORE
   original debit, so native Save/worker code never waits on this metadata. */
static int lid_pb_begin(LidShopPurchaseBridge*b,uint64_t token,LidMPRow*out){
    uint32_t tid;int ok=0;if(!out||!lid_pb_lock(b,&tid))return 0;
    LidShopPurchaseSlot*s=lid_pb_slot(b,token);
    if(tid!=b->ui&&tid!=b->io&&b->ready&&!b->blocked&&!__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE)&&b->live(b->provider)&&s&&lid_pb_phase(s)==LID_PB_RESERVED&&lid_pb_confirmed(s)&&
       !lid_pb_bit(b->bought,s->row.offer)){
        lid_pb_payer_set(s,tid);lid_pb_phase_set(s,LID_PB_PAYING);*out=s->row;ok=1;
    }
    lid_pb_unlock(b);return ok;
}
/* Call after owning native handler RETURN, never just after the UI price/getter.
   Receipt fields are actual native observations, not values to write back.
   Uncertain/partial failure blocks THIS shop bridge; no refunds/fake success. */
/* Payer owns immutable row/token until READY release. Avoid losing a paid
   row merely because UI/IO briefly owns global metadata. Readers acquire phase
   before payload; no native/file call and no global guard held in completion. */
static int lid_pb_finish(LidShopPurchaseBridge*b,uint64_t token,const LidMPReceipt*receipt,unsigned handler){
    uint32_t tid;LidShopPurchaseSlot*s=0;LidShopPurchaseFrame frame={0};
    if(!b||!b->initialized||!token||!(tid=b->thread(b->provider))||tid==b->ui||tid==b->io)return 0;
    for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])==LID_PB_PAYING&&
       lid_pb_token(&b->slots[i])==token&&lid_pb_payer(&b->slots[i])==tid){s=&b->slots[i];break;}
    if(!s)return 0;
    if(!b->live(b->provider)||!lid_sp_from_receipt(&frame,&b->context,b->catalogue,token,&s->row,receipt,handler)||
       !lid_sp_encode(s->raw,sizeof(s->raw),&frame)){
        __atomic_store_n(&b->halted,1,__ATOMIC_RELEASE);lid_pb_phase_set(s,LID_PB_UNCERTAIN);return 0;
    }
    lid_pb_phase_set(s,LID_PB_READY);return 1;
}
static int lid_pb_payment_held(const LidShopPurchaseBridge*b,uint64_t token,uint32_t tid){
    if(!b||!b->initialized||!token||!tid||tid==b->ui||tid==b->io)return 0;
    for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])==LID_PB_PAYING&&
       lid_pb_token(&b->slots[i])==token&&lid_pb_payer(&b->slots[i])==tid)return 1;
    return 0;
}
static int lid_pb_describe(LidShopPurchaseBridge*b,uint64_t token,LidMPRow*out){
    uint32_t tid;int ok=0;if(!out||!lid_pb_lock(b,&tid))return 0;
    LidShopPurchaseSlot*s=lid_pb_slot(b,token);
    if(tid!=b->ui&&tid!=b->io&&s&&lid_pb_phase(s)==LID_PB_RESERVED&&lid_pb_confirmed(s)&&
       !b->blocked&&!__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE)&&b->live(b->provider)){
        *out=s->row;ok=1;
    }
    lid_pb_unlock(b);return ok;
}
/* Old-run owned metadata only; no bank/root/API reads or debit retry. */
static void lid_pb_request_fail(LidShopPurchaseBridge*b,uint64_t token){
    uint32_t tid;if(!b||!b->initialized||!token||!(tid=b->thread(b->provider))||tid==b->ui||tid==b->io)return;
    __atomic_store_n(&b->halted,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<LID_PB_CAP;i++){
        LidShopPurchaseSlot*s=&b->slots[i];unsigned phase=lid_pb_phase(s);
        if((phase!=LID_PB_RESERVED&&phase!=LID_PB_PAYING)||lid_pb_token(s)!=token||
           (phase==LID_PB_PAYING&&lid_pb_payer(s)!=tid))continue;
        if(phase==LID_PB_RESERVED&&!lid_pb_confirmed(s))continue;
        (void)__atomic_compare_exchange_n(&s->phase,&phase,LID_PB_UNCERTAIN,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);return;
    }
}
/* Transport worker provides an ALREADY decoded same-context client bitmap.
   State is monotonic. Unknown offers/regression retire the bridge, not a Save.
   Future UI uses union of this bitmap and occupied per-offer slots. */
static int lid_pb_client_state(LidShopPurchaseBridge*b,const uint8_t bought[512]){
    uint32_t tid;int ok=0;if(!bought||!lid_pb_lock(b,&tid))return 0;
    if(!b->io||tid!=b->io||!b->live(b->provider))goto done;
    for(unsigned i=0;i<512;i++)if((bought[i]&~b->allowed[i])||(b->bought[i]&~bought[i])){
        b->blocked=1;b->ready=0;goto done;
    }
    r15_copy(b->bought,bought,512);b->ready=(b->blocked||__atomic_load_n(&b->halted,__ATOMIC_ACQUIRE))?0:1;ok=1;
done:lid_pb_unlock(b);return ok;
}
static int lid_pb_peek(LidShopPurchaseBridge*b,uint8_t raw[LID_SP_SIZE],uint64_t*token){
    uint32_t tid;LidShopPurchaseSlot*first=0;int ok=0;
    if(!raw||!token||!lid_pb_lock(b,&tid))return 0;
    /* Admission may stop on another uncertain purchase; already proven READY
       receipts still drain. Stopping new buys must not discard prior checks. */
    if(!b->io||tid!=b->io||b->published||!b->live(b->provider))goto done;
    for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])==LID_PB_READY&&
       (!first||lid_pb_token(&b->slots[i])<lid_pb_token(first)))first=&b->slots[i];
    if(first){r15_copy(raw,first->raw,LID_SP_SIZE);*token=lid_pb_token(first);ok=1;}
done:lid_pb_unlock(b);return ok;
}
static int lid_pb_published(LidShopPurchaseBridge*b,uint64_t token){
    uint32_t tid;int ok=0;if(!lid_pb_lock(b,&tid))return 0;
    LidShopPurchaseSlot*s=lid_pb_slot(b,token);
    if(b->io&&tid==b->io&&!b->published&&s&&lid_pb_phase(s)==LID_PB_READY){
        lid_pb_phase_set(s,LID_PB_PUBLISHED);b->published=(unsigned)(s-b->slots)+1;ok=1;
    }
    lid_pb_unlock(b);return ok;
}
static int lid_pb_ack(LidShopPurchaseBridge*b,const LidShopPurchaseFrame*ack){
    uint32_t tid;int ok=0;
    if(!ack||!lid_sp_valid(ack)||ack->kind!=LID_SP_RECEIVED||!lid_pb_lock(b,&tid))return 0;
    if(b->io&&tid==b->io&&b->published&&b->published<=LID_PB_CAP&&
       r15_same_context(&ack->context,&b->context)&&r15_equal(ack->catalogue,b->catalogue,32)){
        LidShopPurchaseSlot*s=&b->slots[b->published-1];
        if(lid_pb_phase(s)==LID_PB_PUBLISHED&&lid_pb_token(s)==ack->sequence&&s->row.offer==ack->offer&&
           lid_pb_bit(b->bought,ack->offer)){
            lid_pb_token_set(s,0);lid_pb_payer_set(s,0);b->published=0;lid_pb_phase_set(s,LID_PB_EMPTY);ok=1;
        }
    }
    lid_pb_unlock(b);return ok;
}
static int lid_pb_retire_empty(LidShopPurchaseBridge*b){
    uint32_t tid;int ok=0;if(!lid_pb_lock(b,&tid))return 0;
    if(!b->io||tid!=b->io||b->published)goto done;
    for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])!=LID_PB_EMPTY)goto done;
    b->ready=0;b->blocked=1;ok=1;
done:lid_pb_unlock(b);return ok;
}
/* Concrete owned bridge IO. Call/start/pump/close ONLY on ONE established
   background transport thread, never UI or native payment thread. No thread
   is automatically launched by this header. Trusted slot directory only. */
typedef struct LidShopPurchaseDisk {
    unsigned initialized;uint32_t worker;uint64_t attempt,awaiting_mark;
    R26Ops ops;HANDLE lease;LidShopPurchaseBridge*bridge;
    wchar_t events[1120],ack[1120],state[1120],temporary[1220],stem[1180];
} LidShopPurchaseDisk;
static int lid_pd_start(LidShopPurchaseDisk*d,LidShopPurchaseBridge*b,const R26Ops*ops,const wchar_t*directory){
    wchar_t owner[1120];size_t n=0;uint32_t worker;
    if(!d||d->initialized||!b||!b->initialized||!r26_ops_valid(ops)||!directory||
       !(worker=ops->thread_id())||worker==b->ui)return 0;
    while(n<1000&&directory[n])n++;
    if(n<3||n==1000||directory[1]!=':'||(directory[2]!='\\'&&directory[2]!='/'))return 0;
    if(!r26_path(d->events,1120,directory,L"shop_native_payment.bin")||
       !r26_path(d->ack,1120,directory,L"shop_client_received.bin")||
       !r26_path(d->state,1120,directory,L"shop_client_state.bin")||
       !r26_path(owner,1120,directory,L"shop_native.owner")||
       !r26_path(d->stem,1180,directory,L".shop-")||!r26_nonce(d->stem,1180,&b->context)||
       !r26_append(d->stem,1180,L".tmp."))return 0;
    HANDLE lease=ops->open(owner,GENERIC_READ|GENERIC_WRITE,0,0,4,FILE_ATTRIBUTE_NORMAL,0);
    if(lease==INVALID_HANDLE_VALUE)return 0;
    if(!lid_pb_bind_io(b,worker)){ops->close(lease);return 0;}
    d->ops=*ops;d->lease=lease;d->bridge=b;d->worker=worker;d->initialized=1;return 1;
}
/* 0 missing, -1 failed/oversized, otherwise bytes read. Paths are fixed at setup. */
static int lid_pd_read(LidShopPurchaseDisk*d,const wchar_t*path,uint8_t*out,unsigned capacity){
    DWORD got=0;HANDLE f=d->ops.open(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(f==INVALID_HANDLE_VALUE){DWORD error=d->ops.error();return error==2||error==3?0:-1;}
    int ok=d->ops.read(f,out,capacity,&got,0)!=0;int closed=d->ops.close(f)!=0;
    return ok&&closed?(got?(int)got:-1):-1;
}
static int lid_pd_write(LidShopPurchaseDisk*d,const uint8_t raw[LID_SP_SIZE]){
    HANDLE f=INVALID_HANDLE_VALUE;DWORD wrote=0;
    for(unsigned i=0;i<8;i++){
        if(d->attempt==UINT64_MAX)return 0;d->temporary[0]=0;
        if(!r26_append(d->temporary,1220,d->stem)||!r26_hex(d->temporary,1220,++d->attempt,16))return 0;
        f=d->ops.open(d->temporary,GENERIC_WRITE,0,0,1,FILE_ATTRIBUTE_NORMAL,0);
        if(f!=INVALID_HANDLE_VALUE)break;DWORD e=d->ops.error();if(e!=80&&e!=183)return 0;
    }
    if(f==INVALID_HANDLE_VALUE)return 0;
    int ok=d->ops.write(f,raw,LID_SP_SIZE,&wrote,0)&&wrote==LID_SP_SIZE;
    if(ok)ok=d->ops.flush(f)!=0;
    int closed=d->ops.close(f)!=0;if(!closed)ok=0;
    if(ok)ok=d->ops.replace(d->temporary,d->events,9)!=0;
    if(!ok&&closed)d->ops.remove(d->temporary);return ok;
}
static int lid_pd_pump(LidShopPurchaseDisk*d){
    uint8_t state[LID_SP_STATE_SIZE+1],bought[512],raw[LID_SP_SIZE+1];LidShopPurchaseFrame ack={0};uint64_t token;
    if(!d||!d->initialized||!d->lease||d->ops.thread_id()!=d->worker)return 0;
    if(d->awaiting_mark){
        if(!lid_pb_published(d->bridge,d->awaiting_mark))return 0;
        d->awaiting_mark=0;
    }
    int n=lid_pd_read(d,d->state,state,sizeof(state));
    if(n==LID_SP_STATE_SIZE&&lid_sp_state_read(bought,state,(size_t)n,&d->bridge->context,d->bridge->catalogue))
        (void)lid_pb_client_state(d->bridge,bought);
    n=lid_pd_read(d,d->ack,raw,sizeof(raw));
    if(n==LID_SP_SIZE&&lid_sp_decode(&ack,raw,(size_t)n))(void)lid_pb_ack(d->bridge,&ack);
    if(lid_pb_peek(d->bridge,raw,&token)){
        if(!lid_pd_write(d,raw))return 0;
        d->awaiting_mark=token;
        if(!lid_pb_published(d->bridge,token))return 0;
        d->awaiting_mark=0;return 1;
    }
    return 1;
}
static int lid_pd_close(LidShopPurchaseDisk*d){
    if(!d||!d->initialized||!d->lease||d->awaiting_mark||d->ops.thread_id()!=d->worker||
       !lid_pb_retire_empty(d->bridge)||!d->ops.close(d->lease))return 0;
    d->lease=0;return 1;
}
#endif
