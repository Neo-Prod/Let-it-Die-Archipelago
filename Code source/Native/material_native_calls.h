#ifndef LID_MATERIAL_NATIVE_CALLS_R91_H
#define LID_MATERIAL_NATIVE_CALLS_R91_H
#include "material_binding_bytes.h"
#include "material_source_plan.h"
/* Low-level Win64 invocations. The owner adapter must prove current object
   graph/record/session and preflight before calling. No hook or install here. */
typedef struct R91String {uint16_t*data;int32_t num,max;} R91String;
typedef struct R91StringBuffer {R91String value;uint16_t data[96];} R91StringBuffer;
static int r91_string(const char*s,R91StringBuffer*out){
 if(!s||!out)return 0;size_t n=0;while(n<96&&s[n]){if((unsigned char)s[n]<32||(unsigned char)s[n]>126)return 0;n++;}
 if(n>=96)return 0;
 memset(out,0,sizeof(*out));for(size_t i=0;i<n;i++)out->data[i]=(unsigned char)s[i];
 out->value.data=out->data;out->value.num=out->value.max=(int32_t)n+1;return 1;
}
static int r91_mark(const R91Binding*b,uint64_t stage,const R83SourcePlan*p){
 R91StringBuffer a,c;
 if(!b||!b->image||!p)return -1;if(p->kind==R83_NONE)return 1;
 if(!stage)return -1;
 if(p->kind==R83_ENTITY){
  if(!p->entity[0]||p->entity[0]=='-'||!b->entry[R91_MARK_ENTITY]||!r91_string(p->entity,&a))return -1;
  return ((int32_t(*)(void*,const R91String*))(uintptr_t)b->entry[R91_MARK_ENTITY])((void*)(uintptr_t)stage,&a.value);
 }
 if(p->kind==R83_POINT){
  if(!b->entry[R91_MARK_POINT]||!r91_string(p->unit,&a)||!r91_string(p->point,&c))return -1;
  return ((int32_t(*)(void*,const R91String*,const R91String*))(uintptr_t)b->entry[R91_MARK_POINT])((void*)(uintptr_t)stage,&a.value,&c.value);
 }return -1;
}
static int r91_drop_call(const R91Binding*b,unsigned which,uint64_t manager,int32_t slot,int32_t unique){
 if(!b||!b->image||!manager||slot<0||slot>=496||unique<0||(which!=R91_EFFECT&&which!=R91_ERASE)||!b->entry[which])return 0;
 ((void(*)(void*,int32_t,int32_t))(uintptr_t)b->entry[which])((void*)(uintptr_t)manager,slot,unique);return 1;
}
static int r91_hide_info(const R91Binding*b,uint64_t info){
 if(!b||!b->image||!info||!b->entry[R91_HIDE_INFO])return 0;
 ((void(*)(void*,uint32_t))(uintptr_t)b->entry[R91_HIDE_INFO])((void*)(uintptr_t)info,0);return 1;
}
static int r91_hide_guide(const R91Binding*b,uint64_t guide){
 if(!b||!b->image||!guide||!b->entry[R91_HIDE_GUIDE])return 0;
 ((void(*)(void*,uint8_t,uint32_t))(uintptr_t)b->entry[R91_HIDE_GUIDE])((void*)(uintptr_t)guide,0x25,0);return 1;
}
#endif
