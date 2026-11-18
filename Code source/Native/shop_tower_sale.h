#ifndef LID_SHOP_TOWER_SALE_H
#define LID_SHOP_TOWER_SALE_H
#include "shop_tower_floor.h"
#include "shop_tower_payment.h"
#include "shop_tower_sale_profiles.h"
/* Native models/repository copies, NEVER off-thread UObject traversal. All
   borrowed identities scoped to one owned native request; native allocations
   use resident scratch, normal original cleanup, stop/retain on exception. */
/* Native purchase uses the current master-floor shop ID. The cached model
   need not own the response builder's active-shop object at +5A0. */
typedef struct LidTowerSaleKey {char room[64];unsigned shop;uintptr_t model;} LidTowerSaleKey;
_Static_assert(sizeof(LidTowerSaleKey)==sizeof(LidTFKey),"Private owner layout retained");
typedef struct LidTowerSale {
    unsigned prepared,active,working,stopped;const LidTowerFloor*floor;M195Ops memory;
    void*context;int(*lease)(void*);LidTowerSaleKey key;uint8_t checked[184];
    uintptr_t(*current)(const wchar_t*,uint64_t,unsigned char);
    void*(*item_get)(void*,const wchar_t*);void(*item_clear)(void*);
    LidTSArray*(*display_get)(uintptr_t,LidTSArray*,const wchar_t*,uint64_t,uint32_t);
    uintptr_t(*product_get)(const wchar_t*);
    LidTSArray*(*items_get)(uintptr_t,LidTSArray*,uint64_t,uint64_t);
    void(*display_clear)(LidTSArray*);void(*items_clear)(LidTSArray*);
    _Alignas(8) uint8_t item[0xec];LidTSArray display,items;
} LidTowerSale;
static int lid_tsa_prepare(LidTowerSale*s,const LidTowerFloor*f,const M195Ops*m,int exact_sha,
                           void*context,int(*lease)(void*)){
    if(!s||s->prepared||!f||!f->prepared||!m||!lease||exact_sha!=1)return 0;
    uint8_t actual[24];for(unsigned i=0;i<8;i++){
        if(!m195_read(m,f->image,lid_tsa_rvas[i],actual,24))return 0;
        for(unsigned j=0;j<24;j++)if(actual[j]!=lid_tsa_profiles[i][j])return 0;
    }
    s->floor=f;s->memory=*m;s->context=context;s->lease=lease;
    s->current=(__typeof__(s->current))(f->image+lid_tsa_rvas[0]);s->item_get=(__typeof__(s->item_get))(f->image+lid_tsa_rvas[1]);
    s->item_clear=(__typeof__(s->item_clear))(f->image+lid_tsa_rvas[2]);s->display_get=(__typeof__(s->display_get))(f->image+lid_tsa_rvas[3]);
    s->product_get=(__typeof__(s->product_get))(f->image+lid_tsa_rvas[4]);s->items_get=(__typeof__(s->items_get))(f->image+lid_tsa_rvas[5]);
    s->display_clear=(__typeof__(s->display_clear))(f->image+lid_tsa_rvas[6]);s->items_clear=(__typeof__(s->items_clear))(f->image+lid_tsa_rvas[7]);
    s->prepared=1;return 1;
}
static int lid_tsa_key(LidTowerSale*s,LidTowerSaleKey*out){
    LidTowerSaleKey key={0};char floor[32],area[32],room[64],model_floor[32],model_area[32];unsigned i=0,j=0;
    if(!out||!s||!s->prepared||s->stopped||s->lease(s->context)!=1){r299_note(15,20,0);return 0;}
    if(!lid_tuc_literal(&s->memory,s->floor->image+0xf8b5194,L"4HMA")){r299_note(15,21,0);return 0;}
    uint8_t valid;int32_t master;uintptr_t model=s->current(0,0,1);
    if(!lid_tf_read(s->floor,model,0x518,&valid,1)||!valid||
       !lid_tf_read(s->floor,model,0x460,&master,4)||master<1||master>1000||
       !lid_tf_id(s->floor,model,8,model_floor)||!lid_tf_id(s->floor,model,0x18,model_area)){
        r299_note(15,22,0);return 0;
    }
    while(model_floor[i]){key.room[i]=model_floor[i];i++;}key.room[i++]='|';
    while(model_area[j])key.room[i++]=model_area[j++];key.room[i]=0;
    key.shop=(unsigned)master;key.model=model;i=0;j=0;
    if(!lid_tuc_identifier(&s->memory,s->floor->image+0xf8b5164,floor)||
       !lid_tuc_identifier(&s->memory,s->floor->image+0xf8b5174,area)){r299_note(15,23,0);return 0;}
    while(floor[i]){room[i]=floor[i];i++;}room[i++]='|';while(area[j])room[i++]=area[j++];room[i]=0;
    if(!lid_ts_room(room,key.room)){r299_note(15,24,0);return 0;}*out=key;r299_note(15,25,key.shop);return 1;
}
static int lid_tsa_begin(LidTowerSale*s){
    if(!s||s->active||s->working||!lid_tsa_key(s,&s->key))return 0;
    __builtin_memset(s->checked,0,sizeof(s->checked));s->active=1;return 1;
}
static int lid_tsa_same(LidTowerSale*s){
    LidTowerSaleKey key;
    return s&&s->active&&lid_tsa_key(s,&key)&&key.shop==s->key.shop&&key.model==s->key.model&&
        lid_ts_room(key.room,s->key.room);
}
/* Room-scoped canonical join only, no DB allocation per selection/UI tick. */
static int lid_tsa_offer(void*context,const uint16_t*item,unsigned units,unsigned*out){
    LidTowerSale*s=context;unsigned found=184;
    if(!s||!s->active||s->stopped||!out||!item||!units||units>=96||item[units]||s->lease(s->context)!=1)return -1;
    for(unsigned i=0;i<184;i++){
        const LidTowerStockData*d=&lid_tower_stock_data[i];
        if(d->floor>s->floor->goal||d->shopid!=s->key.shop||!lid_ts_room(d->room,s->key.room)||!lid_tp_item(item,units,d->item))continue;
        if(found!=184)return -1;found=i;
    }
    if(found==184)return 0;*out=found;return 1;
}
static int lid_tsa_array(LidTSArray*a){
    return a->count>=0&&a->count<=4096&&a->capacity>=a->count&&a->capacity<=4096&&
        (!a->capacity||a->data)&&(uintptr_t)a->data<=UINTPTR_MAX-(size_t)a->capacity*44;
}
/* Real master display -> product -> item membership and current native price.
   Per-request proof cached once PER offer, never across requests/scene loads. */
static int lid_tsa_validate(LidTowerSale*s,unsigned offer){
    if(!s||offer>=184||s->working||!lid_tsa_same(s)){r299_note(15,30,offer);return 0;}
    const LidTowerStockData*d=&lid_tower_stock_data[offer];
    if(d->floor>s->floor->goal||d->shopid!=s->key.shop||!lid_ts_room(d->room,s->key.room)){r299_note(15,31,offer);return 0;}
    if(s->checked[offer]){r299_note(15,47,offer);return 1;}
    volatile unsigned normal=0;int ok=0;s->working=1;
    __try{
        wchar_t query[24]=L"shopid=";unsigned n=7,value=d->shopid;wchar_t reverse[12];unsigned digits=0;
        do{reverse[digits++]=(wchar_t)('0'+value%10);value/=10;}while(value&&digits<12);
        while(digits)query[n++]=reverse[--digits];query[n]=0;
        if(s->display_get(s->floor->image+0xf8ac9f0,&s->display,query,0,0)!=&s->display||!lid_tsa_array(&s->display)){r299_note(15,32,offer);goto clean;}
        unsigned matches=0;
        for(int i=0;i<s->display.count;i++){
            uintptr_t row=(uintptr_t)s->display.data+(unsigned)i*44;
            if(lid_tuc_literal(&s->memory,row+0x10,d->product))matches++;
        }
        if(matches!=1){r299_note(15,33,matches);goto clean;}
        uintptr_t repository=s->product_get(d->product),vt,target;
        if(repository!=s->floor->image+0xf8ac87c||!m195_read(&s->memory,repository,0,&vt,8)||vt!=s->floor->image+0x21470f0||
           !m195_read(&s->memory,vt,0x10,&target,8)||target!=s->floor->image+0x123f050||
           s->items_get(repository,&s->items,0,0)!=&s->items||!lid_tsa_array(&s->items)){r299_note(15,34,offer);goto clean;}
        matches=0;
        for(int i=0;i<s->items.count;i++){
            uintptr_t row=(uintptr_t)s->items.data+(unsigned)i*44;
            if(lid_tuc_literal(&s->memory,row+8,d->product)&&lid_tuc_literal(&s->memory,row+0x18,d->item))matches++;
        }
        if(matches!=1){r299_note(15,36,matches);goto clean;}
        if(s->item_get(s->item,d->item)!=s->item){r299_note(15,37,offer);goto clean;}
        uintptr_t item_vt;int32_t fields[5];
        __builtin_memcpy(&item_vt,s->item,8);
        const unsigned offsets[5]={0x58,0x60,0x68,0x70,0x78};
        for(unsigned i=0;i<5;i++)__builtin_memcpy(&fields[i],s->item+offsets[i],4);
        if(item_vt!=s->floor->image+0x2111280){r299_note(15,38,offer);goto clean;}
        if(s->item[0xe8]!=1){r299_note(15,39,s->item[0xe8]);goto clean;}
        if(!lid_tuc_literal(&s->memory,(uintptr_t)s->item+8,d->item)){r299_note(15,40,offer);goto clean;}
        if(fields[0]!=(int32_t)d->kc){r299_note(15,41,(unsigned)fields[0]);goto clean;}
        if(fields[1]){r299_note(15,42,(unsigned)fields[1]);goto clean;}
        if(fields[2]){r299_note(15,43,(unsigned)fields[2]);goto clean;}
        /* R325: buy_medal (+70) is metadata for other shop contexts.
           Exact native tower helper12DC220 spends KC(+58/+60) and
           spirit(+68/+78); it never reads +70. Keep its actual cost guards. */
        if(fields[4]){r299_note(15,45,(unsigned)fields[4]);goto clean;}
        if(!lid_tsa_same(s)){r299_note(15,46,offer);goto clean;}
        r299_note(15,48,offer);ok=1;
clean:
        /* Only normal-path native cleanup. An exception retains resident
           opaque scratch, marks stopped and propagates without retry. */
        s->item_clear(s->item+8);s->items_clear(&s->items);s->display_clear(&s->display);
        if(ok&&lid_tsa_same(s))s->checked[offer]=1;else {if(ok)r299_note(15,49,offer);ok=0;}
        normal=1;
    }__finally{if(!normal)s->stopped=1;s->working=0;}
    return ok;
}
static void lid_tsa_end(LidTowerSale*s){
    if(!s)return;s->active=0;__builtin_memset(&s->key,0,sizeof(s->key));__builtin_memset(s->checked,0,sizeof(s->checked));
    /* DO NOT erase/free retained native scratch if stopped after exception. */
}
#endif
