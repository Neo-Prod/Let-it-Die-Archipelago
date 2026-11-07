#ifndef LID_SHOP_NATIVE_BINDING_H
#define LID_SHOP_NATIVE_BINDING_H
/* Requires existing mod.c definitions and progression_hub_observer.h first.
   Native read/owned-temporary bindings only. No detours or installation here.
   Init a PRIVATE zeroed context on the known game thread, then publish it.
   Do not reinitialize while any operation uses it. Encoder buffers are private
   to each caller and distinct from all source allocations. */
#include "shop_history_codec.h"
#include "shop_history_adapter.h"
#include "shop_context.h"
#include "shop_context_cache.h"
#include "shop_batch_ledger.h"
#include "shop_price_view.h"
#include "shop_coordinator.h"
typedef void (WINAPI *LidShopNativeDecode)(void*,void*);
typedef void (WINAPI *LidShopNativeRelease)(void*);
typedef struct LidShopNativeBinding {
    BYTE* exe;BYTE* game;
    char slot[21];int uid;
    unsigned long long serial;
    DWORD game_thread;
    unsigned valid;
    volatile unsigned busy,poisoned;
    LidShopNativeDecode decode;
    LidShopNativeRelease release;
    LidShopPropertyCache panel_cache;
} LidShopNativeBinding;
static int shop_binding_enter(LidShopNativeBinding* b){
    unsigned expected=0;
    if(!b||__atomic_load_n(&b->poisoned,__ATOMIC_ACQUIRE))return 0;
    if(!__atomic_compare_exchange_n(&b->busy,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
        __atomic_store_n(&b->poisoned,1,__ATOMIC_RELEASE);return 0;
    }
    if(__atomic_load_n(&b->poisoned,__ATOMIC_ACQUIRE)){
        __atomic_store_n(&b->busy,0,__ATOMIC_RELEASE);return 0;
    }
    return 1;
}
static void shop_binding_leave(LidShopNativeBinding* b){
    if(b)__atomic_store_n(&b->busy,0,__ATOMIC_RELEASE);
}
static unsigned long long shop_binding_session(void* context){
    LidShopNativeBinding* b=context;unsigned i;
    if(!b||!b->valid||!b->serial||__atomic_load_n(&b->poisoned,__ATOMIC_ACQUIRE)||
       !__atomic_load_n(&g_ap_mode_active,__ATOMIC_ACQUIRE)||
       !__atomic_load_n(&g_savedata_route_active,__ATOMIC_ACQUIRE)||!g_v074_cfg.valid||
       b->exe!=g_progress_event_exe||progress_native_game(b->exe)!=b->game||
       !readable_range(b->exe+0xf8b0088,4)||*(int*)(b->exe+0xf8b0088)!=b->uid)return 0;
    for(i=0;i<21;i++)if(b->slot[i]!=g_release_slot_key[i])return 0;
    return b->serial;
}
static int shop_binding_copy(void* context,const void* source,void* destination,unsigned size){
    (void)context;
    if(!source||!destination||!readable_range(source,size))return 0;
    lid_memcpy(destination,source,size);return 1;
}
static int shop_binding_json_class(void* object){
    return lid_event_class_is(object,"JsonObject",&g_progress_event_ops)||
           lid_event_class_is(object,"BrgNetworkRequestContent",&g_progress_event_ops);
}
static int shop_binding_decode(void* context,void* json,unsigned char* row){
    LidShopNativeBinding* b=context;
    if(!row||!shop_binding_session(context)||!b->decode||!shop_binding_json_class(json)||
       !readable_range(json,0x120))return 0;
    b->decode(json,row);return 1;
}
static void shop_binding_release(void* context,unsigned char* row){
    LidShopNativeBinding* b=context;
    /* A failed session check MUST NOT skip release of an owned temporary. */
    if(b&&b->release&&row)b->release(row);
}
static int shop_binding_init(LidShopNativeBinding* b,BYTE* exe,unsigned long long serial){
    static const BYTE decoder[]={0x40,0x55,0x48,0x8b,0xec,0x48,0x83,0xec,0x50,0x48,0xc7,0x45,0xd0,0xfe,0xff,0xff,0xff};
    static const BYTE destructor[]={0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x70,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff};
    BYTE* game;unsigned i;DWORD tid;
    if(!b||b->valid||!exe||!serial||!g_get_tid||exe!=g_progress_event_exe)return 0;
    tid=g_get_tid();
    if(!tid||tid!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE)||
       !g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||
       !(game=progress_native_game(exe))||!readable_range(exe+0xf8b0088,4)||
       !executable_ptr(exe,exe+0xe80e50)||!executable_ptr(exe,exe+0xe1d720)||
       !readable_range(exe+0xe80e50,sizeof(decoder))||!readable_range(exe+0xe1d720,sizeof(destructor))||
       lid_memcmp(exe+0xe80e50,decoder,sizeof(decoder))||lid_memcmp(exe+0xe1d720,destructor,sizeof(destructor)))return 0;
    for(i=0;i<20;i++){
        char c=g_release_slot_key[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;
        b->slot[i]=c;
    }
    if(g_release_slot_key[20])return 0;
    b->slot[20]=0;b->exe=exe;b->game=game;b->uid=*(int*)(exe+0xf8b0088);
    b->serial=serial;b->game_thread=tid;b->busy=0;b->poisoned=0;
    b->decode=(LidShopNativeDecode)(exe+0xe80e50);b->release=(LidShopNativeRelease)(exe+0xe1d720);
    b->valid=1;return shop_binding_session(b)!=0;
}
static LidShopHistoryAdapterOps shop_binding_ops(LidShopNativeBinding* b){
    LidShopHistoryAdapterOps ops={b,shop_binding_copy,shop_binding_session,shop_binding_decode,shop_binding_release};
    return ops;
}
static int shop_binding_json_source(LidShopNativeBinding* b,void* json,LidShopHistorySource* out){
    BYTE header[16];BYTE* value;unsigned count,cap;
    if(!out||!shop_binding_session(b)||!shop_binding_json_class(json))return 0;
    value=lid_shop_named_value(json,"ObjectArray","ArrayProperty",16,0x120,&g_progress_event_ops);
    if(!value||!shop_binding_copy(b,value,header,16))return 0;
    count=lid_shop_history_u32(header+8);cap=lid_shop_history_u32(header+12);
    if(count>256||count>cap||cap>65536||(count&&!lid_shop_history_u64(header)))return 0;
    out->data=(void*)(size_t)lid_shop_history_u64(header);out->count=count;out->stride=8;out->json_pointers=1;
    return !count||readable_range(out->data,(size_t)count*8);
}
static int shop_binding_active_panel(LidShopNativeBinding* b,LidShopContext* shop){
    BYTE* stage;void** vt;LidShopCachedView view;
    BYTE (WINAPI *is_hub)(void*);
    if(!shop||!shop_binding_session(b)||!g_get_tid||g_get_tid()!=b->game_thread||
       b->game_thread!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return 0;
    if(!lid_shop_cache_view(&b->panel_cache,b->game,&g_progress_event_ops,&view))return 0;
    /* Only native location predicate on the already identified game thread. */
    stage=*(BYTE**)(b->game+0x13e8);
    if(!stage||!readable_range(stage,0x58)||*(void**)stage!=b->exe+0x21c3350)return 0;
    vt=*(void***)stage;
    if(!readable_range(vt,0x3d8)||vt[0x3d0/8]!=b->exe+0x1433900)return 0;
    is_hub=(BYTE (WINAPI *)(void*))vt[0x3d0/8];if(!is_hub(stage))return 0;
    shop->shop=view.shop;shop->item_select=view.panel;return 1;
}
static int shop_binding_active_history(LidShopNativeBinding* b,LidShopContext* shop,LidShopHistorySource* out){
    BYTE* value;BYTE header[16];unsigned count,cap;
    if(!out||!shop_binding_active_panel(b,shop))return 0;
    value=lid_shop_cache_history(&b->panel_cache,shop->shop,&g_progress_event_ops);
    if(!value||!shop_binding_copy(b,value,header,16))return 0;
    count=lid_shop_history_u32(header+8);cap=lid_shop_history_u32(header+12);
    if(count>256||count>cap||cap>65536||(count&&!lid_shop_history_u64(header)))return 0;
    out->data=(void*)(size_t)lid_shop_history_u64(header);out->count=count;out->stride=0x94;out->json_pointers=0;
    return (!count||readable_range(out->data,(size_t)count*0x94))&&shop_binding_session(b)!=0;
}
/* Bridge actual native readers/lock/session into the tested coordinator.
   Scope depth is supplied by the detour's future thread-local scope manager;
   no shared global depth and no assumption that backend == game thread. */
typedef struct LidShopNativeCoordinatorContext {
    LidShopNativeBinding* binding;
    void* depth_context;
    unsigned (*depth)(void*);
} LidShopNativeCoordinatorContext;
static unsigned long long shop_coord_native_session(void* p){
    LidShopNativeCoordinatorContext* c=p;return c?shop_binding_session(c->binding):0;
}
static unsigned shop_coord_native_thread(void* p){(void)p;return g_get_tid?g_get_tid():0;}
static unsigned shop_coord_native_depth(void* p){
    LidShopNativeCoordinatorContext* c=p;return c&&c->depth?c->depth(c->depth_context):0;
}
static int shop_coord_native_enter(void* p){
    LidShopNativeCoordinatorContext* c=p;return c&&shop_binding_enter(c->binding);
}
static void shop_coord_native_leave(void* p){
    LidShopNativeCoordinatorContext* c=p;if(c)shop_binding_leave(c->binding);
}
static void shop_coord_native_poison(void* p){
    LidShopNativeCoordinatorContext* c=p;
    if(c&&c->binding)__atomic_store_n(&c->binding->poisoned,1,__ATOMIC_RELEASE);
}
static LidShopCoordinatorOps shop_binding_coordinator_ops(LidShopNativeCoordinatorContext* c){
    LidShopCoordinatorOps o={0};
    if(!c||!c->binding)return o;
    o.context=c;o.reader=shop_binding_ops(c->binding);o.session=shop_coord_native_session;
    o.thread=shop_coord_native_thread;o.depth=shop_coord_native_depth;
    o.enter=shop_coord_native_enter;o.leave=shop_coord_native_leave;o.poison=shop_coord_native_poison;
    return o;
}
#endif
