#ifndef LID_SHOP_TOWER_ICON_PROVIDER_H
#define LID_SHOP_TOWER_ICON_PROVIDER_H
#include "shop_tower_ap_image.h"
#include "shop_tower_display_view.h"
#include "shop_momoka_icon_provider.h"
/* Same native verified Stream getters and pure metadata helper as Momoka;
   completely independent current-game/UI root and AP fallback. No fake labels
   host, carrier-art fallback, native-item-ID mutation or unrooted image cache. */
typedef struct LidTowerIconProvider {
    LidTowerUiCurrent*current;const LidShopDisplay*display;LidTowerAPImage*ap;
    uint64_t image,ui_class,item_class,enma_class;uint32_t item,enma;unsigned ready;
} LidTowerIconProvider;
static int lid_tip_ui(LidTowerUiCurrent*c,uint64_t*out,uint64_t*cls){
    uint64_t game,actual,ui;
    if(!c||!c->prepared||!out||!cls||!c->ops.live(c->ops.live_context)||
       !c->ops.game(c->ops.game_context,&game)||!m195_read(&c->meta->memory,game,0x50,&actual,8)||actual!=c->game_class||
       !m195_read(&c->meta->memory,game,c->manager,&ui,8)||!m195_read(&c->meta->memory,ui,0x50,&actual,8)||actual!=c->manager_class)return 0;
    *out=ui;*cls=actual;return 1;
}
static int lid_tip_prepare(LidTowerIconProvider*p,LidTowerUiCurrent*c,const LidShopDisplay*d,LidTowerAPImage*ap,uintptr_t image,int verified){
    static const uint8_t part_prefix[24]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0x98,0x48,0x81,0xec,0x40,0x01,0,0,0x48};
    static const uint8_t skill_prefix[24]={0x48,0x8b,0xc4,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x50,0x48,0xc7,0x40,0xa8,0xfe,0xff,0xff,0xff,0x48};
    LidTowerIconProvider v={0};uint64_t ui,item,enma,ignored,fallback;uint8_t bytes[24];
    if(!p||p->ready||!c||!c->prepared||!c->catalogue||!d||!d->io.thread_id||!ap||ap->image!=image||verified!=1||
       ap->ops.memory!=&c->meta->memory||ap->ops.owner!=c->catalogue->ui||
       image>UINTPTR_MAX-0x2005bd8||d->io.thread_id()!=c->catalogue->ui||d->catalogue!=c->catalogue||
       __atomic_load_n(&d->state,__ATOMIC_ACQUIRE)!=LID_SD_READY||!lid_tai_get(ap,&fallback)||!lid_tip_ui(c,&ui,&v.ui_class))return 0;
    const M195Ops*o=&c->meta->memory;
    if(!lid_ml_field(o,ui,"BrgUIManagerBase","mItemIconManager","ObjectProperty",8,&v.item)||
       !lid_ml_field(o,ui,"BrgUIManagerBase","mEnmaMenuImageManager","ObjectProperty",8,&v.enma)||
       !m195_read(o,ui,v.item,&item,8)||!m195_read(o,item,0x50,&v.item_class,8)||!m195_named(o,v.item_class,"BrgUIResource_ItemIconManager")||
       !m195_read(o,ui,v.enma,&enma,8)||!m195_read(o,enma,0x50,&v.enma_class,8)||!m195_named(o,v.enma_class,"BrgUIResource_EnmaMenuImageManager")||
       !lid_mip_manager(o,ui,v.item,v.item_class,image+0x2005910,image+0xc37bd0,&ignored)||
       !lid_mip_manager(o,ui,v.enma,v.enma_class,image+0x2004f50,image+0xc41f60,&ignored)||
       !m195_read(o,image,0xc37bd0,bytes,24))return 0;
    for(unsigned i=0;i<24;i++)if(bytes[i]!=part_prefix[i])return 0;
    if(!m195_read(o,image,0xc41f60,bytes,24))return 0;
    for(unsigned i=0;i<24;i++)if(bytes[i]!=skill_prefix[i])return 0;
    v.current=c;v.display=d;v.ap=ap;v.image=image;v.ready=1;*p=v;return 1;
}
/* 1 validated Stream ready for IMMEDIATE assignment to a reflected owner in
   this synchronous UI event; 0 refused. No raw Stream cached between events.
   Native absence falls back to AP only after revalidating current manager. */
static int lid_tip_get(LidTowerIconProvider*p,uint64_t location,uint64_t*out){
    if(!p||!p->ready||!out)return 0;*out=0;
    uint64_t ui,cls,again,after_cls,manager,result=0;const uint8_t*row=lid_sd_row(p->display,location);
    if(!row||r15_r32(row+28)!=2||!lid_tip_ui(p->current,&ui,&cls)||cls!=p->ui_class)return 0;
    unsigned offer=r15_r32(row+32),kind=r15_r32(row+24);
    if(offer>=184||location!=lid_tower_stock_data[offer].location||kind>3)return 0;
    if(kind){
        uint16_t key[96];unsigned units=0;
        for(;units<96&&row[64+units];units++){
            if(row[64+units]<0x21||row[64+units]>0x7e)return 0;key[units]=row[64+units];
        }
        if(!units||units==96)return 0;key[units]=0;
        M197StringHeader input={(uint64_t)(uintptr_t)key,(int32_t)units+1,(int32_t)units+1};
        const M195Ops*o=&p->current->meta->memory;
        if(kind==2){
            if(!lid_mip_manager(o,ui,p->enma,p->enma_class,p->image+0x2004f50,p->image+0xc41f60,&manager))return 0;
            result=((LidMomokaSkillImage)(uintptr_t)(p->image+0xc41f60))((void*)(uintptr_t)manager,&input,2);
        }else{
            if(!lid_mip_manager(o,ui,p->item,p->item_class,p->image+0x2005910,p->image+0xc37bd0,&manager))return 0;
            result=((LidMomokaPartImage)(uintptr_t)(p->image+0xc37bd0))((void*)(uintptr_t)manager,&input,0,0,0);
        }
        if(!lid_tip_ui(p->current,&again,&after_cls)||again!=ui||after_cls!=cls)return 0;
        if(result){
            uint64_t vt,image_cls;
            if(m195_read(o,result,0,&vt,8)&&vt==p->image+0x1ff1e70&&
               m195_read(o,result,0x50,&image_cls,8)&&image_cls==p->ap->stream_class){*out=result;return 1;}
        }
    }
    if(!lid_tip_ui(p->current,&again,&after_cls)||again!=ui||after_cls!=cls)return 0;
    return lid_tai_get(p->ap,out);
}
#endif
