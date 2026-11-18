#ifndef LID_RESEARCH_HOST_PUBLICATION_R208_H
#define LID_RESEARCH_HOST_PUBLICATION_R208_H
#include "momoka_passthrough_observer.h"
#include "momoka_owned_slot.h"
/* Source/test core. Host and cell must stay resident, immutable after publish.
   Does NOT discover/pin UFunction, change protection or install in the game. */
typedef struct M208Cell {const M204Host*host;M207Slot ownership;} M208Cell;
static int m208_publish(M208Cell*c,const M204Host*host,uintptr_t*slot,uintptr_t original,uintptr_t wrapper){
 const M204Host*empty=0;
 if(!c||!host||!host->original||(uintptr_t)host->original!=original||!slot||!wrapper)return 0;
 /* Publish the complete immutable host BEFORE publishing the callable slot.
    Failed CAS never clears a host that another thread may already read. */
 if(!__atomic_compare_exchange_n(&c->host,&empty,host,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
 return m207_publish(&c->ownership,slot,original,wrapper);
}
static void m208_call(const M208Cell*c,void*receiver,void*frame,void*result){
 const M204Host*h=__atomic_load_n(&c->host,__ATOMIC_ACQUIRE);
 /* Installation contract: wrapper cannot be reachable until host published.
    A missing host indicates integration failure, not a valid vanilla result. */
 if(h)m204_observe_then_original(h,receiver,frame,result);
}
#endif
