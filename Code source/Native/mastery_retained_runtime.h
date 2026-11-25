#ifndef LID_MASTERY_RETAINED_RUNTIME_H
#define LID_MASTERY_RETAINED_RUNTIME_H
#include "mastery_retained_patch.h"
static LidMasteryRetainedPatch g_mastery_retained_patch;
static void(WINAPI*g_mastery_retained_error_set)(DWORD);
static unsigned g_mastery_retained_logged;
static int mastery_retained_region(void*c,uintptr_t address,R32Region*out){
    MEMORY_BASIC_INFORMATION info;(void)c;
    if(!g_win.VirtualQuery||!out||!g_win.VirtualQuery((void*)address,&info,sizeof(info)))return 0;
    out->base=(uintptr_t)info.BaseAddress;out->size=info.RegionSize;out->free=info.State==0x10000u;return 1;
}
static void*mastery_retained_allocate(void*c,uintptr_t address,size_t size){
    (void)c;return g_win.VirtualAlloc((void*)address,size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
}
static unsigned mastery_retained_multiplier(uintptr_t caller){
    if(__atomic_load_n(&g_mastery_retained_patch.state,__ATOMIC_ACQUIRE)!=R31_PATCH_READY||
       !lid_mastery_retained_caller(caller)||!checks_session()||
       __atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||
       __atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&g_checks_delivery.mastery.state,__ATOMIC_ACQUIRE)!=1)return 1;
    R15Context context;
    if(!r25_host_context(&g_progress_host,&context)||!r15_same_context(&context,&g_checks_delivery.queue.context))return 1;
    unsigned multiplier=g_checks_delivery.mastery.policy.multiplier;
    return multiplier>=1&&multiplier<=100?multiplier:1;
}
static int WINAPI mastery_retained_award(const void*enemy,const void*stage){
    uintptr_t caller=(uintptr_t)__builtin_return_address(0),base=g_mastery_retained_patch.base;
    /* Original native base calculator exactly once. Native callers apply skill
       rate, accumulate NEW awards, derive levels and save/project as usual. */
    int xp=((int(WINAPI*)(const void*,const void*))(base+0x136b8b0))(enemy,stage);
    DWORD error=g_win.GetLastError();unsigned mult=mastery_retained_multiplier(caller>=base?caller-base:0);
    int scaled=lid_mastery_scaled_xp(xp,mult,0);
    if(mult>1&&xp>0&&!__atomic_exchange_n(&g_mastery_retained_logged,1,__ATOMIC_RELAXED)){
        char text[160];size_t n=0;text[0]=0;
        append_ascii(text,sizeof(text),&n,"AP MASTERY R332: retained native XP base=");append_dec(text,sizeof(text),&n,(unsigned)xp);
        append_ascii(text,sizeof(text),&n," scaled=");append_dec(text,sizeof(text),&n,(unsigned)scaled);text[n]=0;log_ascii(text);
    }
    g_mastery_retained_error_set(error);return scaled;
}
static int mastery_retained_prepare_at_idle(void){
    LidMasteryRetainedPatch*s=&g_mastery_retained_patch;
    if(s->state!=R31_PATCH_EMPTY)return s->state==R31_PATCH_READY;
    if(!g_reload_runtime.proxy_verified||!g_reload_runtime.exe||!progress_reload_idle_valid()||
       !g_get_tid||g_get_tid()!=g_reload_runtime.writer||!g_win.GetLastError)return 0;
    void*kernel=lid_find_module_ascii("kernel32.dll");
    if(!kernel||(g_mastery_retained_error_set=(void(WINAPI*)(DWORD))lid_get_export(kernel,"SetLastError"))==0)return 0;
    R32CallPatchOps ops={{&g_reload_runtime,progress_reload_patch_quiet,progress_reload_patch_read,progress_reload_patch_alloc,
      progress_reload_patch_free,progress_reload_patch_write,progress_reload_patch_protect,progress_reload_patch_flush},
      {0,mastery_retained_region,mastery_retained_allocate}};
    int ok=lid_mastery_retained_install(s,(uintptr_t)g_reload_runtime.exe,1,(void*)mastery_retained_award,&ops);
    log_ascii(ok?"AP MASTERY R332: six retained award CALLs ready; shared native calculator unchanged.":
      "AP MASTERY R332: retained award preparation refused; existing native return path retained.");
    return ok;
}
#endif
