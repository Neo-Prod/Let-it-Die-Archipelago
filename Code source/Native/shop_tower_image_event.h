#ifndef LID_SHOP_TOWER_IMAGE_EVENT_H
#define LID_SHOP_TOWER_IMAGE_EVENT_H
#include "shop_tower_image_binding.h"
#include "shop_tower_display_event.h"
typedef struct LidTowerImageFrame {uint64_t self,hud,image;unsigned lane,drawn;} LidTowerImageFrame;
typedef struct LidTowerImageEvent {
    LidTowerImageBinding*images;LidTowerDisplayOps ops;uint32_t owner;
    uintptr_t slots[5],original[5],wrappers[5];M207Slot ownership[5];unsigned protection[5];
    unsigned prepared,published,active,depth,drawing,stopped,protection_dirty;
    LidTowerImageFrame frame;
} LidTowerImageEvent;
static LidTowerImageEvent*g_lid_tower_image_event;
typedef void(*LidTowerGridImageNative)(void*,void*,float,float,float,unsigned,void*);
typedef void(*LidTowerPanelImageNative)(void*,void*,void*,float,float,float,float,int);
typedef void(*LidTowerPosImageNative)(void*,void*,float,float,unsigned,unsigned);
typedef void(*LidTowerMiniNative)(void*,void*,void*,float,float,float,unsigned,unsigned,unsigned,unsigned);
static void lid_tir_stop(LidTowerImageEvent*h){h->stopped=1;lid_tq_halt(h->images->text->queue);}
static int lid_tir_enter(LidTowerImageEvent*h){
    return h&&h->prepared&&__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)&&!h->stopped&&
        h->ops.thread(h->ops.context)==h->owner;
}
static void lid_tir_grid_call(LidTowerImageEvent*h,void*self,void*hud,float x,float y,float alpha,unsigned gray,void*unit){
    if(!h||!h->prepared)return;
    LidTowerGridImageNative original=(LidTowerGridImageNative)h->original[0];
    if(!lid_tir_enter(h)){original(self,hud,x,y,alpha,gray,unit);return;}
    uint32_t incoming=h->ops.error(h->ops.context),outgoing=incoming;volatile unsigned normal=0;
    LidTowerImageFrame previous=h->frame;h->frame=(LidTowerImageFrame){0};h->depth++;
    __try{
        uint64_t automatic,image;
        if(h->depth==1&&(uintptr_t)self==h->images->item_select&&
           m195_read(&h->images->text->current->meta->memory,(uintptr_t)unit,0x300,&automatic,8)&&
           lid_tib_get(h->images,automatic,&image))h->frame=(LidTowerImageFrame){(uintptr_t)self,(uintptr_t)hud,image,0,0};
        h->ops.error_set(h->ops.context,incoming);original(self,hud,x,y,alpha,gray,unit);
        outgoing=h->ops.error(h->ops.context);normal=1;
    }__finally{
        uint32_t error=normal?outgoing:h->ops.error(h->ops.context);
        h->frame=previous;h->depth--;if(!normal)lid_tir_stop(h);h->ops.error_set(h->ops.context,error);
    }
}
static void lid_tir_panel_call(LidTowerImageEvent*h,void*self,void*hud,void*info,float x,float y,float scale,float alpha,int target){
    if(!h||!h->prepared)return;
    LidTowerPanelImageNative original=(LidTowerPanelImageNative)h->original[1];
    if(!lid_tir_enter(h)){original(self,hud,info,x,y,scale,alpha,target);return;}
    uint32_t incoming=h->ops.error(h->ops.context),outgoing=incoming;volatile unsigned normal=0;
    LidTowerImageFrame previous=h->frame;h->frame=(LidTowerImageFrame){0};h->depth++;
    __try{
        uint64_t automatic,image,mini,vt;
        const M195Ops*o=&h->images->text->current->meta->memory;
        if(h->depth==1&&m195_read(o,(uintptr_t)info,0x234,&automatic,8)&&lid_tib_get(h->images,automatic,&image)&&
           m195_read(o,(uintptr_t)self,0x8c,&mini,8)&&m195_read(o,mini,0,&vt,8)&&
           vt==h->images->text->image+0x21e1200)h->frame=(LidTowerImageFrame){mini,(uintptr_t)hud,image,1,0};
        /* Native owns the by-value Info storage and its cleanup. Pass ONCE;
           no retained pointer or inspection after native destructor. */
        h->ops.error_set(h->ops.context,incoming);original(self,hud,info,x,y,scale,alpha,target);
        outgoing=h->ops.error(h->ops.context);normal=1;
    }__finally{
        uint32_t error=normal?outgoing:h->ops.error(h->ops.context);
        h->frame=previous;h->depth--;if(!normal)lid_tir_stop(h);h->ops.error_set(h->ops.context,error);
    }
}
/* New-info grid/list path. Native owns and destroys the by-value Info. Read
   only before forwarding, and never replace a parent's description scope. */
static void lid_tir_mini_call(LidTowerImageEvent*h,void*self,void*hud,void*info,float alpha,float x,float y,
                              unsigned selected,unsigned use_frame,unsigned armor_skin,unsigned equipped){
    if(!h||!h->prepared)return;
    LidTowerMiniNative original=(LidTowerMiniNative)h->original[4];
    if(!lid_tir_enter(h)||h->depth){original(self,hud,info,alpha,x,y,selected,use_frame,armor_skin,equipped);return;}
    uint32_t incoming=h->ops.error(h->ops.context);uint64_t image;
    if(!lid_tib_mini(h->images,(uintptr_t)self,(uintptr_t)info,&image)){
        h->ops.error_set(h->ops.context,incoming);original(self,hud,info,alpha,x,y,selected,use_frame,armor_skin,equipped);return;
    }
    uint32_t outgoing=incoming;volatile unsigned normal=0;LidTowerImageFrame previous=h->frame;
    h->depth++;h->frame=(LidTowerImageFrame){(uintptr_t)self,(uintptr_t)hud,image,1,0};
    __try{
        h->ops.error_set(h->ops.context,incoming);original(self,hud,info,alpha,x,y,selected,use_frame,armor_skin,equipped);
        outgoing=h->ops.error(h->ops.context);normal=1;
    }__finally{
        uint32_t error=normal?outgoing:h->ops.error(h->ops.context);
        h->frame=previous;h->depth--;if(!normal)lid_tir_stop(h);h->ops.error_set(h->ops.context,error);
    }
}
static void lid_tir_pos_call(LidTowerImageEvent*h,unsigned lane,uintptr_t caller,void*self,void*image,float x,float y,unsigned horizontal,unsigned vertical){
    if(!h||!h->prepared||lane>1)return;
    LidTowerPosImageNative original=(LidTowerPosImageNative)h->original[2+lane];
    if(!lid_tir_enter(h)||h->depth!=1||h->drawing||h->frame.self!=(uintptr_t)self||h->frame.lane!=lane){original(self,image,x,y,horizontal,vertical);return;}
    uint32_t incoming=h->ops.error(h->ops.context),outgoing=incoming;volatile unsigned normal=0;h->drawing=1;
    __try{
        unsigned site=lid_tid_site(&h->images->draw,caller,lane);int replaced=0;uint64_t manager,current_hud;
        const M195Ops*o=&h->images->text->current->meta->memory;
        if(!m195_read(o,(uintptr_t)self,0x60,&manager,8)||!m195_read(o,manager,0xa4,&current_hud,8)||current_hud!=h->frame.hud)site=0;
        h->ops.error_set(h->ops.context,incoming);
        if(site==1){
            replaced=lid_tid_draw(&h->images->draw,h->frame.hud,(uintptr_t)image,h->frame.image,x,y,horizontal,vertical);
            if(replaced)h->frame.drawn=1;
        }else if(site==2&&h->frame.drawn)replaced=1;
        if(!replaced){h->ops.error_set(h->ops.context,incoming);original(self,image,x,y,horizontal,vertical);}
        /* Our substituted draw/overlay skip has no caller-visible Win32 API
           result. Do not leak validation/resource-request LastError changes.
           On native fallback preserve exactly the original call's result. */
        outgoing=replaced?incoming:h->ops.error(h->ops.context);normal=1;
    }__finally{
        uint32_t error=normal?outgoing:h->ops.error(h->ops.context);
        h->drawing=0;if(!normal)lid_tir_stop(h);h->ops.error_set(h->ops.context,error);
    }
}
static __attribute__((noinline)) void lid_tir_grid(void*self,void*hud,float x,float y,float alpha,unsigned gray,void*unit){lid_tir_grid_call(__atomic_load_n(&g_lid_tower_image_event,__ATOMIC_ACQUIRE),self,hud,x,y,alpha,gray,unit);}
static __attribute__((noinline)) void lid_tir_panel(void*self,void*hud,void*info,float x,float y,float scale,float alpha,int target){lid_tir_panel_call(__atomic_load_n(&g_lid_tower_image_event,__ATOMIC_ACQUIRE),self,hud,info,x,y,scale,alpha,target);}
static __attribute__((noinline)) void lid_tir_grid_pos(void*self,void*image,float x,float y,unsigned horizontal,unsigned vertical){lid_tir_pos_call(__atomic_load_n(&g_lid_tower_image_event,__ATOMIC_ACQUIRE),0,(uintptr_t)__builtin_return_address(0),self,image,x,y,horizontal,vertical);}
static __attribute__((noinline)) void lid_tir_mini_pos(void*self,void*image,float x,float y,unsigned horizontal,unsigned vertical){lid_tir_pos_call(__atomic_load_n(&g_lid_tower_image_event,__ATOMIC_ACQUIRE),1,(uintptr_t)__builtin_return_address(0),self,image,x,y,horizontal,vertical);}
static __attribute__((noinline)) void lid_tir_mini(void*self,void*hud,void*info,float alpha,float x,float y,unsigned selected,unsigned use_frame,unsigned armor_skin,unsigned equipped){
    lid_tir_mini_call(__atomic_load_n(&g_lid_tower_image_event,__ATOMIC_ACQUIRE),self,hud,info,alpha,x,y,selected,use_frame,armor_skin,equipped);
}
static int lid_tir_prepare(LidTowerImageEvent*h,LidTowerImageBinding*images,const LidTowerDisplayOps*ops){
    if(!h||h->prepared||!images||!images->prepared||!ops||!ops->thread||!ops->error||!ops->error_set||!ops->quiet||!ops->coherent||!ops->resident||!ops->protect||ops->thread(ops->context)!=images->text->queue->owner)return 0;
    static const uint32_t offsets[5]={0x21e1550+0x3b0,0x21df170+0x340,0x21e1550+0x268,0x21e1200+0x268,0x21e1200+0x308};
    static const uint32_t rvas[5]={0x14c8980,0x14b5450,0xc1adb0,0xc1adb0,0x14c7290};
    static const uint8_t prefixes[4][24]={
        {0x48,0x8b,0xc4,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xa0,0,0,0,0x48,0xc7,0x40,0x88,0xfe,0xff,0xff,0xff,0x48},
        {0x48,0x8b,0xc4,0x4c,0x89,0x40,0x18,0x55,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xd8,0xfc,0xff,0xff,0x48,0x81},
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0x41,0x60,0x48,0x8b,0xf9,0x0f,0x29,0x74,0x24,0x30,0x48,0x8b},
        {0x48,0x8b,0xc4,0x4c,0x89,0x40,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0x88,0xfc,0xff}};
    h->images=images;h->ops=*ops;h->owner=images->text->queue->owner;
    h->wrappers[0]=(uintptr_t)lid_tir_grid;h->wrappers[1]=(uintptr_t)lid_tir_panel;h->wrappers[2]=(uintptr_t)lid_tir_grid_pos;h->wrappers[3]=(uintptr_t)lid_tir_mini_pos;
    h->wrappers[4]=(uintptr_t)lid_tir_mini;
    for(unsigned i=0;i<5;i++){
        uintptr_t current;uint8_t bytes[24];h->slots[i]=images->text->image+offsets[i];h->original[i]=images->text->image+rvas[i];
        if((h->slots[i]&7)||!ops->resident(ops->context,h->wrappers[i])||!m195_read(&images->text->current->meta->memory,h->slots[i],0,&current,8)||current!=h->original[i]||!m195_read(&images->text->current->meta->memory,h->original[i],0,bytes,24))return 0;
        for(unsigned j=0;j<24;j++)if(bytes[j]!=prefixes[i==4?3:(i<2?i:2)][j])return 0;
    }
    h->prepared=1;return 1;
}
/* Publication helpers follow below; provider must remain resident forever
   after any slot has become externally visible, including failed rollback. */
static int lid_tir_publish(LidTowerImageEvent*h){
    if(!h||!h->prepared||h->published||h->stopped||h->protection_dirty||
       h->ops.thread(h->ops.context)!=h->owner||h->ops.quiet(h->ops.context)!=1)return 0;
    LidTowerImageEvent*empty=0;
    if(!__atomic_compare_exchange_n(&g_lid_tower_image_event,&empty,h,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    volatile unsigned normal=0;int ok=0;
    __try{
        for(unsigned i=0;i<5;i++){
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
static int lid_tir_enable(LidTowerImageEvent*h){
    if(!h||!h->published||h->stopped||h->protection_dirty||h->ops.thread(h->ops.context)!=h->owner||
       h->ops.quiet(h->ops.context)!=1||h->ops.coherent(h->ops.context)!=1)return 0;
    const M195Ops*o=&h->images->text->current->meta->memory;
    for(unsigned i=0;i<5;i++){
        uintptr_t value;if(__atomic_load_n(&h->ownership[i].state,__ATOMIC_ACQUIRE)!=M207_INSTALLED||
            !m195_read(o,h->slots[i],0,&value,8)||value!=h->wrappers[i])return 0;
    }
    __atomic_store_n(&h->active,1,__ATOMIC_RELEASE);return 1;
}
static int lid_tir_revert(LidTowerImageEvent*h){
    if(!h||!h->prepared||h->ops.thread(h->ops.context)!=h->owner||h->ops.quiet(h->ops.context)!=1)return 0;
    __atomic_store_n(&h->active,0,__ATOMIC_RELEASE);int ok=1;
    for(unsigned j=5;j;j--){
        unsigned i=j-1,ignored;uintptr_t current;
        if(!m195_read(&h->images->text->current->meta->memory,h->slots[i],0,&current,8)){ok=0;continue;}
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
#endif
