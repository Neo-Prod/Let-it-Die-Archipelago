#ifndef LID_CHECKS_ROTATION_OBSERVER_R33_H
#define LID_CHECKS_ROTATION_OBSERVER_R33_H
/* Once-per-real-entry scheduling. A failed observation is not retried each
   frame in the hub; a later genuine return may observe a new serial. */
typedef struct R33Boundary {unsigned serial;} R33Boundary;
static int r33_boundary_claim(R33Boundary*s,unsigned serial,unsigned owner,unsigned current,int outside){
    if(!s||!serial||!owner||owner!=current||outside||serial<=s->serial)return 0;
    s->serial=serial;return 1;
}
#if LID_CHECKS_ROTATION_ENABLED
typedef struct R33NativeWindow {LidProgressGateView view;uintptr_t stage;unsigned serial,owner;int held;} R33NativeWindow;
static R33Boundary g_checks_rotation_boundary;
static int checks_rotation_observer_identity(void*c,R31Identity*out){(void)c;return checks_rotation_identity(&g_checks_rotation_runtime,out);}
static int checks_rotation_observer_read(void*c,uintptr_t p,void*out,size_t n){(void)c;return checks_rotation_read(&g_checks_rotation_runtime,(void*)p,out,n);}
static int checks_rotation_observer_window(void*c){
    R33NativeWindow*w=c;LidProgressGateView now;BYTE*game;
    if(!w||!w->held||!g_get_tid||g_get_tid()!=w->owner||g_hub_events.outside||
       __atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE)!=w->serial||
       !progress_native_gate_view(&now,0)||!lid_progress_gate_same_view(&w->view,&now)||
       !lid_progress_gate_view_valid(&now,&g_progress_gate_ops)||!lid_progress_gate_arrays_valid(&now)||
       (*(unsigned*)((BYTE*)now.manager+0xa0)&8u)||
       *(int*)((BYTE*)now.manager+0xc0)||*(int*)((BYTE*)now.manager+0xd4)||
       *(void**)((BYTE*)now.request_runner+0x40)||*(void**)((BYTE*)now.save_runner+0x40))return 0;
    game=now.generation;
    return readable_range(game+0x13e8,8)&&(uintptr_t)*(void**)(game+0x13e8)==w->stage;
}
static void checks_rotation_observe_boundary(unsigned owner){
    unsigned serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
    R33NativeWindow window={0};R31Pin pin;R33Result result={0};int locked=0,cached=0,ok=0;const char*step="pin";
    if(!g_get_tid||!r33_boundary_claim(&g_checks_rotation_boundary,serial,owner,g_get_tid(),g_hub_events.outside))return;
#if LID_CHECKS_DELIVERY_ENABLED
    r35_revoke(&g_checks_rotation_proof);
    uint64_t proof_revision=r35_revision(&g_checks_rotation_proof);
    int published=0;
#endif
    result.status=R33_CONTEXT;
    if(!r31_snapshot(&g_checks_rotation_runtime.rotation,&pin))goto done;
    step="gate";
    if(!g_ap_mode_active||!g_savedata_route_active||!g_progress_native_ready||g_progress_apply_lease.active||g_progress_save_job.active||
       !g_progress_runtime_mutex.owned||g_progress_runtime_mutex.poisoned||g_progress_runtime_mutex.owner_thread!=owner||
       !progress_native_gate_view(&window.view,1)||window.view.game_started!=1||window.view.in_hub!=1||window.view.stage_ready!=1||
       !lid_progress_gate_view_valid(&window.view,&g_progress_gate_ops))goto done;
    step="locks";
    if(!lid_progress_gate_try_locks(&window.view,&g_progress_gate_ops))goto done;locked=1;
    /* Synchronization only: do not use mutation lease_begin, which changes
       manager flag8. No locks, cache scope or borrowed payload survive this call. */
    window.owner=owner;window.serial=serial;window.stage=(uintptr_t)g_hub_entry_signal.stage;window.held=1;
    step="window";if(!checks_rotation_observer_window(&window))goto done;
#if LID_REFLECTION_READ_SCOPE_ENABLED
    cached=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    step="Stage";
    R33Ops ops={&window,owner,checks_rotation_thread,checks_rotation_observer_window,checks_rotation_observer_identity,checks_rotation_observer_read};
    ok=r33_stage(&ops,(uintptr_t)g_checks_rotation_runtime.exe,window.stage,&pin,serial,&result);
done:
#if LID_REFLECTION_READ_SCOPE_ENABLED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,cached);
#else
    (void)cached;
#endif
    if(locked){window.held=0;lid_progress_gate_unlock(&window.view,&g_progress_gate_ops);}
#if LID_CHECKS_DELIVERY_ENABLED
    R31Identity identity;
    if(ok&&checks_rotation_identity(&g_checks_rotation_runtime,&identity))
        published=r35_publish(&g_checks_rotation_proof,owner,g_get_tid(),serial,proof_revision,&pin.identity,&identity,1);
#endif
    /* Log after releasing synchronization. A failed observation stays closed. */
    char line[384];size_t n=0;
    append_ascii(line,sizeof(line),&n,"AP ROTATION R33: observe=");append_ascii(line,sizeof(line),&n,ok?"first40_and_copies_ok":"refused");
    append_ascii(line,sizeof(line),&n," stage=");append_ascii(line,sizeof(line),&n,step);
    append_ascii(line,sizeof(line),&n," serial=");append_dec(line,sizeof(line),&n,serial);
    append_ascii(line,sizeof(line),&n," status=");append_dec(line,sizeof(line),&n,result.status);
    append_ascii(line,sizeof(line),&n," maps=");append_dec(line,sizeof(line),&n,result.nodes);
    append_ascii(line,sizeof(line),&n," edges=");append_dec(line,sizeof(line),&n,result.edges);
    append_ascii(line,sizeof(line),&n," reads=");append_dec(line,sizeof(line),&n,result.reads);
#if LID_CHECKS_DELIVERY_ENABLED
    append_ascii(line,sizeof(line),&n,published?"; session delivery proof published.":"; session delivery proof closed.");
#else
    append_ascii(line,sizeof(line),&n,"; delivery proof remains closed.");
#endif
    line[n]=0;log_ascii(line);
}
#endif
#endif
