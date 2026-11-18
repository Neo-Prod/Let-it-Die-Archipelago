#ifndef LID_RESEARCH_MOMOKA_CONTEXT_R196_H
#define LID_RESEARCH_MOMOKA_CONTEXT_R196_H
#include "momoka_metadata_reader.h"
/* UNCOMPILED/UNTESTED. Snapshot lifetime/build/origin must be proven by host.
   Only produces observations; never changes native results or AP state. */
typedef struct M196Binding {
 uint64_t menu,menu_class,owner;
 M195Signature buy,sell,exchange,add;
} M196Binding;
static int m196_bind(const M195Ops*o,uint64_t menu,M196Binding*out){
 M196Binding b={0};uint64_t cls,next,seen[64],child,fields[4096];unsigned n=0,count=0,found=0;
 const char*names[4]={"BuySticker","SellSticker","ExchangeSticker","AddHaveSkillList"};
 M195Signature*signatures[4]={&b.buy,&b.sell,&b.exchange,&b.add};
 if(!out||!m195_read(o,menu,0x50,&cls,8))return 0;b.menu_class=cls;b.menu=menu;
 while(cls){
  if(n==64)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
  if(m195_named(o,cls,"BrgUIMenu_SkillExchange")){if(b.owner)return 0;b.owner=cls;}
  if(!m195_read(o,cls,0x78,&next,8))return 0;cls=next;
 }
 if(!b.owner||!m195_read(o,b.owner,0x80,&child,8))return 0;
 while(child){
  if(count==4096)return 0;for(unsigned i=0;i<count;i++)if(fields[i]==child)return 0;
  fields[count++]=child;
  if(!m195_read(o,child,0x60,&next,8))return 0;
  for(unsigned i=0;i<4;i++)if(m195_named(o,child,names[i])){
   if((found&(1u<<i))||!m195_signature(o,child,b.owner,names[i],i==3,signatures[i]))return 0;
   found|=1u<<i;
  }
  child=next;
 }
 if(found!=15)return 0;*out=b;return 1;
}
enum M196Kind {M196_UNKNOWN,M196_PRICE,M196_BUY_ADD,M196_SELL_ADD,M196_EXCHANGE_ADD};
typedef struct M196Context {uint64_t frame,parent,node,parent_node,locals,parent_locals;int32_t num;enum M196Kind kind;} M196Context;
static int m196_context(const M195Ops*o,const M196Binding*b,uint64_t frame,M196Context*out){
 M196Context c={0};uint64_t object,parent_object,cls;uint32_t offset;
 if(!b||!out||!frame||!m195_read(o,b->menu,0x50,&cls,8)||cls!=b->menu_class||
 !m195_read(o,frame,0x14,&c.node,8)||!m195_read(o,frame,0x1c,&object,8)||object!=b->menu)return 0;
 c.frame=frame;
 if(c.node==b->buy.function){
  if(!m195_read(o,frame,0x2c,&c.locals,8)||!c.locals)return 0;
  c.kind=M196_PRICE;*out=c;return 1;
 }
 if(c.node!=b->add.function||!m195_read(o,frame,0x34,&c.parent,8)||!c.parent||c.parent==frame||
 !m195_read(o,c.parent,0x14,&c.parent_node,8)||!m195_read(o,c.parent,0x1c,&parent_object,8)||parent_object!=b->menu||
 !m195_read(o,frame,0x2c,&c.locals,8)||!c.locals||!m195_read(o,c.parent,0x2c,&c.parent_locals,8)||!c.parent_locals)return 0;
 offset=b->add.num_offset;
 if(!b->add.has_num||offset>b->add.parms_size||b->add.parms_size-offset<4||!m195_read(o,c.locals,offset,&c.num,4))return 0;
 if(c.parent_node==b->buy.function)c.kind=M196_BUY_ADD;
 else if(c.parent_node==b->sell.function)c.kind=M196_SELL_ADD;
 else if(c.parent_node==b->exchange.function)c.kind=M196_EXCHANGE_ADD;
 else return 0;
 *out=c;return 1;
}
#endif
