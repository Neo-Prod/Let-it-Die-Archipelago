#ifndef LID_SHOP_TOWER_UI_META_H
#define LID_SHOP_TOWER_UI_META_H
#include "shop_momoka_label_event.h"
/* Cold READ-ONLY metadata. No UI result override/slot publication/reservation,
   inventory write, engine call or cached instance. Native execs may be linker-
   pooled: ownership must target each EXACT UFunction.f0, never the C entry. */
enum {LID_TUM_STATE,LID_TUM_OTHER,LID_TUM_YESNO,LID_TUM_SELECT,LID_TUM_PRICE,
      LID_TUM_PENDING,LID_TUM_IDS,LID_TUM_HISTORY,LID_TUM_ITEMSELECT,LID_TUM_END,LID_TUM_FIELDS};
typedef struct LidTowerUiMeta {
    M195Ops memory;uint64_t image,shop_class,window_class,item_class,tick,buy;
    M205Native native[2];uint32_t offsets[LID_TUM_FIELDS],end_mask;
    uint32_t history_stride,history_type,history_item;unsigned ready;
} LidTowerUiMeta;
static int lid_tum_class(const M195Ops*o,uint64_t cls,const char*name){
    uint64_t meta,flags;
    return cls&&m195_named(o,cls,name)&&m195_read(o,cls,0x50,&meta,8)&&m195_named(o,meta,"Class")&&
        m195_read(o,cls,0x10,&flags,8)&&(flags&0x4000)!=0;
}
static int lid_tum_owner(const M195Ops*o,uint64_t cls,const char*name,uint64_t*out){
    uint64_t seen[64],found=0,next;unsigned count=0;
    if(!out)return 0;
    while(cls){
        if(count==64)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==cls)return 0;seen[count++]=cls;
        if(m195_named(o,cls,name)){if(found)return 0;found=cls;}
        if(!m195_read(o,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!found)return 0;*out=found;return 1;
}
static int lid_tum_native(const M195Ops*o,uint64_t owner,const char*name,uint64_t expected,
                         const char*return_type,M205Native*out){
    uint64_t f,meta,root,child,next,seen[128],pf;uint32_t flags,extent,at;uint16_t index,parms,ret;
    unsigned count=0,found=0;M205Native value={0};
    if(!out||!lid_mle_member(o,owner,name,&f)||!m195_read(o,f,0x50,&meta,8)||!m195_named(o,meta,"Function")||
       !m195_read(o,f,0x10,&root,8)||!(root&0x4000)||!m195_read(o,f,0xd0,&flags,4)||!(flags&0x400)||
       !m195_read(o,f,0xd4,&index,2)||index||!m195_read(o,f,0x88,&extent,4)||extent<4||extent>65536||
       !m195_read(o,f,0xe2,&parms,2)||parms!=4||!m195_read(o,f,0xe4,&ret,2)||ret||
       !m195_read(o,f,0xf0,&value.original,8)||value.original!=expected||f>UINT64_MAX-0xf0||((f+0xf0)&7)||
       !m195_read(o,f,0x80,&child,8))return 0;
    while(child){
        if(count==128)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==child)return 0;seen[count++]=child;
        if(!m195_read(o,child,0x60,&next,8)||!m195_read(o,child,0x70,&pf,8))return 0;
        if(pf&0x80){
            if(found++||!m195_named(o,child,"ReturnValue")||(pf&0x580)!=0x580||
               !lid_ml_property(o,child,return_type,1,4,4,0,&at)||at)return 0;
            if(return_type[0]=='B'){
                uint32_t mask;if(!m195_read(o,child,0xb0,&mask,4)||mask!=1)return 0;
            }
        }child=next;
    }
    if(found!=1)return 0;
    value.function=f;value.slot=f+0xf0;value.flags=flags;*out=value;return 1;
}
static int lid_tum_script(const M195Ops*o,uint64_t owner,const char*name,int tick,uint64_t*out){
    uint64_t f,meta,child,next,pf,seen[128];uint32_t flags,extent,at;uint16_t parms;unsigned count=0,found=0;
    if(!out||!lid_mle_member(o,owner,name,&f)||!m195_read(o,f,0x50,&meta,8)||!m195_named(o,meta,"Function")||
       !m195_read(o,f,0xd0,&flags,4)||(flags&0x400)||!m195_read(o,f,0xe2,&parms,2)||parms!=(tick?4:0)||
       !m195_read(o,f,0x88,&extent,4)||extent<parms||extent>65536||!m195_read(o,f,0x80,&child,8))return 0;
    while(child){
        if(count==128)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==child)return 0;seen[count++]=child;
        if(!m195_read(o,child,0x60,&next,8)||!m195_read(o,child,0x70,&pf,8))return 0;
        if(pf&0x80){
            if(!tick||found++||(pf&0x500)||!m195_named(o,child,"DeltaTime")||
               !lid_ml_property(o,child,"FloatProperty",1,4,4,0,&at)||at)return 0;
        }child=next;
    }
    if(found!=(unsigned)tick)return 0;*out=f;return 1;
}
static int lid_tum_array(const M195Ops*o,uint64_t owner,const char*name,uint32_t extent,
                        const char*structure_name,uint32_t size,uint32_t*offset,uint64_t*structure){
    uint64_t p,inner,s,meta;uint32_t actual,at;
    if(!offset||!structure||!lid_mle_member(o,owner,name,&p)||
       !lid_ml_property(o,p,"ArrayProperty",1,16,extent,0x58,offset)||
       !m195_read(o,p,0xb0,&inner,8)||!m195_read(o,inner,0xb0,&s,8)||
       !m195_named(o,s,structure_name)||!m195_read(o,s,0x50,&meta,8)||!m195_named(o,meta,"ScriptStruct")||
       !m195_read(o,s,0x88,&actual,4)||actual!=size||
       !lid_ml_property(o,inner,"StructProperty",1,size,size,0,&at)||at)return 0;
    *structure=s;return 1;
}
/* 0 WAIT only for NULL native class caches; -1 malformed/current owner conflict;
   1 ready. WAIT does no member walk and publishes no partial metadata. */
static int lid_tum_prepare(LidTowerUiMeta*out,const M195Ops*memory,uint64_t image){
    if(!out||out->ready||!memory||!memory->copy||!memory->name_is||!image||image>UINT64_MAX-0xfd1dc58)return -1;
    LidTowerUiMeta b={0};b.memory=*memory;b.image=image;const M195Ops*o=&b.memory;
    if(!m195_read(o,image,0xfd1dc50,&b.shop_class,8)||!m195_read(o,image,0xfd1d9f0,&b.window_class,8)||
       !m195_read(o,image,0xfd1dad8,&b.item_class,8))return -1;
    if(!b.shop_class||!b.window_class||!b.item_class)return 0;
    if(!lid_tum_class(o,b.shop_class,"BrgUIMenu_ShopBase")||!lid_tum_class(o,b.window_class,"BrgUIHUD_SystemWindow")||
       !lid_tum_class(o,b.item_class,"BrgUIMenuPart_ItemSelect"))return -1;
    static const uint8_t prefix[24]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0xff,0x42,0x24,0x48,0x8b,0x42,0x24,0x49,0x8b,0xf8,0x80,0x38,0x41};
    const unsigned exec[2]={0x1012a70,0x103a320};uint8_t bytes[24];
    for(unsigned i=0;i<2;i++){
        if(!m195_read(o,image,exec[i],bytes,24))return -1;
        for(unsigned j=0;j<24;j++)if(bytes[j]!=prefix[j])return -1;
    }
    if(!lid_tum_native(o,b.window_class,"CheckCancel",image+exec[0],"BoolProperty",&b.native[0])||
       !lid_tum_native(o,b.item_class,"GetSelectItemCount",image+exec[1],"IntProperty",&b.native[1])||
       b.native[0].slot==b.native[1].slot||
       !lid_tum_script(o,b.shop_class,"TickASync",1,&b.tick)||!lid_tum_script(o,b.shop_class,"BuyItem",0,&b.buy)||b.tick==b.buy)return -1;
    uint64_t base,p,property_class,structure;uint32_t extent,base_extent;
    if(!m195_read(o,b.shop_class,0x88,&extent,4)||extent!=0x250||
       !lid_tum_owner(o,b.shop_class,"BrgUIMenu_Base",&base)||!m195_read(o,base,0x88,&base_extent,4)||
       base_extent>extent||!lid_mle_member(o,base,"mState",&p)||
       !lid_ml_property(o,p,"IntProperty",1,4,base_extent,0x58,&b.offsets[LID_TUM_STATE]))return -1;
    const char*names[5]={"mOtherStateNum","mYesNoSelectIndex","mBuyMenuSelectIndex","mSelectPrice","mBuyItemCount"};
    for(unsigned i=0;i<5;i++)if(!lid_mle_member(o,b.shop_class,names[i],&p)||
        !lid_ml_property(o,p,"IntProperty",1,4,extent,0x58,&b.offsets[LID_TUM_OTHER+i]))return -1;
    if(!lid_tum_array(o,b.shop_class,"mBuyItemIds",extent,"BrgUIMenu_ShopBase_DiffInfo",20,&b.offsets[LID_TUM_IDS],&structure)||
       !lid_tum_array(o,b.shop_class,"mDbUserShopHistorys",extent,"BrgDbUserShopHistory",148,&b.offsets[LID_TUM_HISTORY],&structure)||
       !lid_ml_struct_member(o,structure,"mType",&p)||!lid_ml_property(o,p,"StrProperty",1,16,148,0,&b.history_type)||
       !lid_ml_struct_member(o,structure,"mItemId",&p)||!lid_ml_property(o,p,"StrProperty",1,16,148,0,&b.history_item)||
       (b.history_type<b.history_item+16&&b.history_item<b.history_type+16)||
       !lid_mle_member(o,b.shop_class,"mItemSelect",&p)||
       !lid_ml_property(o,p,"ObjectProperty",1,8,extent,0x58,&b.offsets[LID_TUM_ITEMSELECT])||
       !m195_read(o,p,0xb0,&property_class,8)||property_class!=b.item_class||
       !lid_mle_member(o,b.shop_class,"mEnd",&p)||!lid_ml_property(o,p,"BoolProperty",1,4,extent,0x58,&b.offsets[LID_TUM_END])||
       !m195_read(o,p,0xb0,&b.end_mask,4)||!b.end_mask||(b.end_mask&(b.end_mask-1)))return -1;
    const unsigned widths[LID_TUM_FIELDS]={4,4,4,4,4,4,16,16,8,4};
    for(unsigned i=0;i<LID_TUM_FIELDS;i++)for(unsigned j=0;j<i;j++)
        if(b.offsets[i]<b.offsets[j]+widths[j]&&b.offsets[j]<b.offsets[i]+widths[i])return -1;
    b.history_stride=148;b.ready=1;*out=b;return 1;
}
#endif
