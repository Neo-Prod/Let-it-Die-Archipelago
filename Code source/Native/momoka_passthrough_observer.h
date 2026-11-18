#ifndef LID_RESEARCH_MOMOKA_PASSTHROUGH_R204_H
#define LID_RESEARCH_MOMOKA_PASSTHROUGH_R204_H
#include "momoka_skill_snapshot.h"
/* Source-only synchronous adapter. NOT installed, compiled or game-tested.
   Host callbacks must not throw, fault, reenter engine, block or flush logs.
   All writes are to host-owned output/guard data, never game state. */
typedef void (*M204Exec)(void*,void*,void*);
typedef struct M204Host {
 void*context;M204Exec original;
 /* Host verifies build, caller/receiver/thread and snapshot lifetime; enter
    supplies a private verified binding, plus a reentrancy/read-scope guard.
    It must close partial guards internally on failed entry. */
 int (*enter)(void*,uint64_t,uint64_t,M195Ops*,M196Binding*);
 void (*leave)(void*);
 /* Must copy output synchronously into bounded owned storage; never retain
    addresses as deferred-call targets. No disk/network/engine calls here. */
 void (*record)(void*,const M197SkillSnapshot*);
 uint32_t (*get_error)(void*);
 void (*set_error)(void*,uint32_t);
} M204Host;
static void m204_observe_then_original(const M204Host*h,void*receiver,void*frame,void*result){
 M195Ops memory={0};M196Binding binding={0};M197SkillSnapshot snapshot={0};
 uint32_t error=0;unsigned preserve=0;
 /* Host installation must guarantee non-null original. This source adapter
    cannot synthesize native behavior if its original pointer is absent. */
 if(!h||!h->original)return;
 if(h->get_error&&h->set_error){error=h->get_error(h->context);preserve=1;}
 if(h->enter&&h->leave&&h->record&&
    h->enter(h->context,(uint64_t)(uintptr_t)receiver,(uint64_t)(uintptr_t)frame,&memory,&binding)){
  if(m197_capture(&memory,&binding,(uint64_t)(uintptr_t)frame,&snapshot))h->record(h->context,&snapshot);
  h->leave(h->context);
 }
 if(preserve)h->set_error(h->context,error);
 /* Exactly one ordinary call for a valid installed host. No access to result
    before or after it. No read scope or borrowed snapshot spans this call. */
 h->original(receiver,frame,result);
}
#endif
