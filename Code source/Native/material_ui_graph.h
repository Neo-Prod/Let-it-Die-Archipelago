#ifndef LID_RESEARCH_MATERIAL_UI_GRAPH_R87_H
#define LID_RESEARCH_MATERIAL_UI_GRAPH_R87_H
#include "material_record_reader.h"
#include "material_field_walk.h"
#include <string.h>
/* Research readonly binder. Caller proves module hash/base and transaction
   owner/session. This helper never calls native code or edits game memory. */
typedef struct R87Identity {uint64_t object,vtable,cls;uint32_t name,number;} R87Identity;
typedef struct R87Graph {R87Identity manager,ui,info,guide,player,panels[3];} R87Graph;
typedef struct R87Fields {uint32_t manager_ui,manager_guide,ui_info,ui_player,info_panels;} R87Fields;
static int r87_owner(const R6Ops*o,uint64_t cls,const char*name,uint64_t*out){
 uint64_t seen[32],next,owner=0;unsigned count=0;
 while(cls){if(count==32)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==cls)return 0;seen[count++]=cls;
  if(r6_name(o,cls,name)){if(owner)return 0;owner=cls;}if(!r6_at(o,cls,0x78,&next,8))return 0;cls=next;}
 if(!owner)return 0;*out=owner;return 1;
}
static int r87_identity(const R6Ops*o,uint64_t obj,const char*name,uint64_t vt,R87Identity*out){
 R87Identity v={0};uint64_t owner;
 if(!out||!r6_at(o,obj,0,&v.vtable,8)||!v.vtable||(vt&&vt!=v.vtable)||!r6_at(o,obj,0x50,&v.cls,8)||
 !r87_owner(o,v.cls,name,&owner)||!r6_at(o,obj,0x48,&v.name,4)||!r6_at(o,obj,0x4c,&v.number,4))return 0;
 v.object=obj;*out=v;return 1;
}
static int r87_field(const R6Ops*o,uint64_t cls,const char*owner_name,const char*field,
 uint32_t off,uint32_t dim,uint32_t extent,uint32_t*out){
 /* Include all reflected siblings (functions included), not just variables. */
 uint64_t owner,child,next;R132FieldWalk walk={0};unsigned found=0;
 if(!out||!dim||off<0x60||(uint64_t)off+8ull*dim>extent||!r87_owner(o,cls,owner_name,&owner)||!r6_at(o,owner,0x80,&child,8))return 0;
 while(child){uint64_t type;uint32_t d,size,at;
  if(!r132_field_visit(&walk,child))return 0;
  if(!r6_at(o,child,0x60,&next,8))return 0;
  if(r6_name(o,child,field)){
   if(found++||!r6_at(o,child,0x50,&type,8)||!r6_name(o,type,"ObjectProperty")||
   !r6_at(o,child,0x68,&d,4)||d!=dim||!r6_at(o,child,0x6c,&size,4)||size!=8||
   !r6_at(o,child,0x94,&at,4)||at!=off)return 0;
  }child=next;
 }if(found!=1)return 0;*out=off;return 1;
}
static int r87_graph(const R6Ops*o,uint64_t manager,uint64_t player,uint64_t base,const R87Fields*f,R87Graph*out){
 R87Graph g={0};uint64_t ui,info,guide,pawn,panel;
 if(!f||!out||!base||base>UINT64_MAX-0x2200000||f->manager_ui!=0x68||f->manager_guide!=0x92cfc||f->ui_info!=0x296c||f->ui_player!=0x2d4c||f->info_panels!=0xb8||
 !r87_identity(o,manager,"BrgDropItemManager",base+0x20f8d60,&g.manager)||
 !r6_at(o,manager,f->manager_ui,&ui,8)||!r87_identity(o,ui,"BrgUIManager",base+0x21d81b0,&g.ui)||
 !r6_at(o,manager,f->manager_guide,&guide,8)||!r87_identity(o,guide,"BrgUIHUD_ButtonGuide",base+0x21cc290,&g.guide)||
 !r6_at(o,ui,f->ui_info,&info,8)||!r87_identity(o,info,"BrgUIHUD_ItemInfoManager",base+0x21d0d20,&g.info)||
 !r6_at(o,ui,f->ui_player,&pawn,8)||pawn!=player||!r87_identity(o,pawn,"BrgCommonPawn_CustomCharaNative",0,&g.player))return 0;
 for(unsigned i=0;i<3;i++)if(!r6_at(o,info,f->info_panels+8*i,&panel,8)||!r87_identity(o,panel,"BrgUIMenuPart_ItemInfoPanel",0,&g.panels[i]))return 0;
 *out=g;return 1;
}
static int r87_bind(const R6Ops*o,uint64_t manager,uint64_t player,uint64_t base,R87Fields*out,R87Graph*graph){
 R87Fields f={0};R87Graph g;uint64_t mc,ui,uc,info,ic;
 if(!out||!graph||!r6_at(o,manager,0x50,&mc,8)||
 !r87_field(o,mc,"BrgUIBase","mUIManager",0x68,1,R81_EXTENT,&f.manager_ui)||
 !r87_field(o,mc,"BrgDropItemManager","mButtonGuide",0x92cfc,1,R81_EXTENT,&f.manager_guide)||
 !r6_at(o,manager,f.manager_ui,&ui,8)||!r6_at(o,ui,0x50,&uc,8)||
 !r87_field(o,uc,"BrgUIManager","mItemInfoManager",0x296c,1,0x30c0,&f.ui_info)||
 !r87_field(o,uc,"BrgUIManager","mPlayerCommonPawnNative",0x2d4c,1,0x30c0,&f.ui_player)||
 !r6_at(o,ui,f.ui_info,&info,8)||!r6_at(o,info,0x50,&ic,8)||
 !r87_field(o,ic,"BrgUIHUD_ItemInfoManager","mInfoPanel",0xb8,3,0x1c8,&f.info_panels)||
 !r87_graph(o,manager,player,base,&f,&g))return 0;
 *out=f;*graph=g;return 1;
}
static int r87_same_identity(const R87Identity*a,const R87Identity*b){return a->object==b->object&&a->vtable==b->vtable&&a->cls==b->cls&&a->name==b->name&&a->number==b->number;}
static int r87_same_graph(const R87Graph*a,const R87Graph*b){
 if(!a||!b||!r87_same_identity(&a->manager,&b->manager)||!r87_same_identity(&a->ui,&b->ui)||!r87_same_identity(&a->info,&b->info)||!r87_same_identity(&a->guide,&b->guide)||!r87_same_identity(&a->player,&b->player))return 0;
 for(unsigned i=0;i<3;i++)if(!r87_same_identity(&a->panels[i],&b->panels[i]))return 0;return 1;
}
#endif
