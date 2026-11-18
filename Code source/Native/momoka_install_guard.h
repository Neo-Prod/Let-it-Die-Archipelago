#ifndef LID_MOMOKA_INSTALL_GUARD_H
#define LID_MOMOKA_INSTALL_GUARD_H
#include <stdint.h>
#include <stddef.h>
#include "momoka_native_hooks.h"
/* INSTALL/RESTORE ONLY. Not a purchase lease, not a Save barrier. Invoked on
   the already established game thread. Native request/save launch uses each
   inline runner's critical section +0x18. Holding both while +0x40 is zero
   excludes those native launchers until release. No game calls/allocations,
   waits, manager flag changes or cross-tick ownership. Does NOT exclude unknown
   direct callers or a different executable. Never activate from a mere bool. */
enum {LID_MI_INVALID=-1,LID_MI_BUSY=0,LID_MI_HELD=1};
typedef struct LidMomokaInstallOps {
    void*context;int(*copy)(void*,uintptr_t,void*,size_t);
    int(*try_enter)(void*,uintptr_t);void(*leave)(void*,uintptr_t);
    uint32_t(*thread)(void*);
} LidMomokaInstallOps;
typedef struct LidMomokaInstallView {uintptr_t game,manager,request_runner,save_runner;} LidMomokaInstallView;
typedef struct LidMomokaInstallGuard {
    uintptr_t base;uint32_t writer;unsigned held,initialized;
    LidMomokaInstallOps ops;LidMomokaInstallView view;
} LidMomokaInstallGuard;
static int lid_mi_read(const LidMomokaInstallGuard*g,uintptr_t p,size_t offset,void*out,size_t n){
    return out&&n&&p&&p<=UINTPTR_MAX-offset&&p+offset<=UINTPTR_MAX-n&&
      g->ops.copy(g->ops.context,p+offset,out,n)==1;
}
/* Current-world discovery follows existing progression_native_adapter.h.
   All reads only; no Engine/Stage IsReady/IsHub calls under the locks. */
static int lid_mi_view(const LidMomokaInstallGuard*g,LidMomokaInstallView*out){
    uintptr_t engine,world,actors,first,game,manager,vt,method;uint32_t started;
    if(!g||!g->initialized||!out||!lid_mi_read(g,g->base,0x29759f0,&engine,8)||
       !lid_mi_read(g,engine,0x80,&world,8)||!lid_mi_read(g,world,0x60,&actors,8)||
       !lid_mi_read(g,actors,0,&first,8)||!lid_mi_read(g,first,0x714,&game,8)||
       !lid_mi_read(g,game,0,&vt,8)||!lid_mi_read(g,vt,0xa98,&method,8)||method!=g->base+0x10b5c50||
       !lid_mi_read(g,vt,0x880,&method,8)||method!=g->base+0x10b35a0||
       !lid_mi_read(g,game,0x844,&started,4)||!(started&0x08000000u)||
       !lid_mi_read(g,game,0x7e8,&manager,8)||!lid_mi_read(g,manager,0,&vt,8)||vt!=g->base+0x210e040||
       !lid_mi_read(g,g->base,0xf8a5390,&vt,8)||vt!=g->base+0x212e0e8||
       !lid_mi_read(g,g->base,0xf8a5490,&vt,8)||vt!=g->base+0x212e0e8)return 0;
    *out=(LidMomokaInstallView){game,manager,g->base+0xf8a5390,g->base+0xf8a5490};return 1;
}
static int lid_mi_same(const LidMomokaInstallView*a,const LidMomokaInstallView*b){
    return a->game==b->game&&a->manager==b->manager&&a->request_runner==b->request_runner&&a->save_runner==b->save_runner;
}
static int lid_mi_idle(const LidMomokaInstallGuard*g,const LidMomokaInstallView*v){
    struct {uintptr_t data;int32_t count,capacity;} pending,waiting;uintptr_t worker;uint32_t flags;
    if(!lid_mi_read(g,v->request_runner,0x40,&worker,8))return LID_MI_INVALID;if(worker)return LID_MI_BUSY;
    if(!lid_mi_read(g,v->save_runner,0x40,&worker,8))return LID_MI_INVALID;if(worker)return LID_MI_BUSY;
    if(!lid_mi_read(g,v->manager,0xa0,&flags,4)||
       !lid_mi_read(g,v->manager,0xb8,&pending,16)||
       !lid_mi_read(g,v->manager,0xcc,&waiting,16)||pending.count<0||pending.capacity<pending.count||pending.capacity>4096||
       waiting.count<0||waiting.capacity<waiting.count||waiting.capacity>4096)return LID_MI_INVALID;
    return (flags&8)||pending.count||waiting.count?LID_MI_BUSY:LID_MI_HELD;
}
/* Host supplies its previously verified game-thread ID; this function does not
   establish writer identity merely from the thread calling prepare. Zeroed
   private storage, stable module/run binding and native OS ops are required. */
static int lid_momoka_install_prepare(LidMomokaInstallGuard*g,uintptr_t base,int exact_sha,
                                  uint32_t verified_writer,const LidMomokaInstallOps*o){
    if(!g||g->initialized||!base||base>UINTPTR_MAX-0xf8a5508||exact_sha!=1||!verified_writer||
       !o||!o->copy||!o->try_enter||!o->leave||!o->thread)return 0;
    g->base=base;g->writer=verified_writer;g->ops=*o;g->initialized=1;return 1;
}
static int lid_momoka_install_begin(LidMomokaInstallGuard*g){
    LidMomokaInstallView before,after;
    if(!g||!g->initialized||g->held||g->ops.thread(g->ops.context)!=g->writer||!lid_mi_view(g,&before))return LID_MI_INVALID;
    if(g->ops.try_enter(g->ops.context,before.request_runner+0x18)!=1)return LID_MI_BUSY;
    if(g->ops.try_enter(g->ops.context,before.save_runner+0x18)!=1){g->ops.leave(g->ops.context,before.request_runner+0x18);return LID_MI_BUSY;}
    int valid=lid_mi_view(g,&after)&&lid_mi_same(&before,&after);
    int idle=valid?lid_mi_idle(g,&after):LID_MI_INVALID;
    if(idle!=LID_MI_HELD){g->ops.leave(g->ops.context,before.save_runner+0x18);g->ops.leave(g->ops.context,before.request_runner+0x18);return idle;}
    g->view=after;g->held=1;return LID_MI_HELD;
}
/* Suitable as the installer's quiescent callback when its ops.context binds
   THIS guard. Same thread and current idle native view rechecked every call.
   The host must never release these locks before activate/restore completes. */
static int lid_momoka_install_held(const LidMomokaInstallGuard*g){
    LidMomokaInstallView current;return g&&g->initialized&&g->held&&
      g->ops.thread(g->ops.context)==g->writer&&lid_mi_view(g,&current)&&
      lid_mi_same(&g->view,&current)&&lid_mi_idle(g,&current)==LID_MI_HELD;
}
static int lid_momoka_install_end(LidMomokaInstallGuard*g){
    if(!g||!g->initialized||!g->held||g->ops.thread(g->ops.context)!=g->writer)return 0;
    /* Release the exact acquired inline runner locks even if identity was
       rejected at activation. Do not touch a replaced GameInfo/manager. */
    g->ops.leave(g->ops.context,g->view.save_runner+0x18);g->ops.leave(g->ops.context,g->view.request_runner+0x18);g->held=0;return 1;
}
/* Concrete adapter to R228 installer: no external always-true quiescent flag.
   Platform ops and dispatcher active counter remain resident/immutable. */
typedef struct LidMomokaInstallBinding {
    unsigned initialized;LidMomokaInstallGuard guard;LidMomokaHookOps platform;
    const unsigned*active_handlers;
} LidMomokaInstallBinding;
static int lid_mib_read(void*c,uintptr_t p,void*out,size_t n){LidMomokaInstallBinding*b=c;return b->platform.read(b->platform.context,p,out,n);}
static int lid_mib_quiet(void*c){LidMomokaInstallBinding*b=c;return b&&b->initialized&&
    b->active_handlers&&!__atomic_load_n(b->active_handlers,__ATOMIC_ACQUIRE)&&lid_momoka_install_held(&b->guard);}
static void*lid_mib_alloc(void*c,uintptr_t p,size_t n){LidMomokaInstallBinding*b=c;
    return b->guard.held?0:b->platform.near_alloc(b->platform.context,p,n);}
static int lid_mib_protect(void*c,void*p,size_t n,unsigned protection,unsigned*old){LidMomokaInstallBinding*b=c;
    return b->platform.protect(b->platform.context,p,n,protection,old);}
static int lid_mib_flush(void*c,void*p,size_t n){LidMomokaInstallBinding*b=c;return b->platform.flush(b->platform.context,p,n);}
static int lid_mib_executable(void*c,uintptr_t p){LidMomokaInstallBinding*b=c;return b->platform.executable(b->platform.context,p);}
/* Publish output only on success. near_alloc is never invoked inside held
   engine locks. Host uses this ops result for hooks_prepare BEFORE activation. */
static int lid_momoka_install_bind(LidMomokaInstallBinding*b,uintptr_t base,int exact_sha,
    uint32_t writer,const LidMomokaInstallOps*native,const LidMomokaHookOps*platform,
    const unsigned*active_handlers,LidMomokaHookOps*out){
    if(!b||b->initialized||!out||!platform||!platform->read||!platform->near_alloc||
       !platform->protect||!platform->flush||!platform->executable||!active_handlers||
       !lid_momoka_install_prepare(&b->guard,base,exact_sha,writer,native))return 0;
    b->platform=*platform;b->active_handlers=active_handlers;b->initialized=1;
    *out=(LidMomokaHookOps){b,lid_mib_read,lid_mib_quiet,lid_mib_alloc,lid_mib_protect,lid_mib_flush,lid_mib_executable};return 1;
}
/* No retry loop or cross-tick lease. INVALID on hook error, BUSY only before
   mutation. Partial install rollback occurs while both locks are still held.
   A failed hook set may be retired/poisoned; never retry it as a fresh set. */
static int lid_momoka_install_activate(LidMomokaInstallBinding*b,LidMomokaHooks*h){
    if(!b||!b->initialized||!h||h->ops.context!=b||h->ops.quiescent!=lid_mib_quiet||
       h->base!=b->guard.base||h->state!=LID_MH_PREPARED)return LID_MI_INVALID;
    int admitted=lid_momoka_install_begin(&b->guard);if(admitted!=LID_MI_HELD)return admitted;
    int activated=lid_momoka_hooks_activate(h);int released=lid_momoka_install_end(&b->guard);
    return activated&&released?LID_MI_HELD:LID_MI_INVALID;
}
#endif
