#ifndef LID_SHOP_SAVE_OBSERVER_H
#define LID_SHOP_SAVE_OBSERVER_H
#include "shop_save_scope.h"
#include "shop_ap_save_origin.h"
/* Metadata observer only. No file/network/native calls, no Save result changes.
   Caller configures immutable resident bindings before publishing Save scope.
   Events are NOT durable proof or AP run authentication. */
typedef struct LidShopSaveEvent {
    uint64_t process_epoch,sequence,batch;uint32_t thread;
    unsigned stage,kind,ticket,native_result;
} LidShopSaveEvent;
typedef struct LidShopSaveObserver {
    uintptr_t base;unsigned ready,owner,read_at,count,dropped,pending;uint32_t writer;
    const LidShopSaveScope*scope;LidShopSaveOriginOps source;
    const LidShopApSaveBinding*ap[2];LidShopSaveEvent events[64];
} LidShopSaveObserver;
static int lid_shop_save_observer_prepare(LidShopSaveObserver*o,const LidShopSaveScope*s,
    uintptr_t base,int exact_sha,const LidShopSaveOriginOps*source,
    const LidShopApSaveBinding*progression,const LidShopApSaveBinding*coins){
    if(!o||o->ready||!s||!s->initialized||!base||base>UINTPTR_MAX-0xf8a5508||exact_sha!=1||
       !source||!source->copy||!source->thread||(progression&&!progression->ready)||(coins&&!coins->ready))return 0;
    o->base=base;o->scope=s;o->writer=s->writer;o->source=*source;o->ap[0]=progression;o->ap[1]=coins;o->ready=1;return 1;
}
static void lid_shop_save_observe(void*context,const LidShopSaveScope*s,const LidShopSaveFrame*f,
                                  unsigned stage,unsigned char result){
    LidShopSaveObserver*o=context;LidShopSaveOrigin native;LidShopApSaveOrigin ap;
    if(!o||!o->ready||s!=o->scope||!f||lid_shop_save_frame(s)!=f||(stage!=1&&stage!=2))return;
    uint32_t tid=o->source.thread(o->source.context);unsigned empty=0;
    if(!tid||!__atomic_compare_exchange_n(&o->owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
        __atomic_add_fetch(&o->dropped,1,__ATOMIC_RELAXED);return;
    }
    if(o->count==64){__atomic_add_fetch(&o->dropped,1,__ATOMIC_RELAXED);__atomic_store_n(&o->owner,0,__ATOMIC_RELEASE);return;}
    LidShopSaveEvent e={s->process_epoch,f->sequence,0,tid,stage,0,0,stage==2?result:0};
    e.kind=(unsigned)lid_shop_save_origin(&o->source,o->base,1,o->writer,f->runner,f->text,f->caller,&native);
    if(!e.kind)for(unsigned i=0;i<2;i++)if(o->ap[i]&&
       lid_shop_ap_save_origin(o->ap[i],&o->source,o->base,1,o->writer,f->runner,f->text,f->caller,&ap)){
        e.kind=3+i;e.ticket=ap.ticket;e.batch=ap.batch;break;
    }
    o->events[(o->read_at+o->count)%64]=e;o->count++;
    __atomic_store_n(&o->pending,o->count,__ATOMIC_RELEASE);
    __atomic_store_n(&o->owner,0,__ATOMIC_RELEASE);
}
/* Main writer drains one copied event. No native pointer escapes invocation.
   Absence/overflow is lost observation, never lost/fake native Save. */
static int lid_shop_save_event_take(LidShopSaveObserver*o,LidShopSaveEvent*out){
    if(!o||!o->ready||!out||o->source.thread(o->source.context)!=o->writer)return 0;
    unsigned empty=0;if(!__atomic_compare_exchange_n(&o->owner,&empty,o->writer,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    int got=o->count!=0;if(got){*out=o->events[o->read_at];o->read_at=(o->read_at+1)%64;o->count--;}
    __atomic_store_n(&o->pending,o->count,__ATOMIC_RELEASE);
    __atomic_store_n(&o->owner,0,__ATOMIC_RELEASE);return got;
}
#endif
