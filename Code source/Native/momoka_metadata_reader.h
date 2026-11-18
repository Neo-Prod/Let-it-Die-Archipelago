#ifndef LID_RESEARCH_MOMOKA_METADATA_R195_H
#define LID_RESEARCH_MOMOKA_METADATA_R195_H
/* Uncompiled research prototype. No hooks, native calls, allocations or writes
   to observed memory. Caller must guarantee snapshot lifetime/build identity. */
#include <stdint.h>
#include <stddef.h>
typedef struct M195Ops {
 void *context;
 int (*copy)(void*,uint64_t,void*,size_t);
 int (*name_is)(void*,uint64_t,const char*);
} M195Ops;
typedef struct M195Signature {
 uint64_t function,owner;
 uint32_t flags,extent,skill_offset,num_offset,return_offset;
 uint16_t parms_size;
 unsigned has_num,has_return;
} M195Signature;
static int m195_read(const M195Ops*o,uint64_t p,uint32_t at,void*out,size_t n){
 return o&&o->copy&&p&&out&&n&&p<=UINT64_MAX-at&&
 p+at<=UINT64_MAX-n&&o->copy(o->context,p+at,out,n);
}
static int m195_named(const M195Ops*o,uint64_t p,const char*s){
 return o&&o->name_is&&p&&o->name_is(o->context,p,s);
}
/* function is a candidate supplied by a separate bounded class walk.
   Caller proves function membership in owner's Children chain; this routine
   does not guess UObject.Outer offsets. No mutable engine name resolver. */
static int m195_signature(const M195Ops*o,uint64_t function,uint64_t owner,
 const char*name,unsigned add,M195Signature*out){
 M195Signature s={0};uint64_t cls,child,next,seen[512];
 uint32_t at[3]={0},sz[3]={16,4,4};unsigned n=0,found=0;
 uint16_t ret;uint64_t flags;
 if(!out||!m195_named(o,owner,"BrgUIMenu_SkillExchange")||
 !m195_named(o,function,name)||
 !m195_read(o,function,0x50,&cls,8)||!m195_named(o,cls,"Function")||
 !m195_read(o,function,0xd0,&s.flags,4)||(s.flags&0x400)||
 !m195_read(o,function,0x88,&s.extent,4)||!s.extent||s.extent>65536||
 !m195_read(o,function,0xe2,&s.parms_size,2)||!s.parms_size||s.parms_size>s.extent||
 !m195_read(o,function,0xe4,&ret,2)||!m195_read(o,function,0x80,&child,8))return 0;
 while(child){
  unsigned index=3;uint32_t dim,element,offset;
  if(n==512)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==child)return 0;
  seen[n++]=child;
  if(!m195_read(o,child,0x60,&next,8)||!m195_read(o,child,0x70,&flags,8))return 0;
  if(flags&0x80){
   if(m195_named(o,child,"SkillID"))index=0;
   else if(add&&m195_named(o,child,"Num"))index=1;
   else if(add&&m195_named(o,child,"ReturnValue"))index=2;
   if(index==3||(found&(1u<<index))||((flags&0x400)!=0)!=(index==2)||((flags&0x100)&&index!=2)||
    !m195_read(o,child,0x50,&cls,8)||!m195_named(o,cls,index?"IntProperty":"StrProperty")||
    !m195_read(o,child,0x68,&dim,4)||dim!=1||
    !m195_read(o,child,0x6c,&element,4)||element!=sz[index]||
    !m195_read(o,child,0x94,&offset,4)||element>s.parms_size||offset>s.parms_size-element)return 0;
   for(unsigned i=0;i<3;i++)if((found&(1u<<i))&&offset<at[i]+sz[i]&&at[i]<offset+element)return 0;
   at[index]=offset;found|=1u<<index;
  }
  child=next;
 }
 if(found!=(add?7u:1u)||(add&&ret!=at[2]))return 0;
 s.function=function;s.owner=owner;s.skill_offset=at[0];s.num_offset=at[1];
 s.return_offset=at[2];s.has_num=add!=0;s.has_return=add!=0;*out=s;return 1;
}
#endif
