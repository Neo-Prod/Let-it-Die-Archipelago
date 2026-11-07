#ifndef LID_RELOAD_REQUEST_RESERVATION_RESEARCH_H
#define LID_RELOAD_REQUEST_RESERVATION_RESEARCH_H
/* Research-only dedicated request-worker reservation. Include existing
   progression_reservation.h for Win32 ops. No manager flag mutation, no wait,
   no engine call and no save/AP proof. Caller provides fresh verified view. */
typedef struct LidReloadRequestView {
    void *generation,*request_runner,*save_runner,*runner_vtable;
    void *handler,*handler_vtable,*core_self;
    void **current_request_global;
    unsigned writer_thread;
    int ownership_verified,callsite_verified,transaction_active;
} LidReloadRequestView;
typedef struct LidReloadRequestLease {
    LidReloadRequestView view;LidProgressGateOps ops;unsigned request_thread;int held;
    unsigned error,after_locks;
} LidReloadRequestLease;
enum {LID_RQ_OK=0,LID_RQ_LEASE,LID_RQ_OPS,LID_RQ_GENERATION,LID_RQ_POINTERS,LID_RQ_OWNERSHIP,
      LID_RQ_RUNNER_READ,LID_RQ_HANDLER_READ,LID_RQ_CURRENT_READ,LID_RQ_REQUEST_VT,LID_RQ_SAVE_VT,
      LID_RQ_HANDLER_VT,LID_RQ_HANDLER_CHANGED,LID_RQ_CORE_SELF,LID_RQ_THREAD,LID_RQ_REQUEST_LOCK,
      LID_RQ_SAVE_LOCK,LID_RQ_REQUEST_WORKER,LID_RQ_REQUEST_CALLABLE,LID_RQ_SAVE_WORKER,LID_RQ_PROXY};
static const char*lid_reload_request_error_name(unsigned error){
    static const char*const names[]={"none","lease_already_held","missing_ops","missing_game_generation","invalid_view_pointers",
        "ownership_or_callsite","unreadable_runners","unreadable_handler","unreadable_current_handler","request_runner_vtable",
        "save_runner_vtable","handler_vtable","current_handler_changed","core_self","thread_owner","request_lock_busy",
        "save_lock_busy","request_worker_missing","request_callable_missing","save_worker_active","proxy_route_profile_state"};
    return error<sizeof(names)/sizeof(names[0])?names[error]:"unknown";
}
/* UE3 packs some fields on four bytes: callable pointer +0x64 is unaligned. */
static void* lid_reload_request_pointer(const void*base,size_t offset){
    void*out;__builtin_memcpy(&out,(const BYTE*)base+offset,sizeof(out));return out;
}
static unsigned lid_reload_request_view_error(const LidReloadRequestView*v,const LidProgressGateOps*o){
    if(!v||!o||!o->readable||!o->try_enter||!o->leave||!o->thread_id)return LID_RQ_OPS;
    if(!v->generation)return LID_RQ_GENERATION;
    if(!v->request_runner||!v->save_runner||v->request_runner==v->save_runner||!v->runner_vtable||!v->handler||
       !v->handler_vtable||!v->current_request_global)return LID_RQ_POINTERS;
    if(!v->writer_thread||v->ownership_verified!=1||v->callsite_verified!=1||v->transaction_active)return LID_RQ_OWNERSHIP;
    if(!o->readable(v->request_runner,0x78)||!o->readable(v->save_runner,0x78))return LID_RQ_RUNNER_READ;
    if(!o->readable(v->handler,0x418))return LID_RQ_HANDLER_READ;
    if(!o->readable(v->current_request_global,sizeof(void*)))return LID_RQ_CURRENT_READ;
    if(lid_reload_request_pointer(v->request_runner,0)!=v->runner_vtable)return LID_RQ_REQUEST_VT;
    if(lid_reload_request_pointer(v->save_runner,0)!=v->runner_vtable)return LID_RQ_SAVE_VT;
    if(lid_reload_request_pointer(v->handler,0)!=v->handler_vtable)return LID_RQ_HANDLER_VT;
    if(lid_reload_request_pointer(v->current_request_global,0)!=v->handler)return LID_RQ_HANDLER_CHANGED;
    return (BYTE*)v->handler+0x410==(BYTE*)v->core_self?LID_RQ_OK:LID_RQ_CORE_SELF;
}
static int lid_reload_request_view_valid(const LidReloadRequestView*v,const LidProgressGateOps*o){
    return lid_reload_request_view_error(v,o)==LID_RQ_OK;
}
/* 1 held,0 busy,-1 invalid. Lock order identical to existing delivery lease.
   Active request worker is EXPECTED here; active save worker is forbidden. */
static int lid_reload_request_begin(LidReloadRequestLease*l,const LidReloadRequestView*v,const LidProgressGateOps*o){
    unsigned tid;
    if(!l)return 0;l->after_locks=0;l->error=LID_RQ_OK;
    if(l->held){l->error=LID_RQ_LEASE;return 0;}
    l->error=lid_reload_request_view_error(v,o);if(l->error)return -1;
    tid=o->thread_id();if(!tid||tid==v->writer_thread){l->error=LID_RQ_THREAD;return -1;}
    if(!o->try_enter((BYTE*)v->request_runner+0x18)){l->error=LID_RQ_REQUEST_LOCK;return 0;}
    if(!o->try_enter((BYTE*)v->save_runner+0x18)){l->error=LID_RQ_SAVE_LOCK;o->leave((BYTE*)v->request_runner+0x18);return 0;}
    l->after_locks=1;l->error=lid_reload_request_view_error(v,o);
    if(l->error){
        o->leave((BYTE*)v->save_runner+0x18);o->leave((BYTE*)v->request_runner+0x18);return -1;
    }
    if(!lid_reload_request_pointer(v->request_runner,0x40))l->error=LID_RQ_REQUEST_WORKER;
    else if(!lid_reload_request_pointer(v->request_runner,0x64))l->error=LID_RQ_REQUEST_CALLABLE;
    else if(lid_reload_request_pointer(v->save_runner,0x40))l->error=LID_RQ_SAVE_WORKER;
    if(l->error){
        o->leave((BYTE*)v->save_runner+0x18);o->leave((BYTE*)v->request_runner+0x18);return 0;
    }
    l->view=*v;l->ops=*o;l->request_thread=tid;l->held=1;return 1;
}
static int lid_reload_request_end(LidReloadRequestLease*l){
    if(!l||!l->held||l->ops.thread_id()!=l->request_thread)return 0;
    l->ops.leave((BYTE*)l->view.save_runner+0x18);l->ops.leave((BYTE*)l->view.request_runner+0x18);
    l->held=0;return 1;
}
#endif
