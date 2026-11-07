#ifndef LID_MATERIAL_BAG_SLOT_R113_H
#define LID_MATERIAL_BAG_SLOT_R113_H
#include "checks_observer_metadata.h"
#include <string.h>
/* Read-only. Static provenance: CommonPawn GetBagItemNum RVA FA3500,
   GetBagSpace FA3640. This is NOT the serialized Chara bag (stride44). */
enum {R113_STRIDE=0x718};
typedef struct R113Array {uint64_t data;int32_t num,max;} R113Array;
typedef struct R113Slot {uint64_t player,image;R113Array array;int slot;unsigned occupied;uint8_t bytes[R113_STRIDE];} R113Slot;
static int r113_array(const R6Ops*o,uint64_t player,uint64_t image,R113Array*out){
 uint64_t vt;R113Array a;
 if(!out||!image||image>UINT64_MAX-0x2200000||!r6_copy(o,player,&vt,8)||vt!=image+0x20ac950||!r6_at(o,player,0x3e58,&a,sizeof(a))||
 a.num<0||a.num>99||a.max<a.num||a.max>4096||(!a.data)!=(a.max==0)||a.data>UINT64_MAX-(uint64_t)a.max*R113_STRIDE)return 0;
 *out=a;return 1;
}
static int r113_same_array(const R113Array*a,const R113Array*b){return a->data==b->data&&a->num==b->num&&a->max==b->max;}
static int r113_slot(const R6Ops*o,uint64_t player,uint64_t image,int slot,R113Slot*out){
 R113Slot s={0};R113Array end;uint8_t again[R113_STRIDE];uint64_t at;
 if(!out||slot<0||slot>=99||!r113_array(o,player,image,&s.array)||slot>=s.array.num)return 0;
 at=s.array.data+(uint64_t)(unsigned)slot*R113_STRIDE;
 if(!r6_copy(o,at,s.bytes,sizeof(s.bytes))||!r6_copy(o,at,again,sizeof(again))||memcmp(s.bytes,again,sizeof(again))||
 !r113_array(o,player,image,&end)||!r113_same_array(&s.array,&end))return 0;
 s.player=player;s.image=image;s.slot=slot;s.occupied=s.bytes[0]&1;*out=s;return 1;
}
static int r113_unchanged(const R6Ops*o,const R113Slot*s){
 R113Slot now;return s&&r113_slot(o,s->player,s->image,s->slot,&now)&&r113_same_array(&s->array,&now.array)&&!memcmp(s->bytes,now.bytes,R113_STRIDE);
}
/* Proves the selected slot is disabled after native handling. Does not
   prove actor spawn or physical recoverability of a dropped object. */
static int r113_empty_after(const R6Ops*o,const R113Slot*s){
 R113Slot now;return s&&r113_slot(o,s->player,s->image,s->slot,&now)&&r113_same_array(&s->array,&now.array)&&!now.occupied;
}
#endif
