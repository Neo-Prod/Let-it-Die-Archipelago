#ifndef LID_PROGRESSION_TRANSACTION_H
#define LID_PROGRESSION_TRANSACTION_H
#include "progression_extra.h"
/* Closed UI-thread orchestration of the tested host/lease/apply/save helpers.
   No platform API or game address. All bridge I/O occurs OUTSIDE engine locks.
   Disabled until the complete runtime restore/Rage policy is ready. */
enum {LID_TX_IDLE=0,LID_TX_SAVING,LID_TX_RETRY,LID_TX_CLOSE_OK,LID_TX_CLOSE_CANCEL,
      LID_TX_CLOSE_FAILED,LID_TX_ACK,LID_TX_STOPPED,LID_TX_LOST,LID_TX_CLOSE_REFRESH,LID_TX_ARMING,LID_TX_ARMED};
enum {LID_TX_DISABLED=0,LID_TX_WAIT=1,LID_TX_STARTED=2,LID_TX_WAIT_SAVE=3,
      LID_TX_WAIT_RETRY=4,LID_TX_WAIT_ACK=5,LID_TX_SAVED=6,LID_TX_REFUSED=7,
      LID_TX_REFRESHED=8,LID_TX_REFRESH_REFUSED=9,LID_TX_ENTRY_IDLE=10,LID_TX_ENTRY_DONE=11,
      LID_TX_FAILED=-1,LID_TX_GENERATION_LOST=-2};
typedef struct LidCharacterTransactionOps {
    void* context;
    int (*gate)(void*,LidProgressGateView*);
    int (*character)(void*,LidCharacterView*);
} LidCharacterTransactionOps;
typedef struct LidCharacterTransaction {
    LidCharacterHost* host;LidProgressLease* lease;LidProgressSaveJob* job;
    LidCharacterApplication* application;const LidCharacterConfig* config;
    const LidProgressGateOps* gate_ops;const LidCharacterApplyOps* apply_ops;
    void* entry_vtable;LidCharacterTransactionOps ops;
    unsigned ticket,retry_delay,retry_wait;
    DWORD owner_thread;
    unsigned refresh_wait;
    int refresh_result;
    int initialized,phase,transport_status;
    unsigned entry_serial;
    int entry_open,entry_pinned;
    const LidProgressExtraOps* extra;
    void* arm_context;int (*arm_before_commit)(void*);int (*validate_armed)(void*);
    void (*trace_event)(unsigned,int,int);
    unsigned refresh_reason;
} LidCharacterTransaction;
/* Preserve the exact expression type, single evaluation and short circuit.
   Optional diagnostics only; callback never determines the return value. */
#define LID_TX_TIMED(tx,id,expression) __extension__ ({ \
    LidCharacterTransaction*_lid_trace_tx=(tx); \
    if(_lid_trace_tx->trace_event)_lid_trace_tx->trace_event(id,1,0); \
    __auto_type _lid_trace_value=(expression); \
    if(_lid_trace_tx->trace_event)_lid_trace_tx->trace_event(id,0,(int)_lid_trace_value); \
    _lid_trace_value; })
static const char*lid_transaction_refresh_reason(unsigned reason){
    static const char*const names[]={"none","lease","extra_refresh","character_view","config_targets",
        "model_levels_readable","model_bag_readable","model_levels_mismatch","model_bag_size",
        "cache_levels_readable","cache_bag_readable","pawn_readable","apply_prepare","apply_commit"};
    return reason<sizeof(names)/sizeof(names[0])?names[reason]:"unknown";
}
static int lid_transaction_init(LidCharacterTransaction* tx,LidCharacterHost* host,
                                LidProgressLease* lease,LidProgressSaveJob* job,
                                LidCharacterApplication* application,const LidCharacterConfig* config,
                                const LidProgressGateOps* gate_ops,const LidCharacterApplyOps* apply_ops,
                                void* entry_vtable,const LidCharacterTransactionOps* ops,unsigned retry_delay){
    if(!tx||tx->initialized||!host||!host->initialized||host->active||!lease||lease->active||lease->window||
       !job||job->active||!application||application->started||application->prepared||application->applied||
       !lid_config_valid(config)||!gate_ops||!gate_ops->readable||!gate_ops->try_enter||!gate_ops->leave||
       !gate_ops->thread_id||!apply_ops||!entry_vtable||!ops||!ops->gate||!ops->character)return 0;
    tx->host=host;tx->lease=lease;tx->job=job;tx->application=application;tx->config=config;
    tx->gate_ops=gate_ops;tx->apply_ops=apply_ops;tx->entry_vtable=entry_vtable;tx->ops=*ops;
    tx->retry_delay=retry_delay;tx->retry_wait=tx->ticket=0;tx->owner_thread=0;
    tx->refresh_wait=0;tx->refresh_result=0;
    tx->entry_serial=0;tx->entry_open=tx->entry_pinned=0;
    tx->arm_context=0;tx->arm_before_commit=0;tx->validate_armed=0;
    tx->trace_event=0;tx->refresh_reason=0;
    tx->extra=0;tx->transport_status=0;tx->phase=LID_TX_IDLE;tx->initialized=1;return 1;
}
/* Attach only before the first lifecycle entry/offer, never mid-transaction. */
static int lid_transaction_attach_extra(LidCharacterTransaction* tx,const LidProgressExtraOps* extra){
    if(!tx||!tx->initialized||tx->extra||!extra||!extra->prepare||!extra->commit||!extra->readback||
       !extra->refresh||!extra->publish_saved||!extra->reset||tx->entry_serial||tx->entry_open||
       tx->phase!=LID_TX_IDLE||tx->lease->active||tx->host->active||tx->host->offer_published)return 0;
    tx->extra=extra;return 1;
}
/* Only a real, validated lifecycle notification may call this. Receipts never
   open entries. One accepted TARGET will be pinned for the entire entry. */
static int lid_transaction_open_entry(LidCharacterTransaction* tx,unsigned serial){
    if(!tx||!tx->initialized||!serial||serial<=tx->entry_serial||tx->entry_open||
       tx->phase!=LID_TX_IDLE||tx->lease->active||tx->lease->window||tx->job->active||
       tx->host->active||tx->host->poisoned||tx->application->started)return 0;
    if(tx->extra)tx->extra->reset(tx->extra->context);
    tx->entry_serial=serial;tx->entry_open=1;tx->entry_pinned=0;
    tx->retry_wait=tx->refresh_wait=0;return 1;
}
static int lid_transaction_cancel_entry(LidCharacterTransaction* tx){
    if(!tx||!tx->initialized||!tx->entry_open||tx->phase!=LID_TX_IDLE||
       tx->lease->active||tx->lease->window||tx->job->active||tx->host->active||
       tx->application->started)return 0;
    tx->entry_open=tx->entry_pinned=0;
    if(tx->extra)tx->extra->reset(tx->extra->context);
    return 1;
}
static int lid_transaction_leave(LidCharacterTransaction* tx,int result){
    if(tx->lease->window&&!lid_progress_lease_leave_window(tx->lease)){
        tx->host->poisoned=1;tx->phase=LID_TX_LOST;return LID_TX_GENERATION_LOST;
    }
    return result;
}
static int lid_transaction_close(LidCharacterTransaction* tx){
    int mode=tx->phase,result;
    if(mode==LID_TX_CLOSE_FAILED&&tx->job->active){
        result=LID_TX_TIMED(tx,13,lid_progress_save_finish(tx->job,tx->ticket,tx->host->frozen.sequence,
                                       lid_progress_lease_save_active(tx->lease)));
        if(result!=1&&result!=2)return lid_transaction_leave(tx,LID_TX_FAILED);
    }
    if((mode==LID_TX_CLOSE_OK||(mode==LID_TX_CLOSE_REFRESH&&tx->application->applied))&&
       !LID_TX_TIMED(tx,10,lid_character_apply_readback(tx->application))){
        tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;mode=LID_TX_CLOSE_FAILED;
    }
    if(mode==LID_TX_CLOSE_OK&&tx->extra&&LID_TX_TIMED(tx,11,tx->extra->readback(tx->extra->context))!=1){
        tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;mode=LID_TX_CLOSE_FAILED;
    }
    if(!LID_TX_TIMED(tx,21,lid_progress_lease_close(tx->lease)))return lid_transaction_leave(tx,LID_TX_WAIT);
    if(mode==LID_TX_CLOSE_OK){
        if(!LID_TX_TIMED(tx,16,lid_host_prove(tx->host,tx->host->frozen.sequence,tx->host->frozen.generation,1))){
            tx->host->poisoned=1;tx->phase=LID_TX_STOPPED;return LID_TX_FAILED;
        }
        tx->phase=LID_TX_ACK;return LID_TX_WAIT_ACK;
    }
    if(mode==LID_TX_CLOSE_CANCEL){
        if(tx->application->started||tx->job->active||
           (tx->host->active&&!lid_host_cancel_unapplied(tx->host,1))){
            tx->host->poisoned=1;tx->phase=LID_TX_STOPPED;return LID_TX_FAILED;
        }
        lid_bag_zero(tx->application,sizeof(*tx->application));tx->phase=LID_TX_IDLE;
        if(tx->extra)tx->extra->reset(tx->extra->context);
        tx->entry_open=tx->entry_pinned=0;tx->retry_wait=0;return LID_TX_REFUSED;
    }
    if(mode==LID_TX_CLOSE_REFRESH){
        lid_bag_zero(tx->application,sizeof(*tx->application));tx->phase=LID_TX_IDLE;
        if(tx->extra)tx->extra->reset(tx->extra->context);
        tx->refresh_wait=0;tx->entry_open=tx->entry_pinned=0;return tx->refresh_result;
    }
    tx->host->poisoned=1;tx->phase=LID_TX_STOPPED;return LID_TX_FAILED;
}
/* Reconcile reloaded caches ONLY if persistent models already match the saved
   receipt. Never repair persistent drift by silently replaying old receipts. */
static int lid_transaction_refresh(LidCharacterTransaction* tx,const LidProgressGateView* gate){
    LidCharacterView view;int absolute[9],levels[10],i,j,result,same=1;const int* existing;
    tx->refresh_reason=1;
    result=LID_TX_TIMED(tx,3,lid_progress_lease_begin(tx->lease,gate,tx->gate_ops));
    if(result!=1){
        if(result<0){tx->entry_open=tx->entry_pinned=0;return LID_TX_REFRESH_REFUSED;}
        return LID_TX_WAIT;
    }
    lid_bag_zero(tx->application,sizeof(*tx->application));tx->phase=LID_TX_CLOSE_REFRESH;
    tx->refresh_result=LID_TX_REFRESH_REFUSED;
    tx->refresh_reason=2;
    if(tx->extra&&LID_TX_TIMED(tx,14,tx->extra->refresh(tx->extra->context,&tx->host->peer.last_target))!=1)return lid_transaction_close(tx);
    tx->refresh_reason=3;
    if(!LID_TX_TIMED(tx,4,tx->ops.character(tx->ops.context,&view)))return lid_transaction_close(tx);
    tx->refresh_reason=4;
    if(!lid_config_targets(tx->config,tx->host->peer.last_target.counts,absolute,levels))return lid_transaction_close(tx);
    tx->refresh_reason=5;
    if(!tx->apply_ops->readable(view.models.levels,40))return lid_transaction_close(tx);
    tx->refresh_reason=6;
    if(!tx->apply_ops->readable(view.models.slots,sizeof(LidBagArray)))return lid_transaction_close(tx);
    tx->refresh_reason=7;
    for(i=0;i<10;i++)if(view.models.levels[i]!=levels[i])return lid_transaction_close(tx);
    tx->refresh_reason=8;
    if(view.models.slots->num!=absolute[6])return lid_transaction_close(tx);
    for(i=0;i<2;i++){
        existing=(int*)((i?view.body:view.chara)+0x14c);
        tx->refresh_reason=9;
        if(!tx->apply_ops->readable(existing,40))return lid_transaction_close(tx);
        for(j=0;j<10;j++)if(existing[j]!=levels[j])same=0;
        tx->refresh_reason=10;
        if(!tx->apply_ops->readable((i?view.body:view.chara)+0x18c,sizeof(LidBagArray)))return lid_transaction_close(tx);
        if(((LidBagArray*)((i?view.body:view.chara)+0x18c))->num!=absolute[6])same=0;
    }
    tx->refresh_reason=11;
    if(!tx->apply_ops->readable(view.common_grade,4)||!tx->apply_ops->readable(view.pawn_slots,sizeof(LidBagArray)))return lid_transaction_close(tx);
    if(*view.common_grade!=6||view.pawn_slots->num!=absolute[6])same=0;
    if(same){tx->refresh_reason=0;tx->refresh_result=LID_TX_ENTRY_DONE;return lid_transaction_close(tx);}
    tx->refresh_reason=12;
    if(!LID_TX_TIMED(tx,5,lid_character_apply_prepare(tx->application,&view,tx->config,tx->host->peer.last_target.counts,
       tx->apply_ops,tx->entry_vtable))){lid_character_apply_abort(tx->application);return lid_transaction_close(tx);}
    tx->refresh_reason=13;
    result=LID_TX_TIMED(tx,8,lid_character_apply_commit(tx->application));
    if(result!=1){
        lid_character_apply_abort(tx->application);
        if(result<0||tx->application->started){tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;}
        return lid_transaction_close(tx);
    }
    tx->refresh_reason=0;tx->refresh_result=LID_TX_REFRESHED;return lid_transaction_close(tx);
}
static int lid_transaction_start_save(LidCharacterTransaction* tx){
    if(!LID_TX_TIMED(tx,10,lid_character_apply_readback(tx->application))||
       (tx->extra&&LID_TX_TIMED(tx,11,tx->extra->readback(tx->extra->context))!=1)){
        tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;return lid_transaction_close(tx);
    }
    if(lid_progress_lease_save_active(tx->lease))return lid_transaction_leave(tx,LID_TX_WAIT_SAVE);
    tx->ticket=LID_TX_TIMED(tx,12,lid_progress_save_begin(tx->job,tx->lease->view.save_runner,tx->host->frozen.sequence));
    if(!tx->ticket){
        tx->phase=LID_TX_RETRY;tx->retry_wait=tx->retry_delay;
        return lid_transaction_leave(tx,LID_TX_WAIT_RETRY);
    }
    tx->phase=LID_TX_SAVING;return lid_transaction_leave(tx,LID_TX_STARTED);
}
static int lid_transaction_commit_prepared(LidCharacterTransaction* tx){
    int result;
    if(tx->phase==LID_TX_ARMED&&tx->validate_armed&&LID_TX_TIMED(tx,7,tx->validate_armed(tx->arm_context))!=1){
        lid_character_apply_abort(tx->application);tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;
        return lid_transaction_close(tx);
    }
    result=LID_TX_TIMED(tx,8,lid_character_apply_commit(tx->application));
    if(result!=1){
        lid_character_apply_abort(tx->application);
        if(result<0||tx->application->started){tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;}
        else tx->phase=LID_TX_CLOSE_CANCEL;
        return lid_transaction_close(tx);
    }
    if(tx->extra&&LID_TX_TIMED(tx,9,tx->extra->commit(tx->extra->context))!=1){
        tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;return lid_transaction_close(tx);
    }
    return lid_transaction_start_save(tx);
}
static int lid_transaction_tick(LidCharacterTransaction* tx,int enabled){
    LidProgressGateView gate;LidCharacterView character;LidWireMessage frozen;int result;DWORD tid,expected=0;
    if(!tx||!tx->initialized||enabled!=1)return LID_TX_DISABLED;
    if(tx->phase==LID_TX_IDLE&&!tx->entry_open)return LID_TX_ENTRY_IDLE;
    tid=tx->gate_ops->thread_id();if(!tid)return LID_TX_REFUSED;
    if(!__atomic_compare_exchange_n(&tx->owner_thread,&expected,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)&&expected!=tid)
        return LID_TX_REFUSED; /* Ignore other threads BEFORE protocol I/O/state. */
    if(tx->phase==LID_TX_LOST)return LID_TX_GENERATION_LOST;
    if(tx->phase==LID_TX_STOPPED)return LID_TX_FAILED;
    /* Entry invariant: the previous Tick released BOTH engine locks. */
    if(tx->lease->window){tx->host->poisoned=1;tx->phase=LID_TX_LOST;return LID_TX_GENERATION_LOST;}
    if(!tx->host->offer_published){
        if(!LID_TX_TIMED(tx,0,tx->ops.gate(tx->ops.context,&gate))||gate.game_started!=1||gate.in_hub!=1||gate.stage_ready!=1)return LID_TX_WAIT;
    }
    if(!tx->entry_pinned){
        tx->transport_status=LID_TX_TIMED(tx,1,lid_host_step(tx->host,1)); /* File I/O BEFORE any lock. */
        if(tx->transport_status==3||tx->transport_status==4)tx->entry_pinned=1;
    }
    if(tx->host->poisoned){
        if(!tx->lease->active){tx->phase=LID_TX_STOPPED;return LID_TX_FAILED;}
        tx->phase=LID_TX_CLOSE_FAILED;
    }
    /* File barrier occurs once, with the frozen batch retained, before any
       application commit. Both engine locks were released on previous Tick. */
    if(tx->phase==LID_TX_ARMING){
        if(!tx->arm_before_commit||LID_TX_TIMED(tx,19,tx->arm_before_commit(tx->arm_context))!=1){
            tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;
        }else tx->phase=LID_TX_ARMED;
    }
    if(tx->phase==LID_TX_ACK){
        if(tx->extra&&LID_TX_TIMED(tx,17,tx->extra->publish_saved(tx->extra->context,&tx->host->frozen))!=1)return LID_TX_WAIT_ACK;
        if(!LID_TX_TIMED(tx,18,lid_host_publish_saved(tx->host)))return LID_TX_WAIT_ACK;
        lid_bag_zero(tx->application,sizeof(*tx->application));tx->ticket=0;tx->phase=LID_TX_IDLE;tx->refresh_wait=0;
        if(tx->extra)tx->extra->reset(tx->extra->context);
        tx->entry_open=tx->entry_pinned=0;
        return LID_TX_SAVED;
    }
    if(tx->phase==LID_TX_IDLE){
        if(tx->retry_wait){tx->retry_wait--;return LID_TX_WAIT;}
        if(!tx->host->grant_published||!tx->entry_pinned)return LID_TX_WAIT;
        if(tx->job->active||tx->lease->active){tx->host->poisoned=1;tx->phase=LID_TX_LOST;return LID_TX_GENERATION_LOST;}
        if(!LID_TX_TIMED(tx,0,tx->ops.gate(tx->ops.context,&gate)))return LID_TX_WAIT;
        if(!gate.in_hub)tx->refresh_wait=0;
        if(tx->host->peer.last_target.sequence<=tx->host->acknowledged){
            if(!tx->host->acknowledged)return LID_TX_WAIT;
            return LID_TX_TIMED(tx,15,lid_transaction_refresh(tx,&gate));
        }
        result=LID_TX_TIMED(tx,3,lid_progress_lease_begin(tx->lease,&gate,tx->gate_ops));
        if(result!=1){
            if(result<0){tx->entry_open=tx->entry_pinned=0;return LID_TX_REFUSED;}
            return LID_TX_WAIT;
        }
        lid_bag_zero(tx->application,sizeof(*tx->application));
        if(!LID_TX_TIMED(tx,2,lid_host_freeze(tx->host,&frozen))||!LID_TX_TIMED(tx,4,tx->ops.character(tx->ops.context,&character))||
           !LID_TX_TIMED(tx,5,lid_character_apply_prepare(tx->application,&character,tx->config,frozen.counts,tx->apply_ops,tx->entry_vtable))||
           (tx->extra&&LID_TX_TIMED(tx,6,tx->extra->prepare(tx->extra->context,&frozen))!=1)){
            lid_character_apply_abort(tx->application);tx->phase=LID_TX_CLOSE_CANCEL;return lid_transaction_close(tx);
        }
        if(tx->arm_before_commit){tx->phase=LID_TX_ARMING;return lid_transaction_leave(tx,LID_TX_WAIT);}
        return lid_transaction_commit_prepared(tx);
    }
    /* Ongoing lease: compare a FRESH view before touching any old manager/lock. */
    if(!LID_TX_TIMED(tx,0,tx->ops.gate(tx->ops.context,&gate)))return LID_TX_WAIT;
    result=LID_TX_TIMED(tx,20,lid_progress_lease_reenter(tx->lease,&gate));
    if(result==0)return LID_TX_WAIT;
    if(result<0){tx->host->poisoned=1;tx->phase=LID_TX_LOST;return LID_TX_GENERATION_LOST;}
    if(tx->phase==LID_TX_ARMED)return lid_transaction_commit_prepared(tx);
    if(tx->phase==LID_TX_CLOSE_FAILED&&!tx->application->started&&tx->application->prepared)lid_character_apply_abort(tx->application);
    if(tx->phase==LID_TX_CLOSE_OK||tx->phase==LID_TX_CLOSE_CANCEL||tx->phase==LID_TX_CLOSE_FAILED||
       tx->phase==LID_TX_CLOSE_REFRESH)return lid_transaction_close(tx);
    if(tx->phase==LID_TX_RETRY){
        if(tx->retry_wait){tx->retry_wait--;return lid_transaction_leave(tx,LID_TX_WAIT_RETRY);}
        return lid_transaction_start_save(tx);
    }
    result=LID_TX_TIMED(tx,13,lid_progress_save_finish(tx->job,tx->ticket,tx->host->frozen.sequence,lid_progress_lease_save_active(tx->lease)));
    if(result==0)return lid_transaction_leave(tx,LID_TX_WAIT_SAVE); /* No timeout-as-proof. */
    if(result==2){tx->phase=LID_TX_RETRY;tx->retry_wait=tx->retry_delay;return lid_transaction_leave(tx,LID_TX_WAIT_RETRY);}
    if(result!=1){tx->host->poisoned=1;tx->phase=LID_TX_CLOSE_FAILED;return lid_transaction_close(tx);}
    tx->phase=LID_TX_CLOSE_OK;return lid_transaction_close(tx);
}
#endif
