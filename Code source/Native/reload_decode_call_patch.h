#ifndef LID_RELOAD_DECODE_CALL_PATCH_R37_H
#define LID_RELOAD_DECODE_CALL_PATCH_R37_H
#include "checks_rotation_call_patch.h"
enum{R37_SITE_COUNT=6};
static const struct R37CallProfile{uintptr_t rva,target;unsigned char bytes[5];} r37_calls[R37_SITE_COUNT]={
    {0x1107f08,0x21170,{0xe8,0x63,0x92,0xf1,0xfe}},
    {0x11082d3,0x14efb0,{0xe8,0xd8,0x6c,0x04,0xff}},
    {0x110859a,0x14efb0,{0xe8,0x11,0x6a,0x04,0xff}},
    {0x1108652,0xe06f0,{0xe8,0x99,0x80,0xfd,0xfe}},
    {0x110886f,0x248b00,{0xe8,0x8c,0x02,0x14,0xff}},
    {0x1107eb3,0x1102850,{0xe8,0x98,0xa9,0xff,0xff}}
};
typedef struct R37CallSet{
    unsigned state,touched[R37_SITE_COUNT],protection[R37_SITE_COUNT];
    unsigned char*sites[R37_SITE_COUNT];void*originals[R37_SITE_COUNT];
    void*relay;unsigned char patched[R37_SITE_COUNT][5];
}R37CallSet;
static int r37_calls_rollback(R37CallSet*s,const R32CallPatchOps*ops){
    if(!s||!ops||s->state==R31_PATCH_POISONED)return 0;
    const R31PatchOps*o=&ops->memory;int good=1;unsigned unused;
    if(!o->quiet||!o->readable||!o->writable||!o->flush||!o->protect||!o->release||o->quiet(o->context)!=1){s->state=R31_PATCH_POISONED;return 0;}
    for(unsigned j=R37_SITE_COUNT;j>0;j--){unsigned i=j-1;if(!s->touched[i])continue;
        if(!o->readable(o->context,s->sites[i],5)||__builtin_memcmp(s->sites[i],s->patched[i],5)||
           o->writable(o->context,s->sites[i],5,&unused)!=1){good=0;continue;}
        __builtin_memcpy(s->sites[i],r37_calls[i].bytes,5);
        if(o->flush(o->context,s->sites[i],5)!=1||o->protect(o->context,s->sites[i],5,s->protection[i])!=1)good=0;
        s->touched[i]=0;
    }
    if(!good){s->state=R31_PATCH_POISONED;return 0;}
    if(s->relay){if(o->release(o->context,s->relay)!=1){s->state=R31_PATCH_POISONED;return 0;}s->relay=0;}
    for(unsigned i=0;i<R37_SITE_COUNT;i++)s->originals[i]=0;
    s->state=R31_PATCH_FAILED;return 1;
}
static int r37_calls_install(R37CallSet*s,uintptr_t base,int exact_sha,void*const detours[R37_SITE_COUNT],const R32CallPatchOps*ops){
    if(!s||s->state!=R31_PATCH_EMPTY||!base||base>UINTPTR_MAX-0x1110000||exact_sha!=1||!detours||!ops)return 0;
    const R31PatchOps*o=&ops->memory;
    if(!o->quiet||!o->readable||!o->release||!o->writable||!o->protect||!o->flush||o->quiet(o->context)!=1)return 0;
    for(unsigned i=0;i<R37_SITE_COUNT;i++){
        if(!detours[i]||!o->readable(o->context,(void*)(base+r37_calls[i].rva),5)||
           __builtin_memcmp((void*)(base+r37_calls[i].rva),r37_calls[i].bytes,5))return 0;
    }
    s->state=R31_PATCH_PREPARING;
    s->relay=r32_allocate_near(base+r37_calls[0].rva,&ops->proximity);if(!s->relay)goto fail;
    /* Publish all original targets and complete all relays before any CALL. */
    for(unsigned i=0;i<R37_SITE_COUNT;i++){
        s->sites[i]=(unsigned char*)(base+r37_calls[i].rva);s->originals[i]=(void*)(base+r37_calls[i].target);
        void*relay=(unsigned char*)s->relay+16*i;
        if(!r32_call_bytes(s->patched[i],(uintptr_t)s->sites[i],(uintptr_t)relay))goto fail;
        r31_jump(relay,detours[i]);
    }
    if(o->flush(o->context,s->relay,16*R37_SITE_COUNT)!=1)goto fail;
    for(unsigned i=0;i<R37_SITE_COUNT;i++){
        if(o->quiet(o->context)!=1||!o->readable(o->context,s->sites[i],5)||
           __builtin_memcmp(s->sites[i],r37_calls[i].bytes,5)||
           o->writable(o->context,s->sites[i],5,&s->protection[i])!=1)goto fail;
        s->touched[i]=1;__builtin_memcpy(s->sites[i],s->patched[i],5);
        if(o->flush(o->context,s->sites[i],5)!=1||o->protect(o->context,s->sites[i],5,s->protection[i])!=1)goto fail;
    }
    s->state=R31_PATCH_READY;return 1;
fail:
    (void)r37_calls_rollback(s,ops);return 0;
}
#endif
