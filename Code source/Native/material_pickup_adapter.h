#ifndef LID_MATERIAL_PICKUP_ADAPTER_R93_H
#define LID_MATERIAL_PICKUP_ADAPTER_R93_H
#include "material_commit_pipeline.h"
#include "material_identity.h"
/* Consolidated owner state: historical R84/R88/R89/R90 adapters are not used.
   Engine-current proof excludes AP authority so disconnect can still cancel. */
typedef struct R93Ops {
 R92Ops commit;uint64_t manager,caller;
 int(*snapshot)(void*,R82Stamp*);
 int(*engine_current)(void*,uint64_t manager,uint64_t player);
} R93Ops;
typedef struct R93Pickup {
 unsigned busy,pending,fault,accepted,cancelled;
 R82Event event;R83SourcePlan source;R92View view;R92Receipt receipt;
} R93Pickup;
static int r93_ops(const R93Ops*o){
 if(!o||!o->manager||!o->caller||!o->snapshot||!o->engine_current||!o->commit.thread||!o->commit.owner||
 !o->commit.current_game||!o->commit.enqueue||!o->commit.writable||!o->commit.write||!o->commit.binding.image||!r81_layout_valid(o->commit.layout))return 0;
 for(unsigned i=0;i<R91_ENTRIES;i++)if(!o->commit.binding.entry[i])return 0;return 1;
}
static int r93_cancel_still(const R93Ops*o,const R93Pickup*t,const R87Graph*g){
 R87Graph fresh;R81Sample s;const R92Ops*p=&o->commit;
 return p->thread(p->context)==p->owner&&o->engine_current(p->context,o->manager,t->event.stamp.player)&&
 r81_sample(&p->memory,o->manager,p->layout,&s)==1&&(s.state==2||s.state==4)&&r82_same_sample(&s,&t->event.sample)&&
 r87_graph(&p->memory,o->manager,t->event.stamp.player,p->binding.image,&p->fields,&fresh)&&r87_same_graph(g,&fresh);
}
static int r93_abort(R93Pickup*t,const R93Ops*o){
 const R92Ops*p=&o->commit;R87Graph g;int32_t selected=-1;uint8_t state=0;uint32_t flags;
 if(!t->pending)return R82_BLOCKED;
 if(!r87_graph(&p->memory,o->manager,t->event.stamp.player,p->binding.image,&p->fields,&g)||
 !r87_same_identity(&g.manager,&t->view.graph.manager)||!r93_cancel_still(o,t,&g)||!r92_write_ready(p,o->manager)||
 !r91_hide_info(&p->binding,g.info.object)||!r93_cancel_still(o,t,&g)||
 !r91_hide_guide(&p->binding,g.guide.object)||!r93_cancel_still(o,t,&g)||!r92_write_ready(p,o->manager)||
 !r6_at(&p->memory,o->manager,0x92cf8,&flags,4))goto failed;
 flags&=~3u;
 if(!p->write(p->context,o->manager+0x92cc8,&selected,4)||!p->write(p->context,o->manager+0x92cc0,&state,1)||
 !p->write(p->context,o->manager+0x92cf8,&flags,4))goto failed;
 t->pending=0;t->cancelled++;memset(&t->event,0,sizeof(t->event));return R82_BLOCKED;
failed:t->pending=0;t->fault=1;return R82_BLOCKED;
}
static int r93_candidate(const R93Ops*o,R82Event*e,R83SourcePlan*source,R92View*view){
 const R92Ops*p=&o->commit;R82Stamp after;R81Sample sample;R83SourcePlan again;R92View end;
 if(!o->snapshot(p->context,&e->stamp)||!r82_stamp_valid(&e->stamp)||
 !o->engine_current(p->context,o->manager,e->stamp.player)||!r82_actor(&p->memory,&e->sample)||
 !r83_source_plan(&p->memory,&e->sample,source)||!r92_view(p,e,view)||!r92_write_ready(p,o->manager)||
 !o->snapshot(p->context,&after)||!r82_same_stamp(&e->stamp,&after)||
 r81_sample(&p->memory,o->manager,p->layout,&sample)!=1||!r82_same_sample(&e->sample,&sample)||
 sample.state!=e->sample.state||sample.flags!=e->sample.flags||
 !r83_source_plan(&p->memory,&sample,&again)||!r83_source_equal(source,&again)||
 !r92_view(p,e,&end)||!r92_same_view(view,&end))return 0;
 return 1;
}
static int r93_admit(R93Pickup*t,const R93Ops*o,int enabled,uint64_t caller,uint64_t player,uint64_t linked,int index){
 R82Event e={0};R83SourcePlan source;R92View view;int read,result=R82_BLOCKED;
 if(!t||!r93_ops(o))return R82_BLOCKED;
 if(o->commit.thread(o->commit.context)!=o->commit.owner)return t->pending?R82_BLOCKED:R82_PASS;
 if(t->busy)return R82_BLOCKED;
 if(caller!=o->caller||index!=-1)return R82_PASS;
 if(!enabled)return t->pending?R82_BLOCKED:R82_PASS;
 t->busy=1;read=r81_sample(&o->commit.memory,o->manager,o->commit.layout,&e.sample);
 if(read!=1){result=t->pending?r93_abort(t,o):(read==0?R82_PASS:R82_BLOCKED);goto done;}
 e.material=r82_material_lookup(e.sample.item_id);
 if(!e.material){result=t->pending?r93_abort(t,o):R82_PASS;goto done;}
 if(t->fault)goto done;
 if(!r82_admission_scope(&e.sample,caller,o->caller,o->manager,player,linked,index,1,1,1,1)||
 !r93_candidate(o,&e,&source,&view)||e.stamp.player!=player){if(t->pending)result=r93_abort(t,o);goto done;}
 if(t->pending){
  if(!r82_same_sample(&t->event.sample,&e.sample)||!r82_same_stamp(&t->event.stamp,&e.stamp)||
  !r83_source_equal(&t->source,&source)||!r92_same_view(&t->view,&view)){result=r93_abort(t,o);goto done;}
 }else{
  if(t->receipt.accepted&&(t->receipt.fault||(r82_same_sample(&t->receipt.event.sample,&e.sample)&&r82_same_stamp(&t->receipt.event.stamp,&e.stamp))))goto done;
  memset(&t->receipt,0,sizeof(t->receipt));t->event=e;t->source=source;t->view=view;t->pending=1;
 }
 result=R82_MANAGED;
done:t->busy=0;return result;
}
static int r93_complete(R93Pickup*t,const R93Ops*o,int enabled){
 R82Event e={0};R83SourcePlan source;R92View view;int read,result=R82_BLOCKED,commit;
 if(!t||!r93_ops(o)||o->commit.thread(o->commit.context)!=o->commit.owner||t->busy)return R82_BLOCKED;
 if(!enabled&&!t->pending)return R82_PASS;
 t->busy=1;read=r81_sample(&o->commit.memory,o->manager,o->commit.layout,&e.sample);
 if(read!=1){result=t->pending?r93_abort(t,o):(read==0?R82_PASS:R82_BLOCKED);goto done;}
 e.material=r82_material_lookup(e.sample.item_id);
 if(!e.material){result=t->pending?r93_abort(t,o):R82_PASS;goto done;}
 if(t->fault||!t->pending)goto done;
 if(!enabled||!r82_same_sample(&e.sample,&t->event.sample)||e.material!=t->event.material||e.sample.state!=4||(e.sample.flags&14)||
 !r93_candidate(o,&e,&source,&view)||!r82_same_stamp(&e.stamp,&t->event.stamp)||
 !r83_source_equal(&t->source,&source)||!r92_same_view(&t->view,&view)){result=r93_abort(t,o);goto done;}
 if(!(e.sample.flags&1)){result=R82_WAIT;goto done;}
 /* Exactly one enqueue site, inside R92, never in this adapter. */
 commit=r92_commit(&t->receipt,&o->commit,&e,&source);
 if(commit==R92_OK||commit==R92_DUPLICATE){t->pending=0;if(commit==R92_OK)t->accepted++;result=R82_MANAGED;}
 else if(t->receipt.accepted){t->pending=0;t->fault=1;}
 else result=r93_abort(t,o);
done:t->busy=0;return result;
}
/* Explicit safe lifecycle recovery; captured receipts are never discarded. */
static int r93_rebind_idle(R93Pickup*t,const R93Ops*o){
 R82Stamp stamp,after;R87Fields fields;R87Graph graph,end;uint8_t state;int32_t selected;uint32_t flags;
 if(!t||!r93_ops(o)||t->busy||t->pending||t->receipt.accepted||o->commit.thread(o->commit.context)!=o->commit.owner||
 !o->snapshot(o->commit.context,&stamp)||!r82_stamp_valid(&stamp)||!o->engine_current(o->commit.context,o->manager,stamp.player)||
 !r87_bind(&o->commit.memory,o->manager,stamp.player,o->commit.binding.image,&fields,&graph)||memcmp(&fields,&o->commit.fields,sizeof(fields))||
 !r6_at(&o->commit.memory,o->manager,0x92cc0,&state,1)||state!=0||
 !r6_at(&o->commit.memory,o->manager,0x92cc8,&selected,4)||selected!=-1||
 !r6_at(&o->commit.memory,o->manager,0x92cf8,&flags,4)||(flags&11)||
 !o->snapshot(o->commit.context,&after)||!r82_same_stamp(&stamp,&after)||
 !r87_graph(&o->commit.memory,o->manager,stamp.player,o->commit.binding.image,&fields,&end)||!r87_same_graph(&graph,&end))return 0;
 t->fault=0;memset(&t->event,0,sizeof(t->event));memset(&t->source,0,sizeof(t->source));memset(&t->receipt,0,sizeof(t->receipt));return 1;
}
#endif
