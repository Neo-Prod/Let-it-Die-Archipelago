#ifndef LID_CHECKS_ROTATION_PATCH_R31_H
#define LID_CHECKS_ROTATION_PATCH_R31_H
#include <stdint.h>
#include <stddef.h>
/* Two-profile transaction, private tests first. Caller must establish real
   quiescence BEFORE invoking this API. It does not suspend threads, discover
   a process, establish quiescence, or modify the existing reload hook set. */
typedef struct R31PatchOps {
    void*context;int(*quiet)(void*);int(*readable)(void*,const void*,size_t);
    void*(*allocate)(void*,size_t);int(*release)(void*,void*);
    int(*writable)(void*,void*,size_t,unsigned*);
    int(*protect)(void*,void*,size_t,unsigned);int(*flush)(void*,void*,size_t);
} R31PatchOps;
typedef struct R31Patch {
    unsigned char*target,*trampoline;void*detour;unsigned size,protection,touched;
    unsigned char original[16];
} R31Patch;
enum{R31_PATCH_EMPTY,R31_PATCH_PREPARING,R31_PATCH_READY,R31_PATCH_FAILED,R31_PATCH_POISONED};
typedef struct R31PatchSet {unsigned state;R31Patch entries[2];void*originals[2];} R31PatchSet;
static const struct R31Profile {uintptr_t rva;unsigned size;unsigned char bytes[16];} r31_profiles[2]={
    {0x114ac90,15,{0x48,0x89,0x4c,0x24,0x08,0x53,0x55,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x70}},
    {0x1360950,16,{0x44,0x88,0x44,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}}
};
static void r31_jump(unsigned char*out,const void*target){
    uintptr_t p=(uintptr_t)target;out[0]=0xff;out[1]=0x25;out[2]=out[3]=out[4]=out[5]=0;__builtin_memcpy(out+6,&p,8);
}
static int r31_patch_owned(const R31Patch*p,const R31PatchOps*o){
    unsigned char expected[16];r31_jump(expected,p->detour);
    for(unsigned j=14;j<p->size;j++)expected[j]=0x90;
    return o->readable(o->context,p->target,p->size)&&!__builtin_memcmp(p->target,expected,p->size);
}
static int r31_patch_rollback(R31PatchSet*s,const R31PatchOps*o){
    unsigned unused;int good=1;
    if(!s||!o||!o->quiet||!o->readable||!o->writable||!o->flush||!o->protect||!o->release)return 0;
    if(s->state==R31_PATCH_POISONED)return 0; /* no unproved recovery/release */
    if(o->quiet(o->context)!=1){s->state=R31_PATCH_POISONED;return 0;}
    for(unsigned i=2;i>0;i--){R31Patch*p=&s->entries[i-1];
        if(!p->touched)continue;
        /* Never overwrite an intervening unknown patch. Keep trampolines
           alive when ownership or instruction-cache/protection is uncertain. */
        if(!r31_patch_owned(p,o)||!o->writable(o->context,p->target,p->size,&unused)){good=0;continue;}
        __builtin_memcpy(p->target,p->original,p->size);
        if(o->flush(o->context,p->target,p->size)!=1)good=0;
        if(o->protect(o->context,p->target,p->size,p->protection)!=1)good=0;
        p->touched=0;
    }
    if(!good){s->state=R31_PATCH_POISONED;return 0;}
    s->originals[0]=s->originals[1]=0;
    for(unsigned i=0;i<2;i++){R31Patch*p=&s->entries[i];if(!p->trampoline)continue;
        if(o->release(o->context,p->trampoline)==1)p->trampoline=0;else good=0;
    }
    s->state=good?R31_PATCH_FAILED:R31_PATCH_POISONED;return good;
}
static int r31_patch_install(R31PatchSet*s,uintptr_t base,int verified_sha,void*const detours[2],const R31PatchOps*o){
    if(!s||s->state!=R31_PATCH_EMPTY||!base||base>UINTPTR_MAX-0x1361000||verified_sha!=1||
       !detours||!detours[0]||!detours[1]||!o||!o->quiet||!o->readable||!o->allocate||!o->release||
       !o->writable||!o->protect||!o->flush||o->quiet(o->context)!=1)return 0;
    for(unsigned i=0;i<2;i++){
        const struct R31Profile*p=&r31_profiles[i];const void*target=(void*)(base+p->rva);
        if(!o->readable(o->context,target,p->size)||__builtin_memcmp(target,p->bytes,p->size))return 0;
    }
    s->state=R31_PATCH_PREPARING;
    for(unsigned i=0;i<2;i++){
        R31Patch*p=&s->entries[i];p->target=(unsigned char*)(base+r31_profiles[i].rva);p->size=r31_profiles[i].size;p->detour=detours[i];
        __builtin_memcpy(p->original,p->target,p->size);
        p->trampoline=o->allocate(o->context,p->size+14);if(!p->trampoline)goto fail;
        __builtin_memcpy(p->trampoline,p->original,p->size);r31_jump(p->trampoline+p->size,p->target+p->size);
        if(o->flush(o->context,p->trampoline,p->size+14)!=1)goto fail;
        s->originals[i]=p->trampoline;
    }
    for(unsigned i=0;i<2;i++){
        R31Patch*p=&s->entries[i];
        if(o->quiet(o->context)!=1||!o->readable(o->context,p->target,p->size)||__builtin_memcmp(p->target,p->original,p->size))goto fail;
        if(o->writable(o->context,p->target,p->size,&p->protection)!=1)goto fail;
        p->touched=1;r31_jump(p->target,p->detour);for(unsigned j=14;j<p->size;j++)p->target[j]=0x90;
        if(o->flush(o->context,p->target,p->size)!=1||o->protect(o->context,p->target,p->size,p->protection)!=1)goto fail;
    }
    s->state=R31_PATCH_READY;return 1;
fail:
    (void)r31_patch_rollback(s,o);return 0;
}
#endif
