#ifndef LID_SHOP_TOWER_FLOOR_CALLS_H
#define LID_SHOP_TOWER_FLOOR_CALLS_H
#include "shop_tower_floor.h"
/* Source owner ONLY: no runtime caller yet. Redirect verified native CALLs,
   NEVER steal the builder prologue. Original entry/unwind stay untouched.
   Optional near relay is JMP-only, no stack/register changes. All published
   owner/wrapper/relay memory remains resident, including partial failures. */
enum {LID_TC_EMPTY,LID_TC_PREPARED,LID_TC_PUBLISHING,LID_TC_INSTALLED,LID_TC_RETIRED,LID_TC_FAILED};
typedef struct LidTCOps {
    void*context;int(*quiet)(void*);int(*unlocked)(void*);int(*ready)(void*);
    int(*resident_executable)(void*,uintptr_t);
    void*(*allocate_near)(void*,uintptr_t,size_t);
    int(*private_rw)(void*,uintptr_t,size_t);
    int(*protect)(void*,uintptr_t,size_t,unsigned,unsigned*);
    int(*flush)(void*,uintptr_t,size_t);
} LidTCOps;
typedef struct LidTowerFloorCalls {
    LidTowerFloor*floor;LidTCOps ops;uintptr_t wrapper,relay,destination;
    unsigned state,touched,protection_saved,protection_dirty,protection[7];uint8_t patched[7][5];
} LidTowerFloorCalls;
static const unsigned lid_tc_rva[7]={0x111cec6,0x111db0a,0x115158e,0x115a1a5,0x115a604,0x11b524a,0x115cc71};
static const uint8_t lid_tc_native[7][10]={
    {0xe8,0xd5,0x02,0x0e,0x00,0x49,0x8d,0x8d,0x90,0x03},
    {0xe8,0x91,0xf6,0x0d,0x00,0x48,0x8b,0x15,0x7a,0xb4},
    {0xe8,0x0d,0xbc,0x0a,0x00,0x48,0x8d,0x8b,0x90,0x03},
    {0xe8,0xf6,0x2f,0x0a,0x00,0x48,0x8d,0x8d,0xe0,0x03},
    {0xe8,0x97,0x2b,0x0a,0x00,0x48,0x8d,0x8e,0xe0,0x03},
    {0xe8,0x51,0x7f,0x04,0x00,0xf3,0x44,0x0f,0x10,0x0d},
    {0xe8,0x2a,0x05,0x0a,0x00,0x48,0x8d,0x8f,0x90,0x03}};
static int lid_tc_relative(uintptr_t call,uintptr_t destination,uint8_t out[5]){
    if(!out||call>UINTPTR_MAX-5)return 0;
    uintptr_t next=call+5;uint32_t relative;
    if(destination>=next){uintptr_t delta=destination-next;if(delta>INT32_MAX)return 0;relative=(uint32_t)delta;}
    else{uintptr_t delta=next-destination;if(delta>0x80000000ULL)return 0;relative=0u-(uint32_t)delta;}
    out[0]=0xe8;for(unsigned i=0;i<4;i++)out[i+1]=(uint8_t)(relative>>(i*8));return 1;
}
static int lid_tc_current(LidTowerFloorCalls*c){
    return c&&c->floor&&c->floor->prepared&&c->floor->ops.thread(c->floor->ops.context)==c->floor->owner&&
        c->ops.quiet(c->ops.context)==1;
}
static int lid_tc_match(LidTowerFloorCalls*c,unsigned i,int patched){
    uint8_t actual[10];if(i>=7||!lid_tf_read(c->floor,c->floor->image,lid_tc_rva[i],actual,10))return 0;
    for(unsigned j=0;j<10;j++)if(actual[j]!=(patched&&j<5?c->patched[i][j]:lid_tc_native[i][j]))return 0;
    return 1;
}
static int lid_tc_original(void*context,uintptr_t entry,uintptr_t original,unsigned copied){
    LidTowerFloorCalls*c=context;
    if(!lid_tc_current(c)||entry!=c->floor->image+0x11fd1a0||original!=entry||copied!=15||
       c->state<LID_TC_PREPARED||c->state>LID_TC_INSTALLED||!lid_tf_prefix(c->floor)||
       c->ops.resident_executable(c->ops.context,c->wrapper)!=1)return 0;
    for(unsigned i=0;i<7;i++)if(!lid_tc_match(c,i,(c->touched&(1u<<i))!=0))return 0;
    return 1;
}
static int lid_tc_destination(LidTowerFloorCalls*c);
static int lid_tc_prepare(LidTowerFloorCalls*c,LidTowerFloor*floor,uintptr_t wrapper,const LidTCOps*ops){
    if(!c||c->state||!floor||!floor->prepared||floor->original||floor->active||floor->stopped||
       !wrapper||wrapper==floor->image+0x11fd1a0||!ops||!ops->quiet||!ops->unlocked||!ops->ready||
       !ops->resident_executable||!ops->allocate_near||!ops->private_rw||!ops->protect||!ops->flush)return 0;
    c->floor=floor;c->ops=*ops;c->wrapper=wrapper;
    if(floor->ops.thread(floor->ops.context)!=floor->owner||ops->unlocked(ops->context)!=1||
       !lid_tf_prefix(floor)||ops->resident_executable(ops->context,wrapper)!=1)goto fail;
    for(unsigned i=0;i<7;i++)if(!lid_tc_match(c,i,0))goto fail;
    if(!lid_tc_destination(c))goto fail;
    c->state=LID_TC_PREPARED;return 1;
fail:c->state=LID_TC_FAILED;return 0;
}
/* OS code protection helpers; publisher requires held real quiescence. Writes
   cover just CALL bytes, never its following instruction or the native entry. */
static int lid_tc_write(LidTowerFloorCalls*c,unsigned i,const uint8_t bytes[5]){
    unsigned old=0,ignored=0;uintptr_t at=c->floor->image+lid_tc_rva[i];
    if(!lid_tc_current(c)||c->ops.protect(c->ops.context,at,5,0x40,&old)!=1)return 0;
    unsigned bit=1u<<i;
    if(!(c->protection_saved&bit)){c->protection[i]=old;c->protection_saved|=bit;}
    c->protection_dirty|=bit;
    for(unsigned j=0;j<5;j++)((uint8_t*)at)[j]=bytes[j];
    int flushed=c->ops.flush(c->ops.context,at,5)==1;
    int restored=c->ops.protect(c->ops.context,at,5,c->protection[i],&ignored)==1;
    if(restored)c->protection_dirty&=~bit;
    return flushed&&restored;
}
static int lid_tc_revert(LidTowerFloorCalls*c){
    int ok=1;if(!lid_tc_current(c))return 0;
    __atomic_store_n(&c->floor->active,0,__ATOMIC_RELEASE);
    for(unsigned n=7;n;n--){unsigned i=n-1,bit=1u<<i;if(!(c->touched&bit))continue;
        if(lid_tc_match(c,i,0)){
            unsigned ignored=0;
            if((c->protection_dirty&bit)&&c->ops.protect(c->ops.context,c->floor->image+lid_tc_rva[i],5,c->protection[i],&ignored)!=1){ok=0;continue;}
            c->protection_dirty&=~bit;c->touched&=~bit;continue;
        }
        if(!lid_tc_match(c,i,1)||!lid_tc_write(c,i,lid_tc_native[i])||!lid_tc_match(c,i,0)){ok=0;continue;}
        c->touched&=~bit;
    }
    int restored=ok&&!c->touched&&!c->protection_dirty;
    if(restored&&c->state==LID_TC_INSTALLED)c->state=LID_TC_RETIRED;return restored;
}
static int lid_tc_destination(LidTowerFloorCalls*c){
    int near=1;uintptr_t image=c->floor->image;
    if(c->ops.unlocked(c->ops.context)!=1)return 0;
    for(unsigned i=0;i<7;i++)if(!lid_tc_relative(image+lid_tc_rva[i],c->wrapper,c->patched[i]))near=0;
    if(near){c->destination=c->wrapper;return 1;}
    /* R32 near allocator provider must return a private RW allocation. No retry,
       code emulation, prologue relocation or executable fixture in this owner. */
    c->relay=(uintptr_t)c->ops.allocate_near(c->ops.context,image+0x1160000,14);
    if(!c->relay||c->relay>UINTPTR_MAX-14||c->ops.private_rw(c->ops.context,c->relay,14)!=1)return 0;
    uint8_t code[14]={0xff,0x25,0,0,0,0};
    for(unsigned i=0;i<8;i++)code[i+6]=(uint8_t)((uint64_t)c->wrapper>>(i*8));
    for(unsigned i=0;i<7;i++)if(!lid_tc_relative(image+lid_tc_rva[i],c->relay,c->patched[i]))return 0;
    for(unsigned i=0;i<14;i++)((uint8_t*)c->relay)[i]=code[i];
    unsigned old=0;
    if(c->ops.protect(c->ops.context,c->relay,14,0x20,&old)!=1||
       c->ops.flush(c->ops.context,c->relay,14)!=1||
       c->ops.resident_executable(c->ops.context,c->relay)!=1)return 0;
    uint8_t check[14];if(!lid_tf_read(c->floor,c->relay,0,check,14))return 0;
    for(unsigned i=0;i<14;i++)if(check[i]!=code[i])return 0;
    c->destination=c->relay;return 1;
}
#if defined(LID_SHOP_FLOOR_SEH_ENABLED) && LID_SHOP_FLOOR_SEH_ENABLED
/* INSTALLED means redirected native CALLs, NOT enabled AP normalization. Real
   coherent tower readiness is required separately by lid_tc_enable. */
static int lid_tc_publish(LidTowerFloorCalls*c){
    if(!lid_tc_current(c)||c->state!=LID_TC_PREPARED||c->floor->active||c->floor->stopped)return 0;
    uint32_t error=c->floor->ops.error_get(c->floor->ops.context);unsigned normal=0;int ok=0;
    __try{
        for(unsigned i=0;i<7;i++)if(!lid_tc_match(c,i,0))goto done;
        if(!lid_tf_prefix(c->floor)||!c->destination||
           !lid_tf_bind_original(c->floor,(LidTFOriginal)(c->floor->image+0x11fd1a0)))goto done;
        c->state=LID_TC_PUBLISHING;
        for(unsigned i=0;i<7;i++){
            if(!lid_tc_match(c,i,0))goto done;
            c->touched|=1u<<i;
            if(!lid_tc_write(c,i,c->patched[i])||!lid_tc_match(c,i,1))goto done;
        }
        c->state=LID_TC_INSTALLED;ok=1;
done:
        if(!ok){lid_tc_revert(c);c->state=LID_TC_FAILED;}normal=1;
    }__finally{
        if(!normal){__atomic_store_n(&c->floor->active,0,__ATOMIC_RELEASE);c->state=LID_TC_FAILED;}
        c->floor->ops.error_set(c->floor->ops.context,error);
    }return ok;
}
#endif
static int lid_tc_enable(LidTowerFloorCalls*c){
    if(!lid_tc_current(c)||c->state!=LID_TC_INSTALLED||c->touched!=127||c->protection_dirty||c->floor->stopped||
       c->floor->original!=(LidTFOriginal)(c->floor->image+0x11fd1a0)||
       c->ops.ready(c->ops.context)!=1||!lid_tf_prefix(c->floor))return 0;
    for(unsigned i=0;i<7;i++)if(!lid_tc_match(c,i,1))return 0;
    __atomic_store_n(&c->floor->active,1,__ATOMIC_RELEASE);return 1;
}
#endif
