#ifndef LID_SHOP_MOMOKA_PURCHASE_EXEC_H
#define LID_SHOP_MOMOKA_PURCHASE_EXEC_H
#include "shop_momoka_purchase_ui.h"
#include "shop_momoka_request.h"
#include "momoka_native_dispatch.h"
#include "status_scan_guard.h"
#include "momoka_install_guard.h"
#include "momoka_owned_slot.h"
#ifndef LID_SHOP_EXEC_SEH_ENABLED
#define LID_SHOP_EXEC_SEH_ENABLED 0
#endif
/* Full source composition, NOT automatically published. One resident host per
   process; no rebinding to another AP run. Normal/abnormal cleanup uses compiler
   finally only when both SEH flags enabled. Native tests have NOT validated it.
   Runtime uses the coherent publisher below, never separate UI activation.
   No game Save, native item grant, receipt application or hub-entry operation. */
typedef struct LidShopPurchaseExec {
    unsigned initialized,owner,prepared,poisoned,failed,published,retired;uint64_t current;
    LidMomokaDispatch*dispatch;LidShopPurchaseUi*ui;LidShopPurchaseBridge*bridge;
    LidShopMarkerNative json;LidShopMarkerAccess raw;
    LidMomokaDispatchFrame*frame; /* borrowed only inside current handler */
    unsigned char(*original)(void*,void*,void*);
    unsigned char(*reject)(void*,void*);void(*yield)(uint32_t);
    LidMomokaRequest request;LidMomokaPayment payment;
    LidMPRow rows[128];uint64_t tokens[128];
    LidMPReceipt receipts[128];unsigned char adopted[128],received[128];
    const LidMPPriceCollector*metadata;M207Slot getter;
    struct LidShopPurchaseExec**entry_gate; /* Exact persistent gate's published cell. */
} LidShopPurchaseExec;
static LidShopPurchaseExec*g_lid_purchase_exec;
/* Real exec ABI. All referenced storage is resident before this address can
   be published. Before full publication/after retirement only pass through. */
static __attribute__((noinline)) void lid_pxe_getter(void*receiver,void*frame,void*result){
    LidShopPurchaseExec*h=__atomic_load_n(&g_lid_purchase_exec,__ATOMIC_ACQUIRE);
    if(!h||!h->metadata)return; /* unreachable after owned publication */
    if(__atomic_load_n(&h->published,__ATOMIC_ACQUIRE))
        lid_pui_exec(h->ui,h->metadata,receiver,frame,result);
    else ((M204Exec)(uintptr_t)h->metadata->windows->prepared.native.original)(receiver,frame,result);
}
static int lid_pxe_window(LidShopPurchaseExec*h){
    if(!h||!h->initialized||!h->dispatch)return 0;
    uint32_t tid=h->dispatch->tls.thread(h->dispatch->tls.context);
    if(!tid||__atomic_load_n(&h->owner,__ATOMIC_ACQUIRE)!=tid)return 0;
    /* Only the current owner may read request-local frame/prepared fields. */
    if(!h->frame||!h->bridge->live(h->bridge->provider))return 0;
    return lid_mnd_frame(h->dispatch)==h->frame&&h->frame->api&&h->frame->root;
}
static int lid_pxe_copy_raw(LidShopPurchaseExec*h,uintptr_t p,void*out,size_t n){
    return h&&h->raw.copy&&p&&p<=UINTPTR_MAX-n&&h->raw.copy(h->raw.context,(void*)p,out,n)==1;
}
static int lid_pxe_json_window(void*c,const void*object,int write){(void)object;return !write&&lid_pxe_window(c);}
static int lid_pxe_json_copy(void*c,const void*p,void*out,size_t n){
    return lid_pxe_window(c)&&lid_pxe_copy_raw(c,(uintptr_t)p,out,n);
}
static int lid_pxe_offer(void*c,const uint16_t*skill,unsigned units,unsigned*out){
    LidShopPurchaseExec*h=c;if(!out||!lid_mp_id(skill,units))return -1;
    for(unsigned i=0;i<h->ui->count;i++)if(lid_mp_equal(skill,units,h->ui->rows[i].skill,h->ui->rows[i].units)){
        *out=h->ui->rows[i].offer;return 1;
    }
    return 0;
}
static uint64_t lid_pxe_session(void*c){
    LidShopPurchaseExec*h=c;
    /* Process cookie only. Full run/config/catalogue identity remains in the
       immutable bridge and captured live provider, never inferred from birth. */
    return lid_pxe_window(h)?h->bridge->context.birth:0;
}
static uint64_t lid_pxe_thread(void*c){LidShopPurchaseExec*h=c;return h->dispatch->tls.thread(h->dispatch->tls.context);}
static int lid_pxe_worker(void*c){return lid_pxe_window(c);}
static int lid_pxe_admission(void*c){
    LidShopPurchaseExec*h=c;return lid_pxe_window(h)&&lid_pb_payment_held(h->bridge,h->current,(uint32_t)lid_pxe_thread(h));
}
static int lid_pxe_payment_copy(void*c,uintptr_t p,void*out,size_t n){
    return lid_pxe_window(c)&&lid_pxe_copy_raw(c,p,out,n);
}
static int lid_pxe_callsite(void*c,uintptr_t p,const unsigned char*original,unsigned size){
    LidShopPurchaseExec*h=c;return lid_momoka_hooks_callsite(h->dispatch->hooks,p,original,size);
}
static void lid_pxe_poison(LidShopPurchaseExec*h){
    __atomic_store_n(&h->poisoned,1,__ATOMIC_RELEASE);
    __atomic_store_n(&h->bridge->halted,1,__ATOMIC_RELEASE);
}
static void lid_pxe_uncertain(void*c,unsigned offer){
    LidShopPurchaseExec*h=c;lid_pxe_poison(h);
    uint32_t tid=(uint32_t)lid_pxe_thread(h);
    if(!tid||__atomic_load_n(&h->owner,__ATOMIC_ACQUIRE)!=tid)return;
    h->failed=1;
    for(unsigned i=0;i<h->request.ap_count;i++)if(h->rows[i].offer==offer&&h->tokens[i])
        lid_pb_request_fail(h->bridge,h->tokens[i]);
}
static int lid_pxe_receipt(void*c,const LidMPReceipt*r){
    LidShopPurchaseExec*h=c;
    if(!r||!lid_pxe_admission(h))return 0;
    for(unsigned i=0;i<h->request.ap_count;i++)if(h->tokens[i]==h->current&&
       h->rows[i].offer==r->offer&&h->rows[i].price==r->price){
        h->receipts[i]=*r;h->received[i]=1;return 1;
    }
    return 0;
}
/* No engine lock/file IO or UI wait. Native worker may yield at most16 times
   for a short metadata claim; exhaustion becomes actual native API rejection. */
static int lid_pxe_preflight(LidShopPurchaseExec*h){
    LidMomokaRequestOps lookup={h,lid_pxe_offer};
    int status=lid_momoka_request_copy(&h->json,(void*)h->frame->root,&lookup,&h->request);
    if(status!=LID_MP_MATCH)return status;
    for(unsigned i=0;i<h->request.ap_count;i++){
        int found=0;
        for(unsigned attempt=0;attempt<16;attempt++){
            uint64_t token=0;
            if(lid_pb_find(h->bridge,h->request.ids[i],h->request.expected[i].skill,
                           h->request.expected[i].units,&token)&&
               lid_pb_describe(h->bridge,token,&h->rows[i])){
                h->tokens[i]=token;found=1;break;
            }
            if(__atomic_load_n(&h->bridge->halted,__ATOMIC_ACQUIRE)||!lid_pxe_window(h))break;
            h->yield(0);
        }
        if(!found)return LID_MP_INVALID;
    }
    LidMPOps ops={h,lid_pxe_session,lid_pxe_thread,lid_pxe_worker,lid_pxe_admission,
        lid_pxe_payment_copy,lid_pxe_receipt,lid_pxe_uncertain,lid_pxe_callsite};
    return lid_momoka_payment_prepare(&h->payment,h->dispatch->hooks->base,1,&ops,
                                     h->rows,h->request.ap_count)?LID_MP_MATCH:LID_MP_INVALID;
}
static void lid_pxe_cleanup(LidShopPurchaseExec*h,unsigned completed,unsigned char result){
    if(h->prepared&&h->payment.active>=0)(void)lid_momoka_payment_abort(&h->payment);
    unsigned n=h->request.ready?h->request.ap_count:0;if(n>128)n=0;
    for(unsigned i=0;i<n;i++)if(h->tokens[i]){
        if(h->adopted[i]&&h->received[i]&&h->receipts[i].paid&&completed&&result==1){
            if(!lid_pb_finish(h->bridge,h->tokens[i],&h->receipts[i],result))
                lid_pb_request_fail(h->bridge,h->tokens[i]);
        }else lid_pb_request_fail(h->bridge,h->tokens[i]);
    }
    h->prepared=0;h->current=0;h->frame=0;h->request.ready=0;
    __atomic_store_n(&h->owner,0,__ATOMIC_RELEASE);
}
static unsigned char lid_pxe_body(void*api,void*response,void*root){
    LidShopPurchaseExec*h=__atomic_load_n(&g_lid_purchase_exec,__ATOMIC_ACQUIRE);
    if(!h)return 0; /* Forbidden after publication. */
    LidMomokaDispatchFrame*f=lid_mnd_frame(h->dispatch);uint32_t tid=(uint32_t)lid_pxe_thread(h),empty=0;
    unsigned incoming=h->dispatch->tls.error_get(h->dispatch->tls.context);
    if(!f||f->api!=(uintptr_t)api||f->root!=(uintptr_t)root||!tid||tid==h->bridge->ui||tid==h->bridge->io||
       !__atomic_compare_exchange_n(&h->owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
        lid_pxe_poison(h);h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);
        return h->reject(api,response); /* Completes native error response, not a bare false. */
    }
    volatile unsigned completed=0;unsigned char result=0;
#if LID_SHOP_EXEC_SEH_ENABLED
    __try {
#else
    {
#endif
        h->frame=f;h->prepared=h->failed=0;h->current=0;
        __builtin_memset(&h->request,0,sizeof(h->request));__builtin_memset(&h->payment,0,sizeof(h->payment));
        __builtin_memset(h->tokens,0,sizeof(h->tokens));__builtin_memset(h->adopted,0,sizeof(h->adopted));
        __builtin_memset(h->received,0,sizeof(h->received));
        int status=lid_pxe_preflight(h);
        h->prepared=status==LID_MP_MATCH;
        h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);
        if(status==LID_MP_MATCH||status==LID_MP_NATIVE||!h->request.ready){
            /* Missing/malformed history alone is not proof of an AP buy.
               Grant/debit wrappers still refuse any unprepared AP carrier. */
            result=h->original(api,response,root);
        }else{
            lid_pxe_poison(h);result=h->reject(api,response);
        }
        completed=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    } __finally {
#else
    } {
#endif
        unsigned error=h->dispatch->tls.error_get(h->dispatch->tls.context);
        lid_pxe_cleanup(h,completed,result);
        h->dispatch->tls.error_set(h->dispatch->tls.context,error);
    }
    return result;
}
/* Borrowed native CALL argument lifetime only, even for an unsupported outer
   invocation. Does not inspect or retain an unowned JSON/API/root pointer. */
static int lid_pxe_grant_id(LidShopPurchaseExec*h,uintptr_t list,uint16_t skill[129],unsigned*units){
    struct {uintptr_t data;int32_t num,capacity;} a,s;
    if(!units||!lid_pxe_copy_raw(h,list,&a,16)||a.num!=1||a.capacity<1||a.capacity>4096||
       !lid_pxe_copy_raw(h,a.data,&s,16)||s.num<2||s.num>129||s.capacity<s.num||s.capacity>1048576||
       !lid_pxe_copy_raw(h,s.data,skill,(size_t)s.num*2)||!lid_mp_id(skill,(unsigned)s.num-1))return 0;
    *units=(unsigned)s.num-1;return 1;
}
static void lid_pxe_grant_at(LidShopPurchaseExec*h,void*account,void*list,unsigned char flag,uintptr_t caller){
    unsigned incoming=h->dispatch->tls.error_get(h->dispatch->tls.context);uint16_t skill[129];unsigned units=0,offer=0;
    if(caller!=h->dispatch->hooks->base+0x1157418||
       (uintptr_t)account!=h->dispatch->hooks->base+0xf8b5088||flag!=1){
        h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);h->dispatch->grant(account,list,flag);return;
    }
    if(__atomic_load_n(&h->poisoned,__ATOMIC_ACQUIRE)||!lid_pxe_grant_id(h,(uintptr_t)list,skill,&units)){
        lid_pxe_poison(h);goto suppressed;
    }
    if(lid_pxe_offer(h,skill,units,&offer)==0){
        h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);h->dispatch->grant(account,list,flag);return;
    }
    if(!lid_pxe_window(h)||!h->prepared){lid_pxe_poison(h);goto suppressed;}
    for(unsigned i=0;i<h->request.ap_count;i++)if(h->rows[i].offer==offer&&
       lid_mp_equal(skill,units,h->rows[i].skill,h->rows[i].units)){
        LidMPRow current={0};int begun=0;
        for(unsigned attempt=0;attempt<16;attempt++){
            if(lid_pb_begin(h->bridge,h->tokens[i],&current)){begun=1;break;}
            if(__atomic_load_n(&h->bridge->halted,__ATOMIC_ACQUIRE)||!lid_pxe_window(h))break;
            h->yield(0);
        }
        if(begun&&current.offer==h->rows[i].offer&&current.price==h->rows[i].price&&
           lid_mp_equal(current.skill,current.units,h->rows[i].skill,h->rows[i].units)){
            h->adopted[i]=1;h->current=h->tokens[i];
            if(lid_momoka_payment_grant(&h->payment,caller,(uintptr_t)account,(uintptr_t)list,flag)==LID_MP_MATCH)
                goto suppressed;
        }
        lid_pxe_uncertain(h,offer);goto suppressed;
    }
    lid_pxe_poison(h);
suppressed:h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);
}
static unsigned char lid_pxe_debit_at(LidShopPurchaseExec*h,void*account,int amount,
    const uint16_t*reason,const uint16_t*detail,uintptr_t caller){
    unsigned incoming=h->dispatch->tls.error_get(h->dispatch->tls.context);
    if(caller!=h->dispatch->hooks->base+0x115745b){
        h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);return h->dispatch->debit(account,amount,reason,detail);
    }
    if(__atomic_load_n(&h->poisoned,__ATOMIC_ACQUIRE)){
        h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);return 0;
    }
    if(!lid_pxe_window(h)||!h->prepared||h->payment.active<0){
        h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);return h->dispatch->debit(account,amount,reason,detail);
    }
    if(lid_momoka_payment_before_debit(&h->payment,caller,(uintptr_t)account,amount,(uintptr_t)reason)!=LID_MP_MATCH){
        lid_pxe_poison(h);h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);return 0;
    }
    h->dispatch->tls.error_set(h->dispatch->tls.context,incoming);
    unsigned char result=h->dispatch->debit(account,amount,reason,detail); /* EXACTLY ONCE */
    unsigned returned=h->dispatch->tls.error_get(h->dispatch->tls.context);
    if(lid_momoka_payment_after_debit(&h->payment,result)!=LID_MP_MATCH){
        (void)lid_momoka_payment_abort(&h->payment);lid_pxe_poison(h);
    }
    h->current=0;h->dispatch->tls.error_set(h->dispatch->tls.context,returned);return result;
}
static __attribute__((noinline)) unsigned char lid_pxe_handler(void*api,void*response,void*root){
    LidShopPurchaseExec*h=__atomic_load_n(&g_lid_purchase_exec,__ATOMIC_ACQUIRE);
    if(!h)return 0;return lid_mnd_handler(h->dispatch,api,response,root,(uintptr_t)__builtin_return_address(0));
}
static __attribute__((noinline)) void lid_pxe_grant(void*account,void*list,unsigned char flag){
    LidShopPurchaseExec*h=__atomic_load_n(&g_lid_purchase_exec,__ATOMIC_ACQUIRE);
    if(h)lid_pxe_grant_at(h,account,list,flag,(uintptr_t)__builtin_return_address(0));
}
static __attribute__((noinline)) unsigned char lid_pxe_debit(void*account,int amount,const uint16_t*reason,const uint16_t*detail){
    LidShopPurchaseExec*h=__atomic_load_n(&g_lid_purchase_exec,__ATOMIC_ACQUIRE);
    return h?lid_pxe_debit_at(h,account,amount,reason,detail,(uintptr_t)__builtin_return_address(0)):0;
}
/* BEFORE owned hook publication only. source raw.copy must be the real bounded
   read provider, not a stale Save-window callback. json is exact-profile native
   JSON API only; no marker setter/Save used. Runtime supplies actual resolved
   OS __C_specific_handler; existing validated proxy initializer must accept it.
   Fixed singleton process context cannot be reused for a new run after publish. */
static int lid_pxe_prepare(LidShopPurchaseExec*h,LidMomokaDispatch*d,LidShopPurchaseUi*ui,
    const LidMPPriceCollector*metadata,
    const LidShopMarkerNative*json,const LidShopMarkerAccess*raw,void(*yield)(uint32_t),void*language_handler){
#if !LID_SHOP_EXEC_SEH_ENABLED || !LID_MOMOKA_DISPATCH_SEH_ENABLED
    (void)h;(void)d;(void)ui;(void)metadata;(void)json;(void)raw;(void)yield;(void)language_handler;return 0;
#else
    uintptr_t reject=0;LidShopPurchaseExec*empty=0;
    if(!h||h->initialized||!d||!d->initialized||d->observer||d->hooks->state!=LID_MH_PREPARED||
       __atomic_load_n(&d->active,__ATOMIC_ACQUIRE)||!ui||!ui->initialized||!ui->bridge||
       !ui->root||!ui->root->current_game||
       !metadata||!metadata->ready||!metadata->windows||ui->binding!=&metadata->binding||
       ui->root->memory.copy!=metadata->memory.copy||
       ui->root->memory.name_is!=metadata->memory.name_is||
       ui->root->memory.context!=metadata->memory.context||
       (!metadata->entry&&metadata->wrapper!=(uintptr_t)lid_pxe_getter)||
       (metadata->entry&&!lid_mpc_owned(&metadata->memory,metadata->windows,metadata->entry,metadata->wrapper))||
       metadata->owner!=ui->bridge->ui||
       !metadata->windows->prepared.native.original||!metadata->windows->prepared.native.slot||
       !json||!json->initialized||json->base!=d->hooks->base||!json->object||!json->string||!json->release||
       !raw||!raw->copy||!yield||!language_handler||d->handler!=(void*)d->hooks->handler_original||
       d->hooks->handler_wrapper!=(uintptr_t)lid_pxe_handler||d->hooks->calls[0].wrapper!=(uintptr_t)lid_pxe_grant||
       d->hooks->calls[1].wrapper!=(uintptr_t)lid_pxe_debit||
       !lid_mh_read(d->hooks,d->hooks->base+0x2112e18,&reject,8)||reject!=d->hooks->base+0x10fd3b0||
       !lid_master_scan_guard_init(language_handler))return 0;
    h->dispatch=d;h->ui=ui;h->bridge=ui->bridge;h->metadata=metadata;h->json=*json;h->raw=*raw;h->yield=yield;
    h->original=(void*)d->hooks->handler_original;h->reject=(void*)reject;
    h->json.access=(LidShopMarkerAccess){h,lid_pxe_json_window,lid_pxe_json_copy};h->initialized=1;
    if(!__atomic_compare_exchange_n(&g_lid_purchase_exec,&empty,h,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    d->handler=lid_pxe_body;return 1;
#endif
}
/* No allocations, file IO, game calls or cross-tick engine lock. BUSY is safe
   to retry only before mutation. A failed attempt is terminal and resident.
   Platform providers are the existing real Windows adapters, never a fabricated
   quiescent bool. Unknown direct native callers remain outside this profile. */
static int lid_pxe_activate(LidShopPurchaseExec*h,LidMomokaInstallBinding*b,LidMomokaHooks*k){
    if(!h||!h->initialized||!b||!b->initialized||!k||h->dispatch->hooks!=k||h->retired||
       __atomic_load_n(&h->published,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&h->poisoned,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&g_lid_purchase_exec,__ATOMIC_ACQUIRE)!=h)return LID_MI_INVALID;
    const LidMPPriceCollector*m=h->metadata;
    if(m->entry&&(!h->entry_gate||__atomic_load_n(h->entry_gate,__ATOMIC_ACQUIRE)!=h))return LID_MI_INVALID;
    if(k->state!=LID_MH_PREPARED||k->ops.context!=b||k->ops.quiescent!=lid_mib_quiet||
       b->active_handlers!=&h->dispatch->active||b->guard.base!=k->base||
       b->guard.writer!=m->owner||b->guard.writer!=h->bridge->ui||
       !h->bridge->live(h->bridge->provider)||!m->live(m->run_context))return LID_MI_INVALID;
    int admitted=lid_momoka_install_begin(&b->guard);
    if(admitted!=LID_MI_HELD)return admitted;
    unsigned old=0,ignored=0;int writable=0,ui_owned=0,good=0;uintptr_t current=0;
    uintptr_t slot=m->windows->prepared.native.slot,original=m->windows->prepared.native.original;
    if(!lid_mib_quiet(b)||!lid_mh_read(k,slot,&current,8))goto done;
    if(m->entry){
        /* Existing persistent guard remains the ONLY getter owner. It blocks
           buys until this full execution graph is published, and after failure.
           No slot write/protection change and no borrowed-owner restoration. */
        if(current!=m->wrapper||!lid_mpc_owned(&m->memory,m->windows,m->entry,m->wrapper)||
           !k->ops.executable(k->ops.context,(uintptr_t)m->wrapper))goto done;
    }else{
        if(current!=original||!k->ops.executable(k->ops.context,(uintptr_t)lid_pxe_getter))goto done;
        if(!k->ops.protect(k->ops.context,(void*)slot,8,0x04,&old))goto done;
        writable=1;
        ui_owned=m207_publish(&h->getter,(uintptr_t*)slot,original,(uintptr_t)lid_pxe_getter);
        if(!k->ops.protect(k->ops.context,(void*)slot,8,old,&ignored))goto done;
        writable=0;
        if(!ui_owned)goto done;
    }
    if(!lid_momoka_hooks_activate(k))goto done;
    __atomic_store_n(&h->published,1,__ATOMIC_RELEASE);good=1;
done:
    if(!good){
        h->retired=1;lid_pxe_poison(h);
        if(k->state==LID_MH_ACTIVE)(void)lid_momoka_hooks_restore(k);
        if(ui_owned){
            unsigned rollback_old=0;
            /* Re-establish write access even after a failed restore call;
               never infer the current page state from an earlier success. */
            if(k->ops.protect(k->ops.context,(void*)slot,8,0x04,&rollback_old)){
                (void)m207_restore(&h->getter);
                (void)k->ops.protect(k->ops.context,(void*)slot,8,old,&ignored);
                writable=0;
            }
        }
        if(writable)(void)k->ops.protect(k->ops.context,(void*)slot,8,old,&ignored);
    }
    int released=lid_momoka_install_end(&b->guard);
    if(!released){h->retired=1;lid_pxe_poison(h);}
    return good&&released?LID_MI_HELD:LID_MI_INVALID;
}
#endif
