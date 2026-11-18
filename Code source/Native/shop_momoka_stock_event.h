#ifndef LID_SHOP_MOMOKA_STOCK_EVENT_H
#define LID_SHOP_MOMOKA_STOCK_EVENT_H
#include "shop_momoka_stock.h"
#include "shop_momoka_label_event.h"
typedef struct LidMomokaStockEvent {
    LidMomokaStock stock;LidMomokaLabelEvent*host;M207Slot ownership;
    uint64_t function,slot,select,resume;uintptr_t original;
    unsigned prepared,active,busy,stopped;int last_result;
    void*visual_context;void(*visual_after)(void*);
} LidMomokaStockEvent;
static int lid_mse_script(const M195Ops*o,uint64_t list,const char*name,uint64_t*out){
    uint64_t f,cls;uint32_t flags;
    if(!out||!m212_member(o,list,"BrgUIMenu_SkillExchange_SkillList",name,&f)||
       !m195_read(o,f,0x50,&cls,8)||!m195_named(o,cls,"Function")||
       !m195_read(o,f,0xd0,&flags,4)||(flags&0x400))return 0;
    *out=f;return 1;
}
static int lid_mse_native(const M195Ops*o,uint64_t list,uint64_t image,uint64_t*out,uint64_t*slot){
    static const uint8_t prefix[24]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x42,0x24,0x0f,0x57,0xc0,0x48,0x8b};
    uint8_t actual[24];uint64_t f,cls,flags,p,next,original,root,seen[128];
    uint32_t function_flags,extent,at[2]={0,0};uint16_t index,parms;unsigned count=0,found=0;
    if(!out||!slot||!image||!m195_read(o,image,0xfeb430,actual,24))return 0;
    for(unsigned i=0;i<24;i++)if(actual[i]!=prefix[i])return 0;
    if(!m212_member(o,list,"BrgUIMenu_Base","StartWait",&f)||
       !m195_read(o,f,0x50,&cls,8)||!m195_named(o,cls,"Function")||
       !m195_read(o,f,0x10,&root,8)||!(root&0x4000)||
       !m195_read(o,f,0xd0,&function_flags,4)||!(function_flags&0x400)||
       !m195_read(o,f,0xd4,&index,2)||index||
       !m195_read(o,f,0x88,&extent,4)||extent>65536||
       !m195_read(o,f,0xe2,&parms,2)||parms<8||parms>extent||
       !m195_read(o,f,0xf0,&original,8)||original!=image+0xfeb430||
       f>UINT64_MAX-0xf0||((f+0xf0)&7)||!m195_read(o,f,0x80,&p,8))return 0;
    while(p){
        if(count==128)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==p)return 0;seen[count++]=p;
        if(!m195_read(o,p,0x60,&next,8)||!m195_read(o,p,0x70,&flags,8))return 0;
        if(flags&0x80){
            unsigned i=m195_named(o,p,"WaitTime")?0:m195_named(o,p,"WaitNextState")?1:2;
            if(i==2||(found&(1u<<i))||(flags&0x500)||
               !lid_ml_property(o,p,i?"IntProperty":"FloatProperty",1,4,parms,0,&at[i]))return 0;
            found|=1u<<i;
        }p=next;
    }
    if(found!=3||(at[0]<at[1]+4&&at[1]<at[0]+4))return 0;
    *out=f;*slot=f+0xf0;return 1;
}
static int lid_mse_prepare(LidMomokaStockEvent*h,LidMomokaLabelEvent*host,LidShopPurchaseBridge*bridge,uint64_t image){
    uint64_t list;
    if(!h||h->prepared||!host||!host->prepared||!host->live(host->context)||
       host->thread(host->context)!=host->owner||!lid_ms_current(&host->labels,&list)||
       !lid_ms_prepare(&h->stock,&host->labels,bridge)||
       !lid_mse_script(&host->labels.root->memory,list,"SelectSkillList",&h->select)||
       !lid_mse_script(&host->labels.root->memory,list,"ResumeSelectSkillList",&h->resume)||h->select==h->resume||
       !lid_mse_native(&host->labels.root->memory,list,image,&h->function,&h->slot))return 0;
    h->host=host;h->original=(uintptr_t)(image+0xfeb430);h->prepared=1;return 1;
}
static int lid_mse_scope(LidMomokaStockEvent*h,void*receiver,void*frame){
    const M195Ops*o=&h->host->labels.root->memory;uint64_t node,object,list;
    return m195_read(o,(uint64_t)(uintptr_t)frame,0x14,&node,8)&&(node==h->select||node==h->resume)&&
        m195_read(o,(uint64_t)(uintptr_t)frame,0x1c,&object,8)&&object==(uint64_t)(uintptr_t)receiver&&
        lid_ms_current(&h->host->labels,&list)&&list==object;
}
static void lid_mse_call(LidMomokaStockEvent*h,void*receiver,void*frame,void*result){
    ((M204Exec)h->original)(receiver,frame,result); /* original wait/state exactly once */
    LidMomokaLabelEvent*host=h->host;uint32_t error=host->error_get(host->context);
    if(__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)&&host->thread(host->context)==host->owner&&
       !h->busy&&!h->stopped&&host->live(host->context)&&lid_mse_scope(h,receiver,frame)){
        volatile unsigned completed=0;h->busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
        __try {
#endif
            h->last_result=lid_ms_apply(&h->stock);
            if(h->last_result>=0&&h->visual_after)h->visual_after(h->visual_context);
            completed=1;if(h->last_result<0)h->stopped=1;
#if LID_SHOP_EXEC_SEH_ENABLED
        } __finally {
#endif
            if(!completed)h->stopped=1;h->busy=0;host->error_set(host->context,error);
#if LID_SHOP_EXEC_SEH_ENABLED
        }
#endif
    }
    host->error_set(host->context,error);
}
#endif
