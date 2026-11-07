#ifndef LID_BLUEPRINT_NATIVE_MODEL_H
#define LID_BLUEPRINT_NATIVE_MODEL_H
/* Candidate detached reader/full field readback. Include progression_wire.h,
   blueprint_catalog.h and blueprint_plan.h first. No allocations or game calls.
   Caller owns buffers and proves reservation/current identity before EACH read.
   Never put snapshot buffers on the game stack. Not referenced by mod.c. */
typedef struct LidBlueprintNativeRecord {
    LidBlueprintRow state;
    unsigned short ptid[64],research_name[32],receive_name[32],before_ptid[64];
    unsigned before_level;
} LidBlueprintNativeRecord;
typedef struct LidBlueprintNativeSnapshot {
    LidBlueprintNativeRecord* records;
    size_t capacity,count,known;
    unsigned valid;
} LidBlueprintNativeSnapshot;
typedef struct LidBlueprintNameIndex {unsigned sorted[976],initialized;} LidBlueprintNameIndex;
static unsigned lid_bp_ascii_fold(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int lid_bp_ascii_compare(const char* a,const char* b){
    while(*a&&lid_bp_ascii_fold((unsigned char)*a)==lid_bp_ascii_fold((unsigned char)*b)){a++;b++;}
    return (int)lid_bp_ascii_fold((unsigned char)*a)-(int)lid_bp_ascii_fold((unsigned char)*b);
}
static void lid_bp_name_index_init(LidBlueprintNameIndex* index){
    unsigned i,j,key;
    if(index->initialized)return;
    for(i=0;i<976;i++){
        key=i;j=i;
        while(j&&lid_bp_ascii_compare(lid_blueprint_parts[index->sorted[j-1]],lid_blueprint_parts[key])>0){
            index->sorted[j]=index->sorted[j-1];j--;
        }
        index->sorted[j]=key;
    }
    index->initialized=1;
}
static int lid_bp_utf_ascii_compare(const unsigned short* a,const char* b){
    while(*a&&lid_bp_ascii_fold(*a)==lid_bp_ascii_fold((unsigned char)*b)){a++;b++;}
    return (int)lid_bp_ascii_fold(*a)-(int)lid_bp_ascii_fold((unsigned char)*b);
}
static unsigned lid_bp_part_index(const LidBlueprintNameIndex* index,const unsigned short* ptid){
    unsigned low=0,high=976,middle;int cmp;
    while(low<high){middle=low+(high-low)/2;cmp=lid_bp_utf_ascii_compare(ptid,lid_blueprint_parts[index->sorted[middle]]);
        if(!cmp)return index->sorted[middle];if(cmp<0)high=middle;else low=middle+1;}
    return LID_BLUEPRINT_PART_COUNT;
}
static int lid_bp_utf_equal_ascii(const unsigned short* text,const char* expected){return !lid_bp_utf_ascii_compare(text,expected);}
static void lid_bp_utf_from_ascii(unsigned short* out,const char* text){while(*text)*out++=(unsigned char)*text++;*out=0;}
static int lid_bp_read_string(unsigned short* out,size_t capacity,const BYTE* header,int (*readable)(const void*,size_t)){
    const BYTE* data;unsigned n,max;size_t i;
    if(!readable(header,16))return 0;
    data=(const BYTE*)(size_t)lid_wire_r64(header);n=lid_wire_r32(header+8);max=lid_wire_r32(header+12);
    if(n>max||max>0x7fffffffu||n>capacity)return 0;
    if(!n){out[0]=0;return 1;}
    if(!data||!readable(data,(size_t)n*2))return 0;
    for(i=0;i<n;i++){
        unsigned short c=(unsigned short)(data[i*2]|((unsigned)data[i*2+1]<<8));
        if((i+1==n&&c)||(i+1<n&&!c))return 0;out[i]=c;
    }
    return 1;
}
static int lid_bp_record_order(const LidBlueprintNativeRecord* a,const LidBlueprintNativeRecord* b){
    size_t i;
    if(a->state.part!=b->state.part)return a->state.part<b->state.part?-1:1;
    if(a->state.part==LID_BLUEPRINT_PART_COUNT)for(i=0;i<64;i++){
        if(lid_bp_ascii_fold(a->ptid[i])!=lid_bp_ascii_fold(b->ptid[i]))return lid_bp_ascii_fold(a->ptid[i])<lid_bp_ascii_fold(b->ptid[i])?-1:1;
        if(!a->ptid[i])break;
    }
    return a->state.level==b->state.level?0:a->state.level<b->state.level?-1:1;
}
static void lid_bp_record_swap(LidBlueprintNativeRecord* a,LidBlueprintNativeRecord* b){LidBlueprintNativeRecord copy=*a;*a=*b;*b=copy;}
static void lid_bp_heap_sift(LidBlueprintNativeRecord* records,size_t root,size_t count){
    size_t child;
    while(root<count/2){child=root*2+1;
        if(child+1<count&&lid_bp_record_order(records+child,records+child+1)<0)child++;
        if(lid_bp_record_order(records+root,records+child)>=0)return;
        lid_bp_record_swap(records+root,records+child);root=child;
    }
}
static void lid_bp_records_sort(LidBlueprintNativeRecord* records,size_t count){
    size_t i;if(count<2)return;
    for(i=count/2;i>0;i--)lid_bp_heap_sift(records,i-1,count);
    for(i=count;i>1;i--){lid_bp_record_swap(records,records+i-1);lid_bp_heap_sift(records,0,i-1);}
}
static int lid_blueprint_native_read(LidBlueprintNativeSnapshot* out,LidBlueprintNameIndex* index,const BYTE* repository,
                                     const void* expected_repository_vtable,const void* expected_row_vtable,
                                     int (*readable)(const void*,size_t)){
    const BYTE* data;size_t i,known=0;unsigned count,max;
    if(!out)return 0;out->valid=0;out->count=out->known=0;
    if(!index||!readable||!expected_repository_vtable||!expected_row_vtable||!repository||
       !out->records||!out->capacity||out->capacity>LID_BLUEPRINT_MAX_ROWS||!readable(repository,0x60)||
       (const void*)(size_t)lid_wire_r64(repository)!=expected_repository_vtable)return 0;
    data=(const BYTE*)(size_t)lid_wire_r64(repository+0x50);count=lid_wire_r32(repository+0x58);max=lid_wire_r32(repository+0x5c);
    if(count>max||max>LID_BLUEPRINT_MAX_ROWS||count>out->capacity||
       (count&&(!data||!readable(data,(size_t)count*0x58))))return 0;
    lid_bp_name_index_init(index);
    for(i=0;i<count;i++){
        const BYTE* row=data+i*0x58;LidBlueprintNativeRecord* r=out->records+i;
        lid_wire_zero(r,sizeof(*r));
        if((const void*)(size_t)lid_wire_r64(row)!=expected_row_vtable||
           !lid_bp_read_string(r->ptid,64,row+8,readable)||!r->ptid[0]||
           !lid_bp_read_string(r->research_name,32,row+0x1c,readable)||
           !lid_bp_read_string(r->receive_name,32,row+0x2c,readable)||
           !lid_bp_read_string(r->before_ptid,64,row+0x44,readable))return 0;
        r->state.part=lid_bp_part_index(index,r->ptid);r->state.level=lid_wire_r32(row+0x18);
        {size_t j;for(j=0;r->ptid[j];j++)if(r->ptid[j]>127)return 0;}
        r->state.announced=lid_wire_r32(row+0x3c);r->state.checked=lid_wire_r32(row+0x40);
        r->before_level=lid_wire_r32(row+0x54);
        if(!r->state.level||r->state.level>1000||r->state.announced>1||r->before_level>1000)return 0;
        r->state.research=lid_bp_utf_equal_ascii(r->research_name,"FINISHED")?LID_BP_FINISHED:
            lid_bp_utf_equal_ascii(r->research_name,"LEVELUP")?LID_BP_LEVELUP:
            lid_bp_utf_equal_ascii(r->research_name,"REMODEL")?LID_BP_REMODEL:
            !r->research_name[0]?LID_BP_EMPTY:LID_BP_OTHER;
        r->state.receive=lid_bp_utf_equal_ascii(r->receive_name,"CHARGE")?LID_BP_CHARGE:
            lid_bp_utf_equal_ascii(r->receive_name,"FREE")?LID_BP_FREE:
            lid_bp_utf_equal_ascii(r->receive_name,"FINISHED")?LID_BP_ARCHIVED:
            lid_bp_utf_equal_ascii(r->receive_name,"UNKNOWN")?LID_BP_UNKNOWN:LID_BP_RECEIVE_OTHER;
        if(r->state.part<LID_BLUEPRINT_PART_COUNT)known++;
    }
    lid_bp_records_sort(out->records,count);
    for(i=1;i<count;i++)if(lid_bp_record_order(out->records+i-1,out->records+i)>=0)return 0;
    out->count=count;out->known=known;out->valid=1;return 1;
}
static int lid_blueprint_native_semantic(const LidBlueprintNativeSnapshot* snapshot,LidBlueprintRow* out,size_t capacity){
    size_t i;
    if(!snapshot||snapshot->valid!=1||snapshot->known>snapshot->count||snapshot->count>snapshot->capacity||
       snapshot->count>LID_BLUEPRINT_MAX_ROWS||(!snapshot->records&&snapshot->count)||!out||capacity<snapshot->known)return 0;
    for(i=0;i<snapshot->known;i++)out[i]=snapshot->records[i].state;
    return lid_blueprint_rows_valid(out,snapshot->known);
}
/* Recognize active uncapped variants without merging their real keys/levels.
   Only the highest FINISHED level of each alias can establish ownership. */
static int lid_blueprint_native_alias_state(const LidBlueprintNativeSnapshot* snapshot,unsigned char* owned,unsigned char* present,size_t count){
    unsigned highest[LID_BLUEPRINT_ALIAS_COUNT]={0};unsigned char active[LID_BLUEPRINT_ALIAS_COUNT]={0};
    size_t i;unsigned low,high,middle;int cmp;
    if(!owned||count!=LID_BLUEPRINT_PART_COUNT)return 0;lid_wire_zero(owned,count);
    if(present)lid_wire_zero(present,count);
    if(!snapshot||snapshot->valid!=1||snapshot->known>snapshot->count||snapshot->count>snapshot->capacity||
       snapshot->count>LID_BLUEPRINT_MAX_ROWS||(!snapshot->records&&snapshot->count))return 0;
    for(i=snapshot->known;i<snapshot->count;i++){
        const LidBlueprintNativeRecord* r=snapshot->records+i;
        low=0;high=LID_BLUEPRINT_ALIAS_COUNT;
        while(low<high){middle=low+(high-low)/2;cmp=lid_bp_utf_ascii_compare(r->ptid,lid_blueprint_aliases[middle].ptid);
            if(!cmp){
                if(present)present[lid_blueprint_aliases[middle].owner]=1;
                if(r->state.research==LID_BP_FINISHED&&r->state.level>highest[middle]){highest[middle]=r->state.level;
                    active[middle]=(unsigned char)(r->state.receive==LID_BP_CHARGE||r->state.receive==LID_BP_FREE);}
                break;
            }
            if(cmp<0)high=middle;else low=middle+1;
        }
    }
    for(i=0;i<LID_BLUEPRINT_ALIAS_COUNT;i++)if(active[i])owned[lid_blueprint_aliases[i].owner]=1;
    return 1;
}
static int lid_blueprint_native_alias_owned(const LidBlueprintNativeSnapshot* snapshot,unsigned char* owned,size_t count){
    return lid_blueprint_native_alias_state(snapshot,owned,0,count);
}
static void lid_bp_expected_action(LidBlueprintNativeRecord* out,const LidBlueprintAction* action){
    lid_wire_zero(out,sizeof(*out));out->state=action->row;
    lid_bp_utf_from_ascii(out->ptid,lid_blueprint_parts[action->row.part]);
    lid_bp_utf_from_ascii(out->research_name,action->row.research==LID_BP_FINISHED?"FINISHED":"LEVELUP");
    lid_bp_utf_from_ascii(out->receive_name,action->row.receive==LID_BP_CHARGE?"CHARGE":"UNKNOWN");
}
/* Exact detached-field comparison of every before/after record, including
   unrelated/non-catalogue records. Only plan actions may replace/add records.
   No hash-as-proof: all copied UTF-16 fields and scalar fields are compared.
   Does not prove save/reload or cover other repositories. */
static int lid_blueprint_native_readback(const LidBlueprintNativeSnapshot* before,const LidBlueprintNativeSnapshot* after,
                                         const LidBlueprintPlan* plan){
    size_t b=0,a=0,p=0;LidBlueprintNativeRecord expected;int order;
    if(!before||!after||before->valid!=1||after->valid!=1||!plan||plan->prepared!=1||plan->action_count>LID_BLUEPRINT_MAX_ACTIONS||
       before->count>before->capacity||after->count>after->capacity||
       before->count>LID_BLUEPRINT_MAX_ROWS||after->count>LID_BLUEPRINT_MAX_ROWS||
       (!before->records&&before->count)||(!after->records&&after->count))return 0;
    if(plan->action_count&&before->count&&after->count){
        size_t baddr=(size_t)before->records,aaddr=(size_t)after->records;
        if(baddr<=aaddr?aaddr-baddr<before->count*sizeof(*before->records):
                         baddr-aaddr<after->count*sizeof(*after->records))return 0;
    }
    while(b<before->count||p<plan->action_count){
        order=1;
        if(p<plan->action_count){
            if(plan->actions[p].row.part>=LID_BLUEPRINT_PART_COUNT)return 0;
            lid_bp_expected_action(&expected,plan->actions+p);
            if(b<before->count)order=lid_bp_record_order(before->records+b,&expected);
        }else order=-1;
        if(order<0)expected=before->records[b++];
        else {
            if((order==0)!=!!plan->actions[p].replaces_existing)return 0;
            if(order==0)b++;
            p++;
        }
        if(a==after->count||!lid_wire_equal((const BYTE*)&expected,(const BYTE*)(after->records+a),sizeof(expected)))return 0;
        a++;
    }
    return a==after->count;
}
#endif
