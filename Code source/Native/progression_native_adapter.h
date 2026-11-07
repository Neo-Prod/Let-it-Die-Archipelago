#ifndef LID_PROGRESSION_NATIVE_ADAPTER_H
#define LID_PROGRESSION_NATIVE_ADAPTER_H
#include "performance_events.h"
/* Included by mod.c AFTER its runtime helpers. Current SHA f33e91e6...17bddfd.
   No installation or hooks here. All mutable entry points remain guarded by
   the real writer ownership and short engine reservation; host still unarmed. */
static LidProgressLease g_progress_apply_lease;
static LidProgressGateOps g_progress_gate_ops;
static LidProgressSaveJob g_progress_save_job;
static LidCharacterApplication g_progress_application;
static LidCharacterApplyOps g_progress_apply_ops;
static int g_progress_native_ready;
static int g_progress_dirty_failed;
static LidCharacterTransaction g_progress_transaction;
static int g_progress_transaction_last=LID_TX_DISABLED;
static unsigned long long g_progress_snapshot_sequence;
#ifndef LID_CHARACTER_DELIVERY_ENABLED
#define LID_CHARACTER_DELIVERY_ENABLED 0
#endif
#ifndef LID_CHARACTER_DIAGNOSTIC_ONLY
#define LID_CHARACTER_DIAGNOSTIC_ONLY 0
#endif
#ifndef LID_CHARACTER_ENTRY_DELIVERY_READY
#define LID_CHARACTER_ENTRY_DELIVERY_READY 0
#endif
static int WINAPI progress_native_readable(const void* p,size_t n){return readable_range(p,n);}
static int WINAPI progress_native_writable(const void* p,size_t n){
    MEMORY_BASIC_INFORMATION mbi;
    if(!readable_range(p,n)||!g_win.VirtualQuery||!g_win.VirtualQuery(p,&mbi,sizeof(mbi)))return 0;
    return mbi.State==MEM_COMMIT&&v074_writable_region(mbi.Protect);
}
static void* WINAPI progress_native_allocate(size_t size){return g_tbb_malloc?g_tbb_malloc(size):0;}
static void WINAPI progress_native_release(void* p){if(p&&g_tbb_free)g_tbb_free(p);}
static BYTE* progress_native_game(BYTE* exe){
    BYTE* engine;BYTE* world;BYTE* actors;BYTE* first;BYTE* game;void** vt;
    if(!exe||!readable_range(exe+0x29709f0,8))return 0;
    engine=*(BYTE**)(exe+0x29709f0);if(!engine||!readable_range(engine+0x80,8))return 0;
    world=*(BYTE**)(engine+0x80);if(!world||!readable_range(world+0x60,8))return 0;
    actors=*(BYTE**)(world+0x60);if(!actors||!readable_range(actors,8))return 0;
    first=*(BYTE**)actors;if(!first||!readable_range(first+0x714,8))return 0;
    game=*(BYTE**)(first+0x714);if(!game||!readable_range(game,0x1430))return 0;
    vt=*(void***)game;
    if(!vt||!readable_range(vt,0xaa0)||vt[0xa98/8]!=exe+0x10b5d80||vt[0x880/8]!=exe+0x10b36a0)return 0;
    return game;
}
static int progress_native_gate_view(LidProgressGateView* out,int query_ready){
    LidProgressGateView current={0};BYTE* exe=g_progress_event_exe;BYTE* game;BYTE* stage;
    BYTE (WINAPI *is_hub)(void*);void** vt;
    if(!out||!g_progress_native_ready||!(game=progress_native_game(exe))||
       !readable_range(exe+0xf8a0390,8)||!readable_range(exe+0xf8a0490,8))return 0;
    current.generation=game;current.manager=*(void**)(game+0x7e8);
    /* Inline global objects, not pointers: their first words ARE vtables.
       Confirmed in live R14 memory; the read-only reader used this layout. */
    current.request_runner=exe+0xf8a0390;current.save_runner=exe+0xf8a0490;
    current.manager_vtable=exe+0x210a2e0;current.runner_vtable=exe+0x212a498;
    if(!lid_progress_gate_view_valid(&current,&g_progress_gate_ops))return 0;
    current.game_started=lid_progress_game_started(game,progress_native_readable);
    stage=*(BYTE**)(game+0x13e8);
    if(!stage||!readable_range(stage,0x58)||*(void**)stage!=exe+0x21c3350)return 0;
    vt=*(void***)stage;if(!readable_range(vt,0x3d8)||vt[0x3d0/8]!=exe+0x1433900)return 0;
    is_hub=(BYTE (WINAPI *)(void*))vt[0x3d0/8];
    /* Location/readiness predicates only for beginning a mutation. Completion
       and the pre-reload wait need identity, not another hub query. */
    current.in_hub=query_ready&&current.game_started==1?!!is_hub(stage):0;
    current.stage_ready=query_ready&&current.in_hub?lid_event_call(stage,LID_EVENT_BOOL,&g_progress_event_ops,0):0;
    *out=current;return 1;
}
static int progress_native_gate(LidProgressGateView* out){return progress_native_gate_view(out,1);}
static int progress_native_window(void* context){
    LidProgressGateView current;DWORD tid;
    (void)context;
    if(!g_progress_native_ready||!g_progress_apply_lease.active||!g_progress_apply_lease.window||
       !g_progress_runtime_mutex.owned||g_progress_runtime_mutex.poisoned||!g_progress_gate_ops.thread_id)return 0;
    tid=g_progress_gate_ops.thread_id();
    if(tid!=g_progress_apply_lease.owner_thread||tid!=g_progress_runtime_mutex.owner_thread)return 0;
    /* IsReady is required for BEGIN, not for finishing a save: a newly queued
       floor request may make it false while our reservation still blocks it. */
    return progress_native_gate_view(&current,0)&&lid_progress_gate_same_view(&current,&g_progress_apply_lease.view);
}
static int* progress_native_grade(BYTE* common){
#if LID_REFLECTION_READ_SCOPE_ENABLED
    int acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    LID_PERF_BEGIN(grade_lookup_time);
    BYTE* property=lid_event_field(common,"mFighterGrade","IntProperty",&g_progress_event_ops);
#if LID_REFLECTION_READ_SCOPE_ENABLED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    LID_PERF_END(grade_lookup_time,36,"native_grade_field_walk",property!=0);
    if(!property||!readable_range(property,0x98)||*(int*)(property+0x68)!=1||
       *(int*)(property+0x6c)!=4||*(int*)(property+0x94)!=0x2a4||!readable_range(common+0x2a4,4))return 0;
    return (int*)(common+0x2a4);
}
static int progress_native_character(LidCharacterView* out){
    LidCharacterView current={0};LidCharacterModelRoots roots={0};BYTE* exe=g_progress_event_exe;
    BYTE* game;BYTE* userdata;BYTE* payload;BYTE* records;BYTE* body;BYTE* pawn;BYTE* common;void** vt;
    int index,num,capacity;NativeUserDataBool valid;NativeUserDataNoArg get_index;NativeUserDataPtr get_body;
    if(!out||!progress_native_window(0)||!(game=progress_native_game(exe)))return 0;
    userdata=*(BYTE**)(game+0x1428);
    if(!userdata||!readable_range(userdata,0x7c)||*(void**)userdata!=exe+0x21f7ac0)return 0;
    vt=*(void***)userdata;if(!readable_range(vt,0x1138))return 0;
    valid=(NativeUserDataBool)vt[0x280/8];get_index=(NativeUserDataNoArg)vt[0x530/8];get_body=(NativeUserDataPtr)vt[0x1130/8];
    if(!executable_ptr(exe,(void*)valid)||!executable_ptr(exe,(void*)get_index)||!executable_ptr(exe,(void*)get_body)||!valid(userdata))return 0;
    index=get_index(userdata);body=get_body(userdata);payload=*(BYTE**)(userdata+0x74);
    if(!payload||!readable_range(payload,0x2fc)||!body||!readable_range(body,0x220))return 0;
    records=*(BYTE**)(payload+0x2ec);num=*(int*)(payload+0x2f4);capacity=*(int*)(payload+0x2f8);
    if(index<0||num<=0||num>capacity||capacity>64||index>=num||!records||!readable_range(records,(size_t)num*0x220))return 0;
    current.chara=records+(size_t)index*0x220;current.body=body;current.cid=(LidBagString*)current.chara;
    if(!rage_player_objects(exe,&pawn,&common)||!(current.common_grade=progress_native_grade(common))||
       !readable_range(common+0x3e58,sizeof(LidBagArray))||!readable_range(exe+0xf8b0088,4))return 0;
    roots.body_repository=exe+0x27ddf50;roots.bag_repository=exe+0x27e4db8;
    roots.body_repository_vtable=exe+0x212bc30;roots.body_group_vtable=exe+0x212bb80;roots.body_row_vtable=exe+0x212bb30;
    roots.bag_repository_vtable=exe+0x2186cc0;roots.bag_group_vtable=exe+0x2186c50;roots.bag_row_vtable=exe+0x2186ba0;
    roots.uid=*(int*)(exe+0xf8b0088);roots.cid=current.cid;roots.readable=progress_native_readable;
    LID_PERF_BEGIN(models_time);
    int models_result=lid_character_models_resolve(&current.models,&roots);
    LID_PERF_END(models_time,37,"native_character_models",models_result);
    if(models_result!=1)return 0;
    current.generation=game;current.userdata=userdata;current.pawn=pawn;current.common=common;
    current.pawn_slots=(LidBagArray*)(common+0x3e58);*out=current;return 1;
}
static int progress_native_current(void* context,const LidCharacterView* expected){
    LidCharacterView current;(void)context;
    if(!expected||!progress_native_character(&current))return 0;
    return current.generation==expected->generation&&current.userdata==expected->userdata&&
        current.pawn==expected->pawn&&current.common==expected->common&&current.chara==expected->chara&&
        current.body==expected->body&&current.common_grade==expected->common_grade&&
        current.pawn_slots==expected->pawn_slots&&current.models.body==expected->models.body&&
        current.models.bag==expected->models.bag&&current.models.uid==expected->models.uid;
}
static int progress_native_events(void* context,const LidCharacterView* view){
    void* resize;void* recalc;LidProgressGateView gate;int a,b;(void)context;
    if(!view||!g_v074_cfg.valid||g_v074_cfg.fighter_grade!=6||!progress_native_gate(&gate)||
       gate.game_started!=1||gate.in_hub!=1||gate.stage_ready!=1)return 0;
    if(!lid_skill_equipped_floor(view->chara,progress_native_readable,&a)||
       !lid_skill_equipped_floor(view->body,progress_native_readable,&b)||
       !lid_skill_can_apply(&g_v076_skill_capacity_policy,g_release_slot_key,
           g_progress_application.absolute[7],a>b?a:b))return 0;
    resize=lid_event_find(view->common,"SetDeathBagSize",&g_progress_event_ops);
    recalc=lid_event_find(view->common,"StatusUpdateByChangeEquipPart",&g_progress_event_ops);
    return progress_native_grade(view->common)==view->common_grade&&
        lid_event_layout(resize,LID_EVENT_BAG_SIZE,&g_progress_event_ops)&&lid_event_layout(recalc,LID_EVENT_VOID,&g_progress_event_ops)&&
        r40_install_bag_space_guard(view->common);
}
static int progress_native_resize(void* context,const LidCharacterView* view,int target){
    if(!progress_native_window(context)||!progress_native_current(context,view))return 0;
    return lid_event_call(view->common,LID_EVENT_BAG_SIZE,&g_progress_event_ops,target)==1;
}
static int progress_native_recalculate(void* context,const LidCharacterView* view){
    int a,b;
    if(!progress_native_window(context)||!progress_native_current(context,view))return 0;
    if(!g_progress_application.applied||
       !lid_skill_equipped_floor(view->chara,progress_native_readable,&a)||
       !lid_skill_equipped_floor(view->body,progress_native_readable,&b)||
       !lid_skill_record_application(&g_v076_skill_capacity_policy,g_release_slot_key,
           g_progress_application.absolute[7],a>b?a:b))return 0;
    g_v076_active_chara=view->chara;
    return lid_event_call(view->common,LID_EVENT_VOID,&g_progress_event_ops,0)==1;
}
static void WINAPI progress_native_dirty(void){
    BYTE* flag=g_progress_event_exe?g_progress_event_exe+0xf8a1890:0;
    if(!g_progress_save_job.active||__atomic_load_n(&g_progress_save_job.completion,__ATOMIC_ACQUIRE)!=-1||
       !flag||!progress_native_writable(flag,1)){__atomic_store_n(&g_progress_dirty_failed,1,__ATOMIC_RELEASE);return;}
    *(volatile BYTE*)flag=1;__atomic_store_n(&g_progress_dirty_failed,0,__ATOMIC_RELEASE);
}
static BYTE WINAPI progress_native_save(void* runner,void* json){
    BYTE (WINAPI *save)(void*,void*);
    if(__atomic_load_n(&g_progress_dirty_failed,__ATOMIC_ACQUIRE)||!g_progress_event_exe||
       !g_progress_save_job.active||runner!=g_progress_save_job.runner||
       __atomic_load_n(&g_progress_save_job.completion,__ATOMIC_ACQUIRE)!=-1)return 0;
    save=(BYTE (WINAPI *)(void*,void*))(g_progress_event_exe+0x110f110);return save(runner,json);
}
static BYTE WINAPI progress_native_start(void* runner,LidProgressSaveBox* box,const wchar_t* name){
    BYTE (WINAPI *start)(void*,LidProgressSaveBox*,const wchar_t*);
    if(!g_progress_event_exe||!g_progress_save_job.active||runner!=g_progress_save_job.runner||
       !progress_native_window(0)||runner!=g_progress_apply_lease.view.save_runner)return 0;
    start=(BYTE (WINAPI *)(void*,LidProgressSaveBox*,const wchar_t*))(g_progress_event_exe+0x11ca160);
    if(!start(runner,box,name))return 0;
    /* Start returns1 even if its factory returned no thread. We still hold
       runner+18, so a normal completed thread cannot clear+40 under us. */
    return readable_range((BYTE*)runner+0x40,8)&&*(void**)((BYTE*)runner+0x40)!=0;
}
static int progress_transaction_gate(void* context,LidProgressGateView* out){
    (void)context;
    /* Completion never needs reflected IsReady. Before application, wait for
       the body reload observed AFTER this entry notification. */
    return progress_native_gate_view(out,!g_progress_apply_lease.active&&progress_entry_body_ready());
}
static int progress_transaction_character(void* context,LidCharacterView* out){
    char line[512];size_t pos=0;int group,i;const int* levels;
    (void)context;if(!progress_native_character(out))return 0;
    if(g_progress_host.active&&g_progress_host.frozen.sequence!=g_progress_snapshot_sequence){
        line[0]=0;append_ascii(line,sizeof(line),&pos,"AP CHARACTER PRE-APPLY: native-order model/chara/body levels=");
        for(group=0;group<3;group++){
            if(group)append_ascii(line,sizeof(line),&pos," | ");
            levels=group==0?out->models.levels:group==1?(int*)(out->chara+0x14c):(int*)(out->body+0x14c);
            for(i=0;i<10;i++){
                if(i)append_ascii(line,sizeof(line),&pos,"/");
                if(levels[i]<0)append_ascii(line,sizeof(line),&pos,"NEG");
                else append_dec(line,sizeof(line),&pos,(unsigned)levels[i]);
            }
        }
        if(readable_range(out->models.slots,sizeof(LidBagArray))&&readable_range(out->pawn_slots,sizeof(LidBagArray))){
            append_ascii(line,sizeof(line),&pos,"; model/pawn bag=");
            append_dec(line,sizeof(line),&pos,(unsigned)out->models.slots->num);
            append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)out->pawn_slots->num);
        }
        line[pos]=0;log_ascii(line);g_progress_snapshot_sequence=g_progress_host.frozen.sequence;
    }
    return 1;
}
#if LID_EVENT_PROFILE_ENABLED
static LidPerfMark g_transaction_marks[22];
static void progress_transaction_trace(unsigned category,int begin,int result){
    static const char*const names[]={"tx_gate","tx_host_step","tx_host_freeze","tx_lease_begin","tx_character",
        "tx_character_prepare","tx_extra_prepare","tx_validate_armed","tx_character_commit","tx_extra_commit",
        "tx_character_readback","tx_extra_readback","tx_save_begin","tx_save_finish","tx_extra_refresh","tx_refresh_total",
        "tx_host_prove","tx_extra_publish","tx_host_publish","tx_arm_barrier","tx_lease_reenter","tx_lease_close"};
    if(category>=22)return;
    if(begin)g_transaction_marks[category]=lid_perf_mark();
    else lid_perf_end(g_transaction_marks[category],12+category,names[category],result);
}
#endif
static int progress_init_native_adapter(void){
    void* kernel;BYTE* exe=g_progress_event_exe;
    LidCharacterTransactionOps transaction_ops={0,progress_transaction_gate,progress_transaction_character};
    if(g_progress_native_ready)return 1;
    if(!exe||!g_tbb_malloc||!g_tbb_free||!g_progress_runtime_mutex.ops.thread_id||
       !executable_ptr(exe,exe+0x110f110)||!executable_ptr(exe,exe+0x11ca160))return 0;
    kernel=lid_find_module_ascii("kernel32.dll");if(!kernel)return 0;
    g_progress_gate_ops.readable=progress_native_readable;g_progress_gate_ops.thread_id=g_progress_runtime_mutex.ops.thread_id;
    g_progress_gate_ops.try_enter=(BOOL (WINAPI *)(void*))lid_get_export(kernel,"TryEnterCriticalSection");
    g_progress_gate_ops.leave=(void (WINAPI *)(void*))lid_get_export(kernel,"LeaveCriticalSection");
    if(!g_progress_gate_ops.try_enter||!g_progress_gate_ops.leave)return 0;
    g_progress_apply_ops=(LidCharacterApplyOps){0,progress_native_readable,progress_native_writable,progress_native_allocate,progress_native_release,
        progress_native_window,progress_native_current,progress_native_events,progress_native_resize,progress_native_recalculate};
    g_progress_save_job.ops=(LidProgressSaveOps){progress_native_allocate,progress_native_release,progress_native_dirty,progress_native_save,progress_native_start};
    g_progress_native_ready=1;
    if(!lid_transaction_init(&g_progress_transaction,&g_progress_host,&g_progress_apply_lease,&g_progress_save_job,
        &g_progress_application,&g_progress_config,&g_progress_gate_ops,&g_progress_apply_ops,
        exe+0x2186b50,&transaction_ops,10)){g_progress_native_ready=0;return 0;}
#if LID_BLUEPRINT_DELIVERY_ENABLED
    if(!progress_blueprint_attach()){g_progress_native_ready=0;return 0;}
#endif
#if LID_EVENT_PROFILE_ENABLED
    g_progress_transaction.trace_event=progress_transaction_trace;
#endif
    return 1;
}
static int progress_native_transaction_tick(void){
    int status;const char* message;
#if LID_CHARACTER_DIAGNOSTIC_ONLY || !LID_CHARACTER_ENTRY_DELIVERY_READY
    static int diagnostic_logged;
    if(!diagnostic_logged){
        log_ascii("AP CHARACTER R16: native delivery/save suspended until entry-only controller is ready; receipts remain pending.");
        diagnostic_logged=1;
    }
    return LID_TX_DISABLED;
#endif
    if(!g_progress_native_ready)return LID_TX_DISABLED;
    LID_PERF_BEGIN(transaction_time);
    status=lid_transaction_tick(&g_progress_transaction,LID_CHARACTER_DELIVERY_ENABLED);
    LID_PERF_END(transaction_time,1,"transaction_tick",status);
#if LID_EVENT_PROFILE_ENABLED
    if(status==LID_TX_REFRESH_REFUSED){char reason[144];size_t pos=0;
        append_ascii(reason,sizeof(reason),&pos,"AP PERF R26AU: refresh refusal reason=");
        append_ascii(reason,sizeof(reason),&pos,lid_transaction_refresh_reason(g_progress_transaction.refresh_reason));reason[pos]=0;log_ascii(reason);}
#endif
    if(status==LID_TX_DISABLED||status==LID_TX_ENTRY_IDLE||status==g_progress_transaction_last)return status;
#if LID_BLUEPRINT_DELIVERY_ENABLED
    progress_blueprint_log_status(status);
#endif
    g_progress_transaction_last=status;
    switch(status){
        case LID_TX_STARTED:message="AP CHARACTER: frozen batch applied; own save started.";break;
        case LID_TX_WAIT_SAVE:message="AP CHARACTER: awaiting own callback AND inactive save thread.";break;
        case LID_TX_WAIT_RETRY:message="AP CHARACTER: save retry pending; no item reapplication.";break;
        case LID_TX_WAIT_ACK:message="AP CHARACTER: own save/readback done; acknowledgement pending.";break;
        case LID_TX_SAVED:message="AP CHARACTER: own save and RAM readback reported; character-only.";break;
        case LID_TX_REFUSED:message="AP CHARACTER: preflight refused before mutation; target remains pending.";break;
        case LID_TX_REFRESHED:message="AP CHARACTER: loaded caches reconciled with saved models; no new grant or save.";break;
        case LID_TX_ENTRY_DONE:message="AP CHARACTER: entry reconciled; no new grant/save, idle until next return.";break;
        case LID_TX_REFRESH_REFUSED:message="AP CHARACTER: cache refresh refused; verify saved models/identity, no replay repair.";break;
        case LID_TX_FAILED:message="AP CHARACTER: transaction suspended; no SAVED acknowledgement.";break;
        case LID_TX_GENERATION_LOST:message="AP CHARACTER: generation/reservation lost; old objects untouched, no SAVED.";break;
        default:message="AP CHARACTER: waiting for safe delivery conditions.";break;
    }
    log_ascii(message);
    return status;
}
#endif
