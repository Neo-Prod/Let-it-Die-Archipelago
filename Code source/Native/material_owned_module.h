#ifndef LID_MATERIAL_OWNED_MODULE_R106_H
#define LID_MATERIAL_OWNED_MODULE_R106_H
#include "material_slot_owner.h"
#include "material_module_hash.h"
/* R91's original-slot check is adapted for exactly one already-owned slot.
   Read the real replacement each time before normalizing that one value to
   the retained original. PE, all prefixes and all seven other slots unchanged. */
typedef struct R106OwnedRead {const R6Ops*memory;const R105Slot*slot;} R106OwnedRead;
static int r106_owned_copy(void*opaque,uint64_t p,void*out,size_t n){
 R106OwnedRead*c=opaque;uint64_t value;
 if(p==c->slot->address&&n==8){
  if(!r6_copy(c->memory,p,&value,8)||value!=c->slot->replacement)return 0;
  memcpy(out,&c->slot->original,8);return 1;
 }
 return r6_copy(c->memory,p,out,n);
}
static int r106_owned_module(const R105Hash*hash,const R105Slot*slot,const R105SlotOps*ops,const R91Binding*bound){
 R91Binding now;char digest[65];
 if(!bound||!slot||!ops||!ops->thread||slot->address!=bound->image+0x20f90d0||slot->original!=bound->entry[R91_ITEM_PROCESS]||
 !r105_verified(hash,ops->thread(ops->context),bound->image,digest)||!r105_slot_owned(slot,ops))return 0;
 R106OwnedRead ctx={&ops->memory,slot};R6Ops memory={&ctx,r106_owned_copy,0};
 if(!r91_module_bind(&memory,bound->image,digest,&now)||now.image!=bound->image)return 0;
 for(unsigned i=0;i<R91_ENTRIES;i++)if(now.entry[i]!=bound->entry[i])return 0;
 return r105_slot_owned(slot,ops);
}
#endif
