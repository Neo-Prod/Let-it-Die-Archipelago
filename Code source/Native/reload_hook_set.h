#ifndef LID_RELOAD_HOOK_SET_H
#define LID_RELOAD_HOOK_SET_H
#include <stdint.h>
#include <stddef.h>
#include "reload_native_profiles.h"
/* Caller must establish actual quiescence of request/save workers BEFORE
   calling install. These writes are not atomic hot patches. A callback merely
   reports that externally established condition; it cannot create it.
   No engine calls, file IO, thread suspension or shared hook helper changes. */
typedef unsigned char (*LidReloadNativeSave)(void*,void*);
typedef unsigned char (*LidReloadNativeApply)(void*,void*);
typedef unsigned char (*LidReloadNativeLoad)(void*);
typedef unsigned char (*LidReloadNativeProject)(void*);
typedef unsigned char (*LidReloadNativeCore)(void*,void*,void*,void*,const unsigned short*,int*,const unsigned short*);
typedef struct LidReloadPatchOps {
    void *context;
    int (*quiescent)(void*);
    int (*readable)(void*,const void*,size_t);
    void* (*allocate)(void*,size_t);
    int (*free)(void*,void*);
    int (*writable)(void*,void*,size_t,unsigned*);
    int (*protect)(void*,void*,size_t,unsigned);
    int (*flush)(void*,void*,size_t);
} LidReloadPatchOps;
typedef struct LidReloadPatch {
    unsigned char *target,*trampoline;
    void *detour;
    unsigned size,old_protection,touched;
    unsigned char original[32];
} LidReloadPatch;
enum {LID_RELOAD_HOOK_EMPTY=0,LID_RELOAD_HOOK_PREPARING,LID_RELOAD_HOOK_READY,
      LID_RELOAD_HOOK_FAILED,LID_RELOAD_HOOK_POISONED};
typedef struct LidReloadHookSet {
    unsigned state;
    uintptr_t base;
    LidReloadPatch patch[3];
    LidReloadNativeSave save;
    LidReloadNativeApply apply;
    LidReloadNativeCore core;
    LidReloadNativeLoad load;
    LidReloadNativeProject project;
} LidReloadHookSet;
static void lid_reload_jump(unsigned char*p,const void*to){
    uintptr_t address=(uintptr_t)to;
    p[0]=0xff;p[1]=0x25;p[2]=p[3]=p[4]=p[5]=0;
    __builtin_memcpy(p+6,&address,8);
}
static int lid_reload_hook_profiles(uintptr_t base,int sha,const LidReloadPatchOps*o){
    unsigned i;
    if(!base||base>UINTPTR_MAX-0xf8b1000||sha!=1||!o||!o->quiescent||!o->readable||
       !o->allocate||!o->free||!o->writable||!o->protect||!o->flush||o->quiescent(o->context)!=1)return 0;
    for(i=0;i<5;i++){
        const LidReloadNativeProfile*p=&lid_reload_native_profiles[i];
        const void*target=(const void*)(base+p->rva);
        if(!o->readable(o->context,target,p->size)||__builtin_memcmp(target,p->bytes,p->size))return 0;
    }
    return 1;
}
/* On uncertain rollback, retain every trampoline: a remaining detour can still
   need it. Poisoned sets cannot be reinstalled or advertised as protected. */
static int lid_reload_hook_rollback(LidReloadHookSet*s,const LidReloadPatchOps*o){
    unsigned i,ignored;int good=1;
    if(o->quiescent(o->context)!=1){s->state=LID_RELOAD_HOOK_POISONED;return 0;}
    for(i=3;i>0;i--){LidReloadPatch*p=&s->patch[i-1];
        if(!p->touched)continue;
        if(!o->writable(o->context,p->target,p->size,&ignored)){good=0;continue;}
        __builtin_memcpy(p->target,p->original,p->size);
        if(!o->flush(o->context,p->target,p->size))good=0;
        if(!o->protect(o->context,p->target,p->size,p->old_protection))good=0;
        /* Even if bytes were restored, an unsuccessful flush/protect leaves
           the complete set uncertain. Do not free any backing trampolines. */
        p->touched=0;
    }
    if(!good){s->state=LID_RELOAD_HOOK_POISONED;return 0;}
    s->save=0;s->apply=0;s->core=0;s->load=0;s->project=0;
    for(i=0;i<3;i++){
        LidReloadPatch*p=&s->patch[i];
        if(p->trampoline&&!o->free(o->context,p->trampoline))good=0;
        /* Don't retry a failed release: retain pointer for diagnostics. */
        else p->trampoline=0;
    }
    s->state=good?LID_RELOAD_HOOK_FAILED:LID_RELOAD_HOOK_POISONED;
    return good;
}
static int lid_reload_hook_install(LidReloadHookSet*s,uintptr_t base,int sha,
                                   void*const detour[3],const LidReloadPatchOps*o){
    unsigned i,j;
    if(!s||s->state!=LID_RELOAD_HOOK_EMPTY||!detour||!detour[0]||!detour[1]||!detour[2])return 0;
    if(!lid_reload_hook_profiles(base,sha,o))return 0;
    for(i=0;i<3;i++)if(s->patch[i].trampoline||s->patch[i].touched)return 0;
    s->state=LID_RELOAD_HOOK_PREPARING;s->base=base;
    /* Prepare all trampolines and publish all original function pointers before
       publishing the first detour. The precondition forbids concurrent entry. */
    for(i=0;i<3;i++){
        const LidReloadNativeProfile*profile=&lid_reload_native_profiles[i];
        LidReloadPatch*p=&s->patch[i];
        p->target=(unsigned char*)(base+profile->rva);p->size=profile->size;p->detour=detour[i];
        __builtin_memcpy(p->original,p->target,p->size);
        p->trampoline=o->allocate(o->context,p->size+14);
        if(!p->trampoline)goto fail;
        __builtin_memcpy(p->trampoline,p->original,p->size);
        lid_reload_jump(p->trampoline+p->size,p->target+p->size);
        if(!o->flush(o->context,p->trampoline,p->size+14))goto fail;
    }
    s->save=(LidReloadNativeSave)s->patch[0].trampoline;
    s->apply=(LidReloadNativeApply)s->patch[1].trampoline;
    s->core=(LidReloadNativeCore)s->patch[2].trampoline;
    s->load=(LidReloadNativeLoad)(base+lid_reload_native_profiles[LID_RELOAD_PROFILE_LOAD].rva);
    s->project=(LidReloadNativeProject)(base+lid_reload_native_profiles[LID_RELOAD_PROFILE_PROJECT].rva);
    for(i=0;i<3;i++){
        LidReloadPatch*p=&s->patch[i];
        if(o->quiescent(o->context)!=1||!o->readable(o->context,p->target,p->size)||
           __builtin_memcmp(p->target,p->original,p->size))goto fail;
        if(!o->writable(o->context,p->target,p->size,&p->old_protection))goto fail;
        p->touched=1;
        lid_reload_jump(p->target,p->detour);
        for(j=14;j<p->size;j++)p->target[j]=0x90;
        if(!o->flush(o->context,p->target,p->size))goto fail;
        if(!o->protect(o->context,p->target,p->size,p->old_protection))goto fail;
    }
    s->state=LID_RELOAD_HOOK_READY;return 1;
fail:
    (void)lid_reload_hook_rollback(s,o);return 0;
}
#endif
