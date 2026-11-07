#ifndef LID_KILL_COIN_PICKUP_BINDING_R146_H
#define LID_KILL_COIN_PICKUP_BINDING_R146_H
#include "material_ui_graph.h"
/* READ ONLY. Exact native offline carried-income model, not frontend body.
   Provenance KC_R145_Authority_Layout.json. No engine calls or mutations. */
typedef struct R146String {uint64_t data;int32_t num,max;} R146String;
typedef struct R146Authority {uint64_t repository,group,row;int32_t money;} R146Authority;
static int r146_array(const R6Ops*o,uint64_t at,uint64_t*data,int32_t*num){
 int32_t max;if(!r6_at(o,at,0,data,8)||!r6_at(o,at,8,num,4)||!r6_at(o,at,12,&max,4)||
 *num<0||max<*num||max>64||((!*data)!=(max==0)))return 0;return 1;
}
static int r146_string(const R6Ops*o,uint64_t at,R146String*out){
 R146String s;uint16_t ch;
 if(!r6_copy(o,at,&s,16)||s.num<2||s.num>96||s.max<s.num||s.max>256||!s.data)return 0;
 for(int i=0;i<s.num;i++)if(!r6_at(o,s.data,(uint32_t)i*2,&ch,2)||(i==s.num-1?ch!=0:ch==0))return 0;
 *out=s;return 1;
}
static int r146_uid(const R6Ops*o,const R146String*s,uint32_t uid){
 uint16_t digits[10],ch;unsigned n=0;if(!uid||uid>2147483647u)return 0;
 do{digits[n++]=(uint16_t)('0'+uid%10);uid/=10;}while(uid);
 if(s->num!=(int32_t)n+1)return 0;
 for(unsigned i=0;i<n;i++)if(!r6_at(o,s->data,i*2,&ch,2)||ch!=digits[n-i-1])return 0;return 1;
}
static int r146_same_string(const R6Ops*o,const R146String*a,const R146String*b){
 uint16_t x,y;if(a->num!=b->num)return 0;
 for(int32_t i=0;i<a->num;i++)if(!r6_at(o,a->data,(uint32_t)i*2,&x,2)||!r6_at(o,b->data,(uint32_t)i*2,&y,2)||x!=y)return 0;return 1;
}
/* 1 unique match, 0 absent, -1 invalid/duplicate. Output unchanged on refusal.
   CID argument is an address of a validated current-character FString, not a
   copied/borrowed server identifier. Caller proves current UID/CID/ownership. */
static int r146_authority(const R6Ops*o,uint64_t image,uint32_t uid,uint64_t cid,R146Authority*out){
 R146Authority v={0};R146String wanted,key;uint64_t groups,rows,vt;int32_t ng,nr;unsigned matches=0,uid_groups=0;
 if(!out||!image||image>UINT64_MAX-0x27e4d80||!uid||uid>2147483647u||!r146_string(o,cid,&wanted))return -1;
 v.repository=image+0x27e4d68;
 if(!r6_copy(o,v.repository,&vt,8)||vt!=image+0x2186a90||!r146_array(o,v.repository+8,&groups,&ng))return -1;
 for(int32_t g=0;g<ng;g++){
  if(groups>UINT64_MAX-(uint64_t)g*0x70-0x70)return -1;uint64_t group=groups+(uint64_t)g*0x70;
  if(!r6_copy(o,group,&vt,8)||vt!=image+0x21869d0||!r146_string(o,group+0x60,&key))return -1;
  if(!r146_uid(o,&key,uid))continue;
  if(++uid_groups>1)return -1;
  if(!r146_array(o,group+0x50,&rows,&nr))return -1;
  for(int32_t r=0;r<nr;r++){
   if(rows>UINT64_MAX-(uint64_t)r*0x150-0x150)return -1;uint64_t row=rows+(uint64_t)r*0x150;
   if(!r6_copy(o,row,&vt,8)||vt!=image+0x21868d0||!r146_string(o,row+0xc,&key))return -1;
   if(!r146_same_string(o,&wanted,&key))continue;
   if(++matches>1||!r6_at(o,row,0x54,&v.money,4)||v.money<0)return -1;v.group=group;v.row=row;
  }
 }
 if(!matches)return 0;*out=v;return 1;
}
/* Fixed pickup lifecycle signatures; offsets are DISCOVERED from metadata.
   No engine dispatch here. In particular, no guessed Spawn argument layout. */
enum {R146_SPAWN,R146_DESTROY,R146_GET_MONEY,R146_MESSAGE,R146_EVENT};
typedef struct R146Signature {uint64_t function;uint32_t offset[8],mask[8];uint16_t size,ret;unsigned count;} R146Signature;
static const char*const r146_spawn_names[8]={"SpawnClass","SpawnOwner","SpawnTag","SpawnLocation","SpawnRotation","ActorTemplate","bNoCollisionFail","ReturnValue"};
static const char*const r146_spawn_types[8]={"ClassProperty","ObjectProperty","NameProperty","StructProperty","StructProperty","ObjectProperty","BoolProperty","ObjectProperty"};
static const unsigned r146_spawn_sizes[8]={8,8,8,12,12,8,4,8};
static const char*const r146_event_names[3]={"event_type","Value","additionalValue"};
static unsigned r146_count(unsigned kind){return kind==R146_SPAWN?8:kind==R146_EVENT?3:kind<=R146_MESSAGE?1:0;}
static const char*r146_param_name(unsigned kind,unsigned i){
 return kind==R146_SPAWN?r146_spawn_names[i]:kind==R146_DESTROY?"ReturnValue":kind==R146_GET_MONEY?"inMoney":kind==R146_MESSAGE?"inAddCount":r146_event_names[i];
}
static const char*r146_param_type(unsigned kind,unsigned i){return kind==R146_SPAWN?r146_spawn_types[i]:kind==R146_DESTROY?"BoolProperty":"IntProperty";}
static unsigned r146_param_size(unsigned kind,unsigned i){return kind==R146_SPAWN?r146_spawn_sizes[i]:4;}
static int r146_is_return(unsigned kind,unsigned i){return (kind==R146_SPAWN&&i==7)||kind==R146_DESTROY;}
static int r146_signature(const R6Ops*o,uint64_t cls,unsigned kind,R146Signature*out){
 static const char*const owners[5]={"Actor","Actor","BrgActor_Coin","BrgUIManager","BrgActor_Coin"};
 static const char*const functions[5]={"Spawn","Destroy","GetMoney","AddMessage","SetEventValueData"};
 R146Signature s={0};R132FieldWalk walk={0};uint64_t owner,child,next,type,flags;unsigned functions_found=0,found=0;uint32_t function_flags;
 if(!out||kind>R146_EVENT||!r87_owner(o,cls,owners[kind],&owner)||!r6_at(o,owner,0x80,&child,8))return 0;
 while(child){
  if(!r132_field_visit(&walk,child)||!r6_at(o,child,0x60,&next,8))return 0;
  if(r6_name(o,child,functions[kind])){if(functions_found++||!r6_at(o,child,0x50,&type,8)||!r6_name(o,type,"Function"))return 0;s.function=child;}child=next;
 }
 if(functions_found!=1||!r6_at(o,s.function,0xd0,&function_flags,4)||((function_flags&0x400)!=0)!=(kind<=R146_DESTROY)||
 !r6_at(o,s.function,0xe2,&s.size,2)||!s.size||s.size>128||!r6_at(o,s.function,0xe4,&s.ret,2)||!r6_at(o,s.function,0x80,&child,8))return 0;
 s.count=r146_count(kind);R132FieldWalk params={0};unsigned visited=0;
 while(child){
  if(++visited>64||!r132_field_visit(&params,child)||!r6_at(o,child,0x60,&next,8)||!r6_at(o,child,0x70,&flags,8))return 0;
  unsigned i;for(i=0;i<s.count;i++)if(r6_name(o,child,r146_param_name(kind,i)))break;
  if(!(flags&0x80)){if(i<s.count)return 0;child=next;continue;}
  uint32_t dim,size,at,mask=0;
  if(i==s.count||(found&(1u<<i))||((flags&0x400)!=0)!=r146_is_return(kind,i)||(!r146_is_return(kind,i)&&(flags&0x100))||
  !r6_at(o,child,0x50,&type,8)||!r6_name(o,type,r146_param_type(kind,i))||
  !r6_at(o,child,0x68,&dim,4)||dim!=1||!r6_at(o,child,0x6c,&size,4)||size!=r146_param_size(kind,i)||
  !r6_at(o,child,0x94,&at,4)||size>s.size||at>s.size-size)return 0;
  if(!strcmp(r146_param_type(kind,i),"BoolProperty")&&(!r6_at(o,child,0xb0,&mask,4)||!mask||(mask&(mask-1))))return 0;
  s.offset[i]=at;s.mask[i]=mask;found|=1u<<i;child=next;
 }
 if(found!=((1u<<s.count)-1))return 0;
 if(kind<=R146_DESTROY){unsigned i=kind==R146_SPAWN?7:0;if(s.ret!=s.offset[i])return 0;}
 else{
  /* Generated native event dispatchers use closed packed int32 frames. */
  if(s.ret!=0xffff||s.size!=4*s.count)return 0;
  for(unsigned i=0;i<s.count;i++)if(s.offset[i]!=4*i)return 0;
 }
 for(unsigned i=0;i<s.count;i++)for(unsigned j=0;j<i;j++){
  if(s.offset[i]<s.offset[j]+r146_param_size(kind,j)&&s.offset[j]<s.offset[i]+r146_param_size(kind,i)&&
  !(s.mask[i]&&s.mask[j]&&s.offset[i]==s.offset[j]&&!(s.mask[i]&s.mask[j])))return 0;
 }
 *out=s;return 1;
}
#endif
