#ifndef LID_MATERIAL_INPUT_SLOT_R122_H
#define LID_MATERIAL_INPUT_SLOT_R122_H
#include "material_owned_module.h"
static const uint8_t r122_input_bytes[32]={0x48,0x83,0xec,0x28,0x48,0x8b,0x49,0x68,0xba,0x01,0x00,0x00,0x00,0x48,0x8b,0x01,0xff,0x90,0xb0,0x02,0x00,0x00,0xc1,0xe8,0x04,0x83,0xe0,0x01,0x48,0x83,0xc4,0x28};
static int r122_input_prefix(const R6Ops*o,uint64_t image){uint8_t bytes[32];return image&&image<=UINT64_MAX-0x2200000&&r6_at(o,image,0x10b5af0,bytes,32)&&!memcmp(bytes,r122_input_bytes,32);}
static int r122_input_install_raw(R105Slot*s,const R105SlotOps*o,const R91Binding*b,uint32_t owner,uint64_t replacement){
 uint64_t current,observed;uint32_t old=0,ignored=0;
 if(!s||!o||!o->thread||!o->protect||!o->cas||!b||!b->image||b->image>UINT64_MAX-0x20f90f8||
 !owner||o->thread(o->context)!=owner||!replacement||!r122_input_prefix(&o->memory,b->image))return 0;
 if(s->attempted)return s->owner==owner&&s->replacement==replacement&&s->address==b->image+0x20f90f8&&r105_slot_owned(s,o);
 if(!r6_copy(&o->memory,b->image+0x20f90f8,&current,8)||current!=(b->image+0x10b5af0)||replacement==current)return 0;
 s->owner=owner;s->address=b->image+0x20f90f8;s->replacement=replacement;s->attempted=1;
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

/* SHA + all R91 structure proofs precede this second, narrowly typed slot.
   Input gate must be installed BEFORE completion/first input. */
static int r122_input_install(R105Slot*s,const R105SlotOps*o,const R91Binding*b,const R105Hash*hash,uint32_t owner,uint64_t replacement){
 char digest[65];R91Binding now;
 if(!s||!o||!b||!r105_verified(hash,owner,b->image,digest)||!r91_module_bind(&o->memory,b->image,digest,&now))return 0;
 for(unsigned i=0;i<R91_ENTRIES;i++)if(now.entry[i]!=b->entry[i])return 0;
 return r122_input_install_raw(s,o,b,owner,replacement);
}
/* Input ownership is checked in addition to all existing R106 checks.
   A NULL completion means pre-completion validation, not permission to add. */
static int r122_owned_module(const R105Hash*hash,const R105Slot*input,const R105Slot*completion,const R105SlotOps*o,const R91Binding*b){
 char digest[65];R91Binding now;
 if(!input||!o||!b||input->address!=b->image+0x20f90f8||input->original!=b->image+0x10b5af0||
 !r105_slot_owned(input,o)||!r122_input_prefix(&o->memory,b->image))return 0;
 if(completion){if(!r106_owned_module(hash,completion,o,b))return 0;}
 else{
  if(!r105_verified(hash,o->thread(o->context),b->image,digest)||!r91_module_bind(&o->memory,b->image,digest,&now))return 0;
  for(unsigned i=0;i<R91_ENTRIES;i++)if(now.entry[i]!=b->entry[i])return 0;
 }
 return r105_slot_owned(input,o);
}
#endif
