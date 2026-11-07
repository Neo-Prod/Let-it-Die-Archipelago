#ifndef LID_PROGRESSION_RESERVATION_H
#define LID_PROGRESSION_RESERVATION_H

/* Candidate for the verified offline build. No hooks or game function calls.
   Include winlite.h first. Only the UI/game-thread host may initiate a lease.
   A fresh current-world view is mandatory on EVERY subsequent operation. */
typedef struct LidProgressGateOps {
    int (WINAPI *readable)(const void*,size_t);
    BOOL (WINAPI *try_enter)(void*);
    void (WINAPI *leave)(void*);
    DWORD (WINAPI *thread_id)(void);
} LidProgressGateOps;
typedef struct LidProgressGateView {
    void* generation;
    void* manager;
    void* request_runner;
    void* save_runner;
    void* manager_vtable;
    void* runner_vtable;
    int game_started;
    int in_hub;
    int stage_ready;
} LidProgressGateView;
typedef struct LidProgressLease {
    LidProgressGateView view;
    LidProgressGateOps ops;
    DWORD owner_thread;
    int active;
    int window;
} LidProgressLease;

static int lid_progress_gate_view_valid(const LidProgressGateView* v,const LidProgressGateOps* ops){
    if(!v||!ops||!ops->readable||!ops->try_enter||!ops->leave||!ops->thread_id)return 0;
    if(!v->generation||!v->manager||!v->request_runner||!v->save_runner||
       v->request_runner==v->save_runner||!v->manager_vtable||!v->runner_vtable)return 0;
    if(!ops->readable(v->manager,0xdc)||!ops->readable(v->request_runner,0x78)||
       !ops->readable(v->save_runner,0x78))return 0;
    return *(void**)v->manager==v->manager_vtable&&
           *(void**)v->request_runner==v->runner_vtable&&*(void**)v->save_runner==v->runner_vtable;
}
/* Caller must first prove this is the current GameInfo. Static leaf1410B5D80:
   MOV EAX,[RCX+844]; SHR EAX,27; AND EAX,1; RET. R4 low-byte snapshots alone
   did not capture this flag. No title/menu state is written here. */
static int lid_progress_game_started(const void* game,int (WINAPI *readable)(const void*,size_t)){
    if(!game||!readable||!readable((const BYTE*)game+0x844,4))return -1;
    return (*(const unsigned*)((const BYTE*)game+0x844)&0x08000000u)!=0;
}
static int lid_progress_gate_same_view(const LidProgressGateView* a,const LidProgressGateView* b){
    return a&&b&&a->generation==b->generation&&a->manager==b->manager&&
           a->request_runner==b->request_runner&&a->save_runner==b->save_runner&&
           a->manager_vtable==b->manager_vtable&&a->runner_vtable==b->runner_vtable;
}
static int lid_progress_gate_arrays_valid(const LidProgressGateView* v){
    int pn=*(int*)((BYTE*)v->manager+0xc0),pm=*(int*)((BYTE*)v->manager+0xc4);
    int wn=*(int*)((BYTE*)v->manager+0xd4),wm=*(int*)((BYTE*)v->manager+0xd8);
    return pn>=0&&pn<=pm&&pm<=4096&&wn>=0&&wn<=wm&&wm<=4096&&
           (!pn||*(void**)((BYTE*)v->manager+0xb8))&&(!wn||*(void**)((BYTE*)v->manager+0xcc));
}
static void lid_progress_gate_unlock(const LidProgressGateView* v,const LidProgressGateOps* ops){
    ops->leave((BYTE*)v->save_runner+0x18);
    ops->leave((BYTE*)v->request_runner+0x18);
}
static int lid_progress_gate_try_locks(const LidProgressGateView* v,const LidProgressGateOps* ops){
    if(!ops->try_enter((BYTE*)v->request_runner+0x18))return 0;
    if(!ops->try_enter((BYTE*)v->save_runner+0x18)){
        ops->leave((BYTE*)v->request_runner+0x18);return 0;
    }
    return 1;
}

/* 1=window held, 0=busy, -1=invalid. Successful begin leaves both critical
   sections held; host applies its prepared batch then starts the owned save
   before leave_window(). No locks are held across frames. */
static int lid_progress_lease_begin(LidProgressLease* lease,const LidProgressGateView* view,
                                    const LidProgressGateOps* ops){
    DWORD tid;
    if(!lease||lease->active||lease->window)return 0;
    if(view&&(view->game_started!=1||view->in_hub!=1||view->stage_ready!=1))return 0;
    if(!lid_progress_gate_view_valid(view,ops))return -1;
    tid=ops->thread_id();if(!tid)return -1;
    if(!lid_progress_gate_try_locks(view,ops))return 0;
    if(!lid_progress_gate_view_valid(view,ops)||!lid_progress_gate_arrays_valid(view)){
        lid_progress_gate_unlock(view,ops);return -1;
    }
    if((*(unsigned*)((BYTE*)view->manager+0xa0)&8)||
       *(int*)((BYTE*)view->manager+0xc0)||*(int*)((BYTE*)view->manager+0xd4)||
       *(void**)((BYTE*)view->request_runner+0x40)||*(void**)((BYTE*)view->save_runner+0x40)){
        lid_progress_gate_unlock(view,ops);return 0;
    }
    lease->view=*view;lease->ops=*ops;lease->owner_thread=tid;
    *(unsigned*)((BYTE*)view->manager+0xa0)|=8;
    lease->active=lease->window=1;
    return 1;
}
static int lid_progress_lease_leave_window(LidProgressLease* lease){
    if(!lease||!lease->active||!lease->window||lease->ops.thread_id()!=lease->owner_thread)return 0;
    lid_progress_gate_unlock(&lease->view,&lease->ops);lease->window=0;return 1;
}
static int lid_progress_lease_reenter(LidProgressLease* lease,const LidProgressGateView* current){
    if(!lease||!lease->active||lease->window)return 0;
    /* Compare new view before dereferencing ANY old manager/lock. */
    if(!lid_progress_gate_same_view(&lease->view,current)||lease->ops.thread_id()!=lease->owner_thread)return -1;
    if(!lid_progress_gate_view_valid(current,&lease->ops))return -1;
    if(!lid_progress_gate_try_locks(current,&lease->ops))return 0;
    if(!lid_progress_gate_view_valid(current,&lease->ops)||!lid_progress_gate_arrays_valid(current)){
        lid_progress_gate_unlock(current,&lease->ops);return -1;
    }
    /* mWaitingResponses and the request worker are transient native activity,
       not evidence that the current world/objects were destroyed. Never touch
       the prepared batch or acknowledge it while that activity is present. */
    if(*(int*)((BYTE*)current->manager+0xd4)||*(void**)((BYTE*)current->request_runner+0x40)){
        lid_progress_gate_unlock(current,&lease->ops);return 0;
    }
    if(!(*(unsigned*)((BYTE*)current->manager+0xa0)&8)){
        /* mRequesting belongs to the engine: UpdateWaitingResponse may clear
           it. Reacquire only at a fully idle native boundary, under BOTH locks.
           Let already queued requests drain before reserving it again. */
        if(*(int*)((BYTE*)current->manager+0xc0)||*(void**)((BYTE*)current->save_runner+0x40)){
            lid_progress_gate_unlock(current,&lease->ops);return 0;
        }
        *(unsigned*)((BYTE*)current->manager+0xa0)|=8u;
    }
    /* Pending may grow during our save, but cannot execute while bit8 is held.
       Save runner may still be active: host can poll, never mutate at that time. */
    lease->window=1;return 1;
}
static int lid_progress_lease_save_active(const LidProgressLease* lease){
    if(!lease||!lease->active||!lease->window||lease->ops.thread_id()!=lease->owner_thread)return -1;
    return *(void**)((BYTE*)lease->view.save_runner+0x40)!=0;
}
/* Close only inside a validated window. On busy, leave the window and keep the
   lease for later; caller must NOT acknowledge or begin another batch. */
static int lid_progress_lease_close(LidProgressLease* lease){
    if(!lease||!lease->active||!lease->window||lease->ops.thread_id()!=lease->owner_thread)return 0;
    if(*(void**)((BYTE*)lease->view.save_runner+0x40)||
       *(void**)((BYTE*)lease->view.request_runner+0x40)||
       *(int*)((BYTE*)lease->view.manager+0xd4)){
        (void)lid_progress_lease_leave_window(lease);return 0;
    }
    *(unsigned*)((BYTE*)lease->view.manager+0xa0)&=~8u;
    lid_progress_gate_unlock(&lease->view,&lease->ops);
    lease->window=lease->active=0;
    return 1;
}

#endif
