#ifndef LID_SHOP_AP_SAVE_ORIGIN_H
#define LID_SHOP_AP_SAVE_ORIGIN_H
#include "winlite.h"
#include "progression_save_job.h"
#include "shop_save_origin.h"
/* Read-only observer of an ALREADY bound resident AP save job. Not a scheduler,
   lock, permission to write, or durability proof. No pointers copied to output.
   Callback size is trusted metadata from THIS DLL build's COFF function section,
   never a YAML offset/guess. Host must bind its actual known job and native
   callback; all source objects remain owned through current Save invocation. */
typedef struct LidShopApSaveBinding {
    const LidProgressSaveJob*job;LidProgressSaveOps ops;
    uintptr_t callback_begin,callback_end;unsigned ready;
} LidShopApSaveBinding;
typedef struct LidShopApSaveOrigin {uint64_t batch;unsigned ticket;uint32_t thread;} LidShopApSaveOrigin;
static int lid_shop_ap_save_bind(LidShopApSaveBinding*b,const LidProgressSaveJob*job,unsigned verified_callback_size){
    if(!b||b->ready||!job||!verified_callback_size||verified_callback_size>65536||
       !job->ops.allocate||!job->ops.release||!job->ops.mark_dirty||!job->ops.save||!job->ops.start)return 0;
    uintptr_t begin=(uintptr_t)job->ops.save;
    if(!begin||begin>UINTPTR_MAX-verified_callback_size)return 0;
    b->job=job;b->ops=job->ops;b->callback_begin=begin;b->callback_end=begin+verified_callback_size;b->ready=1;return 1;
}
static int lid_shop_ap_save_origin(const LidShopApSaveBinding*b,const LidShopSaveOriginOps*o,
    uintptr_t base,int exact_sha,uint32_t game_writer,uintptr_t runner,uintptr_t optional_text,
    uintptr_t caller,LidShopApSaveOrigin*out){
    uintptr_t vt,slot,callable,again;uint32_t tid;LidProgressSaveCallable current;
    LidProgressSaveJob job;unsigned ticket;int completion;
    if(!b||!b->ready||!o||!o->copy||!o->thread||!out||!base||base>UINTPTR_MAX-0xf8a5508||
       exact_sha!=1||!game_writer||optional_text||runner!=base+0xf8a5490||
       caller<b->callback_begin||caller>=b->callback_end||!(tid=o->thread(o->context))||tid==game_writer||
       !lid_so_read(o,runner,0,&vt,8)||vt!=base+0x212e0e8||
       !lid_so_read(o,vt,8,&slot,8)||slot!=base+0x11bd630||
       !lid_so_read(o,runner,0x64,&callable,8)||!lid_so_read(o,callable,0,&current,sizeof(current))||
       current.vtable!=&lid_progress_save_vtable||current.job!=b->job||
       !lid_so_read(o,(uintptr_t)b->job,0,&job,sizeof(job))||!job.active||job.completion!=-1||
       !job.batch||!job.ticket||current.ticket!=job.ticket||job.runner!=(void*)runner||
       job.ops.allocate!=b->ops.allocate||job.ops.release!=b->ops.release||job.ops.mark_dirty!=b->ops.mark_dirty||
       job.ops.save!=b->ops.save||job.ops.start!=b->ops.start)return 0;
    /* reserved in the old callable is not initialized by save_begin; DO NOT
       invent a required zero value for it. Never retain borrowed callable. */
    if(!lid_so_read(o,runner,0x64,&again,8)||again!=callable||
       !lid_so_read(o,(uintptr_t)b->job,__builtin_offsetof(LidProgressSaveJob,ticket),&ticket,sizeof(ticket))||ticket!=job.ticket||
       !lid_so_read(o,(uintptr_t)b->job,__builtin_offsetof(LidProgressSaveJob,completion),&completion,sizeof(completion))||completion!=-1)return 0;
    *out=(LidShopApSaveOrigin){job.batch,job.ticket,tid};return 1;
}
#endif
