#ifndef LID_MATERIAL_DROP_PART_LAYOUT_R110_H
#define LID_MATERIAL_DROP_PART_LAYOUT_R110_H
#include "material_ui_graph.h"
/* DropPart only. Read-only reflected parameter binder, no engine invocation.
   Parameter offsets/masks are discovered; fixture offsets are NOT game RVAs. */
enum {R110_SITE,R110_FROM_BAG,R110_INDEX,R110_CANCEL,R110_DUR,R110_REST,R110_SPARE,R110_RETURN,R110_PARAMS};
typedef struct R110DropLayout {uint64_t function;uint32_t offset[8],mask[8],size[8],extent;} R110DropLayout;
static const char*const r110_names[8]={"site","inDropDeathBagSlot","inDeathBagSlotIndex","inDropEnableCancel","inDropPartDur","inDropPartRest","inDropPartSpare","ReturnValue"};
static const char*const r110_types[8]={"ByteProperty","BoolProperty","IntProperty","BoolProperty","IntProperty","IntProperty","IntProperty","ObjectProperty"};
static uint32_t r110_size(unsigned i){return i==R110_SITE?1:i==R110_RETURN?8:4;}
static int r110_drop_layout(const R6Ops*o,uint64_t player,R110DropLayout*out){
 R110DropLayout v={0};uint64_t cls,owner,child,next,seen[512];R132FieldWalk fields={0};unsigned n=0,functions=0,found=0;
 if(!out||!r6_at(o,player,0x50,&cls,8)||!r87_owner(o,cls,"BrgCommonPawn_CustomChara",&owner)||!r6_at(o,owner,0x80,&child,8))return 0;
 while(child){
  /* The declaring character class has over1100 reflected declarations.
     This is the class list, distinct from the small parameter list below. */
  uint64_t type;if(!r132_field_visit(&fields,child))return 0;
  if(!r6_at(o,child,0x60,&next,8))return 0;
  if(r6_name(o,child,"DropPart")){if(functions++||!r6_at(o,child,0x50,&type,8)||!r6_name(o,type,"Function"))return 0;v.function=child;}
  child=next;
 }
 if(functions!=1||!r6_at(o,v.function,0x80,&child,8))return 0;n=0;
 while(child){
  if(n==512)return 0;for(unsigned j=0;j<n;j++)if(seen[j]==child)return 0;seen[n++]=child;
  if(!r6_at(o,child,0x60,&next,8))return 0;
  for(unsigned i=0;i<8;i++)if(r6_name(o,child,r110_names[i])){
   uint64_t type;uint32_t dim,size,at,mask=0;
   if((found&(1u<<i))||!r6_at(o,child,0x50,&type,8)||!r6_name(o,type,r110_types[i])||
   !r6_at(o,child,0x68,&dim,4)||dim!=1||!r6_at(o,child,0x6c,&size,4)||size!=r110_size(i)||
   !r6_at(o,child,0x94,&at,4)||at>128-size)return 0;
   if(i==R110_FROM_BAG||i==R110_CANCEL){if(!r6_at(o,child,0xb0,&mask,4)||!mask||(mask&(mask-1)))return 0;}
   v.offset[i]=at;v.size[i]=size;v.mask[i]=mask;found|=1u<<i;if(v.extent<at+size)v.extent=at+size;
  }child=next;
 }
 if(found!=255)return 0;
 for(unsigned i=0;i<8;i++)for(unsigned j=0;j<i;j++){
  if(v.offset[i]<v.offset[j]+v.size[j]&&v.offset[j]<v.offset[i]+v.size[i]&&
  !(v.mask[i]&&v.mask[j]&&v.offset[i]==v.offset[j]&&!(v.mask[i]&v.mask[j])))return 0;
 }
 *out=v;return 1;
}
static int r110_drop_params(const R110DropLayout*l,int slot,uint8_t out[128]){
 if(!l||!out||!l->function||slot<0||slot>=99||!l->extent||l->extent>128)return 0;
 uint8_t v[128]={0};int32_t neg=-1,index=slot;
 for(unsigned i=0;i<8;i++)if(l->size[i]!=r110_size(i)||l->offset[i]>128-l->size[i])return 0;
 for(unsigned k=0;k<2;k++){unsigned i=k?R110_CANCEL:R110_FROM_BAG;uint32_t bits;if(!l->mask[i]||(l->mask[i]&(l->mask[i]-1)))return 0;memcpy(&bits,v+l->offset[i],4);bits|=l->mask[i];memcpy(v+l->offset[i],&bits,4);}
 memcpy(v+l->offset[R110_INDEX],&index,4);
 for(unsigned i=R110_DUR;i<=R110_SPARE;i++)memcpy(v+l->offset[i],&neg,4);
 memcpy(out,v,128);return 1;
}
#endif
