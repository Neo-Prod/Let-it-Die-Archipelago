#ifndef LID_MATERIAL_PREINPUT_GATE_R121_H
#define LID_MATERIAL_PREINPUT_GATE_R121_H
#include "material_identity.h"
#include "material_record_reader.h"
#include "material_ordinary_pickup.h"
/* Only the verified Select2 input call (RVA10CCED2) may be gated.
   Original ABI uint32(this), no params. Hook installation is separate. */
typedef struct R121InputOps {
 R6Ops memory;void*context;uint32_t owner;uint64_t image,manager;const R81Layout*layout;
 uint32_t(*thread)(void*);uint32_t(*original)(void*);
 /* 0=no material REQUEST,1=current REQUEST,-1=REQUEST authority uncertain */
 int(*request)(void*);
 /* MUST install/prove completion AND pin the pending intention before 1. */
 int(*prepare)(void*,const R81Sample*);
} R121InputOps;
typedef struct R121InputGate {unsigned busy,blocked,admitted;R130Ordinary ordinary;unsigned diagnostic,material_stage;} R121InputGate;
static uint32_t r121_preinput(R121InputGate*g,const R121InputOps*o,uint64_t self,uint64_t caller){
 if(!o||!o->original)return 0;
 uint32_t native=o->original((void*)(uintptr_t)self);
 if(!g||!o->thread||!o->owner||o->thread(o->context)!=o->owner||!o->image||o->image>UINT64_MAX-0x2200000||
 self!=o->manager||caller!=o->image+0x10cced2)return native;
 if(g->busy)return 0;
 memset(&g->ordinary,0,sizeof(g->ordinary));
 g->diagnostic=0;g->material_stage=0;
 if(!native)return native;
 g->busy=1;uint32_t result=0;R81Sample sample;R130Ordinary ordinary;
 if(r130_pickup_kind(&o->memory,o->image,self,&ordinary)==0){
  uint8_t state;uint32_t flags;
  if(r6_at(&o->memory,self,0x92cc0,&state,1)&&state==2&&r6_at(&o->memory,self,0x92cf8,&flags,4)&&!(flags&15)&&
   r130_native_player(&o->memory,o->image,self,&ordinary.player))g->ordinary=ordinary;
  g->diagnostic=1;result=native;goto end;
 }
 int request=o->request?o->request(o->context):-1;
 g->diagnostic=2;
 if(request==0){g->diagnostic=3;result=native;goto end;}
 if(request!=1)goto end;
 g->diagnostic=4;int read=r81_sample(&o->memory,self,o->layout,&sample);
 if(read==0){result=native;goto end;}
 if(read!=1)goto end; /* REQUEST exists; cannot prove this record ordinary. */
 if(!r82_material_lookup(sample.item_id)){result=native;goto end;}
 g->diagnostic=5;if(sample.state!=2||(sample.flags&15))goto end;
 g->diagnostic=6;if(!o->prepare||o->prepare(o->context,&sample)!=1)goto end;
 g->diagnostic=7;g->admitted++;result=native;
end:
 if(!result)g->blocked++;g->busy=0;return result;
}
#endif
