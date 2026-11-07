#ifndef LID_SHOP_RUNTIME_DRIVER_H
#define LID_SHOP_RUNTIME_DRIVER_H
#include "shop_native_binding.h"
/* Current SHA only. Initial install uses the same VirtualProtect/jump14
   mechanism as the existing mod. Logical feature activation is all-or-none;
   machine-code writes themselves are NOT atomic/concurrency-proof. */
typedef DWORD (WINAPI *ShopTlsAlloc)(void);
typedef void* (WINAPI *ShopTlsGet)(DWORD);
typedef BOOL (WINAPI *ShopTlsSet)(DWORD,void*);
typedef void (WINAPI *ShopSetError)(DWORD);
typedef void* (WINAPI *ShopAddVeh)(unsigned,void*);
typedef void (WINAPI *ShopSetObject)(void*,RawFString*,void*);
typedef unsigned long long (WINAPI *ShopHandler)(void*,void*,void*);
typedef unsigned long long (WINAPI *ShopPurchase)(int,const wchar_t*,int,int,char);
typedef void* (WINAPI *ShopGetter)(void*,const wchar_t*,void*);
typedef int (WINAPI *ShopPanel)(void*,void*,BYTE,int,RawFString*,int);
typedef unsigned long long (WINAPI *ShopInitial)(void*,unsigned long long,unsigned long long,unsigned long long);
typedef struct ShopDriverFrame {
    struct ShopDriverFrame* previous;
    unsigned depth,purchase_depth,id_units;
    unsigned short id[257];
    LidShopRequestScope scope;
    void* key;void* second_key;
} ShopDriverFrame;
static ShopTlsAlloc shop_tls_alloc;
static ShopTlsGet shop_tls_get_api;
static ShopTlsSet shop_tls_set_api;
static ShopSetError shop_set_error;
static DWORD shop_tls_index=0xffffffffu;
static volatile int shop_install_state,shop_binding_ready,shop_bind_busy;
static volatile unsigned shop_log_bits;
static BYTE* shop_exe;
static Hook shop_hooks[6];
static void* shop_veh_handle;
static LidShopNativeBinding shop_binding;
static LidShopCoordinator shop_coordinator;
static LidShopNativeCoordinatorContext shop_native_context;
static LidShopCoordinatorOps shop_ops;
static ShopSetObject shop_original_set;
static ShopHandler shop_original_handler;
static ShopPurchase shop_original_purchase;
static ShopGetter shop_original_getter;
static ShopPanel shop_original_panel;
static ShopInitial shop_original_initial;
static void shop_once(unsigned bit,const char* line){
    if(!(__atomic_fetch_or(&shop_log_bits,bit,__ATOMIC_ACQ_REL)&bit))log_ascii(line);
}
static ShopDriverFrame* shop_frame(void){
    DWORD error;void* p;
    if(!shop_tls_get_api||shop_tls_index==0xffffffffu)return 0;
    error=g_win.GetLastError?g_win.GetLastError():0;p=shop_tls_get_api(shop_tls_index);
    if(shop_set_error)shop_set_error(error);return p;
}
static int shop_frame_set(ShopDriverFrame* f){
    DWORD error=g_win.GetLastError?g_win.GetLastError():0;int ok=shop_tls_set_api&&shop_tls_set_api(shop_tls_index,f);
    if(shop_set_error)shop_set_error(error);return ok;
}
static unsigned shop_scope_depth(void* p){ShopDriverFrame* f=shop_frame();(void)p;return f?f->depth:0;}
static LONG WINAPI shop_exception_guard(void* p){
    (void)p;
    if(__atomic_load_n(&shop_install_state,__ATOMIC_ACQUIRE)==1&&shop_frame())
        __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);
    return 0; /* EXCEPTION_CONTINUE_SEARCH: original exception handling unchanged. */
}
static int shop_bind(void){
    int expected=0;BYTE* game;
    if(__atomic_load_n(&shop_binding_ready,__ATOMIC_ACQUIRE)){
        if(shop_binding_session(&shop_binding))return 1;
        __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);return 0;
    }
    if(__atomic_load_n(&shop_install_state,__ATOMIC_ACQUIRE)!=1||!g_get_tid||
       !g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||
       g_get_tid()!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE)||
       !(game=progress_native_game(shop_exe))||lid_progress_game_started(game,progress_native_readable)!=1)return 0;
    if(!__atomic_compare_exchange_n(&shop_bind_busy,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    if(!shop_binding_init(&shop_binding,shop_exe,1)){
        __atomic_store_n(&shop_bind_busy,shop_binding.valid?-1:0,__ATOMIC_RELEASE);return 0;
    }
    shop_native_context.binding=&shop_binding;shop_native_context.depth_context=0;shop_native_context.depth=shop_scope_depth;
    shop_ops=shop_binding_coordinator_ops(&shop_native_context);
    if(!lid_shop_coord_init(&shop_coordinator,&shop_ops,1))return 0;
    __atomic_store_n(&shop_coordinator.armed,1,__ATOMIC_RELEASE);
    __atomic_store_n(&shop_binding_ready,1,__ATOMIC_RELEASE);
    shop_once(1,"AP SHOP R20: session bound; named properties and per-thread request scopes enabled.");return 1;
}
static int shop_copy_id(const wchar_t* text,unsigned short* out,unsigned* length){
    unsigned i;
    if(!text||!out||!length)return 0;
    for(i=0;i<=256;i++){
        if(!readable_range(text+i,2))return 0;
        out[i]=(unsigned short)text[i];if(!out[i]){*length=i;return 1;}
    }
    return 0;
}
static int shop_history_key(RawFString* key){
    static const char wanted[]="shophistory";unsigned i;
    if(!key||!readable_range(key,16)||key->num!=12||key->max<12||key->max>4096||
       !readable_range(key->data,24))return 0;
    for(i=0;i<11;i++)if(key->data[i]!=(wchar_t)wanted[i])return 0;
    return !key->data[11];
}
static void* shop_array_from_root(void* root){
    RawFString name;void** vt;
    if(!shop_binding_json_class(root)||!readable_range(root,sizeof(void*)))return 0;
    vt=*(void***)root;
    if(!readable_range(vt,0x260)||vt[0x258/8]!=shop_exe+0x251b30)return 0;
    name.data=L"shophistory";name.num=name.max=12;
    return ((void*(WINAPI*)(void*,RawFString*))vt[0x258/8])(root,&name);
}
static void WINAPI shop_set_object(void* self,RawFString* key,void* value){
    LidShopHistorySource json={0},history={0};LidShopContext context={0};void* a=0;void* b=0;int hub,status;
    ShopDriverFrame* capture=0;ShopDriverFrame* previous;
    /* Capture the script producer ONLY. A native transport decoder can call
       SetObject again: that must not taint the saved origin as UNKNOWN. */
    if(__builtin_return_address(0)==shop_exe+0x49927e&&shop_history_key(key)&&shop_bind()){
        if(!shop_binding_json_source(&shop_binding,value,&json)){
            __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);
        }else if(json.count){
            if(g_tbb_malloc&&g_tbb_free){capture=g_tbb_malloc(sizeof(*capture));a=g_tbb_malloc(1048576);b=g_tbb_malloc(1048576);}
            previous=shop_frame();
            if(capture){lid_memset(capture,0,sizeof(*capture));capture->previous=previous;
                capture->scope.permission.closed=1;capture->key=a;capture->second_key=b;}
            if(capture&&a&&b&&shop_frame_set(capture)){
                hub=shop_binding_active_history(&shop_binding,&context,&history);
                status=lid_shop_coord_capture(&shop_coordinator,&shop_ops,1,&json,hub?&history:0,hub,a,b,1048576);
                if(status==1)log_ascii("AP SHOP R20: complete hub batch captured and matched to active shop history.");
                else if(status==2)shop_once(2,"AP SHOP R20: non-hub/unknown origin retained; original price preserved.");
                if(!shop_frame_set(previous)){
                    __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);
                    a=b=0;capture=0; /* Resident TLS frame owns these allocations. */
                }
            }else __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);
            if(a)g_tbb_free(a);if(b)g_tbb_free(b);if(capture)g_tbb_free(capture);
        }
    }
    shop_original_set(self,key,value);
}
static unsigned long long WINAPI shop_handler(void* self,void* response,void* root){
    ShopDriverFrame* frame;ShopDriverFrame* previous;LidShopHistorySource json={0};void* array;unsigned long long result;
    if(!__atomic_load_n(&shop_binding_ready,__ATOMIC_ACQUIRE))return shop_original_handler(self,response,root);
    if(!shop_binding_session(&shop_binding)){
        __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);return shop_original_handler(self,response,root);
    }
    previous=shop_frame();
    if(previous&&previous->depth>=8){__atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);return shop_original_handler(self,response,root);}
    if(!g_tbb_malloc||!g_tbb_free||!(frame=g_tbb_malloc(sizeof(*frame)))){
        __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);return shop_original_handler(self,response,root);
    }
    lid_memset(frame,0,sizeof(*frame));frame->previous=previous;frame->depth=previous?previous->depth+1:1;frame->scope.permission.closed=1;
    if(!shop_frame_set(frame)){
        __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);g_tbb_free(frame);return shop_original_handler(self,response,root);
    }
    array=shop_array_from_root(root);
    if(array&&shop_binding_json_source(&shop_binding,array,&json)&&json.count&&frame->depth==1){
        frame->key=g_tbb_malloc(1048576);
        if(frame->key){
            if(lid_shop_coord_begin(&shop_coordinator,&shop_ops,1,&json,frame->key,1048576,&frame->scope))
                log_ascii("AP SHOP R20: processing batch matched; scoped native purchase enabled.");
            else log_ascii("AP SHOP R20: processing batch not authorized; original price retained.");
        }
        else __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);
    }
    result=shop_original_handler(self,response,root);
    lid_shop_coord_end(&frame->scope);
    if(shop_frame_set(previous)){
        if(frame->key)g_tbb_free(frame->key);g_tbb_free(frame);
    }else __atomic_store_n(&shop_binding.poisoned,1,__ATOMIC_RELEASE);
    return result;
}
static unsigned long long WINAPI shop_purchase(int uid,const wchar_t* ptid,int level,int time,char flag){
    void* caller=__builtin_return_address(0);ShopDriverFrame* f=shop_frame();unsigned long long result;int selected=0;
    if(f){
        f->purchase_depth++;
        if(f->depth==1&&f->purchase_depth==1&&flag==0&&caller==shop_exe+0x116833a&&uid==shop_binding.uid&&
           shop_binding_session(&shop_binding)&&shop_copy_id(ptid,f->id,&f->id_units))
            selected=lid_shop_coord_purchase(&f->scope,f->id,f->id_units,level);
        if(selected)shop_once(64,"AP SHOP R20: expected buy_part row selected before native resource checks.");
    }
    result=shop_original_purchase(uid,ptid,level,time,flag);
    if(f){if(selected)lid_shop_coord_purchase_end(&f->scope);f->purchase_depth--;}
    return result;
}
static void* WINAPI shop_getter(void* output,const wchar_t* ptid,void* type){
    void* caller=__builtin_return_address(0);void* result=shop_original_getter(output,ptid,type);
    ShopDriverFrame* f;unsigned short id[257];unsigned units,i;
    if(!lid_shop_price_site((unsigned long long)(ULONG_PTR)caller-(unsigned long long)(ULONG_PTR)shop_exe)||type||result!=output||!(f=shop_frame())||f->depth!=1||
       f->purchase_depth!=1||!f->scope.purchase_active||!shop_copy_id(ptid,id,&units)||units!=f->id_units)return result;
    for(i=0;i<units;i++)if(id[i]!=f->id[i])return result;
    if(readable_range(output,0x340)&&*(void**)output==shop_exe+0x210d4b0&&
       progress_native_writable((BYTE*)output+0x17c,8)&&
       lid_shop_coord_price(&shop_coordinator,&shop_ops,1,&f->scope,output,0x340)){
        if(caller==shop_exe+0x1309bde)
            shop_once(4,"AP SHOP R20: required-money preflight set to zero on detached row.");
        else shop_once(32,"AP SHOP R20: final money debit set to zero on detached row.");
    }
    return result;
}
static int WINAPI shop_panel(void* self,void* part,BYTE buy,int level,RawFString* before,int before_level){
    LidShopContext context={0};
    if(buy==1&&shop_bind()&&shop_binding_active_panel(&shop_binding,&context)&&context.item_select==self&&
       readable_range(part,0x234)&&progress_native_writable((BYTE*)part+0x200,4)&&
       lid_shop_price_ui(part,0x234,1,1))
        shop_once(8,"AP SHOP R20: hub purchase panel uses zero price on its existing owned temporary.");
    return shop_original_panel(self,part,buy,level,before,before_level);
}
static unsigned long long WINAPI shop_initial(void* output,unsigned long long a,unsigned long long b,unsigned long long c){
    RawArray* array=output;
    if(__atomic_load_n(&shop_install_state,__ATOMIC_ACQUIRE)==1&&g_ap_mode_active&&g_savedata_route_active&&g_v074_cfg.valid&&
       readable_range(array,16)&&progress_native_writable(array,16)&&array->num>=0&&array->num<=array->max&&array->max<=4096&&
       (!array->num||readable_range(array->data,(size_t)array->num*16))){
        ((void(WINAPI*)(void*,int))(shop_exe+0xa378f0))(output,0);
        shop_once(16,"AP SHOP R20: vanilla initial-plan collector suppressed for AP; existing research records untouched.");return 1;
    }
    return shop_original_initial(output,a,b,c);
}
static int shop_driver_install(BYTE* exe){
    static const BYTE prefixes[6][20]={
        {0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57},
        {0x48,0x8b,0xc4,0x48,0x89,0x50,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56},
        {0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xb8,0xfc,0xff,0xff},
        {0x40,0x55,0x56,0x57,0x48,0x8d,0x6c,0x24,0x80,0x48,0x81,0xec,0x80,0x01,0x00,0x00},
        {0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xa0,0x05,0x00,0x00},
        {0x40,0x57,0x48,0x83,0xec,0x40,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x50}};
    static const unsigned rvas[6]={0x26ad90,0x1167730,0x1319030,0x12ceac0,0x14ba5a0,0x130cba0};
    static const unsigned sizes[6]={16,16,19,16,19,20};
    void* detours[6]={(void*)shop_set_object,(void*)shop_handler,(void*)shop_purchase,(void*)shop_getter,(void*)shop_panel,(void*)shop_initial};
    void* kernel;ShopAddVeh add_veh;unsigned i,j,patched=0;DWORD old,ignored,protections[6];
    if(__atomic_load_n(&shop_install_state,__ATOMIC_ACQUIRE))return shop_install_state==1;
    __atomic_store_n(&shop_install_state,-1,__ATOMIC_RELEASE);
    if(!exe||!g_win.VirtualAlloc||!g_win.VirtualProtect||!g_win.FlushInstructionCache||!g_win.GetCurrentProcess)goto fail;
    kernel=lid_find_module_ascii("kernel32.dll");if(!kernel)goto fail;
    if(!shop_tls_alloc)shop_tls_alloc=(ShopTlsAlloc)lid_get_export(kernel,"TlsAlloc");
    if(!shop_tls_get_api)shop_tls_get_api=(ShopTlsGet)lid_get_export(kernel,"TlsGetValue");
    if(!shop_tls_set_api)shop_tls_set_api=(ShopTlsSet)lid_get_export(kernel,"TlsSetValue");
    if(!shop_set_error)shop_set_error=(ShopSetError)lid_get_export(kernel,"SetLastError");
    add_veh=(ShopAddVeh)lid_get_export(kernel,"AddVectoredExceptionHandler");
    if(!shop_tls_alloc||!shop_tls_get_api||!shop_tls_set_api||!shop_set_error||!add_veh)goto fail;
    for(i=0;i<6;i++)if(!readable_range(exe+rvas[i],sizes[i])||lid_memcmp(exe+rvas[i],prefixes[i],sizes[i]))goto fail;
    if(!executable_ptr(exe,exe+0xa378f0)||!readable_range(exe+0x1168335,5)||!readable_range(exe+0x134f389,5)||
       lid_memcmp(exe+0x1168335,"\xe8\xf6\x0c\x1b\x00",5)||lid_memcmp(exe+0x134f389,"\xe8\x32\xf7\xf7\xff",5))goto fail;
    if(!readable_range(exe+0x499278,6)||lid_memcmp(exe+0x499278,"\xff\x90\xd0\x02\x00\x00",6))goto fail;
    if(!readable_range(exe+0x1309bd9,5)||lid_memcmp(exe+0x1309bd9,"\xe8\xe2\x4e\xfc\xff",5))goto fail;
    shop_tls_index=shop_tls_alloc();if(shop_tls_index==0xffffffffu)goto fail;
    shop_exe=exe;
    for(i=0;i<6;i++){
        Hook* h=shop_hooks+i;BYTE* trampoline=g_win.VirtualAlloc(0,sizes[i]+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        if(!trampoline)goto fail;
        h->target=exe+rvas[i];h->size=sizes[i];h->trampoline=trampoline;
        lid_memcpy(h->original,prefixes[i],sizes[i]);lid_memcpy(trampoline,prefixes[i],sizes[i]);jump14(trampoline+sizes[i],exe+rvas[i]+sizes[i]);
        g_win.FlushInstructionCache(g_win.GetCurrentProcess(),trampoline,sizes[i]+14);
    }
    shop_original_set=(ShopSetObject)shop_hooks[0].trampoline;shop_original_handler=(ShopHandler)shop_hooks[1].trampoline;
    shop_original_purchase=(ShopPurchase)shop_hooks[2].trampoline;shop_original_getter=(ShopGetter)shop_hooks[3].trampoline;
    shop_original_panel=(ShopPanel)shop_hooks[4].trampoline;shop_original_initial=(ShopInitial)shop_hooks[5].trampoline;
    shop_veh_handle=add_veh(0,(void*)shop_exception_guard);if(!shop_veh_handle)goto fail;
    for(i=0;i<6;i++){
        Hook* h=shop_hooks+i;
        if(lid_memcmp(h->target,h->original,h->size))goto rollback;
        if(!g_win.VirtualProtect(h->target,h->size,PAGE_EXECUTE_READWRITE,&old))goto rollback;
        protections[i]=old;
        jump14(h->target,detours[i]);for(j=14;j<h->size;j++)h->target[j]=0x90;patched=i+1;
        g_win.FlushInstructionCache(g_win.GetCurrentProcess(),h->target,h->size);
        if(!g_win.VirtualProtect(h->target,h->size,old,&ignored))goto rollback;
    }
    __atomic_store_n(&shop_install_state,1,__ATOMIC_RELEASE);
    log_ascii("AP SHOP R20: six profiled hooks installed; no idle shop watcher; session binds lazily on game thread.");return 1;
rollback:
    while(patched){Hook* h=shop_hooks+--patched;
        if(g_win.VirtualProtect(h->target,h->size,PAGE_EXECUTE_READWRITE,&old)){
            lid_memcpy(h->target,h->original,h->size);g_win.FlushInstructionCache(g_win.GetCurrentProcess(),h->target,h->size);
            g_win.VirtualProtect(h->target,h->size,protections[patched],&ignored);
        }
    }
fail:
    /* Keep any published trampoline/TLS/VEH resident: a pass-through detour
       can still be in flight. Bounded one-attempt resources, no dangling code. */
    __atomic_store_n(&shop_coordinator.armed,0,__ATOMIC_RELEASE);
    log_ascii("AP SHOP R20: installation rejected; feature disabled and original paths retained.");return 0;
}
#endif
