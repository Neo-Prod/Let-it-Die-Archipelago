#ifndef LID_PROGRESSION_CHARACTER_APPLY_H
#define LID_PROGRESSION_CHARACTER_APPLY_H
/* Include config, game_bag and models first. This adapter never starts a save.
   Its caller must own the shared writer mutex AND a fresh engine reservation.
   All event callbacks run on that owner thread. Never blindly roll back an
   event that may already have changed engine state. */
typedef struct LidCharacterView {
    void* generation;void* userdata;void* pawn;void* common;
    BYTE* chara;BYTE* body;int* common_grade;LidBagArray* pawn_slots;
    LidCharacterModels models;
    const LidBagString* cid;
} LidCharacterView;
typedef struct LidCharacterApplyOps {
    void* context;
    int (WINAPI *readable)(const void*,size_t);
    int (WINAPI *writable)(const void*,size_t);
    void* (WINAPI *allocate)(size_t);
    void (WINAPI *release)(void*);
    int (*window_held)(void*);
    int (*is_current)(void*,const LidCharacterView*);
    int (*events_supported)(void*,const LidCharacterView*);
    int (*resize_pawn)(void*,const LidCharacterView*,int);
    int (*recalculate)(void*,const LidCharacterView*);
} LidCharacterApplyOps;
typedef struct LidCharacterApplication {
    LidCharacterView view;LidCharacterApplyOps ops;
    LidGameBagContext contexts[3];LidBagArray* arrays[3];LidBagPlan plans[3];
    LidBagArray original_pawn;
    int original_levels[3][10],levels[10],absolute[9],original_grade;
    int count,prepared,started,applied;
    int bag_storage_target; /* logical capacity remains absolute[6] */
} LidCharacterApplication;
static void lid_character_apply_abort(LidCharacterApplication* batch){
    int i;if(!batch)return;
    for(i=0;i<3;i++)lid_bag_abort(&batch->plans[i]);
    batch->prepared=0;
}
static int lid_character_levels_valid(const int* levels){
    int i;if(levels[0]<1||levels[0]>397)return 0;
    for(i=1;i<=6;i++)if(levels[i]<1||levels[i]>45)return 0;
    for(i=7;i<10;i++)if(levels[i]<0||levels[i]>44)return 0;
    return 1;
}
static int lid_character_apply_guard(const LidCharacterView* view,const LidCharacterApplyOps* ops){
    if(!view||!ops||!ops->readable||!ops->writable||!ops->allocate||!ops->release||
       !ops->window_held||!ops->is_current||!ops->events_supported||!ops->resize_pawn||!ops->recalculate)return 0;
    return ops->window_held(ops->context)==1&&ops->is_current(ops->context,view)==1;
}
static int lid_character_identity_ownership(const LidCharacterApplication* batch){
    const LidBagString* keys[4];int g,i,j,k,n;
    keys[0]=batch->view.cid;keys[1]=(LidBagString*)batch->view.body;
    keys[2]=(LidBagString*)(batch->view.models.body+0xc);keys[3]=(LidBagString*)(batch->view.models.bag+0x60);
    if(!lid_game_bag_preflight((LidBagArray**)batch->arrays,(LidGameBagContext*)batch->contexts,batch->count))return 0;
    for(k=0;k<4;k++)if(!lid_game_bag_string_valid(keys[k],&batch->contexts[0])||
        !lid_game_bag_string_same(keys[k],keys[0]))return 0;
    /* A tail destructor must never free a CID owned by any parent record. */
    for(g=0;g<batch->count;g++)for(i=0;i<batch->arrays[g]->num;i++){
        const LidBagString* strings[4];
        const void* entry=(BYTE*)batch->arrays[g]->data+(size_t)i*(batch->contexts[g].model?0x48:0x44);
        n=lid_game_bag_strings(entry,&batch->contexts[g],strings);
        for(j=0;j<n;j++)if(strings[j]->data)for(k=0;k<4;k++)if(strings[j]->data==keys[k]->data)return 0;
    }
    return 1;
}
static int lid_character_apply_prepare(LidCharacterApplication* batch,const LidCharacterView* view,
                                       const LidCharacterConfig* config,const unsigned* counts,
                                       const LidCharacterApplyOps* ops,void* bag_entry_vtable){
    int i,j;const int* existing;LidBagString* cid;
    if(!batch||batch->prepared||batch->started||batch->applied||!bag_entry_vtable||
       !lid_character_apply_guard(view,ops))return 0;
    if(!view->chara||!view->body||!view->models.body||!view->models.bag||view->models.uid<=0||
       view->cid!=(LidBagString*)view->chara||view->models.levels!=(int*)(view->models.body+0x1c)||
       view->models.slots!=(LidBagArray*)(view->models.bag+0x50)||
       !ops->readable(view->chara,0x220)||!ops->readable(view->body,0x220)||
       !ops->readable(view->models.levels,40)||!ops->readable(view->common_grade,4)||
       !ops->writable(view->chara+0x14c,40)||!ops->writable(view->body+0x14c,40)||
       !ops->writable(view->models.levels,40)||!ops->writable(view->common_grade,4))return 0;
    if(*(int*)(view->chara+0x38)!=6||*(int*)(view->body+0x38)!=6||
       *view->common_grade<1||*view->common_grade>9)return 0;
    if(!lid_config_targets(config,counts,batch->absolute,batch->levels)||
       !lid_character_levels_valid(batch->levels)||batch->absolute[6]<1||batch->absolute[6]>=100||
       batch->absolute[7]<0||batch->absolute[7]>64)return 0;
    batch->view=*view;batch->ops=*ops;batch->original_grade=*view->common_grade;
    for(i=0;i<3;i++){
        existing=i==0?(int*)(view->chara+0x14c):i==1?(int*)(view->body+0x14c):view->models.levels;
        if(!lid_character_levels_valid(existing))return 0;
        for(j=0;j<10;j++){
            if(existing[j]>batch->levels[j])return 0;
            batch->original_levels[i][j]=existing[j];
        }
    }
    batch->count=0;
    batch->arrays[batch->count++]=(LidBagArray*)(view->chara+0x18c);
    if(view->body!=view->chara)batch->arrays[batch->count++]=(LidBagArray*)(view->body+0x18c);
    batch->arrays[batch->count++]=view->models.slots;
    for(i=0;i<batch->count;i++){
        LidGameBagContext* ctx=&batch->contexts[i];
        lid_bag_zero(ctx,sizeof(*ctx));
        ctx->model=i==batch->count-1;ctx->model_entry_vtable=bag_entry_vtable;
        ctx->uid=view->models.uid;ctx->cid=view->cid;
        ctx->allocate=ops->allocate;ctx->release=ops->release;ctx->readable=ops->readable;
        if(!ops->writable(batch->arrays[i],sizeof(LidBagArray)))return 0;
    }
    if(!view->cid||!lid_game_bag_string_valid(view->cid,&batch->contexts[0])||view->cid->num<=1)return 0;
    for(i=0;i<2;i++){
        cid=(LidBagString*)(i?view->body:view->chara);
        if(!lid_game_bag_string_valid(cid,&batch->contexts[0])||!lid_game_bag_string_same(cid,view->cid))return 0;
    }
    if(!lid_character_identity_ownership(batch)||
       !lid_game_bag_pawn_tail_empty(view->pawn_slots,99,ops->readable))return 0;
    batch->bag_storage_target=batch->absolute[6];
    for(i=0;i<batch->count;i++)for(j=0;j<batch->arrays[i]->num;j++){
        LidGameBagContext*ctx=&batch->contexts[i];
        void*entry=(BYTE*)batch->arrays[i]->data+(size_t)j*(ctx->model?0x48:0x44);
        if(!lid_game_bag_entry_empty(entry,ctx)&&j+1>batch->bag_storage_target)batch->bag_storage_target=j+1;
    }
    for(i=0;i<view->pawn_slots->num;i++)
        if((*((BYTE*)view->pawn_slots->data+(size_t)i*0x718)&1)&&i+1>batch->bag_storage_target)batch->bag_storage_target=i+1;
    if(!lid_game_bag_pawn_tail_empty(view->pawn_slots,batch->bag_storage_target,ops->readable)||
       ops->events_supported(ops->context,view)!=1)return 0;
    batch->original_pawn=*view->pawn_slots;
    if(!lid_game_bag_prepare_group(batch->plans,batch->arrays,batch->contexts,batch->count,batch->bag_storage_target))return 0;
    batch->prepared=1;return 1;
}
/* -1 means a mutation/event MAY already have happened; no save/ack is allowed.
   0 is pre-mutation refusal;1 is applied AND RAM readback matching this batch.
   Caller retains the view for own save/readback under a fresh reservation. */
static int lid_character_apply_readback(const LidCharacterApplication* batch){
    int i,j;const int* levels;
    if(!batch||!batch->applied||batch->bag_storage_target<batch->absolute[6]||batch->bag_storage_target>99||!lid_character_apply_guard(&batch->view,&batch->ops))return 0;
    for(i=0;i<3;i++){
        levels=i==0?(int*)(batch->view.chara+0x14c):i==1?(int*)(batch->view.body+0x14c):batch->view.models.levels;
        if(!batch->ops.readable(levels,40))return 0;
        for(j=0;j<10;j++)if(levels[j]!=batch->levels[j])return 0;
    }
    if(!batch->ops.readable(batch->view.common_grade,4)||*batch->view.common_grade!=6||
       !lid_game_bag_pawn_tail_empty(batch->view.pawn_slots,batch->bag_storage_target,batch->ops.readable)||
       batch->view.pawn_slots->num!=batch->bag_storage_target)return 0;
    for(i=0;i<batch->count;i++)if(!batch->ops.readable(batch->arrays[i],sizeof(LidBagArray))||
        batch->arrays[i]->num!=batch->bag_storage_target)return 0;
    return lid_character_identity_ownership(batch);
}
static int lid_character_apply_commit(LidCharacterApplication* batch){
    int i,j;const int* existing;const LidCharacterView* view;
    if(!batch||!batch->prepared||batch->started||batch->applied||batch->bag_storage_target<batch->absolute[6]||batch->bag_storage_target>99||
       !lid_character_apply_guard(&batch->view,&batch->ops))return 0;
    view=&batch->view;
    for(i=0;i<3;i++){
        existing=i==0?(int*)(view->chara+0x14c):i==1?(int*)(view->body+0x14c):view->models.levels;
        if(!batch->ops.readable(existing,40)||!batch->ops.writable(existing,40))return 0;
        for(j=0;j<10;j++)if(existing[j]!=batch->original_levels[i][j])return 0;
    }
    for(i=0;i<batch->count;i++)if(!batch->ops.writable(batch->arrays[i],sizeof(LidBagArray)))return 0;
    if(!batch->ops.readable(view->common_grade,4)||!batch->ops.writable(view->common_grade,4)||*view->common_grade!=batch->original_grade||
       !batch->ops.readable(view->pawn_slots,sizeof(LidBagArray))||
       !lid_bag_equal(view->pawn_slots,&batch->original_pawn,sizeof(LidBagArray))||
       !lid_game_bag_pawn_tail_empty(view->pawn_slots,batch->bag_storage_target,batch->ops.readable)||
       !lid_character_identity_ownership(batch)||
       batch->ops.events_supported(batch->ops.context,view)!=1)return 0;
    /* All replacement allocations are already prepared before the UE3 event. */
    batch->started=1;
    if(view->pawn_slots->num!=batch->bag_storage_target&&
       batch->ops.resize_pawn(batch->ops.context,view,batch->bag_storage_target)!=1){lid_character_apply_abort(batch);return -1;}
    if(!batch->ops.readable(view->pawn_slots,sizeof(LidBagArray))||view->pawn_slots->num!=batch->bag_storage_target||
       !lid_game_bag_commit_group(batch->plans,batch->arrays,batch->contexts,batch->count)){
        lid_character_apply_abort(batch);return -1;
    }
    for(i=0;i<10;i++){
        *(int*)(view->chara+0x14c+i*4)=batch->levels[i];
        *(int*)(view->body+0x14c+i*4)=batch->levels[i];
        view->models.levels[i]=batch->levels[i];
    }
    *view->common_grade=6;batch->prepared=0;batch->applied=1;
    if(batch->ops.recalculate(batch->ops.context,view)!=1)return -1;
    return lid_character_apply_readback(batch)?1:-1;
}
#endif
