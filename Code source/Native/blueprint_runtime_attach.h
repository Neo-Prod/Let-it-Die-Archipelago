#ifndef LID_BLUEPRINT_RUNTIME_ATTACH_H
#define LID_BLUEPRINT_RUNTIME_ATTACH_H
/* Included after progression_native_adapter.h by candidate mod.c only.
   Buffers are persistent BSS, not game stack/allocations. No separate watcher.
   Character bridge verified the executable hash before QUIESCED/GRANT; native
   code additionally rechecks prefix/vtables and current writer/reservations. */
static LidBlueprintEngineAdapter g_blueprint_engine;
static LidBlueprintDomain g_blueprint_domain;
static LidBlueprintNativeRecord g_blueprint_before[32768],g_blueprint_after[32768];
static LidBlueprintRow g_blueprint_semantic[32768];
#include "blueprint_starter_runtime.h"
static int progress_blueprint_reserved(void* context){
    (void)context;
    return g_progress_host.grant_published&&g_progress_host.peer.granted&&!g_progress_host.poisoned&&
        lid_wire_equal(g_progress_host.expected_config,g_progress_host.peer.binding.config,32)&&progress_native_window(0);
}
static int progress_blueprint_guard(void* context){
    LidBlueprintEngineAdapter* engine=context;
    /* No native operation can precede the hash-checked runtime handoff. */
    engine->verified_build=(unsigned)(g_progress_host.grant_published&&g_progress_host.peer.granted);
    return lid_blueprint_engine_guard(engine);
}
static int progress_blueprint_executable(const void* p){return g_progress_event_exe&&executable_ptr(g_progress_event_exe,(void*)p);}
#if LID_DECAL_DELIVERY_ENABLED
#include "decal_runtime_attach.h"
#endif
static int progress_blueprint_publish(void* context,const BYTE* data,size_t size){
    (void)context;
    if(g_progress_apply_lease.active||g_progress_apply_lease.window||!g_progress_runtime_mutex.owned||
       g_progress_runtime_mutex.poisoned||!g_progress_gate_ops.thread_id||
       g_progress_gate_ops.thread_id()!=g_progress_transaction.owner_thread)return 0;
    if(g_starter_policy_state==0){
        if(!starter_marker_publish())return 0;
        g_starter_policy_state=1;
        log_ascii("AP STARTER R22: own save/readback completed; one-time slot policy marker committed.");
    }
    return lid_blueprint_bridge_publish_saved(&g_blueprint_reader,data,size);
}
static int progress_blueprint_attach(void){
    LidBlueprintCommitOps ops;
    if(g_blueprint_domain.initialized)return 1;
    if(!g_progress_event_exe||!g_progress_native_ready||!g_blueprint_pair.initialized)return 0;
    g_blueprint_engine.exe=g_progress_event_exe;g_blueprint_engine.reserved=progress_blueprint_reserved;
    g_blueprint_engine.readable=progress_native_readable;g_blueprint_engine.executable=progress_blueprint_executable;
    ops=(LidBlueprintCommitOps){&g_blueprint_engine,progress_blueprint_guard,lid_blueprint_engine_read,lid_blueprint_engine_upsert};
    if(!lid_blueprint_domain_init(&g_blueprint_domain,&g_progress_transaction,&g_blueprint_pair,
        g_blueprint_before,g_blueprint_after,32768,g_blueprint_semantic,32768,&ops,
        &g_blueprint_engine,lid_blueprint_engine_waiting,0,progress_blueprint_publish))return 0;
    g_starter_policy_state=starter_marker_read();
    g_blueprint_domain.extra.prepare=starter_prepare;
    g_blueprint_domain.extra.commit=starter_commit;
    g_blueprint_domain.extra.readback=starter_readback;
#if LID_DECAL_DELIVERY_ENABLED
    if(!progress_decal_install_domain())return 0;
#endif
    if(g_starter_policy_state<0)log_ascii("AP STARTER R22: invalid/unreadable policy marker; no initial-plan removal authorized.");
    log_ascii("AP BLUEPRINT: common entry/save domain attached; native shop behavior unchanged.");return 1;
}
static void progress_blueprint_log_status(int status){
    char line[192];size_t pos=0;
    if(status==LID_TX_STARTED){
        append_ascii(line,sizeof(line),&pos,"AP BLUEPRINT: applying entry target types=");
        append_dec(line,sizeof(line),&pos,g_blueprint_domain.plan.target_count);
        append_ascii(line,sizeof(line),&pos," preserved=");append_dec(line,sizeof(line),&pos,g_blueprint_domain.plan.preserved_count);
        append_ascii(line,sizeof(line),&pos," upserts=");append_dec(line,sizeof(line),&pos,g_blueprint_domain.plan.action_count);
        line[pos]=0;log_ascii(line);
    }else if(status==LID_TX_SAVED)log_ascii("AP BLUEPRINT: own save/readback and paired native reports completed.");
    else if(status==LID_TX_REFUSED)log_ascii("AP BLUEPRINT: preflight refused; receipts remain pending for a later entry.");
    else if(status==LID_TX_FAILED||status==LID_TX_GENERATION_LOST)log_ascii("AP BLUEPRINT: transaction suspended; no invented save acknowledgement.");
}
#endif
