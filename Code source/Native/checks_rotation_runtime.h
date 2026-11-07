#ifndef LID_CHECKS_ROTATION_RUNTIME_R31_H
#define LID_CHECKS_ROTATION_RUNTIME_R31_H
/* R34 optional integration: prepare_at_idle is called while the existing
   verified idle lease is held, before the Core request starts. Never prepare
   from a periodic gameplay scan. Runtime game validation remains required. */
#if LID_CHECKS_ROTATION_ENABLED
#if !LID_NATIVE_RELOAD_ENABLED
#error "Rotation adapter requires the existing verified reload runtime"
#endif
typedef struct R31Runtime {R31Rotation rotation;R32CallPatchSet hooks;BYTE*exe;unsigned installation_window_verified;} R31Runtime;
static R31Runtime g_checks_rotation_runtime;
static uint32_t checks_rotation_thread(void*c){(void)c;return g_get_tid?g_get_tid():0;}
static int checks_rotation_identity(void*c,R31Identity*out){
    R31Runtime*r=c;const LidCharacterHost*h=&g_progress_host;const LidWireMessage*b=&h->peer.binding;
    if(r->hooks.state!=R31_PATCH_READY||!r->exe||r->exe!=g_reload_runtime.exe||!g_reload_runtime.proxy_verified||
       !g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||
       !h->initialized||h->poisoned||!h->grant_published||!h->peer.bound||!h->peer.granted||
       b->kind!=LID_WIRE_QUIESCED||!lid_wire_valid(b)||!lid_wire_same_session(&h->peer.offer,b)||
       !lid_wire_equal(b->config,h->expected_config,32))return 0;
    unsigned phase=progress_reload_phase();
    if(phase!=LID_RR_LOADING&&phase!=LID_RR_VIEWS&&phase!=LID_RR_READY)return 0;
    out->pid=b->pid;out->birth=b->creation_time;lid_wire_copy(out->nonce,b->nonce,16);
    lid_wire_copy(out->identity,b->identity,32);lid_wire_copy(out->config,b->config,32);return 1;
}
static int checks_rotation_read(void*c,const void*in,void*out,size_t n){
    (void)c;if(!in||!out||!n||!readable_range(in,n))return 0;lid_memcpy(out,in,n);return 1;
}
static int checks_rotation_assign(void*c,R31String*out,const uint16_t*text,unsigned length){
    R31Runtime*r=c;typedef void (WINAPI *Assign)(R31String*,const uint16_t*);
    uint16_t copy[32];
    if(!r->exe||!out||out->data||out->num||out->capacity||!text||length<1||length>32||text[length-1])return 0;
    ((Assign)(r->exe+0x19970))(out,text);
    return out->num==(int)length&&out->capacity>=out->num&&out->capacity<=4096&&out->data&&
      checks_rotation_read(c,out->data,copy,length*2)&&!lid_memcmp(copy,text,length*2);
}
static void checks_rotation_destroy(void*c,R31String*value){
    R31Runtime*r=c;typedef void (WINAPI *Release)(void*);
    if(value->data)((Release)(r->exe+0xdfdf0))(value->data);
    *value=(R31String){0};
}
static void*checks_rotation_original_selector(void*c,void*out){
    R31Runtime*r=c;typedef void* (WINAPI *Selector)(void*);
    return r->hooks.originals[0]?((Selector)r->hooks.originals[0])(out):0;
}
static uint64_t checks_rotation_original_writer(void*c,uint32_t uid,const uint16_t*tmpl,unsigned char flag){
    R31Runtime*r=c;typedef uint64_t (WINAPI *Writer)(uint32_t,const uint16_t*,unsigned char);
    return r->hooks.originals[1]?((Writer)r->hooks.originals[1])(uid,tmpl,flag):0;
}
static uintptr_t checks_rotation_caller(void*caller){
    uintptr_t base=(uintptr_t)g_checks_rotation_runtime.exe,address=(uintptr_t)caller;
    return base&&address>=base&&address-base<0x2b00000?address-base:0;
}
static int checks_rotation_patch_quiet(void*c){
    (void)c;return g_checks_rotation_runtime.installation_window_verified==1&&progress_reload_idle_valid();
}
static int checks_rotation_region(void*c,uintptr_t address,R32Region*out){
    MEMORY_BASIC_INFORMATION info;(void)c;
    if(!g_win.VirtualQuery||!out||!g_win.VirtualQuery((void*)address,&info,sizeof(info)))return 0;
    out->base=(uintptr_t)info.BaseAddress;out->size=info.RegionSize;out->free=info.State==0x10000u;return 1;
}
static void*checks_rotation_allocate_at(void*c,uintptr_t address,size_t size){
    (void)c;return g_win.VirtualAlloc((void*)address,size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
}
static void*WINAPI checks_rotation_selector_detour(void*out){
    return r31_selector(&g_checks_rotation_runtime.rotation,checks_rotation_caller(__builtin_return_address(0)),out);
}
static int checks_rotation_writer_eligible(void*c){
    R31Runtime*r=c;int value=0,found;char line[192];size_t n=0;
    typedef unsigned char (WINAPI *Flag)(int*,const uint16_t*);
    if(!r->exe)return 0;
    found=((Flag)(r->exe+0x128f110))(&value,(const uint16_t*)L"KGF_MET_TUTORIAL_CLEAR")==1;
    append_ascii(line,sizeof(line),&n,"AP ROTATION R34: tutorial found=");append_dec(line,sizeof(line),&n,(unsigned)found);
    append_ascii(line,sizeof(line),&n," positive=");append_dec(line,sizeof(line),&n,value>=1);
    append_ascii(line,sizeof(line),&n,found&&value>=1?"; writer override eligible.":"; original template argument preserved.");line[n]=0;log_ascii(line);
    return found&&value>=1;
}
static uint64_t WINAPI checks_rotation_writer_detour(uint32_t uid,const uint16_t*tmpl,unsigned char flag){
#if LID_CHECKS_DELIVERY_ENABLED
    uintptr_t caller=checks_rotation_caller(__builtin_return_address(0));
    /* Revoke before either known Core writer can rebuild the Stage. Foreign
       threads touch only the atomic revision, never proof payload fields. */
    if(caller==0x111dee5||caller==0x111e8a5)r35_revoke(&g_checks_rotation_proof);
    return r34_writer_guard(&g_checks_rotation_runtime.rotation,caller,uid,tmpl,flag,checks_rotation_writer_eligible);
#else
    return r34_writer_guard(&g_checks_rotation_runtime.rotation,checks_rotation_caller(__builtin_return_address(0)),uid,tmpl,flag,checks_rotation_writer_eligible);
#endif
}
/* PRECONDITION is an already-held reload idle lease and an already verified
   EXE/proxy fingerprint. This adapter cannot establish either on its own. */
static int checks_rotation_prepare(void){
    R31Runtime*r=&g_checks_rotation_runtime;
    if(r->hooks.state!=R31_PATCH_EMPTY)return r->hooks.state==R31_PATCH_READY;
    /* R32 no longer patches the shared selector. Both writes are inside the
       writer reached by the two known Core callers. The scheduling point
       must still hold and verify the real request/save idle lease. R34 only
       opens the extra installation guard during prepare_at_idle. */
    if(r->installation_window_verified!=1||!g_reload_runtime.exe||!g_reload_runtime.proxy_verified||!progress_reload_idle_valid())return 0;
    r->exe=g_reload_runtime.exe;
    R31Ops o={r,checks_rotation_thread,checks_rotation_identity,checks_rotation_read,checks_rotation_assign,
      checks_rotation_destroy,checks_rotation_original_selector,checks_rotation_original_writer};
    R32CallPatchOps p={{&g_reload_runtime,checks_rotation_patch_quiet,progress_reload_patch_read,progress_reload_patch_alloc,
      progress_reload_patch_free,progress_reload_patch_write,progress_reload_patch_protect,progress_reload_patch_flush},
      {r,checks_rotation_region,checks_rotation_allocate_at}};
    void*detours[2]={(void*)checks_rotation_selector_detour,(void*)checks_rotation_writer_detour};
    if(!r31_init(&r->rotation,&o))return 0;
    return r32_call_install(&r->hooks,(uintptr_t)r->exe,1,detours,&p);
}
/* Called only by the optional extension inside the existing idle preparation
   window, after its EXE/proxy checks. No new file hashing, wait or phase change. */
static int checks_rotation_prepare_at_idle(void){
    R31Runtime*r=&g_checks_rotation_runtime;int installed;
    if(!g_get_tid||g_get_tid()!=g_reload_runtime.writer||!g_reload_runtime.proxy_verified||!progress_reload_idle_valid())return 0;
    r->installation_window_verified=1;
    installed=checks_rotation_prepare();
    r->installation_window_verified=0;
#if LID_CHECKS_DELIVERY_ENABLED
    log_ascii(installed?"AP ROTATION R35: local writer/CALL hooks ready; checks await a current verified Stage/session.":
      "AP ROTATION R35: local hook preparation refused; existing reload continues; checks remain closed.");
#else
    log_ascii(installed?"AP ROTATION R34: local writer/CALL hooks ready; Stage observation enabled; checks delivery disabled in test build.":
      "AP ROTATION R34: local hook preparation refused; existing reload continues; checks remain closed.");
#endif
    return installed;
}
#endif
#endif
