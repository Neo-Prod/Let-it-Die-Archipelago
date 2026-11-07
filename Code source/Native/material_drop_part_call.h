#ifndef LID_MATERIAL_DROP_PART_CALL_R111_H
#define LID_MATERIAL_DROP_PART_CALL_R111_H
#include "performance_events.h"
#include "material_drop_part_signature.h"
/* Native dispatch guard from Ghidra R111. permit must independently prove
   live module, owner/current player, selected bag slot and transaction scope. */
typedef struct R111DropOps {R6Ops memory;void*context;uint32_t owner;uint64_t image;uint32_t(*thread)(void*);int(*permit)(void*,uint64_t,int);} R111DropOps;
typedef struct R111DropReceipt {unsigned invoked;uint64_t player,function,actor;int slot;} R111DropReceipt;
enum {R111_REFUSED=0,R111_SENT=1,R111_ALREADY_SENT=2};
static const uint8_t r111_dispatch_prefix[32]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x0d,0x12,0x15,0x5b,0x02,0x49,0x8b};
static int r111_dispatch(const R111DropOps*o,uint64_t player,uint64_t*out){
 uint64_t vt,target;uint8_t bytes[32];
 if(!o||!out||!o->image||o->image>UINT64_MAX-0x2200000||!r6_copy(&o->memory,player,&vt,8)||vt!=o->image+0x20ac950||
 !r6_at(&o->memory,vt,0x218,&target,8)||target!=o->image+0x3bf4c0||!r6_copy(&o->memory,target,bytes,32)||memcmp(bytes,r111_dispatch_prefix,32))return 0;
 *out=target;return 1;
}
static int r111_same_signature(const R111DropSignature*a,const R111DropSignature*b){
 if(a->layout.function!=b->layout.function||a->layout.extent!=b->layout.extent||a->parms_size!=b->parms_size||a->return_offset!=b->return_offset||a->function_flags!=b->function_flags)return 0;
 for(unsigned i=0;i<8;i++)if(a->layout.offset[i]!=b->layout.offset[i]||a->layout.mask[i]!=b->layout.mask[i]||a->layout.size[i]!=b->layout.size[i])return 0;return 1;
}
static int r111_drop_call_core(R111DropReceipt*r,const R111DropOps*o,uint64_t player,int slot){
 R111DropSignature signature,again;uint64_t dispatch,after;uint8_t params[128];
 if(!r||!o||!o->thread||!o->permit||!o->owner||o->thread(o->context)!=o->owner||slot<0||slot>=99)return R111_REFUSED;
 if(r->invoked)return r->player==player&&r->slot==slot?R111_ALREADY_SENT:R111_REFUSED;
 if(o->permit(o->context,player,slot)!=1||!r111_drop_signature(&o->memory,player,&signature)||!r111_dispatch(o,player,&dispatch)||
 !r110_drop_params(&signature.layout,slot,params)||o->thread(o->context)!=o->owner||o->permit(o->context,player,slot)!=1||
 !r111_drop_signature(&o->memory,player,&again)||!r111_same_signature(&signature,&again)||!r111_dispatch(o,player,&after)||dispatch!=after)return R111_REFUSED;
 /* Latch BEFORE engine dispatch to prevent retry/reentry after side effects.
    SENT is not proof of successful spawn/delete: caller verifies slot outcome. */
 r->player=player;r->slot=slot;r->function=signature.layout.function;r->invoked=1;
 ((void(*)(void*,void*,void*,void*))(uintptr_t)dispatch)((void*)(uintptr_t)player,(void*)(uintptr_t)signature.layout.function,params,0);
 memcpy(&r->actor,params+signature.return_offset,8);return R111_SENT;
}
static int r111_drop_call(R111DropReceipt*r,const R111DropOps*o,uint64_t player,int slot){
 LID_PERF_BEGIN(pickup_drop_call_time);int result=r111_drop_call_core(r,o,player,slot);
 LID_PERF_END(pickup_drop_call_time,47,"pickup_drop_call_checks_dispatch",result);return result;
}
#endif
