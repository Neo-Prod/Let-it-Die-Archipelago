#ifndef LID_MATERIAL_SLOT_OWNER_R105_H
#define LID_MATERIAL_SLOT_OWNER_R105_H
#include "material_binding_bytes.h"
/* Data-slot hook only. Caller must first obtain R91Binding using the actual
   measured digest. This helper does not manufacture module verification. */
typedef struct R105SlotOps {
 R6Ops memory;void*context;uint32_t(*thread)(void*);
 int(*protect)(void*,uint64_t,size_t,uint32_t,uint32_t*);
 int(*cas)(void*,uint64_t,uint64_t,uint64_t,uint64_t*);
} R105SlotOps;
typedef struct R105Slot {
 uint32_t owner,attempted,installed,restored,fault;
 uint64_t address,original,replacement;
} R105Slot;
static int r105_slot_owned(const R105Slot*s,const R105SlotOps*o){
 uint64_t target;
 return s&&o&&o->thread&&s->owner&&o->thread(o->context)==s->owner&&s->installed&&s->restored&&!s->fault&&
 r6_copy(&o->memory,s->address,&target,8)&&target==s->replacement;
}
static int r105_slot_install(R105Slot*s,const R105SlotOps*o,const R91Binding*b,uint32_t owner,uint64_t replacement){
 uint64_t current,observed;uint32_t old=0,ignored=0;
 if(!s||!o||!o->thread||!o->protect||!o->cas||!b||!b->image||b->image>UINT64_MAX-0x20f90d0||
 !owner||o->thread(o->context)!=owner||!replacement||b->entry[R91_ITEM_PROCESS]!=b->image+0x10b5f70)return 0;
 if(s->attempted)return s->owner==owner&&s->replacement==replacement&&s->address==b->image+0x20f90d0&&r105_slot_owned(s,o);
 if(!r6_copy(&o->memory,b->image+0x20f90d0,&current,8)||current!=b->entry[R91_ITEM_PROCESS]||replacement==current)return 0;
 s->owner=owner;s->address=b->image+0x20f90d0;s->replacement=replacement;s->attempted=1;
 /* Dispatcher must acquire original even for foreign/reentrant calls. */
 __atomic_store_n(&s->original,current,__ATOMIC_RELEASE);
 if(!o->protect(o->context,s->address,8,4 /* PAGE_READWRITE */,&old))return 0;
 int exchanged=o->cas(o->context,s->address,current,replacement,&observed);
 if(exchanged&&observed==current)s->installed=1;
 s->restored=o->protect(o->context,s->address,8,old,&ignored)==1;
 if(!s->restored||!s->installed)s->fault=1;
 /* No blind rollback into a slot potentially changed by another hook.
    Faulted dispatcher must forward original only, never run pickup effects. */
 return r105_slot_owned(s,o);
}
#endif
