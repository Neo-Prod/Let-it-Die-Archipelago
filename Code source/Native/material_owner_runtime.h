#ifndef LID_MATERIAL_OWNER_RUNTIME_R104_H
#define LID_MATERIAL_OWNER_RUNTIME_R104_H
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
#include "material_owner_snapshot.h"
static R104Cache g_material_owner_cache;
static uint64_t checks_material_current_game(void*unused){
 (void)unused;return (uint64_t)(ULONG_PTR)progress_native_game(g_checks_diag.exe);
}
/* Existing stamp proves active session/queue/rotation/map/epoch on owner.
   The snapshot itself does not prove module SHA, hook ownership or writability.
   No automatic call site until pickup hook preconditions are bound. */
static int checks_material_owner_snapshot(uint64_t expected_player,uint64_t expected_manager,R104Snapshot*out){
 R104Ops ops={g_checks_ops,0,g_checks_diag.owner,(uint64_t)(ULONG_PTR)g_checks_diag.exe,
             checks_chest_thread,checks_chest_stamp,checks_material_current_game,checks_chest_scope_begin,checks_chest_scope_end};
 /* Snapshot path is read-only: cache VM region queries for this synchronous
    walk only. UObject/FName values are always read again, never cached. */
 int scope=checks_chest_scope_begin(0);
 int ok=r104_snapshot(&g_material_owner_cache,&ops,expected_player,expected_manager,out);
 checks_chest_scope_end(0,scope);return ok;
}
#endif
#endif
