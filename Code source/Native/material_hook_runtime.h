#ifndef LID_MATERIAL_HOOK_RUNTIME_R109_H
#define LID_MATERIAL_HOOK_RUNTIME_R109_H
#include "performance_events.h"
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
static void WINAPI checks_material_item_process(void*self);
/* Only called by the existing free-slot wrapper at the exact native pickup
   return site, after the preserved R45 player/reload guards. One-time owner hash fallback may perform bounded file I/O. */
static int checks_material_bag_request_core(void*self,int index,uint64_t caller,int native_space,int*out){
 R108Host*h=&g_material_host;R104Snapshot snapshot;R81Layout layout;R81Sample sample;R99Capability request;
 uint64_t image=(uint64_t)(ULONG_PTR)g_checks_diag.exe;uint32_t owner=g_checks_diag.owner;char digest[65];
 if(!out||!image||caller!=image+0x10cd01e||index!=-1||!owner||!g_get_tid||g_get_tid()!=owner)return 0;
 if(!r105_slot_owned(&g_material_input_slot,&g_material_input_ops))return 0; /* no pre-input guard => activation unavailable, never READY */
 if(!h->route.busy&&!h->pickup.busy&&r130_ordinary_matches(&g_material_input_gate.ordinary,&g_checks_ops,image,
  g_material_input_gate.ordinary.manager,(uint64_t)(ULONG_PTR)self,2))return 0; /* preserve actual native free slot/full menu */
 g_material_input_gate.material_stage=1;if(!r100_owner_request(&g_checks_delivery.material_mailbox,owner,&request))return 0;
 if(h->route.busy||h->pickup.busy){*out=-1;return 1;}
 g_material_input_gate.material_stage=2;if(!checks_material_callback_init(owner))return 0;
 g_material_input_gate.material_stage=3;if(!checks_material_owner_snapshot((uint64_t)(ULONG_PTR)self,0,&snapshot)||
 !r15_same_context(&request.context,&snapshot.stamp.context)){
  /* A catalogue5 request is already bound. Without a current graph/context
     we cannot prove this pickup ordinary: wait instead of bypassing AP. */
  *out=-1;return 1;
 }
 uint64_t manager=snapshot.view.graph.manager.object;
 g_material_input_gate.material_stage=4;layout=h->layout;
 if(!r81_layout_valid(&layout)||layout.object_class!=snapshot.view.graph.manager.cls||layout.vtable!=image+0x20f8d60){
  int scoped=checks_chest_scope_begin(0);
  int bound=r81_bind(&g_checks_ops,manager,image+0x20f8d60,&layout);
  checks_chest_scope_end(0,scoped);
  if(!bound){*out=-1;return 1;}
 }
 g_material_input_gate.material_stage=5;int read=r81_sample(&g_checks_ops,manager,&layout,&sample);
 if(read!=1||!r82_material_lookup(sample.item_id)){
  if(h->pickup.pending&&h->manager==manager)(void)r118_admit_selection(h,1,caller,(uint64_t)(ULONG_PTR)self,(uint64_t)(ULONG_PTR)self,index);
  if(read>=0)return 0; /* proven ordinary pickup keeps real capacity even if an old intent is fenced */
  if(__atomic_load_n(&h->route.fence,__ATOMIC_ACQUIRE)==manager){*out=native_space;return 1;}return 0;
 }
 /* Measure before completion installation; worker/owner proof storage is independent.
    Failure branch below is intentionally still marked NONDEPLOYABLE. */
 *out=-1;
 g_material_input_gate.material_stage=6;int proof_source=r119_activation_proof(&g_checks_delivery.material_hash,&g_material_owner_measurement,&g_checks_delivery.material_crypto,&request,&snapshot.stamp.context,owner,image,digest);
 if(!proof_source)return 1; /* STILL BLOCKING: -1 is not a neutral refusal. */
 g_material_activation_hash=proof_source==R119_WORKER_PROOF?&g_checks_delivery.material_hash:&g_material_owner_measurement;
 if(!h->slot.attempted){
  R91Binding binding;
  g_material_input_gate.material_stage=7;if(!r91_module_bind(&g_checks_ops,image,digest,&binding))return 1;
  h->binding=binding;
  g_material_input_gate.material_stage=8;if(!r105_slot_install(&h->slot,&h->slot_ops,&h->binding,owner,(uint64_t)(ULONG_PTR)checks_material_item_process))return 1;
 }
 g_material_input_gate.material_stage=9;if(!checks_material_callback_module(h))return 1;
 g_material_input_gate.material_stage=10;if(h->pickup.pending){if(h->manager!=manager)return 1;}
 else if(!r108_bind_callbacks(h,manager,&snapshot.fields,&layout,&h->binding))return 1;
 __atomic_store_n(&g_material_world.active,h->manager,__ATOMIC_RELEASE);
 g_material_input_gate.material_stage=11;int result=r118_admit_selection(h,1,caller,(uint64_t)(ULONG_PTR)self,snapshot.view.graph.player.object,index);
 if(result!=R82_MANAGED)return 1;
 R99Proof proof={owner,g_get_tid(),(unsigned)checks_material_callback_module(h),(unsigned)r105_slot_owned(&h->slot,&h->slot_ops),
                 (unsigned)(r93_ops(&h->transaction)&&r81_layout_valid(&h->layout)),
                 (unsigned)(__atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)==R26_ACTIVE&&!__atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)),
                 (unsigned)checks_delivery_rotation_ready(owner)};
 (void)r100_owner_ready(&g_checks_delivery.material_mailbox,&proof);
 g_material_input_gate.material_stage=12;*out=native_space;return 1; /* completion owned; preserve full menu/free slot */
}
static int checks_material_bag_request(void*self,int index,uint64_t caller,int native_space,int*out){
 if(caller!=(uint64_t)(ULONG_PTR)g_checks_diag.exe+0x10cd01e)
  return checks_material_bag_request_core(self,index,caller,native_space,out);
 LID_PERF_BEGIN(pickup_bag_time);int result=checks_material_bag_request_core(self,index,caller,native_space,out);
 LID_PERF_END(pickup_bag_time,41,"pickup_bag_admission",result);return result;
}
static void WINAPI checks_material_item_process_core(void*self){
 R108Host*h=&g_material_host;
 R107Original original=(R107Original)(ULONG_PTR)__atomic_load_n(&h->slot.original,__ATOMIC_ACQUIRE);
 uint32_t tid=g_get_tid?g_get_tid():0;uint64_t object=(uint64_t)(ULONG_PTR)self;
#if LID_MATERIAL_PICKUP_OPTIMIZED_ENABLED
 /* Native1410B5F70 does no pickup work without GetFlag. A fully clean
    transaction has no AP completion/cleanup to perform either. Keep receipt
    evidence intact; every real get/cancel/death/fault takes the checked path. */
 if(tid==h->route.owner&&h->manager==object&&!h->route.busy&&!h->pickup.busy&&
    !h->pickup.pending&&!h->pickup.fault&&!h->pickup.receipt.busy&&!h->pickup.receipt.fault&&
    !h->selected_drop.prepared&&!h->selected_drop.done&&!h->selected_drop.busy&&!h->selected_drop.fault&&
    !__atomic_load_n(&h->route.fence,__ATOMIC_ACQUIRE)){
  uint64_t vt;uint8_t state;uint32_t flags;
  if(r6_at(&g_checks_ops,object,0,&vt,8)&&vt==(uint64_t)(ULONG_PTR)g_checks_diag.exe+0x20f8d60&&
     r6_at(&g_checks_ops,object,0x92cc0,&state,1)&&(state==0||state==4)&&
     r6_at(&g_checks_ops,object,0x92cf8,&flags,4)&&!(flags&15))return;
 }
#endif
 /* Native ItemGetProcess returns immediately before mItemGetFlag is set.
    Do not re-run the complete AP graph/module proof on every animation frame.
    Final get/cancel/death events still take the entire checked path below. */
 if(tid==h->route.owner&&h->manager==object&&h->pickup.pending&&!h->route.busy&&!h->pickup.busy){
  uint64_t vt;uint8_t state;uint32_t flags;
  if(r6_at(&g_checks_ops,object,0,&vt,8)&&vt==(uint64_t)(ULONG_PTR)g_checks_diag.exe+0x20f8d60&&
   r6_at(&g_checks_ops,object,0x92cc0,&state,1)&&state==4&&r6_at(&g_checks_ops,object,0x92cf8,&flags,4)&&!(flags&15))return;
 }
 if(tid==g_material_input_slot.owner&&!h->route.busy&&!h->pickup.busy&&checks_material_callback_module(h)&&
 r130_ordinary_matches(&g_material_input_gate.ordinary,&g_checks_ops,(uint64_t)(ULONG_PTR)g_checks_diag.exe,object,0,4)){
  memset(&g_material_input_gate.ordinary,0,sizeof(g_material_input_gate.ordinary));
  (void)r107_forward(original,object);return; /* no material receipt is cleared or edited */
 }
 if(r124_retired(&g_material_world,object))return; /* never forward retired pending manager */
 if(tid!=h->route.owner){(void)r107_dispatch(&h->route,0,0,tid,0,object,original);return;}
 int bound=h->manager==object&&checks_material_callback_module(h)&&r93_ops(&h->transaction);
 R82Stamp stamp;int enabled=bound&&r108_snapshot(h,&stamp);
 if(bound)(void)r116_host_dispatch(h,tid,enabled,object,original);
 else (void)r107_dispatch(&h->route,&h->pickup,0,tid,0,object,original);
}
static void WINAPI checks_material_item_process(void*self){
 uint32_t flags;
 if(g_get_tid&&g_get_tid()==g_material_input_slot.owner&&
  r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)self,0x92cf8,&flags,4)&&(flags&1)){
  LID_PERF_BEGIN(pickup_complete_time);checks_material_item_process_core(self);
  LID_PERF_END(pickup_complete_time,42,"pickup_completion",1);return;
 }
 checks_material_item_process_core(self);
}
#endif
#endif
