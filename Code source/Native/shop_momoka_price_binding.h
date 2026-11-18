#ifndef LID_SHOP_MOMOKA_PRICE_BINDING_H
#define LID_SHOP_MOMOKA_PRICE_BINDING_H
#include "shop_momoka_skill_price.h"
/* Passive post-original reader. No hooks, calls to GetSkill, reservations,
   engine writes or save operations. Host proves exact native getter/build,
   thread and live VM scope. Do not use the old pre-original M204 observation
   as a source of initialized skillData. Function metadata stays host-rooted. */
enum { LID_MPB_NONE=0, LID_MPB_BEFORE_UI_CHARGE=1, LID_MPB_AFTER_UI_CHARGE=2 };
typedef struct LidMPPriceBinding {
    unsigned ready;
    M196Binding scripts;
    LidMPSkillLayout buy,add;
} LidMPPriceBinding;
typedef struct LidMPPriceObservation {
    unsigned boundary;
    LidMPRow row; /* Owned ID and currency values only; no VM pointers. */
} LidMPPriceObservation;
/* Prepare once under a real metadata/lifetime window. Never publish one valid
   layout while the other failed. Only initialized output is immutable. */
static int lid_mp_price_bind(const M195Ops*o,const M196Binding*scripts,
                             uint64_t structure,LidMPPriceBinding*out){
    LidMPPriceBinding b={0};
    if(!o||!scripts||!out||out->ready||!scripts->menu||!scripts->menu_class||
       !scripts->owner||!scripts->buy.function||!scripts->add.function||
       scripts->buy.function==scripts->add.function||
       scripts->buy.owner!=scripts->owner||scripts->add.owner!=scripts->owner||
       scripts->buy.has_num||scripts->buy.has_return||
       !scripts->add.has_num||!scripts->add.has_return||
       !m195_named(o,scripts->buy.function,"BuySticker")||
       !m195_named(o,scripts->add.function,"AddHaveSkillList")||
       !lid_mp_skill_layout(o,&scripts->buy,structure,&b.buy)||
       !lid_mp_skill_layout(o,&scripts->add,structure,&b.add))return 0;
    b.scripts=*scripts;b.ready=1;*out=b;return 1;
}
static int lid_mp_price_context_same(const M196Context*a,const M196Context*b){
    return a->frame==b->frame&&a->node==b->node&&a->locals==b->locals&&
      a->parent==b->parent&&a->parent_node==b->parent_node&&
      a->parent_locals==b->parent_locals&&a->kind==b->kind&&a->num==b->num;
}
/* Call synchronously AFTER original GetSkill, BEFORE the script resumes. The
   four-byte result comes from native execGetSkill's output, not ADD's return
   value. offer is resolved by host catalogue, never inferred from skill name.
   Unknown/failed/moved frame leaves output untouched and never alters native.
   Structural rechecks do not replace the caller's live-frame/thread scope. */
static int lid_mp_price_post(const M195Ops*o,const LidMPPriceBinding*b,
   uint64_t frame,uint64_t getter_result,unsigned offer,LidMPPriceObservation*out){
    uint32_t result=0,after_result=0;M197SkillSnapshot s={0};M196Context after={0};
    LidMPPriceObservation observation={0};const LidMPSkillLayout*layout;
    if(!o||!b||!b->ready||!out||offer>=4096||
       !m195_read(o,getter_result,0,&result,4)||result!=1||
       !m197_capture(o,&b->scripts,frame,&s))return 0;
    if(s.context.kind==M196_PRICE){
        layout=&b->buy;observation.boundary=LID_MPB_BEFORE_UI_CHARGE;
    }else if(s.context.kind==M196_BUY_ADD&&s.context.num==1){
        layout=&b->add;observation.boundary=LID_MPB_AFTER_UI_CHARGE;
    }else return 0;
    if(!lid_mp_skill_price(o,layout,&s,result,offer,&observation.row)||
       !m196_context(o,&b->scripts,frame,&after)||
       !lid_mp_price_context_same(&s.context,&after)||
       !m195_read(o,getter_result,0,&after_result,4)||after_result!=result)return 0;
    *out=observation;return 1;
}
/* Inactive adapter for a future host. No observer/read scope spans original.
   Host enter must verify current native binding, receiver, thread, rooted
   cached layouts and catalogue offer before returning a scope. On failure it
   cleans partial acquisition itself. All provider callbacks must be bounded,
   nonthrowing and avoid engine calls, IO, native mutation and reentry.
   Original exceptional exit propagates without acquiring this read scope. */
typedef struct LidMPPostHost {
    void*context;
    void(*original)(void*,void*,void*);
    uint32_t(*error_get)(void*);
    void(*error_set)(void*,uint32_t);
    int(*enter)(void*,uint64_t,uint64_t,M195Ops*,const LidMPPriceBinding**,unsigned*);
    void(*leave)(void*);
    void(*record)(void*,const LidMPPriceObservation*);
} LidMPPostHost;
static void lid_mp_price_observe_after(const LidMPPostHost*h,void*receiver,void*frame,void*result){
    /* A published wrapper must always have a resident valid original. */
    if(!h||!h->original)return;
    h->original(receiver,frame,result);
    if(!h->error_get||!h->error_set)return;
    uint32_t returned_error=h->error_get(h->context);
    if(h->enter&&h->leave&&h->record){
        M195Ops memory={0};const LidMPPriceBinding*binding=0;unsigned offer=4096;
        if(h->enter(h->context,(uint64_t)(uintptr_t)receiver,(uint64_t)(uintptr_t)frame,
                    &memory,&binding,&offer)){
            LidMPPriceObservation observation={0};
            if(lid_mp_price_post(&memory,binding,(uint64_t)(uintptr_t)frame,
                                 (uint64_t)(uintptr_t)result,offer,&observation))
                h->record(h->context,&observation);
            h->leave(h->context);
        }
    }
    h->error_set(h->context,returned_error);
}
#endif
