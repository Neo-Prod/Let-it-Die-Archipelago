#ifndef LID_BLUEPRINT_STARTER_POLICY_H
#define LID_BLUEPRINT_STARTER_POLICY_H
/* Only the two verified initial +0 pairs. No game/file API here. */
static unsigned lid_starter_part(unsigned family){return family?20u:16u;}
static int lid_starter_catalogue(void){
    return lid_blueprint_families[4].ap_id==7110005u&&lid_blueprint_families[4].first==16&&
        lid_blueprint_families[5].ap_id==7110006u&&lid_blueprint_families[5].first==20&&
        lid_bp_ascii_compare(lid_blueprint_parts[16],"PT_ARM_WP005_001")==0&&
        lid_bp_ascii_compare(lid_blueprint_parts[20],"PT_ARM_WP006_001")==0;
}
static unsigned lid_starter_mask(const LidBlueprintNativeSnapshot*s,const unsigned*counts,const unsigned char*waiting){
    unsigned f,mask=0;
    if(!s||s->valid!=1||!counts||!waiting||!lid_starter_catalogue())return 0;
    for(f=0;f<2;f++){
        size_t i;unsigned seen=0,bad=0,part=lid_starter_part(f);
        if(counts[4+f]||waiting[part])continue;
        for(i=0;i<s->count;i++)if(s->records[i].state.part==part){
            const LidBlueprintNativeRecord*r=s->records+i;unsigned level=r->state.level,bit=level==1?1u:level==2?2u:0u;
            if(!bit||(seen&bit)||r->before_level||r->before_ptid[0]||r->state.announced>1||r->state.checked>1||
               (level==1?(r->state.research!=LID_BP_FINISHED||r->state.receive!=LID_BP_CHARGE):
                          (r->state.research!=LID_BP_LEVELUP||r->state.receive!=LID_BP_UNKNOWN)))bad=1;
            seen|=bit;
        }
        if(seen==3&&!bad)mask|=1u<<f;
    }
    return mask;
}
static int lid_starter_selected(unsigned part,unsigned mask){
    return ((mask&1)&&part==16)||((mask&2)&&part==20);
}
static int lid_starter_absent(const LidBlueprintNativeSnapshot*s,unsigned mask){
    size_t i;if(!s||s->valid!=1)return 0;
    for(i=0;i<s->count;i++)if(lid_starter_selected(s->records[i].state.part,mask))return 0;
    return 1;
}
typedef struct LidStarterOps {
    void*context;int(*guard)(void*);int(*read)(void*,LidBlueprintNativeSnapshot*);
    int(*erase)(void*,unsigned,unsigned);
} LidStarterOps;
static int lid_starter_cleanup(LidBlueprintNativeSnapshot*before,LidBlueprintNativeSnapshot*after,
    unsigned mask,const LidStarterOps*ops){
    unsigned f,level;size_t i,n=0,known=0;
    if(!mask||mask>3||!ops||!ops->guard||!ops->read||!ops->erase||
       !lid_bp_commit_separate(before,after)||ops->guard(ops->context)!=1||
       ops->read(ops->context,after)!=1||!lid_bp_commit_unchanged(before,after))return 0;
    for(f=0;f<2;f++)if(mask&(1u<<f))for(level=1;level<=2;level++){
        if(ops->guard(ops->context)!=1||ops->erase(ops->context,lid_starter_part(f),level)!=1)return 0;
    }
    /* Before is a detached snapshot, not the game's array. */
    for(i=0;i<before->count;i++)if(!lid_starter_selected(before->records[i].state.part,mask)){
        if(n!=i)before->records[n]=before->records[i];
        if(before->records[n].state.part<LID_BLUEPRINT_PART_COUNT)known++;n++;
    }
    before->count=n;before->known=known;
    return ops->guard(ops->context)==1&&ops->read(ops->context,after)==1&&
        lid_starter_absent(after,mask)&&lid_bp_commit_unchanged(before,after);
}
#endif
