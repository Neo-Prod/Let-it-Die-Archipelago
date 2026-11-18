#ifndef LID_SHOP_TOWER_RECEIPT_WIRE_H
#define LID_SHOP_TOWER_RECEIPT_WIRE_H
#include "death_wire.h"
#include "shop_tower_stock_data.h"
/* Pure tower codec. Distinct from reserve-only Momoka LIDSHOP3. No native
   calls, mutation, receipt fabrication, disk IO, ownership or AP send here. */
enum {LID_TPW_SIZE=512,LID_TPW_STATE_SIZE=224,LID_TPW_PAYMENT=1,LID_TPW_ACK=2,LID_TPW_STATE=3};
typedef struct LidTowerPurchaseFrame {
    R15Context context;uint8_t catalogue[32];uint64_t sequence,location;
    unsigned kind,offer,price;int32_t before_carried,after_carried,before_free,after_free,before_paid,after_paid;
    unsigned debit,handler,item_units,product_units;uint16_t item[96],product[32];
} LidTowerPurchaseFrame;
static uint32_t lid_tpw_checksum(const uint8_t*p,unsigned size,unsigned at){
    uint32_t h=2166136261u;for(unsigned i=0;i<size;i++)if(i<at||i>=at+4)h=(h^p[i])*16777619u;return h;
}
static int lid_tpw_text(const uint16_t*text,unsigned units,unsigned capacity,const wchar_t*expected){
    if(!units||units>=capacity||text[units])return 0;
    for(unsigned i=0;i<units;i++)if(text[i]<33||text[i]>126||text[i]!=(uint16_t)expected[i])return 0;
    return !expected[units];
}
static int lid_tpw_valid(const LidTowerPurchaseFrame*f){
    if(!f||!r15_context_valid(&f->context)||!r15_nonzero(f->catalogue,32)||!f->sequence||f->offer>=184||
       f->location!=lid_tower_stock_data[f->offer].location||f->price>INT32_MAX||
       f->before_carried<0||f->after_carried<0||f->before_free<0||f->after_free<0||f->before_paid<0||f->after_paid<0||
       f->debit>1||f->handler>1)return 0;
    if(f->kind==LID_TPW_ACK){
        if(f->price||f->before_carried||f->after_carried||f->before_free||f->after_free||f->before_paid||f->after_paid||
           f->debit||f->handler||f->item_units||f->product_units)return 0;
        for(unsigned i=0;i<96;i++)if(f->item[i])return 0;for(unsigned i=0;i<32;i++)if(f->product[i])return 0;return 1;
    }
    const LidTowerStockData*d=&lid_tower_stock_data[f->offer];
    int64_t used=f->before_carried<(int64_t)f->price?f->before_carried:f->price;
    return f->kind==LID_TPW_PAYMENT&&f->price&&f->price==d->kc&&f->debit==1&&f->handler==1&&
        (int64_t)f->before_free>=(int64_t)f->price-used&&f->after_carried==(int64_t)f->before_carried-used&&
        f->after_free==(int64_t)f->before_free-((int64_t)f->price-used)&&f->after_paid==f->before_paid&&
        lid_tpw_text(f->item,f->item_units,96,d->item)&&lid_tpw_text(f->product,f->product_units,32,d->product);
}
static void lid_tpw_context_write(uint8_t*out,const R15Context*c,const uint8_t catalogue[32],unsigned kind){
    r15_copy(out,"LIDTSHP1",8);r15_w32(out+8,1);r15_w32(out+12,kind);
    r15_w32(out+16,c->pid);r15_w32(out+20,c->client_pid);r15_w64(out+24,c->birth);
    r15_copy(out+32,c->nonce,16);r15_copy(out+48,c->writer,16);r15_copy(out+64,c->run,32);
    r15_copy(out+96,c->config,32);r15_copy(out+128,catalogue,32);
}
static void lid_tpw_context_read(R15Context*c,const uint8_t*p){
    c->pid=r15_r32(p+16);c->client_pid=r15_r32(p+20);c->birth=r15_r64(p+24);
    r15_copy(c->nonce,p+32,16);r15_copy(c->writer,p+48,16);r15_copy(c->run,p+64,32);r15_copy(c->config,p+96,32);
}
static int lid_tpw_encode(uint8_t*out,size_t size,const LidTowerPurchaseFrame*f){
    if(!out||size!=LID_TPW_SIZE||!lid_tpw_valid(f))return 0;
    r15_zero(out,size);lid_tpw_context_write(out,&f->context,f->catalogue,f->kind);
    r15_w64(out+160,f->sequence);r15_w32(out+168,f->offer);r15_w32(out+172,f->price);r15_w64(out+176,f->location);
    r15_w32(out+184,(uint32_t)f->before_carried);r15_w32(out+188,(uint32_t)f->after_carried);
    r15_w32(out+192,(uint32_t)f->before_free);r15_w32(out+196,(uint32_t)f->after_free);
    r15_w32(out+200,(uint32_t)f->before_paid);r15_w32(out+204,(uint32_t)f->after_paid);
    out[208]=(uint8_t)f->debit;out[209]=(uint8_t)f->handler;
    out[210]=(uint8_t)f->item_units;out[211]=(uint8_t)(f->item_units>>8);
    out[212]=(uint8_t)f->product_units;out[213]=(uint8_t)(f->product_units>>8);
    for(unsigned i=0;i<f->item_units;i++){out[216+i*2]=(uint8_t)f->item[i];out[217+i*2]=(uint8_t)(f->item[i]>>8);}
    for(unsigned i=0;i<f->product_units;i++){out[408+i*2]=(uint8_t)f->product[i];out[409+i*2]=(uint8_t)(f->product[i]>>8);}
    r15_w32(out+508,lid_tpw_checksum(out,LID_TPW_SIZE,508));return 1;
}
static int lid_tpw_decode(LidTowerPurchaseFrame*out,const uint8_t*p,size_t size){
    LidTowerPurchaseFrame f={0};uint8_t canonical[LID_TPW_SIZE];
    if(!out||!p||size!=LID_TPW_SIZE||!r15_equal(p,"LIDTSHP1",8)||r15_r32(p+8)!=1||
       r15_r32(p+508)!=lid_tpw_checksum(p,LID_TPW_SIZE,508))return 0;
    lid_tpw_context_read(&f.context,p);r15_copy(f.catalogue,p+128,32);f.kind=r15_r32(p+12);
    f.sequence=r15_r64(p+160);f.offer=r15_r32(p+168);f.price=r15_r32(p+172);f.location=r15_r64(p+176);
    f.before_carried=(int32_t)r15_r32(p+184);f.after_carried=(int32_t)r15_r32(p+188);
    f.before_free=(int32_t)r15_r32(p+192);f.after_free=(int32_t)r15_r32(p+196);
    f.before_paid=(int32_t)r15_r32(p+200);f.after_paid=(int32_t)r15_r32(p+204);
    f.debit=p[208];f.handler=p[209];f.item_units=(unsigned)p[210]|((unsigned)p[211]<<8);
    f.product_units=(unsigned)p[212]|((unsigned)p[213]<<8);if(f.item_units>95||f.product_units>31)return 0;
    for(unsigned i=0;i<f.item_units;i++)f.item[i]=(uint16_t)((unsigned)p[216+2*i]|((unsigned)p[217+2*i]<<8));
    for(unsigned i=0;i<f.product_units;i++)f.product[i]=(uint16_t)((unsigned)p[408+2*i]|((unsigned)p[409+2*i]<<8));
    if(!lid_tpw_encode(canonical,sizeof(canonical),&f)||!r15_equal(canonical,p,size))return 0;*out=f;return 1;
}
static int lid_tpw_state_read(uint8_t bought[23],const uint8_t*p,size_t size,const R15Context*context,const uint8_t catalogue[32]){
    R15Context actual;uint8_t canonical[LID_TPW_STATE_SIZE];
    if(!bought||!p||!context||!catalogue||size!=LID_TPW_STATE_SIZE||!r15_context_valid(context)||!r15_nonzero(catalogue,32))return 0;
    lid_tpw_context_read(&actual,p);if(!r15_same_context(&actual,context))return 0;
    r15_zero(canonical,sizeof(canonical));lid_tpw_context_write(canonical,context,catalogue,LID_TPW_STATE);
    r15_copy(canonical+160,p+160,23);r15_w32(canonical+220,lid_tpw_checksum(canonical,LID_TPW_STATE_SIZE,220));
    if(!r15_equal(canonical,p,size))return 0;r15_copy(bought,p+160,23);return 1;
}
#endif
