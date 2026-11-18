#ifndef LID_SHOP_MOMOKA_CURRENT_ROOT_H
#define LID_SHOP_MOMOKA_CURRENT_ROOT_H
#include "momoka_prepare_observer.h"
/* ZERO-initialize at final resident address, UI-owner-only; do not reconfigure
   after publication. current_game supplies a live root within the existing host
   lifetime window, never a cached UObject. Cache CLASSES/OFFSETS, not instances.
   No engine calls, allocation, file IO, writes to observed memory or GC lock. */
typedef struct LidShopMomokaRoot {
    M195Ops memory;void*context;
    int(*current_game)(void*,uint64_t*);
    uint64_t game_class,manager_class,menu_class;
    uint32_t database,manager,menu,ended,end_mask;
} LidShopMomokaRoot;
static int lid_mr_field(const M195Ops*o,uint64_t object,const char*owner,
    const char*name,const char*type,uint32_t size,uint32_t*offset,uint32_t*mask){
    uint64_t property,cls;uint32_t dim,actual,at,m=0;
    if(!offset||!m212_member(o,object,owner,name,&property)||
       !m195_read(o,property,0x50,&cls,8)||!m195_named(o,cls,type)||
       !m195_read(o,property,0x68,&dim,4)||dim!=1||
       !m195_read(o,property,0x6c,&actual,4)||actual!=size||
       !m195_read(o,property,0x94,&at,4)||at<0x58||at>0x10000-size)return 0;
    /* 64K is a rejection ceiling, NOT an inferred instance size/offset. */
    if(mask&&(!m195_read(o,property,0xb0,&m,4)||!m||(m&(m-1))))return 0;
    *offset=at;if(mask)*mask=m;return 1;
}
static int lid_mr_current(LidShopMomokaRoot*r,uint64_t menu_class,
    uint64_t database_class,uint64_t*database,uint64_t*menu){
    uint64_t game,cls,manager,db,current;uint32_t ended;
    if(!r||!database||!menu||!menu_class||!database_class||!r->current_game||
       !r->current_game(r->context,&game)||!game||
       !m195_read(&r->memory,game,0x50,&cls,8)||!cls)return 0;
    if(cls!=r->game_class){
        uint32_t db_offset,ui_offset;
        if(!lid_mr_field(&r->memory,game,"BrgGameInfoNative","mDatabase","ObjectProperty",8,&db_offset,0)||
           !lid_mr_field(&r->memory,game,"BrgGameInfoNative","mUIManager","ObjectProperty",8,&ui_offset,0))return 0;
        r->database=db_offset;r->manager=ui_offset;r->game_class=cls;r->manager_class=0;
    }
    if(!m195_read(&r->memory,game,r->database,&db,8)||!db||
       !m195_read(&r->memory,db,0x50,&cls,8)||cls!=database_class||
       !m195_read(&r->memory,game,r->manager,&manager,8)||!manager||
       !m195_read(&r->memory,manager,0x50,&cls,8)||!cls)return 0;
    if(cls!=r->manager_class){
        uint32_t offset;
        if(!lid_mr_field(&r->memory,manager,"BrgUIManager","mSkillExchange","ObjectProperty",8,&offset,0))return 0;
        r->menu=offset;r->manager_class=cls;
    }
    if(!m195_read(&r->memory,manager,r->menu,&current,8)||!current||
       !m195_read(&r->memory,current,0x50,&cls,8)||cls!=menu_class)return 0;
    if(cls!=r->menu_class){
        uint32_t offset,mask;
        if(!lid_mr_field(&r->memory,current,"BrgUIMenu_SkillExchange","mEnd","BoolProperty",4,&offset,&mask))return 0;
        r->ended=offset;r->end_mask=mask;r->menu_class=cls;
    }
    if(!m195_read(&r->memory,current,r->ended,&ended,4)||(ended&r->end_mask))return 0;
    *database=db;*menu=current;return 1;
}
#endif
