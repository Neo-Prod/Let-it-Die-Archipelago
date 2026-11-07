#ifndef LID_PROGRESSION_MODELS_H
#define LID_PROGRESSION_MODELS_H
/* Include progression_game_bag.h first. Current offline build only.
   Read-only bounded lookup: never invokes a native get-or-create function.
   Caller must hold its engine reservation for any mutable use of the result. */
typedef struct LidCharacterModelRoots {
    void* body_repository;void* bag_repository;
    void* body_repository_vtable;void* body_group_vtable;void* body_row_vtable;
    void* bag_repository_vtable;void* bag_group_vtable;void* bag_row_vtable;
    int uid;const LidBagString* cid;
    int (WINAPI *readable)(const void*,size_t);
} LidCharacterModelRoots;
typedef struct LidCharacterModels {
    BYTE* body;BYTE* bag;
    int* levels;LidBagArray* slots;
    int uid;
} LidCharacterModels;
static int lid_model_array(const LidBagArray* array,size_t stride,
                            int (WINAPI *readable)(const void*,size_t)){
    if(!array||!readable||!readable(array,sizeof(*array))||!stride)return 0;
    if(array->num<0||array->capacity<array->num||array->capacity>64)return 0;
    if((!array->data)!=(array->capacity==0))return 0;
    return !array->num||readable(array->data,(size_t)array->num*stride);
}
static int lid_model_string_valid(const LidBagString* s,const LidCharacterModelRoots* roots){
    LidGameBagContext context={0};context.readable=roots->readable;
    return s&&lid_game_bag_string_valid(s,&context);
}
static int lid_model_uid_matches(const LidBagString* key,int uid){
    wchar_t reversed[10];int n=0,i;unsigned value;
    if(uid<=0)return 0;value=(unsigned)uid;
    do{reversed[n++]=(wchar_t)('0'+value%10);value/=10;}while(value);
    if(key->num!=n+1)return 0;
    for(i=0;i<n;i++)if(key->data[i]!=reversed[n-1-i])return 0;
    return key->data[n]==0;
}
/* 1=exactly one row in each model,0=not available,-1=invalid/ambiguous.
   Output remains unchanged on refusal. All matching groups are inspected so
   duplicate keys cannot silently select the first character. */
static int lid_character_models_resolve(LidCharacterModels* out,const LidCharacterModelRoots* roots){
    LidCharacterModels found={0};int model,g,r,hits[2]={0,0};
    if(!out||!roots||!roots->readable||roots->uid<=0||!roots->cid||
       !lid_model_string_valid(roots->cid,roots)||roots->cid->num<=1)return -1;
    for(model=0;model<2;model++){
        BYTE* repository=model?roots->bag_repository:roots->body_repository;
        void* repository_vt=model?roots->bag_repository_vtable:roots->body_repository_vtable;
        void* group_vt=model?roots->bag_group_vtable:roots->body_group_vtable;
        void* row_vt=model?roots->bag_row_vtable:roots->body_row_vtable;
        size_t group_stride=model?0x28:0x70,row_stride=model?0x70:0x5c;
        LidBagArray* groups;
        if(!repository)return 0;
        if(!repository_vt||!group_vt||!row_vt||!roots->readable(repository,0x18)||
           *(void**)repository!=repository_vt)return -1;
        groups=(LidBagArray*)(repository+8);
        if(!lid_model_array(groups,group_stride,roots->readable))return -1;
        for(g=0;g<groups->num;g++){
            BYTE* group=(BYTE*)groups->data+(size_t)g*group_stride;
            LidBagString* uid=(LidBagString*)(group+(model?0x18:0x60));LidBagArray* rows;
            if(*(void**)group!=group_vt||!lid_model_string_valid(uid,roots))return -1;
            if(!lid_model_uid_matches(uid,roots->uid))continue;
            rows=(LidBagArray*)(group+(model?8:0x50));
            if(!lid_model_array(rows,row_stride,roots->readable))return -1;
            for(r=0;r<rows->num;r++){
                BYTE* row=(BYTE*)rows->data+(size_t)r*row_stride;
                LidBagString* cid=(LidBagString*)(row+(model?0x60:0xc));
                if(*(void**)row!=row_vt||!lid_model_string_valid(cid,roots))return -1;
                if(!lid_game_bag_string_same(cid,roots->cid))continue;
                if(++hits[model]>1)return -1;
                if(model){found.bag=row;found.slots=(LidBagArray*)(row+0x50);}
                else{
                    if(*(int*)(row+8)!=roots->uid)return -1;
                    found.body=row;found.levels=(int*)(row+0x1c);
                }
            }
        }
    }
    if(hits[0]!=1||hits[1]!=1)return 0;
    found.uid=roots->uid;*out=found;return 1;
}
#endif
