#ifndef LID_RESEARCH_OWNED_SLOT_R207_H
#define LID_RESEARCH_OWNED_SLOT_R207_H
#include <stdint.h>
/* Source core, tested ONLY with owned process memory. No VirtualProtect,
   pointer discovery or game access. Host MUST supply a stable writable slot.
   State object/original/wrapper must remain resident after publication. */
enum {M207_EMPTY,M207_PREPARING,M207_INSTALLED,M207_RESTORING,M207_RETIRED,M207_REFUSED};
typedef struct M207Slot {unsigned state;uintptr_t*slot;uintptr_t original,wrapper;} M207Slot;
static int m207_publish(M207Slot*s,uintptr_t*slot,uintptr_t original,uintptr_t wrapper){
 unsigned expected=M207_EMPTY;uintptr_t old=original;
 if(!s||!slot||((uintptr_t)slot&(sizeof(uintptr_t)-1))||!original||!wrapper||original==wrapper)return 0;
 if(!__atomic_compare_exchange_n(&s->state,&expected,M207_PREPARING,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
 s->slot=slot;s->original=original;s->wrapper=wrapper;
 /* Publication before slot CAS. Native wrapper must only use fields after
    appropriate acquire synchronization; source does not implement wrapper. */
 __atomic_thread_fence(__ATOMIC_RELEASE);
 if(!__atomic_compare_exchange_n(slot,&old,wrapper,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
  __atomic_store_n(&s->state,M207_REFUSED,__ATOMIC_RELEASE);return 0;
 }
 __atomic_store_n(&s->state,M207_INSTALLED,__ATOMIC_RELEASE);return 1;
}
static int m207_restore(M207Slot*s){
 unsigned expected=M207_INSTALLED;uintptr_t old;
 if(!s||!__atomic_compare_exchange_n(&s->state,&expected,M207_RESTORING,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
 old=s->wrapper;
 int restored=__atomic_compare_exchange_n(s->slot,&old,s->original,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 /* Even if a later owner changed it, never overwrite that owner or free our
    published wrapper. RETIRED does not imply no invocation is in flight. */
 __atomic_store_n(&s->state,M207_RETIRED,__ATOMIC_RELEASE);return restored;
}
#endif
