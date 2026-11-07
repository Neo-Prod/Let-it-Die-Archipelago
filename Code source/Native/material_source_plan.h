#ifndef LID_RESEARCH_MATERIAL_SOURCE_R83_H
#define LID_RESEARCH_MATERIAL_SOURCE_R83_H
#include "material_record_reader.h"
#include <string.h>
/* Research only. No engine calls, writes, installation or acquisition actions. */
enum {R83_NONE,R83_ENTITY,R83_POINT};
typedef struct R83SourcePlan {
    uint8_t create_type,action_material,kind;
    char entity[96],unit[96],point[96];
} R83SourcePlan;
/* Bind the inherited Treasure field; never guess its runtime offset. */
static int r83_point_field(const R6Ops*ops,uint64_t cls,R6Field*out){
    uint64_t seen[32],props[512],owner=0,next,child;unsigned n=0,used=0,found=0;
    R6Field field={0};
    if(!out)return 0;
    while(cls){
        if(n==32)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
        if(r6_name(ops,cls,"BrgActor_Treasure")){if(owner)return 0;owner=cls;}
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!owner||!r6_at(ops,owner,0x80,&child,8))return 0;
    while(child){
        uint64_t type;uint32_t dim,size,off;
        if(used==512)return 0;for(unsigned i=0;i<used;i++)if(props[i]==child)return 0;props[used++]=child;
        if(!r6_at(ops,child,0x60,&next,8))return 0;
        if(r6_name(ops,child,"mDbTreasureTargetPointId")){
            if(found++||!r6_at(ops,child,0x50,&type,8)||!r6_name(ops,type,"StrProperty")||
               !r6_at(ops,child,0x68,&dim,4)||dim!=1||!r6_at(ops,child,0x6c,&size,4)||size!=16||
               !r6_at(ops,child,0x94,&off,4)||off<0x60||(uint64_t)off+size>0x4cc)return 0;
            field=(R6Field){off,size,0};
        }
        child=next;
    }
    if(found!=1)return 0;*out=field;return 1;
}
/* Input sample must be re-read and identity-checked by the transaction owner.
   Failure leaves output untouched and is NOT permission for inventory fallback. */
static int r83_source_plan(const R6Ops*ops,const R81Sample*s,R83SourcePlan*out){
    R83SourcePlan p={0};uint64_t record;uint32_t flags;char point[96];R6Field f;
    if(!s||!out||!s->manager||!s->actor||s->slot<0||s->slot>=R81_COUNT||
       s->manager>UINT64_MAX-R81_EXTENT)return 0;
    record=s->manager+0x74+(uint64_t)(uint32_t)s->slot*R81_STRIDE;
    if(!r6_at(ops,record,0x88,&p.create_type,1)||p.create_type>4||
       !r6_at(ops,record,0xcc,&flags,4))return 0;
    p.action_material=(uint8_t)(flags&1);
    if(p.create_type==0){
        if(p.action_material){
            size_t n=0;while(n<sizeof(s->entity_id)&&s->entity_id[n])n++;
            if(n==sizeof(s->entity_id))return 0;
            if(n&&s->entity_id[0]!='-'){p.kind=R83_ENTITY;memcpy(p.entity,s->entity_id,n+1);}
        }else{
            if(!r83_point_field(ops,s->actor_class,&f)||
               s->actor>UINT64_MAX-f.offset||!r81_id(ops,s->actor+f.offset,point,sizeof(point),0))return 0;
            char*dot=strchr(point,'.');
            /* Native UpdateUserData false is ignored by its pickup caller.
               No separator therefore means no world mark, not pickup denial. */
            if(dot){size_t n=(size_t)(dot-point);p.kind=R83_POINT;
                memcpy(p.unit,point,n);p.unit[n]=0;strcpy(p.point,dot+1);}
        }
    }
    *out=p;return 1;
}
static int r83_source_equal(const R83SourcePlan*a,const R83SourcePlan*b){
    return a&&b&&a->create_type==b->create_type&&a->action_material==b->action_material&&
        a->kind==b->kind&&!strcmp(a->entity,b->entity)&&!strcmp(a->unit,b->unit)&&!strcmp(a->point,b->point);
}
#endif
