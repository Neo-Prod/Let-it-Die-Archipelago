#ifndef LID_CHECKS_ROTATION_CALL_PATCH_R32_H
#define LID_CHECKS_ROTATION_CALL_PATCH_R32_H
#include "checks_rotation_patch.h"
/* The shared selector is NEVER patched. Only the writer's entry and its
   known CALL instruction are changed, under an externally held idle lease. */
#define R32_WRITER_RVA ((uintptr_t)0x1360950)
#define R32_CALL_RVA ((uintptr_t)0x1360b2c)
#define R32_SELECTOR_RVA ((uintptr_t)0x114ac90)
typedef struct R32Region {uintptr_t base;size_t size;int free;} R32Region;
typedef struct R32NearOps {
    void*context;int(*query)(void*,uintptr_t,R32Region*);
    void*(*allocate_at)(void*,uintptr_t,size_t);
} R32NearOps;
static int r32_call_bytes(unsigned char*out,uintptr_t call,uintptr_t relay){
    uintptr_t next=call+5,difference;int32_t delta;
    if(next<call)return 0;
    if(relay>=next){difference=relay-next;if(difference>INT32_MAX)return 0;delta=(int32_t)difference;}
    else{difference=next-relay;if(difference>(uintptr_t)INT32_MAX+1)return 0;delta=(int32_t)(-(int64_t)difference);}
    out[0]=0xe8;__builtin_memcpy(out+1,&delta,4);return 1;
}
/* Bounded free-region search above the call. No wait, file I/O, unbounded
   page scanning or allocation outside the rel32 range. Failure is passive. */
static void*r32_allocate_near(uintptr_t call,const R32NearOps*o){
    uintptr_t at,limit;unsigned char encoded[5];R32Region region;
    if(!o||!o->query||!o->allocate_at||call>UINTPTR_MAX-0x80010000)return 0;
    at=(call+0x10000)&~(uintptr_t)0xffff;limit=call+5+INT32_MAX;
    for(unsigned i=0;i<64&&at<=limit;i++){
        if(o->query(o->context,at,&region)!=1||!region.size||region.base>at||region.base>UINTPTR_MAX-region.size)return 0;
        uintptr_t end=region.base+region.size;if(end<=at)return 0;
        if(region.free&&end-at>=4096&&r32_call_bytes(encoded,call,at)){
            void*p=o->allocate_at(o->context,at,4096);
            if(p){/* allocate_at promises the exact requested address. */return p;}
            if(at>UINTPTR_MAX-0x10000)return 0;at+=0x10000;
        }else{
            if(end>UINTPTR_MAX-0xffff)return 0;at=(end+0xffff)&~(uintptr_t)0xffff;
        }
    }
    return 0;
}
typedef struct R32CallPatchSet {
    unsigned state;R31Patch writer;unsigned char*call,*relay;unsigned call_protection,call_touched;
    unsigned char original_call[5],patched_call[5];void*originals[2];
} R32CallPatchSet;
typedef struct R32CallPatchOps {R31PatchOps memory;R32NearOps proximity;} R32CallPatchOps;
static int r32_call_rollback(R32CallPatchSet*s,const R32CallPatchOps*ops){
    if(!s||!ops||s->state==R31_PATCH_POISONED)return 0;
    const R31PatchOps*o=&ops->memory;unsigned unused;int good=1;
    if(!o->quiet||!o->readable||!o->writable||!o->flush||!o->protect||!o->release)return 0;
    if(o->quiet(o->context)!=1){s->state=R31_PATCH_POISONED;return 0;}
    R31Patch*p=&s->writer;
    if(p->touched){
        if(!r31_patch_owned(p,o)||o->writable(o->context,p->target,16,&unused)!=1)good=0;
        else{
            __builtin_memcpy(p->target,p->original,16);
            if(o->flush(o->context,p->target,16)!=1)good=0;
            if(o->protect(o->context,p->target,16,p->protection)!=1)good=0;p->touched=0;
        }
    }
    if(s->call_touched){
        if(!o->readable(o->context,s->call,5)||__builtin_memcmp(s->call,s->patched_call,5)||
           o->writable(o->context,s->call,5,&unused)!=1)good=0;
        else{
            __builtin_memcpy(s->call,s->original_call,5);
            if(o->flush(o->context,s->call,5)!=1)good=0;
            if(o->protect(o->context,s->call,5,s->call_protection)!=1)good=0;s->call_touched=0;
        }
    }
    if(!good){s->state=R31_PATCH_POISONED;return 0;}
    s->originals[0]=s->originals[1]=0;
    if(s->relay){if(o->release(o->context,s->relay)==1)s->relay=0;else good=0;}
    if(p->trampoline){if(o->release(o->context,p->trampoline)==1)p->trampoline=0;else good=0;}
    s->state=good?R31_PATCH_FAILED:R31_PATCH_POISONED;return good;
}
static int r32_call_install(R32CallPatchSet*s,uintptr_t base,int sha,void*const detours[2],const R32CallPatchOps*ops){
    static const unsigned char expected_call[]={0xe8,0x5f,0xa1,0xde,0xff};
    if(!s||s->state!=R31_PATCH_EMPTY||!ops||!base||base>UINTPTR_MAX-0x1361000||sha!=1||!detours||!detours[0]||!detours[1])return 0;
    const R31PatchOps*o=&ops->memory;
    if(!o->quiet||!o->readable||!o->allocate||!o->release||!o->writable||!o->protect||!o->flush||o->quiet(o->context)!=1)return 0;
    unsigned char*entry=(unsigned char*)(base+R32_WRITER_RVA),*call=(unsigned char*)(base+R32_CALL_RVA);
    if(!o->readable(o->context,entry,16)||__builtin_memcmp(entry,r31_profiles[1].bytes,16)||
       !o->readable(o->context,call,5)||__builtin_memcmp(call,expected_call,5))return 0;
    s->state=R31_PATCH_PREPARING;s->call=call;__builtin_memcpy(s->original_call,call,5);
    R31Patch*p=&s->writer;p->target=entry;p->size=16;p->detour=detours[1];__builtin_memcpy(p->original,entry,16);
    p->trampoline=o->allocate(o->context,30);if(!p->trampoline)goto fail;
    __builtin_memcpy(p->trampoline,entry,16);r31_jump(p->trampoline+16,entry+16);
    s->relay=r32_allocate_near((uintptr_t)call,&ops->proximity);if(!s->relay)goto fail;
    if(!r32_call_bytes(s->patched_call,(uintptr_t)call,(uintptr_t)s->relay))goto fail;
    r31_jump(s->relay,detours[0]);
    if(o->flush(o->context,p->trampoline,30)!=1||o->flush(o->context,s->relay,14)!=1)goto fail;
    /* Constructor points to its unchanged original entry. No copied selector
       prologue or address-dependent relocation is needed. */
    s->originals[0]=(void*)(base+R32_SELECTOR_RVA);s->originals[1]=p->trampoline;
    if(o->quiet(o->context)!=1||!o->readable(o->context,call,5)||__builtin_memcmp(call,s->original_call,5)||
       o->writable(o->context,call,5,&s->call_protection)!=1)goto fail;
    s->call_touched=1;__builtin_memcpy(call,s->patched_call,5);
    if(o->flush(o->context,call,5)!=1||o->protect(o->context,call,5,s->call_protection)!=1)goto fail;
    if(o->quiet(o->context)!=1||!o->readable(o->context,entry,16)||__builtin_memcmp(entry,p->original,16)||
       o->writable(o->context,entry,16,&p->protection)!=1)goto fail;
    p->touched=1;r31_jump(entry,p->detour);entry[14]=entry[15]=0x90;
    if(o->flush(o->context,entry,16)!=1||o->protect(o->context,entry,16,p->protection)!=1)goto fail;
    s->state=R31_PATCH_READY;return 1;
fail:
    (void)r32_call_rollback(s,ops);return 0;
}
#endif
