#ifndef LID_BLUEPRINT_TRANSACTION_DOMAIN_H
#define LID_BLUEPRINT_TRANSACTION_DOMAIN_H
/* Candidate common-transaction domain. Include transaction, pair gate and
   native commit first. Host owns buffers/ops; no allocation or polling.
   Not yet attached by mod.c. Context must outlive the transaction. */
#include "blueprint_enhancement_plan.h"
#ifndef LID_ITEM_READBACK_FAIL
#define LID_ITEM_READBACK_FAIL(code) 0
#endif
typedef struct LidBlueprintDomain {
    LidCharacterTransaction* transaction;LidBlueprintPairGate* pair;
    LidBlueprintNativeSnapshot before,after;
    LidBlueprintRow* semantic;size_t semantic_capacity;
    LidBlueprintCommitOps native;
    void* waiting_context;int (*waiting)(void*,unsigned char*,size_t);
    void* publish_context;int (*publish)(void*,const BYTE*,size_t);
    LidBlueprintPlan plan;LidBlueprintCommit commit;LidWireMessage frozen;
    LidEnhancementPlan enhancement;unsigned enhanced_mode,enhanced_steps;
    unsigned counts[329];unsigned char waiting_parts[LID_BLUEPRINT_PART_COUNT],alias_owned[LID_BLUEPRINT_PART_COUNT],alias_present[LID_BLUEPRINT_PART_COUNT];BYTE proof[1636];
    LidProgressExtraOps extra;
    unsigned initialized,prepared,proof_encoded,proof_published;
} LidBlueprintDomain;
static int lid_bp_domain_matches(const LidWireMessage* a,const LidWireMessage* b){
    BYTE x[256],y[256];
    return a&&b&&lid_wire_encode(x,256,a)&&lid_wire_encode(y,256,b)&&lid_wire_equal(x,y,256);
}
static void lid_blueprint_domain_reset(void* context){
    LidBlueprintDomain* d=context;if(!d)return;
    d->prepared=d->proof_encoded=d->proof_published=0;
    d->enhanced_mode=d->enhanced_steps=0;lid_wire_zero(&d->enhancement,sizeof(d->enhancement));
    d->before.valid=d->after.valid=0;d->before.count=d->before.known=d->after.count=d->after.known=0;
    lid_wire_zero(&d->plan,sizeof(d->plan));lid_wire_zero(&d->commit,sizeof(d->commit));
    lid_wire_zero(&d->frozen,sizeof(d->frozen));
}
static int lid_blueprint_domain_prepare(void* context,const LidWireMessage* target){
    LidBlueprintDomain* d=context;LidCharacterHost* host;unsigned i;
    if(!d||!d->initialized||!target||d->prepared||!(host=d->transaction->host)||
       !host->grant_published||!host->peer.granted||host->poisoned||
       !lid_wire_same_writer(&host->peer.binding,target)||
       !lid_blueprint_pair_counts(d->pair,target,d->counts,329)||d->native.guard(d->native.context)!=1)return 0;
    if(d->native.read_model(d->native.context,&d->before)!=1||
       !lid_blueprint_native_semantic(&d->before,d->semantic,d->semantic_capacity)||
       !lid_blueprint_native_alias_state(&d->before,d->alias_owned,d->alias_present,LID_BLUEPRINT_PART_COUNT))return 0;
    lid_wire_zero(d->waiting_parts,sizeof(d->waiting_parts));
    if(d->waiting(d->waiting_context,d->waiting_parts,LID_BLUEPRINT_PART_COUNT)!=1)return 0;
    for(i=0;i<LID_BLUEPRINT_PART_COUNT;i++)if(d->alias_present[i]&&!d->alias_owned[i])d->waiting_parts[i]=1;
    d->enhanced_mode=lid_wire_r32(d->pair->companion+8)==3;d->enhanced_steps=d->enhanced_mode?lid_wire_r32(d->pair->companion+56):0;
    if(!lid_bp_commit_separate(&d->before,&d->after))return 0;
    if(d->enhanced_mode){
        if(!d->native.enhanced_guard||!d->native.enhanced_upsert||!d->native.archive||d->native.enhanced_guard(d->native.context)!=1||
           !lid_enhance_prepare(&d->enhancement,d->counts,d->enhanced_steps,&d->before,d->waiting_parts,d->alias_owned))return 0;
        d->plan.prepared=1;d->plan.action_count=d->enhancement.action_count+d->enhancement.archive_count;
        d->plan.target_count=d->enhancement.target_count;d->plan.preserved_count=d->enhancement.preserved_count;
    }else if(!lid_blueprint_plan_prepare_owned(&d->plan,d->counts,329,d->semantic,d->before.known,d->waiting_parts,d->alias_owned)||
       !lid_bp_commit_plan_valid(&d->plan))return 0;
    d->commit.before=&d->before;d->commit.after=&d->after;d->commit.plan=&d->plan;d->commit.ops=d->native;
    d->frozen=*target;d->prepared=1;return 1;
}
static int lid_enhance_domain_commit(LidBlueprintDomain*d){
    LidBlueprintCommit*job=&d->commit;LidEnhancementPlan*p=&d->enhancement;LidBlueprintNativeRecord desired;
    if(!p->prepared||job->attempted||job->started||job->applied||job->stopped||!d->native.enhanced_guard||!d->native.enhanced_upsert||!d->native.archive)return 0;
    job->attempted=1;
    if(d->native.enhanced_guard(d->native.context)!=1||d->native.read_model(d->native.context,&d->after)!=1||!lid_bp_commit_unchanged(&d->before,&d->after))return 0;
    for(unsigned i=0;i<p->action_count;i++){
        if(d->waiting_parts[p->actions[i].part]||d->native.enhanced_guard(d->native.context)!=1||!lid_enhance_desired(&desired,&d->before,p->actions+i)){job->stopped=1;return job->started?-1:0;}
        job->started=1;if(d->native.enhanced_upsert(d->native.context,&desired)!=1){LID_ITEM_READBACK_FAIL(408);job->stopped=1;return -1;}
    }
    for(unsigned part=0;part<LID_BLUEPRINT_PART_COUNT;part++)if(p->archive[part]){
        if(d->waiting_parts[part]||d->native.enhanced_guard(d->native.context)!=1){job->stopped=1;return job->started?-1:0;}
        job->started=1;if(d->native.archive(d->native.context,part,p->archive[part])!=1){LID_ITEM_READBACK_FAIL(411);job->stopped=1;return -1;}
    }
    unsigned failure=0;
    if(d->native.enhanced_guard(d->native.context)!=1)failure=412;
    else if(d->native.read_model(d->native.context,&d->after)!=1)failure=413;
    else if(!lid_enhance_readback(&d->before,&d->after,p))failure=106;
    if(failure){LID_ITEM_READBACK_FAIL(failure);job->stopped=1;return job->started?-1:0;}
    job->applied=1;return 1;
}
static int lid_blueprint_domain_commit(void* context){
    LidBlueprintDomain* d=context;unsigned i;if(!d||!d->prepared)return 0;
    lid_wire_zero(d->waiting_parts,sizeof(d->waiting_parts));
    if(d->native.guard(d->native.context)!=1||d->waiting(d->waiting_context,d->waiting_parts,LID_BLUEPRINT_PART_COUNT)!=1)return 0;
    if(d->enhanced_mode)return lid_enhance_domain_commit(d);
    for(i=0;i<d->plan.action_count;i+=2)if(d->waiting_parts[d->plan.actions[i].row.part])return 0;
    return lid_blueprint_commit_run(&d->commit);
}
static int lid_blueprint_domain_readback(void* context){
    LidBlueprintDomain* d=context;
    if(!d||!d->prepared)return LID_ITEM_READBACK_FAIL(101);
    if(!d->enhanced_mode){int ok=lid_blueprint_commit_verify(&d->commit);return ok?ok:LID_ITEM_READBACK_FAIL(102);}
    unsigned failure=0;
    if(!d->commit.applied||d->commit.stopped)failure=103;
    else if(d->native.enhanced_guard(d->native.context)!=1)failure=104;
    else if(d->native.read_model(d->native.context,&d->after)!=1)failure=105;
    else if(!lid_enhance_readback(&d->before,&d->after,&d->enhancement))failure=106;
    if(failure){d->commit.stopped=1;return LID_ITEM_READBACK_FAIL(failure);}
    return 1;
}
static int lid_blueprint_domain_refresh(void* context,const LidWireMessage* target){
    LidBlueprintDomain* d=context;
    return lid_blueprint_domain_prepare(d,target)&&d->plan.action_count==0;
}
static int lid_blueprint_domain_publish(void* context,const LidWireMessage* target){
    LidBlueprintDomain* d=context;LidCharacterTransaction* tx;
    if(!d||!d->prepared||!(tx=d->transaction)||tx->phase!=LID_TX_ACK||!tx->host->active||
       !tx->host->proved||tx->host->poisoned||tx->lease->active||tx->lease->window||tx->job->active||
       !d->commit.applied||d->commit.stopped||!lid_bp_domain_matches(target,&d->frozen)||
       !lid_bp_domain_matches(target,&tx->host->frozen))return 0;
    if(d->proof_published)return 1;
    if(!d->proof_encoded){
        if(d->enhanced_mode){
            LidWireMessage saved=*target;saved.kind=LID_WIRE_SAVED;saved.status=1;
            if(!lid_blueprint_encode_enhanced(d->proof,sizeof(d->proof),&saved,d->counts,329,d->enhanced_steps,LID_WIRE_SAVED))return 0;
        }else if(!lid_blueprint_saved_encode(d->proof,sizeof(d->proof),target,d->counts,329,lid_blueprint_catalogue_hash))return 0;
        d->proof_encoded=1;
    }
    if(d->publish(d->publish_context,d->proof,sizeof(d->proof))!=1)return 0;
    d->proof_published=1;return 1;
}
static int lid_blueprint_domain_init(LidBlueprintDomain* d,LidCharacterTransaction* transaction,
    LidBlueprintPairGate* pair,LidBlueprintNativeRecord* before,LidBlueprintNativeRecord* after,size_t capacity,
    LidBlueprintRow* semantic,size_t semantic_capacity,const LidBlueprintCommitOps* native,
    void* waiting_context,int (*waiting)(void*,unsigned char*,size_t),
    void* publish_context,int (*publish)(void*,const BYTE*,size_t)){
    if(!d||d->initialized||!transaction||!pair||!pair->initialized||!before||!after||!semantic||
       !capacity||capacity>LID_BLUEPRINT_MAX_ROWS||semantic_capacity<capacity||
       !native||!native->guard||!native->read_model||!native->upsert||!waiting||!publish)return 0;
    {size_t b=(size_t)before,a=(size_t)after;
     if(b<=a?a-b<capacity*sizeof(*before):b-a<capacity*sizeof(*after))return 0;}
    d->transaction=transaction;d->pair=pair;d->semantic=semantic;d->semantic_capacity=semantic_capacity;
    d->before=(LidBlueprintNativeSnapshot){before,capacity,0,0,0};d->after=(LidBlueprintNativeSnapshot){after,capacity,0,0,0};
    d->native=*native;d->waiting_context=waiting_context;d->waiting=waiting;
    d->publish_context=publish_context;d->publish=publish;
    d->extra=(LidProgressExtraOps){d,lid_blueprint_domain_prepare,lid_blueprint_domain_commit,lid_blueprint_domain_readback,
        lid_blueprint_domain_refresh,lid_blueprint_domain_publish,lid_blueprint_domain_reset};
    lid_blueprint_domain_reset(d);
    if(!lid_transaction_attach_extra(transaction,&d->extra))return 0;
    d->initialized=1;return 1;
}
#endif
