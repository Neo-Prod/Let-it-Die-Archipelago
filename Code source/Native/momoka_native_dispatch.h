#ifndef LID_MOMOKA_NATIVE_DISPATCH_H
#define LID_MOMOKA_NATIVE_DISPATCH_H
#include "momoka_native_hooks.h"
#ifndef LID_MOMOKA_DISPATCH_SEH_ENABLED
#define LID_MOMOKA_DISPATCH_SEH_ENABLED 0
#endif
/* Forward-only dispatcher. No AP grant suppression/payment is enabled here.
   Own one OS TLS slot for the entire resident lifetime; never free/reassign it
   while wrappers may execute. All callbacks and original pointers are immutable
   after publication. Counter is diagnostic, NOT proof of worker quiescence. */
typedef struct LidMomokaDispatch LidMomokaDispatch;
typedef struct LidMomokaDispatchFrame {
    const LidMomokaDispatch*dispatch;struct LidMomokaDispatchFrame*previous;
    uintptr_t api,root;uint32_t owner;unsigned live;
} LidMomokaDispatchFrame;
typedef struct LidMomokaTlsOps {
    void*context;void*(*get)(void*,unsigned);int(*set)(void*,unsigned,void*);
    uint32_t(*thread)(void*);unsigned(*error_get)(void*);void(*error_set)(void*,unsigned);
} LidMomokaTlsOps;
struct LidMomokaDispatch {
    const LidMomokaHooks*hooks;LidMomokaTlsOps tls;unsigned slot,initialized,active,tls_fault;
    LidMomokaDispatchFrame frames[64]; /* resident even if TLS restoration fails */
    unsigned char(*handler)(void*,void*,void*);
    void(*grant)(void*,void*,unsigned char);
    unsigned char(*debit)(void*,int,const uint16_t*,const uint16_t*);
    void*observer_context;
    /* Metadata/read-only observation:1=enter,2=leave while frame is live.
       It never authorizes AP mutations or changes native dispatch/results. */
    void(*observer)(void*,const LidMomokaDispatch*,LidMomokaDispatchFrame*,unsigned,unsigned char);
};
static int lid_mnd_prepare(LidMomokaDispatch*d,const LidMomokaHooks*h,unsigned slot,const LidMomokaTlsOps*o){
    if(!d||d->initialized||!h||h->state!=LID_MH_PREPARED||slot==~0u||!o||!o->get||!o->set||!o->thread||!o->error_get||!o->error_set)return 0;
    d->hooks=h;d->tls=*o;d->slot=slot;d->handler=(void*)h->handler_original;
    d->grant=(void*)h->calls[0].original_target;d->debit=(void*)h->calls[1].original_target;
    d->initialized=1;return 1;
}
static int lid_mnd_set_observer(LidMomokaDispatch*d,void*context,
    void(*observer)(void*,const LidMomokaDispatch*,LidMomokaDispatchFrame*,unsigned,unsigned char)){
    if(!d||!d->initialized||d->hooks->state!=LID_MH_PREPARED||d->observer||!observer||
       __atomic_load_n(&d->active,__ATOMIC_ACQUIRE))return 0;
    d->observer_context=context;d->observer=observer;return 1;
}
/* Only our exact installed slot is accepted; no generic signature bypass. */
static int lid_mnd_origin(const LidMomokaDispatch*d,uintptr_t api,uintptr_t caller){
    static const unsigned char thunk[10]={0x48,0x8b,0x01,0x48,0xff,0xa0,0x80,0,0,0};
    const LidMomokaHooks*h=d->hooks;uintptr_t vt,p;unsigned char b[10];
    return h->state==LID_MH_ACTIVE&&caller==h->base+0x10f4fde&&
      lid_mh_read(h,api,&vt,8)&&vt==h->base+0x2112db0&&
      lid_mh_read(h,vt+0x18,&p,8)&&p==h->base+0x110d570&&
      lid_mh_read(h,vt+0x80,&p,8)&&p==h->handler_wrapper&&
      lid_mh_read(h,h->base+0x10f4fdb,b,3)&&b[0]==0xff&&b[1]==0x50&&b[2]==0x18&&
      lid_mh_read(h,h->base+0x110d570,b,10)&&lid_mh_same(b,thunk,10);
}
static LidMomokaDispatchFrame*lid_mnd_frame(const LidMomokaDispatch*d){
    if(!d||!d->initialized)return 0;
    unsigned error=d->tls.error_get(d->tls.context);
    LidMomokaDispatchFrame*f=d->tls.get(d->tls.context,d->slot);
    if(f&&(f->dispatch!=d||!__atomic_load_n(&f->live,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&f->owner,__ATOMIC_ACQUIRE)!=d->tls.thread(d->tls.context)))f=0;
    d->tls.error_set(d->tls.context,error);return f;
}
/* Caller wrapper supplies its own return address, not this helper's return.
   On unsupported origin/TLS failure the native operation still runs once.
   Native C++ exception unwinding is not handled: do not enable AP mutations
   until cleanup on abnormal exits has a separately verified implementation. */
#if LID_MOMOKA_DISPATCH_SEH_ENABLED
/* Compiler-generated unwind only. Existing language-handler delegate must be
   initialized before any publication; no filter/swallow or intentional raise.
   Observers must remain nonthrowing; stage3 means abnormal exit, not success.
   Static/COFF evidence does NOT constitute runtime unwind validation. */
static unsigned char lid_mnd_handler(LidMomokaDispatch*d,void*api,void*response,void*root,uintptr_t caller){
    unsigned incoming=d->tls.error_get(d->tls.context);unsigned char result=0;
    volatile unsigned completed=0,pushed=0;LidMomokaDispatchFrame*volatile frame=0;
    __atomic_add_fetch(&d->active,1,__ATOMIC_ACQ_REL);
    __try {
        if(!__atomic_load_n(&d->tls_fault,__ATOMIC_ACQUIRE)&&lid_mnd_origin(d,(uintptr_t)api,caller)){
            uint32_t tid=d->tls.thread(d->tls.context);
            if(tid)for(unsigned i=0;i<64;i++){
                uint32_t empty=0;
                if(__atomic_compare_exchange_n(&d->frames[i].owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
                    frame=&d->frames[i];break;
                }
            }
            if(frame){
                frame->dispatch=d;frame->api=(uintptr_t)api;frame->root=(uintptr_t)root;
                frame->previous=d->tls.get(d->tls.context,d->slot);
                __atomic_store_n(&frame->live,1,__ATOMIC_RELEASE);
                pushed=d->tls.set(d->tls.context,d->slot,frame)==1;
                if(!pushed){
                    frame->api=frame->root=0;__atomic_store_n(&frame->live,0,__ATOMIC_RELEASE);
                    __atomic_store_n(&frame->owner,0,__ATOMIC_RELEASE);frame=0;
                }
            }
        }
        if(pushed&&d->observer)d->observer(d->observer_context,d,frame,1,0);
        d->tls.error_set(d->tls.context,incoming);
        result=d->handler(api,response,root);completed=1;
    } __finally {
        unsigned returned_error=d->tls.error_get(d->tls.context);
        if(pushed){
            if(d->observer)d->observer(d->observer_context,d,frame,completed?2:3,result);
            int restored=d->tls.set(d->tls.context,d->slot,frame->previous)==1;
            frame->api=frame->root=0;__atomic_store_n(&frame->live,0,__ATOMIC_RELEASE);
            if(restored)__atomic_store_n(&frame->owner,0,__ATOMIC_RELEASE);
            else __atomic_store_n(&d->tls_fault,1,__ATOMIC_RELEASE);
        }
        __atomic_sub_fetch(&d->active,1,__ATOMIC_ACQ_REL);d->tls.error_set(d->tls.context,returned_error);
    }
    return result;
}
#else
static unsigned char lid_mnd_handler(LidMomokaDispatch*d,void*api,void*response,void*root,uintptr_t caller){
    unsigned error=d->tls.error_get(d->tls.context);int pushed=0;
    LidMomokaDispatchFrame*f=0;
    __atomic_add_fetch(&d->active,1,__ATOMIC_ACQ_REL);
    if(!__atomic_load_n(&d->tls_fault,__ATOMIC_ACQUIRE)&&lid_mnd_origin(d,(uintptr_t)api,caller)){
        uint32_t tid=d->tls.thread(d->tls.context);
        if(tid)for(unsigned i=0;i<64;i++){
            uint32_t empty=0;
            if(__atomic_compare_exchange_n(&d->frames[i].owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){f=&d->frames[i];break;}
        }
        if(f){f->dispatch=d;f->api=(uintptr_t)api;f->root=(uintptr_t)root;f->previous=d->tls.get(d->tls.context,d->slot);
            __atomic_store_n(&f->live,1,__ATOMIC_RELEASE);
            pushed=d->tls.set(d->tls.context,d->slot,f)==1;
            if(!pushed){f->api=f->root=0;__atomic_store_n(&f->live,0,__ATOMIC_RELEASE);__atomic_store_n(&f->owner,0,__ATOMIC_RELEASE);}
        }
    }
    if(pushed&&d->observer)d->observer(d->observer_context,d,f,1,0);
    d->tls.error_set(d->tls.context,error);
    unsigned char result=d->handler(api,response,root);
    error=d->tls.error_get(d->tls.context);
    if(pushed){
        if(d->observer)d->observer(d->observer_context,d,f,2,result);
        int restored=d->tls.set(d->tls.context,d->slot,f->previous)==1;
        f->api=f->root=0;
        __atomic_store_n(&f->live,0,__ATOMIC_RELEASE);
        if(restored)__atomic_store_n(&f->owner,0,__ATOMIC_RELEASE);
        else __atomic_store_n(&d->tls_fault,1,__ATOMIC_RELEASE);
    }
    __atomic_sub_fetch(&d->active,1,__ATOMIC_ACQ_REL);
    d->tls.error_set(d->tls.context,error);return result;
}
#endif
static void lid_mnd_grant(LidMomokaDispatch*d,void*a,void*list,unsigned char flag){d->grant(a,list,flag);}
static unsigned char lid_mnd_debit(LidMomokaDispatch*d,void*a,int amount,const uint16_t*reason,const uint16_t*detail){
    return d->debit(a,amount,reason,detail);
}
#endif
