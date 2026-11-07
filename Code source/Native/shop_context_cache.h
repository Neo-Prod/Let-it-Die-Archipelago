#ifndef LID_SHOP_CONTEXT_CACHE_H
#define LID_SHOP_CONTEXT_CACHE_H
/* Include shop_context.h first. Main/game-thread-only metadata cache.
   NO live manager/shop/panel instance is retained. Native class identities
   invalidate their offsets; every access follows the current object chain.
   No name walk at all on a warm successful lookup. */
typedef struct LidShopPropertyCache {
    void* game_class;void* manager_class;void* shop_class;void* panel_class;
    int game_ui,manager_shop,buy_only,ended,panel,history;
    unsigned buy_mask,end_mask,history_ready;
} LidShopPropertyCache;
typedef struct LidShopCachedView {void* shop;void* panel;} LidShopCachedView;
static void* lid_shop_cache_class(void* obj,const LidEventOps* ops){
    return obj&&ops&&ops->readable&&ops->readable(obj,0x58)?*(void**)((BYTE*)obj+0x50):0;
}
static int lid_shop_cache_field(void* obj,const char* name,const char* type,int size,int bound,
                                const LidEventOps* ops,int* offset,unsigned* mask){
    BYTE* data=lid_shop_named_value(obj,name,type,size,bound,ops);BYTE* field;unsigned m;
    if(!data)return 0;
    if(mask){
        field=lid_event_field(obj,name,type,ops);
        if(!field||!ops->readable(field,0xb4))return 0;
        m=*(unsigned*)(field+0xb0);if(!m||(m&(m-1)))return 0;*mask=m;
    }
    *offset=(int)(data-(BYTE*)obj);return 1;
}
static void* lid_shop_cache_pointer(void* obj,int offset,const LidEventOps* ops){
    if(!obj||offset<0x58||!ops->readable((BYTE*)obj+offset,8))return 0;
    return *(void**)((BYTE*)obj+offset);
}
static int lid_shop_cache_view(LidShopPropertyCache* c,void* game,const LidEventOps* ops,LidShopCachedView* out){
    void* cls;void* manager;void* shop;void* panel;int off,buy,end,panel_off;unsigned bm,em;
    if(!out)return 0;
    out->shop=0;out->panel=0;
    if(!c||!(cls=lid_shop_cache_class(game,ops)))return 0;
    if(c->game_class!=cls){
        if(!lid_shop_cache_field(game,"mUIManager","ObjectProperty",8,0x1430,ops,&off,0))return 0;
        c->game_ui=off;c->game_class=cls;c->manager_class=0;c->shop_class=0;c->panel_class=0;c->history_ready=0;
    }
    manager=lid_shop_cache_pointer(game,c->game_ui,ops);
    if(!(cls=lid_shop_cache_class(manager,ops)))return 0;
    if(c->manager_class!=cls){
        if(!lid_event_class_is(manager,"BrgUIManager",ops)||
           !lid_shop_cache_field(manager,"mShop","ObjectProperty",8,0x30c0,ops,&off,0))return 0;
        c->manager_shop=off;c->manager_class=cls;c->shop_class=0;c->panel_class=0;c->history_ready=0;
    }
    shop=lid_shop_cache_pointer(manager,c->manager_shop,ops);
    if(!(cls=lid_shop_cache_class(shop,ops)))return 0;
    if(c->shop_class!=cls){
        if(!lid_event_class_is(shop,"BrgUIMenu_PartShop",ops)||
           !lid_shop_cache_field(shop,"mBuyOnly","BoolProperty",4,0x37c,ops,&buy,&bm)||
           !lid_shop_cache_field(shop,"mEnd","BoolProperty",4,0x37c,ops,&end,&em)||
           !lid_shop_cache_field(shop,"mItemSelect","ObjectProperty",8,0x37c,ops,&panel_off,0))return 0;
        c->buy_only=buy;c->ended=end;c->panel=panel_off;c->buy_mask=bm;c->end_mask=em;
        c->shop_class=cls;c->panel_class=0;c->history_ready=0;
    }
    if(!ops->readable((BYTE*)shop+c->buy_only,4)||!ops->readable((BYTE*)shop+c->ended,4)||
       (*(unsigned*)((BYTE*)shop+c->buy_only)&c->buy_mask)||(*(unsigned*)((BYTE*)shop+c->ended)&c->end_mask))return 0;
    panel=lid_shop_cache_pointer(shop,c->panel,ops);
    if(!(cls=lid_shop_cache_class(panel,ops)))return 0;
    if(c->panel_class!=cls){
        if(!lid_event_class_is(panel,"BrgUIMenuPart_ItemSelect",ops))return 0;
        c->panel_class=cls;
    }
    out->shop=shop;out->panel=panel;return 1;
}
static BYTE* lid_shop_cache_history(LidShopPropertyCache* c,void* shop,const LidEventOps* ops){
    int off;
    if(!c||!shop||lid_shop_cache_class(shop,ops)!=c->shop_class)return 0;
    if(!c->history_ready){
        if(!lid_shop_cache_field(shop,"mDbUserShopHistorys","ArrayProperty",16,0x37c,ops,&off,0))return 0;
        c->history=off;c->history_ready=1;
    }
    return ops->readable((BYTE*)shop+c->history,16)?(BYTE*)shop+c->history:0;
}
#endif
