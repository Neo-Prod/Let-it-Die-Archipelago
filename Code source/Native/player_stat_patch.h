#ifndef LID_PLAYER_STAT_PATCH_H
#define LID_PLAYER_STAT_PATCH_H
#include "checks_rotation_call_patch.h"
/* Transactional publication under the caller's existing idle reservation.
 * No thread suspension or process access. Entry profiles contain whole,
 * position-independent instructions; CALL profiles use a nearby relay and
 * leave the original function prologue intact. Originals precede publication.
 */
enum {LID_PSP_MAX=24};
enum {LID_PSP_EMPTY=0,LID_PSP_PREPARED,LID_PSP_ACTIVE,LID_PSP_FAILED,LID_PSP_POISONED};
typedef struct LidPSPatchProfile {uintptr_t rva,original_rva;unsigned size,call;unsigned char bytes[32];unsigned chara_rbx;} LidPSPatchProfile;
typedef struct LidPSPatchEntry {
    unsigned char *target,*storage;void *detour,*original;unsigned size,call,touched,protection;
    unsigned char before[32],after[32];
} LidPSPatchEntry;
typedef struct LidPSPatchSet {unsigned state,count;uintptr_t base;LidPSPatchEntry entries[LID_PSP_MAX];} LidPSPatchSet;
static int lid_psp_owned(const LidPSPatchEntry *p,const R31PatchOps *o){
    return o->readable(o->context,p->target,p->size)==1&&!__builtin_memcmp(p->target,p->after,p->size);
}
static int lid_psp_rollback(LidPSPatchSet *s,const R32CallPatchOps *ops){
    const R31PatchOps *o=&ops->memory;unsigned ignored;int good=1;
    if(!s||s->state==LID_PSP_POISONED||o->quiet(o->context)!=1)return 0;
    for(unsigned i=s->count;i>0;i--){
        LidPSPatchEntry *p=&s->entries[i-1];if(!p->touched)continue;
        if(!lid_psp_owned(p,o)||o->writable(o->context,p->target,p->size,&ignored)!=1){good=0;continue;}
        __builtin_memcpy(p->target,p->before,p->size);
        int flushed=o->flush(o->context,p->target,p->size),protected=o->protect(o->context,p->target,p->size,p->protection);
        if(flushed!=1||protected!=1||!o->readable(o->context,p->target,p->size)||__builtin_memcmp(p->target,p->before,p->size))good=0;
        else p->touched=0;
    }
    /* An uncertain branch keeps ALL original storage resident. Never free a
       trampoline that an unproved code path might still reference. */
    if(!good){s->state=LID_PSP_POISONED;return 0;}
    for(unsigned i=0;i<s->count;i++)if(s->entries[i].storage){
        if(o->release(o->context,s->entries[i].storage)!=1)good=0;
        else s->entries[i].storage=0;
    }
    s->state=good?LID_PSP_FAILED:LID_PSP_POISONED;return good;
}
static int lid_psp_prepare(LidPSPatchSet *s,uintptr_t base,int exact_sha,const LidPSPatchProfile *profiles,
                           void *const *detours,unsigned count,const R32CallPatchOps *ops){
    if(!s||s->state!=LID_PSP_EMPTY||!base||base>UINTPTR_MAX-0x2000000||exact_sha!=1||!profiles||!detours||
       !count||count>LID_PSP_MAX||!ops)return 0;
    const R31PatchOps *o=&ops->memory;
    if(!o->quiet||!o->readable||!o->allocate||!o->release||!o->writable||!o->protect||!o->flush||o->quiet(o->context)!=1)return 0;
    for(unsigned i=0;i<count;i++){
        const LidPSPatchProfile *p=&profiles[i];
        if(!detours[i]||!p->rva||p->rva>0x2000000||p->call>1||
           (p->call?(p->size!=5||!p->original_rva):(p->size<14||p->size>32))||
           !o->readable(o->context,(void*)(base+p->rva),p->size)||__builtin_memcmp((void*)(base+p->rva),p->bytes,p->size))return 0;
        for(unsigned j=0;j<i;j++)if(p->rva<profiles[j].rva+profiles[j].size&&profiles[j].rva<p->rva+p->size)return 0;
        if(p->chara_rbx&&(!p->call||p->chara_rbx!=1||p->rva!=0x1384f84||p->original_rva!=0x11a0b20))return 0;
        if(p->chara_rbx){
            static const unsigned char subject[3]={0x48,0x8b,0xd9}; /* MOV RBX,RCX */
            static const unsigned char arguments[5]={0x45,0x33,0xc0,0x8b,0xd6}; /* R8D=0, EDX=ESI */
            if(!o->readable(o->context,(void*)(base+0x1384f4a),3)||__builtin_memcmp((void*)(base+0x1384f4a),subject,3)||
               !o->readable(o->context,(void*)(base+0x1384f7f),5)||__builtin_memcmp((void*)(base+0x1384f7f),arguments,5))return 0;
        }
        if(p->call){int32_t rel;__builtin_memcpy(&rel,p->bytes+1,4);
            if(p->bytes[0]!=0xe8||(int64_t)p->rva+5+rel!=(int64_t)p->original_rva)return 0;}
    }
    s->count=count;s->base=base;s->state=LID_PSP_PREPARED;
    for(unsigned i=0;i<count;i++){
        const LidPSPatchProfile *v=&profiles[i];LidPSPatchEntry *p=&s->entries[i];
        p->target=(unsigned char*)(base+v->rva);p->size=v->size;p->call=v->call;p->detour=detours[i];
        __builtin_memcpy(p->before,v->bytes,p->size);
        if(p->call){
            p->storage=r32_allocate_near((uintptr_t)p->target,&ops->proximity);if(!p->storage)goto fail;
            unsigned extra=v->chara_rbx?3:0;
            if(extra){/* Current-build caller holds the Chara row in RBX.
                          MOV R9,RBX; no stack/flags/nonvolatile mutation. */
                p->storage[0]=0x4c;p->storage[1]=0x8b;p->storage[2]=0xcb;
            }
            r31_jump(p->storage+extra,p->detour);p->original=(void*)(base+v->original_rva);
            if(!r32_call_bytes(p->after,(uintptr_t)p->target,(uintptr_t)p->storage)||
               o->flush(o->context,p->storage,14+extra)!=1||o->protect(o->context,p->storage,4096,0x20)!=1)goto fail;
        }else{
            p->storage=o->allocate(o->context,p->size+14);if(!p->storage)goto fail;
            __builtin_memcpy(p->storage,p->before,p->size);r31_jump(p->storage+p->size,p->target+p->size);p->original=p->storage;
            r31_jump(p->after,p->detour);for(unsigned j=14;j<p->size;j++)p->after[j]=0x90;
            if(o->flush(o->context,p->storage,p->size+14)!=1||o->protect(o->context,p->storage,p->size+14,0x20)!=1)goto fail;
        }
    }
    return 1;
fail:
    (void)lid_psp_rollback(s,ops);return 0;
}
static int lid_psp_publish(LidPSPatchSet *s,const R32CallPatchOps *ops){
    const R31PatchOps *o=&ops->memory;
    if(!s||s->state!=LID_PSP_PREPARED||o->quiet(o->context)!=1)return 0;
    for(unsigned i=0;i<s->count;i++){
        LidPSPatchEntry *p=&s->entries[i];
        if(o->quiet(o->context)!=1||!o->readable(o->context,p->target,p->size)||
           __builtin_memcmp(p->target,p->before,p->size)||o->writable(o->context,p->target,p->size,&p->protection)!=1)goto fail;
        p->touched=1;__builtin_memcpy(p->target,p->after,p->size);
        int flushed=o->flush(o->context,p->target,p->size),protected=o->protect(o->context,p->target,p->size,p->protection);
        if(flushed!=1||protected!=1||!lid_psp_owned(p,o))goto fail;
    }
    __atomic_store_n(&s->state,LID_PSP_ACTIVE,__ATOMIC_RELEASE);return 1;
fail:
    (void)lid_psp_rollback(s,ops);return 0;
}
#endif
