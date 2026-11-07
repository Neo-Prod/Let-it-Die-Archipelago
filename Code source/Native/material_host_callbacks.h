#ifndef LID_MATERIAL_HOST_CALLBACKS_R108_H
#define LID_MATERIAL_HOST_CALLBACKS_R108_H
#include "performance_events.h"
#include "material_selected_drop.h"
#include "material_pickup_dispatch.h"
#include "material_owner_snapshot.h"
#include "material_capture.h"
#include "material_owned_module.h"
#include "material_capability_mailbox.h"
typedef struct R108Ops {
 R6Ops memory;void*context;uint32_t(*thread)(void*);int(*module_verified)(void*);
 /* active_snapshot must prove actual required Wednesday rotation, session,
    transport and map, as checks_material_owner_snapshot does. */
 int(*active_snapshot)(void*,uint64_t,uint64_t,R104Snapshot*);
 uint64_t(*engine_game)(void*);int(*can_write)(void*,uint64_t,size_t);
 int(*store)(void*,uint64_t,const void*,size_t);
} R108Ops;
typedef struct R108Host {
 R108Ops ops;R114Drop selected_drop;R92Receipt previous_success;R111DropReceipt previous_drop;unsigned retired;R107Route route;R93Pickup pickup;R93Ops transaction;
 R81Layout layout;R91Binding binding;R105Slot slot;R105SlotOps slot_ops;
 R103Fields engine_fields;R15Queue*queue;const R100Mailbox*mailbox;uint64_t manager;
} R108Host;
static int r108_owner(R108Host*h){return h&&h->route.owner&&h->ops.thread&&h->ops.thread(h->ops.context)==h->route.owner;}
static int r108_module(R108Host*h){return r108_owner(h)&&h->ops.module_verified&&h->ops.module_verified(h->ops.context);}
static int r108_active(R108Host*h,uint64_t player,R104Snapshot*out){
 R104Snapshot s;R99Capability request;
 if(!out||!r108_module(h)||!h->ops.active_snapshot||!h->manager||!h->queue||!h->mailbox||
 !r100_owner_request(h->mailbox,h->route.owner,&request)||
 !h->ops.active_snapshot(h->ops.context,player,h->manager,&s)||!r104_stamp_valid(&s.stamp)||
 !r15_same_context(&request.context,&s.stamp.context)||!r15_same_context(&h->queue->context,&s.stamp.context)||
 !__atomic_load_n(&h->queue->initialized,__ATOMIC_ACQUIRE)||h->queue->owner!=h->route.owner||
 s.view.graph.manager.object!=h->manager||!r108_module(h))return 0;
 *out=s;return 1;
}
static void r108_stamp_from(const R104Snapshot*s,R82Stamp*out){
 R82Stamp v={0};v.context=s->stamp.context;v.epoch=s->stamp.epoch;v.revision=s->stamp.revision;v.player=s->view.graph.player.object;
 memcpy(v.floor,s->stamp.floor,16);memcpy(v.area,s->stamp.area,32);*out=v;
}
static int r108_snapshot(void*context,R82Stamp*out){
 R108Host*h=context;R104Snapshot s;R82Stamp v;
 if(!h||!out||!r108_active(h,h->pickup.pending?h->pickup.event.stamp.player:0,&s))return 0;
 r108_stamp_from(&s,&v);if(!r82_stamp_valid(&v))return 0;*out=v;return 1;
}
static int r108_current_game(void*context,const R82Event*e,uint64_t*out){
 R108Host*h=context;R104Snapshot s;R82Stamp stamp;
 if(!h||!out||!e||e->sample.manager!=h->manager||!r108_active(h,e->stamp.player,&s))return 0;
 r108_stamp_from(&s,&stamp);if(!r82_same_stamp(&e->stamp,&stamp))return 0;*out=s.view.game.object;return 1;
}
/* Independent of AP authority, but never independent of owner/module and
   the actual current engine object graph. No stale cancellation writes. */
static int r108_engine_view_core(R108Host*h,uint64_t manager,uint64_t player){
 R103View view;uint64_t game;
 return r108_module(h)&&manager==h->manager&&h->ops.engine_game&&(game=h->ops.engine_game(h->ops.context))&&
 r103_view(&h->ops.memory,game,player,h->binding.image,&h->engine_fields,&view)&&view.graph.manager.object==manager&&
 (!player||view.graph.player.object==player)&&h->ops.engine_game(h->ops.context)==game&&r108_module(h);
}
static int r108_engine_view(R108Host*h,uint64_t manager,uint64_t player){
 int scope=R152_READ_BEGIN();int result=r108_engine_view_core(h,manager,player);
 R152_READ_END(scope);return result;
}
static int r108_engine_current(void*context,uint64_t manager,uint64_t player){return r108_engine_view(context,manager,player);}
static int r108_write_field(R108Host*h,uint64_t address,size_t size){
 return h&&h->manager&&((address==h->manager+0x92cc8&&size==4)||(address==h->manager+0x92cc0&&size==1)||(address==h->manager+0x92cf8&&size==4));
}
static int r108_writable(void*context,uint64_t address,size_t size){
 R108Host*h=context;
 return r108_write_field(h,address,size)&&r108_engine_view(h,h->manager,h->pickup.pending?h->pickup.event.stamp.player:0)&&
 h->ops.can_write&&h->ops.can_write(h->ops.context,address,size);
}
static int r108_write(void*context,uint64_t address,const void*value,size_t size){
 R108Host*h=context;uint8_t result[4];
 if(!value||!r108_writable(h,address,size)||!h->ops.store)return 0;
 if(address==h->manager+0x92cc8){int32_t v;memcpy(&v,value,4);if(v!=-1)return 0;}
 else if(address==h->manager+0x92cc0){if(*(const uint8_t*)value)return 0;}
 else{uint32_t old,v;memcpy(&v,value,4);if(!r6_copy(&h->ops.memory,address,&old,4)||v!=(old&~3u))return 0;}
 return h->ops.store(h->ops.context,address,value,size)&&r6_copy(&h->ops.memory,address,result,size)&&!memcmp(result,value,size);
}
static int r108_enqueue(void*context,const R82Event*e){
 R108Host*h=context;R104Snapshot s;R82Stamp stamp;
 if(!h||!e||!h->pickup.pending||!r82_same_sample(&h->pickup.event.sample,&e->sample)||
 !r82_same_stamp(&h->pickup.event.stamp,&e->stamp)||__atomic_load_n(&h->route.fence,__ATOMIC_ACQUIRE)!=h->manager||
 !r108_active(h,e->stamp.player,&s))return 0;
 r108_stamp_from(&s,&stamp);if(!r82_same_stamp(&stamp,&e->stamp))return 0;
 return r95_capture_material(h->queue,h->route.owner,5,e,&h->pickup.source,"4HMA");
}
static uint32_t r108_thread(void*context){R108Host*h=context;return h->ops.thread?h->ops.thread(h->ops.context):0;}
/* Actual host authority, not a caller-supplied constant proof. */
static int r115_drop_authorize(void*context,const R82Event*e,unsigned accepted){
 R108Host*h=context;uint64_t game;
 if(!h||!e||!r108_module(h)||!h->route.busy||!h->pickup.busy||!h->pickup.pending||!h->pickup.receipt.busy||
 h->pickup.fault||h->pickup.receipt.fault||h->manager!=e->sample.manager||
 __atomic_load_n(&h->route.fence,__ATOMIC_ACQUIRE)!=h->manager||
 (h->pickup.receipt.accepted!=0)!=(accepted!=0)||!r114_same_event(&h->pickup.receipt.event,e)||
 !r82_same_stamp(&h->pickup.event.stamp,&e->stamp)||!r82_same_sample(&h->pickup.event.sample,&e->sample)||
 !r108_current_game(h,e,&game)||!r92_still(&h->transaction.commit,&h->pickup.receipt,1))return 0;
 return 1;
}
static int r115_drop_prepare(void*c,const R82Event*e,R112DropPlan*out){
 R108Host*h=c;LID_PERF_BEGIN(pickup_drop_prepare_time);int result=h&&r114_prepare(&h->selected_drop,e,out);
 LID_PERF_END(pickup_drop_prepare_time,43,"pickup_drop_preflight",result);return result;
}
static int r115_drop_finish(void*c,const R82Event*e,const R112DropPlan*p,R112DropState*out){
 R108Host*h=c;LID_PERF_BEGIN(pickup_drop_finish_time);int result=h&&r114_finish(&h->selected_drop,e,p,out);
 LID_PERF_END(pickup_drop_finish_time,44,"pickup_drop_finish",result);return result;
}
/* Explicit release only after native/adapter cancellation has reached idle.
   Engine proof permits AP-disconnect cleanup; no writes to the game. */
static int r115_release_unaccepted(R108Host*h){
 uint8_t state;int32_t selected;uint32_t flags;R114Ops ops;
 if(!h||!r108_owner(h)||h->route.busy||h->pickup.busy||h->pickup.pending||h->pickup.fault||h->pickup.receipt.busy||h->pickup.receipt.accepted||
 !h->selected_drop.prepared||h->selected_drop.busy||h->selected_drop.receipt.invoked||h->selected_drop.done||
 !r108_engine_view(h,h->manager,h->selected_drop.event.stamp.player)||
 !r6_at(&h->ops.memory,h->manager,0x92cc0,&state,1)||state!=0||
 !r6_at(&h->ops.memory,h->manager,0x92cc8,&selected,4)||selected!=-1||
 !r6_at(&h->ops.memory,h->manager,0x92cf8,&flags,4)||(flags&15)||
 !r108_engine_view(h,h->manager,h->selected_drop.event.stamp.player))return 0;
 ops=h->selected_drop.ops;memset(&h->selected_drop,0,sizeof(h->selected_drop));h->selected_drop.ops=ops;return 1;
}
/* Retire only a fully completed transaction. Keep its receipt as evidence,
   and leave the pickup receipt intact for the ordinary duplicate guard. */
static int r116_retire_success(R108Host*h){
 uint8_t state;int32_t selected;uint32_t flags;R114Ops ops;
 if(!h||!r108_module(h)||h->route.busy||h->pickup.busy||h->pickup.pending||h->pickup.fault||h->pickup.receipt.busy||
 !h->pickup.receipt.accepted||h->pickup.receipt.fault||h->pickup.receipt.phase!=R92_RESET||
 !h->selected_drop.done||h->selected_drop.busy||h->selected_drop.fault||!h->pickup.receipt.drop_state.verified||
 h->pickup.receipt.drop_state.invoked!=h->selected_drop.receipt.invoked||
 !r114_same_event(&h->selected_drop.event,&h->pickup.receipt.event)||
 __atomic_load_n(&h->route.fence,__ATOMIC_ACQUIRE)||
 !r108_engine_view(h,h->manager,h->selected_drop.event.stamp.player)||
 !r6_at(&h->ops.memory,h->manager,0x92cc0,&state,1)||state!=0||
 !r6_at(&h->ops.memory,h->manager,0x92cc8,&selected,4)||selected!=-1||
 !r6_at(&h->ops.memory,h->manager,0x92cf8,&flags,4)||(flags&15)||
 !r108_engine_view(h,h->manager,h->selected_drop.event.stamp.player))return 0;
 h->previous_success=h->pickup.receipt;h->previous_drop=h->selected_drop.receipt;h->retired++;
 ops=h->selected_drop.ops;memset(&h->selected_drop,0,sizeof(h->selected_drop));h->selected_drop.ops=ops;return 1;
}
static int r116_host_dispatch(R108Host*h,uint32_t tid,int enabled,uint64_t self,R107Original original){
 if(!h)return r107_forward(original,self);
 int result=r107_dispatch(&h->route,&h->pickup,&h->transaction,tid,enabled,self,original);
 if(tid==h->route.owner&&!h->route.busy&&!h->pickup.busy){
  if(h->selected_drop.done)(void)r116_retire_success(h);
  else if(h->selected_drop.prepared&&!h->pickup.pending&&!h->pickup.receipt.accepted)(void)r115_release_unaccepted(h);
 }
 return result;
}
/* Native TickProcess only calls GetBagSpace from Select2, not menu3 or
   animation4. A later proven Select2 can supersede an unaccepted intention.
   This is event-driven: no new tick hook, scan or background poll. */
static int r117_old_record(R108Host*h){
 const R81Sample*s=&h->pickup.event.sample;uint64_t record,actor;uint32_t use,name,number;int32_t unique,time;char id[64],entity[96];R83SourcePlan source;
 if(s->slot<0||s->slot>=R81_COUNT||s->manager!=h->manager||!r82_actor(&h->ops.memory,s))return 0;
 record=h->manager+0x74+(uint64_t)(unsigned)s->slot*R81_STRIDE;
 return r6_at(&h->ops.memory,record,0,&use,4)&&(use&1)&&r6_at(&h->ops.memory,record,4,&unique,4)&&unique==s->unique&&
 r6_at(&h->ops.memory,record,0x14,&actor,8)&&actor==s->actor&&r6_at(&h->ops.memory,actor,0x48,&name,4)&&name==s->actor_name&&
 r6_at(&h->ops.memory,actor,0x4c,&number,4)&&number==s->actor_number&&
 r6_at(&h->ops.memory,record,0x240,&time,4)&&time==s->gettime&&
 r81_id(&h->ops.memory,record+0x230,id,64,1)&&!strcmp(id,s->item_id)&&r81_id(&h->ops.memory,record+0x220,entity,96,0)&&!strcmp(entity,s->entity_id)&&
 r83_source_plan(&h->ops.memory,s,&source)&&r83_source_equal(&source,&h->pickup.source);
}
static int r117_admit_selection(R108Host*h,int enabled,uint64_t caller,uint64_t player,uint64_t linked,int index){
 if(!h)return R82_BLOCKED;
 if(h->pickup.pending){
  R82Event e={0};R83SourcePlan source;R92View view;
  if(!enabled||!r108_module(h)||h->route.busy||h->pickup.busy||h->pickup.fault||h->pickup.receipt.accepted||h->pickup.receipt.busy||
  h->selected_drop.prepared||h->selected_drop.busy||h->selected_drop.receipt.invoked||
  caller!=h->transaction.caller||index!=-1||player!=h->pickup.event.stamp.player||linked!=player||
  r81_sample(&h->ops.memory,h->manager,&h->layout,&e.sample)!=1||e.sample.state!=2||(e.sample.flags&15))return R82_BLOCKED;
  e.material=r82_material_lookup(e.sample.item_id);
  if(!r93_candidate(&h->transaction,&e,&source,&view)||!r82_same_stamp(&e.stamp,&h->pickup.event.stamp)||
  !r92_same_view(&view,&h->pickup.view))return R82_BLOCKED;
  if(!r82_same_sample(&e.sample,&h->pickup.event.sample)){

   /* No native writes and no accepted receipt cleared. Current Select2 and
      absence of acceptance/drop prove this old intent can be abandoned. */
   h->pickup.pending=0;h->pickup.cancelled++;memset(&h->pickup.event,0,sizeof(h->pickup.event));
  }
 }
 return r107_admit(&h->route,&h->pickup,&h->transaction,enabled,caller,player,linked,index);
}
/* Ordinary non-item record: R81 read0 already verifies metadata, selected
   use-bit and type!=4. Never claim it is a material or enqueue it. */
static int r118_release_for_ordinary(R108Host*h,uint64_t caller,uint64_t player,int index){
 R81Sample sample;R92View view;uint8_t state;uint32_t flags;int32_t selected;
 if(!h||!r108_module(h)||!h->pickup.pending||h->route.busy||h->pickup.busy||h->pickup.fault||h->pickup.receipt.accepted||h->pickup.receipt.busy||
 h->selected_drop.prepared||h->selected_drop.busy||h->selected_drop.receipt.invoked||caller!=h->transaction.caller||index!=-1||player!=h->pickup.event.stamp.player||
 r81_sample(&h->ops.memory,h->manager,&h->layout,&sample)!=0||
 !r6_at(&h->ops.memory,h->manager,0x92cc0,&state,1)||state!=2||!r6_at(&h->ops.memory,h->manager,0x92cf8,&flags,4)||(flags&15)||
 !r6_at(&h->ops.memory,h->manager,0x92cc8,&selected,4)||selected==h->pickup.event.sample.slot||
 !r92_view(&h->transaction.commit,&h->pickup.event,&view)||!r92_same_view(&view,&h->pickup.view))return 0;
 h->pickup.pending=0;h->pickup.cancelled++;memset(&h->pickup.event,0,sizeof(h->pickup.event));r107_release(&h->route,&h->pickup);return 1;
}
/* A verified new room in the SAME AP context, already idle, can abandon an
   unaccepted source without dereferencing the old source/actor/old stage. */
static int r118_release_changed_area(R108Host*h){
 R104Snapshot current;R82Stamp stamp;uint8_t state;uint32_t flags;int32_t selected;
 if(!h||!r108_owner(h)||h->route.busy||h->pickup.busy||!h->pickup.pending||h->pickup.fault||h->pickup.receipt.accepted||h->pickup.receipt.busy||
 h->selected_drop.prepared||h->selected_drop.busy||h->selected_drop.receipt.invoked||
 !r108_active(h,h->pickup.event.stamp.player,&current))return 0;
 r108_stamp_from(&current,&stamp);
 if(!r15_same_context(&stamp.context,&h->pickup.event.stamp.context)||
 (!strcmp(stamp.floor,h->pickup.event.stamp.floor)&&!strcmp(stamp.area,h->pickup.event.stamp.area))||
 !r87_same_identity(&current.view.game,&h->pickup.view.game)||!r87_same_graph(&current.view.graph,&h->pickup.view.graph)||
 !r6_at(&h->ops.memory,h->manager,0x92cc0,&state,1)||state!=0||!r6_at(&h->ops.memory,h->manager,0x92cc8,&selected,4)||selected!=-1||
 !r6_at(&h->ops.memory,h->manager,0x92cf8,&flags,4)||(flags&15)||!r108_module(h))return 0;
 h->pickup.pending=0;h->pickup.cancelled++;memset(&h->pickup.event,0,sizeof(h->pickup.event));r107_release(&h->route,&h->pickup);return 1;
}
static int r118_admit_selection(R108Host*h,int enabled,uint64_t caller,uint64_t player,uint64_t linked,int index){
 if(enabled&&h&&linked==player)(void)r118_release_for_ordinary(h,caller,player,index);
 return r117_admit_selection(h,enabled,caller,player,linked,index);
}
/* Use only AFTER completion protection has been independently armed. */
static int r117_admit_space(R108Host*h,int native_space,uint64_t caller,uint64_t player,int*out){
 if(!out||native_space < -1||native_space>=99)return 0;
 if(r118_admit_selection(h,1,caller,player,player,-1)!=R82_MANAGED)return 0;
 *out=native_space;return 1; /* -1 keeps native menu; >=0 keeps actual free slot */
}
/* Binding does not reset a transaction or erase a receipt. Accepted faults
   cannot be rebound. Caller supplies actual metadata/module validated fields. */
static int r108_bind_callbacks(R108Host*h,uint64_t manager,const R103Fields*fields,const R81Layout*layout,const R91Binding*binding){
 if(!r108_owner(h)||!h->ops.memory.copy||!h->ops.memory.name_is||!h->ops.module_verified||!h->ops.active_snapshot||
 !h->ops.engine_game||!h->ops.can_write||!h->ops.store||!manager||manager>UINT64_MAX-R81_EXTENT||!fields||!layout||!binding||!r81_layout_valid(layout)||!binding->image||binding->image>UINT64_MAX-0x1005c000ull||
 h->route.busy||h->pickup.busy||h->pickup.pending||h->pickup.fault||h->pickup.receipt.fault||h->selected_drop.prepared||h->selected_drop.receipt.invoked)return 0;
 R81Layout candidate=*layout;
 R92Ops commit={0};commit.drop_prepare=r115_drop_prepare;commit.drop_finish=r115_drop_finish;
 commit.memory=h->ops.memory;commit.context=h;commit.thread=r108_thread;commit.current_game=r108_current_game;commit.writable=r108_writable;commit.write=r108_write;
 commit.enqueue=r108_enqueue;commit.binding=*binding;commit.fields=fields->graph;commit.layout=&candidate;commit.owner=h->route.owner;
 R93Ops transaction={commit,manager,binding->image+0x10cd01e,r108_snapshot,r108_engine_current};
 if(!r93_ops(&transaction))return 0;
 h->manager=manager;h->engine_fields=*fields;h->layout=candidate;h->binding=*binding;
 transaction.commit.layout=&h->layout;h->transaction=transaction;
 h->selected_drop.ops=(R114Ops){h->ops.memory,h,h->route.owner,binding->image,r108_thread,r115_drop_authorize};return 1;
}
#endif
