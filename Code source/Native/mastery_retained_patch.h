#ifndef LID_MASTERY_RETAINED_PATCH_H
#define LID_MASTERY_RETAINED_PATCH_H
#include "checks_rotation_call_patch.h"
/* Six known award CALLs only. No shared helper/prologue or save hook rewrite. */
static const uintptr_t lid_mastery_retained_calls[6]={0x1291c69,0x129215b,0x12925ec,0x1292b05,0x1293037,0x12936d5};
static const unsigned char lid_mastery_retained_bytes[6][5]={
    {0xe8,0x42,0x9c,0x0d,0},{0xe8,0x50,0x97,0x0d,0},{0xe8,0xbf,0x92,0x0d,0},
    {0xe8,0xa6,0x8d,0x0d,0},{0xe8,0x74,0x88,0x0d,0},{0xe8,0xd6,0x81,0x0d,0}};
typedef struct LidMasteryRetainedPatch {
    unsigned state,touched[6],protection[6];uintptr_t base;
    unsigned char*relay;unsigned char patched[6][5];
} LidMasteryRetainedPatch;
static int lid_mastery_retained_caller(uintptr_t rva){
    for(unsigned i=0;i<6;i++)if(rva==lid_mastery_retained_calls[i]+5)return 1;
    return 0;
}
static int lid_mastery_retained_rollback(LidMasteryRetainedPatch*s,const R32CallPatchOps*ops){
    const R31PatchOps*o=&ops->memory;unsigned ignored;int good=1;
    if(o->quiet(o->context)!=1){s->state=R31_PATCH_POISONED;return 0;}
    for(unsigned i=6;i>0;i--){unsigned n=i-1;unsigned char*p=(unsigned char*)(s->base+lid_mastery_retained_calls[n]);
        if(!s->touched[n])continue;
        if(!o->readable(o->context,p,5)||__builtin_memcmp(p,s->patched[n],5)||
           o->writable(o->context,p,5,&ignored)!=1){good=0;continue;}
        __builtin_memcpy(p,lid_mastery_retained_bytes[n],5);
        int flushed=o->flush(o->context,p,5),protected=o->protect(o->context,p,5,s->protection[n]);
        if(flushed!=1||protected!=1)good=0;else s->touched[n]=0;
    }
    if(!good){s->state=R31_PATCH_POISONED;return 0;}
    if(s->relay){if(o->release(o->context,s->relay)!=1){s->state=R31_PATCH_POISONED;return 0;}s->relay=0;}
    s->state=R31_PATCH_FAILED;return 1;
}
static int lid_mastery_retained_install(LidMasteryRetainedPatch*s,uintptr_t base,int sha,void*detour,const R32CallPatchOps*ops){
    if(!s||s->state!=R31_PATCH_EMPTY||!ops||!base||base>UINTPTR_MAX-0x136c000||sha!=1||!detour)return 0;
    const R31PatchOps*o=&ops->memory;
    if(!o->quiet||!o->readable||!o->release||!o->writable||!o->flush||!o->protect||o->quiet(o->context)!=1)return 0;
    for(unsigned i=0;i<6;i++){
        unsigned char*p=(unsigned char*)(base+lid_mastery_retained_calls[i]);
        if(!o->readable(o->context,p,5)||__builtin_memcmp(p,lid_mastery_retained_bytes[i],5))return 0;
    }
    s->base=base;s->state=R31_PATCH_PREPARING;
    s->relay=r32_allocate_near(base+lid_mastery_retained_calls[0],&ops->proximity);if(!s->relay)goto fail;
    r31_jump(s->relay,detour);
    if(o->flush(o->context,s->relay,14)!=1||o->protect(o->context,s->relay,4096,0x20)!=1)goto fail;
    for(unsigned i=0;i<6;i++)if(!r32_call_bytes(s->patched[i],base+lid_mastery_retained_calls[i],(uintptr_t)s->relay))goto fail;
    for(unsigned i=0;i<6;i++){
        unsigned char*p=(unsigned char*)(base+lid_mastery_retained_calls[i]);
        if(o->quiet(o->context)!=1||!o->readable(o->context,p,5)||__builtin_memcmp(p,lid_mastery_retained_bytes[i],5)||
           o->writable(o->context,p,5,&s->protection[i])!=1)goto fail;
        s->touched[i]=1;__builtin_memcpy(p,s->patched[i],5);
        if(o->flush(o->context,p,5)!=1||o->protect(o->context,p,5,s->protection[i])!=1)goto fail;
    }
    __atomic_store_n(&s->state,R31_PATCH_READY,__ATOMIC_RELEASE);return 1;
fail:
    (void)lid_mastery_retained_rollback(s,ops);return 0;
}
#endif
