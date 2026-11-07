#ifndef LID_BLUEPRINT_PLAN_H
#define LID_BLUEPRINT_PLAN_H
/* Candidate pure preparation helper; include blueprint_catalog.h first.
   Rows are a caller-owned semantic snapshot sorted by (catalogue part,level).
   No game pointers, writes, allocations, I/O or save claims. The future adapter
   must prove a fresh writer/request/save reservation and timer availability.
   Keep the large output in persistent storage, not the engine stack. */
#define LID_BLUEPRINT_MAX_ROWS 32768u
#define LID_BLUEPRINT_MAX_ACTIONS (LID_BLUEPRINT_PART_COUNT*2u)
enum { LID_BP_EMPTY=0,LID_BP_FINISHED=1,LID_BP_LEVELUP=2,LID_BP_REMODEL=3,LID_BP_OTHER=4 };
enum { LID_BP_UNKNOWN=0,LID_BP_CHARGE=1,LID_BP_FREE=2,LID_BP_ARCHIVED=3,LID_BP_RECEIVE_OTHER=4 };
typedef struct LidBlueprintRow {
    unsigned part,level,research,receive,announced,checked;
} LidBlueprintRow;
typedef struct LidBlueprintAction {
    LidBlueprintRow row;
    unsigned replaces_existing;
} LidBlueprintAction;
typedef struct LidBlueprintPlan {
    LidBlueprintAction actions[LID_BLUEPRINT_MAX_ACTIONS];
    unsigned action_count,target_count,preserved_count,prepared;
} LidBlueprintPlan;
static int lid_blueprint_rows_valid(const LidBlueprintRow* rows,size_t count){
    size_t i;
    if(count>LID_BLUEPRINT_MAX_ROWS||(!rows&&count))return 0;
    for(i=0;i<count;i++){
        const LidBlueprintRow* r=rows+i;
        if(r->part>=LID_BLUEPRINT_PART_COUNT||!r->level||r->level>1000||
           r->research>LID_BP_OTHER||r->receive>LID_BP_RECEIVE_OTHER||r->announced>1)return 0;
        if(i&&(rows[i-1].part>r->part||(rows[i-1].part==r->part&&rows[i-1].level>=r->level)))return 0;
    }
    return 1;
}
static void lid_blueprint_plan_add(LidBlueprintPlan* plan,unsigned part,unsigned level,
                                   unsigned research,unsigned receive,unsigned replace){
    LidBlueprintAction* a=plan->actions+plan->action_count++;
    a->row=(LidBlueprintRow){part,level,research,receive,level==1?1u:0u,0};
    a->replaces_existing=replace;
}
/* waiting_by_part must include any unresolved paid research/operation that the
   adapter cannot safely reconcile, even expired-but-not-yet-processed entries.
   1 = prepared, 0 = refused; prepared remains zero on every rejection.
   Plan is caller-exclusive and must be consumed before snapshot can change. */
static int lid_blueprint_plan_prepare_owned(LidBlueprintPlan* plan,const unsigned* counts,size_t family_count,
                                      const LidBlueprintRow* rows,size_t row_count,const unsigned char* waiting_by_part,
                                      const unsigned char* already_owned){
    unsigned family,tier;size_t cursor=0;
    if(!plan)return 0;
    plan->prepared=plan->action_count=plan->target_count=plan->preserved_count=0;
    if(!counts||family_count!=LID_BLUEPRINT_FAMILY_COUNT||!waiting_by_part||!lid_blueprint_rows_valid(rows,row_count))return 0;
    for(family=0;family<family_count;family++)if(counts[family]>100000)return 0;
    for(family=0;family<family_count;family++){
        const LidBlueprintFamily* f=lid_blueprint_families+family;
        unsigned target=counts[family]<f->count?counts[family]:f->count;
        for(tier=0;tier<target;tier++){
            unsigned part=f->first+tier,highest=0,buyable=0,has_base=0,has_higher=0,uncertain=0;
            size_t start,end,i;
            while(cursor<row_count&&rows[cursor].part<part)cursor++;
            start=cursor;
            while(cursor<row_count&&rows[cursor].part==part)cursor++;
            end=cursor;plan->target_count++;
            if(already_owned&&already_owned[part]){plan->preserved_count++;continue;}
            for(i=start;i<end;i++){
                const LidBlueprintRow* r=rows+i;
                if(r->research==LID_BP_FINISHED){
                    highest=r->level;buyable=r->receive==LID_BP_CHARGE||r->receive==LID_BP_FREE;
                }
                if(r->level==1)has_base=1;
                if(r->level>1)has_higher=1;
                if(r->research!=LID_BP_EMPTY&&r->research!=LID_BP_REMODEL&&r->research!=LID_BP_LEVELUP)uncertain=1;
                if(r->receive!=LID_BP_UNKNOWN)uncertain=1;
            }
            if(highest){
                /* Do not resurrect an archived lower +0 hidden by a higher row. */
                if(!buyable)return 0;
                plan->preserved_count++;continue;
            }
            if(waiting_by_part[part]||has_higher||uncertain)return 0;
            lid_blueprint_plan_add(plan,part,1,LID_BP_FINISHED,LID_BP_CHARGE,has_base);
            lid_blueprint_plan_add(plan,part,2,LID_BP_LEVELUP,LID_BP_UNKNOWN,0);
        }
    }
    plan->prepared=1;return 1;
}
static int lid_blueprint_plan_prepare(LidBlueprintPlan* plan,const unsigned* counts,size_t family_count,
                                      const LidBlueprintRow* rows,size_t row_count,const unsigned char* waiting_by_part){
    return lid_blueprint_plan_prepare_owned(plan,counts,family_count,rows,row_count,waiting_by_part,0);
}
/* Readback for the semantic fields of proposed mutations, not a full model or
   save/reload proof. Full preservation/readback belongs to the native adapter. */
static int lid_blueprint_plan_actions_match(const LidBlueprintPlan* plan,const LidBlueprintRow* rows,size_t count){
    size_t cursor=0;unsigned i;
    if(!plan||plan->prepared!=1||plan->action_count>LID_BLUEPRINT_MAX_ACTIONS||
       !lid_blueprint_rows_valid(rows,count))return 0;
    for(i=0;i<plan->action_count;i++){
        const LidBlueprintRow* expected=&plan->actions[i].row;
        while(cursor<count&&(rows[cursor].part<expected->part||
              (rows[cursor].part==expected->part&&rows[cursor].level<expected->level)))cursor++;
        if(cursor==count||rows[cursor].part!=expected->part||rows[cursor].level!=expected->level||
           rows[cursor].research!=expected->research||rows[cursor].receive!=expected->receive||
           rows[cursor].announced!=expected->announced||rows[cursor].checked!=expected->checked)return 0;
    }
    return 1;
}
#endif
