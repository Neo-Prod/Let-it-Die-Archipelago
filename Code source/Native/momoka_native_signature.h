#ifndef LID_RESEARCH_MOMOKA_NATIVE_R205_H
#define LID_RESEARCH_MOMOKA_NATIVE_R205_H
#include "momoka_metadata_reader.h"
/* Source-only READ adapter, current observed build only. Caller proves exact
   GetSkill membership in BrgDatabase.Children and stable snapshot lifetime.
   RootSet/Struct identity checked from R210 observation; not install authorization. */
typedef struct M205Native {uint64_t function,slot,original;uint32_t flags;} M205Native;
static int m205_signature(const M195Ops*o,uint64_t function,uint64_t owner,
 uint64_t expected_exec,M205Native*out){
 M205Native v={0};uint64_t cls,child,next,seen[512],pf,exec_after,object_flags,structure;
 uint32_t extent,dim,size,at,mask;uint16_t index,parms,ret;
 unsigned count=0,found=0;
 const char*names[3]={"Id","skl","ReturnValue"};
 const char*types[3]={"StrProperty","StructProperty","BoolProperty"};
 const uint32_t sizes[3]={16,196,4},offsets[3]={0,16,212};
 const uint64_t property_flags[3]={0x400080,0x400180,0x580};
 if(!out||!expected_exec||!m195_named(o,owner,"BrgDatabase")||!m195_named(o,function,"GetSkill")||
 !m195_read(o,function,0x10,&object_flags,8)||!(object_flags&0x4000)||
 !m195_read(o,owner,0x10,&object_flags,8)||!(object_flags&0x4000)||
 !m195_read(o,function,0x50,&cls,8)||!m195_named(o,cls,"Function")||
 !m195_read(o,function,0xd0,&v.flags,4)||v.flags!=0x420400||
 !m195_read(o,function,0xd4,&index,2)||index||
 !m195_read(o,function,0x88,&extent,4)||extent<216||extent>65536||
 !m195_read(o,function,0xe2,&parms,2)||parms!=216||
 !m195_read(o,function,0xe4,&ret,2)||ret!=212||
 !m195_read(o,function,0xf0,&v.original,8)||v.original!=expected_exec||
 function>UINT64_MAX-0xf0||((function+0xf0)&7)||
 !m195_read(o,function,0x80,&child,8))return 0;
 while(child){
  unsigned i=3;
  if(count==512)return 0;for(unsigned j=0;j<count;j++)if(seen[j]==child)return 0;seen[count++]=child;
  if(!m195_read(o,child,0x60,&next,8)||!m195_read(o,child,0x70,&pf,8))return 0;
  if(pf&0x80){
   for(unsigned j=0;j<3;j++)if(m195_named(o,child,names[j])){i=j;break;}
   if(i==3||(found&(1u<<i))||pf!=property_flags[i]||
    !m195_read(o,child,0x50,&cls,8)||!m195_named(o,cls,types[i])||
    !m195_read(o,child,0x68,&dim,4)||dim!=1||
    !m195_read(o,child,0x6c,&size,4)||size!=sizes[i]||
    !m195_read(o,child,0x94,&at,4)||at!=offsets[i])return 0;
   if(i==2&&(!m195_read(o,child,0xb0,&mask,4)||mask!=1))return 0;
   if(i==1&&(!m195_read(o,child,0xb0,&structure,8)||!m195_named(o,structure,"BrgDbSkill")||
    !m195_read(o,structure,0x50,&cls,8)||!m195_named(o,cls,"ScriptStruct")))return 0;
   found|=1u<<i;
  }child=next;
 }
 if(found!=7||!m195_read(o,function,0xf0,&exec_after,8)||exec_after!=v.original)return 0;
 v.function=function;v.slot=function+0xf0;*out=v;return 1;
}
#endif
