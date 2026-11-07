#ifndef LID_MATERIAL_FIELD_WALK_R132_H
#define LID_MATERIAL_FIELD_WALK_R132_H
#include <stdint.h>
/* UStruct.Children contains functions, properties and other UFields. Match
   the existing4096 field bound without a large unprobed stack array.
   Brent cycle detection observes only the pointers already read by caller. */
typedef struct R132FieldWalk {uint64_t checkpoint;unsigned visited,power,span;} R132FieldWalk;
static int r132_field_visit(R132FieldWalk*w,uint64_t child){
 if(!w||!child||w->visited>=4096)return 0;
 if(!w->visited){w->checkpoint=child;w->power=1;w->span=0;}
 else{
  w->span++;
  if(child==w->checkpoint)return 0;
  if(w->span==w->power){w->checkpoint=child;w->power*=2;w->span=0;}
 }
 w->visited++;return 1;
}
#endif
