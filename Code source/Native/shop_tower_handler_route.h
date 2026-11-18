#ifndef LID_SHOP_TOWER_HANDLER_ROUTE_H
#define LID_SHOP_TOWER_HANDLER_ROUTE_H
#include <stdint.h>
#if !LID_SHOP_EXEC_SEH_ENABLED
#error "Tower handler route requires compiler finally"
#endif
/* Composition within the EXISTING1167730 owner, not a second detour. No
   automatically published provider, backend readiness stub or native grant.
   Once published every host/callback stays resident, even after retirement. */
typedef unsigned long long(*LidTowerHandlerOriginal)(void*,void*,void*);
typedef struct LidTowerHandlerOps {
    void*context;uint32_t(*thread)(void*);uint32_t(*error)(void*);void(*error_set)(void*,uint32_t);
    int(*quiet)(void*);int(*coherent)(void*);
    /*0=native,-1=proven refused AP,1=admitted AP. Publish ticket immediately
       on acquisition so finally can release on a later native exception. */
    int(*begin)(void*,void*,void*,void*,uint64_t*);
    unsigned long long(*reject)(void*,void*,void*); /* actual native error response */
    void(*complete)(void*,uint64_t,unsigned char); /* original returned, low AL only */
    void(*end)(void*,uint64_t,unsigned); /* C metadata only, infallible/idempotent */
} LidTowerHandlerOps;
typedef struct LidTowerHandlerRoute {unsigned prepared,active;uint32_t owner;LidTowerHandlerOps ops;} LidTowerHandlerRoute;
static LidTowerHandlerRoute*g_lid_tower_handler_route;
static int lid_thr_prepare(LidTowerHandlerRoute*h,uint32_t owner,const LidTowerHandlerOps*ops){
    if(!h||h->prepared||!owner||!ops||!ops->thread||ops->thread(ops->context)!=owner||
       !ops->error||!ops->error_set||!ops->quiet||!ops->coherent||!ops->begin||!ops->reject||!ops->complete||!ops->end)return 0;
    h->ops=*ops;h->owner=owner;h->prepared=1;return 1;
}
static int lid_thr_publish(LidTowerHandlerRoute*h){
    if(!h||!h->prepared||h->ops.thread(h->ops.context)!=h->owner||
       h->ops.quiet(h->ops.context)!=1||h->ops.coherent(h->ops.context)!=1)return 0;
    LidTowerHandlerRoute*empty=0;
    if(!__atomic_compare_exchange_n(&g_lid_tower_handler_route,&empty,h,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    __atomic_store_n(&h->active,1,__ATOMIC_RELEASE);return 1;
}
static unsigned long long lid_thr_call(LidTowerHandlerOriginal original,void*api,void*response,void*root){
    LidTowerHandlerRoute*h=__atomic_load_n(&g_lid_tower_handler_route,__ATOMIC_ACQUIRE);
    if(!h||!__atomic_load_n(&h->active,__ATOMIC_ACQUIRE))return original(api,response,root);
    uint32_t incoming=h->ops.error(h->ops.context);uint64_t ticket=0;
    volatile unsigned returned=0;unsigned long long result=0;int status=-1;
    __try{
        status=h->ops.begin(h->ops.context,api,response,root,&ticket);
        h->ops.error_set(h->ops.context,incoming);
        if(status==0&&!ticket)result=original(api,response,root);
        else if(status==1&&ticket){
            result=original(api,response,root);returned=1;
            uint32_t native_error=h->ops.error(h->ops.context);
            /* Native handler ABI returns byte. Preserve its raw legacy return
               to original caller; never compare unspecified upper RAX bits. */
            h->ops.complete(h->ops.context,ticket,(unsigned char)result);
            h->ops.error_set(h->ops.context,native_error);
        }else result=h->ops.reject(h->ops.context,api,response);
    }__finally{
        uint32_t error=h->ops.error(h->ops.context);
        if(ticket)h->ops.end(h->ops.context,ticket,returned);
        h->ops.error_set(h->ops.context,error);
    }
    return result;
}
#endif
