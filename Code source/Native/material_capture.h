#ifndef LID_MATERIAL_CAPTURE_R95_H
#define LID_MATERIAL_CAPTURE_R95_H
#include "material_queue.h"
#include "material_completion.h"
#include "material_source_plan.h"
#include <string.h>
/* Caller supplies actual verified locked rotation and pinned source plan.
   Build only: no queue sequence allocation, native calls or game pointers. */
static int r95_material_draft(R94MaterialFrame*out,const R82Event*e,const R83SourcePlan*source,const char*rotation){
 R94MaterialFrame f={0};
 if(!out||!e||!source||!rotation||strcmp(rotation,"4HMA")||!r82_stamp_valid(&e->stamp)||
 e->sample.state!=4||!(e->sample.flags&1)||(e->sample.flags&14)||source->create_type>4||
 e->sample.slot<0||e->sample.slot>=496||e->sample.unique<0||e->material<1||e->material>46)return 0;
 f.context=e->stamp.context;f.epoch=e->stamp.epoch;f.slot=(uint32_t)e->sample.slot;f.unique=e->sample.unique;
 f.actor_name=e->sample.actor_name;f.actor_number=e->sample.actor_number;f.ordinal=e->material;f.gettime=e->sample.gettime;f.source_kind=source->create_type;
 memcpy(f.floor,e->stamp.floor,16);memcpy(f.area,e->stamp.area,32);memcpy(f.rotation,"4HMA",5);memcpy(f.entity_id,e->sample.entity_id,96);memcpy(f.item_id,e->sample.item_id,64);
 f.sequence=1;if(!r94_material_valid(&f))return 0;f.sequence=0;*out=f;return 1;
}
static int r95_capture_material(R15Queue*q,uint32_t thread,unsigned catalogue,const R82Event*e,const R83SourcePlan*source,const char*rotation){
 R94MaterialFrame draft;if(!r95_material_draft(&draft,e,source,rotation))return 0;
 return r94_queue_push_material(q,thread,catalogue,&draft);
}
#endif
