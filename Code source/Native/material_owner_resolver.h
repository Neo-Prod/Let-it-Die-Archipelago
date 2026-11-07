#ifndef LID_MATERIAL_OWNER_RESOLVER_R103_H
#define LID_MATERIAL_OWNER_RESOLVER_R103_H
#include "material_ui_graph.h"
/* Read-only owner-event helper. No object-list scan, native call, hooks or I/O.
   Caller proves owner, current GameInfo, build and AP context independently.
   New offsets are discovered from live metadata, never compiled guesses. */
typedef struct R103Fields {
 uint64_t game_class,ui_class,manager_class,info_class;
 uint32_t game_ui,ui_drop;R87Fields graph;
} R103Fields;
typedef struct R103View {R87Identity game;R87Graph graph;} R103View;
static int r103_object_field(const R6Ops*o,uint64_t cls,const char*owner_name,const char*name,uint32_t extent,uint32_t*out){
 /* UStruct.Children contains functions as well as properties. The supplied
    GameInfo/UI classes exceed512 declared members; match the established
    bounded4096 field reader, retaining full-list duplicate/cycle checks. */
 uint64_t owner,child,next;R132FieldWalk walk={0};unsigned found=0;uint32_t offset=0;
 if(!out||extent<0x68||!r87_owner(o,cls,owner_name,&owner)||!r6_at(o,owner,0x80,&child,8))return 0;
 while(child){
  if(!r132_field_visit(&walk,child))return 0;
  if(!r6_at(o,child,0x60,&next,8))return 0;
  if(r6_name(o,child,name)){
   uint64_t type;uint32_t dim,size,at;
   if(found++||!r6_at(o,child,0x50,&type,8)||!r6_name(o,type,"ObjectProperty")||
   !r6_at(o,child,0x68,&dim,4)||dim!=1||!r6_at(o,child,0x6c,&size,4)||size!=8||
   !r6_at(o,child,0x94,&at,4)||at<0x60||at>extent-8)return 0;
   offset=at;
  }child=next;
 }
 if(found!=1)return 0;*out=offset;return 1;
}
static int r103_view(const R6Ops*o,uint64_t game,uint64_t expected_player,uint64_t base,const R103Fields*f,R103View*out){
 R103View v={0};R87Identity ui;uint64_t manager,player,info;
 if(!out||!f||!base||base>UINT64_MAX-0x2200000||f->game_ui<0x60||f->game_ui>0x1430-8||f->ui_drop<0x60||f->ui_drop>0x30c0-8||
 !r87_identity(o,game,"BrgGameInfoNative",0,&v.game)||v.game.cls!=f->game_class||
 !r6_at(o,game,f->game_ui,&ui.object,8)||!r87_identity(o,ui.object,"BrgUIManager",base+0x21d81b0,&ui)||ui.cls!=f->ui_class||
 !r6_at(o,ui.object,f->ui_drop,&manager,8)||!r6_at(o,ui.object,f->graph.ui_player,&player,8)||
 (expected_player&&player!=expected_player)||!r87_graph(o,manager,player,base,&f->graph,&v.graph)||
 !r87_same_identity(&ui,&v.graph.ui)||v.graph.manager.cls!=f->manager_class||
 !r6_at(o,ui.object,f->graph.ui_info,&info,8)||info!=v.graph.info.object||v.graph.info.cls!=f->info_class)return 0;
 *out=v;return 1;
}
static int r103_same_view(const R103View*a,const R103View*b){return r87_same_identity(&a->game,&b->game)&&r87_same_graph(&a->graph,&b->graph);}
static int r103_bind(const R6Ops*o,uint64_t game,uint64_t expected_player,uint64_t base,R103Fields*out,R103View*view){
 R103Fields f={0};R103View v,end;R87Identity g,ui;R87Graph graph;uint64_t manager,player;
 if(!out||!view||!base||base>UINT64_MAX-0x2200000||!r87_identity(o,game,"BrgGameInfoNative",0,&g)||
 !r103_object_field(o,g.cls,"BrgGameInfoNative","mUIManager",0x1430,&f.game_ui)||
 !r6_at(o,game,f.game_ui,&ui.object,8)||!r87_identity(o,ui.object,"BrgUIManager",base+0x21d81b0,&ui)||
 !r103_object_field(o,ui.cls,"BrgUIManager","mDropItemManager",0x30c0,&f.ui_drop)||
 !r6_at(o,ui.object,f.ui_drop,&manager,8)||!r6_at(o,ui.object,0x2d4c,&player,8)||(expected_player&&player!=expected_player)||
 !r87_bind(o,manager,player,base,&f.graph,&graph))return 0;
 f.game_class=g.cls;f.ui_class=ui.cls;f.manager_class=graph.manager.cls;f.info_class=graph.info.cls;
 if(!r103_view(o,game,expected_player,base,&f,&v)||!r103_view(o,game,expected_player,base,&f,&end)||!r103_same_view(&v,&end)||
 !r87_same_identity(&g,&v.game)||!r87_same_graph(&graph,&v.graph))return 0;
 *out=f;*view=v;return 1;
}
#endif
