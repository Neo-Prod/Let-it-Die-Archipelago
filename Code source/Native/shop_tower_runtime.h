#ifndef LID_SHOP_TOWER_RUNTIME_H
#define LID_SHOP_TOWER_RUNTIME_H
/* Included after actual R254 API/session helpers. Separate tower preparation,
   never dependent on opening Momoka. No game/receipt/reward/Save changes here. */
#ifndef LID_SHOP_FLOOR_SEH_ENABLED
#define LID_SHOP_FLOOR_SEH_ENABLED LID_SHOP_EXEC_SEH_ENABLED
#endif
#if !LID_SHOP_FLOOR_SEH_ENABLED
#error "Tower floor owner requires compiler-finally support"
#endif
#include "shop_tower_floor_calls.h"
#include "shop_tower_ui_meta.h"
#include "shop_tower_memory.h"
#include "shop_tower_ui_current.h"
#include "shop_tower_purchase_ui.h"
#include "shop_tower_payment.h"
#include "shop_tower_backend.h"
#include "shop_tower_item_call.h"
#include "shop_tower_receipt_queue.h"
#include "shop_tower_icon_provider.h"
#include "shop_tower_auto_view.h"
#include "shop_tower_display_event.h"
#include "shop_tower_image_event.h"
#include "shop_tower_appearance.h"
enum {R267_COLD,R267_PREPARED,R267_PUBLISHED,R267_FAILED};
/* Future real tower UI/payment/display composition supplies this resident
   capability. NULL is NOT ready, and preparation NEVER supplies a fake gate. */
typedef struct R267Coherent {
    void*context;int(*ready)(void*);uint32_t owner;unsigned goal;
    R15Context session;uint8_t catalogue[32];
} R267Coherent;
typedef struct R267Tower {
    R254Momoka*parent;unsigned phase,pinned;R15Context context;unsigned goal;
    uint8_t full[32],tower[32];uintptr_t allocation;uint64_t publication_began;
    LidTowerFloor floor;LidTowerFloorCalls calls;LidMomokaInstallGuard guard;
    const R267Coherent*coherent;
    LidTowerUiMeta ui_meta;uint64_t metadata_began; /* append, existing private offsets preserved */
    LidTowerUiCurrent ui_current;
    LidTowerQueue ui_queue;LidTowerUiExec ui_execution;
    LidTowerPaymentNative payment_native; /* cold immutable ABI, no request admission */
    LidTowerBackend backend;uintptr_t item_relay;
    LidTowerItemCall item_call;uintptr_t item_allocation;
    LidTowerReceiptPort receipts;
    LidTowerAPImage ap_image;LidTowerIconProvider icons;int visual_state;
    LidTowerAutoView text_view;int text_state;uint64_t text_began;
    LidTowerDisplayEvent display_events;int display_event_state;
    LidTowerImageBinding image_bindings;LidTowerImageEvent image_events;int image_event_state;
    LidTowerAppearance appearance;uint64_t appearance_began,appearance_next;
    R267Coherent capability;unsigned capability_failed;
    LidTowerMemory memory;
} R267Tower;
static R267Tower g_shop_tower;
static int r296_owned(R267Tower*);
static int r270_queue_session(void*);
static uint64_t r267_floor_entry(void*model,void*output){return lid_tf_call(&g_shop_tower.floor,model,output);}
static const LidShopRuntimeCatalogue*r267_join(R254Momoka*r){
    R26Transport*t=&g_checks_delivery;const LidShopPolicy*p=&t->shop_policy;
    const LidShopRuntimeCatalogue*c=&t->shop_catalogue.catalogue;
    if(!r||r->phase<R254_GUARDED||r->phase>R254_ACTIVE||!r254_session(r)||
       __atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)||__atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||
       lid_sm_mode(p)!=LID_SM_ENABLED||__atomic_load_n(&t->shop_catalogue_result,__ATOMIC_ACQUIRE)!=LID_SC_READY||
       !__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||c->ui!=r->owner||p->ui!=r->owner||
       c->worker!=p->worker||c->goal!=p->goal||c->goal<10||c->goal>40||c->goal%10||
       !r15_context_valid(&t->queue.context)||!r15_same_context(&c->context,&p->context)||
       !r15_same_context(&c->context,&t->queue.context)||!r15_equal(c->full,p->full,32))return 0;
    return c;
}
static int r267_live(R267Tower*h){
    const LidShopRuntimeCatalogue*c=h?r267_join(h->parent):0;
    return c&&h->phase!=R267_FAILED&&h->pinned&&c->goal==h->goal&&
        r15_same_context(&c->context,&h->context)&&r15_equal(c->full,h->full,32)&&r15_equal(c->tower_hash,h->tower,32);
}
static int r267_wednesday(R267Tower*h){
    LidTSString s;uint16_t label[5];
    return r254_copy(h->parent,(uintptr_t)h->parent->image+0xf8b5194,&s,16)&&s.count==5&&
        s.capacity>=5&&s.capacity<=4096&&r254_copy(h->parent,(uintptr_t)s.data,label,10)&&
        label[0]=='4'&&label[1]=='H'&&label[2]=='M'&&label[3]=='A'&&!label[4];
}
static int r267_ready(void*context){
    R267Tower*h=context;const R267Coherent*c=h?h->coherent:0;
    return c&&c->ready&&r296_owned(h)&&h->appearance.phase==2&&!h->appearance.busy&&
        h->appearance.catalogue==&g_checks_delivery.shop_catalogue.catalogue&&h->appearance.goal==h->goal&&h->visual_state==1&&h->ap_image.phase==3&&h->icons.ready&&
        h->image_event_state==1&&h->image_bindings.prepared&&!h->image_bindings.stopped&&!h->image_bindings.busy&&
        h->image_events.prepared&&!h->image_events.stopped&&!h->image_events.depth&&!h->image_events.drawing&&!h->image_events.protection_dirty&&
        h->text_state==1&&h->text_view.prepared&&!h->text_view.stopped&&!h->text_view.busy&&
        h->display_event_state==1&&h->display_events.prepared&&!h->display_events.stopped&&
        !h->display_events.busy&&!h->display_events.protection_dirty&&
        r276_tower_ready(&g_checks_delivery)&&r267_live(h)&&h->backend.prepared&&!h->backend.stopped&&!h->backend.sale.stopped&&
        __atomic_load_n(&h->item_call.state,__ATOMIC_ACQUIRE)>=LID_TIC_PREPARED&&
        __atomic_load_n(&h->item_call.state,__ATOMIC_ACQUIRE)<=LID_TIC_INSTALLED&&!h->item_call.protection_dirty&&
        h->ui_execution.prepared&&!h->ui_execution.stopped&&
        __atomic_load_n(&h->ui_queue.ready,__ATOMIC_ACQUIRE)&&
        !h->ui_execution.protection_dirty&&!__atomic_load_n(&h->ui_queue.halted,__ATOMIC_ACQUIRE)&&
        !__atomic_load_n(&h->floor.stopped,__ATOMIC_ACQUIRE)&&
        h->calls.state!=LID_TC_FAILED&&!h->calls.protection_dirty&&
        c->owner==h->parent->owner&&c->goal==h->goal&&
        r15_same_context(&c->session,&h->context)&&r15_equal(c->catalogue,h->tower,32)&&
        c->ready(c->context)==1;
}
static int r269_ui_live(void*context){R267Tower*h=context;return r267_live(h)&&r267_wednesday(h);}
/* The verified builder is called synchronously by native response producers.
   Do not call r267_ready/r254_session here: both require the UI thread. The
   active release flag is published only after full UI composition succeeds.
   No UI traversal, pointer deferral, disk IO or engine call in this admission. */
static int r317_floor_producer(void*context){
    R267Tower*h=context;int uid;
    return h&&h->parent&&h->calls.state==LID_TC_INSTALLED&&
        __atomic_load_n(&h->floor.active,__ATOMIC_ACQUIRE)&&
        !__atomic_load_n(&h->floor.stopped,__ATOMIC_ACQUIRE)&&
        !__atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)&&
        __atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)==R26_ACTIVE&&
        r270_queue_session(h)&&r267_wednesday(h)&&
        r254_copy(h->parent,(uintptr_t)h->parent->image+0xf8b5090,&uid,4)&&uid==h->parent->uid;
}
static int r267_admit(void*context,unsigned goal,const char*room,unsigned shop){
    R267Tower*h=context;
    if(!r317_floor_producer(h)||goal!=h->goal)return 0;
    const LidShopRuntimeCatalogue*c=&g_checks_delivery.shop_catalogue.catalogue;
    for(unsigned i=0;i<c->tower_count;i++)if(c->tower[i].shopid==shop&&lid_ts_room(room,c->tower[i].room_key))return 1;
    return 0;
}
static int r267_read(void*context,uintptr_t p,void*out,size_t n){R267Tower*h=context;return r254_copy(h->parent,p,out,n);}
static uint32_t r267_thread(void*context){R267Tower*h=context;return r254_thread(h->parent);}
static uint32_t r267_error(void*context){R267Tower*h=context;return r254_error(h->parent);}
static void r267_error_set(void*context,uint32_t error){R267Tower*h=context;r254_error_set(h->parent,error);}
static int r267_verified(void*context,uintptr_t image){
    R267Tower*h=context;return h&&image==(uintptr_t)h->parent->image&&r267_live(h)&&r254_verified(h->parent);
}
static int r267_region(uintptr_t p,size_t n,MEMORY_BASIC_INFORMATION*out){
    if(!out||!p||!n||p>UINTPTR_MAX-n||!g_win.VirtualQuery||
       !g_win.VirtualQuery((void*)p,out,sizeof(*out))||out->State!=MEM_COMMIT||
       (out->Protect&PAGE_GUARD)||(uintptr_t)out->BaseAddress>p||
       (uintptr_t)out->BaseAddress>UINTPTR_MAX-out->RegionSize)return 0;
    return p+n<=(uintptr_t)out->BaseAddress+out->RegionSize;
}
static int r267_writable(void*context,uintptr_t p,size_t n){
    MEMORY_BASIC_INFORMATION m;(void)context;
    if(!r267_region(p,n,&m))return 0;unsigned mode=m.Protect&0xff;
    return mode==0x04||mode==0x08||mode==0x40||mode==0x80;
}
static int r267_resident(void*context,uintptr_t p){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    if(!h||!h->pinned||!r267_region(p,1,&m)||!r254_executable(h->parent,p))return 0;
    if(p==(uintptr_t)r267_floor_entry)return m.Type==0x1000000u&&m.AllocationBase==g_self;
    return p==h->allocation&&m.Type==0x20000u&&(uintptr_t)m.AllocationBase==h->allocation;
}
static int r267_unlocked(void*context){
    R267Tower*h=context;return r267_live(h)&&!h->guard.held&&!h->parent->install.guard.held&&
        !__atomic_load_n(&h->backend.owner,__ATOMIC_ACQUIRE)&&!h->ui_execution.busy&&!h->display_events.busy&&!h->image_bindings.busy&&!h->image_events.depth&&!h->image_events.drawing&&!h->appearance.busy&&
        !__atomic_load_n(&h->floor.busy,__ATOMIC_ACQUIRE);
}
static int r267_quiet(void*context){
    R267Tower*h=context;return r267_live(h)&&!h->parent->install.guard.held&&
        !__atomic_load_n(&h->parent->dispatch.active,__ATOMIC_ACQUIRE)&&
        !__atomic_load_n(&h->backend.owner,__ATOMIC_ACQUIRE)&&!h->ui_execution.busy&&!h->display_events.busy&&!h->image_bindings.busy&&!h->image_events.depth&&!h->image_events.drawing&&!h->appearance.busy&&
        !__atomic_load_n(&h->floor.busy,__ATOMIC_ACQUIRE)&&lid_momoka_install_held(&h->guard);
}
static void*r267_allocate(void*context,uintptr_t p,size_t n){
    R267Tower*h=context;if(!r267_unlocked(h)||h->allocation)return 0;
    h->allocation=(uintptr_t)r254_near(h->parent,p,n);return (void*)h->allocation;
}
static int r267_private(void*context,uintptr_t p,size_t n){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    return h&&p==h->allocation&&r267_region(p,n,&m)&&m.Type==0x20000u&&
        (uintptr_t)m.AllocationBase==p&&(m.Protect&0xff)==0x04;
}
static int r267_protect(void*context,uintptr_t p,size_t n,unsigned mode,unsigned*old){
    R267Tower*h=context;return r254_protect(h->parent,(void*)p,n,mode,old);
}
static int r267_flush(void*context,uintptr_t p,size_t n){R267Tower*h=context;return r254_flush(h->parent,(void*)p,n);}
static int r267_original(void*context,uintptr_t entry,uintptr_t original,unsigned size){
    R267Tower*h=context;return lid_tc_original(&h->calls,entry,original,size);
}
static int r267_ascii_wide(const char*s,unsigned limit,const wchar_t*w){
    for(unsigned i=0;i<limit;i++){if((unsigned char)s[i]!=(unsigned)w[i])return 0;if(!s[i])return 1;}return 0;
}
/* Cold once. Verify every immutable descriptor row against the compiled stock
   identities; plans remain abstract in the wire, technical carriers local. */
static int r267_catalogue(const LidShopRuntimeCatalogue*c){
    uint8_t seen[184]={0};unsigned expected=0;
    if(!c||c->tower_count>184)return 0;
    for(unsigned i=0;i<184;i++)if(lid_tower_stock_data[i].floor<=c->goal)expected++;
    if(c->tower_count!=expected)return 0;
    for(unsigned i=0;i<c->tower_count;i++){
        const LidShopRuntimeTower*s=&c->tower[i];if(s->offer>=184||seen[s->offer])return 0;seen[s->offer]=1;
        const LidTowerStockData*d=&lid_tower_stock_data[s->offer];
        if(s->location!=d->location||s->floor!=d->floor||s->floor>c->goal||s->shopid!=d->shopid||
           s->plan_slot!=d->plan_slot||s->kind!=(d->plan_slot?2u:1u)||!lid_ts_room(s->room_key,d->room))return 0;
        if(d->plan_slot){for(unsigned j=0;j<32;j++)if(s->product[j])return 0;for(unsigned j=0;j<96;j++)if(s->native_item[j])return 0;}
        else if(!r267_ascii_wide(s->product,32,d->product)||!r267_ascii_wide(s->native_item,96,d->item))return 0;
    }return 1;
}
#include "shop_tower_policy_calls.h"
static void r267_fail(R267Tower*h,const char*message){
    if(h->phase==R267_FAILED)return;__atomic_store_n(&g_r296_policy.active,0,__ATOMIC_RELEASE);
    __atomic_store_n(&h->floor.active,0,__ATOMIC_RELEASE);
    __atomic_store_n(&h->display_events.active,0,__ATOMIC_RELEASE);
    __atomic_store_n(&h->image_events.active,0,__ATOMIC_RELEASE);
    h->phase=R267_FAILED;log_ascii(message);
}
static int r267_pin(R267Tower*h){
    typedef BOOL(WINAPI*PinModule)(DWORD,LPCWSTR,HMODULE*);
    void*k=lid_find_module_ascii("kernel32.dll");HMODULE module=0;
    PinModule pin=k?(PinModule)lid_get_export(k,"GetModuleHandleExW"):0;
    /* FROM_ADDRESS(4)|PIN(1): actual OS pin, never a synthetic residency bool. */
    if(!g_self||!pin||!pin(5,(LPCWSTR)(uintptr_t)r267_floor_entry,&module)||module!=g_self)return 0;
    h->pinned=1;return 1;
}
/* C-only pinned identity, including on the later transport consumer. Unlike
   r267_live this does NOT traverse the game or require the UI owner thread. */
static int r270_queue_session(void*context){
    R267Tower*h=context;const LidShopRuntimeCatalogue*c=&g_checks_delivery.shop_catalogue.catalogue;
    return h&&h->parent&&h->pinned&&__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)&&
        r15_same_context(&h->context,&g_checks_delivery.queue.context)&&
        r15_same_context(&h->context,&c->context)&&h->goal==c->goal&&
        r15_equal(h->full,c->full,32)&&r15_equal(h->tower,c->tower_hash,32);
}
static int r270_before_charge(void*context,unsigned offer){
    R267Tower*h=context;
    if(offer>=184){r299_note(14,1,offer);return 0;}
    if(!r267_ready(h)){
        unsigned detail=(h->image_bindings.stopped?1u:0u)|(h->text_view.stopped?2u:0u)|
            (h->display_events.stopped?4u:0u)|(h->image_events.stopped?8u:0u)|
            (h->backend.stopped?16u:0u)|(h->backend.sale.stopped?32u:0u)|
            (h->ui_execution.stopped?64u:0u)|(__atomic_load_n(&h->ui_queue.halted,__ATOMIC_ACQUIRE)?128u:0u);
        r299_note(14,2,detail);return 0;
    }
    if(!r267_wednesday(h)){r299_note(14,3,0);return 0;}
    if(h->calls.state!=LID_TC_INSTALLED){r299_note(14,4,h->calls.state);return 0;}
    if(!__atomic_load_n(&h->floor.active,__ATOMIC_ACQUIRE)){r299_note(14,5,0);return 0;}
    if(lid_tower_stock_data[offer].floor>h->goal){r299_note(14,6,offer);return 0;}
    int ready=lid_tde_offer_ready(&h->display_events,offer);r299_note(14,ready?8:7,offer);return ready;
}
static int r270_ui_resident(void*context,uintptr_t p){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    return h&&h->pinned&&(p==h->ui_execution.wrappers[0]||p==h->ui_execution.wrappers[1])&&
        r267_region(p,1,&m)&&m.Type==0x1000000u&&m.AllocationBase==g_self&&r254_executable(h->parent,p);
}
static int r270_ui_protect(void*context,uintptr_t p,unsigned mode,unsigned*old){return r267_protect(context,p,8,mode,old);}
static __attribute__((noinline)) LidTSString*r273_item_entry(LidTSString*out,int uid,const wchar_t*item){
    /* Capture actual native CALL return here, never in a C forwarding helper. */
    return lid_tni_call(&g_shop_tower.backend.no_item,out,uid,item,(uintptr_t)__builtin_return_address(0));
}
static int r273_owned_call(void*context,uintptr_t caller){
    R267Tower*h=context;return h&&h->backend.prepared&&lid_tic_owned(&h->item_call,caller);
}
static int r274_item_ready(void*context){
    R267Tower*h=context;
    return r267_ready(h)&&h->ui_execution.published&&
        __atomic_load_n(&g_lid_tower_handler_route,__ATOMIC_ACQUIRE)==&h->backend.route&&
        __atomic_load_n(&h->backend.route.active,__ATOMIC_ACQUIRE);
}
static int r274_item_resident(void*context,uintptr_t p){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    if(!h||!h->pinned||!r267_region(p,1,&m)||!r254_executable(h->parent,p))return 0;
    if(p==(uintptr_t)r273_item_entry)return m.Type==0x1000000u&&m.AllocationBase==g_self;
    return p==h->item_allocation&&m.Type==0x20000u&&(uintptr_t)m.AllocationBase==h->item_allocation;
}
static void*r274_item_allocate(void*context,uintptr_t p,size_t n){
    R267Tower*h=context;if(!r267_unlocked(h)||h->item_allocation)return 0;
    h->item_allocation=(uintptr_t)r254_near(h->parent,p,n);return (void*)h->item_allocation;
}
static int r274_item_private(void*context,uintptr_t p,size_t n){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    return h&&p==h->item_allocation&&r267_region(p,n,&m)&&m.Type==0x20000u&&
        (uintptr_t)m.AllocationBase==p&&(m.Protect&0xff)==0x04;
}
static int r274_item_prepare(R267Tower*h){
    LidTCOps ops={h,r267_quiet,r267_unlocked,r274_item_ready,r274_item_resident,r274_item_allocate,r274_item_private,r267_protect,r267_flush};
    int ok=lid_tic_prepare(&h->item_call,&h->floor,(uintptr_t)r273_item_entry,&ops);
    h->item_relay=h->item_call.relay;return ok;
}
static int r273_backend_prepare(R267Tower*h,const LidShopRuntimeCatalogue*c){
    LidTowerBackendOps ops={h,r267_thread,r267_error,r267_error_set,r270_queue_session,r267_quiet,r267_ready,r273_owned_call};
    return c->ui==h->parent->owner&&lid_tb_prepare(&h->backend,&h->ui_queue,&h->floor,&h->payment_native,
        &h->ui_meta.memory,h->parent->uid,r254_verified(h->parent),&ops);
}
static int r270_ui_prepare(R267Tower*h,const LidShopRuntimeCatalogue*c){
    LidTowerQueueOps queue={h,r267_thread,r270_queue_session,r270_before_charge};
    LidTowerUiExecOps execution={h,r267_thread,r267_error,r267_error_set,r267_quiet,r270_ui_resident,r270_ui_protect,r267_ready};
    return lid_tq_prepare(&h->ui_queue,c,&queue)&&
        lid_tue_prepare(&h->ui_execution,&h->ui_current,&h->ui_queue,&execution)&&
        lid_tp_native_prepare(&h->payment_native,&h->ui_meta.memory,(uintptr_t)h->parent->image,r254_verified(h->parent))&&
        r273_backend_prepare(h,c)&&r274_item_prepare(h)&&
        lid_trq_prepare(&h->receipts,&h->backend.payment)&&r276_tower_bind(&g_checks_delivery,&h->receipts);
}
static int r267_prepare(R267Tower*h,R254Momoka*r,const LidShopRuntimeCatalogue*c){
    if(h->phase!=R267_COLD||!r267_catalogue(c)||!r254_verified(r))return 0;
    h->parent=r;h->context=c->context;h->goal=c->goal;r15_copy(h->full,c->full,32);r15_copy(h->tower,c->tower_hash,32);
    if(!r267_pin(h))return 0;
    LidMomokaInstallOps locks={r,r254_copy,r254_try,r254_leave,r254_thread};
    LidTFOps native={h,r267_verified,r267_read,r267_writable,r267_original,r267_thread,r267_error,r267_error_set,r267_admit,r317_floor_producer};
    LidTCOps calls={h,r267_quiet,r267_unlocked,r267_ready,r267_resident,r267_allocate,r267_private,r267_protect,r267_flush};
    return lid_momoka_install_prepare(&h->guard,(uintptr_t)r->image,r254_verified(r),r->owner,&locks)&&
        lid_tf_prepare(&h->floor,(uintptr_t)r->image,c->goal,r->owner,&native)&&
        lid_tc_prepare(&h->calls,&h->floor,(uintptr_t)r267_floor_entry,&calls)&&r270_ui_prepare(h,c);
}
/* Real future composition attaches once; no fake capability is supplied by this
   host. The existing authenticated Context/tower digest must match exactly. */
static int r267_attach(const R267Coherent*capability){
    R267Tower*h=&g_shop_tower;
    if(!capability||!capability->ready||h->coherent||h->phase!=R267_PREPARED||!r267_live(h)||
       capability->owner!=h->parent->owner||capability->goal!=h->goal||
       !r15_same_context(&capability->session,&h->context)||!r15_equal(capability->catalogue,h->tower,32))return 0;
    h->coherent=capability;h->publication_began=h->parent->clock();return 1;
}
static int r267_publish(R267Tower*h){
    uint32_t error=r267_error(h);
    if(!r267_ready(h)){r267_error_set(h,error);return LID_MI_BUSY;}
    int held=lid_momoka_install_begin(&h->guard);if(held!=LID_MI_HELD){r267_error_set(h,error);return held;}
    int ok=0,released=0;
    __try{
        /* All pieces prepared BEFORE guard. Inactive UI -> actual existing
           handler provider -> owned item CALL -> floor CALLs -> activation. */
        ok=lid_tir_publish(&h->image_events)&&lid_tde_publish(&h->display_events)&&lid_tue_publish(&h->ui_execution)&&lid_thr_publish(&h->backend.route)&&
            lid_tic_publish(&h->item_call)&&lid_tc_publish(&h->calls)&&
            lid_tir_enable(&h->image_events)&&lid_tde_enable(&h->display_events)&&lid_tue_enable(&h->ui_execution)&&lid_tc_enable(&h->calls);
        if(!ok){
            __atomic_store_n(&h->ui_execution.active,0,__ATOMIC_RELEASE);
            __atomic_store_n(&h->floor.active,0,__ATOMIC_RELEASE);
            __atomic_store_n(&h->display_events.active,0,__ATOMIC_RELEASE);
            __atomic_store_n(&h->image_events.active,0,__ATOMIC_RELEASE);
            (void)lid_tde_revert(&h->display_events);
            (void)lid_tir_revert(&h->image_events);
            int item_restored=lid_tic_revert(&h->item_call);
            (void)lid_tc_revert(&h->calls);
            /* If our item CALL remains, retain the real request provider for
               fail-closed handling. Never free a possibly published host. */
            if(item_restored)__atomic_store_n(&h->backend.route.active,0,__ATOMIC_RELEASE);
            h->backend.stopped=1;lid_tq_halt(&h->ui_queue);
        }
    }__finally{
        released=lid_momoka_install_end(&h->guard);
        if(!ok||!released){
            __atomic_store_n(&h->ui_execution.active,0,__ATOMIC_RELEASE);
            __atomic_store_n(&h->floor.active,0,__ATOMIC_RELEASE);
            __atomic_store_n(&h->display_events.active,0,__ATOMIC_RELEASE);
    __atomic_store_n(&h->image_events.active,0,__ATOMIC_RELEASE);
            h->backend.stopped=1;lid_tq_halt(&h->ui_queue);
        }
        r267_error_set(h,error);
    }
    return ok&&released?LID_MI_HELD:LID_MI_INVALID;
}
static int r278_image_live(void*context){return r267_live((R267Tower*)context);}
/* Existing UI owner, cold once after immutable scout data. No Momoka menu or
   explain root required. Never allocate under either install/payment guard.
   Partial own native objects stay rooted/resident; failure is terminal. */
static void r278_visual_tick(R267Tower*h){
    if(!h||h->visual_state||h->phase!=R267_PREPARED)return;
    uint32_t error=r267_error(h);const LidShopDisplay*d=&g_checks_delivery.shop_display;
    int state=__atomic_load_n(&d->state,__ATOMIC_ACQUIRE);
    if(state==LID_SD_WAIT){r267_error_set(h,error);return;} /* C-only waiting */
    if(state!=LID_SD_READY){h->visual_state=-1;r267_error_set(h,error);return;}
    if(!r267_unlocked(h)){r267_error_set(h,error);return;}
    h->visual_state=-1;volatile unsigned completed=0;
    __try{
        LidTowerAPImageOps ops={h,r267_thread,r278_image_live,r267_unlocked,&h->ui_meta.memory,h->parent->owner};
        if(lid_tai_prepare(&h->ap_image,&ops,(uintptr_t)h->parent->image,r254_verified(h->parent))&&
           lid_tai_create(&h->ap_image)&&
           lid_tip_prepare(&h->icons,&h->ui_current,d,&h->ap_image,(uintptr_t)h->parent->image,r254_verified(h->parent)))
            h->visual_state=1;
        completed=1;
    }__finally{
        if(!completed){h->visual_state=-1;if(h->ap_image.phase==2)h->ap_image.phase=-1;}
        r267_error_set(h,error);
    }
    log_ascii(h->visual_state==1?"AP SHOPSANITY R278: independent tower AP Stream and native icon provider prepared; render consumer pending.":
        "AP SHOPSANITY R278: tower image preparation refused; no retry or partial rendering enabled.");
    r267_error_set(h,error);
}
/* Metadata only: does NOT invoke private view construction/text writes. Actual
   apply awaits the complete native event owner and rendering composition. */
static unsigned g_r286_tower_stage=~0u;
static void r279_text_tick(R267Tower*h){
    if(!h||h->text_state||h->phase!=R267_PREPARED||h->visual_state!=1)return;
    uint32_t saved=r267_error(h);uint64_t now=h->parent->clock();
    if(!h->text_began)h->text_began=now;
    if(now<h->text_began||now-h->text_began>=120000){h->text_state=-1;r267_error_set(h,saved);return;}
    int result=lid_tav_prepare(&h->text_view,&h->ui_current,&h->ui_queue,&g_checks_delivery.shop_display,
        (uintptr_t)h->parent->image,r254_verified(h->parent));
    if(result)h->text_state=result>0?1:-1;
    if(g_r286_tower_stage!=g_lid_tav_prepare_stage){
        r286_schema_log("AP SHOPSANITY R286 TOWER",g_lid_tav_prepare_stage,0,result==0);
        g_r286_tower_stage=g_lid_tav_prepare_stage;
    }
    if(result)log_ascii(result>0?"AP SHOPSANITY R279: private native text-view metadata prepared; UI event consumer pending.":
        "AP SHOPSANITY R279: private text-view schema refused; no shared cache changed.");
    r267_error_set(h,saved);
}
static int r280_display_resident(void*context,uintptr_t p){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    return h&&h->pinned&&(p==(uintptr_t)lid_tde_info||p==(uintptr_t)lid_tde_disable)&&
        r267_region(p,1,&m)&&m.Type==0x1000000u&&m.AllocationBase==g_self&&r254_executable(h->parent,p);
}
static int r280_display_protect(void*context,uintptr_t p,unsigned mode,unsigned*old){return r267_protect(context,p,8,mode,old);}
static void r280_display_tick(R267Tower*h){
    if(!h||h->display_event_state||h->phase!=R267_PREPARED||h->text_state!=1)return;
    uint32_t saved=r267_error(h);h->display_event_state=-1;
    LidTowerDisplayOps ops={h,r267_thread,r267_error,r267_error_set,r267_quiet,r267_ready,r280_display_resident,r280_display_protect};
    if(lid_tde_prepare(&h->display_events,&h->text_view,&ops))h->display_event_state=1;
    log_ascii(h->display_event_state==1?"AP SHOPSANITY R280: native tower text/availability event owner prepared; full coherent rendering awaited.":
        "AP SHOPSANITY R280: native display slot admission refused; no display slot published.");
    r267_error_set(h,saved);
}
static int r282_image_resident(void*context,uintptr_t p){
    R267Tower*h=context;MEMORY_BASIC_INFORMATION m;
    return h&&h->pinned&&(p==(uintptr_t)lid_tir_grid||p==(uintptr_t)lid_tir_panel||p==(uintptr_t)lid_tir_grid_pos||p==(uintptr_t)lid_tir_mini_pos||p==(uintptr_t)lid_tir_mini)&&
        r267_region(p,1,&m)&&m.Type==0x1000000u&&m.AllocationBase==g_self&&r254_executable(h->parent,p);
}
static void r282_image_tick(R267Tower*h){
    if(!h||h->image_event_state||h->phase!=R267_PREPARED||h->display_event_state!=1)return;
    uint32_t saved=r267_error(h),offset;uint64_t ui,cls,hud;
    const M195Ops*o=&h->ui_meta.memory;
    if(!lid_tip_ui(&h->ui_current,&ui,&cls)){r267_error_set(h,saved);return;}
    if(!lid_ml_field(o,ui,"BrgUIManagerBase","mHUDBase","ObjectProperty",8,&offset)||offset!=0xa4||
       !m195_read(o,ui,offset,&hud,8)){h->image_event_state=-1;r267_error_set(h,saved);return;}
    if(!hud){r267_error_set(h,saved);return;}
    h->image_event_state=-1;
    LidTowerDisplayOps ops={h,r267_thread,r267_error,r267_error_set,r267_quiet,r267_ready,r282_image_resident,r280_display_protect};
    if(lid_tib_prepare(&h->image_bindings,&h->text_view,&h->icons,hud)&&lid_tir_prepare(&h->image_events,&h->image_bindings,&ops)){
        h->display_events.images=&h->image_bindings;h->image_event_state=1;
    }
    log_ascii(h->image_event_state==1?"AP SHOPSANITY R282: scoped native grid/panel image owner and private reflected retention prepared; coherent activation awaited.":
        "AP SHOPSANITY R282: image owner schema/profile refused; no partial image publication.");
    r267_error_set(h,saved);
}
/* No repository calls after success/failure; waiting is bounded and at most
   once per second, outside both install/payment guards on the actual UI owner. */
/* Transition-only, existing 1Hz cold path; unsigned bit patterns are explicit. */
static LidTowerAppearanceProbe g_r292_last_probe;
static unsigned g_r292_probe_logged;
static void r292_appearance_log(void){
    const LidTowerAppearanceProbe*s=&g_lid_tap_probe;LidTowerAppearanceProbe*p=&g_r292_last_probe;
    if(g_r292_probe_logged&&s->stage==p->stage&&s->room==p->room&&
       s->timer_present==p->timer_present&&s->minutes==p->minutes&&s->rate==p->rate)return;
    *p=*s;g_r292_probe_logged=1;
    r286_schema_log("AP SHOPSANITY R292 POLICY stage/stock_index/timer_present",s->stage,s->room,s->timer_present);
    r286_schema_log("AP SHOPSANITY R292 VALUES minutes_low/minutes_high/rate_bits",
        (unsigned)(uint64_t)s->minutes,(unsigned)((uint64_t)s->minutes>>32),(unsigned)s->rate);
}
static void r283_appearance_tick(R267Tower*h){
    if(!h||h->phase!=R267_PREPARED||h->image_event_state!=1||h->appearance.phase<0||(h->appearance.phase==2&&r296_owned(h)))return;
    if(!r267_unlocked(h))return;
    uint32_t saved=r267_error(h);uint64_t now=h->parent->clock();
    if(!h->appearance_began)h->appearance_began=now;
    if(now<h->appearance_began||now-h->appearance_began>=120000){
        h->appearance.phase=-1;log_ascii("AP SHOPSANITY R283: loaded master policy deadline exceeded; tower activation withheld.");r267_error_set(h,saved);return;
    }
    if(now<h->appearance_next){r267_error_set(h,saved);return;}
    h->appearance_next=now<=UINT64_MAX-1000?now+1000:UINT64_MAX;
    volatile unsigned normal=0;
    __try{
        if(!h->appearance.phase){
            LidTowerAPImageOps ops={h,r267_thread,r278_image_live,r267_unlocked,&h->ui_meta.memory,h->parent->owner};
            if(!lid_tap_prepare(&h->appearance,&ops,&g_checks_delivery.shop_catalogue.catalogue,(uintptr_t)h->parent->image,r254_verified(h->parent))){h->appearance.phase=-1;goto end;}
        }
        /* Retain the original repository observation, separately from policy. */
        (void)lid_tap_check(&h->appearance);r292_appearance_log();
        if(!g_r296_policy.state&&!r296_prepare(h)){
            h->appearance.phase=-1;log_ascii("AP SHOPSANITY R296: merchant policy preparation refused.");goto end;
        }
        if(g_r296_policy.state==1){
            int published=r296_publish(h);if(published==LID_MI_BUSY)goto end;
            if(published!=LID_MI_HELD){h->appearance.phase=-1;log_ascii("AP SHOPSANITY R296: merchant policy publication refused.");goto end;}
        }
        if(!r296_check(h)){
            __atomic_store_n(&g_r296_policy.active,0,__ATOMIC_RELEASE);h->appearance.phase=-1;
            log_ascii("AP SHOPSANITY R296: effective merchant policy ownership/readback refused.");goto end;
        }
        log_ascii("AP SHOPSANITY R296: two merchant CALL owners verified; effective one-minute/Wednesday-100 policy ready, original repository unchanged.");
end:normal=1;
    }__finally{if(!normal)h->appearance.phase=-1;r267_error_set(h,saved);}
}
/* Concrete links and native policy proof, never a constant-ready gate.
   Prepared owners are valid here BEFORE publication; actual installed-slot
   checks remain in each enabler and the UI precharge boundary. C state only. */
static int r284_components(void*context){
    R267Tower*h=context;if(!h||h!=&g_shop_tower||!h->pinned||!r270_queue_session(h))return 0;
    const LidShopRuntimeCatalogue*c=&g_checks_delivery.shop_catalogue.catalogue;
    unsigned expected=h->goal==10?2:h->goal==20?3:h->goal==30?5:h->goal==40?7:0;
    unsigned calls=__atomic_load_n(&h->calls.state,__ATOMIC_ACQUIRE);
    unsigned item=__atomic_load_n(&h->item_call.state,__ATOMIC_ACQUIRE);
    return expected&&r296_owned(h)&&h->appearance.phase==2&&!h->appearance.busy&&h->appearance.catalogue==c&&h->appearance.goal==h->goal&&h->appearance.rooms==expected&&
        h->floor.prepared&&h->calls.floor==&h->floor&&calls>=LID_TC_PREPARED&&calls<=LID_TC_INSTALLED&&
        h->item_call.floor==&h->floor&&item>=LID_TIC_PREPARED&&item<=LID_TIC_INSTALLED&&
        h->ui_meta.ready&&h->ui_current.prepared&&h->ui_current.meta==&h->ui_meta&&h->ui_current.catalogue==c&&
        h->ui_queue.initialized&&h->ui_queue.owner==h->parent->owner&&h->ui_queue.worker==c->worker&&
        r15_same_context(&h->ui_queue.context,&h->context)&&r15_equal(h->ui_queue.catalogue,h->tower,32)&&
        h->ui_execution.prepared&&h->ui_execution.current==&h->ui_current&&h->ui_execution.queue==&h->ui_queue&&
        h->backend.prepared&&h->backend.queue==&h->ui_queue&&h->backend.payment.prepared&&h->backend.payment.queue==&h->ui_queue&&
        h->backend.sale.prepared&&h->backend.sale.floor==&h->floor&&h->backend.route.prepared&&
        h->backend.no_item.prepared&&h->backend.no_item.payment==&h->backend.payment&&
        h->receipts.host==&h->backend.payment&&h->receipts.ui==c->ui&&h->receipts.worker==c->worker&&
        r15_same_context(&h->receipts.context,&h->context)&&r15_equal(h->receipts.catalogue,h->tower,32)&&r276_tower_ready(&g_checks_delivery)&&
        h->text_view.prepared&&h->text_view.current==&h->ui_current&&h->text_view.queue==&h->ui_queue&&
        h->text_view.display==&g_checks_delivery.shop_display&&h->icons.ready&&h->icons.current==&h->ui_current&&h->icons.ap==&h->ap_image&&
        h->image_bindings.prepared&&h->image_bindings.text==&h->text_view&&h->image_bindings.provider==&h->icons&&h->image_bindings.draw.ap==&h->ap_image&&
        h->image_events.prepared&&h->image_events.images==&h->image_bindings&&
        h->display_events.prepared&&h->display_events.text==&h->text_view&&h->display_events.images==&h->image_bindings;
}
static void r284_bind(R267Tower*h){
    if(!h||h->phase!=R267_PREPARED||h->coherent||h->capability_failed||h->appearance.phase!=2)return;
    if(!r267_live(h)||!r284_components(h))return;
    uint32_t saved=r267_error(h);MEMORY_BASIC_INFORMATION code,data;
    if(!r267_region((uintptr_t)r284_components,1,&code)||code.Type!=0x1000000u||code.AllocationBase!=g_self||
       !r254_executable(h->parent,(uintptr_t)r284_components)||!r267_region((uintptr_t)&h->capability,sizeof(h->capability),&data)||
       data.Type!=0x1000000u||data.AllocationBase!=g_self){h->capability_failed=1;r267_error_set(h,saved);return;}
    h->capability.context=h;h->capability.ready=r284_components;h->capability.owner=h->parent->owner;h->capability.goal=h->goal;
    h->capability.session=h->context;r15_copy(h->capability.catalogue,h->tower,32);
    if(!r267_attach(&h->capability))h->capability_failed=1;
    else log_ascii("AP SHOPSANITY R284: actual tower UI, payment, receipt, render and loaded master policy composed; guarded publication pending.");
    r267_error_set(h,saved);
}
/* Owner-only publication of unique scalar event observations; no repeated rows. */
static uint64_t g_r299_next;
static void r299_flush(R254Momoka*r){
    uint64_t now=r->clock();if(now<g_r299_next)return;g_r299_next=now+1000;
    uint32_t saved=r254_error(r);
    for(unsigned c=0;c<LID_TOWER_PROBE_CHANNELS;c++)for(unsigned stage=0;stage<64;stage++)
        if(__atomic_load_n(&g_r299_events[c][stage],__ATOMIC_ACQUIRE)==2){
            unsigned detail=g_r299_details[c][stage];
            r286_schema_log("AP SHOPSANITY R299 EVENT channel/code/detail",c,stage,detail);
            __atomic_store_n(&g_r299_events[c][stage],3,__ATOMIC_RELEASE);
        }
    r254_error_set(r,saved);
}
static void r267_tick(R254Momoka*r){
    R267Tower*h=&g_shop_tower;
    if(h->parent&&r254_thread(r)==r->owner)r299_flush(r);
    if(h->phase==R267_FAILED||h->phase==R267_PUBLISHED)return;
    if(h->phase==R267_COLD){
        const LidShopRuntimeCatalogue*c=r267_join(r);if(!c)return;
        if(!h->ui_meta.ready){
            uint32_t meta_error=r254_error(r);uint64_t now=r->clock();if(!h->metadata_began)h->metadata_began=now;
            h->memory=(LidTowerMemory){&r->driver.adapter,(uintptr_t)r->image};
            M195Ops memory={&h->memory,lid_tm_copy,lid_tm_name};
            int meta=lid_tum_prepare(&h->ui_meta,&memory,(uintptr_t)r->image);
            if(meta<0||now<h->metadata_began||now-h->metadata_began>=120000){
                r267_fail(h,"AP SHOPSANITY R268: tower UI metadata refused; no UI slot or floor callsite published.");r254_error_set(r,meta_error);return;
            }
            r254_error_set(r,meta_error);
            if(!meta)return;
        }
        if(!h->ui_current.prepared){
            uint32_t saved=r254_error(r);uint64_t now=r->clock();
            LidTowerUiCurrentOps ops={r->root.context,r->root.current_game,h,r269_ui_live};
            int current=lid_tuc_prepare(&h->ui_current,&h->ui_meta,c,&ops);
            if(current<0||now<h->metadata_began||now-h->metadata_began>=120000){
                r267_fail(h,"AP SHOPSANITY R269: tower current-root schema refused; no UI slot or floor callsite published.");
                r254_error_set(r,saved);return;
            }
            r254_error_set(r,saved);if(!current)return;
        }
        uint32_t error=r254_error(r);int prepared=0;unsigned normal=0;
        __try{prepared=r267_prepare(h,r,c);normal=1;}
        __finally{
            if(!normal){__atomic_store_n(&h->floor.active,0,__ATOMIC_RELEASE);h->phase=R267_FAILED;}
            r254_error_set(r,error);
        }
        if(!prepared){r267_fail(h,"AP SHOPSANITY R267: tower cold preparation refused; no callsite published.");r254_error_set(r,error);return;}
        h->phase=R267_PREPARED;
        log_ascii("AP SHOPSANITY R267: tower owner prepared independently; coherent purchase capability awaited.");
        r254_error_set(r,error);
    }
    r278_visual_tick(h);
    r279_text_tick(h);
    r280_display_tick(h);
    r282_image_tick(h);
    r283_appearance_tick(h);
    r284_bind(h);
    /* Without the fully verified resident composition, never publish any
       tower UI/code slot. Terminal cold states perform no repeated native scan. */
    if(!h->coherent)return;
    if(!r267_live(h)){r267_fail(h,"AP SHOPSANITY R267: tower session retired; normalization stopped.");return;}
    uint64_t now=r->clock();
    if(now<h->publication_began||now-h->publication_began>=120000){r267_fail(h,"AP SHOPSANITY R267: tower publication deadline exceeded.");return;}
    int published=r267_publish(h);if(published==LID_MI_BUSY)return;
    if(published!=LID_MI_HELD){r267_fail(h,"AP SHOPSANITY R267: tower publication refused; owned state retained.");return;}
    h->phase=R267_PUBLISHED;log_ascii("AP SHOPSANITY R267: coherent tower floor owner published.");
}
#endif
