#ifndef LID_MATERIAL_OWNER_SNAPSHOT_R104_H
#define LID_MATERIAL_OWNER_SNAPSHOT_R104_H
#include "material_owner_resolver.h"
#include "checks_chest_observer.h"
/* Read-only snapshot; no hook install, capability arming, I/O or inventory.
   current_game and stamp must be actual existing live host resolvers. */
typedef struct R104Ops {
 R6Ops memory;void*context;uint32_t owner;uint64_t image;
 uint32_t(*thread)(void*);int(*stamp)(void*,R74Stamp*);
 uint64_t(*current_game)(void*);int(*scope_begin)(void*);void(*scope_end)(void*,int);
} R104Ops;
typedef struct R104Cache {unsigned busy,valid;R103Fields fields;unsigned diagnostic;} R104Cache;
typedef struct R104Snapshot {R74Stamp stamp;R103View view;R103Fields fields;} R104Snapshot;
static int r104_stamp_valid(const R74Stamp*s){return s&&r15_context_valid(&s->context)&&s->epoch&&r15_ascii(s->floor,16)&&r15_ascii(s->area,32)&&r22_map_floor(s->floor,s->area);}
static int r104_snapshot(R104Cache*cache,const R104Ops*o,uint64_t expected_player,uint64_t expected_manager,R104Snapshot*out){
 R104Snapshot s={0};R103View after;R74Stamp stamp;uint64_t game;int scope=0,entered=0,ok=0;
 if(!cache||!out||!o||!o->owner||!o->thread||!o->stamp||!o->current_game||!o->scope_begin||!o->scope_end||
 o->thread(o->context)!=o->owner||cache->busy)return 0;
 cache->busy=1;cache->diagnostic=1;
 if(!o->stamp(o->context,&s.stamp)||!r104_stamp_valid(&s.stamp)||(game=o->current_game(o->context))==0)goto done;
 cache->diagnostic=2;s.fields=cache->fields;
 if(!cache->valid||!r103_view(&o->memory,game,expected_player,o->image,&s.fields,&s.view)){
  scope=o->scope_begin(o->context);entered=1;
  int bound=r103_bind(&o->memory,game,expected_player,o->image,&s.fields,&s.view);
  o->scope_end(o->context,scope);entered=0;
  if(!bound)goto done;
 }
 cache->diagnostic=3;if((expected_manager&&expected_manager!=s.view.graph.manager.object)||o->thread(o->context)!=o->owner||
 o->current_game(o->context)!=game||!o->stamp(o->context,&stamp)||!r104_stamp_valid(&stamp)||!r74_same_stamp(&s.stamp,&stamp)||
 !r103_view(&o->memory,game,expected_player,o->image,&s.fields,&after)||!r103_same_view(&s.view,&after)||
 !o->stamp(o->context,&stamp)||!r104_stamp_valid(&stamp)||!r74_same_stamp(&s.stamp,&stamp)||
 o->current_game(o->context)!=game||o->thread(o->context)!=o->owner)goto done;
 cache->fields=s.fields;cache->valid=1;*out=s;ok=1;cache->diagnostic=0;
done:if(entered)o->scope_end(o->context,scope);cache->busy=0;return ok;
}
#endif
