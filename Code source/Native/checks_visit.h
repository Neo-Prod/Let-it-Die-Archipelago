#ifndef LID_CHECKS_VISIT_R39_H
#define LID_CHECKS_VISIT_R39_H
#include "death_queue.h"
/* Pure copied notifications; no UObject/game pointers, disk or waiting. */
typedef struct R39VisitState {unsigned count;char floor[101][16],area[101][32];} R39VisitState;
static int r39_visit_capture(R39VisitState*s,R15Queue*q,uint32_t owner,
    const R15Context*context,uint64_t epoch,const char floor[16],const char area[32],int verified){
    if(!s||!q||!context||!owner||owner!=q->owner||!epoch||verified!=1||
       !__atomic_load_n(&q->initialized,__ATOMIC_ACQUIRE)||!r15_same_context(context,&q->context)||
       !r15_ascii(floor,16)||!r15_ascii(area,32)||!r22_map_floor(floor,area))return 0;
    if(s->count>101)return -1;
    for(unsigned i=0;i<s->count;i++)if(r15_equal(s->floor[i],floor,16)&&r15_equal(s->area[i],area,32))return 0;
    if(s->count==101){r15_queue_fault(q,owner,4);return -1;}
    R15Frame f;r15_zero(&f,sizeof(f));f.context=*context;f.kind=R39_VISIT;f.epoch=epoch;
    r15_copy(f.floor,floor,16);r15_copy(f.area,area,32);r15_copy(f.rotation,"4HMA",5);
    if(r15_queue_push(q,owner,&f)!=1)return -1;
    r15_copy(s->floor[s->count],floor,16);r15_copy(s->area[s->count],area,32);s->count++;return 1;
}
#endif
