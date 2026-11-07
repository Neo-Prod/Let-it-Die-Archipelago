#ifndef LID_RESEARCH_MATERIAL_COMPLETION_R82_H
#define LID_RESEARCH_MATERIAL_COMPLETION_R82_H
/* Research core: no installer, inventory API, native acquisition action,
   file/network I/O or permanent polling. Game effects are explicit callbacks. */
#include "material_admission_scope_r82.h"
#include "death_wire.h"
enum {R82_PASS=0,R82_MANAGED=1,R82_WAIT=2,R82_BLOCKED=-1};
typedef struct R82Stamp {R15Context context;uint64_t epoch,revision,player;char floor[16],area[32];} R82Stamp;
typedef struct R82Event {R82Stamp stamp;R81Sample sample;unsigned material;} R82Event;
typedef struct R82Ops {
 R6Ops memory;void*context;uint32_t(*thread)(void*);int(*snapshot)(void*,R82Stamp*);
 unsigned(*supported)(void*,const char*);int(*preflight)(void*,const R82Event*);
 int(*enqueue)(void*,const R82Event*);void(*mark_world)(void*,const R82Event*);
 void(*effect)(void*,const R82Event*);void(*erase)(void*,const R82Event*);
 void(*reset)(void*,const R82Event*);void(*cancel)(void*,uint64_t);
 const R81Layout*layout;uint64_t manager,expected_caller;
} R82Ops;
typedef struct R82Transaction {uint32_t owner,busy,pending;R82Event pinned;unsigned accepted,cancelled;} R82Transaction;
static int r82_ops(const R82Ops*o){return o&&o->thread&&o->snapshot&&o->supported&&o->preflight&&o->enqueue&&o->mark_world&&o->effect&&o->erase&&o->reset&&o->cancel&&o->manager&&o->expected_caller&&r81_layout_valid(o->layout);}
static int r82_stamp_valid(const R82Stamp*s){return s&&r15_context_valid(&s->context)&&s->epoch&&s->player&&r15_ascii(s->floor,16)&&r15_ascii(s->area,32)&&r22_map_floor(s->floor,s->area);}
static int r82_same_stamp(const R82Stamp*a,const R82Stamp*b){return r82_stamp_valid(a)&&r82_stamp_valid(b)&&r15_same_context(&a->context,&b->context)&&a->epoch==b->epoch&&a->revision==b->revision&&a->player==b->player&&r15_equal(a->floor,b->floor,16)&&r15_equal(a->area,b->area,32);}
static int r82_same_sample(const R81Sample*a,const R81Sample*b){
 return a&&b&&a->manager==b->manager&&a->actor==b->actor&&a->actor_class==b->actor_class&&a->actor_name==b->actor_name&&a->actor_number==b->actor_number&&a->slot==b->slot&&a->unique==b->unique&&a->gettime==b->gettime&&r15_equal(a->item_id,b->item_id,64)&&r15_equal(a->entity_id,b->entity_id,96);
}
static int r82_actor(const R6Ops*o,const R81Sample*s){
 uint64_t cls=s->actor_class,next,seen[32];unsigned n=0,treasure=0,actor=0;uint32_t flags;
 if(!s->actor||!cls||!r6_at(o,s->actor,0x290,&flags,4)||(flags&0x200))return 0;
 while(cls){
  if(n==32)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
  if(r6_name(o,cls,"BrgActor_Treasure")){if(treasure)return 0;treasure=n;}
  if(r6_name(o,cls,"Actor")){if(actor)return 0;actor=n;}
  if(!r6_at(o,cls,0x78,&next,8))return 0;cls=next;
 }
 return treasure&&actor&&treasure<actor;
}
/* Cancel callback must recheck live manager/name/owner/state, close the pickup
   UI and synchronously reset selection/state. Merely setting bit2 can be lost
   at TickProcess's tail. It never removes or edits a drop record. */
static int r82_abort(R82Transaction*t,const R82Ops*o,const R81Sample*current){
 (void)current;t->pending=0;t->cancelled++;o->cancel(o->context,o->manager);return R82_BLOCKED;
}
static int r82_admit(R82Transaction*t,const R82Ops*o,int enabled,uint64_t caller,uint64_t receiver,uint64_t linked_player,int index){
 R82Event e={0};R82Stamp after={0};int result=R82_BLOCKED,read;
 if(!t||!r82_ops(o)||!t->owner)return R82_BLOCKED;
 if(o->thread(o->context)!=t->owner)return R82_PASS;
 if(t->busy)return R82_BLOCKED;
 if(!enabled||caller!=o->expected_caller||index!=-1)return R82_PASS;
 t->busy=1;
 read=r81_sample(&o->memory,o->manager,o->layout,&e.sample);
 if(read!=1){result=t->pending||read<0?r82_abort(t,o,0):R82_PASS;goto done;}
 e.material=o->supported(o->context,e.sample.item_id);
 if(!e.material||e.material>46){result=t->pending?r82_abort(t,o,&e.sample):R82_PASS;goto done;}
 if(!o->snapshot(o->context,&e.stamp)||!r82_stamp_valid(&e.stamp)||e.stamp.player!=receiver||
    !r82_admission_scope(&e.sample,caller,o->expected_caller,o->manager,receiver,linked_player,index,1,1,1,1)||
    !r82_actor(&o->memory,&e.sample)||!o->preflight(o->context,&e)||
    !o->snapshot(o->context,&after)||!r82_same_stamp(&e.stamp,&after)){
  if(t->pending)result=r82_abort(t,o,&e.sample);else{o->cancel(o->context,e.sample.manager);t->cancelled++;}goto done;
 }
 if(t->pending){if(!r82_same_sample(&t->pinned.sample,&e.sample)||!r82_same_stamp(&t->pinned.stamp,&e.stamp)){result=r82_abort(t,o,&e.sample);goto done;}}
 else{t->pinned=e;t->pending=1;}
 result=R82_MANAGED;
done:t->busy=0;return result;
}
/* Caller forwards original ONLY for PASS. Never forward after virtual admission.
   queue acceptance is in RAM, not a synchronous durable server acknowledgment. */
static int r82_complete(R82Transaction*t,const R82Ops*o,int enabled){
 R82Event e={0};R82Stamp after={0};int result=R82_BLOCKED,read;
 if(!t||!r82_ops(o)||!t->owner)return R82_BLOCKED;
 if(o->thread(o->context)!=t->owner)return R82_BLOCKED;
 if(t->busy)return R82_BLOCKED;
 if(!enabled&&!t->pending)return R82_PASS;
 t->busy=1;read=r81_sample(&o->memory,o->manager,o->layout,&e.sample);
 if(read!=1){result=t->pending||read<0?r82_abort(t,o,0):R82_PASS;goto done;}
 e.material=o->supported(o->context,e.sample.item_id);
 if(!e.material||e.material>46){result=t->pending?r82_abort(t,o,&e.sample):R82_PASS;goto done;}
 if(!t->pending){result=r82_abort(t,o,&e.sample);goto done;}
 if(!enabled||!r82_same_sample(&t->pinned.sample,&e.sample)||e.material!=t->pinned.material||
    !o->snapshot(o->context,&e.stamp)||!r82_same_stamp(&t->pinned.stamp,&e.stamp)||
    !r82_actor(&o->memory,&e.sample)||e.sample.state!=4||(e.sample.flags&14)||
    !o->preflight(o->context,&e)||!o->snapshot(o->context,&after)||!r82_same_stamp(&e.stamp,&after)){
  result=r82_abort(t,o,&e.sample);goto done;
 }
 if(!(e.sample.flags&1)){result=R82_WAIT;goto done;}
 if(o->enqueue(o->context,&e)!=1){result=r82_abort(t,o,&e.sample);goto done;}
 t->pending=0;t->accepted++;
 o->mark_world(o->context,&e);o->effect(o->context,&e);o->erase(o->context,&e);o->reset(o->context,&e);
 result=R82_MANAGED;
done:t->busy=0;return result;
}
#endif
