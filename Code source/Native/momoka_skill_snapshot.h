#ifndef LID_RESEARCH_MOMOKA_SKILL_R197_H
#define LID_RESEARCH_MOMOKA_SKILL_R197_H
#include "momoka_context_reader.h"
/* UNCOMPILED research. Not a hook; stable copied memory required by caller. */
typedef struct M197StringHeader {uint64_t data;int32_t count,capacity;} M197StringHeader;
typedef struct M197SkillSnapshot {M196Context context;uint16_t skill[128];unsigned length;} M197SkillSnapshot;
static int m197_string(const M195Ops*o,uint64_t locals,const M195Signature*s,
 uint16_t*out,unsigned*length){
 M197StringHeader h={0},after={0};uint16_t value[128];unsigned n;
 if(!s||!out||!length||s->skill_offset>s->parms_size||
 s->parms_size-s->skill_offset<16||!m195_read(o,locals,s->skill_offset,&h,16)||
 !h.data||h.count<2||h.count>128||h.capacity<h.count||h.capacity>1048576)return 0;
 n=(unsigned)h.count;
 if(!m195_read(o,h.data,0,value,n*2)||value[n-1]!=0)return 0;
 for(unsigned i=0;i<n-1;i++){
  /* IDs are restricted to printable ASCII here, not user-facing names.
     Unexpected encoding refuses observation, never alters game behavior. */
  if(value[i]<0x21||value[i]>0x7e)return 0;
 }
 if(!m195_read(o,locals,s->skill_offset,&after,16)||h.data!=after.data||
 h.count!=after.count||h.capacity!=after.capacity)return 0;
 for(unsigned i=0;i<n;i++)out[i]=value[i];*length=n-1;return 1;
}
static int m197_capture(const M195Ops*o,const M196Binding*b,uint64_t frame,M197SkillSnapshot*out){
 M197SkillSnapshot s={0};M196Context after={0};const M195Signature*parent;
 uint16_t other[128];unsigned length=0;
 if(!out||!m196_context(o,b,frame,&s.context))return 0;
 if(s.context.kind==M196_PRICE){
  if(!m197_string(o,s.context.locals,&b->buy,s.skill,&s.length))return 0;
 }else{
  parent=s.context.kind==M196_BUY_ADD?&b->buy:
   s.context.kind==M196_SELL_ADD?&b->sell:&b->exchange;
  if(!m197_string(o,s.context.locals,&b->add,s.skill,&s.length)||
   !m197_string(o,s.context.parent_locals,parent,other,&length)||length!=s.length)return 0;
  for(unsigned i=0;i<=length;i++)if(s.skill[i]!=other[i])return 0;
 }
 if(!m196_context(o,b,frame,&after)||after.node!=s.context.node||
 after.parent!=s.context.parent||after.parent_node!=s.context.parent_node||after.locals!=s.context.locals||
 (s.context.kind!=M196_PRICE&&(after.parent_locals!=s.context.parent_locals||after.num!=s.context.num)))return 0;
 *out=s;return 1;
}
#endif
