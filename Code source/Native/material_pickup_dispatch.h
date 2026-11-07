#ifndef LID_MATERIAL_PICKUP_DISPATCH_R107_H
#define LID_MATERIAL_PICKUP_DISPATCH_R107_H
#include "material_pickup_adapter.h"
/* Pure routing around R93. Native ItemGetProcess ABI: void(this).
   Owner and original must be initialized before publishing the vtable hook.
   Only atomic fence is consulted by foreign threads; R93 state is owner-only. */
typedef void(*R107Original)(void*);
typedef struct R107Route {uint32_t owner,busy;uint64_t fence;} R107Route;
enum {R107_FORWARDED=0,R107_CONSUMED=1,R107_HELD=-1};
static void r107_release(R107Route*r,const R93Pickup*t){
 if(!t->pending&&!t->fault&&!(t->receipt.accepted&&t->receipt.fault))__atomic_store_n(&r->fence,0,__ATOMIC_RELEASE);
}
static int r107_admit(R107Route*r,R93Pickup*t,const R93Ops*o,int enabled,uint64_t caller,uint64_t player,uint64_t linked,int index){
 if(!r||!t||!o||!o->commit.thread||!r->owner||o->commit.thread(o->commit.context)!=r->owner)return R82_PASS;
 if(caller!=o->caller||index!=-1)return R82_PASS;
 if(!enabled)return t->pending||__atomic_load_n(&r->fence,__ATOMIC_ACQUIRE)==o->manager?R82_BLOCKED:R82_PASS;
 if(r->busy)return R82_BLOCKED;
 uint64_t fence=__atomic_load_n(&r->fence,__ATOMIC_ACQUIRE);
 if(fence&&fence!=o->manager)return R82_BLOCKED;
 r->busy=1;__atomic_store_n(&r->fence,o->manager,__ATOMIC_RELEASE);
 int result=r93_admit(t,o,enabled,caller,player,linked,index);
 r107_release(r,t);r->busy=0;return result;
}
static int r107_forward(R107Original original,uint64_t self){if(!original)return R107_HELD;original((void*)(uintptr_t)self);return R107_FORWARDED;}
static int r107_dispatch(R107Route*r,R93Pickup*t,const R93Ops*o,uint32_t tid,int enabled,uint64_t self,R107Original original){
 if(!r||!r->owner||!self)return r107_forward(original,self);
 uint64_t fence=__atomic_load_n(&r->fence,__ATOMIC_ACQUIRE);
 if(tid!=r->owner)return fence==self?R107_HELD:r107_forward(original,self);
 if(!t||!o||!r93_ops(o)||o->commit.owner!=r->owner||o->commit.thread(o->commit.context)!=r->owner||self!=o->manager)
  return fence==self?R107_HELD:r107_forward(original,self);
 if(r->busy||t->busy)return fence==self?R107_HELD:r107_forward(original,self);
 r->busy=1;int result=R107_CONSUMED;
 if(!t->pending){
  R81Sample sample;int read=r81_sample(&o->commit.memory,self,o->commit.layout,&sample);
  if(t->fault&&fence==self&&read!=0){result=R107_HELD;goto done;}
  /* A counted-but-incomplete pickup may never fall back to native inventory,
     including after AP authority becomes inactive. Unproved record is held. */
  if(t->receipt.accepted&&((read<0&&t->receipt.event.sample.manager==self)||
    (read==1&&r82_same_sample(&sample,&t->receipt.event.sample)))){result=R107_HELD;goto done;}
  if(!enabled||read<=0||(read==1&&!r82_material_lookup(sample.item_id))){
   r107_release(r,t);r->busy=0;return r107_forward(original,self);
  }
 }
 /* R93 owns the one completion/abort path. A virtual admission never falls
    through to original, even when cancellation or commit is refused. */
 int completed=r93_complete(t,o,enabled);
 if(completed!=R82_MANAGED)result=R107_HELD;
 if(t->fault)result=R107_HELD;
done:r107_release(r,t);r->busy=0;return result;
}
#endif
