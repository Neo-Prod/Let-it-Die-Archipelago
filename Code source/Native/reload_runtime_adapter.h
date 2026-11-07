#ifndef LID_RELOAD_RUNTIME_ADAPTER_H
#define LID_RELOAD_RUNTIME_ADAPTER_H
#include "performance_events.h"
/* Included by mod.c after native adapter/blueprint attach/hub observer.
   Complete candidate path is compile gated pending its integration tests and
   real game validation. Do not distribute an unverified enabled build. */
#ifndef LID_NATIVE_RELOAD_ENABLED
#define LID_NATIVE_RELOAD_ENABLED 0
#endif
#include "reload_bound_source.h"
#include "reload_handoff.h"
#include "reload_request_reservation.h"
#include "reload_proxy_guard.h"
#ifndef LID_RELOAD_GATE_VIEW
#define LID_RELOAD_GATE_VIEW progress_native_gate_view
#endif
#ifndef LID_RELOAD_CHARACTER_VIEW
#define LID_RELOAD_CHARACTER_VIEW progress_native_character
#endif
#ifndef LID_RELOAD_PROTECTED_RECHECK
#define LID_RELOAD_PROTECTED_RECHECK lid_reload_bound_source_recheck
#endif
#if LID_NATIVE_RELOAD_ENABLED
enum {LID_RR_NEW=0,LID_RR_HANDOFF,LID_RR_ARMED,LID_RR_LOADING,LID_RR_VIEWS,LID_RR_READY,LID_RR_FAILED};
typedef struct LidReloadRuntime {
    unsigned phase,prepared,writer,request_thread,idle_held,proxy_verified;
    BYTE*exe;BYTE*proxy;unsigned long long epoch,request;
    unsigned long long (WINAPI *clock)(void);
    LidReloadSourceIo io;LidReloadHookSet hooks;LidReloadBoundary boundary;
    LidReloadJsonBinding json;LidReloadBoundSource source;LidReloadNativeBridge bridge;
    LidReloadHandoff handoff;LidReloadRequestLease request_lease;
    LidBoundaryArgs args;void*caller;void*handler;
    wchar_t module_path[1120],save_path[1120];
    wchar_t restored_cid[256];unsigned restored_cid_num,hub_before;
    void*body_before;unsigned view_verified,neutral_ticket,neutral_finished;
    unsigned long long views_started,last_view_tick;
    LidReloadCapacityValues capacities;unsigned capacity_ready;
} LidReloadRuntime;
static LidReloadRuntime g_reload_runtime;
static int progress_reload_attach_override(BYTE*,LidReloadAttachOverride*);
static unsigned progress_reload_phase(void){return __atomic_load_n(&g_reload_runtime.phase,__ATOMIC_ACQUIRE);}
static void progress_reload_phase_set(unsigned p){__atomic_store_n(&g_reload_runtime.phase,p,__ATOMIC_RELEASE);}
static void progress_reload_fail(const char*message){
    /* May be called by an unexpected thread. The boundary/source state has a
       single serialized owner: only close the atomic save gate and publish a
       terminal phase here, never race that owner's non-atomic proof fields. */
    lid_reload_save_fail(&g_reload_runtime.boundary.save);
    progress_reload_phase_set(LID_RR_FAILED);log_ascii(message);
}
static int progress_reload_capacity(LidReloadCapacityValues*out){
    unsigned phase=progress_reload_phase();LidReloadRuntime*r=&g_reload_runtime;
    if(phase==LID_RR_FAILED)return -1;
    if(phase!=LID_RR_LOADING&&phase!=LID_RR_VIEWS)return 0;
    if(!out||!__atomic_load_n(&r->capacity_ready,__ATOMIC_ACQUIRE)){
        progress_reload_fail("AP RELOAD R26AF: passive capacity requested before verified projection; old character refused.");return -1;
    }
    *out=r->capacities;return 1;
}
static unsigned progress_reload_thread(void*unused){(void)unused;return g_progress_runtime_mutex.ops.thread_id?g_progress_runtime_mutex.ops.thread_id():0;}
static unsigned long long progress_reload_clock(void*unused){(void)unused;return g_reload_runtime.clock?g_reload_runtime.clock():0;}
static int progress_reload_build_path(wchar_t*out,size_t cap,const wchar_t*name){
    size_t n=0,i;while(g_progress_slot_directory[n]&&n+1<cap){out[n]=g_progress_slot_directory[n];n++;}
    if(!n||n+1>=cap)return 0;
    if(out[n-1]!=L'\\'&&out[n-1]!=L'/')out[n++]=L'\\';
    for(i=0;name[i];i++){if(n+1>=cap)return 0;out[n++]=name[i];}out[n]=0;return 1;
}
/* Exact known file sizes bound one-time temporary allocations. The EXE is
   45,069,312 bytes on disk, not its much larger virtual/BSS image. */
static int progress_reload_hash_module(void*module,DWORD size,const BYTE*expected){
    LidReloadRuntime*r=&g_reload_runtime;BYTE*bytes=0;BYTE digest[32];HANDLE h;void*algorithm=0;
    DWORD path_size,got,total=0,want;int good=0;
    if(!module||!size||size>64u*1024u*1024u||!expected||!g_win.GetModuleFileNameW)return 0;
    path_size=g_win.GetModuleFileNameW((HMODULE)module,r->module_path,1120);
    if(!path_size||path_size>=1120)return 0;
    h=r->io.open(r->module_path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE||!h)return 0;
    bytes=r->io.allocate((size_t)size+1);if(!bytes)goto done;
    while(total<=size){want=size+1-total;if(want>65536)want=65536;got=0;
        if(!r->io.read(h,bytes+total,want,&got,0)||got>want)goto done;
        if(!got)break;total+=got;
    }
    if(total!=size||r->io.algorithm(&algorithm,L"SHA256",0,0)<0||!algorithm)goto done;
    if(r->io.hash(algorithm,0,0,bytes,total,digest,32)<0)goto done;
    good=lid_wire_equal(digest,expected,32);
done:
    if(algorithm&&r->io.algorithm_close(algorithm,0)<0)good=0;
    if(bytes)r->io.release(bytes);
    if(!r->io.close(h))good=0;return good;
}
static int progress_reload_idle_valid(void){
    LidReloadRuntime*r=&g_reload_runtime;BYTE*exe=r->exe;
    return r->idle_held&&progress_reload_thread(0)==r->writer&&
        readable_range(exe+0xf8a0390,0x78)&&readable_range(exe+0xf8a0490,0x78)&&
        lid_reload_request_pointer(exe+0xf8a0390,0)==exe+0x212a498&&
        lid_reload_request_pointer(exe+0xf8a0490,0)==exe+0x212a498&&
        !lid_reload_request_pointer(exe+0xf8a0390,0x40)&&!lid_reload_request_pointer(exe+0xf8a0490,0x40);
}
static int progress_reload_idle_begin(void){
    LidReloadRuntime*r=&g_reload_runtime;BYTE*exe=r->exe;
    if(r->idle_held||progress_reload_thread(0)!=r->writer||!readable_range(exe+0xf8a0390,0x78)||
       !readable_range(exe+0xf8a0490,0x78)||lid_reload_request_pointer(exe+0xf8a0390,0)!=exe+0x212a498||
       lid_reload_request_pointer(exe+0xf8a0490,0)!=exe+0x212a498)return 0;
    if(!g_progress_gate_ops.try_enter(exe+0xf8a03a8))return 0;
    if(!g_progress_gate_ops.try_enter(exe+0xf8a04a8)){g_progress_gate_ops.leave(exe+0xf8a03a8);return 0;}
    r->idle_held=1;
    if(progress_reload_idle_valid())return 1;
    r->idle_held=0;g_progress_gate_ops.leave(exe+0xf8a04a8);g_progress_gate_ops.leave(exe+0xf8a03a8);return 0;
}
static void progress_reload_idle_end(void){
    LidReloadRuntime*r=&g_reload_runtime;if(!r->idle_held)return;
    r->idle_held=0;g_progress_gate_ops.leave(r->exe+0xf8a04a8);g_progress_gate_ops.leave(r->exe+0xf8a03a8);
}
static int progress_reload_patch_quiet(void*c){(void)c;return progress_reload_idle_valid();}
static int progress_reload_patch_read(void*c,const void*p,size_t n){(void)c;return readable_range(p,n);}
static void*progress_reload_patch_alloc(void*c,size_t n){(void)c;return g_win.VirtualAlloc(0,n,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);}
static int progress_reload_patch_free(void*c,void*p){(void)c;return !!g_win.VirtualFree(p,0,MEM_RELEASE);}
static int progress_reload_patch_write(void*c,void*p,size_t n,unsigned*old){DWORD prot=0;(void)c;
    if(!g_win.VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&prot))return 0;*old=(unsigned)prot;return 1;}
static int progress_reload_patch_protect(void*c,void*p,size_t n,unsigned old){DWORD ignored;(void)c;return !!g_win.VirtualProtect(p,n,(DWORD)old,&ignored);}
static int progress_reload_patch_flush(void*c,void*p,size_t n){(void)c;return !!g_win.FlushInstructionCache(g_win.GetCurrentProcess(),p,n);}
static int progress_reload_proxy_read(LidReloadProxyGuard*out){
    size_t chars=lid_wcslen(g_reload_runtime.save_path)+1;
    return g_reload_runtime.proxy_verified&&chars<=900&&lid_reload_proxy_guard_read(out,g_reload_runtime.proxy,
        lid_reload_proxy_sha,(const unsigned short*)g_reload_runtime.save_path,(unsigned)chars,readable_range);
}
static int progress_reload_json_window(void*unused){
    LidReloadRuntime*r=&g_reload_runtime;unsigned tid=progress_reload_thread(0),p=progress_reload_phase();(void)unused;
    if(p==LID_RR_HANDOFF)return tid==r->writer&&g_progress_host.grant_published&&g_progress_host.peer.granted;
    if(p==LID_RR_VIEWS&&tid==r->writer&&g_progress_apply_lease.active&&!g_progress_apply_lease.window&&
       g_progress_apply_lease.owner_thread==tid){
        LidProgressGateView current;
        return LID_RELOAD_GATE_VIEW(&current,0)&&lid_progress_gate_same_view(&current,&g_progress_apply_lease.view);
    }
    return p==LID_RR_LOADING&&tid==r->request_thread&&r->boundary.stage==LID_BOUNDARY_BUSY&&
        !r->idle_held&&!r->request_lease.held&&readable_range(r->exe+0xf89fec0,8)&&
        lid_reload_request_pointer(r->exe+0xf89fec0,0)==r->handler;
}
static void*progress_reload_json_allocate(void*c,size_t n){(void)c;return g_tbb_malloc(n);}
static void progress_reload_json_release(void*c,void*p){(void)c;if(p)g_tbb_free(p);}
static int progress_reload_bind_json(BYTE*exe){
    LidReloadAttachOverride owned={0};LidReloadRuntime*r=&g_reload_runtime;
    int known=progress_reload_attach_override(exe,&owned),good;
    if(known<0){log_ascii("AP RELOAD R26AI: own shop JSON hook state refused; no unknown detour accepted.");return 0;}
    good=lid_reload_json_bind_ex(&r->json,(uintptr_t)exe,1,r,progress_reload_json_window,readable_range,
        progress_reload_json_allocate,progress_reload_json_release,known?&owned:0);
    if(known)log_ascii(good?"AP RELOAD R26AI: own shop JSON hook/trampoline verified; private JSON bind complete.":
        "AP RELOAD R26AI: own shop hook selected; another JSON/pool precondition refused.");
    return good;
}
static int progress_reload_request_identity(const LidBoundaryArgs*a,void*caller){
    LidReloadRuntime*r=&g_reload_runtime;void*handler;
    if(!a||caller!=r->exe+0x1167309||!readable_range(r->exe+0xf89fec0,8))return 0;
    handler=lid_reload_request_pointer(r->exe+0xf89fec0,0);
    return handler&&readable_range(handler,0x418)&&lid_reload_request_pointer(handler,0)==r->exe+0x210db10&&
        (BYTE*)handler+0x410==(BYTE*)a->self;
}
static int progress_reload_validate(void*c,const LidBoundaryArgs*a){
    LidReloadRuntime*r=&g_reload_runtime;(void)c;
    if(!progress_reload_request_identity(a,r->caller)||!g_progress_runtime_mutex.owned||g_progress_runtime_mutex.poisoned||
       g_progress_runtime_mutex.owner_thread!=r->writer||!g_progress_host.grant_published||!g_progress_host.peer.granted||
       g_progress_host.active||g_progress_host.poisoned||g_progress_apply_lease.active||g_progress_save_job.active)return 0;
    return lid_reload_bound_source_prepare(&r->source,&r->json,&r->io,&g_progress_host.peer,
        g_progress_slot_directory,r->epoch,r->request)&&lid_reload_bound_source_protect(&r->source);
}
static int progress_reload_reserve(void*c){
    LidReloadRuntime*r=&g_reload_runtime;LidReloadRequestView view={0};LidReloadProxyGuard proxy;
    char line[192];size_t n=0;int status;unsigned attempts=0;unsigned long long start=0,now=0;
    const char*reason=0;(void)c;
    view.generation=progress_native_game(r->exe);view.request_runner=r->exe+0xf8a0390;view.save_runner=r->exe+0xf8a0490;
    view.runner_vtable=r->exe+0x212a498;view.handler=r->handler;view.handler_vtable=r->exe+0x210db10;
    view.core_self=r->args.self;view.current_request_global=(void**)(r->exe+0xf89fec0);view.writer_thread=r->writer;
    view.ownership_verified=g_progress_runtime_mutex.owned&&!g_progress_runtime_mutex.poisoned&&g_progress_runtime_mutex.owner_thread==r->writer;
    view.callsite_verified=progress_reload_request_identity(&r->args,r->caller);
    view.transaction_active=g_progress_host.active||g_progress_host.poisoned||!g_progress_host.grant_published||
        !g_progress_host.peer.granted||g_progress_apply_lease.active||g_progress_save_job.active;
    for(;;){
        status=lid_reload_request_begin(&r->request_lease,&view,&g_progress_gate_ops);attempts++;
        if(status==1||r->request_lease.error!=LID_RQ_REQUEST_LOCK)break;
        if(!r->clock||!g_win.Sleep||r->request_lease.held||r->idle_held)break;
        now=r->clock();if(attempts==1)start=now;
        if(now<start||now-start>=1000||attempts>=1000)break;
        if(attempts==1)log_ascii("AP RELOAD R26AK: initial request lock busy; bounded worker wait with no held locks.");
        g_win.Sleep(1);
        if(progress_reload_phase()!=LID_RR_LOADING){reason="cancelled_during_request_wait";break;}
        if(progress_native_game(r->exe)!=view.generation){reason="generation_changed_during_request_wait";break;}
        view.ownership_verified=g_progress_runtime_mutex.owned&&!g_progress_runtime_mutex.poisoned&&g_progress_runtime_mutex.owner_thread==r->writer;
        view.callsite_verified=progress_reload_request_identity(&r->args,r->caller);
        view.transaction_active=g_progress_host.active||g_progress_host.poisoned||!g_progress_host.grant_published||
            !g_progress_host.peer.granted||g_progress_apply_lease.active||g_progress_save_job.active;
    }
    if(status!=1){
        append_ascii(line,sizeof(line),&n,"AP RELOAD R26AJ: reservation refused; reason=");
        append_ascii(line,sizeof(line),&n,reason?reason:lid_reload_request_error_name(r->request_lease.error));
        append_ascii(line,sizeof(line),&n," after_locks=");append_dec(line,sizeof(line),&n,r->request_lease.after_locks);
        append_ascii(line,sizeof(line),&n," attempts=");append_dec(line,sizeof(line),&n,attempts);
        line[n]=0;log_ascii(line);return 0;
    }
    if(attempts>1)log_ascii("AP RELOAD R26AK: initial request lock acquired after bounded wait; strict reservation verified.");
    if(progress_reload_proxy_read(&proxy))return 1;
    (void)lid_reload_request_end(&r->request_lease);r->request_lease.error=LID_RQ_PROXY;
    log_ascii("AP RELOAD R26AJ: reservation refused; reason=proxy_route_profile_state after_locks=1");return 0;
}
static void progress_reload_release(void*c){(void)c;(void)lid_reload_request_end(&g_reload_runtime.request_lease);}
static unsigned char progress_reload_load(void*c){unsigned char result;(void)c;
    if(progress_reload_phase()!=LID_RR_LOADING)return 0;
    LID_PERF_BEGIN(load_time);
#if LID_RELOAD_LOAD_TRACE_ENABLED
    lid_memset(&g_reload_load_trace,0,sizeof(g_reload_load_trace));
#endif
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
    checks_load_decode_begin();
#endif
    result=lid_reload_bound_source_load(&g_reload_runtime.source,&g_reload_runtime.bridge);
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
    checks_load_decode_end();
#endif
    LID_PERF_END(load_time,2,"source_load_capture",result);
#if LID_RELOAD_LOAD_TRACE_ENABLED
    {char line[700];size_t n=0;append_ascii(line,sizeof(line),&n,"AP RELOAD R36 LOAD:");
#define R36_PRINT(field) append_ascii(line,sizeof(line),&n," " #field "=");append_dec(line,sizeof(line),&n,LID_R36_GET(field))
    R36_PRINT(load_stage);R36_PRINT(hold_stage);R36_PRINT(os_error);R36_PRINT(close_error);R36_PRINT(bridge_stage);R36_PRINT(native_result);
    R36_PRINT(apply_stage);R36_PRINT(capture_stage);R36_PRINT(capture_index);R36_PRINT(bound_capture_stage);
    R36_PRINT(hash_match);R36_PRINT(cng_open_status);R36_PRINT(cng_hash_status);R36_PRINT(cng_close_status);
    R36_PRINT(apply_reject);R36_PRINT(bridge_reject);
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
    R36_PRINT(apply_entered);R36_PRINT(apply_invalid);R36_PRINT(apply_inactive);
#endif
#undef R36_PRINT
    append_ascii(line,sizeof(line),&n," seen=");append_dec(line,sizeof(line),&n,__atomic_load_n(&g_reload_runtime.bridge.seen,__ATOMIC_ACQUIRE));
    append_ascii(line,sizeof(line),&n," applied=");append_dec(line,sizeof(line),&n,__atomic_load_n(&g_reload_runtime.bridge.applied,__ATOMIC_ACQUIRE));
    append_ascii(line,sizeof(line),&n," refused=");append_dec(line,sizeof(line),&n,__atomic_load_n(&g_reload_runtime.bridge.refused,__ATOMIC_ACQUIRE));
    line[n]=0;log_ascii(line);}
#endif

    return result==1&&progress_reload_phase()==LID_RR_LOADING;
}
static int progress_reload_readback(void*c){int result;(void)c;
    if(progress_reload_phase()!=LID_RR_LOADING)return 0;
    LID_PERF_BEGIN(readback_time);
    result=lid_reload_bound_source_readback(&g_reload_runtime.source);
    LID_PERF_END(readback_time,3,"source_readback",result);
    return result==1&&progress_reload_phase()==LID_RR_LOADING;
}
static unsigned char progress_reload_project(void*c,void*soul){unsigned char result;(void)c;
    if(progress_reload_phase()!=LID_RR_LOADING)return 0;
    LID_PERF_BEGIN(project_time);
    result=lid_reload_bridge_project(&g_reload_runtime.bridge,soul);
    LID_PERF_END(project_time,4,"source_projection",result);
    return result==1&&progress_reload_phase()==LID_RR_LOADING;
}
/* Structural safety before the native merge. Post-core JSON proof supplies
   semantic bag/decal/bodyuser preservation; no persistent/RPC format conflation. */
static int progress_reload_projection(void*c,void*soul){
    LidReloadRuntime*r=&g_reload_runtime;RawArray characters,bag;LidBagString selected,cid;
    LidGameBagContext strings={0};BYTE*record;int i,j,floor,hits=0;wchar_t digits[12];size_t n=0;unsigned value;
    LidReloadCapacityValues restored={0};int selected_record,slot,skill_target,bag_target;
    (void)c;strings.readable=progress_native_readable;
    if(progress_reload_phase()==LID_RR_FAILED)return 0;
    if(!soul||!readable_range(soul,0x39c)||!readable_range(r->exe+0xf8b0088,4))return 0;
    /* RPC Soul[0] comes from F8B0124, NOT the account UID at F8B0088. */
    value=lid_wire_r32(r->exe+0xf8b0088);
    if(!value||value>2147483647u)return 0;
    do{digits[n++]=(wchar_t)('0'+value%10);value/=10;}while(value);
    for(i=0;i<(int)n;i++)if(r->source.snapshot.uid[i]!=(unsigned short)digits[n-1-(size_t)i])return 0;
    if(r->source.snapshot.uid[n])return 0;
    lid_memcpy(&characters,(BYTE*)soul+0xb0,sizeof(characters));lid_memcpy(&selected,(BYTE*)soul+0x1d4,sizeof(selected));
    if(characters.num<1||characters.num>64||characters.num>characters.max||characters.max>4096||
       !characters.data||!readable_range(characters.data,(size_t)characters.num*0x220)||
       !lid_game_bag_string_valid(&selected,&strings)||selected.num<2)return 0;
    for(i=0;i<characters.num;i++){
        record=(BYTE*)characters.data+(size_t)i*0x220;lid_memcpy(&cid,record,sizeof(cid));
        if(!lid_game_bag_string_valid(&cid,&strings)||cid.num<2||!lid_skill_equipped_floor(record,readable_range,&floor))return 0;
        selected_record=lid_game_bag_string_same(&selected,&cid);
        if(selected_record){
            hits++;if(!lid_character_levels_valid((int*)(record+0x14c)))return 0;
            restored.skill_level=*(int*)(record+0x168);restored.bag_level=*(int*)(record+0x16c);
            restored.rage_level=*(int*)(record+0x170);restored.skill_floor=floor;
        }
        lid_memcpy(&bag,record+0x18c,sizeof(bag));
        if(bag.num<0||bag.num>99||bag.num>bag.max||bag.max>4096||(!bag.data&&bag.max)||
           (bag.num&&(!bag.data||!readable_range(bag.data,(size_t)bag.num*0x44))))return 0;
        for(j=0;j<bag.num;j++){
            BYTE*entry=(BYTE*)bag.data+(size_t)j*0x44;
            if(!lid_game_bag_entry_valid(entry,&strings))return 0;
            if(selected_record&&!lid_game_bag_entry_empty(entry,&strings)){
                slot=*(int*)entry+1;if(slot>restored.bag_floor)restored.bag_floor=slot;
            }
        }
    }
    if(hits!=1)return 0;
    skill_target=v076_capacity_from_level(g_v074_cfg.starting_skill_slots,g_v074_cfg.skill_slots_increase,
        g_v074_cfg.skill_slots_quantity,restored.skill_level);
    bag_target=v076_capacity_from_level(g_v074_cfg.starting_death_bag,g_v074_cfg.death_bag_increase,
        g_v074_cfg.death_bag_quantity,restored.bag_level);
    if(skill_target>64||bag_target<1||bag_target>99)return 0;
    r->restored_cid_num=(unsigned)selected.num;
    lid_memcpy(r->restored_cid,selected.data,(size_t)selected.num*2);
    if(progress_reload_phase()==LID_RR_LOADING){
        if(__atomic_load_n(&r->capacity_ready,__ATOMIC_ACQUIRE))return 0;
        r->capacities=restored;__atomic_store_n(&r->capacity_ready,1,__ATOMIC_RELEASE);
        if(progress_reload_phase()!=LID_RR_LOADING)return 0;
    }
    return 1;
}
static unsigned char progress_reload_original(void*a,void*b,void*c,void*d,const unsigned short*e,int*f,const unsigned short*g){
    LidBoundaryArgs args={a,b,c,d,e,f,g};unsigned char result;
    if(progress_reload_phase()!=LID_RR_LOADING)return 0;
    LID_PERF_BEGIN(core_time);
    result=lid_reload_bound_source_core(&g_reload_runtime.source,&g_reload_runtime.bridge,&args);
    LID_PERF_END(core_time,5,"initial_core_and_postcheck",result);
    return result==1&&progress_reload_phase()==LID_RR_LOADING;
}
static unsigned char WINAPI progress_reload_save_hook(void*runner,void*json){return lid_reload_bridge_save(&g_reload_runtime.bridge,runner,json);}
static unsigned char WINAPI progress_reload_apply_hook(void*runner,void*root){
    void*caller=__builtin_return_address(0);
    return lid_reload_bridge_apply(&g_reload_runtime.bridge,runner,root,caller);
}
static unsigned char progress_reload_core_dispatch(const LidBoundaryArgs*a,void*caller){
    LidReloadRuntime*r=&g_reload_runtime;unsigned p,tid,expected;unsigned char result;unsigned long long start;
    LidBoundaryOps ops={r,progress_reload_validate,progress_reload_reserve,progress_reload_release,
        progress_reload_load,progress_reload_readback,progress_reload_project,progress_reload_projection,progress_reload_original};
    if(!g_ap_mode_active)return r->hooks.core?r->hooks.core(a->self,a->response,a->request_json,a->soul,
        a->entry_item,a->use_elevator,a->debug_template):0;
    tid=progress_reload_thread(0);
    if(!tid||tid==r->writer||!progress_reload_request_identity(a,caller)){
        progress_reload_fail("AP RELOAD R26AE: unexpected native core caller; no restore/save acknowledgement.");return 0;
    }
    p=progress_reload_phase();start=r->clock();
    LID_PERF_BEGIN(handoff_time);
    while(p==LID_RR_HANDOFF){
        if(r->clock()-start>=60000||!g_win.Sleep){progress_reload_fail("AP RELOAD R26AE: initial handoff timed out in request worker.");return 0;}
        g_win.Sleep(1);p=progress_reload_phase();
    }
    LID_PERF_END(handoff_time,6,"core_handoff_wait",(int)p);
    if(p==LID_RR_READY){LID_PERF_BEGIN(return_time);
        result=r->hooks.core(a->self,a->response,a->request_json,a->soul,a->entry_item,a->use_elevator,a->debug_template);
        LID_PERF_END(return_time,7,"return_native_core",result);return result;}
    if(p==LID_RR_LOADING){progress_reload_fail("AP RELOAD R26AE: reentrant core refused.");return 0;}
    if(p!=LID_RR_ARMED)return 0;
    expected=LID_RR_ARMED;
    if(!__atomic_compare_exchange_n(&r->phase,&expected,LID_RR_LOADING,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    r->args=*a;r->caller=caller;r->handler=lid_reload_request_pointer(r->exe+0xf89fec0,0);
    r->request=(unsigned long long)(uintptr_t)r->handler;r->request_thread=tid;
    r->body_before=g_v070_last_body;r->hub_before=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
    result=lid_boundary_call(&r->boundary,&ops,a,1,r->epoch,r->request);
    if(result==1){
        if(r->source.empty_bag_growth){char growth[128];size_t at=0;
            append_ascii(growth,sizeof(growth),&at,"AP RELOAD R26AO: original DeathBag content verified; native empty tail slots=");
            append_dec(growth,sizeof(growth),&at,r->source.empty_bag_growth);growth[at]=0;log_ascii(growth);}
        r->views_started=r->clock();expected=LID_RR_LOADING;
        if(!__atomic_compare_exchange_n(&r->phase,&expected,LID_RR_VIEWS,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
        log_ascii("AP RELOAD R26AE: native load, projection and protected post-core readback passed; delivery still waits for views/guard.");
    }else{
        char line[128];size_t n=0;
        if(r->boundary.error==LID_BOUNDARY_CORE){
            char detail[256];size_t d=0;
            append_ascii(detail,sizeof(detail),&d,"AP RELOAD R26AL: core refusal; reason=");
            append_ascii(detail,sizeof(detail),&d,lid_reload_bound_core_reason(&r->source));
            append_ascii(detail,sizeof(detail),&d," domain=");append_ascii(detail,sizeof(detail),&d,lid_reload_bound_core_domain(&r->source));
            append_ascii(detail,sizeof(detail),&d," called=");append_dec(detail,sizeof(detail),&d,r->source.core_called);
            append_ascii(detail,sizeof(detail),&d," native_result=");append_dec(detail,sizeof(detail),&d,r->source.core_native_result);
            append_ascii(detail,sizeof(detail),&d," cleanup_ok=");append_dec(detail,sizeof(detail),&d,r->source.core_cleanup_ok);
            append_ascii(detail,sizeof(detail),&d," phase=");append_dec(detail,sizeof(detail),&d,progress_reload_phase());
            append_ascii(detail,sizeof(detail),&d," save_phase=");append_dec(detail,sizeof(detail),&d,lid_reload_save_phase(&r->boundary.save));
            detail[d]=0;log_ascii(detail);
            if(r->source.core_stage==LID_RC_COMPARE){
                const LidReloadCompareTrace*t=&r->source.compare_trace;d=0;
                append_ascii(detail,sizeof(detail),&d,"AP RELOAD R26AN: compare refusal; reason=");
                append_ascii(detail,sizeof(detail),&d,lid_reload_compare_reason(t));
                append_ascii(detail,sizeof(detail),&d," side=");append_dec(detail,sizeof(detail),&d,t->side);
                append_ascii(detail,sizeof(detail),&d," expected_units=");append_dec(detail,sizeof(detail),&d,t->expected_units);
                append_ascii(detail,sizeof(detail),&d," actual_units=");append_dec(detail,sizeof(detail),&d,t->actual_units);
                append_ascii(detail,sizeof(detail),&d," equal_called=");append_dec(detail,sizeof(detail),&d,t->equal_called);
                append_ascii(detail,sizeof(detail),&d," equal_result=");append_dec(detail,sizeof(detail),&d,t->equal_result);
                append_ascii(detail,sizeof(detail),&d," cleanup_ok=");append_dec(detail,sizeof(detail),&d,t->cleanup_ok);
                detail[d]=0;log_ascii(detail);
            }
        }
        append_ascii(line,sizeof(line),&n,"AP RELOAD R26AE: boundary refusal code=");append_dec(line,sizeof(line),&n,r->boundary.error);line[n]=0;log_ascii(line);
        progress_reload_fail("AP RELOAD R26AE: restore/core proof refused; native save admission remains closed.");
    }
    return result;
}
static unsigned char WINAPI progress_reload_core_hook(void*a,void*b,void*c,void*d,const unsigned short*e,int*f,const unsigned short*g){
    LidBoundaryArgs args={a,b,c,d,e,f,g};void*caller=__builtin_return_address(0);
    return progress_reload_core_dispatch(&args,caller);
}
static int progress_reload_prepare(BYTE*exe){
    LidReloadRuntime*r=&g_reload_runtime;void*bcrypt,*kernel;void*detours[3];LidReloadProxyGuard proxy;
    LidReloadPatchOps patch={r,progress_reload_patch_quiet,progress_reload_patch_read,progress_reload_patch_alloc,
        progress_reload_patch_free,progress_reload_patch_write,progress_reload_patch_protect,progress_reload_patch_flush};
    if(r->prepared)return 0;r->prepared=1;r->exe=exe;r->writer=progress_reload_thread(0);r->epoch=g_progress_creation_time;
    bcrypt=lid_find_module_ascii("bcrypt.dll");kernel=lid_find_module_ascii("kernel32.dll");
    if(!bcrypt||!kernel||!r->writer||!r->epoch)return 0;
    r->clock=(unsigned long long (WINAPI*)(void))lid_get_export(kernel,"GetTickCount64");
    r->io.open=g_win.CreateFileW;r->io.read=g_win.ReadFile;r->io.close=g_win.CloseHandle;r->io.allocate=g_tbb_malloc;r->io.release=g_tbb_free;
    r->io.algorithm=(LidCngOpen)lid_get_export(bcrypt,"BCryptOpenAlgorithmProvider");
    r->io.hash=(LidCngHash)lid_get_export(bcrypt,"BCryptHash");r->io.algorithm_close=(LidCngClose)lid_get_export(bcrypt,"BCryptCloseAlgorithmProvider");
    r->proxy=lid_find_module_ascii("dinput8.dll");
    if(!r->clock||!r->io.algorithm||!r->io.hash||!r->io.algorithm_close||!r->proxy||
       !progress_reload_hash_module(exe,45069312,lid_wire_game)||!progress_reload_hash_module(r->proxy,31232,lid_reload_proxy_sha)||
       !progress_reload_build_path(r->save_path,1120,L"brggame.sav"))return 0;
    r->proxy_verified=1;
    r->bridge.hooks=&r->hooks;r->bridge.boundary=&r->boundary;r->bridge.context=&r->source;
    r->bridge.runner=exe+0xf8a0490;r->bridge.apply_return=exe+0x11088b3;
    r->bridge.thread_id=progress_reload_thread;r->bridge.capture=lid_reload_bound_source_capture;
    if(!progress_reload_idle_begin())return 0;
    if(!progress_reload_proxy_read(&proxy)){progress_reload_idle_end();return 0;}
#if defined(LID_CHECKS_ROTATION_ENABLED) && LID_CHECKS_ROTATION_ENABLED
    (void)checks_rotation_prepare_at_idle();
#endif
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
    (void)checks_load_decode_prepare_at_idle();
#endif
    detours[0]=(void*)progress_reload_save_hook;detours[1]=(void*)progress_reload_apply_hook;detours[2]=(void*)progress_reload_core_hook;
    if(!lid_reload_hook_install(&r->hooks,(uintptr_t)exe,1,detours,&patch)||
       !lid_boundary_bind(&r->boundary,r->epoch,1,1,1)){
        if(r->hooks.state==LID_RELOAD_HOOK_READY)(void)lid_reload_hook_rollback(&r->hooks,&patch);
        progress_reload_idle_end();return 0;
    }
    progress_reload_phase_set(LID_RR_HANDOFF);progress_reload_idle_end();
    {LidReloadHandoffOps ops={r,progress_reload_clock,progress_reload_thread};
     if(!lid_reload_handoff_init(&r->handoff,&g_progress_host,&ops,60000,250))return 0;}
    return 1;
}
static int progress_reload_view_valid(void){
    LidReloadRuntime*r=&g_reload_runtime;LidCharacterView view={0};LidGameBagContext strings={0};
    LidBagString expected={r->restored_cid,(int)r->restored_cid_num,(int)r->restored_cid_num};
    int floor,body_floor;unsigned serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
    strings.readable=progress_native_readable;
    if(!r->source.core_verified||!r->restored_cid_num||!__atomic_load_n(&r->capacity_ready,__ATOMIC_ACQUIRE)||serial<=r->hub_before||
       !LID_RELOAD_CHARACTER_VIEW(&view)||view.body==r->body_before||
       view.generation!=g_hub_entry_signal.game||
       !lid_game_bag_string_valid(view.cid,&strings)||!lid_game_bag_string_same(view.cid,&expected)||
       !lid_skill_equipped_floor(view.chara,readable_range,&floor)||
       !lid_skill_equipped_floor(view.body,readable_range,&body_floor)||
       floor<r->capacities.skill_floor||body_floor<r->capacities.skill_floor||
       !progress_loaded_levels_ready(view.chara,view.body)||
       *(int*)(view.chara+0x168)!=r->capacities.skill_level||
       *(int*)(view.chara+0x16c)!=r->capacities.bag_level||
       *(int*)(view.chara+0x170)!=r->capacities.rage_level||
       !readable_range(view.pawn_slots,sizeof(*view.pawn_slots))||view.pawn_slots->num<0||view.pawn_slots->num>99||
       view.pawn_slots->num>view.pawn_slots->capacity||view.pawn_slots->capacity>4096||
       view.pawn_slots->num<r->capacities.bag_floor)return 0;
    /* Normal response refresh is observed; actual item commit still performs
       its existing full character/ownership/tail-safety checks before writes. */
    g_v076_active_chara=view.chara; /* Live UserData record, never borrowed RPC. */
    return 1;
}
static void progress_reload_close_idle_lease(void){
    LidProgressGateView current;
    if(!g_progress_apply_lease.active||g_progress_save_job.active)return;
    if(!g_progress_apply_lease.window){
        if(!LID_RELOAD_GATE_VIEW(&current,0)||
           lid_progress_lease_reenter(&g_progress_apply_lease,&current)!=1)return;
    }
    if(lid_progress_lease_save_active(&g_progress_apply_lease)==0)
        (void)lid_progress_lease_close(&g_progress_apply_lease);
    else (void)lid_progress_lease_leave_window(&g_progress_apply_lease);
}
/* Initial completion only; no TARGET/receipt freezing and no SAVED publication.
   Native callback success+guard drain is neutral synchronization, not a claim
   that the proxy's first discarded write reached disk. */
static void progress_reload_views_tick(void){
    LidReloadRuntime*r=&g_reload_runtime;LidProgressGateView gate;LidReloadProxyGuard proxy;
    unsigned long long now=r->clock();int status,active;
    if(now<r->views_started||now-r->views_started>=120000){
        progress_reload_fail("AP RELOAD R26AE: view/neutral-save synchronization timed out; no delivery/ACK.");return;
    }
    if(r->last_view_tick&&now-r->last_view_tick<100)return;r->last_view_tick=now;
    if(!g_progress_apply_lease.active){
        if(!LID_RELOAD_GATE_VIEW(&gate,1)||lid_progress_lease_begin(&g_progress_apply_lease,&gate,&g_progress_gate_ops)!=1)return;
        if(!progress_reload_view_valid()||!progress_reload_proxy_read(&proxy)){
            (void)lid_progress_lease_close(&g_progress_apply_lease);return;
        }
        r->view_verified=1;
        r->neutral_ticket=lid_progress_save_begin(&g_progress_save_job,gate.save_runner,~0ull);
        if(!r->neutral_ticket){(void)lid_progress_lease_close(&g_progress_apply_lease);
            progress_reload_fail("AP RELOAD R26AE: neutral native save could not start; no delivery/ACK.");return;}
        (void)lid_progress_lease_leave_window(&g_progress_apply_lease);return;
    }
    if(!LID_RELOAD_GATE_VIEW(&gate,0)){
        progress_reload_fail("AP RELOAD R26AE: world lost during neutral synchronization; retained job lifetime.");return;
    }
    status=lid_progress_lease_reenter(&g_progress_apply_lease,&gate);
    if(status==0)return;
    if(status<0){progress_reload_fail("AP RELOAD R26AE: neutral reservation lost; no delivery/ACK.");return;}
    active=lid_progress_lease_save_active(&g_progress_apply_lease);
    if(active<0){progress_reload_fail("AP RELOAD R26AE: invalid neutral worker observation; no delivery/ACK.");return;}
    if(active){(void)lid_progress_lease_leave_window(&g_progress_apply_lease);return;}
    if(!r->neutral_ticket){(void)lid_progress_lease_close(&g_progress_apply_lease);return;}
    if(!r->neutral_finished){
        status=lid_progress_save_finish(&g_progress_save_job,r->neutral_ticket,~0ull,0);
        if(status==0){(void)lid_progress_lease_leave_window(&g_progress_apply_lease);return;}
        if(status!=1){(void)lid_progress_lease_close(&g_progress_apply_lease);
            progress_reload_fail("AP RELOAD R26AE: neutral callback refused; no delivery/ACK.");return;}
        if(!progress_reload_proxy_read(&proxy)||!lid_reload_proxy_guard_can_write(&proxy,1)){
            (void)lid_progress_lease_close(&g_progress_apply_lease);
            progress_reload_fail("AP RELOAD R26AE: proxy guard not drained after neutral callback; no delivery/ACK.");return;}
        /* Keep the manager's logical reservation, release both runner locks
           before pool/writer calls that can require main-thread work. */
        (void)lid_progress_lease_leave_window(&g_progress_apply_lease);
        LID_PERF_BEGIN(neutral_time);
        int content_ok=LID_RELOAD_PROTECTED_RECHECK(&r->source);
        LID_PERF_END(neutral_time,9,"neutral_protected_recheck",content_ok);
        if(!content_ok){
            progress_reload_close_idle_lease();
            progress_reload_fail("AP RELOAD R26AE: protected content changed during neutral save; no delivery/ACK.");return;
        }
        if(progress_reload_phase()!=LID_RR_VIEWS){progress_reload_close_idle_lease();return;}
        r->neutral_finished=1;return;
    }
    if(!progress_reload_proxy_read(&proxy)||!lid_reload_proxy_guard_can_write(&proxy,1)){
        (void)lid_progress_lease_close(&g_progress_apply_lease);
        progress_reload_fail("AP RELOAD R26AE: proxy route/guard changed before delivery authorization.");return;
    }
    if(!lid_progress_lease_close(&g_progress_apply_lease))return;
    if(!lid_reload_bound_source_close(&r->source)||lid_reload_save_phase(&r->boundary.save)!=LID_RELOAD_SAVE_ACCEPTED||
       !lid_boundary_observe(&r->boundary,r->epoch,r->request,(int)r->view_verified,1)||
       !lid_boundary_delivery_ready(&r->boundary)){
        progress_reload_fail("AP RELOAD R26AE: completion identity/cleanup refused; no delivery/ACK.");return;
    }
    {unsigned expected=LID_RR_VIEWS;
     if(!__atomic_compare_exchange_n(&r->phase,&expected,LID_RR_READY,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;}
    log_ascii("AP RELOAD R26AE: normal view, own neutral callback, guard drain and protected readback complete; entry-only item dispatcher enabled, no neutral ACK.");
}
/* Returns1 to pause legacy AP mutators. The main game/UI continues normally.
   READY/FAILED are atomic cached checks, no steady hub file/clock loop. */
static int progress_reload_tick(BYTE*exe){
    LidReloadRuntime*r=&g_reload_runtime;unsigned phase=progress_reload_phase();int status;
    if(phase==LID_RR_READY)return 0;
    if(phase==LID_RR_FAILED)return 1;
    if(phase==LID_RR_NEW){
        if(!g_savedata_route_active||!g_v074_cfg.valid)return 0;
        LID_PERF_BEGIN(setup_time);
        int setup_ok=progress_prepare_host()&&progress_init_native_adapter()&&progress_reload_prepare(exe);
        LID_PERF_END(setup_time,10,"reload_setup_hash_hooks",setup_ok);
        if(!setup_ok){
            progress_reload_fail("AP RELOAD R26AE: setup refused; do not treat missing/partial hooks as write protection.");return 1;
        }
        phase=progress_reload_phase();
    }
    if(progress_reload_thread(0)!=r->writer)return 1;
    if(phase==LID_RR_HANDOFF){
        LID_PERF_BEGIN(handoff_tick_time);
        status=lid_reload_handoff_tick(&r->handoff);
        LID_PERF_END(handoff_tick_time,11,"handoff_tick",status);
        if(status<0){progress_reload_fail("AP RELOAD R26AE: writer handoff refused; receipts remain pending.");return 1;}
        if(status==1){
            if(!progress_reload_bind_json(exe)){
                progress_reload_fail("AP RELOAD R26AE: native JSON profiles refused after handoff.");return 1;
            }
            {unsigned expected=LID_RR_HANDOFF;
             (void)__atomic_compare_exchange_n(&r->phase,&expected,LID_RR_ARMED,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
        }
    }
    if(phase==LID_RR_VIEWS)progress_reload_views_tick();
    return 1;
}
static int progress_reload_delivery_ready(void){return progress_reload_phase()==LID_RR_READY;}
#endif
#endif
