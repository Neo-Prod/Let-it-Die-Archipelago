#ifndef LID_SHOP_TOWER_REQUEST_H
#define LID_SHOP_TOWER_REQUEST_H
#include "shop_marker_native.h"
#include "shop_tower_stock_data.h"
#include "shop_tower_event_probe.h"
#if !LID_SHOP_EXEC_SEH_ENABLED
#error "Tower JSON string ownership requires compiler finally"
#endif
/* Request worker synchronous scope only; mandatory access.window supplies the
   actual borrowed handler-root lifetime. No JSON writes, item grant, receipt,
   reservation or cached UObject/JSON/string address. Own copied values only. */
typedef struct LidTowerRequestRow {unsigned buy,ap,offer,units;uint16_t item[96];} LidTowerRequestRow;
typedef struct LidTowerRequest {unsigned ready,count,ap_count;LidTowerRequestRow rows[128];} LidTowerRequest;
typedef struct LidTowerRequestOps {
    void*context;
    /* Current native room/shop/master/price join, NOT a global itemID match:
       0 genuine native/noneligible,1 unique AP offer,-1 ambiguous/invalid. */
    int(*offer)(void*,const uint16_t*,unsigned,unsigned*);
} LidTowerRequestOps;
static LidShopMarkerString lid_tr_key(unsigned index){
    static uint16_t history[]={'s','h','o','p','h','i','s','t','o','r','y',0};
    static uint16_t type[]={'t','y','p','e',0};
    static uint16_t item[]={'i','t','e','m','I','d',0};
    return index==0?(LidShopMarkerString){history,12,12}:
        index==1?(LidShopMarkerString){type,5,5}:(LidShopMarkerString){item,7,7};
}
static int lid_tr_text(const LidShopMarkerNative*b,void*row,unsigned key_index,
                       uint16_t*out,unsigned cap,unsigned*units){
    LidShopMarkerString key=lid_tr_key(key_index),value={0};int ok=0;
    if(!out||!units||cap<2||cap>96||!lid_sm_root(b,row,0))return 0;
    /* Release even a partial native GetString result on exceptional exit.
       Never return pointers into this native temporary or its JSON row. */
    __try{
        LidShopMarkerString*returned=b->string(row,&value,&key);
        if(returned!=&value||value.num<0||value.num>(int)cap||value.capacity<value.num||value.capacity>1048576)goto end;
        if(!value.num){out[0]=0;*units=0;ok=1;goto end;}
        if(!value.data||!b->access.copy(b->access.context,value.data,out,(size_t)value.num*2)||out[value.num-1])goto end;
        for(int i=0;i<value.num-1;i++)if(out[i]<0x21||out[i]>0x7e)goto end;
        if(!lid_sm_root(b,row,0))goto end;
        *units=(unsigned)value.num-1;ok=1;
end:;
    }__finally{if(value.data)b->release(value.data);}
    return ok;
}
static int lid_tr_buy(const uint16_t*type,unsigned units){
    static const char buy[]="buy_item";
    if(units!=8)return 0;
    for(unsigned i=0;i<8;i++){
        unsigned c=type[i];if(c>='A'&&c<='Z')c+='a'-'A';if(c!=(unsigned)buy[i])return 0;
    }return 1;
}
/* -1 malformed/ambiguous (NOT permission),0 native/no AP,1 AP snapshot.
   Match native _wcsicmp for type; exact native item IDs remain canonical.
   Snapshot/ops must be disjoint and resident for this synchronous call only. */
static int lid_tr_copy(const LidShopMarkerNative*b,void*root,const LidTowerRequestOps*ops,LidTowerRequest*out){
    struct {uintptr_t data;int32_t count,capacity;} a,z;
    LidShopMarkerString key=lid_tr_key(0);void*array;uintptr_t row,again;
    if(!out)return -1;__builtin_memset(out,0,sizeof(*out));
    if(!ops||!ops->offer||!b||!b->object||!b->string||!b->release||!lid_sm_root(b,root,0)){r299_note(6,30,0);return -1;}
    array=b->object(root,&key);
    if(!lid_sm_root(b,root,0)){r299_note(6,31,0);return -1;}
    if(!array){out->ready=1;return 0;}
    if(!lid_sm_root(b,array,0)||(uintptr_t)array>UINTPTR_MAX-0x120||
       !b->access.copy(b->access.context,(uint8_t*)array+0x110,&a,16)||a.count<0||a.count>128||
       a.capacity<a.count||a.capacity>4096||(a.count&&(!a.data||a.data>UINTPTR_MAX-(size_t)a.count*8))){r299_note(6,32,0);return -1;}
    for(unsigned i=0;i<(unsigned)a.count;i++){
        LidTowerRequestRow*r=&out->rows[i];uint16_t type[33];unsigned type_units;
        if(!lid_sm_root(b,array,0)||!b->access.copy(b->access.context,(void*)(a.data+i*8),&row,8)||
           !lid_tr_text(b,(void*)row,1,type,33,&type_units)){r299_note(6,33,i);return -1;}
        r->buy=lid_tr_buy(type,type_units);
        if(r->buy){
            if(!lid_tr_text(b,(void*)row,2,r->item,96,&r->units)||!r->units){r299_note(6,34,i);return -1;}
            int ap=ops->offer(ops->context,r->item,r->units,&r->offer);if(ap!=0&&ap!=1){r299_note(6,35,i);return -1;}
            r->ap=(unsigned)ap;
            if(r->ap){
                if(r->offer>=184){r299_note(6,36,i);return -1;}
                const wchar_t*expected=lid_tower_stock_data[r->offer].item;
                for(unsigned j=0;j<r->units;j++)if(r->item[j]!=(uint16_t)expected[j]){r299_note(6,37,i);return -1;}
                if(expected[r->units]){r299_note(6,38,i);return -1;}
                for(unsigned j=0;j<i;j++)if(out->rows[j].ap&&out->rows[j].offer==r->offer){r299_note(6,39,i);return -1;}
                if(++out->ap_count>64){r299_note(6,40,i);return -1;} /* real token queue bound */
            }
        }
        if(!lid_sm_root(b,array,0)||!b->access.copy(b->access.context,(void*)(a.data+i*8),&again,8)||again!=row){r299_note(6,41,i);return -1;}
    }
    if(!lid_sm_root(b,root,0)||!lid_sm_root(b,array,0)||
       !b->access.copy(b->access.context,(uint8_t*)array+0x110,&z,16)||
       a.data!=z.data||a.count!=z.count||a.capacity!=z.capacity||b->object(root,&key)!=array||!lid_sm_root(b,root,0)){r299_note(6,42,0);return -1;}
    out->count=(unsigned)a.count;out->ready=1;return out->ap_count?1:0;
}
#endif
