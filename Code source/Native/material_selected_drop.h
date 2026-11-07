#ifndef LID_MATERIAL_SELECTED_DROP_R114_H
#define LID_MATERIAL_SELECTED_DROP_R114_H
#include "material_commit_pipeline.h"
#include "material_bag_slot.h"
#include "material_drop_part_call.h"
/* Host authorize must prove module, current owner/player and transaction.
   accepted=1 additionally requires the matching R92 receipt.accepted.
   No host integration or lifecycle reset is implicit in this helper. */
typedef struct R114Ops {R6Ops memory;void*context;uint32_t owner;uint64_t image;uint32_t(*thread)(void*);int(*authorize)(void*,const R82Event*,unsigned accepted);} R114Ops;
typedef struct R114Drop {R114Ops ops;R82Event event;R113Slot slot;R111DropReceipt receipt;unsigned prepared,busy,fault,done;} R114Drop;
static int r114_current(R114Drop*d,unsigned accepted){
 int32_t index;uint8_t state;uint32_t flags;R114Ops*o=&d->ops;
 return o->thread&&o->authorize&&o->owner&&o->thread(o->context)==o->owner&&o->authorize(o->context,&d->event,accepted)==1&&
 r6_at(&o->memory,d->event.sample.manager,0x92cd4,&index,4)&&index==d->event.sample.bag_slot&&index==d->slot.slot&&
 r6_at(&o->memory,d->event.sample.manager,0x92cc0,&state,1)&&state==4&&
 r6_at(&o->memory,d->event.sample.manager,0x92cf8,&flags,4)&&(flags&1)&&!(flags&14);
}
static int r114_same_event(const R82Event*a,const R82Event*b){return a->material==b->material&&a->sample.bag_slot==b->sample.bag_slot&&r82_same_stamp(&a->stamp,&b->stamp)&&r82_same_sample(&a->sample,&b->sample);}
static int r114_permit(void*c,uint64_t player,int slot){R114Drop*d=c;return d->busy&&d->prepared&&!d->fault&&!d->receipt.invoked&&player==d->slot.player&&slot==d->slot.slot&&d->slot.occupied&&r114_current(d,1)&&r113_unchanged(&d->ops.memory,&d->slot);}
static uint32_t r114_thread(void*c){R114Drop*d=c;return d->ops.thread?d->ops.thread(d->ops.context):0;}
static void r114_plan(const R114Drop*d,R112DropPlan*p){*p=(R112DropPlan){d->slot.slot,d->slot.occupied,{d->slot.player,d->slot.array.data,(uint64_t)(unsigned)d->slot.array.num,(uint64_t)(unsigned)d->slot.array.max}};}
static int r114_prepare(R114Drop*d,const R82Event*e,R112DropPlan*out){
 R114Drop candidate;R111DropSignature signature;uint64_t target;int ok=0;
 if(!d||!e||!out||d->busy||d->fault||d->receipt.invoked||d->done||!r82_stamp_valid(&e->stamp)||e->material<1||e->material>46)return 0;
 d->busy=1;
 if(d->prepared){if(!r114_same_event(e,&d->event)||!r114_current(d,0)||!r113_unchanged(&d->ops.memory,&d->slot))goto done;r114_plan(d,out);ok=1;goto done;}
 candidate=*d;candidate.event=*e;
 if(!r113_slot(&d->ops.memory,e->stamp.player,d->ops.image,e->sample.bag_slot,&candidate.slot)||!r114_current(&candidate,0))goto done;
 if(candidate.slot.occupied){R111DropOps call={d->ops.memory,&candidate,d->ops.owner,d->ops.image,r114_thread,r114_permit};
  if(!r111_drop_signature(&d->ops.memory,e->stamp.player,&signature)||!r111_dispatch(&call,e->stamp.player,&target))goto done;
 }
 candidate.prepared=1;*d=candidate;r114_plan(d,out);ok=1;
done:d->busy=0;return ok;
}
static int r114_finish(R114Drop*d,const R82Event*e,const R112DropPlan*p,R112DropState*out){
 R112DropPlan expected;int ok=0;
 if(!d||!e||!p||!out||d->busy||!d->prepared||d->fault||d->done||d->receipt.invoked||!r114_same_event(e,&d->event))return 0;
 r114_plan(d,&expected);if(p->slot!=expected.slot||p->occupied!=expected.occupied||memcmp(p->identity,expected.identity,sizeof(p->identity)))return 0;
 d->busy=1;
 if(!r114_current(d,1)||!r113_unchanged(&d->ops.memory,&d->slot))goto end;
 if(d->slot.occupied){R111DropOps call={d->ops.memory,d,d->ops.owner,d->ops.image,r114_thread,r114_permit};
  if(r111_drop_call(&d->receipt,&call,d->slot.player,d->slot.slot)!=R111_SENT)goto end;
 }
 if(!r114_current(d,1)||!r113_empty_after(&d->ops.memory,&d->slot))goto end;
 d->done=1;ok=1;
end:
 out->invoked=d->receipt.invoked;out->actor=d->receipt.actor;out->verified=ok;
 if(!ok)d->fault=1;d->busy=0;return ok;
}
#endif
