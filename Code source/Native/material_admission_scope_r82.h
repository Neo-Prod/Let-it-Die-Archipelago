#ifndef LID_RESEARCH_MATERIAL_ADMISSION_R82_H
#define LID_RESEARCH_MATERIAL_ADMISSION_R82_H
#include "material_record_reader.h"
/* Unlike the earlier conservative R81 predicate, stale delete-cancel bit4
   is allowed at admission: native SetItemGetFlag(false) replaces that bit
   before invoking completion. It remains a refusal at completion itself. */
static int r82_admission_scope(const R81Sample*s,uint64_t caller,uint64_t expected_caller,
 uint64_t expected_manager,uint64_t receiver,uint64_t linked_player,int index,
 int supported,int active,int bound,int owner_thread){
 return s&&expected_caller&&caller==expected_caller&&expected_manager&&s->manager==expected_manager&&
 receiver&&receiver==linked_player&&index==-1&&supported==1&&active==1&&bound==1&&owner_thread==1&&
 s->state==2&&!(s->flags&11)&&s->count>0&&s->count<=R81_COUNT&&s->slot>=0&&s->slot<R81_COUNT&&
 s->unique>=0&&s->actor&&s->actor_class&&s->item_id[0];
}
#endif
