#ifndef LID_BLUEPRINT_NATIVE_COMMIT_H
#define LID_BLUEPRINT_NATIVE_COMMIT_H
/* Candidate execution boundary; include blueprint_native_model.h first.
   No hardcoded addresses, allocations, files or save calls. All callbacks run
   synchronously on the reserved owner thread. Runtime wiring remains disabled.
   read_model must detach the actual repository into the supplied output buffer;
   upsert must synchronously copy the desired fields through the native routine. */
typedef struct LidBlueprintCommitOps {
    void* context;
    int (*guard)(void*);
    int (*read_model)(void*,LidBlueprintNativeSnapshot*);
    int (*upsert)(void*,const LidBlueprintNativeRecord*);
} LidBlueprintCommitOps;
typedef struct LidBlueprintCommit {
    const LidBlueprintNativeSnapshot* before;
    LidBlueprintNativeSnapshot* after;
    const LidBlueprintPlan* plan;
    LidBlueprintCommitOps ops;
    unsigned attempted,started,applied,stopped;
} LidBlueprintCommit;
static int lid_bp_commit_plan_valid(const LidBlueprintPlan* plan){
    unsigned i;
    if(!plan||plan->prepared!=1||plan->action_count>LID_BLUEPRINT_MAX_ACTIONS||
       plan->action_count%2||plan->target_count>LID_BLUEPRINT_PART_COUNT||
       plan->preserved_count>plan->target_count||plan->action_count/2!=plan->target_count-plan->preserved_count)return 0;
    for(i=0;i<plan->action_count;i+=2){
        const LidBlueprintAction* a=plan->actions+i;const LidBlueprintRow *base=&a[0].row,*next=&a[1].row;
        if(base->part>=LID_BLUEPRINT_PART_COUNT||base->level!=1||base->research!=LID_BP_FINISHED||
           base->receive!=LID_BP_CHARGE||base->announced!=1||base->checked||a[0].replaces_existing>1||
           next->part!=base->part||next->level!=2||next->research!=LID_BP_LEVELUP||
           next->receive!=LID_BP_UNKNOWN||next->announced||next->checked||a[1].replaces_existing||
           (i&&plan->actions[i-2].row.part>=base->part))return 0;
    }
    return 1;
}
static int lid_bp_commit_separate(const LidBlueprintNativeSnapshot* before,const LidBlueprintNativeSnapshot* after){
    size_t b,a;
    if(!before||!after||before==after||before->valid!=1||!before->records||!after->records||
       !before->capacity||!after->capacity||before->capacity>LID_BLUEPRINT_MAX_ROWS||after->capacity>LID_BLUEPRINT_MAX_ROWS||
       before->count>before->capacity||before->known>before->count)return 0;
    b=(size_t)before->records;a=(size_t)after->records;
    return b<=a?a-b>=before->capacity*sizeof(*before->records):b-a>=after->capacity*sizeof(*after->records);
}
static int lid_bp_commit_unchanged(const LidBlueprintNativeSnapshot* before,const LidBlueprintNativeSnapshot* fresh){
    size_t i;
    if(before->valid!=1||fresh->valid!=1||before->count!=fresh->count||before->known!=fresh->known||fresh->count>fresh->capacity)return 0;
    for(i=0;i<before->count;i++)if(!lid_wire_equal((const BYTE*)(before->records+i),(const BYTE*)(fresh->records+i),sizeof(*before->records)))return 0;
    return 1;
}
/* 1=applied and full RAM readback, 0=pre-mutation refusal, -1=uncertain after
   mutation began. Never retry the same job, never infer a save proof here. */
static int lid_blueprint_commit_run(LidBlueprintCommit* job){
    unsigned i;LidBlueprintNativeRecord desired;
    if(!job||job->attempted||job->started||job->applied||job->stopped||
       !job->ops.guard||!job->ops.read_model||!job->ops.upsert||
       !lid_bp_commit_plan_valid(job->plan)||!lid_bp_commit_separate(job->before,job->after))return 0;
    job->attempted=1;
    if(job->ops.guard(job->ops.context)!=1||job->ops.read_model(job->ops.context,job->after)!=1||
       !lid_bp_commit_unchanged(job->before,job->after))return 0;
    for(i=0;i<job->plan->action_count;i++){
        if(job->ops.guard(job->ops.context)!=1){job->stopped=1;return job->started?-1:0;}
        lid_bp_expected_action(&desired,job->plan->actions+i);
        job->started=1; /* native failure may still have mutated the model */
        if(job->ops.upsert(job->ops.context,&desired)!=1){job->stopped=1;return -1;}
    }
    if(job->ops.guard(job->ops.context)!=1||job->ops.read_model(job->ops.context,job->after)!=1||
       !lid_blueprint_native_readback(job->before,job->after,job->plan)){
        job->stopped=1;return job->started?-1:0;
    }
    job->applied=1;return 1;
}
/* Call again AFTER the caller's own save callback and runner inactivity. This
   supplies only RAM-model proof; the transaction separately proves its save. */
static int lid_blueprint_commit_verify(LidBlueprintCommit* job){
    if(!job||job->attempted!=1||job->applied!=1||job->stopped||
       !job->ops.guard||!job->ops.read_model||!lid_bp_commit_separate(job->before,job->after)||
       !lid_bp_commit_plan_valid(job->plan))return 0;
    if(job->ops.guard(job->ops.context)!=1||job->ops.read_model(job->ops.context,job->after)!=1||
       !lid_blueprint_native_readback(job->before,job->after,job->plan)){
        job->stopped=1;return 0;
    }
    return 1;
}
#endif
