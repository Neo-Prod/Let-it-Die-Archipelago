#ifndef LID_PROGRESSION_GAME_BAG_H
#define LID_PROGRESSION_GAME_BAG_H

/* Candidate UE3 layouts, SHA f33e91e6...17bddfd. No addresses or game calls.
   Include progression_bag_plan.h first. Host must use the game's allocator,
   reserve workers and prove player/model identity before calling prepare_group.
   This header is not included by mod.c yet. */

typedef struct __attribute__((packed,aligned(4))) LidBagString {
    wchar_t* data;int num;int capacity;
} LidBagString;
typedef struct __attribute__((packed,aligned(4))) LidCharaBagEntry {
    int slot;LidBagString ids[4];
} LidCharaBagEntry;
typedef struct __attribute__((packed,aligned(4))) LidModelBagEntry {
    void* vtable;int uid;LidBagString cid;int slot;int type;
    LidBagString entity;LidBagString equip_site;int scalar44;
} LidModelBagEntry;
typedef char LidCharaBagEntrySize[(sizeof(LidCharaBagEntry)==0x44)?1:-1];
typedef char LidModelBagEntrySize[(sizeof(LidModelBagEntry)==0x48)?1:-1];
typedef char LidModelEntityOffset[(__builtin_offsetof(LidModelBagEntry,entity)==0x24)?1:-1];
typedef char LidModelSlotOffset[(__builtin_offsetof(LidModelBagEntry,slot)==0x1c)?1:-1];

typedef struct LidGameBagContext {
    int model; /* 0 Chara, 1 offline model. */
    void* model_entry_vtable;
    int uid;
    const LidBagString* cid; /* Borrowed identity key, never destroyed here. */
    void* (WINAPI *allocate)(size_t);
    void (WINAPI *release)(void*);
    int (WINAPI *readable)(const void*,size_t);
} LidGameBagContext;

static int lid_game_bag_string_valid(const LidBagString* s,const LidGameBagContext* ctx){
    int i;
    if(!ctx->readable(s,sizeof(*s)))return 0;
    if(s->num<0||s->capacity<s->num||s->capacity>256)return 0;
    if(!s->data)return s->num==0&&s->capacity==0;
    if(!ctx->readable(s->data,(size_t)(s->num?s->num:1)*sizeof(wchar_t)))return 0;
    if(s->num&&s->data[s->num-1]!=0)return 0;
    for(i=0;i<s->num-1;i++)if(!s->data[i])return 0;
    return 1;
}
static int lid_game_bag_string_same(const LidBagString* a,const LidBagString* b){
    int i;
    if(a->num!=b->num)return 0;
    for(i=0;i<a->num;i++)if(a->data[i]!=b->data[i])return 0;
    return 1;
}
static int lid_game_bag_chara_absent(const LidBagString* s){
    return s->num<=1||(s->num==3&&s->data[0]=='-'&&s->data[1]=='1'&&s->data[2]==0);
}
static int lid_game_bag_strings(const void* entry,const LidGameBagContext* ctx,
                               const LidBagString** result){
    int i;
    if(ctx->model){
        const LidModelBagEntry* e=entry;
        result[0]=&e->cid;result[1]=&e->entity;result[2]=&e->equip_site;return 3;
    }
    for(i=0;i<4;i++)result[i]=&((const LidCharaBagEntry*)entry)->ids[i];
    return 4;
}
static int WINAPI lid_game_bag_entry_valid(const void* entry,void* context){
    LidGameBagContext* ctx=context;
    const LidBagString* strings[4];int i,n;
    if(!ctx||!ctx->readable(entry,ctx->model?0x48:0x44))return 0;
    n=lid_game_bag_strings(entry,ctx,strings);
    for(i=0;i<n;i++)if(!lid_game_bag_string_valid(strings[i],ctx))return 0;
    if(ctx->model){
        const LidModelBagEntry* e=entry;
        if(!ctx->cid||!ctx->model_entry_vtable||!lid_game_bag_string_valid(ctx->cid,ctx))return 0;
        if(e->vtable!=ctx->model_entry_vtable||e->uid!=ctx->uid||
           e->slot<0||e->slot>=99||e->type< -1||e->type>3)return 0;
        return ctx->cid->num>1&&lid_game_bag_string_same(&e->cid,ctx->cid);
    }
    return ((const LidCharaBagEntry*)entry)->slot>=0&&((const LidCharaBagEntry*)entry)->slot<99;
}
static int WINAPI lid_game_bag_entry_empty(const void* entry,void* context){
    LidGameBagContext* ctx=context;int i;
    if(ctx->model){
        const LidModelBagEntry* e=entry;
        /* Conservative subset of native empty: reject stale non-default metadata. */
        return e->entity.num<=1&&e->equip_site.num<=1&&e->type== -1&&e->scalar44== -1;
    }
    for(i=0;i<4;i++)if(!lid_game_bag_chara_absent(&((const LidCharaBagEntry*)entry)->ids[i]))return 0;
    return 1;
}
static int lid_game_bag_string_copy(LidBagString* out,const LidBagString* source,
                                    LidGameBagContext* ctx){
    int i;
    if(!source->num)return 1;
    out->data=ctx->allocate((size_t)source->num*sizeof(wchar_t));
    if(!out->data)return 0;
    out->num=out->capacity=source->num;
    for(i=0;i<source->num;i++)out->data[i]=source->data[i];
    return 1;
}
static int WINAPI lid_game_bag_entry_construct(void* entry,int index,void* context){
    LidGameBagContext* ctx=context;
    static wchar_t absent_data[]={'-','1',0};
    LidBagString absent={absent_data,3,3};int i;
    /* The generic plan zeros the destination first. Failure leaves owned
       partial fields for entry_destroy, never borrowed source pointers. */
    if(ctx->model){
        LidModelBagEntry* e=entry;
        e->vtable=ctx->model_entry_vtable;e->uid=ctx->uid;e->slot=index;
        e->type= -1;e->scalar44= -1;
        return lid_game_bag_string_copy(&e->cid,ctx->cid,ctx);
    }
    ((LidCharaBagEntry*)entry)->slot=index;
    for(i=0;i<4;i++)if(!lid_game_bag_string_copy(&((LidCharaBagEntry*)entry)->ids[i],&absent,ctx))return 0;
    return 1;
}
static void WINAPI lid_game_bag_entry_destroy(void* entry,void* context){
    LidGameBagContext* ctx=context;const LidBagString* strings[4];int i,n;
    n=lid_game_bag_strings(entry,ctx,strings);
    for(i=0;i<n;i++){
        LidBagString* s=(LidBagString*)strings[i];
        if(s->data)ctx->release(s->data);
        s->data=0;s->num=s->capacity=0;
    }
}

/* All allocated FString pointers must have one owner across this group. Native
   copy constructors are deep. Reject unexpected aliasing rather than deciding
   which entry may free a shared buffer. The borrowed identity key is excluded
   from ownership and forbidden as an owned entry allocation. */
static int lid_game_bag_preflight(LidBagArray** arrays,LidGameBagContext* contexts,int count){
    int g,i,j,k,n,pg,pi,pj,pn;
    if(!arrays||!contexts||count<1||count>4)return 0;
    for(g=0;g<count;g++){
        LidGameBagContext* ctx=&contexts[g];
        if((ctx->model!=0&&ctx->model!=1)||!ctx->allocate||!ctx->release||!ctx->readable)return 0;
        if(ctx->cid&&!lid_game_bag_string_valid(ctx->cid,ctx))return 0;
        if(ctx->model&&(!ctx->cid||ctx->cid->num<=1||!ctx->model_entry_vtable))return 0;
    }
    for(g=0;g<count;g++){
        LidGameBagContext* ctx=&contexts[g];LidBagArray* a=arrays[g];size_t stride;
        if((ctx->model!=0&&ctx->model!=1)||!ctx->allocate||!ctx->release||!ctx->readable)return 0;
        if(!a||!ctx->readable(a,sizeof(*a)))return 0;
        if(a->num<0||a->num>99||a->capacity<a->num||a->capacity>4096)return 0;
        if((!a->data)!=(a->capacity==0))return 0;
        stride=ctx->model?0x48:0x44;
        if(a->num&&!ctx->readable(a->data,(size_t)a->num*stride))return 0;
        for(i=0;i<g;i++)if(arrays[i]==a||(a->data&&arrays[i]->data==a->data))return 0;
        for(i=0;i<a->num;i++){
            const void* entry=(BYTE*)a->data+(size_t)i*stride;
            const LidBagString* strings[4];
            if(!lid_game_bag_entry_valid(entry,ctx))return 0;
            if((ctx->model?((const LidModelBagEntry*)entry)->slot:((const LidCharaBagEntry*)entry)->slot)!=i)return 0;
            n=lid_game_bag_strings(entry,ctx,strings);
            for(j=0;j<n;j++)if(strings[j]->data){
                for(k=0;k<count;k++)if(contexts[k].cid&&contexts[k].cid->data==strings[j]->data)return 0;
                /* Bounded pairwise scan avoids a >12KB stack buffer in the
                   freestanding DLL, whose toolchain disables stack probes. */
                for(pg=0;pg<=g;pg++)for(pi=0;pi<(pg==g?i+1:arrays[pg]->num);pi++){
                    const LidBagString* previous[4];
                    const void* previous_entry=(BYTE*)arrays[pg]->data+(size_t)pi*(contexts[pg].model?0x48:0x44);
                    pn=lid_game_bag_strings(previous_entry,&contexts[pg],previous);
                    for(pj=0;pj<((pg==g&&pi==i)?j:pn);pj++)
                        if(previous[pj]->data==strings[j]->data)return 0;
                }
            }
        }
    }
    return 1;
}
static int lid_game_bag_prepare_group(LidBagPlan* plans,LidBagArray** arrays,
                                     LidGameBagContext* contexts,int count,int target){
    int i;
    if(!plans||!lid_game_bag_preflight(arrays,contexts,count))return 0;
    for(i=0;i<count;i++){
        LidGameBagContext* ctx=&contexts[i];
        LidBagOps ops={ctx->allocate,ctx->release,ctx->readable,lid_game_bag_entry_valid,
                       lid_game_bag_entry_empty,lid_game_bag_entry_construct,lid_game_bag_entry_destroy};
        if(!lid_bag_prepare(&plans[i],arrays[i],target,ctx->model?0x48:0x44,&ops,ctx)){
            while(i>0)lid_bag_abort(&plans[--i]);
            return 0;
        }
    }
    return 1;
}
static int lid_game_bag_commit_group(LidBagPlan* plans,LidBagArray** arrays,
                                    LidGameBagContext* contexts,int count){
    int i;
    if(!plans||!lid_game_bag_preflight(arrays,contexts,count))return 0;
    for(i=0;i<count;i++)if(plans[i].array!=arrays[i]||plans[i].context!=&contexts[i])return 0;
    return lid_bag_commit_all(plans,count);
}

/* Pawn's third representation is resized by its UE3 event, not this planner.
   Static evidence140F9CDD0: stride718, mEnable bit0 at entry+0. */
static int lid_game_bag_pawn_tail_empty(const LidBagArray* array,int target,
                                       int (WINAPI *readable)(const void*,size_t)){
    int i;
    if(!array||!readable||target<1||target>=100||!readable(array,sizeof(*array)))return 0;
    if(array->num<0||array->num>99||array->capacity<array->num||array->capacity>4096)return 0;
    if((!array->data)!=(array->capacity==0))return 0;
    if(array->num&&!readable(array->data,(size_t)array->num*0x718))return 0;
    for(i=target;i<array->num;i++)if(*((BYTE*)array->data+(size_t)i*0x718)&1)return 0;
    return 1;
}

#endif
