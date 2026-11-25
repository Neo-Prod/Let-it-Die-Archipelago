#ifndef LID_SHOP_MOMOKA_RUNTIME_H
#define LID_SHOP_MOMOKA_RUNTIME_H
#include "shop_purchase_feature.h"
#if LID_SHOP_PURCHASE_ENABLED
/* Included by mod.c after the real owner/checks/material definitions. One
   resident process/run binding; no boot/connection or ReceivedItems changes.
   Only the existing profile watcher calls this UI-owner cold initialization.
   No UI file IO/hash, background UObject traversal or extra polling thread. */
#include "shop_momoka_gate.h"
#include "shop_momoka_game_root.h"
#include "shop_momoka_label_event.h"
#include "shop_momoka_stock_event.h"
#include "shop_momoka_icons.h"
enum {R254_COLD,R254_GUARDED,R254_BOUGHT,R254_PREPARED,R254_ACTIVE,R254_DISABLED,R254_FAILED};
enum {R254_MEM_FREE=0x10000u,R254_READWRITE=0x04u}; /* Windows constants missing from winlite */
typedef struct R254Momoka {
    unsigned phase,pinned;uint32_t owner;BYTE*image;int uid;char slot[21];
    uint64_t next,waiting_since;R15Context context;
    M214Driver driver;LidShopMomokaGate gate;LidShopMomokaGame game;LidShopMomokaRoot root;
    LidMPPriceCollector price;LidShopPurchaseUi ui;LidShopPurchaseExec execution;
    LidMomokaHooks hooks;LidMomokaDispatch dispatch;LidMomokaInstallBinding install;LidShopMarkerNative json;
    int(WINAPI*try_enter)(void*);void(WINAPI*leave)(void*);
    DWORD(WINAPI*tls_alloc)(void);void*(WINAPI*tls_get)(DWORD);BOOL(WINAPI*tls_set)(DWORD,void*);
    void*language;uint64_t(WINAPI*clock)(void);
    LidMomokaLabelEvent labels;int labels_state; /* append: existing private offsets unchanged */
    LidMomokaStockEvent stock;int stock_state;
    LidAPImage ap_image;LidMomokaIcons icons;int icons_state;unsigned icons_busy;
} R254Momoka;
static R254Momoka g_shop_momoka;
/* Bounded scalar diagnostics on the existing UI-owner initialization path.
   No extra watcher or game read. Distinct stage/value pairs only, max64 lines. */
static unsigned g_r341_seen[16][8],g_r341_lines;
static void r341_momoka_note(unsigned stage,unsigned value){
    if(stage>=16||value>=256||g_r341_lines>=64)return;
    unsigned bit=1u<<(value&31),word=value>>5;
    if(g_r341_seen[stage][word]&bit)return;g_r341_seen[stage][word]|=bit;g_r341_lines++;
    char line[128];size_t pos=0;append_ascii(line,sizeof(line),&pos,"AP SHOP R341 MOMOKA stage=");
    append_dec(line,sizeof(line),&pos,stage);append_ascii(line,sizeof(line),&pos," value=");
    append_dec(line,sizeof(line),&pos,value);line[pos]=0;log_ascii(line);
}
static uint32_t r254_thread(void*c){R254Momoka*r=c;return r->driver.adapter.api.thread?r->driver.adapter.api.thread():0;}
/* This C-only Context remains pinned while old receipts/lease exist, including
   admission halt/draining. Never read game globals/UObjects off the UI thread. */
static int r254_pinned(void*c){
    R254Momoka*r=c;return r&&__atomic_load_n(&r->pinned,__ATOMIC_ACQUIRE)&&
        r15_same_context(&r->context,&g_checks_delivery.queue.context);
}
static int r254_session(void*c){
    R254Momoka*r=c;int uid;
    if(!r||!r->image||!r->owner||r254_thread(r)!=r->owner||
       !g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||
       !g_get_tid||g_get_tid()!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return 0;
    for(unsigned i=0;i<21;i++)if(r->slot[i]!=g_release_slot_key[i])return 0;
    return m213_copy(&r->driver.adapter,(uintptr_t)r->image+0xf8b5090,&uid,4)&&uid==r->uid;
}
static int r254_copy(void*c,uintptr_t p,void*out,size_t n){R254Momoka*r=c;return m213_copy(&r->driver.adapter,p,out,n);}
static int r254_json_copy(void*c,const void*p,void*out,size_t n){return r254_pinned(c)&&r254_copy(c,(uintptr_t)p,out,n);}
static int r254_json_window(void*c,const void*root,int write){return !root&&!write&&r254_session(c)&&r254_pinned(c);}
static int r254_try(void*c,uintptr_t p){R254Momoka*r=c;return r->try_enter&&r->try_enter((void*)p)!=0;}
static void r254_leave(void*c,uintptr_t p){R254Momoka*r=c;r->leave((void*)p);}
static int r254_protect(void*c,void*p,size_t n,unsigned mode,unsigned*old){
    DWORD previous=0;(void)c;if(!old||!g_win.VirtualProtect)return 0;
    int ok=g_win.VirtualProtect(p,n,mode,&previous)!=0;if(ok)*old=(unsigned)previous;return ok;
}
static void r254_yield(uint32_t delay){g_win.Sleep((DWORD)delay);}
static int r254_flush(void*c,void*p,size_t n){(void)c;return g_win.FlushInstructionCache&&g_win.GetCurrentProcess&&g_win.FlushInstructionCache(g_win.GetCurrentProcess(),p,n)!=0;}
static int r254_executable(void*c,uintptr_t p){
    MEMORY_BASIC_INFORMATION m;(void)c;
    if(!p||!g_win.VirtualQuery||!g_win.VirtualQuery((void*)p,&m,sizeof(m))||
       m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD))return 0;
    unsigned a=m.Protect&0xff;return a==0x10||a==0x20||a==0x40||a==0x80;
}
static int r254_region(void*c,uintptr_t p,R32Region*out){
    MEMORY_BASIC_INFORMATION m;(void)c;
    if(!out||!g_win.VirtualQuery||!g_win.VirtualQuery((void*)p,&m,sizeof(m)))return 0;
    *out=(R32Region){(uintptr_t)m.BaseAddress,m.RegionSize,m.State==R254_MEM_FREE};return 1;
}
static void*r254_at(void*c,uintptr_t p,size_t n){
    (void)c;return g_win.VirtualAlloc?g_win.VirtualAlloc((void*)p,n,MEM_RESERVE|MEM_COMMIT,R254_READWRITE):0;
}
static void*r254_near(void*c,uintptr_t p,size_t n){
    R32NearOps o={c,r254_region,r254_at};return n&&n<=4096?r32_allocate_near(p,&o):0;
}
static int r254_quiet(void*c){R254Momoka*r=c;return lid_momoka_install_held(&r->install.guard);}
static void*r254_tls_get(void*c,unsigned index){R254Momoka*r=c;return r->tls_get(index);}
static int r254_tls_set(void*c,unsigned index,void*p){R254Momoka*r=c;return r->tls_set(index,p)!=0;}
static unsigned r254_error(void*c){R254Momoka*r=c;return r->driver.adapter.api.get_error();}
static void r254_error_set(void*c,unsigned error){R254Momoka*r=c;r->driver.adapter.api.set_error(error);}
static void r254_getter(void*receiver,void*frame,void*result){lid_sg_exec(&g_shop_momoka.gate,receiver,frame,result);}
static void r254_fail(R254Momoka*r,const char*message){
    if(r->phase==R254_FAILED)return;
    r->phase=R254_FAILED;
    if(__atomic_load_n(&g_checks_delivery.shop_bridge_published,__ATOMIC_ACQUIRE))
        __atomic_store_n(&g_checks_delivery.shop_bridge.halted,1,__ATOMIC_RELEASE);
    log_ascii(message); /* once per terminal failure; never a per-frame log */
}
static int r254_api(R254Momoka*r){
    void*k=lid_find_module_ascii("kernel32.dll"),*nt=lid_find_module_ascii("ntdll.dll");M213Api*a=&r->driver.adapter.api;
    if(!k||!nt||!g_win.VirtualQuery||!g_win.VirtualAlloc||!g_win.VirtualProtect||
       !g_win.FlushInstructionCache||!g_win.GetCurrentProcess||!g_win.Sleep)return 0;
#define R254_API(field,type,name) field=(type)lid_get_export(k,name)
    R254_API(a->read,__typeof__(a->read),"ReadProcessMemory");
    R254_API(a->process,__typeof__(a->process),"GetCurrentProcess");
    R254_API(a->thread,__typeof__(a->thread),"GetCurrentThreadId");
    R254_API(a->get_error,__typeof__(a->get_error),"GetLastError");
    R254_API(a->set_error,__typeof__(a->set_error),"SetLastError");
    R254_API(r->clock,__typeof__(r->clock),"GetTickCount64");
    R254_API(r->try_enter,__typeof__(r->try_enter),"TryEnterCriticalSection");
    R254_API(r->leave,__typeof__(r->leave),"LeaveCriticalSection");
    R254_API(r->tls_alloc,__typeof__(r->tls_alloc),"TlsAlloc");
    R254_API(r->tls_get,__typeof__(r->tls_get),"TlsGetValue");
    R254_API(r->tls_set,__typeof__(r->tls_set),"TlsSetValue");
#undef R254_API
    r->language=lid_get_export(nt,"__C_specific_handler");
    return a->read&&a->process&&a->thread&&a->get_error&&a->set_error&&r->clock&&
        r->try_enter&&r->leave&&r->tls_alloc&&r->tls_get&&r->tls_set&&r->language;
}
/* Reuse the already completed material measurement, never hash on this tick. */
static int r254_verified(R254Momoka*r){
    char sha[65];return g_material_activation_hash&&
        r105_verified(g_material_activation_hash,r->owner,(uintptr_t)r->image,sha);
}
static void r286_schema_log(const char*,unsigned,unsigned,unsigned);
#include "shop_tower_runtime.h"
/* Cold-only field walk, then class/offset caches; once ACTIVE no lookup on tick.
   R252 validation at actual buys still follows the current active menu. */
static int r254_menu(R254Momoka*r,uint64_t*database,uint64_t*menu){
    LidShopMomokaRoot*c=&r->root;uint64_t game,cls,db,manager,current,dbcls,menucls;
    if(!lid_sg_game_current(&r->game,&game)||!m195_read(&c->memory,game,0x50,&cls,8)){r341_momoka_note(3,0);return 0;}
    if(c->game_class!=cls){
        uint32_t a,b;
        if(!lid_mr_field(&c->memory,game,"BrgGameInfoNative","mDatabase","ObjectProperty",8,&a,0)||
           !lid_mr_field(&c->memory,game,"BrgGameInfoNative","mUIManager","ObjectProperty",8,&b,0)){r341_momoka_note(4,0);return 0;}
        c->database=a;c->manager=b;c->game_class=cls;c->manager_class=0;
    }
    if(!m195_read(&c->memory,game,c->database,&db,8)||!m195_read(&c->memory,db,0x50,&dbcls,8)||
       !m195_named(&c->memory,dbcls,"BrgDatabase")||
       !m195_read(&c->memory,game,c->manager,&manager,8)||!m195_read(&c->memory,manager,0x50,&cls,8)){r341_momoka_note(5,0);return 0;}
    if(c->manager_class!=cls){
        uint32_t offset;
        if(!lid_mr_field(&c->memory,manager,"BrgUIManager","mSkillExchange","ObjectProperty",8,&offset,0)){r341_momoka_note(6,0);return 0;}
        c->menu=offset;c->manager_class=cls;
    }
    if(!m195_read(&c->memory,manager,c->menu,&current,8)||
       !m195_read(&c->memory,current,0x50,&menucls,8)||!m195_named(&c->memory,menucls,"BrgUIMenu_SkillExchange")){r341_momoka_note(7,0);return 0;}
    int ready=lid_mr_current(c,menucls,dbcls,database,menu);r341_momoka_note(8,!!ready);return ready;
}
static void r254_disabled(R254Momoka*r){
    /* Explicit false is terminal for this Context. Restore only our own guard;
       no native purchase hooks have been prepared/published at this point. */
    M207Slot*s=&r->driver.cell.ownership;unsigned old=0,ignored=0;
    if(r->driver.prepared&&__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)==M207_INSTALLED&&
       r254_protect(r,(void*)s->slot,8,R254_READWRITE,&old)){
        (void)m207_restore(s);(void)r254_protect(r,(void*)s->slot,8,old,&ignored);
    }
    r->phase=R254_DISABLED;log_ascii("AP SHOPSANITY R254: disabled policy; native Momoka retained.");
}
/* Called only by the exact owned PUI wrapper on its UI thread after carrier
   resolution. C-state snapshot only: no engine reads, IO, locks or allocation.
   Existing wrapper proves current root/session/thread; never call for ADD. */
static int r261_before_charge(void*c,unsigned offer){
    R254Momoka*r=c;
    return r&&offer<4096&&r->phase==R254_ACTIVE&&
        r->labels_state==1&&r->labels.prepared&&!r->labels.stopped&&!r->labels.busy&&
        r->labels.description.ready&&
        r->stock_state==1&&r->stock.prepared&&!r->stock.stopped&&!r->stock.busy&&
        r->icons_state==1&&r->icons.ready&&!r->icons_busy&&r->ap_image.phase==3&&
        __atomic_load_n(&g_checks_delivery.shop_display.state,__ATOMIC_ACQUIRE)==LID_SD_READY;
}
static int r254_prepare_graph(R254Momoka*r){
    const LidShopRuntimeCatalogue*c=&g_checks_delivery.shop_catalogue.catalogue;
    LidShopPurchaseBridge*b=&g_checks_delivery.shop_bridge;
    LidMomokaInstallOps install={r,r254_copy,r254_try,r254_leave,r254_thread};
    LidMomokaHookOps platform={r,r254_copy,r254_quiet,r254_near,r254_protect,r254_flush,r254_executable},bound;
    LidMomokaTlsOps tls={r,r254_tls_get,r254_tls_set,r254_thread,r254_error,r254_error_set};
    LidShopMarkerAccess raw={r,r254_json_window,r254_json_copy};
    unsigned slot=r->tls_alloc(); /* retained for process lifetime even on failure */
    if(slot==0xffffffffu||!r254_verified(r)||
       !lid_momoka_install_bind(&r->install,(uintptr_t)r->image,1,r->owner,&install,&platform,&r->dispatch.active,&bound)||
       !lid_momoka_hooks_prepare(&r->hooks,(uintptr_t)r->image,1,(uintptr_t)lid_pxe_handler,(uintptr_t)lid_pxe_grant,(uintptr_t)lid_pxe_debit,&bound)||
       !lid_mnd_prepare(&r->dispatch,&r->hooks,slot,&tls)||
       !lid_shop_marker_bind(&r->json,(uintptr_t)r->image,1,&raw)||
       !lid_pui_prepare(&r->ui,&r->price.binding,b,c->momoka,c->momoka_count))return 0;
    r->ui.root=&r->root;
    r->ui.admission_context=r;r->ui.before_charge=r261_before_charge;
    return lid_pxe_prepare(&r->execution,&r->dispatch,&r->ui,&r->price,&r->json,&raw,r254_yield,r->language)&&
        lid_sg_attach(&r->gate,&r->execution);
}
static int r257_labels_live(void*c){
    R254Momoka*r=c;return r->phase==R254_ACTIVE&&r254_session(r)&&r254_pinned(r);
}
static void r257_change_effect(void*receiver,void*frame,void*result){
    lid_mle_call(&g_shop_momoka.labels,receiver,frame,result);
}
/* Cold admission only; terminal success/failure means no further label scans
   on the watcher tick. Selection changes use actual StartChangeEffect events. */
static uint64_t g_r286_labels_next;
static unsigned g_r286_label_stage=~0u,g_r286_label_member=~0u,g_r286_label_wait=~0u;
static void r286_schema_log(const char*prefix,unsigned stage,unsigned member,unsigned waiting){
    char line[192],digits[12];unsigned at=0;
    for(unsigned i=0;prefix[i]&&at<120;i++)line[at++]=prefix[i];
    const char*labels[3]={" stage="," member="," waiting="};unsigned values[3]={stage,member,waiting};
    for(unsigned k=0;k<3;k++){
        for(unsigned i=0;labels[k][i];i++)line[at++]=labels[k][i];
        unsigned n=0,value=values[k];do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);
        while(n)line[at++]=digits[--n];
    }line[at]=0;log_ascii(line);
}
static void r257_labels_tick(R254Momoka*r){
    LidShopDisplay*d=&g_checks_delivery.shop_display;uint64_t database,menu,dbcls,menucls;
    if(r->labels_state)return;
    int state=__atomic_load_n(&d->state,__ATOMIC_ACQUIRE);
    if(state==LID_SD_WAIT)return;
    if(state!=LID_SD_READY){r->labels_state=-1;log_ascii("AP SHOPSANITY R257: display data unavailable; purchase transport retained.");return;}
    uint64_t now=r->clock();if(now<g_r286_labels_next)return;
    g_r286_labels_next=now<=UINT64_MAX-1000?now+1000:UINT64_MAX;
    if(!r254_menu(r,&database,&menu))return; /* waiting for actual open menu */
    uint32_t error=r254_error(r);LidMomokaLabelEvent*h=&r->labels;
    /* Cold preparation is read-only; an ordinary WAIT is not terminal. */
    h->context=r;h->owner=r->owner;h->thread=r254_thread;
    h->error_get=r254_error;h->error_set=r254_error_set;h->live=r257_labels_live;
    g_lid_mle_prepare_status=(LidMomokaPrepareStatus){100,0,0};
    if(!m195_read(&r->root.memory,database,0x50,&dbcls,8)||
       !m195_read(&r->root.memory,menu,0x50,&menucls,8)||
       !r254_verified(r)||!r254_executable(r,(uintptr_t)r->image+0x10483c0)||
       !r254_executable(r,(uintptr_t)r->image+0x19c40)||
       !r254_executable(r,(uintptr_t)r->image+0x140260)||
       !lid_mle_prepare(h,(uintptr_t)r->image,&r->root,d,menucls,dbcls)){
        const LidMomokaPrepareStatus*s=&g_lid_mle_prepare_status;
        if(s->stage!=g_r286_label_stage||s->member!=g_r286_label_member||s->waiting!=g_r286_label_wait){
            r286_schema_log("AP SHOPSANITY R286 MOMOKA",s->stage,s->member,s->waiting);
            g_r286_label_stage=s->stage;g_r286_label_member=s->member;g_r286_label_wait=s->waiting;
        }
        if(!s->waiting){r->labels_state=-1;log_ascii("AP SHOPSANITY R257: typed label admission refused; purchase transport retained.");}
        r254_error_set(r,error);return;
    }
    r->labels_state=-1; /* actual publication failure stays terminal */
    unsigned old=0,ignored=0;
    if(!r254_protect(r,(void*)(uintptr_t)h->slot,8,R254_READWRITE,&old)){
        log_ascii("AP SHOPSANITY R257: label slot protection refused.");r254_error_set(r,error);return;
    }
    int installed=m207_publish(&h->ownership,(uintptr_t*)(uintptr_t)h->slot,h->original,(uintptr_t)r257_change_effect);
    int restored=r254_protect(r,(void*)(uintptr_t)h->slot,8,old,&ignored);
    if(!installed||!restored){
        /* Retain resident original/host even if restore failed after CAS. The
           inactive wrapper passes through; never free/overwrite a later owner. */
        log_ascii("AP SHOPSANITY R257: label publication refused; original forwarding retained.");
        r254_error_set(r,error);return;
    }
    __atomic_store_n(&h->active,1,__ATOMIC_RELEASE);r->labels_state=1;
    /* First selection may already be cached; refresh ONCE. A closed/non-buy
       panel is normal: future exact selection events provide their own scope. */
    uint64_t explain;
    if(lid_ml_current(&h->labels,&explain)){
        volatile unsigned completed=0;h->busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
        __try {
#endif
            h->last_result=lid_mle_refresh(h);completed=1;
            if(h->last_result<0)h->stopped=1;
#if LID_SHOP_EXEC_SEH_ENABLED
        } __finally {
#endif
            if(!completed)h->stopped=1;h->busy=0;r254_error_set(r,error);
#if LID_SHOP_EXEC_SEH_ENABLED
        }
#endif
    }
    log_ascii(h->stopped?"AP SHOPSANITY R257: label refresh refused; native purchase flow retained.":
        "AP SHOPSANITY R257: event-driven Momoka labels bound; no per-frame menu scan.");
    r254_error_set(r,error);
}
static void r258_start_wait(void*receiver,void*frame,void*result){
    lid_mse_call(&g_shop_momoka.stock,receiver,frame,result);
}
static void r258_stock_tick(R254Momoka*r){
    if(r->stock_state||r->labels_state!=1)return;
    LidMomokaStockEvent*h=&r->stock;uint32_t error=r254_error(r);
    r->stock_state=-1; /* a single cold attempt; no repeated menu scans */
    if(!r257_labels_live(r)||!r254_executable(r,(uintptr_t)r->image+0xfeb430)||
       !lid_mse_prepare(h,&r->labels,&g_checks_delivery.shop_bridge,(uintptr_t)r->image)){
        log_ascii("AP SHOPSANITY R258: stock view admission refused; purchase gate retained.");
        r254_error_set(r,error);return;
    }
    unsigned old=0,ignored=0;
    if(!r254_protect(r,(void*)(uintptr_t)h->slot,8,R254_READWRITE,&old)){
        log_ascii("AP SHOPSANITY R258: stock slot protection refused.");r254_error_set(r,error);return;
    }
    int installed=m207_publish(&h->ownership,(uintptr_t*)(uintptr_t)h->slot,h->original,(uintptr_t)r258_start_wait);
    int restored=r254_protect(r,(void*)(uintptr_t)h->slot,8,old,&ignored);
    if(!installed||!restored){
        log_ascii("AP SHOPSANITY R258: stock publication refused; original wait retained.");
        r254_error_set(r,error);return;
    }
    __atomic_store_n(&h->active,1,__ATOMIC_RELEASE);r->stock_state=1;
    volatile unsigned completed=0;h->busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    __try {
#endif
        h->last_result=lid_ms_apply(&h->stock);completed=1;if(h->last_result<0)h->stopped=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    } __finally {
#endif
        if(!completed)h->stopped=1;h->busy=0;r254_error_set(r,error);
#if LID_SHOP_EXEC_SEH_ENABLED
    }
#endif
    log_ascii(h->stopped?"AP SHOPSANITY R258: initial stock view refused; purchase gate retained.":
        "AP SHOPSANITY R258: Momoka stock view bound to native list rebuild events.");
    r254_error_set(r,error);
}
static void r260_icon_explanation(void*c){
    R254Momoka*r=c;if(r->icons_state!=1||r->icons_busy||!r257_labels_live(r))return;
    volatile unsigned completed=0;r->icons_busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    __try {
#endif
        int needed;
        if(!lid_mic_explanation(&r->icons)||(needed=lid_mic_list_needed(&r->icons))<0||
           (needed&&!lid_mic_list(&r->icons)))r->icons_state=-1;completed=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    } __finally {
#endif
        if(!completed)r->icons_state=-1;r->icons_busy=0;
#if LID_SHOP_EXEC_SEH_ENABLED
    }
#endif
}
static void r260_icon_list(void*c){
    R254Momoka*r=c;if(r->icons_state!=1||r->icons_busy||!r257_labels_live(r))return;
    volatile unsigned completed=0;r->icons_busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    __try {
#endif
        if(!lid_mic_list(&r->icons))r->icons_state=-1;completed=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    } __finally {
#endif
        if(!completed)r->icons_state=-1;r->icons_busy=0;
#if LID_SHOP_EXEC_SEH_ENABLED
    }
#endif
}
static void r260_icons_tick(R254Momoka*r){
    if(r->icons_state||r->labels_state!=1||r->stock_state!=1||r->labels.busy||r->stock.busy)return;
    uint32_t error=r254_error(r);volatile unsigned completed=0;
    r->icons_state=-1;r->labels.busy=r->stock.busy=1;r->icons_busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    __try {
#endif
        int executable=1;
        for(unsigned i=0;i<sizeof(lid_ap_prefixes)/sizeof(lid_ap_prefixes[0]);i++)
            if(!r254_executable(r,(uintptr_t)r->image+lid_ap_prefixes[i].rva))executable=0;
        if(executable&&r254_verified(r)&&
           lid_api_prepare(&r->ap_image,&r->labels,(uintptr_t)r->image,1)&&
           lid_api_create(&r->ap_image)&&lid_mic_prepare(&r->icons,&r->ap_image,&r->stock.stock,1)&&
           lid_mic_explanation(&r->icons)&&lid_mic_list(&r->icons)){
            r->labels.visual_context=r;r->labels.visual_after=r260_icon_explanation;
            r->stock.visual_context=r;r->stock.visual_after=r260_icon_list;
            r->icons_state=1;
        }
        completed=1;
#if LID_SHOP_EXEC_SEH_ENABLED
    } __finally {
#endif
        if(!completed){r->icons_state=-1;if(r->ap_image.phase==2)r->ap_image.phase=-1;}
        r->labels.busy=r->stock.busy=0;r->icons_busy=0;r254_error_set(r,error);
#if LID_SHOP_EXEC_SEH_ENABLED
    }
#endif
    log_ascii(r->icons_state==1?"AP SHOPSANITY R260: native/AP Stream icons bound; one retained texture, event-driven views.":
        "AP SHOPSANITY R260: icon admission refused; text/stock/payment flows retained, no creation retry.");
    r254_error_set(r,error);
}
static void shop_momoka_runtime_tick(BYTE*image){
    R254Momoka*r=&g_shop_momoka;R26Transport*t=&g_checks_delivery;
    if(r->phase==R254_FAILED||r->phase==R254_DISABLED||!g_get_tid||
       g_get_tid()!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return;
    if(r->clock){uint64_t now=r->clock();if(r->next&&now<r->next)return;r->next=now+250;}
    r341_momoka_note(0,r->phase);
    r341_momoka_note(1,(r->labels.stopped?1u:0u)|(r->stock.stopped?2u:0u)|(r->icons_state<0?4u:0u));
    if(r->phase==R254_COLD){
        if(!image||!g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid)return;
        r->owner=g_get_tid();r->image=image;
        if(lid_sm_mode(&t->shop_policy)==LID_SM_DISABLED){r254_disabled(r);return;}
        /* Existing pre-input SHA proof is available before bag readiness. */
        if(!r254_verified(r))return;
        BYTE*game=progress_native_game(image);
        if(!game||lid_progress_game_started(game,progress_native_readable)!=1)return;
        if(!r254_api(r)){r254_fail(r,"AP SHOPSANITY R254: Windows API binding refused.");return;}
        for(unsigned i=0;i<20;i++){
            char ch=g_release_slot_key[i];if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return;r->slot[i]=ch;
        }
        if(g_release_slot_key[20])return;r->slot[20]=0;
        if(!m213_copy(&r->driver.adapter,(uintptr_t)image+0xf8b5090,&r->uid,4)||
           !m214_prepare(&r->driver,(uintptr_t)image,(uintptr_t)game,r->owner,r,r254_session)||
           !lid_sg_prepare(&r->gate,&r->driver.adapter,&r->driver.cell.ownership,&t->shop_policy,r->owner,(uintptr_t)r254_getter)||
           !lid_sg_root_prepare(&r->game,&r->root,&r->driver,(uintptr_t)image)){
            r254_fail(r,"AP SHOPSANITY R254: exact getter/session preparation refused.");return;
        }
        unsigned old=0,ignored=0;uint64_t slot=r->driver.adapter.prepared.native.slot;
        if(!r254_protect(r,(void*)(uintptr_t)slot,8,R254_READWRITE,&old)){
            r254_fail(r,"AP SHOPSANITY R254: getter protection refused.");return;
        }
        int installed=m214_activate(&r->driver,(uintptr_t)r254_getter);
        int restored=r254_protect(r,(void*)(uintptr_t)slot,8,old,&ignored);
        if(!installed||!restored){r254_fail(r,"AP SHOPSANITY R254: guarded getter publication refused.");return;}
        r->phase=R254_GUARDED;
        log_ascii("AP SHOPSANITY R254: single guarded getter installed; waiting for explicit current policy/catalogue.");
    }
    if(!r254_session(r)){
        r254_fail(r,"AP SHOPSANITY R254: session retired; purchases halted, retained receipts preserved.");return;
    }
    unsigned mode=lid_sm_mode(&t->shop_policy);
    if(mode==LID_SM_DISABLED){r254_disabled(r);return;}
    if(mode==LID_SM_FAILED){r254_fail(r,"AP SHOPSANITY R254: explicit policy unavailable/invalid; purchases blocked.");return;}
    unsigned transport=__atomic_load_n(&t->state,__ATOMIC_ACQUIRE);
    if(__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)||transport==R26_FAILED||transport==R26_TIMEOUT||
       transport==R26_DONE||transport==R26_CLOSED){
        r254_fail(r,"AP SHOPSANITY R254: transport stopped; purchases halted, retained receipts preserved.");return;
    }
    r267_tick(r); /* R315: current-build tower diagnostic restored; Momoka unchanged. */
    if(r->phase==R254_ACTIVE){(void)r323_colors_native_prepare(image);r257_labels_tick(r);r258_stock_tick(r);r260_icons_tick(r);return;}
    if(mode!=LID_SM_ENABLED||transport!=R26_ACTIVE)return;
    if(r->phase==R254_GUARDED){
        if(__atomic_load_n(&t->shop_catalogue_result,__ATOMIC_ACQUIRE)==LID_SC_FAILED){
            r254_fail(r,"AP SHOPSANITY R254: current catalogue rejected; purchases blocked.");return;
        }
        if(!__atomic_load_n(&t->shop_catalogue.catalogue.initialized,__ATOMIC_ACQUIRE))return;
        r->context=t->queue.context;__atomic_store_n(&r->pinned,1,__ATOMIC_RELEASE);
        if(!r254_shop_bind(t,r,r254_thread,r254_pinned)){
            r254_fail(r,"AP SHOPSANITY R254: current catalogue/bridge binding refused.");return;
        }
        r->waiting_since=r->clock();r->phase=R254_BOUGHT;
    }
    if(r->phase==R254_BOUGHT){
        int ready=r254_shop_ready(t);
        r341_momoka_note(2,(unsigned)(ready+1));
        if(ready<0||(!ready&&r->clock()-r->waiting_since>=120000)){r254_fail(r,"AP SHOPSANITY R254: current bought-state unavailable/invalid.");return;}
        if(!ready)return;
        uint64_t database,menu;
        if(!r254_menu(r,&database,&menu))return; /* no stale UI instance */
        if(!lid_mpc_prepare_owned(&r->price,&r->driver.adapter,database,menu,r->owner,
               &r->driver.cell.ownership,r,r254_session,0,0)||!r254_prepare_graph(r)){
            r254_fail(r,"AP SHOPSANITY R254: coherent native purchase graph preparation refused.");return;
        }
        r->phase=R254_PREPARED;
    }
    if(r->phase==R254_PREPARED){
        int result=lid_pxe_activate(&r->execution,&r->install,&r->hooks);
        if(result==LID_MI_BUSY)return; /* before mutation only, no held cross-tick lock */
        if(result!=LID_MI_HELD){r254_fail(r,"AP SHOPSANITY R254: coherent native activation refused.");return;}
        r->phase=R254_ACTIVE;
        log_ascii("AP SHOPSANITY R254: Momoka KC purchases active; checks only, no immediate reward or hub-entry dispatch.");
    }
}
#endif
#endif
