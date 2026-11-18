#ifndef LID_RESEARCH_MOMOKA_HOST_GATE_R211_H
#define LID_RESEARCH_MOMOKA_HOST_GATE_R211_H
#include "momoka_passthrough_observer.h"
/* Engine-independent host observation gate. No installation, process writes,
   engine/network/file calls. Memory/scope/thread providers remain host-owned. */
typedef struct M211Host {
 M195Ops memory;void*provider;uint32_t owner;
 uint32_t(*thread)(void*);int(*scope_begin)(void*);void(*scope_end)(void*);
 unsigned busy,attempts,count,stopped,drained;int scoped;
 M197SkillSnapshot records[16];
} M211Host;
static void m211_leave(void*context){
 M211Host*h=context;
 if(h->scoped){h->scope_end(h->provider);h->scoped=0;}
 __atomic_store_n(&h->busy,0,__ATOMIC_RELEASE);
}
static int m211_enter(void*context,uint64_t receiver,uint64_t frame,M195Ops*out,M196Binding*binding){
 M211Host*h=context;unsigned free=0;uint64_t cls,node,menu;
 if(!h||!out||!binding||!receiver||!frame||!h->owner||!h->thread||
 !h->scope_begin||!h->scope_end||h->thread(h->provider)!=h->owner)return 0;
 if(!__atomic_compare_exchange_n(&h->busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
 if(h->stopped||h->attempts>=16||h->count>=16)goto refuse;
 if(!h->scope_begin(h->provider))goto refuse;h->scoped=1;
 /* Native wrapper identity/build are installation preconditions. Filter
    before expensive class traversal; do not keep borrowed menu across calls. */
 if(!m195_read(&h->memory,receiver,0x50,&cls,8)||!m195_named(&h->memory,cls,"BrgDatabase")||
 !m195_read(&h->memory,frame,0x14,&node,8)||
 (!m195_named(&h->memory,node,"BuySticker")&&!m195_named(&h->memory,node,"AddHaveSkillList")))goto refuse;
 h->attempts++;
 if(!m195_read(&h->memory,frame,0x1c,&menu,8)||!menu||!m196_bind(&h->memory,menu,binding))goto refuse;
 *out=h->memory;return 1;
refuse:m211_leave(h);return 0;
}
static void m211_record(void*context,const M197SkillSnapshot*s){
 M211Host*h=context;
 if(!h||!s||!h->scoped||h->count>=16)return;
 /* Owned POD copy only. Buffer is read/drained only after collection stopped
    and no callback is in flight; no concurrent consumer implemented here. */
 h->records[h->count++]=*s;
}
#endif
