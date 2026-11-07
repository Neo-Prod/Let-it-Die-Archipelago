#ifndef LID_SHOP_CONTEXT_H
#define LID_SHOP_CONTEXT_H
/* Read-only context guard, not integrated or installed. Caller must be
   on the game thread, with current-world identity and a fresh native hub test.
   Resolves named metadata: NO reliance on hypothetical mShop+29f0.
   Depends on progression_events.h, BYTE and size_t. */
typedef struct LidShopContext {
    void* shop;
    void* item_select;
} LidShopContext;

static BYTE* lid_shop_named_value(void* object,const char* name,const char* type,
                                int size,int object_size,const LidEventOps* ops){
    BYTE* field=(BYTE*)lid_event_field(object,name,type,ops);int offset;
    if(!field||!ops->readable(field,0x98)||*(int*)(field+0x68)!=1||
       *(int*)(field+0x6c)!=size)return 0;
    offset=*(int*)(field+0x94);
    if(offset<0x58||offset>object_size-size||
       !ops->readable((BYTE*)object+offset,(size_t)size))return 0;
    return (BYTE*)object+offset;
}
static int lid_shop_named_bool(void* object,const char* name,int object_size,
                              const LidEventOps* ops,int* value){
    BYTE* field=(BYTE*)lid_event_field(object,name,"BoolProperty",ops);
    BYTE* data;unsigned mask;
    if(!field||!ops->readable(field,0xb4))return 0;
    mask=*(unsigned*)(field+0xb0);
    if(!mask||(mask&(mask-1)))return 0;
    data=lid_shop_named_value(object,name,"BoolProperty",4,object_size,ops);
    if(!data)return 0;
    *value=!!(*(unsigned*)data&mask);return 1;
}
static int lid_shop_context(void* manager,int native_hub,
                            const LidEventOps* ops,LidShopContext* out){
    BYTE* data;void* shop;void* panel;int tower,ended;
    if(!out)return 0;
    out->shop=0;out->item_select=0;
    if(native_hub!=1||!lid_event_class_is(manager,"BrgUIManager",ops))return 0;
    data=lid_shop_named_value(manager,"mShop","ObjectProperty",8,0x30c0,ops);
    if(!data||!(shop=*(void**)data)||!lid_event_class_is(shop,"BrgUIMenu_PartShop",ops)||
       !lid_shop_named_bool(shop,"mBuyOnly",0x37c,ops,&tower)||tower||
       !lid_shop_named_bool(shop,"mEnd",0x37c,ops,&ended)||ended)return 0;
    data=lid_shop_named_value(shop,"mItemSelect","ObjectProperty",8,0x37c,ops);
    if(!data||!(panel=*(void**)data)||!lid_event_class_is(panel,"BrgUIMenuPart_ItemSelect",ops))return 0;
    out->shop=shop;out->item_select=panel;return 1;
}
#endif
