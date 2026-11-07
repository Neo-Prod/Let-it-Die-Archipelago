#ifndef LID_RESEARCH_MATERIAL_ADMISSION_R81_H
#define LID_RESEARCH_MATERIAL_ADMISSION_R81_H
#include "material_record_reader.h"
/* Caller must capture __builtin_return_address(0) INSIDE the installed
   GetDeathBagSpace wrapper, not inside a helper. Live expected caller is
   verified EXE+10CD01E. This pure predicate never writes a real bag slot. */
static int r81_admission_scope(const R81Sample*s,uint64_t caller,uint64_t expected_caller,
    uint64_t expected_manager,uint64_t receiver,uint64_t linked_player,int character_index,
    int supported,int active,int bound,int owner_thread){
    return s&&expected_caller&&caller==expected_caller&&expected_manager&&s->manager==expected_manager&&
      receiver&&receiver==linked_player&&character_index==-1&&supported==1&&active==1&&bound==1&&owner_thread==1&&
      s->state==2&&!(s->flags&15)&&s->count>0&&s->count<=R81_COUNT&&s->slot>=0&&s->slot<R81_COUNT&&
      s->unique>=0&&s->actor&&s->actor_class&&s->item_id[0];
}
#endif
