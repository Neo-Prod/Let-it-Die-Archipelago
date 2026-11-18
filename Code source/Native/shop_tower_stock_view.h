#ifndef LID_SHOP_TOWER_STOCK_VIEW_H
#define LID_SHOP_TOWER_STOCK_VIEW_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include "shop_tower_stock_data.h"
/* Disconnected pure source builder. NO native calls, allocations, hooks, Save,
   wallet writes, permission claims or UObject access. The host must validate
   current AP policy/Context/floor and synchronously DEEP-COPY this borrowed
   source using the verified engine copier. NEVER assign these headers/pointers
   directly into a game field, retain a copied view, or engine-destroy this view.
   All rows stay in stable order. Bought/pending AP state is a separate gate. */
enum {LID_TS_MAX=36};
#pragma pack(push,4)
typedef struct LidTSString {const wchar_t*data;int32_t count,capacity;} LidTSString;
typedef struct LidTSArray {const void*data;int32_t count,capacity;} LidTSArray;
typedef struct LidTSProduct {
    LidTSString id,name,description;int32_t kc,spirit,medal;
    LidTSString type;LidTSArray parts;LidTSString item;
} LidTSProduct;
typedef struct LidTSShop {LidTSString type;LidTSArray products;} LidTSShop;
#pragma pack(pop)
typedef struct LidTowerStockView {
    LidTSArray shops;LidTSShop shop;LidTSProduct products[LID_TS_MAX];
    unsigned offers[LID_TS_MAX];unsigned count;
} LidTowerStockView;
_Static_assert(sizeof(void*)==8&&sizeof(wchar_t)==2,"Current Windows x64 only");
_Static_assert(sizeof(LidTSString)==16&&sizeof(LidTSArray)==16,"Native header size");
_Static_assert(sizeof(LidTSProduct)==0x6c&&sizeof(LidTSShop)==0x20,"Native row sizes");
_Static_assert(offsetof(LidTSProduct,type)==0x3c&&offsetof(LidTSProduct,parts)==0x4c&&
    offsetof(LidTSProduct,item)==0x5c&&offsetof(LidTSShop,products)==0x10,"Native fields");
static int lid_ts_room(const char*a,const char*b){
    if(!a||!b)return 0;for(unsigned i=0;i<64;i++){if(a[i]!=b[i])return 0;if(!a[i])return i!=0;}return 0;
}
static int lid_ts_string(LidTSString*out,const wchar_t*text){
    unsigned n=0;if(!out||!text)return 0;while(n<96&&text[n])n++;
    if(!n||n==96)return 0;out->data=text;out->count=out->capacity=(int32_t)n+1;return 1;
}
/* Return1: complete BORROWED source view. Return0: no usable view. Caller must
   NOT use output after refusal. No bought/native-owned filters: original floor
   indices remain stable; host controls unavailable rows and actual payment. */
static int lid_ts_build(unsigned goal,const char*room,unsigned shopid,LidTowerStockView*out){
    if(!out)return 0;
    unsigned char*bytes=(unsigned char*)out;
    for(size_t i=0;i<sizeof(*out);i++)bytes[i]=0;
    if(goal<10||goal>40||goal%10||!room||!shopid)return 0;
    unsigned plans=0;
    for(unsigned i=0;i<184;i++){
        const LidTowerStockData*d=&lid_tower_stock_data[i];
        if(d->shopid!=shopid||d->floor>goal||!lid_ts_room(room,d->room))continue;
        if(out->count>=LID_TS_MAX||d->offer!=i||!d->kc||d->kc>INT32_MAX||d->plan_slot>3)goto refused;
        if(d->plan_slot){unsigned bit=1u<<(d->plan_slot-1);if(plans&bit)goto refused;plans|=bit;}
        LidTSProduct*p=&out->products[out->count];
        if(!lid_ts_string(&p->id,d->product)||!lid_ts_string(&p->item,d->item)||
           !lid_ts_string(&p->type,L"PRDTP_ITEM"))goto refused;
        p->kc=(int32_t)d->kc;out->offers[out->count++]=d->offer;
    }
    if(!out->count||plans!=7)goto refused;
    out->shop.products.data=out->products;out->shop.products.count=out->shop.products.capacity=(int32_t)out->count;
    out->shops.data=&out->shop;out->shops.count=out->shops.capacity=1;return 1;
refused:
    for(size_t i=0;i<sizeof(*out);i++)bytes[i]=0;return 0;
}
#endif
