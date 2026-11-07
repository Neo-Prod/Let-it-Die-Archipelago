#ifndef LID_RELOAD_BOUNDARY_H
#define LID_RELOAD_BOUNDARY_H
#include "reload_save_gate.h"
/* Native binding contract. Not active until all three hooks and their exact
   profiles are installed. No file IO or engine addresses are hidden here.
   Request ownership must serialize calls; SaveGate alone is atomic. */
typedef unsigned char LidBoundaryBool;
typedef struct LidBoundaryArgs {
    void *self,*response,*request_json,*soul;
    const unsigned short *entry_item;
    int *use_elevator;
    const unsigned short *debug_template;
} LidBoundaryArgs;
typedef struct LidBoundaryOps {
    void *context;
    /* Includes fresh identity/profile checks. Must not mutate engine state. */
    int (*validate)(void*,const LidBoundaryArgs*);
    /* Short runner reservation. Never retained during any engine callback. */
    int (*reserve)(void*);
    void (*release)(void*);
    LidBoundaryBool (*load)(void*);
    /* Exact ApplyRoot observation AND persistent soul/bodyuser readback. */
    int (*verify_applied)(void*);
    LidBoundaryBool (*project)(void*,void*);
    int (*verify_projected)(void*,void*);
    LidBoundaryBool (*original)(void*,void*,void*,void*,const unsigned short*,int*,const unsigned short*);
} LidBoundaryOps;
enum {LID_BOUNDARY_UNBOUND=0,LID_BOUNDARY_ARMED,LID_BOUNDARY_BUSY,
      LID_BOUNDARY_WAIT_VIEWS,LID_BOUNDARY_READY,LID_BOUNDARY_FAILED};
enum {LID_BOUNDARY_OK=0,LID_BOUNDARY_BINDING,LID_BOUNDARY_IDENTITY,
      LID_BOUNDARY_RESERVATION,LID_BOUNDARY_LOAD,LID_BOUNDARY_APPLY,
      LID_BOUNDARY_PROJECT,LID_BOUNDARY_READBACK,LID_BOUNDARY_CORE,
      LID_BOUNDARY_REENTRANT};
typedef struct LidReloadBoundary {
    LidReloadSaveGate save;
    unsigned stage,error,views,sync;
    unsigned long long epoch,request;
} LidReloadBoundary;
static void lid_boundary_fail(LidReloadBoundary*s,unsigned error){
    if(!s)return;
    s->error=error;s->stage=LID_BOUNDARY_FAILED;
    lid_reload_save_fail(&s->save);
}
static int lid_boundary_bind(LidReloadBoundary*s,unsigned long long epoch,
                             int save_hook,int apply_hook,int core_hook){
    if(!s||s->stage!=LID_BOUNDARY_UNBOUND||!epoch||save_hook!=1||apply_hook!=1||core_hook!=1)return 0;
    /* Caller installs hooks before offering the handoff. A rejected bind must
       not be advertised as a protected session. */
    if(!lid_reload_save_arm(&s->save))return 0;
    s->epoch=epoch;s->stage=LID_BOUNDARY_ARMED;return 1;
}
static LidBoundaryBool lid_boundary_forward(const LidBoundaryOps*o,const LidBoundaryArgs*a){
    return o->original(a->self,a->response,a->request_json,a->soul,a->entry_item,a->use_elevator,a->debug_template);
}
static int lid_boundary_still_busy(const LidReloadBoundary*s){return s->stage==LID_BOUNDARY_BUSY;}
static LidBoundaryBool lid_boundary_call(LidReloadBoundary*s,const LidBoundaryOps*o,
    const LidBoundaryArgs*a,int ap,unsigned long long epoch,unsigned long long request){
    int reserved;LidBoundaryBool result;
    if(!o||!o->original||!a)return 0;
    if(!ap)return lid_boundary_forward(o,a);
    if(!s||s->stage==LID_BOUNDARY_UNBOUND||s->stage==LID_BOUNDARY_FAILED)return 0;
    if(!epoch||epoch!=s->epoch||!request){lid_boundary_fail(s,LID_BOUNDARY_IDENTITY);return 0;}
    if(s->stage==LID_BOUNDARY_BUSY){lid_boundary_fail(s,LID_BOUNDARY_REENTRANT);return 0;}
    if(s->stage==LID_BOUNDARY_READY)return lid_boundary_forward(o,a);
    if(s->stage!=LID_BOUNDARY_ARMED)return 0;
    if(!a->soul||!o->validate||!o->reserve||!o->release||!o->load||
       !o->verify_applied||!o->project||!o->verify_projected){
        lid_boundary_fail(s,LID_BOUNDARY_BINDING);return 0;
    }
    s->request=request;s->stage=LID_BOUNDARY_BUSY;
    if(o->validate(o->context,a)!=1){lid_boundary_fail(s,LID_BOUNDARY_IDENTITY);return 0;}
    if(!lid_boundary_still_busy(s))return 0;
    reserved=o->reserve(o->context)==1;
    if(!reserved){lid_boundary_fail(s,LID_BOUNDARY_RESERVATION);return 0;}
    /* Even a reentrant failure during reserve must release its acquired lock. */
    result=(LidBoundaryBool)(lid_boundary_still_busy(s)&&lid_reload_save_begin_load(&s->save));
    o->release(o->context);
    if(!lid_boundary_still_busy(s))return 0;
    if(!result){lid_boundary_fail(s,LID_BOUNDARY_RESERVATION);return 0;}
    if(o->load(o->context)!=1){lid_boundary_fail(s,LID_BOUNDARY_LOAD);return 0;}
    if(!lid_boundary_still_busy(s))return 0;
    if(o->verify_applied(o->context)!=1){lid_boundary_fail(s,LID_BOUNDARY_APPLY);return 0;}
    if(!lid_boundary_still_busy(s))return 0;
    if(o->project(o->context,a->soul)!=1){lid_boundary_fail(s,LID_BOUNDARY_PROJECT);return 0;}
    if(!lid_boundary_still_busy(s))return 0;
    if(o->verify_projected(o->context,a->soul)!=1){lid_boundary_fail(s,LID_BOUNDARY_READBACK);return 0;}
    if(!lid_boundary_still_busy(s))return 0;
    result=lid_boundary_forward(o,a);
    if(!lid_boundary_still_busy(s))return 0;
    if(result!=1){lid_boundary_fail(s,LID_BOUNDARY_CORE);return 0;}
    if(!lid_reload_save_accept(&s->save)){lid_boundary_fail(s,LID_BOUNDARY_CORE);return 0;}
    s->stage=LID_BOUNDARY_WAIT_VIEWS;return 1;
}
/* Call only with correlated, checked evidence. Neither notification is an ACK.
   sync means guard drained AND own neutral-save completion verified. */
static int lid_boundary_observe(LidReloadBoundary*s,unsigned long long epoch,
    unsigned long long request,int views,int sync){
    if(!s||s->stage!=LID_BOUNDARY_WAIT_VIEWS||s->epoch!=epoch||s->request!=request||
       (views!=0&&views!=1)||(sync!=0&&sync!=1))return 0;
    s->views|=(unsigned)views;s->sync|=(unsigned)sync;
    if(s->views&&s->sync)s->stage=LID_BOUNDARY_READY;
    return 1;
}
static int lid_boundary_delivery_ready(const LidReloadBoundary*s){return s&&s->stage==LID_BOUNDARY_READY;}
#endif
