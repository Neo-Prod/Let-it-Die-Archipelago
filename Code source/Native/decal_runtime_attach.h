#ifndef LID_DECAL_RUNTIME_ATTACH_H
#define LID_DECAL_RUNTIME_ATTACH_H
/* Candidate composition with the EXISTING entry/save extension. Included only
   for LID_DECAL_DELIVERY_ENABLED after native helpers and starter wrappers. */
static LidDecalEngineAdapter g_decal_engine;
static LidDecalCommit g_decal_commit;
static LidDecalLedger g_decal_ledger;
static LidDecalFiles g_decal_files;
static LidDecalRow g_decal_before_rows[4096],g_decal_after_rows[4096],g_decal_scratch_rows[4096];
static LidDecalSnapshot g_decal_before={g_decal_before_rows,4096,0,0};
static LidDecalSnapshot g_decal_after={g_decal_after_rows,4096,0,0};
static LidDecalSnapshot g_decal_scratch={g_decal_scratch_rows,4096,0,0};
static LidCharacterView g_decal_character;
static BYTE g_decal_target[256],g_decal_raw[1152],g_decal_pending[1152],g_decal_saved[1152],g_decal_old_ledger[1152];
static unsigned g_decal_counts[208],g_decal_delta[208];
static unsigned g_decal_prepared,g_decal_pending_owned,g_decal_ledger_published,g_decal_proof_published,g_decal_blocked;
static int progress_decal_stop_read(size_t*size,const char*message){
    g_decal_blocked=1;g_progress_host.poisoned=1;if(size)*size=0;
    if(message)log_ascii(message);return -1;
}
static int progress_decal_outside(void*context){DWORD tid;(void)context;
    if(g_progress_apply_lease.window||!g_progress_runtime_mutex.owned||g_progress_runtime_mutex.poisoned||!g_progress_gate_ops.thread_id)return 0;
    tid=g_progress_gate_ops.thread_id();return tid&&tid==g_progress_runtime_mutex.owner_thread&&(!g_progress_transaction.owner_thread||tid==g_progress_transaction.owner_thread);
}
static void*progress_decal_current_userdata(void*context){(void)context;
    return progress_native_current(0,&g_decal_character)?g_decal_character.userdata:0;
}
static int progress_decal_capture(void){
    if(!progress_native_character(&g_decal_character))return 0;
    g_decal_engine.userdata=g_decal_character.userdata;
    if(!progress_native_readable(g_decal_engine.userdata+0x74,8))return 0;
    g_decal_engine.payload=(BYTE*)(size_t)lid_wire_r64(g_decal_engine.userdata+0x74);
    g_decal_engine.verified_build=(unsigned)(g_progress_host.grant_published&&g_progress_host.peer.granted);
    return lid_decal_engine_guard(&g_decal_engine);
}
static int progress_decal_load_ledger(size_t*size){
    int a,b,parsed;const LidWireMessage*binding=&g_progress_host.peer.binding;
    if(g_decal_blocked)return progress_decal_stop_read(size,0);
    if(g_decal_ledger.loaded)return 1;
    a=lid_decal_file_read(&g_decal_files,LID_DECAL_LEDGER_FILE,g_decal_old_ledger);
    b=lid_decal_file_read(&g_decal_files,LID_DECAL_PENDING_FILE,g_decal_pending);
    if(a<0||b<0)parsed=-1;
    else parsed=lid_decal_ledger_parse(&g_decal_ledger,g_decal_old_ledger,a==1?1152:0,g_decal_pending,b==1?1152:0,binding->identity,binding->config);
    if(parsed!=1)return progress_decal_stop_read(size,parsed==-2?"AP DECAL R25: unresolved durable intent; no replay, no grant or ACK.":"AP DECAL R25: invalid durable journal; no reset, grant or ACK.");
    if(g_decal_ledger.cleanup_pending&&!lid_decal_pending_remove(&g_decal_files))return progress_decal_stop_read(size,"AP DECAL R25: committed intent cleanup failed; delivery suspended.");
    log_ascii("AP DECAL R25: durable delivered-receipt ledger loaded; stock is not a receipt counter.");return 1;
}
/* Called as a read decorator BEFORE engine locks, and only on actual entries. */
static int progress_decal_pair_read(void*context,int id,BYTE*out,size_t capacity,size_t*size){
    LidWireMessage target,decoded;int result;unsigned i;unsigned long long stats=0,plans=0,decals=0;
    /* Binding is already authenticated: detect an invalid journal even when
       Python correctly withholds a TARGET because that journal is invalid. */
    if(id==LID_HOST_TARGET_FILE&&g_progress_host.peer.bound&&g_progress_host.peer.granted&&progress_decal_load_ledger(size)!=1)return -1;
    result=lid_blueprint_pair_read(context,id,out,capacity,size);
    if(result!=1||id!=LID_HOST_TARGET_FILE)return result;
    if(!size||*size!=256||!lid_wire_decode(&target,out,256)||target.kind!=LID_WIRE_TARGET||
       !g_progress_host.peer.bound||!g_progress_host.peer.granted||!lid_wire_same_writer(&target,&g_progress_host.peer.binding)){if(size)*size=0;return 0;}
    result=lid_decal_file_read(&g_decal_files,LID_DECAL_TARGET_FILE,g_decal_raw);
    if(result!=1){*size=0;return result<0?progress_decal_stop_read(size,"AP DECAL R25: unreadable companion; delivery suspended without ACK."):result;}
    if(!lid_decal_target_decode(g_decal_counts,&decoded,g_decal_raw,1152,out)){*size=0;return 0;}
    for(i=0;i<9;i++)stats+=target.counts[i];for(i=0;i<208;i++)decals+=g_decal_counts[i];
    for(i=0;i<329;i++)plans+=g_blueprint_pair.counts[i];
    if(stats+decals+(plans+2)/3>target.history_length)return progress_decal_stop_read(size,"AP DECAL R25: combined receipt budget rejected; delivery suspended.");
    if(g_decal_blocked)return progress_decal_stop_read(size,0);
    lid_wire_copy(g_decal_target,out,256);return 1;
}
static int progress_decal_prepare(void*context,const LidWireMessage*target){
    BYTE frame[256];LidDecalCommitOps ops;LidBlueprintDomain*d=context;
    if(g_decal_blocked||g_decal_pending_owned||!g_decal_ledger.loaded||!lid_wire_encode(frame,256,target)||!lid_wire_equal(frame,g_decal_target,256)||
       !starter_prepare(d,target)||!progress_decal_capture()||!lid_decal_engine_ops(&g_decal_engine,&ops)||
       !lid_decal_engine_stock(&g_decal_engine,&g_decal_before)||
       !lid_decal_ledger_delta(&g_decal_ledger,target,g_decal_counts,g_decal_delta)||
       !lid_decal_commit_prepare(&g_decal_commit,&g_decal_before,&g_decal_after,&g_decal_scratch,g_decal_delta,&ops))return 0;
    g_decal_prepared=1;g_decal_ledger_published=g_decal_proof_published=0;return 1;
}
static int progress_decal_arm(void*context){unsigned i,total=0;LidBlueprintDomain*d=context;
    if(!progress_decal_outside(0)||!g_decal_prepared||g_decal_blocked)return 0;
    for(i=0;i<208;i++)total+=g_decal_delta[i];if(!total)return 1;
    if(g_decal_pending_owned||!lid_blueprint_encode(g_decal_pending,1152,&d->frozen,g_decal_counts,208,lid_decal_catalogue_hash)||
       !lid_decal_file_write(&g_decal_files,LID_DECAL_PENDING_FILE,g_decal_pending)){g_decal_blocked=1;return 0;}
    g_decal_pending_owned=1;log_ascii("AP DECAL R25: frozen intent durably armed before any common-batch mutation.");return 1;
}
static int progress_decal_validate_armed(void*context){LidBlueprintDomain*d=context;unsigned i,total=0;
    for(i=0;i<208;i++)total+=g_decal_delta[i];
    return g_decal_prepared&&!g_decal_blocked&&(!total||g_decal_pending_owned)&&
        lid_decal_engine_stock(&g_decal_engine,&g_decal_scratch)&&lid_decal_unchanged(&g_decal_before,&g_decal_scratch,0)&&
        d->native.guard(d->native.context)==1&&d->native.read_model(d->native.context,&d->after)==1&&lid_bp_commit_unchanged(&d->before,&d->after);
}
static int progress_decal_commit(void*context){return starter_commit(context)&&lid_decal_commit_run(&g_decal_commit)==1;}
static int progress_decal_readback(void*context){return starter_readback(context)&&lid_decal_commit_verify(&g_decal_commit);}
static int progress_decal_refresh(void*context,const LidWireMessage*target){
    unsigned i;LidBlueprintDomain*d=context;
    if(g_decal_blocked||g_decal_pending_owned||!lid_blueprint_domain_refresh(d,target)||!g_decal_ledger.loaded||
       !lid_decal_ledger_delta(&g_decal_ledger,target,g_decal_counts,g_decal_delta)||!progress_decal_capture())return 0;
    for(i=0;i<208;i++)if(g_decal_delta[i])return 0;
    return lid_decal_engine_stock(&g_decal_engine,&g_decal_after)&&lid_decal_engine_project(&g_decal_engine)&&
        lid_decal_engine_view(&g_decal_engine,&g_decal_scratch)&&lid_decal_unchanged(&g_decal_after,&g_decal_scratch,1);
}
static int progress_decal_publish(void*context,const LidWireMessage*target){
    LidBlueprintDomain*d=context;LidDecalLedger updated={0};LidCharacterTransaction*tx=&g_progress_transaction;
    if(!progress_decal_outside(0)||g_progress_apply_lease.active||!g_decal_prepared||g_decal_blocked||!g_decal_commit.applied||g_decal_commit.stopped||
       tx->phase!=LID_TX_ACK||!tx->host->proved||tx->job->active||!lid_bp_domain_matches(target,&d->frozen)||!lid_bp_domain_matches(target,&tx->host->frozen))return 0;
    if(!lid_blueprint_saved_encode(g_decal_saved,1152,target,g_decal_counts,208,lid_decal_catalogue_hash))return 0;
    if(!g_decal_ledger_published){
        if(!lid_decal_file_write(&g_decal_files,LID_DECAL_LEDGER_FILE,g_decal_saved)||
           lid_decal_ledger_parse(&updated,g_decal_saved,1152,0,0,target->identity,target->config)!=1)return 0;
        g_decal_ledger=updated;g_decal_ledger_published=1;
    }
    if(g_decal_pending_owned){if(!lid_decal_pending_remove(&g_decal_files))return 0;g_decal_pending_owned=0;}
    if(!g_decal_proof_published){
        if(!lid_decal_file_write(&g_decal_files,LID_DECAL_SAVED_FILE,g_decal_saved))return 0;
        g_decal_proof_published=1;log_ascii("AP DECAL R25: own save/readback proved; delivered ledger and native decal report committed.");
    }
    return lid_blueprint_domain_publish(d,target);
}
static void progress_decal_reset(void*context){
    if(g_decal_pending_owned)g_decal_blocked=1;
    g_decal_prepared=0;g_decal_ledger_published=g_decal_proof_published=0;
    lid_wire_zero(&g_decal_commit,sizeof(g_decal_commit));g_decal_before.valid=g_decal_after.valid=g_decal_scratch.valid=0;
    lid_blueprint_domain_reset(context);
}
static int progress_decal_install_domain(void){
    g_decal_files.bridge=&g_progress_bridge_io;g_decal_files.outside=progress_decal_outside;
    g_decal_engine.exe=g_progress_event_exe;g_decal_engine.reserved=progress_blueprint_reserved;
    g_decal_engine.current_userdata=progress_decal_current_userdata;g_decal_engine.readable=progress_native_readable;
    g_decal_engine.executable=progress_blueprint_executable;g_decal_engine.scratch=&g_decal_scratch;
    g_blueprint_domain.extra=(LidProgressExtraOps){&g_blueprint_domain,progress_decal_prepare,progress_decal_commit,progress_decal_readback,progress_decal_refresh,progress_decal_publish,progress_decal_reset};
    g_progress_transaction.arm_context=&g_blueprint_domain;g_progress_transaction.arm_before_commit=progress_decal_arm;g_progress_transaction.validate_armed=progress_decal_validate_armed;
    log_ascii("AP DECAL R25: common entry/save composition attached; no extra watcher, shop or skill-equip hooks.");return 1;
}
#endif
