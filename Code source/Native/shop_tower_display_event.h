#ifndef LID_SHOP_TOWER_DISPLAY_EVENT_H
#define LID_SHOP_TOWER_DISPLAY_EVENT_H
#include "shop_tower_auto_view.h"
#include "shop_tower_display_point.h"
#include "shop_tower_display_profiles.h"
#include "shop_tower_image_binding.h"
/* Own exact ItemSelect native vtable slots, not pooled C function entries.
   All callbacks/state remain pinned after any publication, including rollback. */
typedef struct LidTowerDisplayOps {
    void*context;uint32_t(*thread)(void*);uint32_t(*error)(void*);void(*error_set)(void*,uint32_t);
    int(*quiet)(void*);int(*coherent)(void*);int(*resident)(void*,uintptr_t);
    int(*protect)(void*,uintptr_t,unsigned,unsigned*);
} LidTowerDisplayOps;
typedef struct LidTowerDisplayEvent {
    LidTowerAutoView*text;LidTowerDisplayOps ops;uint32_t owner;
    uintptr_t slots[2],original[2],wrappers[2];M207Slot ownership[2];unsigned protection[2];
    unsigned prepared,published,active,busy,stopped,protection_dirty;LidTowerImageBinding*images;
} LidTowerDisplayEvent;
static LidTowerDisplayEvent*g_lid_tower_display_event;
typedef void(*LidTowerInfoNative)(void*);
typedef void(*LidTowerDisableNative)(void*,int,unsigned,unsigned);
static void lid_tde_stop(LidTowerDisplayEvent*h){h->stopped=1;lid_tq_halt(h->text->queue);}
static void lid_tde_info_call(LidTowerDisplayEvent*h,void*self){
    if(!h||!h->prepared)return; /* cannot occur after valid publication */
    LidTowerInfoNative original=(LidTowerInfoNative)h->original[0];
    if(!__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)||h->ops.thread(h->ops.context)!=h->owner||h->busy||h->stopped){original(self);return;}
    uint32_t incoming=h->ops.error(h->ops.context),outgoing=incoming;volatile unsigned normal=0;h->busy=1;
    __try{
        LidTowerDisplayPoint p;int scope=lid_tdp_resolve(h->text->current,(uintptr_t)self,-1,&p);
        if(scope<0)lid_tde_stop(h);
        else if(scope==1){
            int applied=h->images?lid_tib_list(h->images):lid_tav_apply(h->text,p.logical);
            if(applied<0)lid_tde_stop(h);
        }
        h->ops.error_set(h->ops.context,incoming);
        original(self); /* once on ordinary path, AFTER private text update */
        outgoing=h->ops.error(h->ops.context);normal=1;
    }__finally{
        uint32_t error=normal?outgoing:h->ops.error(h->ops.context);
        if(!normal)lid_tde_stop(h);h->busy=0;h->ops.error_set(h->ops.context,error);
    }
}
static void lid_tde_disable_call(LidTowerDisplayEvent*h,void*self,int index,unsigned price,unsigned count){
    if(!h||!h->prepared)return;
    LidTowerDisableNative original=(LidTowerDisableNative)h->original[1];
    if(!__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)||h->ops.thread(h->ops.context)!=h->owner||h->busy||h->stopped){original(self,index,price,count);return;}
    uint32_t incoming=h->ops.error(h->ops.context),outgoing=incoming;volatile unsigned normal=0;h->busy=1;
    __try{
        LidTowerDisplayPoint p;int scope=lid_tdp_resolve(h->text->current,(uintptr_t)self,index,&p);
        if(scope<0)lid_tde_stop(h);
        else if(scope==1){
            uint8_t bought[23];
            /* Busy state is NOT permission to buy. Native affordability is
               preserved by OR, and count-red/raw other argument bits retained. */
            if(!lid_tdv_bought(h->text->queue,bought)||p.flag||(bought[p.offer/8]&(1u<<(p.offer%8))))price|=1u;
        }
        h->ops.error_set(h->ops.context,incoming);original(self,index,price,count);
        outgoing=h->ops.error(h->ops.context);normal=1;
    }__finally{
        uint32_t error=normal?outgoing:h->ops.error(h->ops.context);
        if(!normal)lid_tde_stop(h);h->busy=0;h->ops.error_set(h->ops.context,error);
    }
}
static __attribute__((noinline)) void lid_tde_info(void*self){lid_tde_info_call(__atomic_load_n(&g_lid_tower_display_event,__ATOMIC_ACQUIRE),self);}
static __attribute__((noinline)) void lid_tde_disable(void*self,int i,unsigned p,unsigned n){lid_tde_disable_call(__atomic_load_n(&g_lid_tower_display_event,__ATOMIC_ACQUIRE),self,i,p,n);}
static int lid_tde_prepare(LidTowerDisplayEvent*h,LidTowerAutoView*text,const LidTowerDisplayOps*ops){
    if(!h||h->prepared||!text||!text->prepared||!ops||!ops->thread||!ops->error||!ops->error_set||
       !ops->quiet||!ops->coherent||!ops->resident||!ops->protect||ops->thread(ops->context)!=text->queue->owner)return 0;
    const M195Ops*o=&text->current->meta->memory;uint8_t actual[24];
    h->text=text;h->ops=*ops;h->owner=text->queue->owner;
    const unsigned offsets[2]={0x368,0x3e8};h->wrappers[0]=(uintptr_t)lid_tde_info;h->wrappers[1]=(uintptr_t)lid_tde_disable;
    for(unsigned i=0;i<2;i++){
        h->slots[i]=text->image+0x21e1550+offsets[i];h->original[i]=text->image+lid_tde_profiles[i].rva;
        uintptr_t current;
        if((h->slots[i]&7)||!m195_read(o,h->slots[i],0,&current,8)||current!=h->original[i]||
           !m195_read(o,h->original[i],0,actual,24)||!ops->resident(ops->context,h->wrappers[i]))return 0;
        for(unsigned j=0;j<24;j++)if(actual[j]!=lid_tde_profiles[i].bytes[j])return 0;
    }
    h->prepared=1;return 1;
}
static int lid_tde_publish(LidTowerDisplayEvent*h){
    if(!h||!h->prepared||h->published||h->stopped||h->protection_dirty||
       h->ops.thread(h->ops.context)!=h->owner||h->ops.quiet(h->ops.context)!=1)return 0;
    LidTowerDisplayEvent*empty=0;
    if(!__atomic_compare_exchange_n(&g_lid_tower_display_event,&empty,h,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    volatile unsigned normal=0;int ok=0;
    __try{
        for(unsigned i=0;i<2;i++){
            unsigned ignored;
            if(!h->ops.protect(h->ops.context,h->slots[i],4,&h->protection[i]))goto end;
            h->protection_dirty|=1u<<i;
            int installed=m207_publish(&h->ownership[i],(uintptr_t*)h->slots[i],h->original[i],h->wrappers[i]);
            if(!h->ops.protect(h->ops.context,h->slots[i],h->protection[i],&ignored))goto end;
            h->protection_dirty&=~(1u<<i);if(!installed)goto end;
        }
        h->published=1;ok=1;
end:normal=1;
    }__finally{if(!normal||!ok)h->stopped=1;}
    return ok;
}
static int lid_tde_enable(LidTowerDisplayEvent*h){
    if(!h||!h->published||h->stopped||h->protection_dirty||h->ops.thread(h->ops.context)!=h->owner||
       h->ops.quiet(h->ops.context)!=1||h->ops.coherent(h->ops.context)!=1)return 0;
    const M195Ops*o=&h->text->current->meta->memory;
    for(unsigned i=0;i<2;i++){
        uintptr_t value;if(__atomic_load_n(&h->ownership[i].state,__ATOMIC_ACQUIRE)!=M207_INSTALLED||
            !m195_read(o,h->slots[i],0,&value,8)||value!=h->wrappers[i])return 0;
    }
    __atomic_store_n(&h->active,1,__ATOMIC_RELEASE);return 1;
}
static int lid_tde_revert(LidTowerDisplayEvent*h){
    if(!h||!h->prepared||h->ops.thread(h->ops.context)!=h->owner||h->ops.quiet(h->ops.context)!=1)return 0;
    __atomic_store_n(&h->active,0,__ATOMIC_RELEASE);int ok=1;
    for(unsigned j=2;j;j--){
        unsigned i=j-1,ignored;uintptr_t current;
        if(!m195_read(&h->text->current->meta->memory,h->slots[i],0,&current,8)){ok=0;continue;}
        unsigned state=__atomic_load_n(&h->ownership[i].state,__ATOMIC_ACQUIRE);
        if(current==h->wrappers[i]&&state==M207_INSTALLED){
            if(!(h->protection_dirty&(1u<<i))){
                if(!h->ops.protect(h->ops.context,h->slots[i],4,&h->protection[i])){ok=0;continue;}
                h->protection_dirty|=1u<<i;
            }
            if(!m207_restore(&h->ownership[i]))ok=0;
        }else if(current!=h->original[i])ok=0; /* never overwrite foreign owner */
        if(h->protection_dirty&(1u<<i)){
            if(h->ops.protect(h->ops.context,h->slots[i],h->protection[i],&ignored))h->protection_dirty&=~(1u<<i);else ok=0;
        }
    }
    h->stopped=1;return ok&&!h->protection_dirty;
}
/* Before charge, read actual current private name/type; metadata preparation
   or an earlier selection alone must not authorize buying a carrier label. */
static int lid_tde_offer_ready(LidTowerDisplayEvent*h,unsigned offer){
    if(!h||!h->prepared||!__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)||h->busy||h->stopped||offer>=184||
       h->ops.thread(h->ops.context)!=h->owner){r299_note(14,10,0);return 0;}
    LidTowerUiCurrent*c=h->text->current;LidTowerUiRoots r;LidTowerDisplayPoint p;LidTSArray u;uint64_t object;
    if(!lid_tuc_roots(c,&r)||lid_tdp_resolve(c,(uintptr_t)r.item_select,-1,&p)!=1||p.offer!=offer){r299_note(14,11,offer);return 0;}
    if(
       !lid_tuc_array(&c->meta->memory,r.item_select+h->text->units,0x3e8,36,&u)||p.logical>=(unsigned)u.count||
       !m195_read(&c->meta->memory,(uint64_t)(uintptr_t)u.data+p.logical*0x3e8+h->text->base,h->text->auto_at,&object,8)||
       !lid_tav_own(h->text,object,&lid_tower_stock_data[offer])){r299_note(14,12,offer);return 0;}
    uint64_t image;if(h->images&&!lid_tib_get(h->images,object,&image)){r299_note(14,13,offer);return 0;}
    uint16_t name[LID_UI_NAME_CAP];
    int ready=lid_sd_name(h->text->display,p.location,name,LID_UI_NAME_CAP)&&
        lid_tav_text(&c->meta->memory,object+h->text->name,name,LID_UI_NAME_CAP)&&
        lid_tav_text(&c->meta->memory,object+h->text->part_name,name,LID_UI_NAME_CAP)&&
        lid_tav_text(&c->meta->memory,object+h->text->db_item+h->text->db_type,(const uint16_t*)L"ITTP_MATERIAL",32);
    r299_note(14,ready?15:14,offer);return ready;
}
#endif
