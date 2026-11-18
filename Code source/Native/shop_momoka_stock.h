#ifndef LID_SHOP_MOMOKA_STOCK_H
#define LID_SHOP_MOMOKA_STOCK_H
#include "shop_momoka_labels.h"
#include "shop_purchase_bridge.h"
/* View cache only: never changes a native product, price, quantity or wallet.
   Current buy root/UI thread and verified build are supplied by event host. */
typedef struct LidMomokaStock {
    LidMomokaLabels*labels;LidShopPurchaseBridge*bridge;
    uint32_t arrays[2],stride,skill,enabled,mask;unsigned ready;
} LidMomokaStock;
static int lid_ms_current(LidMomokaLabels*b,uint64_t*out){
    uint64_t explain,db,menu,list,cls,again;
    if(!out||!lid_ml_current(b,&explain)||
       !lid_mr_current(b->root,b->menu_class,b->database_class,&db,&menu)||
       !m195_read(&b->root->memory,menu,b->list,&list,8)||
       !m195_read(&b->root->memory,list,0x50,&cls,8)||cls!=b->list_class||
       !m195_read(&b->root->memory,list,b->explain,&again,8)||again!=explain)return 0;
    *out=list;return 1;
}
static int lid_ms_prepare(LidMomokaStock*s,LidMomokaLabels*b,LidShopPurchaseBridge*bridge){
    uint64_t list,p,inner,structure,actual=0;uint32_t extent,inner_at;
    if(!s||s->ready||!b||!bridge||!bridge->initialized||!lid_ms_current(b,&list))return 0;
    const M195Ops*o=&b->root->memory;
    const char*fields[2]={"mSkillListDetail","mSkillListDetailFiltered"};
    for(unsigned i=0;i<2;i++){
        if(!m212_member(o,list,"BrgUIMenu_SkillExchange_SkillList",fields[i],&p)||
           !lid_ml_property(o,p,"ArrayProperty",1,16,65536,0x58,&s->arrays[i])||
           !m195_read(o,p,0xb0,&inner,8)||!m195_read(o,inner,0xb0,&structure,8)||
           !m195_named(o,structure,"BrgUIMenu_SkillExchange_SkillListSkillDetail")||
           !m195_read(o,structure,0x88,&extent,4)||extent<20||extent>4096||
           !lid_ml_property(o,inner,"StructProperty",1,extent,extent,0,&inner_at)||inner_at||
           (i&&structure!=actual))return 0;
        actual=structure;s->stride=extent;
    }
    if(s->arrays[0]<s->arrays[1]+16&&s->arrays[1]<s->arrays[0]+16)return 0;
    if(!lid_ml_struct_member(o,structure,"SkillID",&p)||
       !lid_ml_property(o,p,"StrProperty",1,16,extent,0,&s->skill)||
       !lid_ml_struct_member(o,structure,"IsEnable",&p)||
       !lid_ml_property(o,p,"BoolProperty",1,4,extent,0,&s->enabled)||
       !m195_read(o,p,0xb0,&s->mask,4)||!s->mask||(s->mask&(s->mask-1))||
       (s->skill<s->enabled+4&&s->enabled<s->skill+16))return 0;
    s->labels=b;s->bridge=bridge;s->ready=1;return 1;
}
/* Short existing bridge lock, own C-state only, no engine calls while held.
   Busy means skip this display update; payment admission remains authoritative. */
static int lid_ms_snapshot(LidShopPurchaseBridge*b,uint8_t unavailable[512]){
    uint32_t tid;int ok=0;
    if(!unavailable||!lid_pb_lock(b,&tid))return 0;
    if(tid==b->ui&&b->ready&&b->live(b->provider)){
        r15_copy(unavailable,b->bought,512);
        for(unsigned i=0;i<LID_PB_CAP;i++)if(lid_pb_phase(&b->slots[i])!=LID_PB_EMPTY){
            unsigned offer=b->slots[i].row.offer;
            if(offer<4096&&lid_pb_bit(b->allowed,offer))unavailable[offer/8]|=(uint8_t)(1u<<(offer%8));
        }
        ok=1;
    }
    lid_pb_unlock(b);return ok;
}
static int lid_ms_array(const M195Ops*o,uint64_t address,unsigned stride,M197StringHeader*out){
    M197StringHeader a;
    if(!stride||!out||!m195_read(o,address,0,&a,16)||a.count<0||a.count>4096||a.capacity<a.count||a.capacity>4096||
       (a.capacity&&!a.data)||(!a.capacity&&a.data)||a.data>UINT64_MAX-(uint64_t)a.capacity*stride)return 0;
    *out=a;return 1;
}
static int lid_ms_apply(LidMomokaStock*s){
    uint64_t list,again;uint8_t unavailable[512];int changed=0;
    if(!s||!s->ready||!s->bridge->thread||s->bridge->thread(s->bridge->provider)!=s->bridge->ui||
       !lid_ms_current(s->labels,&list))return -1;
    if(!lid_ms_snapshot(s->bridge,unavailable))return 0;
    LidMomokaLabels*b=s->labels;const M195Ops*o=&b->root->memory;
    const LidShopRuntimeCatalogue*c=b->display->catalogue;
    if(!c||!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE))return -1;
    for(unsigned array=0;array<2;array++){
        M197StringHeader a,after;
        if(!lid_ms_current(b,&again)||again!=list||!lid_ms_array(o,list+s->arrays[array],s->stride,&a))return -1;
        for(int index=0;index<a.count;index++){
            uint64_t at=a.data+(uint64_t)index*s->stride;uint16_t id[128],check[128];unsigned units,units_after;
            M195Signature sig={0};sig.skill_offset=s->skill;sig.parms_size=(uint16_t)s->stride;
            if(!m197_string(o,at,&sig,id,&units))return -1;
            unsigned offer=UINT32_MAX;
            for(unsigned i=0;i<c->momoka_count;i++){
                const LidShopUiSku*sku=&c->momoka[i];if(sku->units!=units)continue;
                unsigned j=0;while(j<units&&id[j]==sku->skill[j])j++;if(j==units){offer=sku->offer;break;}
            }
            if(offer==UINT32_MAX||!lid_pb_bit(unavailable,offer))continue;
            uint32_t flags;
            if(!lid_ms_current(b,&again)||again!=list||!lid_ms_array(o,list+s->arrays[array],s->stride,&after)||
               after.data!=a.data||after.count!=a.count||after.capacity!=a.capacity||
               !m197_string(o,at,&sig,check,&units_after)||units_after!=units||
               !m195_read(o,at,s->enabled,&flags,4))return -1;
            for(unsigned i=0;i<units;i++)if(id[i]!=check[i])return -1;
            if(flags&s->mask){*(uint32_t*)(uintptr_t)(at+s->enabled)=flags&~s->mask;changed++;}
        }
    }
    return changed;
}
#endif
