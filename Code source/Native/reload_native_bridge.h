#ifndef LID_RELOAD_NATIVE_BRIDGE_H
#define LID_RELOAD_NATIVE_BRIDGE_H
#include "reload_hook_set.h"
#include "reload_boundary.h"
#include "reload_load_trace.h"
/* Routing bodies for the future three detours. The detour itself must obtain
   its caller return address directly, not from an extra wrapper frame.
   Expected runner and callsite are exact-profile values, never user offsets.
   Session/request serialization and the short reservation remain mandatory. */
typedef struct LidReloadNativeBridge {
    LidReloadHookSet *hooks;
    LidReloadBoundary *boundary;
    void *runner,*apply_return;
    void *context;
    unsigned (*thread_id)(void*);
    /* Validate all required roots and identity, and retain private expected
       soul/bodyuser encodings BEFORE ApplyRoot/the loader destroys its root.
       Return1 only after a complete capture. No ownership transfer of root. */
    int (*capture)(void*,void*);
    unsigned active,owner_thread,seen,applied,refused;
} LidReloadNativeBridge;
static int lid_reload_bridge_valid(const LidReloadNativeBridge*b){
    return b&&b->hooks&&b->hooks->state==LID_RELOAD_HOOK_READY&&b->boundary&&
        b->runner&&b->apply_return&&b->thread_id&&b->capture&&b->hooks->save&&
        b->hooks->apply&&b->hooks->load&&b->hooks->project&&b->hooks->core;
}
static unsigned char lid_reload_bridge_save(LidReloadNativeBridge*b,void*runner,void*optional_json){
    unsigned char result;
    if(!lid_reload_bridge_valid(b)||!lid_reload_save_enter(&b->boundary->save))return 0;
    result=b->hooks->save(runner,optional_json);
    if(!lid_reload_save_leave(&b->boundary->save)){
        lid_boundary_fail(b->boundary,LID_BOUNDARY_CORE);return 0;
    }
    return result;
}
static unsigned char lid_reload_bridge_apply(LidReloadNativeBridge*b,void*runner,void*root,void*caller_return){
    unsigned char result;unsigned expected=0;
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
    LID_R36_SET(apply_entered,1);
    if(LID_R36_RESULT(apply_invalid,!lid_reload_bridge_valid(b)))return 0;
    if(LID_R36_RESULT(apply_inactive,!__atomic_load_n(&b->active,__ATOMIC_ACQUIRE)))return b->hooks->apply(runner,root);
#else
    if(!lid_reload_bridge_valid(b))return 0;
    if(!__atomic_load_n(&b->active,__ATOMIC_ACQUIRE))return b->hooks->apply(runner,root);
#endif
    LID_R36_SET(apply_stage,1);
    if(LID_R36_PHASE(apply_reject,1,b->thread_id(b->context)!=b->owner_thread)||
       LID_R36_PHASE(apply_reject,2,runner!=b->runner)||LID_R36_PHASE(apply_reject,3,!root)||
       LID_R36_PHASE(apply_reject,4,caller_return!=b->apply_return)||
       LID_R36_PHASE(apply_reject,5,!__atomic_compare_exchange_n(&b->seen,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))){
        __atomic_store_n(&b->refused,1,__ATOMIC_RELEASE);return 0;
    }
    LID_R36_SET(apply_stage,2);
    if(b->capture(b->context,root)!=1){__atomic_store_n(&b->refused,1,__ATOMIC_RELEASE);return 0;}
    /* Capture may call code that exposes a reentrant attempt; don't apply a
       partially trusted source after such a refusal. */
    if(__atomic_load_n(&b->refused,__ATOMIC_ACQUIRE))return 0;
    LID_R36_SET(apply_stage,3);
    result=b->hooks->apply(runner,root);
    if(result!=1)__atomic_store_n(&b->refused,1,__ATOMIC_RELEASE);
    else {__atomic_store_n(&b->applied,1,__ATOMIC_RELEASE);LID_R36_SET(apply_stage,4);}
    return result;
}
static unsigned char lid_reload_bridge_load(LidReloadNativeBridge*b){
    unsigned char result;unsigned tid,expected=0;
    LID_R36_SET(bridge_stage,1);
    if(LID_R36_PHASE(bridge_reject,1,!lid_reload_bridge_valid(b))||
       LID_R36_PHASE(bridge_reject,2,b->boundary->stage!=LID_BOUNDARY_BUSY)||
       LID_R36_PHASE(bridge_reject,3,lid_reload_save_phase(&b->boundary->save)!=LID_RELOAD_SAVE_LOADING)||
       LID_R36_PHASE(bridge_reject,4,lid_reload_save_active(&b->boundary->save)))return 0;
    LID_R36_SET(bridge_stage,2);
    tid=b->thread_id(b->context);if(!tid)return 0;
    /* Serialized request owner initializes fields before publishing scope.
       A recursive call cannot reset evidence from the outer load. */
    if(__atomic_load_n(&b->active,__ATOMIC_ACQUIRE)){
        __atomic_store_n(&b->refused,1,__ATOMIC_RELEASE);return 0;
    }
    LID_R36_SET(bridge_stage,3);
    b->owner_thread=tid;b->seen=b->applied=b->refused=0;
    if(!__atomic_compare_exchange_n(&b->active,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    LID_R36_SET(bridge_stage,4);
    result=LID_R36_RESULT(native_result,b->hooks->load(b->runner));
    LID_R36_SET(bridge_stage,5);
    __atomic_store_n(&b->active,0,__ATOMIC_RELEASE);
    /* BRG reader ignores the ApplyRoot return: don't trust its bool alone. */
    return result==1&&__atomic_load_n(&b->seen,__ATOMIC_ACQUIRE)==1&&
        __atomic_load_n(&b->applied,__ATOMIC_ACQUIRE)==1&&!__atomic_load_n(&b->refused,__ATOMIC_ACQUIRE);
}
static unsigned char lid_reload_bridge_project(LidReloadNativeBridge*b,void*initialized_soul){
    if(!lid_reload_bridge_valid(b)||!initialized_soul||b->boundary->stage!=LID_BOUNDARY_BUSY||
       lid_reload_save_phase(&b->boundary->save)!=LID_RELOAD_SAVE_LOADING)return 0;
    /* One useful native argument. The caller still verifies projected content:
       the native bool doesn't propagate all nested projection failures. */
    return b->hooks->project(initialized_soul);
}
#endif
