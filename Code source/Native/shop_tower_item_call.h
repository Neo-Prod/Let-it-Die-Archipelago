#ifndef LID_SHOP_TOWER_ITEM_CALL_H
#define LID_SHOP_TOWER_ITEM_CALL_H
#include "shop_tower_floor_calls.h"
/* One verified CALL, never a stolen prologue/entry detour. Normal native
   original/unwind stay intact. Resident wrapper/relay retained after failure.
   Real native guard protects code publication; writes are not globally atomic. */
enum {LID_TIC_EMPTY,LID_TIC_PREPARED,LID_TIC_PUBLISHING,LID_TIC_INSTALLED,LID_TIC_RETIRING,LID_TIC_RETIRED,LID_TIC_FAILED};
typedef struct LidTowerItemCall {
    LidTowerFloor*floor;LidTCOps ops;uintptr_t wrapper,relay,destination;
    unsigned state,touched,protection_saved,protection_dirty,protection;uint8_t patched[5];
} LidTowerItemCall;
static const uint8_t lid_tic_native[10]={0xe8,0x93,0xe0,0x17,0x00,0x90,0x8b,0x45,0x80,0x85};
static int lid_tic_current(LidTowerItemCall*c){
    return c&&c->floor&&c->floor->prepared&&c->floor->ops.thread(c->floor->ops.context)==c->floor->owner&&
        c->ops.quiet(c->ops.context)==1;
}
static int lid_tic_match(const LidTowerItemCall*c,int patched){
    uint8_t actual[10];if(!c||!c->floor||!lid_tf_read(c->floor,c->floor->image,0x115e188,actual,10))return 0;
    for(unsigned i=0;i<10;i++)if(actual[i]!=(patched&&i<5?c->patched[i]:lid_tic_native[i]))return 0;
    return 1;
}
static int lid_tic_target(const LidTowerItemCall*c){
    if(!c||!c->destination||c->ops.resident_executable(c->ops.context,c->wrapper)!=1)return 0;
    if(!c->relay)return c->destination==c->wrapper;
    uint8_t actual[14],expected[14]={0xff,0x25,0,0,0,0};
    for(unsigned i=0;i<8;i++)expected[i+6]=(uint8_t)((uint64_t)c->wrapper>>(8*i));
    if(c->destination!=c->relay||c->ops.resident_executable(c->ops.context,c->relay)!=1||
       !lid_tf_read(c->floor,c->relay,0,actual,14))return 0;
    for(unsigned i=0;i<14;i++)if(actual[i]!=expected[i])return 0;return 1;
}
/* Safe on the real native request worker: no UI-only quiet/thread callback,
   UObjects, allocations, native query, file/lock or callback publication. */
static int lid_tic_owned(const LidTowerItemCall*c,uintptr_t caller){
    return c&&__atomic_load_n(&c->state,__ATOMIC_ACQUIRE)==LID_TIC_INSTALLED&&
        caller==c->floor->image+0x115e18d&&!c->protection_dirty&&c->touched&&lid_tic_target(c)&&lid_tic_match(c,1);
}
static int lid_tic_prepare(LidTowerItemCall*c,LidTowerFloor*floor,uintptr_t wrapper,const LidTCOps*ops){
    if(!c||c->state||!floor||!floor->prepared||!wrapper||wrapper==floor->image+0x12dc220||
       !ops||!ops->quiet||!ops->unlocked||!ops->ready||!ops->resident_executable||
       !ops->allocate_near||!ops->private_rw||!ops->protect||!ops->flush)return 0;
    c->floor=floor;c->ops=*ops;c->wrapper=wrapper;
    if(floor->ops.thread(floor->ops.context)!=floor->owner||ops->unlocked(ops->context)!=1||
       ops->resident_executable(ops->context,wrapper)!=1||!lid_tic_match(c,0))goto fail;
    if(lid_tc_relative(floor->image+0x115e188,wrapper,c->patched))c->destination=wrapper;
    else{
        c->relay=(uintptr_t)ops->allocate_near(ops->context,floor->image+0x115e188,14);
        if(!c->relay||c->relay>UINTPTR_MAX-14||ops->private_rw(ops->context,c->relay,14)!=1||
           !lid_tc_relative(floor->image+0x115e188,c->relay,c->patched))goto fail;
        uint8_t code[14]={0xff,0x25,0,0,0,0};unsigned old=0;
        for(unsigned i=0;i<8;i++)code[i+6]=(uint8_t)((uint64_t)wrapper>>(8*i));
        for(unsigned i=0;i<14;i++)((uint8_t*)c->relay)[i]=code[i];
        if(ops->protect(ops->context,c->relay,14,0x20,&old)!=1||ops->flush(ops->context,c->relay,14)!=1)goto fail;
        c->destination=c->relay;
    }
    if(!lid_tic_target(c))goto fail;
    __atomic_store_n(&c->state,LID_TIC_PREPARED,__ATOMIC_RELEASE);return 1;
fail:__atomic_store_n(&c->state,LID_TIC_FAILED,__ATOMIC_RELEASE);return 0;
}
static int lid_tic_write(LidTowerItemCall*c,const uint8_t bytes[5]){
    unsigned old=0,ignored=0;uintptr_t at=c->floor->image+0x115e188;
    if(!lid_tic_current(c)||c->ops.protect(c->ops.context,at,5,0x40,&old)!=1)return 0;
    if(!c->protection_saved){c->protection=old;c->protection_saved=1;}
    c->protection_dirty=1;
    for(unsigned i=0;i<5;i++)((uint8_t*)at)[i]=bytes[i];
    int flushed=c->ops.flush(c->ops.context,at,5)==1;
    int restored=c->ops.protect(c->ops.context,at,5,c->protection,&ignored)==1;
    if(restored)c->protection_dirty=0;return flushed&&restored;
}
static int lid_tic_revert(LidTowerItemCall*c){
    if(!lid_tic_current(c))return 0;
    __atomic_store_n(&c->state,LID_TIC_RETIRING,__ATOMIC_RELEASE);
    int ok=0;unsigned ignored=0;
    if(!c->touched&&!c->protection_dirty){ok=1;goto done;}
    if(lid_tic_match(c,0)){
        if(c->protection_dirty&&c->ops.protect(c->ops.context,c->floor->image+0x115e188,5,c->protection,&ignored)!=1)goto done;
        c->protection_dirty=0;c->touched=0;ok=1;goto done;
    }
    if(!lid_tic_match(c,1))goto done; /* never overwrite a foreign owner */
    if(lid_tic_write(c,lid_tic_native)&&lid_tic_match(c,0)){c->touched=0;ok=1;}
done:__atomic_store_n(&c->state,ok?LID_TIC_RETIRED:LID_TIC_FAILED,__ATOMIC_RELEASE);return ok;
}
static int lid_tic_publish(LidTowerItemCall*c){
    if(!lid_tic_current(c)||__atomic_load_n(&c->state,__ATOMIC_ACQUIRE)!=LID_TIC_PREPARED||
       c->ops.ready(c->ops.context)!=1||!lid_tic_target(c))return 0;
    uint32_t error=c->floor->ops.error_get(c->floor->ops.context);volatile unsigned normal=0;int ok=0;
    __try{
        if(!lid_tic_match(c,0))goto done;
        __atomic_store_n(&c->state,LID_TIC_PUBLISHING,__ATOMIC_RELEASE);c->touched=1;
        if(!lid_tic_write(c,c->patched)||!lid_tic_match(c,1))goto done;
        __atomic_store_n(&c->state,LID_TIC_INSTALLED,__ATOMIC_RELEASE);ok=1;
done:if(!ok){(void)lid_tic_revert(c);__atomic_store_n(&c->state,LID_TIC_FAILED,__ATOMIC_RELEASE);}normal=1;
    }__finally{
        if(!normal)__atomic_store_n(&c->state,LID_TIC_FAILED,__ATOMIC_RELEASE);
        c->floor->ops.error_set(c->floor->ops.context,error);
    }
    return ok;
}
#endif
