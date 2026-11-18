#ifndef LID_SHOP_MOMOKA_WORKER_H
#define LID_SHOP_MOMOKA_WORKER_H
#include "momoka_native_dispatch.h"
#include "shop_momoka_request.h"
/* Read-only worker binding. No host begin/claim, debit, suppression, save or
   network call. Scratch and run context remain resident. Native handlers are
   always invoked unchanged by dispatcher, even if this observer refuses.
   Observation refusal is NOT a future AP transaction fallback policy. */
typedef struct LidMomokaWorker {
    LidMomokaDispatch*dispatch;const LidMomokaRequestCatalogue*catalogue;
    LidShopMarkerNative json;LidShopMarkerAccess source;
    LidMomokaRequest request;uint64_t session;uint32_t owner;
    unsigned initialized,seen,completed,busy,refused;
    int last_status;unsigned last_count,last_ap_count;unsigned char last_native_result;
    /* Borrowed frame reference only during owned observation, cleared at leave. */
    LidMomokaDispatchFrame*frame;
} LidMomokaWorker;
static int lid_mw_window(void*context,const void*object,int write){
    LidMomokaWorker*w=context;(void)object;
    if(!w||!w->initialized||write||!w->catalogue||!w->catalogue->ready||
       !lid_hm_live(w->catalogue->host)||w->session!=w->catalogue->host->session)return 0;
    LidMomokaDispatch*d=w->dispatch;
    if(__atomic_load_n(&w->owner,__ATOMIC_ACQUIRE)!=d->tls.thread(d->tls.context))return 0;
    LidMomokaDispatchFrame*f=lid_mnd_frame(d);
    return f&&f==w->frame&&f->api&&f->root;
}
static int lid_mw_copy(void*context,const void*source,void*out,size_t n){
    LidMomokaWorker*w=context;return lid_mw_window(w,0,0)&&w->source.copy(w->source.context,source,out,n)==1;
}
static unsigned lid_momoka_worker_role(const LidMomokaWorker*w){
    /* 0 outside worker; UI/SAVE classification must be supplied explicitly by
       those owners. Do not infer UI from being the game's main thread. */
    return w&&lid_mw_window((void*)w,0,0)?LID_HM_REQUEST:0;
}
static void lid_mw_observe(void*context,const LidMomokaDispatch*d,LidMomokaDispatchFrame*f,
                           unsigned stage,unsigned char native_result){
    LidMomokaWorker*w=context;uint32_t tid=d->tls.thread(d->tls.context);
    if(!w||!w->initialized||w->dispatch!=d||!f||f->dispatch!=d||!tid)return;
    if(stage==1){
        uint32_t empty=0;
        if(!__atomic_compare_exchange_n(&w->owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
            __atomic_add_fetch(&w->busy,1,__ATOMIC_RELAXED);return;
        }
        w->frame=f;w->request.ready=0;__atomic_add_fetch(&w->seen,1,__ATOMIC_RELAXED);
        LidMomokaRequestOps ops={(void*)w->catalogue,lid_momoka_catalogue_offer};
        w->last_status=lid_momoka_request_copy(&w->json,(void*)f->root,&ops,&w->request);
        w->last_count=w->request.ready?w->request.count:0;w->last_ap_count=w->request.ready?w->request.ap_count:0;
        if(w->last_status==LID_MP_INVALID)__atomic_add_fetch(&w->refused,1,__ATOMIC_RELAXED);
    }else if(stage==2&&__atomic_load_n(&w->owner,__ATOMIC_ACQUIRE)==tid&&w->frame==f){
        w->last_native_result=native_result;w->request.ready=0;w->frame=0;
        __atomic_add_fetch(&w->completed,1,__ATOMIC_RELAXED);
        __atomic_store_n(&w->owner,0,__ATOMIC_RELEASE);
    }
}
/* Called before publication and hook activation. Reuse validated immutable JSON
   function pointers, but replace its access window with concrete TLS ownership.
   c/host/JSON binding must refer to the same old run and exact game profile. */
static int lid_momoka_worker_prepare(LidMomokaWorker*w,LidMomokaDispatch*d,
                        const LidMomokaRequestCatalogue*c,const LidShopMarkerNative*b){
    if(!w||w->initialized||!d||!d->initialized||d->hooks->state!=LID_MH_PREPARED||
       !c||!c->ready||c->refused||!lid_hm_live(c->host)||c->session!=c->host->session||
       !lid_sm_same(c->identity,c->host->identity,96)||!b||!b->initialized||b->base!=d->hooks->base||
       !b->access.copy||!b->object||!b->string||!b->release)return 0;
    w->dispatch=d;w->catalogue=c;w->source=b->access;w->json=*b;w->session=c->session;
    w->json.access=(LidShopMarkerAccess){w,lid_mw_window,lid_mw_copy};w->initialized=1;
    if(!lid_mnd_set_observer(d,w,lid_mw_observe)){w->initialized=0;return 0;}return 1;
}
#endif
