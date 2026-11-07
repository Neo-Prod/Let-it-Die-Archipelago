#ifndef LID_MATERIAL_ORDINARY_PICKUP_R130_H
#define LID_MATERIAL_ORDINARY_PICKUP_R130_H
#include "material_identity.h"
#include "material_record_reader.h"
/* Read-only classification for the hash-verified native input call. No AP
   context, reflection binding, inventory write or material receipt mutation. */
typedef struct R130Ordinary {
 uint64_t manager,player;int32_t slot,unique;uint8_t type;char id[64];
} R130Ordinary;
/* 0=proved ordinary,1=known AP material,-1=unknown. */
static int r130_pickup_kind(const R6Ops*ops,uint64_t image,uint64_t manager,R130Ordinary*out){
 R130Ordinary s={0};uint64_t vt,record;uint32_t use;
 if(!out||!image||image>UINT64_MAX-0x2200000||!manager||manager>UINT64_MAX-R81_EXTENT||
 !r6_at(ops,manager,0,&vt,8)||vt!=image+0x20f8d60||
 !r6_at(ops,manager,0x92cc8,&s.slot,4)||s.slot<0||s.slot>=R81_COUNT)return -1;
 record=manager+0x74+(uint64_t)(unsigned)s.slot*R81_STRIDE;
 if(!r6_at(ops,record,0,&use,4)||!(use&1)||!r6_at(ops,record,4,&s.unique,4)||s.unique<0||
 !r6_at(ops,record,0xe4,&s.type,1)||s.type>6)return -1;
 if(s.type==4){
  if(!r81_id(ops,record+0x230,s.id,sizeof(s.id),1))return -1;
  if(r82_material_lookup(s.id))return 1;
 }
 s.manager=manager;*out=s;return 0;
}
static int r130_native_player(const R6Ops*ops,uint64_t image,uint64_t manager,uint64_t*out){
 uint64_t ui,player,vt;
 if(!out||!r6_at(ops,manager,0x68,&ui,8)||!ui||
 !r6_at(ops,ui,0x2d4c,&player,8)||!player||!r6_at(ops,player,0,&vt,8)||vt!=image+0x20ac950)return 0;
 *out=player;return 1;
}
static int r130_ordinary_matches(const R130Ordinary*t,const R6Ops*ops,uint64_t image,uint64_t manager,uint64_t player,uint8_t state){
 R130Ordinary now;uint64_t current;uint8_t phase;uint32_t flags;
 if(!t||!t->manager||t->manager!=manager||!t->player||
 r130_pickup_kind(ops,image,manager,&now)!=0||now.slot!=t->slot||now.unique!=t->unique||now.type!=t->type||strcmp(now.id,t->id)||
 !r130_native_player(ops,image,manager,&current)||current!=t->player||(player&&current!=player)||
 !r6_at(ops,manager,0x92cc0,&phase,1)||phase!=state||!r6_at(ops,manager,0x92cf8,&flags,4))return 0;
 return state==2?!(flags&15):state==4&&(flags&15)==1;
}
#endif
