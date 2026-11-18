#ifndef LID_RESEARCH_PREPARE_OBSERVER_R212_H
#define LID_RESEARCH_PREPARE_OBSERVER_R212_H
#include "momoka_native_signature.h"
#include "momoka_host_gate.h"
/* Read-only preparation core; no slot writes. Verified build, current game
   lifetime and caller/thread/session are preconditions supplied by runtime.
   Output must be ZERO initialized at final resident address, never copied after init. */
typedef struct M212Prepared {M211Host gate;M204Host host;M205Native native;} M212Prepared;
static int m212_member(const M195Ops*o,uint64_t object,const char*owner_name,const char*field,
 uint64_t*out){
 uint64_t cls,next,owner=0,seen[64],child,fields[4096],found=0;unsigned n=0,count=0;
 if(!out||!m195_read(o,object,0x50,&cls,8))return 0;
 while(cls){
  if(n==64)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
  if(m195_named(o,cls,owner_name)){if(owner)return 0;owner=cls;}
  if(!m195_read(o,cls,0x78,&next,8))return 0;cls=next;
 }
 if(!owner||!m195_read(o,owner,0x80,&child,8))return 0;
 while(child){
  if(count==4096)return 0;for(unsigned i=0;i<count;i++)if(fields[i]==child)return 0;fields[count++]=child;
  if(m195_named(o,child,field)){if(found)return 0;found=child;}
  if(!m195_read(o,child,0x60,&next,8))return 0;child=next;
 }
 if(!found)return 0;*out=found;return 1;
}
/* Caller already has current Database receiver. No cached database instance.
   owner must be exact class reached by this receiver's ancestor chain. */
static int m212_prepare(M212Prepared*out,const M195Ops*memory,uint64_t database,uint64_t expected_exec,
 void*provider,uint32_t owner_thread,uint32_t(*thread)(void*),int(*begin)(void*),void(*end)(void*),
 uint32_t(*get_error)(void*),void(*set_error)(void*,uint32_t)){
 uint64_t function,cls,owner=0,next,seen[64];unsigned n=0;M205Native native={0};
 if(!out||out->host.original||out->native.slot||!memory||!owner_thread||!thread||thread(provider)!=owner_thread||!begin||!end||
 !m212_member(memory,database,"BrgDatabase","GetSkill",&function)||!m195_read(memory,database,0x50,&cls,8))return 0;
 while(cls){
  if(n==64)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
  if(m195_named(memory,cls,"BrgDatabase")){if(owner)return 0;owner=cls;}
  if(!m195_read(memory,cls,0x78,&next,8))return 0;cls=next;
 }
 if(!owner||!m205_signature(memory,function,owner,expected_exec,&native))return 0;
 /* Initialization only, no reinitialization after publication permitted. */
 *out=(M212Prepared){0};out->native=native;out->gate.memory=*memory;out->gate.provider=provider;
 out->gate.owner=owner_thread;out->gate.thread=thread;out->gate.scope_begin=begin;out->gate.scope_end=end;
 out->host.context=&out->gate;out->host.original=(M204Exec)(uintptr_t)native.original;
 out->host.enter=m211_enter;out->host.leave=m211_leave;out->host.record=m211_record;
 /* Error callbacks receive gate context, unlike scope/thread provider.
    Runtime adapters must explicitly unwrap it; do not pass raw-provider API. */
 out->host.get_error=get_error;out->host.set_error=set_error;return 1;
}
#endif
