#ifndef LID_BLUEPRINT_ENHANCEMENT_PLAN_H
#define LID_BLUEPRINT_ENHANCEMENT_PLAN_H
/* V3 detached planner. No game/allocator/file access; bounded resident output.
   Existing +0 planner remains the legacy path. */
#define LID_ENHANCEMENT_MAX_ACTIONS LID_BLUEPRINT_MAX_ROWS
typedef struct LidEnhancementAction {unsigned part,level,research,announced,before_part,before_level,replace;} LidEnhancementAction;
typedef struct LidEnhancementPlan {
    LidEnhancementAction actions[LID_ENHANCEMENT_MAX_ACTIONS];
    unsigned target[LID_BLUEPRINT_PART_COUNT],archive[LID_BLUEPRINT_PART_COUNT];
    unsigned action_count,archive_count,target_count,preserved_count,prepared,steps;
} LidEnhancementPlan;
static const LidBlueprintNativeRecord*lid_enhance_find(const LidBlueprintNativeSnapshot*s,unsigned part,unsigned level){
    size_t low=0,high=s->known;
    while(low<high){size_t at=low+(high-low)/2;const LidBlueprintNativeRecord*r=s->records+at;
        if(r->state.part==part&&r->state.level==level)return r;
        if(r->state.part<part||(r->state.part==part&&r->state.level<level))low=at+1;else high=at;
    }return 0;
}
static const LidBlueprintNativeRecord*lid_enhance_highest(const LidBlueprintNativeSnapshot*s,unsigned part){
    const LidBlueprintNativeRecord*best=0;size_t low=0,high=s->known;
    while(low<high){size_t at=low+(high-low)/2;if(s->records[at].state.part<part)low=at+1;else high=at;}
    for(size_t i=low;i<s->known&&s->records[i].state.part==part;i++)if(s->records[i].state.research==LID_BP_FINISHED)best=s->records+i;
    return best;
}
static int lid_enhance_buyable(const LidBlueprintNativeRecord*r){return r&&(r->state.receive==LID_BP_CHARGE||r->state.receive==LID_BP_FREE);}
static int lid_enhance_add(LidEnhancementPlan*p,const LidBlueprintNativeSnapshot*s,unsigned part,unsigned level,unsigned research){
    if(p->action_count>=LID_ENHANCEMENT_MAX_ACTIONS)return 0;
    const LidBlueprintNativeRecord*r=lid_enhance_find(s,part,level);unsigned before=LID_BLUEPRINT_PART_COUNT,before_level=0;
    if(level>1){before=part;before_level=level-1;}
    else if(part>=LID_BLUEPRINT_LEGACY_PART_COUNT){before=lid_enhancement_owner[part];before_level=lid_enhancement_cap[before];}
    p->actions[p->action_count++]=(LidEnhancementAction){part,level,research,research==LID_BP_FINISHED,before,before_level,r!=0};return 1;
}
static int lid_enhance_prepare(LidEnhancementPlan*p,const unsigned*counts,unsigned steps,
                              const LidBlueprintNativeSnapshot*s,const unsigned char*waiting,const unsigned char*alias_owned){
    if(!p)return 0;lid_wire_zero(p,sizeof(*p));
    if(!counts||steps>4||!s||s->valid!=1||s->known>s->count||s->count>s->capacity||s->count>LID_BLUEPRINT_MAX_ROWS||!waiting||!alias_owned)return 0;
    p->steps=steps;
    for(unsigned f=0;f<LID_BLUEPRINT_FAMILY_COUNT;f++){
        if(counts[f]>100000)return 0;const LidEnhancementFamily*family=lid_enhancement_families+f;
        unsigned take=counts[f]<family->count[steps]?counts[f]:family->count[steps];
        for(unsigned j=0;j<take;j++){
            const LidEnhancementStage*t=lid_enhancement_stages+family->first[steps]+j;
            if(t->part>=LID_BLUEPRINT_PART_COUNT||!t->level||t->level>lid_enhancement_cap[t->part])return 0;
            if(p->target[t->part]<t->level)p->target[t->part]=t->level;
        }
    }
    /* Existing active uncap is authority for its base, never resurrect +0. */
    for(unsigned part=0;part<LID_BLUEPRINT_LEGACY_PART_COUNT;part++)if(alias_owned[part])p->target[part]=0;
    /* A requested new uncap requires the native capped parent chain. */
    for(unsigned part=LID_BLUEPRINT_LEGACY_PART_COUNT;part<LID_BLUEPRINT_PART_COUNT;part++)if(p->target[part]){
        const LidBlueprintNativeRecord*h=lid_enhance_highest(s,part);
        if(!h||h->state.level<p->target[part]){
            unsigned owner=lid_enhancement_owner[part];
            if(owner>=LID_BLUEPRINT_LEGACY_PART_COUNT||!lid_enhancement_cap[owner])return 0;
            p->target[owner]=lid_enhancement_cap[owner];p->archive[owner]=lid_enhancement_cap[owner]+1;
        }
    }
    for(unsigned part=0;part<LID_BLUEPRINT_PART_COUNT;part++)if(p->target[part]){
        unsigned goal=p->target[part],cap=lid_enhancement_cap[part];p->target_count++;
        if(!cap||goal>cap)return 0;
        const LidBlueprintNativeRecord*h=lid_enhance_highest(s,part);
        if(h&&h->state.level>=goal){
            if(!lid_enhance_buyable(h)&&!(p->archive[part]>goal&&h->state.receive==LID_BP_ARCHIVED))return 0;
            p->preserved_count++;
            if(!p->archive[part])continue;
        }
        if(waiting[part])return 0;
        for(unsigned level=1;level<=goal;level++){
            const LidBlueprintNativeRecord*r=lid_enhance_find(s,part,level);
            if(r&&r->state.research==LID_BP_FINISHED){
                if(r->state.receive!=LID_BP_CHARGE&&r->state.receive!=LID_BP_FREE&&r->state.receive!=LID_BP_ARCHIVED)return 0;
                continue;
            }
            if(r&&(r->state.research!=LID_BP_EMPTY&&r->state.research!=LID_BP_LEVELUP&&r->state.research!=LID_BP_REMODEL))return 0;
            if(r&&(r->state.receive!=LID_BP_UNKNOWN||r->state.checked))return 0;
            if(!lid_enhance_add(p,s,part,level,LID_BP_FINISHED))return 0;
        }
        if(goal<cap){
            const LidBlueprintNativeRecord*r=lid_enhance_find(s,part,goal+1);
            if(!r){if(!lid_enhance_add(p,s,part,goal+1,LID_BP_LEVELUP))return 0;}
            else if(r->state.research!=LID_BP_LEVELUP||r->state.receive!=LID_BP_UNKNOWN)return 0;
        }
        if(p->archive[part]<goal)p->archive[part]=goal;
    }
    /* Archiving is needed only where a CHARGE row will actually change. */
    for(unsigned part=0;part<LID_BLUEPRINT_PART_COUNT;part++)if(p->archive[part]){
        unsigned needed=0;size_t low=0,high=s->known;
        while(low<high){size_t m=low+(high-low)/2;if(s->records[m].state.part<part)low=m+1;else high=m;}
        for(size_t i=low;i<s->known&&s->records[i].state.part==part;i++)if(s->records[i].state.level<p->archive[part]&&s->records[i].state.receive==LID_BP_CHARGE)needed=1;
        for(unsigned i=0;!needed&&i<p->action_count;i++)if(p->actions[i].part==part&&p->actions[i].research==LID_BP_FINISHED&&p->actions[i].level<p->archive[part])needed=1;
        if(needed){if(waiting[part])return 0;p->archive_count++;}else p->archive[part]=0;
    }
    p->prepared=1;return 1;
}
static int lid_enhance_desired(LidBlueprintNativeRecord*out,const LidBlueprintNativeSnapshot*before,const LidEnhancementAction*a){
    if(!out||!a||a->part>=LID_BLUEPRINT_PART_COUNT||!a->level||a->level>lid_enhancement_cap[a->part])return 0;
    const LidBlueprintNativeRecord*old=lid_enhance_find(before,a->part,a->level);
    if((old!=0)!=!!a->replace)return 0;
    if(old)*out=*old;else{
        lid_wire_zero(out,sizeof(*out));lid_bp_utf_from_ascii(out->ptid,lid_blueprint_parts[a->part]);
        if(a->before_part<LID_BLUEPRINT_PART_COUNT){lid_bp_utf_from_ascii(out->before_ptid,lid_blueprint_parts[a->before_part]);out->before_level=a->before_level;}
    }
    out->state=(LidBlueprintRow){a->part,a->level,a->research,a->research==LID_BP_FINISHED?LID_BP_CHARGE:LID_BP_UNKNOWN,a->announced,0};
    lid_wire_zero(out->research_name,sizeof(out->research_name));lid_wire_zero(out->receive_name,sizeof(out->receive_name));
    lid_bp_utf_from_ascii(out->research_name,a->research==LID_BP_FINISHED?"FINISHED":"LEVELUP");
    lid_bp_utf_from_ascii(out->receive_name,a->research==LID_BP_FINISHED?"CHARGE":"UNKNOWN");return 1;
}
#ifndef LID_ITEM_READBACK_RECORD
#define LID_ITEM_READBACK_RECORD(expected,actual,index) ((void)0)
#endif
static int lid_enhance_readback(const LidBlueprintNativeSnapshot*before,const LidBlueprintNativeSnapshot*after,const LidEnhancementPlan*p){
    if(!before||!after||!p||!p->prepared||before->valid!=1||after->valid!=1||before->count>before->capacity||after->count>after->capacity)return 0;
    size_t b=0,a=0;unsigned action=0;LidBlueprintNativeRecord expected,desired;
    while(b<before->count||action<p->action_count){
        int order=-1;
        if(action<p->action_count){if(!lid_enhance_desired(&desired,before,p->actions+action))return 0;order=b<before->count?lid_bp_record_order(before->records+b,&desired):1;}
        if(order<0)expected=before->records[b++];
        else {if((order==0)!=!!p->actions[action].replace)return 0;if(!order)b++;expected=desired;action++;}
        unsigned part=expected.state.part;
        if(part<LID_BLUEPRINT_PART_COUNT&&p->archive[part]&&expected.state.level<p->archive[part]&&expected.state.receive==LID_BP_CHARGE){
            expected.state.receive=LID_BP_ARCHIVED;lid_wire_zero(expected.receive_name,sizeof(expected.receive_name));lid_bp_utf_from_ascii(expected.receive_name,"FINISHED");
        }
        if(a>=after->count||!lid_wire_equal((const BYTE*)&expected,(const BYTE*)(after->records+a),sizeof(expected))){
            LID_ITEM_READBACK_RECORD(&expected,a<after->count?after->records+a:0,a);return 0;
        }a++;
    }return a==after->count;
}
#endif
