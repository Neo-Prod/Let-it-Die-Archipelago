#ifndef LID_SHOP_MOMOKA_ICON_PROVIDER_H
#define LID_SHOP_MOMOKA_ICON_PROVIDER_H
#include "shop_momoka_labels.h"
/* Not connected to rendering yet. Exact observed executable/SHA admission is
   a host precondition. Return native Stream only, never cast generic UIImage.
   No C-side UObject cache, root-flag mutation, new texture, IO or per-frame work.
   Returned image must immediately enter a typed game-owned image property in
   the SAME synchronous UI lifetime. A raw pointer is not a GC root. */
enum {LID_MIP_REFUSED=-1,LID_MIP_NEEDS_AP=0,LID_MIP_NATIVE=1};
typedef struct LidMomokaIconProvider {
    LidMomokaLabels*labels;uint64_t image,ui_class,item_class,enma_class;
    uint32_t item,enma;unsigned ready;
} LidMomokaIconProvider;
typedef uint64_t(*LidMomokaPartImage)(void*,const M197StringHeader*,int,uint8_t,int);
typedef uint64_t(*LidMomokaSkillImage)(void*,const M197StringHeader*,uint8_t);
/* Current chain only. Root helper checks same current game/session/UI owner.
   No old manager instance survives between calls. */
static int lid_mip_ui(LidMomokaLabels*b,uint64_t*out,uint64_t*cls){
    uint64_t explain,game,ui,current;
    if(!b||!b->ready||!out||!cls||!lid_ml_current(b,&explain)||
       !b->root->current_game(b->root->context,&game)||
       !m195_read(&b->root->memory,game,b->root->manager,&ui,8)||
       !m195_read(&b->root->memory,ui,0x50,&current,8)||current!=b->root->manager_class)return 0;
    *out=ui;*cls=current;return 1;
}
static int lid_mip_manager(const M195Ops*o,uint64_t ui,uint32_t offset,
    uint64_t cls,uint64_t vtable,uint64_t method,uint64_t*out){
    uint64_t object,actual,vt,entry;
    if(!out||!m195_read(o,ui,offset,&object,8)||
       !m195_read(o,object,0x50,&actual,8)||actual!=cls||
       !m195_read(o,object,0,&vt,8)||vt!=vtable||
       !m195_read(o,vt,0x2c0,&entry,8)||entry!=method)return 0;
    *out=object;return 1;
}
static int lid_mip_prepare(LidMomokaIconProvider*p,LidMomokaLabels*b,uint64_t image,int verified){
    static const uint8_t part_prefix[24]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0x98,0x48,0x81,0xec,0x40,0x01,0,0,0x48};
    static const uint8_t skill_prefix[24]={0x48,0x8b,0xc4,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x50,0x48,0xc7,0x40,0xa8,0xfe,0xff,0xff,0xff,0x48};
    LidMomokaIconProvider v={0};uint64_t ui,item,enma,ignored;uint8_t bytes[24];
    if(!p||p->ready||!verified||!b||!image||image>UINT64_MAX-0x2005bd8||
       !lid_mip_ui(b,&ui,&v.ui_class))return 0;
    const M195Ops*o=&b->root->memory;
    if(!lid_ml_field(o,ui,"BrgUIManagerBase","mItemIconManager","ObjectProperty",8,&v.item)||
       !lid_ml_field(o,ui,"BrgUIManagerBase","mEnmaMenuImageManager","ObjectProperty",8,&v.enma)||
       !m195_read(o,ui,v.item,&item,8)||!m195_read(o,item,0x50,&v.item_class,8)||
       !m195_named(o,v.item_class,"BrgUIResource_ItemIconManager")||
       !m195_read(o,ui,v.enma,&enma,8)||!m195_read(o,enma,0x50,&v.enma_class,8)||
       !m195_named(o,v.enma_class,"BrgUIResource_EnmaMenuImageManager")||
       !lid_mip_manager(o,ui,v.item,v.item_class,image+0x2005910,image+0xc37bd0,&ignored)||
       !lid_mip_manager(o,ui,v.enma,v.enma_class,image+0x2004f50,image+0xc41f60,&ignored)||
       !m195_read(o,image,0xc37bd0,bytes,24))return 0;
    for(unsigned i=0;i<24;i++)if(bytes[i]!=part_prefix[i])return 0;
    if(!m195_read(o,image,0xc41f60,bytes,24))return 0;
    for(unsigned i=0;i<24;i++)if(bytes[i]!=skill_prefix[i])return 0;
    v.labels=b;v.image=image;v.ready=1;*p=v;return 1;
}
/* Only the immutable compiler-pinned R255 display key is accepted. Foreign
   numeric-ID collisions cannot choose a native LID icon. Representative first
   tier/head icon is intentional, not inference of a remote progressive tier. */
static int lid_mip_get(LidMomokaIconProvider*p,uint64_t location,uint64_t*out){
    uint64_t ui,cls,manager,again,after_cls;uint16_t key[96];unsigned units=0;
    if(!p||!p->ready||!out)return LID_MIP_REFUSED;*out=0;
    const uint8_t*row=lid_sd_row(p->labels->display,location);
    if(!row||r15_r32(row+28)!=1||!lid_mip_ui(p->labels,&ui,&cls)||cls!=p->ui_class)return LID_MIP_REFUSED;
    unsigned kind=r15_r32(row+24);
    if(!kind)return LID_MIP_NEEDS_AP;
    if(kind>3)return LID_MIP_REFUSED;
    for(;units<96&&row[64+units];units++){
        if(row[64+units]<0x21||row[64+units]>0x7e)return LID_MIP_REFUSED;
        key[units]=row[64+units];
    }
    if(!units||units==96)return LID_MIP_REFUSED;key[units]=0;
    M197StringHeader input={(uint64_t)(uintptr_t)key,(int32_t)units+1,(int32_t)units+1};
    const M195Ops*o=&p->labels->root->memory;uint64_t result;
    if(kind==2){
        if(!lid_mip_manager(o,ui,p->enma,p->enma_class,p->image+0x2004f50,p->image+0xc41f60,&manager))return LID_MIP_REFUSED;
        result=((LidMomokaSkillImage)(uintptr_t)(p->image+0xc41f60))((void*)(uintptr_t)manager,&input,2);
    }else{
        if(!lid_mip_manager(o,ui,p->item,p->item_class,p->image+0x2005910,p->image+0xc37bd0,&manager))return LID_MIP_REFUSED;
        result=((LidMomokaPartImage)(uintptr_t)(p->image+0xc37bd0))((void*)(uintptr_t)manager,&input,0,0,0);
    }
    /* Native methods may allocate/request resources. Recheck current UI after
       return, and require exact native Stream type/vtable before exposing it.
       Never free/unroot a borrowed manager-owned image on a refusal. */
    uint64_t vt,image_cls;
    if(!result||!lid_mip_ui(p->labels,&again,&after_cls)||again!=ui||after_cls!=cls||
       !m195_read(o,result,0,&vt,8)||vt!=p->image+0x1ff1e70||
       !m195_read(o,result,0x50,&image_cls,8)||!m195_named(o,image_cls,"BrgUIImage_Stream"))return LID_MIP_REFUSED;
    *out=result;return LID_MIP_NATIVE;
}
#endif
