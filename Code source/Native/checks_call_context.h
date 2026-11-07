#ifndef LID_RESEARCH_CALL_CONTEXT_H
#define LID_RESEARCH_CALL_CONTEXT_H
#include "checks_observer_metadata.h"
/* The expected function must be bound from the
   BrgPawn_Base owner, not accepted just because its name is SetDeath. */
static int r9_death_properties(const R6Ops*ops,uint64_t function){
    static const char*names[4]={"ReturnValue","TouchActor","TouchEvents","Index"};
    static const char*types[4]={"BoolProperty","ObjectProperty","ArrayProperty","IntProperty"};
    static const unsigned sizes[4]={4,8,16,4};
    uint64_t child,next,seen[4];R6Field fields[4];unsigned count=0,found=0;
    if(!r6_at(ops,function,0x80,&child,8))return 0;
    while(child){
        unsigned index=4;
        if(count==4)return 0;
        for(unsigned i=0;i<count;i++)if(seen[i]==child)return 0;
        seen[count++]=child;
        for(unsigned i=0;i<4;i++)if(r6_name(ops,child,names[i])){index=i;break;}
        if(index==4||(found&(1u<<index))||!r6_property(ops,child,types[index],sizes[index],&fields[index]))return 0;
        if(index==0&&(fields[0].offset||fields[0].mask!=1))return 0;
        if(index&&fields[index].offset<4)return 0;
        for(unsigned i=0;i<4;i++)if((found&(1u<<i))&&
           fields[index].offset<fields[i].offset+fields[i].size&&fields[i].offset<fields[index].offset+fields[index].size)return 0;
        found|=1u<<index;
        if(!r6_at(ops,child,0x60,&next,8))return 0;child=next;
    }
    return (found&1u)!=0;
}
static int r7_bind_death_function(const R6Ops*ops,uint64_t object,uint64_t expected_vtable,uint64_t*out){
    R6Layout layout;uint64_t cls,seen[32],owner=0,child,next,found=0;unsigned n=0,fields=0;
    if(!out||!r6_bind(ops,object,expected_vtable,&layout))return 0;
    cls=layout.object_class;
    while(cls){
        if(n==32)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
        if(r6_name(ops,cls,"BrgPawn_Base")){if(owner)return 0;owner=cls;}
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!owner||!r6_at(ops,owner,0x80,&child,8))return 0;
    while(child){
        if(++fields>4096||!r6_at(ops,child,0x60,&next,8))return 0;
        if(r6_name(ops,child,"SetDeath")){
            uint64_t type;uint32_t flags;
            if(found||!r6_at(ops,child,0x50,&type,8)||!r6_name(ops,type,"Function")||
               !r6_at(ops,child,0xd0,&flags,4)||(flags&0x400u)||
               !r9_death_properties(ops,child))return 0;
            found=child;
        }
        child=next;
    }
    if(!found)return 0;*out=found;return 1;
}
typedef struct R7CallContext {uint64_t object,node,origin;} R7CallContext;
static int r7_call_context(const R6Ops*ops,uint64_t frame,uint64_t object,
                          uint64_t origin,uint64_t expected_origin,uint64_t bound_node,R7CallContext*out){
    R7CallContext result={0};uint64_t receiver;
    /* Gate FIRST: RBX from any other caller is not a proven FFrame pointer. */
    if(!out||!expected_origin||origin!=expected_origin||!object||!bound_node||
       !r6_at(ops,frame,0x14,&result.node,8)||result.node!=bound_node||
       !r6_at(ops,frame,0x1c,&receiver,8)||receiver!=object)return 0;
    result.object=object;result.origin=origin;*out=result;return 1;
}
/* A diagnostic label of a proven call path, not an AP check or death counter.
   The known script body sets mbDeath BEFORE UpdateHealth(1), so both samples
   can already be dead. No fabricated previous-alive observation is required. */
static int r7_accepted_setdeath_label(const R7CallContext*ctx,int new_health,
                                    const R6Sample*before,const R6Sample*after){
    return ctx&&before&&after&&ctx->object&&ctx->node&&ctx->origin&&new_health==1&&
      before->object==ctx->object&&after->object==ctx->object&&
      before->object_class==after->object_class&&before->name_index==after->name_index&&
      before->name_number==after->name_number&&before->dead&&after->dead&&
      !before->removing&&!after->removing&&before->kind==after->kind&&
      (after->kind==1||after->kind==3)&&after->max_health>=1&&after->health==1;
}
#endif
