#ifndef LID_PLAYER_STAT_SCOPE_H
#define LID_PLAYER_STAT_SCOPE_H
#include "player_stat_policy.h"

/* One dedicated TLS slot, owned for the resident lifetime of the DLL.
 * Never retain a borrowed UObject, FFrame, native record or stack pointer.
 * This module is not a native identity adapter and installs no hooks.
 */
enum { LID_PS_SCOPE_FRAMES=64 };
typedef struct LidPScopeOps {
    void *context;
    void *(*get)(void *context,unsigned slot);
    int (*set)(void *context,unsigned slot,void *value);
    unsigned (*thread)(void *context);
    unsigned (*error_get)(void *context);
    void (*error_set)(void *context,unsigned error);
} LidPScopeOps;
typedef struct LidPScopeFrame {
    struct LidPScopeFrame *previous;
    uint64_t serial;
    unsigned owner,live,owned,enemy;
    char key[21];
} LidPScopeFrame;
typedef struct LidPScope {
    LidPScopeOps ops;
    unsigned slot,ready,fault;
    uint64_t serial;
    LidPScopeFrame frames[LID_PS_SCOPE_FRAMES];
} LidPScope;
typedef struct LidPScopeToken {
    LidPScopeFrame *frame;
    uint64_t serial;
    unsigned owner,entered;
} LidPScopeToken;

static int lid_pscope_key(const char *key){
    unsigned i;
    if(!key)return 0;
    for(i=0;i<20;i++)if(!((key[i]>='0'&&key[i]<='9')||(key[i]>='a'&&key[i]<='f')))return 0;
    return key[20]==0;
}
static int lid_pscope_same_key(const char *a,const char *b){
    unsigned i;
    if(!lid_pscope_key(a)||!lid_pscope_key(b))return 0;
    for(i=0;i<21;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static int lid_pscope_init(LidPScope *scope,unsigned slot,const LidPScopeOps *ops){
    if(!scope||scope->ready||slot==~0u||!ops||!ops->get||!ops->set||
       !ops->thread||!ops->error_get||!ops->error_set)return 0;
    scope->ops=*ops;scope->slot=slot;scope->ready=1;return 1;
}
/* Match a resident address BEFORE reading through it. TLS is not an arbitrary
 * pointer trust boundary. Active frames cannot be recycled by another thread.
 */
static LidPScopeFrame *lid_pscope_frame(LidPScope *scope,void *value,unsigned owner){
    if(!value)return 0;
    for(unsigned i=0;i<LID_PS_SCOPE_FRAMES;i++)if(value==&scope->frames[i]){
        LidPScopeFrame *frame=&scope->frames[i];
        if(__atomic_load_n(&frame->live,__ATOMIC_ACQUIRE)&&
           __atomic_load_n(&frame->owner,__ATOMIC_ACQUIRE)==owner)return frame;
        break;
    }
    return 0;
}
/* Call for ALL origins, including unknown and enemy. Unknown masks an outer
 * AP context; an enemy ancestor vetoes ordinary nested UI/service origins.
 * A verified current-player OUTPUT is a new subject and can interrupt that
 * origin veto. An enemy recalculation nested in that subject still vetoes AP.
 */
static int lid_pscope_enter_impl(LidPScope *scope,const char *key,const LidPSIdentity *identity,
                                 int verified_roster,LidPScopeToken *token){
    LidPScopeFrame *parent=0,*frame=0;void *value;unsigned owner,error;uint64_t serial;
    if(!token)return 0;*token=(LidPScopeToken){0};
    if(!scope||!scope->ready||__atomic_load_n(&scope->fault,__ATOMIC_ACQUIRE))return 0;
    error=scope->ops.error_get(scope->ops.context);owner=scope->ops.thread(scope->ops.context);
    if(!owner)goto refused;
    value=scope->ops.get(scope->ops.context,scope->slot);
    if(value){parent=lid_pscope_frame(scope,value,owner);if(!parent)goto poison;}
    for(unsigned i=0;i<LID_PS_SCOPE_FRAMES;i++){
        unsigned empty=0;
        if(__atomic_compare_exchange_n(&scope->frames[i].owner,&empty,owner,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
            frame=&scope->frames[i];break;
        }
    }
    if(!frame)goto poison;
    serial=__atomic_add_fetch(&scope->serial,1,__ATOMIC_ACQ_REL);
    if(!serial)goto poison_frame;
    frame->previous=parent;frame->serial=serial;
    int player_output=identity&&identity->role==LID_PS_PLAYER_OUTPUT&&lid_pscope_key(key)&&lid_ps_owned(identity);
    frame->enemy=(parent&&parent->enemy&&!player_output)||(identity&&
        (identity->role==LID_PS_SCREAMER||identity->role==LID_PS_HATER));
    frame->owned=!frame->enemy&&lid_pscope_key(key)&&(lid_ps_owned(identity)||verified_roster==1);
    for(unsigned i=0;i<21;i++)frame->key[i]=frame->owned?key[i]:0;
    __atomic_store_n(&frame->live,1,__ATOMIC_RELEASE);
    if(scope->ops.set(scope->ops.context,scope->slot,frame)!=1)goto poison_frame;
    *token=(LidPScopeToken){frame,serial,owner,1};
    scope->ops.error_set(scope->ops.context,error);return 1;
poison_frame:
    __atomic_store_n(&frame->live,0,__ATOMIC_RELEASE);
    /* A failed TLS set has uncertain publication. Keep the resident frame
       reserved and poison the policy instead of reusing a possibly held slot. */
poison:
    __atomic_store_n(&scope->fault,1,__ATOMIC_RELEASE);
refused:
    scope->ops.error_set(scope->ops.context,error);return 0;
}
static int lid_pscope_enter(LidPScope *scope,const char *key,const LidPSIdentity *identity,
                            LidPScopeToken *token){
    return lid_pscope_enter_impl(scope,key,identity,0,token);
}
static int lid_pscope_enter_roster(LidPScope *scope,const char *key,const LidPSRosterIdentity *identity,
                                   LidPScopeToken *token){
    return lid_pscope_enter_impl(scope,key,0,lid_ps_roster_owned(identity),token);
}
static int lid_pscope_owned(LidPScope *scope,const char *key){
    LidPScopeFrame *frame;unsigned error,owner;int result=0;
    if(!scope||!scope->ready||__atomic_load_n(&scope->fault,__ATOMIC_ACQUIRE))return 0;
    error=scope->ops.error_get(scope->ops.context);owner=scope->ops.thread(scope->ops.context);
    if(!owner)goto done;
    frame=lid_pscope_frame(scope,scope->ops.get(scope->ops.context,scope->slot),owner);
    if(frame&&frame->owned&&!frame->enemy&&lid_pscope_same_key(frame->key,key))result=1;
done:
    scope->ops.error_set(scope->ops.context,error);return result;
}
static int lid_pscope_leave(LidPScope *scope,LidPScopeToken *token){
    LidPScopeFrame *frame,*current;unsigned error,owner;int result=0;
    if(!scope||!scope->ready||!token||!token->entered)return 0;
    error=scope->ops.error_get(scope->ops.context);owner=scope->ops.thread(scope->ops.context);
    frame=token->frame;
    if(!owner)goto poison;
    current=lid_pscope_frame(scope,scope->ops.get(scope->ops.context,scope->slot),owner);
    if(owner!=token->owner||current!=frame||!current||current->serial!=token->serial)goto poison;
    if(frame->previous&&!lid_pscope_frame(scope,frame->previous,owner))goto poison;
    if(scope->ops.set(scope->ops.context,scope->slot,frame->previous)!=1){
        __atomic_store_n(&frame->live,0,__ATOMIC_RELEASE);goto poison;
    }
    __atomic_store_n(&frame->live,0,__ATOMIC_RELEASE);
    frame->owned=frame->enemy=0;frame->previous=0;frame->serial=0;
    __atomic_store_n(&frame->owner,0,__ATOMIC_RELEASE);token->entered=0;result=1;
    goto done;
poison:
    __atomic_store_n(&scope->fault,1,__ATOMIC_RELEASE);
done:
    scope->ops.error_set(scope->ops.context,error);return result;
}
#endif
