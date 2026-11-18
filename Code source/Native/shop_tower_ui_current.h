#ifndef LID_SHOP_TOWER_UI_CURRENT_H
#define LID_SHOP_TOWER_UI_CURRENT_H
#include "shop_tower_ui_meta.h"
#include "shop_tower_stock_view.h"
#include "shop_runtime_catalogue.h"
/* UI-owner synchronous READ ONLY resolver. Own C metadata/output only. No
   engine calls, slots/results/history/inventory/wallet writes or reservations.
   Cache CLASSES/OFFSETS only; instances/arrays are freshly resolved each call. */
enum {LID_TUC_CANCEL=0,LID_TUC_COUNT=1};
typedef struct LidTowerUiCurrentOps {
    void*game_context;int(*game)(void*,uint64_t*);
    void*live_context;int(*live)(void*);
} LidTowerUiCurrentOps;
typedef struct LidTowerUiCurrent {
    const LidTowerUiMeta*meta;const LidShopRuntimeCatalogue*catalogue;LidTowerUiCurrentOps ops;
    uint64_t game_class,manager_class,stage_class,product_struct,sister_class;
    uint32_t manager,stage,shop,window,flags,floor,floor_id,floor_area,floor_shops,shop_products;
    uint32_t products,indices;unsigned prepared;
} LidTowerUiCurrent;
/* All instance/array addresses below are COMPARISON ONLY outside this exact
   VM action; never dereference a previously returned address in a later call. */
typedef struct LidTowerUiSnapshot {
    uint64_t menu,item_select;LidTSArray products,indices;
    unsigned offer,logical,original,history_count,flag;int32_t price;
    uint64_t location;const LidTowerStockData*carrier;
} LidTowerUiSnapshot;
typedef struct LidTowerUiRoots {uint64_t game,manager,stage,shop,window,item_select;} LidTowerUiRoots;
static int lid_tuc_object(const M195Ops*o,uint64_t object,const char*owner,const char*name,
                          uint64_t expected,uint32_t*out){
    uint64_t p,actual;
    return m212_member(o,object,owner,name,&p)&&lid_ml_property(o,p,"ObjectProperty",1,8,65536,0x58,out)&&
        m195_read(o,p,0xb0,&actual,8)&&(!expected||actual==expected);
}
static int lid_tuc_array(const M195Ops*o,uint64_t at,unsigned stride,unsigned maximum,LidTSArray*out){
    LidTSArray a;
    if(!out||!stride||!m195_read(o,at,0,&a,16)||a.count<0||(unsigned)a.count>maximum||
       a.capacity<a.count||a.capacity>4096||(a.capacity&&!a.data)||(!a.capacity&&a.data)||
       (uint64_t)(uintptr_t)a.data>UINT64_MAX-(uint64_t)a.capacity*stride)return 0;
    *out=a;return 1;
}
static int lid_tuc_literal(const M195Ops*o,uint64_t at,const wchar_t*expected){
    LidTSString s;uint16_t value[96];unsigned count=0;
    if(!expected)return 0;while(count<95&&expected[count])count++;
    if(count==95||!m195_read(o,at,0,&s,16)||s.count!=(int32_t)count+1||s.capacity<s.count||s.capacity>4096||
       !m195_read(o,(uint64_t)(uintptr_t)s.data,0,value,2*(count+1)))return 0;
    for(unsigned i=0;i<=count;i++)if(value[i]!=(uint16_t)expected[i])return 0;return 1;
}
static int lid_tuc_identifier(const M195Ops*o,uint64_t at,char out[32]){
    LidTSString s;uint16_t text[32];
    if(!out||!m195_read(o,at,0,&s,16)||s.count<2||s.count>32||s.capacity<s.count||s.capacity>4096||
       !m195_read(o,(uint64_t)(uintptr_t)s.data,0,text,(unsigned)s.count*2)||text[s.count-1])return 0;
    for(int i=0;i<s.count-1;i++){
        unsigned c=text[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return 0;out[i]=(char)c;
    }out[s.count-1]=0;return 1;
}
static int lid_tuc_bytes_inner(const M195Ops*o,uint64_t object,const char*owner,const char*name,
                              const char*inner_type,unsigned stride,uint32_t*out){
    uint64_t p,inner;uint32_t at;
    return m212_member(o,object,owner,name,&p)&&lid_ml_property(o,p,"ArrayProperty",1,16,65536,0x58,out)&&
        m195_read(o,p,0xb0,&inner,8)&&lid_ml_property(o,inner,inner_type,1,stride,stride,0,&at)&&!at;
}
/* Cold once, no SisterPartShop instance needed. 0 current root unavailable,
   -1 malformed reflected schema, 1 prepared. Outputs never contain instances. */
static int lid_tuc_prepare(LidTowerUiCurrent*out,const LidTowerUiMeta*meta,
                           const LidShopRuntimeCatalogue*catalogue,const LidTowerUiCurrentOps*ops){
    if(!out||out->prepared||!meta||!meta->ready||!catalogue||!__atomic_load_n(&catalogue->initialized,__ATOMIC_ACQUIRE)||
       !ops||!ops->game||!ops->live)return -1;
    LidTowerUiCurrent b={0};b.meta=meta;b.catalogue=catalogue;b.ops=*ops;const M195Ops*o=&meta->memory;
    uint64_t game,manager,stage,p,structure,inner,shop_struct,product,cls;uint32_t size,at;
    if(!ops->game(ops->game_context,&game)||!game)return 0;
    if(!m195_read(o,game,0x50,&b.game_class,8)||
       !lid_tuc_object(o,game,"BrgGameInfoNative","mUIManager",0,&b.manager)||
       !lid_tuc_object(o,game,"BrgGameInfoNative","mStageManager",0,&b.stage)||
       !lid_tuc_bytes_inner(o,game,"BrgGameInfoNative","mFloorShopBuyProductFlags","ByteProperty",1,&b.flags))return -1;
    if(!m195_read(o,game,b.manager,&manager,8)||!m195_read(o,game,b.stage,&stage,8))return -1;
    if(!manager||!stage)return 0;
    if(!m195_read(o,manager,0x50,&b.manager_class,8)||!m195_read(o,stage,0x50,&b.stage_class,8)||
       !lid_tum_class(o,b.stage_class,"BrgStageManager")||
       !lid_tuc_object(o,manager,"BrgUIManager","mShop",meta->shop_class,&b.shop)||
       !lid_tuc_object(o,manager,"BrgUIManager","mSystemWindow",meta->window_class,&b.window)||
       !m212_member(o,stage,"BrgStageManager","mCurrentFloor",&p)||
       !lid_ml_property(o,p,"StructProperty",1,0x234,65536,0x58,&b.floor)||b.floor!=0x60||
       !m195_read(o,p,0xb0,&structure,8)||!m195_named(o,structure,"BrgDbFloor")||
       !m195_read(o,structure,0x50,&cls,8)||!m195_named(o,cls,"ScriptStruct")||
       !m195_read(o,structure,0x88,&size,4)||size!=0x234)return -1;
    if(!lid_ml_struct_member(o,structure,"mFlrid",&p)||!lid_ml_property(o,p,"StrProperty",1,16,size,0,&b.floor_id)||b.floor_id!=0x20||
       !lid_ml_struct_member(o,structure,"mAreaid",&p)||!lid_ml_property(o,p,"StrProperty",1,16,size,0,&b.floor_area)||b.floor_area!=0x34||
       !lid_ml_struct_member(o,structure,"mShps",&p)||!lid_ml_property(o,p,"ArrayProperty",1,16,size,0,&b.floor_shops)||b.floor_shops!=0x11c||
       !m195_read(o,p,0xb0,&inner,8)||!m195_read(o,inner,0xb0,&shop_struct,8)||!m195_named(o,shop_struct,"BrgDbShop")||
       !m195_read(o,shop_struct,0x88,&size,4)||size!=32||!lid_ml_property(o,inner,"StructProperty",1,32,32,0,&at)||at||
       !lid_ml_struct_member(o,shop_struct,"mPrds",&p)||!lid_ml_property(o,p,"ArrayProperty",1,16,32,0,&b.shop_products)||b.shop_products!=16||
       !m195_read(o,p,0xb0,&inner,8)||!m195_read(o,inner,0xb0,&product,8)||!m195_named(o,product,"BrgDbProduct")||
       !m195_read(o,product,0x88,&size,4)||size!=108||!lid_ml_property(o,inner,"StructProperty",1,108,108,0,&at)||at)return -1;
    const char*fields[3]={"Mid","mType","mItemId"};const unsigned expected[3]={0,0x3c,0x5c};
    for(unsigned i=0;i<3;i++)if(!lid_ml_struct_member(o,product,fields[i],&p)||
        !lid_ml_property(o,p,"StrProperty",1,16,108,0,&at)||at!=expected[i])return -1;
    b.product_struct=product;b.prepared=1;*out=b;return 1;
}
static int lid_tuc_roots(LidTowerUiCurrent*c,LidTowerUiRoots*out){
    LidTowerUiRoots r={0};uint64_t cls,vt;const M195Ops*o=&c->meta->memory;
    if(!c->ops.game(c->ops.game_context,&r.game)||!m195_read(o,r.game,0x50,&cls,8)||cls!=c->game_class||
       !m195_read(o,r.game,c->manager,&r.manager,8)||!m195_read(o,r.manager,0x50,&cls,8)||cls!=c->manager_class||
       !m195_read(o,r.game,c->stage,&r.stage,8)||!m195_read(o,r.stage,0x50,&cls,8)||cls!=c->stage_class||
       !m195_read(o,r.stage,0,&vt,8)||vt!=c->meta->image+0x21c7820||
       !m195_read(o,r.manager,c->shop,&r.shop,8)||!r.shop||!m195_read(o,r.manager,c->window,&r.window,8)||
       !m195_read(o,r.window,0x50,&cls,8)||cls!=c->meta->window_class||
       !m195_read(o,r.shop,c->meta->offsets[LID_TUM_ITEMSELECT],&r.item_select,8)||
       !m195_read(o,r.item_select,0x50,&cls,8)||cls!=c->meta->item_class)return 0;
    *out=r;return 1;
}
static int lid_tuc_sister(LidTowerUiCurrent*c,uint64_t cls){
    uint64_t parent,structure,p,inner;uint32_t extent,at;
    const M195Ops*o=&c->meta->memory;
    if(!m195_named(o,cls,"BrgUIMenu_SisterPartShop")||!lid_tum_owner(o,cls,"BrgUIMenu_ShopBase",&parent)||parent!=c->meta->shop_class)return 0;
    if(c->sister_class)return c->sister_class==cls;
    if(!m195_read(o,cls,0x88,&extent,4)||extent<0x250||extent>65536||
       !lid_tum_array(o,cls,"mDbProducts",extent,"BrgDbProduct",108,&c->products,&structure)||structure!=c->product_struct||c->products<0x250||
       !lid_mle_member(o,cls,"mDbProductIndices",&p)||!lid_ml_property(o,p,"ArrayProperty",1,16,extent,0x250,&c->indices)||
       !m195_read(o,p,0xb0,&inner,8)||!lid_ml_property(o,inner,"IntProperty",1,4,4,0,&at)||at||
       (c->products<c->indices+16&&c->indices<c->products+16))return 0;
    c->sister_class=cls;return 1;
}
static int lid_tuc_product(const M195Ops*o,uint64_t at,const LidTowerStockData*d){
    LidTSProduct row;
    return m195_read(o,at,0,&row,108)&&row.kc==(int32_t)d->kc&&!row.spirit&&!row.medal&&!row.parts.count&&
        lid_tuc_literal(o,at,d->product)&&lid_tuc_literal(o,at+0x3c,L"PRDTP_ITEM")&&lid_tuc_literal(o,at+0x5c,d->item);
}
/* 0 unrelated/noneligible panel, -1 malformed KNOWN buy context, 1 snapshot.
   Caller must keep native behavior for0; -1 is NOT permission to buy. Node is
   rejected BEFORE world traversal. All output storage belongs to caller C code. */
static int lid_tuc_collect(LidTowerUiCurrent*c,unsigned kind,uint64_t receiver,uint64_t frame,LidTowerUiSnapshot*out){
    if(!c||!c->prepared||!out||kind>1){r299_note(4,1,0);return 0;}
    const LidTowerUiMeta*m=c->meta;const M195Ops*o=&m->memory;uint64_t node,object,cls;
    if(!m195_read(o,frame,0x14,&node,8)||node!=(kind==LID_TUC_CANCEL?m->tick:m->buy)){r299_note(4,2,0);return 0;}
    if(!m195_read(o,frame,0x1c,&object,8)){r299_note(4,3,1);return 0;}
    if(!m195_read(o,object,0x50,&cls,8)){r299_note(4,3,2);return 0;}
    if(!m195_named(o,cls,"BrgUIMenu_SisterPartShop")){r299_note(4,3,3);return 0;}
    int32_t state,other,yes;uint32_t ended;
    if(!m195_read(o,object,m->offsets[LID_TUM_STATE],&state,4)||!m195_read(o,object,m->offsets[LID_TUM_OTHER],&other,4)||
       !m195_read(o,object,m->offsets[LID_TUM_YESNO],&yes,4)||!m195_read(o,object,m->offsets[LID_TUM_END],&ended,4)){r299_note(4,4,0);return -1;}
    if((int64_t)state-(int64_t)other!=9||yes!=0){r299_note(4,5,0);return 0;}
    if((ended&m->end_mask)||!c->ops.live(c->ops.live_context)){r299_note(4,6,0);return -1;}
    LidTowerUiRoots root,again;
    if(!lid_tuc_roots(c,&root)||root.shop!=object||receiver!=(kind==LID_TUC_CANCEL?root.window:root.item_select)||!lid_tuc_sister(c,cls)){r299_note(4,7,0);return -1;}
    char floor[32],area[32],room[64];unsigned len=0,j=0;
    if(!lid_tuc_identifier(o,root.stage+c->floor+c->floor_id,floor)||!lid_tuc_identifier(o,root.stage+c->floor+c->floor_area,area)){r299_note(4,8,0);return -1;}
    while(floor[len]){room[len]=floor[len];len++;}room[len++]='|';while(area[j])room[len++]=area[j++];room[len]=0;
    unsigned offers[36],count=0,shopid=0;
    for(unsigned i=0;i<184;i++){
        const LidTowerStockData*d=&lid_tower_stock_data[i];if(d->floor>c->catalogue->goal||!lid_ts_room(room,d->room))continue;
        if(count==36||(shopid&&shopid!=d->shopid)){r299_note(4,9,0);return -1;}shopid=d->shopid;offers[count++]=i;
    }
    if(!count){r299_note(4,10,0);return 0;}
    LidTSArray shops,full,products,indices,flags,ids,history,sorted;int32_t selected,raw,logical,pending,price,unit_count;
    if(!lid_tuc_array(o,root.stage+c->floor+c->floor_shops,32,1,&shops)||shops.count!=1||
       !lid_tuc_array(o,(uint64_t)(uintptr_t)shops.data+c->shop_products,108,36,&full)||full.count!=(int32_t)count||
       !lid_tuc_array(o,object+c->products,108,36,&products)||!lid_tuc_array(o,object+c->indices,4,36,&indices)||
       products.count!=indices.count||!m195_read(o,object,m->offsets[LID_TUM_SELECT],&selected,4)||selected<0||selected>=products.count||
       !m195_read(o,root.item_select,0x70,&raw,4)||!lid_tuc_array(o,root.item_select+0x254,4,36,&sorted)||raw<0||raw>=sorted.count||
       !m195_read(o,(uint64_t)(uintptr_t)sorted.data,(unsigned)raw*4,&logical,4)||logical!=selected||
       !m195_read(o,root.item_select,0x24c,&unit_count,4)||unit_count<products.count||unit_count>4096){r299_note(4,11,0);return -1;}
    int32_t original[36];uint8_t seen[36]={0};
    if(!m195_read(o,(uint64_t)(uintptr_t)indices.data,0,original,(unsigned)indices.count*4)){r299_note(4,12,0);return -1;}
    for(int i=0;i<indices.count;i++)if(original[i]<0||(unsigned)original[i]>=count||seen[original[i]]++){r299_note(4,13,0);return -1;}
    unsigned ordinal=(unsigned)original[selected];const LidTowerStockData*d=&lid_tower_stock_data[offers[ordinal]];
    if(!lid_tuc_product(o,(uint64_t)(uintptr_t)products.data+(unsigned)selected*108,d)||
       !lid_tuc_product(o,(uint64_t)(uintptr_t)full.data+ordinal*108,d)||
       !m195_read(o,object,m->offsets[LID_TUM_PRICE],&price,4)||price!=(int32_t)d->kc||
       !m195_read(o,object,m->offsets[LID_TUM_PENDING],&pending,4)||pending||
       !lid_tuc_array(o,object+m->offsets[LID_TUM_IDS],20,4096,&ids)||ids.count||
       !lid_tuc_array(o,root.game+c->flags,1,36,&flags)||flags.count!=(int32_t)count||
       !lid_tuc_array(o,object+m->offsets[LID_TUM_HISTORY],m->history_stride,4096,&history)){r299_note(4,14,0);return -1;}
    uint8_t flag;
    if(!m195_read(o,(uint64_t)(uintptr_t)flags.data,ordinal,&flag,1)||flag!=(kind==LID_TUC_CANCEL?0:1)){r299_note(4,15,0);return -1;}
    if(kind==LID_TUC_COUNT){
        if(history.count<1){r299_note(4,16,0);return -1;}uint64_t last=(uint64_t)(uintptr_t)history.data+(unsigned)(history.count-1)*m->history_stride;
        if(!lid_tuc_literal(o,last+m->history_type,L"buy_item")||!lid_tuc_literal(o,last+m->history_item,d->item)){r299_note(4,17,0);return -1;}
    }
    /* No native/allocating call intervenes. Re-read current roots and headers,
       not a dereference of an old cached instance after a generation change. */
    LidTSArray p2,i2,f2,h2,full2,shops2;uint64_t node2,object2;int32_t selected2,price2,state2,other2,yes2,pending2;uint32_t ended2;uint8_t flag2;
    if(!lid_tuc_roots(c,&again)||again.game!=root.game||again.manager!=root.manager||again.stage!=root.stage||
       again.shop!=root.shop||again.window!=root.window||again.item_select!=root.item_select||
       !m195_read(o,frame,0x14,&node2,8)||node2!=node||!m195_read(o,frame,0x1c,&object2,8)||object2!=object||
       !lid_tuc_array(o,object+c->products,108,36,&p2)||!lid_tuc_array(o,object+c->indices,4,36,&i2)||
       p2.data!=products.data||p2.count!=products.count||p2.capacity!=products.capacity||
       i2.data!=indices.data||i2.count!=indices.count||i2.capacity!=indices.capacity||
       !m195_read(o,object,m->offsets[LID_TUM_SELECT],&selected2,4)||selected2!=selected||
       !m195_read(o,object,m->offsets[LID_TUM_PRICE],&price2,4)||price2!=price||
       !m195_read(o,object,m->offsets[LID_TUM_STATE],&state2,4)||state2!=state||
       !m195_read(o,object,m->offsets[LID_TUM_OTHER],&other2,4)||other2!=other||
       !m195_read(o,object,m->offsets[LID_TUM_YESNO],&yes2,4)||yes2!=yes||
       !m195_read(o,object,m->offsets[LID_TUM_END],&ended2,4)||ended2!=ended||
       !m195_read(o,object,m->offsets[LID_TUM_PENDING],&pending2,4)||pending2||
       !lid_tuc_array(o,root.stage+c->floor+c->floor_shops,32,1,&shops2)||
       shops2.data!=shops.data||shops2.count!=shops.count||shops2.capacity!=shops.capacity||
       !lid_tuc_array(o,(uint64_t)(uintptr_t)shops2.data+c->shop_products,108,36,&full2)||
       full2.data!=full.data||full2.count!=full.count||full2.capacity!=full.capacity||
       !lid_tuc_array(o,root.game+c->flags,1,36,&f2)||f2.data!=flags.data||f2.count!=flags.count||f2.capacity!=flags.capacity||
       !m195_read(o,(uint64_t)(uintptr_t)f2.data,ordinal,&flag2,1)||flag2!=flag||
       !lid_tuc_array(o,object+m->offsets[LID_TUM_HISTORY],m->history_stride,4096,&h2)||
       h2.data!=history.data||h2.count!=history.count||h2.capacity!=history.capacity||!c->ops.live(c->ops.live_context)){r299_note(4,18,0);return -1;}
    *out=(LidTowerUiSnapshot){object,root.item_select,products,indices,d->offer,(unsigned)selected,ordinal,(unsigned)history.count,flag,price,d->location,d};{r299_note(4,19,0);return 1;}
}
#endif
