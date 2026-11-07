#ifndef LID_MATERIAL_HOST_RUNTIME_R108_H
#define LID_MATERIAL_HOST_RUNTIME_R108_H
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
#include "material_host_callbacks.h"
#include "material_activation_proof.h"
#include "material_input_slot.h"
#include "material_preinput_gate.h"
#include "material_world_reconcile.h"
static R124World g_material_world;
static R105Slot g_material_input_slot;
static R105SlotOps g_material_input_ops;
static R121InputGate g_material_input_gate;
static unsigned g_material_early_attempted;
static R105Hash g_material_owner_measurement;
static const R105Hash*g_material_activation_hash;
static R108Host g_material_host;
static int checks_material_slot_protect(void*unused,uint64_t p,size_t n,uint32_t flags,uint32_t*old){
 (void)unused;DWORD before=0;if(!old||!g_win.VirtualProtect||!g_win.VirtualProtect((void*)(ULONG_PTR)p,n,flags,&before))return 0;*old=before;return 1;
}
static int checks_material_slot_cas(void*unused,uint64_t p,uint64_t expected,uint64_t replacement,uint64_t*observed){
 (void)unused;if(!observed||(p&7)||!readable_range((void*)(ULONG_PTR)p,8))return 0;
 int ok=__atomic_compare_exchange_n((uint64_t*)(ULONG_PTR)p,&expected,replacement,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);*observed=expected;return ok;
}
static int checks_material_callback_module(void*context){
 R108Host*h=context;if(!g_material_activation_hash)return 0;
 int scope=checks_chest_scope_begin(0);
 int ok=r122_owned_module(g_material_activation_hash,&g_material_input_slot,&h->slot,&h->slot_ops,&h->binding);
 checks_chest_scope_end(0,scope);return ok;
}
static int checks_material_callback_snapshot(void*unused,uint64_t player,uint64_t manager,R104Snapshot*out){
 (void)unused;return checks_material_owner_snapshot(player,manager,out);
}
static int checks_material_page_writable(void*unused,uint64_t address,size_t size){
 (void)unused;if(!size||address>UINT64_MAX-size||!g_win.VirtualQuery)return 0;
 uint64_t end=address+size;
 while(address<end){
  MEMORY_BASIC_INFORMATION mbi;
  if(g_win.VirtualQuery((void*)(ULONG_PTR)address,&mbi,sizeof(mbi))!=sizeof(mbi)||mbi.State!=0x1000||
  (mbi.Protect&(0x100|1))||!(mbi.Protect&(4|8|0x40|0x80)))return 0;
  uint64_t next=(uint64_t)(ULONG_PTR)mbi.BaseAddress+(uint64_t)mbi.RegionSize;
  if(next<=address)return 0;address=next<end?next:end;
 }
 return 1;
}
static int checks_material_store(void*unused,uint64_t p,const void*value,size_t n){
 (void)unused;lid_memcpy((void*)(ULONG_PTR)p,value,n);return 1;
}
/* Callback table only, no slot modification/admission. Preserve all receipt
   and transaction storage; this is not a lifecycle reset. */
static int checks_material_callback_init(uint32_t owner){
 R108Host*h=&g_material_host;
 if(!owner||!g_get_tid||g_get_tid()!=owner||owner!=g_checks_diag.owner||(h->route.owner&&h->route.owner!=owner))return 0;
 h->route.owner=owner;
 h->slot_ops=(R105SlotOps){g_checks_ops,0,checks_chest_thread,checks_material_slot_protect,checks_material_slot_cas};
 h->ops=(R108Ops){g_checks_ops,h,checks_chest_thread,checks_material_callback_module,checks_material_callback_snapshot,
                 checks_material_current_game,checks_material_page_writable,checks_material_store};
 h->queue=&g_checks_delivery.queue;h->mailbox=&g_checks_delivery.material_mailbox;return 1;
}
#endif
#endif
