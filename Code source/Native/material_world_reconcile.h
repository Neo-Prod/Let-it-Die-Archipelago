#ifndef LID_MATERIAL_WORLD_RECONCILE_R124_H
#define LID_MATERIAL_WORLD_RECONCILE_R124_H
#include "material_host_callbacks.h"
/* Append-only retired manager addresses. Foreign dispatch reads immutable
   entries only after acquire of count. No game objects are written. */
enum {R124_RETIRED_MAX=128};
typedef struct R124World {uint32_t busy,count;uint64_t active,retired[R124_RETIRED_MAX];} R124World;
static int r124_retired(const R124World*w,uint64_t self){
 if(!w||!self||__atomic_load_n(&w->active,__ATOMIC_ACQUIRE)==self)return 0;
 uint32_t n=__atomic_load_n(&w->count,__ATOMIC_ACQUIRE);if(n>R124_RETIRED_MAX)return 1;
 for(unsigned i=0;i<n;i++)if(w->retired[i]==self)return 1;return 0;
}
/* Called on existing pre-input request, using a verified CURRENT snapshot.
   1=old unaccepted intent retired,0=no pending/world unchanged,-1=refused.
   Never read the old actor, stage, manager or inventory after scene change. */
static int r124_reconcile(R124World*w,R108Host*h,const R104Snapshot*current,const R81Layout*fresh_layout){
 R104Snapshot again;R82Stamp stamp;R99Capability request;R87Identity stage;uint64_t stage_object;int result=-1;uint8_t state;uint32_t flags;int32_t selected;
 if(!w||!h||!current||!r108_owner(h)||w->busy)return -1;
 unsigned recovering=h->pickup.fault!=0;
 if(!h->pickup.pending&&!recovering)return 0;
 if(h->route.busy||h->pickup.busy||h->pickup.receipt.busy||h->pickup.receipt.accepted||h->pickup.receipt.fault||
 (!recovering&&h->selected_drop.prepared)||h->selected_drop.fault||h->selected_drop.busy||h->selected_drop.receipt.invoked||h->selected_drop.done||!r108_module(h)||
 !h->ops.active_snapshot||!r104_stamp_valid(&current->stamp)||!h->queue||!h->mailbox)return -1;
 w->busy=1;r108_stamp_from(current,&stamp);
 if(!r82_stamp_valid(&stamp)||!r15_same_context(&stamp.context,&h->pickup.event.stamp.context)||!r15_same_context(&stamp.context,&h->queue->context)||
 !__atomic_load_n(&h->queue->initialized,__ATOMIC_ACQUIRE)||h->queue->owner!=h->route.owner||
 !r100_owner_request(h->mailbox,h->route.owner,&request)||!r15_same_context(&request.context,&stamp.context)||
 !h->ops.active_snapshot(h->ops.context,stamp.player,current->view.graph.manager.object,&again)||
 !r74_same_stamp(&again.stamp,&current->stamp)||!r103_same_view(&again.view,&current->view)||
 !r6_at(&h->ops.memory,current->view.game.object,0x13e8,&stage_object,8)||
 !r87_identity(&h->ops.memory,stage_object,"BrgStageManager",h->binding.image+0x21c3350,&stage))goto end;
 if(!strcmp(stamp.floor,h->pickup.event.stamp.floor)&&!strcmp(stamp.area,h->pickup.event.stamp.area)&&
 r87_same_identity(&stage,&h->pickup.view.stage)&&r87_same_identity(&current->view.game,&h->pickup.view.game)&&
 r87_same_identity(&current->view.graph.player,&h->pickup.view.graph.player)&&r87_same_identity(&current->view.graph.manager,&h->pickup.view.graph.manager)&&!recovering){result=0;goto end;}
 uint64_t manager=current->view.graph.manager.object;
 if(!r6_at(&h->ops.memory,manager,0x92cc0,&state,1)||!r6_at(&h->ops.memory,manager,0x92cc8,&selected,4)||
 !r6_at(&h->ops.memory,manager,0x92cf8,&flags,4)||(flags&15))goto end;
 if(recovering&&(state!=2||h->pickup.receipt.phase))goto end;
 if(state==0){if(selected!=-1)goto end;}
 else if(state==2){R81Sample s;R81Layout layout=fresh_layout?*fresh_layout:h->layout;
  if(!r81_layout_valid(&layout)||layout.object_class!=current->view.graph.manager.cls||r81_sample(&h->ops.memory,manager,&layout,&s)<0)goto end;
 }else goto end;
 if(!r108_module(h))goto end;
 if(manager!=h->manager){
  uint32_t n=__atomic_load_n(&w->count,__ATOMIC_ACQUIRE);if(n>R124_RETIRED_MAX)goto end;unsigned i;for(i=0;i<n;i++)if(w->retired[i]==h->manager)break;
  if(i==n){if(n>=R124_RETIRED_MAX)goto end;w->retired[n]=h->manager;__atomic_store_n(&w->count,n+1,__ATOMIC_RELEASE);}
 }
 __atomic_store_n(&w->active,manager,__ATOMIC_RELEASE);
 if(recovering){R114Ops ops=h->selected_drop.ops;memset(&h->selected_drop,0,sizeof(h->selected_drop));h->selected_drop.ops=ops;h->pickup.fault=0;}
 h->pickup.pending=0;h->pickup.cancelled++;memset(&h->pickup.event,0,sizeof(h->pickup.event));r107_release(&h->route,&h->pickup);result=1;
end:w->busy=0;return result;
}
#endif
