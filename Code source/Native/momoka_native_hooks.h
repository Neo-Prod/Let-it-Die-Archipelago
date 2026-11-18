#ifndef LID_MOMOKA_NATIVE_HOOKS_H
#define LID_MOMOKA_NATIVE_HOOKS_H
#include <stdint.h>
#include <stddef.h>
/* Only two Momoka CALLs plus its API handler slot. No shared-function prologue
   rewrite or trampoline relocation. Caller must hold actual request-worker
   quiescence throughout activation/restore: 5-byte code writes are NOT atomic.
   No automatic install. Published code/context/relays remain resident. */
enum {LID_MH_EMPTY,LID_MH_PREPARED,LID_MH_ACTIVE,LID_MH_RETIRED,LID_MH_POISONED};
typedef struct LidMomokaHookOps {
    void*context;int(*read)(void*,uintptr_t,void*,size_t);
    int(*quiescent)(void*);
    void*(*near_alloc)(void*,uintptr_t,size_t); /* NEW disjoint writable allocation */
    int(*protect)(void*,void*,size_t,unsigned,unsigned*);
    int(*flush)(void*,void*,size_t);
    int(*executable)(void*,uintptr_t);
} LidMomokaHookOps;
typedef struct LidMomokaCallHook {
    uintptr_t site,original_target,wrapper,relay;unsigned touched,protection_saved,original_protection;
    unsigned char original[5],patched[5],relay_bytes[14];
} LidMomokaCallHook;
typedef struct LidMomokaHooks {
    unsigned state,ever_published,slot_touched,slot_protection;uintptr_t base,slot,handler_original,handler_wrapper;
    LidMomokaCallHook calls[2];LidMomokaHookOps ops;
} LidMomokaHooks;
static int lid_mh_same(const void*a,const void*b,size_t n){
    const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static int lid_mh_read(const LidMomokaHooks*h,uintptr_t p,void*out,size_t n){
    return h&&h->ops.read&&p&&p<=UINTPTR_MAX-n&&h->ops.read(h->ops.context,p,out,n)==1;
}
static int lid_mh_rel32(uintptr_t site,uintptr_t to,unsigned char*out){
    if(!site||site>INT64_MAX-5||!to||to>INT64_MAX)return 0;
    int64_t delta=(int64_t)to-(int64_t)(site+5);if(delta<INT32_MIN||delta>INT32_MAX)return 0;
    int32_t relative=(int32_t)delta;out[0]=0xe8;__builtin_memcpy(out+1,&relative,4);return 1;
}
static void lid_mh_jump(unsigned char*out,uintptr_t wrapper){
    out[0]=0xff;out[1]=0x25;out[2]=out[3]=out[4]=out[5]=0;__builtin_memcpy(out+6,&wrapper,8);
}
/* Prepare a private zeroed set. Relays only are written. If prepare fails,
   retain any allocations: this one-shot set is refused, never reused blindly. */
static int lid_momoka_hooks_prepare(LidMomokaHooks*h,uintptr_t base,int exact_sha,
    uintptr_t handler,uintptr_t grant,uintptr_t debit,const LidMomokaHookOps*ops){
    static const unsigned offsets[2]={0x1157413,0x1157456},targets[2]={0x134b210,0x1332ef0};
    static const unsigned char expected[2][5]={{0xe8,0xf8,0x3d,0x1f,0},{0xe8,0x95,0xba,0x1d,0}};
    uintptr_t current;unsigned char bytes[5];unsigned old;
    if(!h||h->state||h->base||!base||base>UINTPTR_MAX-0x2112e38||exact_sha!=1||!handler||!grant||!debit||
       !ops||!ops->read||!ops->quiescent||!ops->near_alloc||!ops->protect||!ops->flush||!ops->executable)return 0;
    h->base=base;h->ops=*ops;h->slot=base+0x2112e30;h->handler_original=base+0x1156d30;h->handler_wrapper=handler;
    if((h->slot&7)||!lid_mh_read(h,h->slot,&current,8)||current!=h->handler_original||
       !ops->executable(ops->context,handler)||!ops->executable(ops->context,grant)||!ops->executable(ops->context,debit))goto fail;
    for(unsigned i=0;i<2;i++){
        LidMomokaCallHook*c=&h->calls[i];c->site=base+offsets[i];c->original_target=base+targets[i];c->wrapper=i?debit:grant;
        if(!lid_mh_read(h,c->site,bytes,5)||!lid_mh_same(bytes,expected[i],5)||
           !ops->executable(ops->context,c->original_target))goto fail;
        __builtin_memcpy(c->original,bytes,5);
    }
    /* Validate all originals before allocating/writing any relay. */
    for(unsigned i=0;i<2;i++){
        LidMomokaCallHook*c=&h->calls[i];c->relay=(uintptr_t)ops->near_alloc(ops->context,c->site,14);
        if(!c->relay||c->relay==c->site||!lid_mh_rel32(c->site,c->relay,c->patched))goto fail;
        lid_mh_jump(c->relay_bytes,c->wrapper);__builtin_memcpy((void*)c->relay,c->relay_bytes,14);
        if(!ops->protect(ops->context,(void*)c->relay,14,0x20,&old)||
           !ops->flush(ops->context,(void*)c->relay,14)||!ops->executable(ops->context,c->relay))goto fail;
    }
    h->state=LID_MH_PREPARED;return 1;
fail:h->state=LID_MH_POISONED;return 0;
}
static int lid_mh_call_owned(const LidMomokaHooks*h,const LidMomokaCallHook*c){
    unsigned char call[5],relay[14];return lid_mh_read(h,c->site,call,5)&&lid_mh_same(call,c->patched,5)&&
      lid_mh_read(h,c->relay,relay,14)&&lid_mh_same(relay,c->relay_bytes,14);
}
/* Restore only cells/bytes still owned by this set. Never free published code
   and never overwrite a third-party patch, even during a failed activation. */
static int lid_momoka_hooks_restore(LidMomokaHooks*h){
    unsigned old,ignored;uintptr_t current;int good=1;
    if(!h||!h->ops.quiescent||h->ops.quiescent(h->ops.context)!=1){if(h)h->state=LID_MH_POISONED;return 0;}
    if(h->slot_touched){
        if(!lid_mh_read(h,h->slot,&current,8)||current!=h->handler_wrapper||
           !h->ops.protect(h->ops.context,(void*)h->slot,8,0x04,&old))good=0;
        else{
            uintptr_t expected=h->handler_wrapper;
            if(!__atomic_compare_exchange_n((uintptr_t*)h->slot,&expected,h->handler_original,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))good=0;
            else h->slot_touched=0;
            if(!h->ops.protect(h->ops.context,(void*)h->slot,8,h->slot_protection,&ignored))good=0;
        }
    }
    for(unsigned i=2;i>0;i--){LidMomokaCallHook*c=&h->calls[i-1];
        if(!c->touched)continue;
        if(!lid_mh_call_owned(h,c)||!h->ops.protect(h->ops.context,(void*)c->site,5,0x40,&old)){good=0;continue;}
        __builtin_memcpy((void*)c->site,c->original,5);c->touched=0;
        if(!h->ops.flush(h->ops.context,(void*)c->site,5))good=0;
        if(!h->ops.protect(h->ops.context,(void*)c->site,5,c->protection_saved?c->original_protection:old,&ignored))good=0;
    }
    h->state=good?LID_MH_RETIRED:LID_MH_POISONED;return good;
}
/* Caller publishes immutable original pointers/context to every wrapper BEFORE
   this call. Even a partially installed wrapper must be able to pass through. */
static int lid_momoka_hooks_activate(LidMomokaHooks*h){
    unsigned old,ignored;unsigned char bytes[5];uintptr_t current;
    if(!h||h->state!=LID_MH_PREPARED||h->ops.quiescent(h->ops.context)!=1)return 0;
    for(unsigned i=0;i<2;i++){
        LidMomokaCallHook*c=&h->calls[i];
        if(h->ops.quiescent(h->ops.context)!=1||!lid_mh_read(h,c->site,bytes,5)||!lid_mh_same(bytes,c->original,5)||
           !h->ops.protect(h->ops.context,(void*)c->site,5,0x40,&old))goto fail;
        c->original_protection=old;c->protection_saved=1;
        h->ever_published=1;c->touched=1;__builtin_memcpy((void*)c->site,c->patched,5);
        if(!h->ops.flush(h->ops.context,(void*)c->site,5)){
            (void)h->ops.protect(h->ops.context,(void*)c->site,5,old,&ignored);goto fail;
        }
        if(!h->ops.protect(h->ops.context,(void*)c->site,5,old,&ignored))goto fail;
    }
    if(h->ops.quiescent(h->ops.context)!=1||!lid_mh_read(h,h->slot,&current,8)||current!=h->handler_original||
       !h->ops.protect(h->ops.context,(void*)h->slot,8,0x04,&old))goto fail;
    h->slot_protection=old;current=h->handler_original;h->ever_published=1;
    int exchanged=__atomic_compare_exchange_n((uintptr_t*)h->slot,&current,h->handler_wrapper,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    if(exchanged)h->slot_touched=1;
    int protected_ok=h->ops.protect(h->ops.context,(void*)h->slot,8,old,&ignored);
    if(!exchanged||!protected_ok)goto fail;
    h->state=LID_MH_ACTIVE;return 1;
fail:(void)lid_momoka_hooks_restore(h);return 0;
}
/* Profile override only for this set's two verified owned CALLs; raw reads of
   data are never normalized. Can be supplied to the payment-profile validator. */
static int lid_momoka_hooks_callsite(const LidMomokaHooks*h,uintptr_t site,const unsigned char*original,unsigned size){
    if(!h||h->state!=LID_MH_ACTIVE||!original||size!=5)return 0;
    for(unsigned i=0;i<2;i++)if(h->calls[i].site==site)return lid_mh_same(original,h->calls[i].original,5)&&lid_mh_call_owned(h,&h->calls[i]);
    return 0;
}
#endif
