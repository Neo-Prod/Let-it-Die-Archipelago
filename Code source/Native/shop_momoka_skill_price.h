#ifndef LID_SHOP_MOMOKA_SKILL_PRICE_H
#define LID_SHOP_MOMOKA_SKILL_PRICE_H
#include "momoka_skill_snapshot.h"
#include "shop_momoka_payment.h"
/* Read-only reflected layout, prepared once inside a valid metadata window.
   No guessed BrgDbSkill/local offsets, allocations or per-frame class scans.
   Caller validates current GetSkill binding and keeps its VM frame alive until
   this post-original copy completes. A copied frame address is not a lease. */
typedef struct LidMPSkillLayout {
    uint64_t function,structure;uint32_t local_offset,field[4];unsigned ready;
} LidMPSkillLayout;
static int lid_mp_skill_layout(const M195Ops*o,const M195Signature*caller,
                               uint64_t structure,LidMPSkillLayout*out){
    uint64_t p,next,cls,linked,seen[64],flags;unsigned n=0,found=0,local_found=0;
    uint32_t dim,size,at;LidMPSkillLayout l={0};
    static const char*names[4]={"Mid","mBuy_money","mBuy_spirit","mBuy_recycle_point"};
    if(!out||out->ready||!caller||!caller->function||caller->extent<196||
       !m195_named(o,structure,"BrgDbSkill")||!m195_read(o,structure,0x50,&cls,8)||
       !m195_named(o,cls,"ScriptStruct")||!m195_read(o,caller->function,0x80,&p,8))return 0;
    while(p){
        if(n==64)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==p)return 0;seen[n++]=p;
        if(!m195_read(o,p,0x60,&next,8))return 0;
        if(m195_named(o,p,"skillData")){
            if(local_found||!m195_read(o,p,0x50,&cls,8)||!m195_named(o,cls,"StructProperty")||
               !m195_read(o,p,0x70,&flags,8)||(flags&0x80)||
               !m195_read(o,p,0x68,&dim,4)||dim!=1||!m195_read(o,p,0x6c,&size,4)||size!=196||
               !m195_read(o,p,0x94,&at,4)||at<caller->parms_size||at>caller->extent-196||
               !m195_read(o,p,0xb0,&linked,8)||linked!=structure)return 0;
            l.local_offset=at;local_found=1;
        }
        p=next;
    }
    if(!local_found||!m195_read(o,structure,0x80,&p,8))return 0;n=0;
    while(p){
        unsigned index=4;
        if(n==64)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==p)return 0;seen[n++]=p;
        if(!m195_read(o,p,0x60,&next,8))return 0;
        for(unsigned i=0;i<4;i++)if(m195_named(o,p,names[i])){index=i;break;}
        if(index<4){
            uint32_t expected=index?4:16;
            if((found&(1u<<index))||!m195_read(o,p,0x50,&cls,8)||
               !m195_named(o,cls,index?"IntProperty":"StrProperty")||
               !m195_read(o,p,0x68,&dim,4)||dim!=1||!m195_read(o,p,0x6c,&size,4)||size!=expected||
               !m195_read(o,p,0x94,&at,4)||at>196-expected)return 0;
            for(unsigned i=0;i<4;i++)if(found&(1u<<i)){
                unsigned other_size=i?4:16;
                if(at<l.field[i]+other_size&&l.field[i]<at+expected)return 0;
            }
            l.field[index]=at;found|=1u<<index;
        }
        p=next;
    }
    if(found!=15)return 0;
    l.function=caller->function;l.structure=structure;l.ready=1;*out=l;return 1;
}
/* native_result is the unchanged real getter result, observed AFTER original.
   Only buy price/add contexts are eligible, never sale/exchange or other use.
   Output row is published only on complete ID/currency agreement. */
static int lid_mp_skill_price(const M195Ops*o,const LidMPSkillLayout*l,
  const M197SkillSnapshot*s,unsigned native_result,unsigned offer,LidMPRow*out){
    unsigned char data[196];M197StringHeader mid;uint16_t text[129];int32_t money,spirit,recycle;
    LidMPRow row={0};
    if(!o||!l||!l->ready||!s||!out||native_result!=1||offer>=4096||
       (s->context.kind!=M196_PRICE&&s->context.kind!=M196_BUY_ADD)||
       (s->context.kind==M196_BUY_ADD&&s->context.num!=1)||
       s->context.node!=l->function||!lid_mp_id(s->skill,s->length)||
       !m195_read(o,s->context.locals,l->local_offset,data,196))return 0;
    __builtin_memcpy(&mid,data+l->field[0],16);
    __builtin_memcpy(&money,data+l->field[1],4);__builtin_memcpy(&spirit,data+l->field[2],4);
    __builtin_memcpy(&recycle,data+l->field[3],4);
    if(money<1||spirit||recycle||!mid.data||mid.count<2||mid.count>129||
       mid.capacity<mid.count||mid.capacity>1048576||
       !m195_read(o,mid.data,0,text,(size_t)mid.count*2)||
       !lid_mp_id(text,(unsigned)mid.count-1)||
       !lid_mp_equal(text,(unsigned)mid.count-1,s->skill,s->length))return 0;
    for(unsigned i=0;i<=s->length;i++)row.skill[i]=s->skill[i];
    row.units=s->length;row.offer=offer;row.price=(unsigned)money;*out=row;return 1;
}
#endif
