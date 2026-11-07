#ifndef LID_MATERIAL_COMMIT_PIPELINE_R112_H
#define LID_MATERIAL_COMMIT_PIPELINE_R112_H
#include "material_native_calls.h"
#include "material_ui_graph.h"
#include "material_completion.h"
#include "material_read_scope.h"
/* Research commit pipeline. Context callback proves live owner/AP stamp and
   current GameInfo, using the existing resolver in future integration. */
enum {R92_OK=1,R92_DUPLICATE=2,R92_PRECHECK=-1,R92_QUEUE_REFUSED=-2,R92_BUSY=-3,R92_CAPTURED_FAULT=-4};
enum {R92_CAPTURED=1,R92_MARKED,R92_EFFECTED,R92_ERASED,R92_INFO_HIDDEN,R92_GUIDE_HIDDEN,R92_RESET};
typedef struct R92View {R87Graph graph;R87Identity game,stage;} R92View;
/* prepare validates and pins selected-slot evidence without side effects.
   finish performs at most one native drop when occupied and proves outcome.
   Both callbacks are mandatory for this private full-bag pipeline. */
typedef struct R112DropPlan {int slot;unsigned occupied;uint64_t identity[4];} R112DropPlan;
typedef struct R112DropState {unsigned invoked,verified;uint64_t actor;} R112DropState;
typedef struct R92Ops {
 int(*drop_prepare)(void*,const R82Event*,R112DropPlan*);
 int(*drop_finish)(void*,const R82Event*,const R112DropPlan*,R112DropState*);
 R6Ops memory;void*context;uint32_t(*thread)(void*);
 int(*current_game)(void*,const R82Event*,uint64_t*);
 int(*writable)(void*,uint64_t,size_t);int(*write)(void*,uint64_t,const void*,size_t);
 int(*enqueue)(void*,const R82Event*);R91Binding binding;R87Fields fields;const R81Layout*layout;uint32_t owner;
} R92Ops;
typedef struct R92Receipt {unsigned busy,accepted,phase,fault;R112DropPlan drop_plan;R112DropState drop_state;R82Event event;R83SourcePlan source;R92View view;} R92Receipt;
static int r92_view_graph(const R92Ops*o,const R82Event*e,uint64_t game,R92View*out){
 R92View v={0};uint64_t stage;
 if(!r87_identity(&o->memory,game,"BrgGameInfoNative",0,&v.game)||
 !r6_at(&o->memory,game,0x13e8,&stage,8)||!r87_identity(&o->memory,stage,"BrgStageManager",o->binding.image+0x21c3350,&v.stage)||
 !r87_graph(&o->memory,e->sample.manager,e->stamp.player,o->binding.image,&o->fields,&v.graph))return 0;
 *out=v;return 1;
}
static int r92_view(const R92Ops*o,const R82Event*e,R92View*out){
 uint64_t game;
 if(!o||!e||!out||!o->thread||o->thread(o->context)!=o->owner||!o->current_game||
    !o->current_game(o->context,e,&game))return 0;
 /* current_game may call host callbacks: acquire only after it returns. */
 int scope=R152_READ_BEGIN();int result=r92_view_graph(o,e,game,out);
 R152_READ_END(scope);return result;
}
static int r92_same_view(const R92View*a,const R92View*b){return r87_same_graph(&a->graph,&b->graph)&&r87_same_identity(&a->game,&b->game)&&r87_same_identity(&a->stage,&b->stage);}
static int r92_still(const R92Ops*o,const R92Receipt*r,int record){
 R92View now;R81Sample s;R83SourcePlan source;
 if(!r92_view(o,&r->event,&now)||!r92_same_view(&r->view,&now))return 0;
 if(record&&(r81_sample(&o->memory,r->event.sample.manager,o->layout,&s)!=1||s.state!=4||!(s.flags&1)||(s.flags&14)||
 !r82_same_sample(&s,&r->event.sample)||!r83_source_plan(&o->memory,&s,&source)||!r83_source_equal(&source,&r->source)))return 0;
 return 1;
}
static int r92_write_ready(const R92Ops*o,uint64_t manager){return o->writable&&o->write&&
 o->writable(o->context,manager+0x92cc8,4)&&o->writable(o->context,manager+0x92cc0,1)&&o->writable(o->context,manager+0x92cf8,4);}
static int r92_commit(R92Receipt*r,const R92Ops*o,const R82Event*e,const R83SourcePlan*source){
 int result=R92_PRECHECK;uint32_t flags;int32_t selected=-1;uint8_t state=0;uint64_t manager;
 if(!r||!o||!e||!source||!o->owner||!o->thread||o->thread(o->context)!=o->owner)return R92_PRECHECK;
 if(r->busy)return R92_BUSY;
 if(r->accepted)return !r->fault&&r->phase==R92_RESET&&r82_same_stamp(&r->event.stamp,&e->stamp)&&r82_same_sample(&r->event.sample,&e->sample)&&r->event.material==e->material&&r83_source_equal(&r->source,source)?R92_DUPLICATE:R92_CAPTURED_FAULT;
 if(!o->drop_prepare||!o->drop_finish||!o->enqueue||!r81_layout_valid(o->layout)||!o->binding.image||!r82_stamp_valid(&e->stamp)||!e->material||e->material>46)return R92_PRECHECK;
 for(unsigned i=0;i<R91_ENTRIES;i++)if(!o->binding.entry[i])return R92_PRECHECK;
 r->busy=1;r->event=*e;r->source=*source;manager=e->sample.manager;
 if(!r92_view(o,e,&r->view)||!r82_actor(&o->memory,&e->sample)||!r92_still(o,r,1)||!r92_write_ready(o,manager)||!r92_still(o,r,1))goto done;
 if(!o->drop_prepare(o->context,e,&r->drop_plan)||r->drop_plan.slot<0||r->drop_plan.slot>=99||r->drop_plan.occupied>1||!r92_still(o,r,1))goto done;
 /* Bounded capture only. Receipt survives any failure below: never recapture
    or repeat irreversible effects automatically after acceptance. */
 if(o->enqueue(o->context,e)!=1){result=R92_QUEUE_REFUSED;goto done;}
 r->accepted=1;r->phase=R92_CAPTURED;result=R92_CAPTURED_FAULT;
 /* Queue acceptance precedes any irreversible old-item drop. Receipt and
    busy guard survive failure/reentry, so this callback is never retried. */
 if(!r92_still(o,r,1)||!o->drop_finish(o->context,e,&r->drop_plan,&r->drop_state)||!r->drop_state.verified||r->drop_state.invoked!=r->drop_plan.occupied)goto fault;
 if(!r92_still(o,r,1)||r91_mark(&o->binding,r->view.stage.object,source)<0)goto fault;r->phase=R92_MARKED;
 if(!r92_still(o,r,1)||!r91_drop_call(&o->binding,R91_EFFECT,manager,e->sample.slot,e->sample.unique))goto fault;r->phase=R92_EFFECTED;
 if(!r92_still(o,r,1)||!r91_drop_call(&o->binding,R91_ERASE,manager,e->sample.slot,e->sample.unique))goto fault;
 {uint32_t use;int32_t unique,sel;
  uint64_t record=manager+0x74+(uint64_t)(uint32_t)e->sample.slot*R81_STRIDE;
  if(!r6_at(&o->memory,record,0,&use,4)||(use&1)||!r6_at(&o->memory,record,4,&unique,4)||unique!=-1||
     !r6_at(&o->memory,manager,0x92cc8,&sel,4)||sel!=-1)goto fault;
 }r->phase=R92_ERASED;
 if(!r92_still(o,r,0)||!r91_hide_info(&o->binding,r->view.graph.info.object))goto fault;r->phase=R92_INFO_HIDDEN;
 if(!r92_still(o,r,0)||!r91_hide_guide(&o->binding,r->view.graph.guide.object))goto fault;r->phase=R92_GUIDE_HIDDEN;
 if(!r92_still(o,r,0)||!r92_write_ready(o,manager)||!r6_at(&o->memory,manager,0x92cf8,&flags,4))goto fault;
 flags&=~3u;
 if(!o->write(o->context,manager+0x92cc8,&selected,4)||!o->write(o->context,manager+0x92cc0,&state,1)||!o->write(o->context,manager+0x92cf8,&flags,4))goto fault;
 r->phase=R92_RESET;result=R92_OK;goto done;
fault:r->fault=1;
done:r->busy=0;return result;
}
#endif
