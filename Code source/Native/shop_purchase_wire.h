#ifndef LID_SHOP_PURCHASE_WIRE_H
#define LID_SHOP_PURCHASE_WIRE_H
#include "death_wire.h"
#include "shop_momoka_payment.h"
/* R244 receipt wire, paired with ShopPurchase.py. Owned output only. No hooks,
   native calls, files, wallet/save writes or AP sends. Host authenticates
   current Context/catalogue and reserves queue space BEFORE native mutation.
   Vanilla decal grant AND UI owned-decal addition must be suppressed by the
   exact AP purchase host; this wire does not authorize or perform any grant.
   SKU is the shop carrier, NOT the AP reward. Do not invoke the item-entry
   dispatcher or open a waiting-room entry in response to a shop purchase.
   A valid debit receipt alone cannot prove outer handler success; encode only
   after the exact owning handler returned successfully. Checksum is not MAC. */
enum {LID_SP_SIZE=512,LID_SP_PAYMENT=1,LID_SP_RECEIVED=2,LID_SP_STATE=3,LID_SP_STATE_SIZE=704};
typedef struct LidShopPurchaseFrame {
    R15Context context;uint8_t catalogue[32];uint64_t sequence;
    unsigned kind,offer,price;LidMPMoney before,after;
    unsigned debit_result,handler_result,units;uint16_t skill[129];
} LidShopPurchaseFrame;
static uint32_t lid_sp_checksum(const uint8_t*p){
    uint32_t h=2166136261u;
    for(unsigned i=0;i<LID_SP_SIZE;i++)if(i<476||i>=480)h=(h^p[i])*16777619u;
    return h;
}
static int lid_sp_valid(const LidShopPurchaseFrame*f){
    if(!f||!r15_context_valid(&f->context)||!r15_nonzero(f->catalogue,32)||
       !f->sequence||f->offer>=4096||f->price>INT32_MAX||
       f->before.free_money<0||f->before.paid_money<0||
       f->after.free_money<0||f->after.paid_money<0||
       f->debit_result>1||f->handler_result>1)return 0;
    if(f->kind==LID_SP_RECEIVED){
        if(f->units||f->price||f->before.free_money||f->before.paid_money||
           f->after.free_money||f->after.paid_money||f->debit_result||f->handler_result)return 0;
        for(unsigned i=0;i<129;i++)if(f->skill[i])return 0;return 1;
    }
    return f->kind==LID_SP_PAYMENT&&lid_mp_id(f->skill,f->units)&&f->price&&
      f->before.free_money>=(int32_t)f->price&&
      f->after.free_money==f->before.free_money-(int32_t)f->price&&
      f->after.paid_money==f->before.paid_money&&f->debit_result==1&&f->handler_result==1;
}
static int lid_sp_encode(uint8_t*out,size_t capacity,const LidShopPurchaseFrame*f){
    if(!out||capacity!=LID_SP_SIZE||!lid_sp_valid(f))return 0;
    r15_zero(out,capacity);r15_copy(out,"LIDSHOP3",8);r15_w32(out+8,1);r15_w32(out+12,f->kind);
    r15_w32(out+16,f->context.pid);r15_w32(out+20,f->context.client_pid);r15_w64(out+24,f->context.birth);
    r15_copy(out+32,f->context.nonce,16);r15_copy(out+48,f->context.writer,16);
    r15_copy(out+64,f->context.run,32);r15_copy(out+96,f->context.config,32);r15_copy(out+128,f->catalogue,32);
    r15_w64(out+160,f->sequence);r15_w32(out+168,f->offer);r15_w32(out+172,f->price);
    r15_w32(out+176,(uint32_t)f->before.free_money);r15_w32(out+180,(uint32_t)f->after.free_money);
    r15_w32(out+184,(uint32_t)f->before.paid_money);r15_w32(out+188,(uint32_t)f->after.paid_money);
    out[192]=(uint8_t)f->debit_result;out[193]=(uint8_t)f->handler_result;
    out[194]=(uint8_t)f->units;out[195]=(uint8_t)(f->units>>8);
    for(unsigned i=0;i<f->units;i++){out[196+i*2]=(uint8_t)f->skill[i];out[197+i*2]=(uint8_t)(f->skill[i]>>8);}
    r15_w32(out+476,lid_sp_checksum(out));return 1;
}
static int lid_sp_decode(LidShopPurchaseFrame*out,const uint8_t*p,size_t size){
    LidShopPurchaseFrame f={0};uint8_t canonical[LID_SP_SIZE];
    if(!out||!p||size!=LID_SP_SIZE)return 0;
    f.kind=r15_r32(p+12);f.context.pid=r15_r32(p+16);f.context.client_pid=r15_r32(p+20);
    f.context.birth=r15_r64(p+24);r15_copy(f.context.nonce,p+32,16);r15_copy(f.context.writer,p+48,16);
    r15_copy(f.context.run,p+64,32);r15_copy(f.context.config,p+96,32);r15_copy(f.catalogue,p+128,32);
    f.sequence=r15_r64(p+160);f.offer=r15_r32(p+168);f.price=r15_r32(p+172);
    f.before.free_money=(int32_t)r15_r32(p+176);f.after.free_money=(int32_t)r15_r32(p+180);
    f.before.paid_money=(int32_t)r15_r32(p+184);f.after.paid_money=(int32_t)r15_r32(p+188);
    f.debit_result=p[192];f.handler_result=p[193];f.units=(unsigned)p[194]|((unsigned)p[195]<<8);
    if(f.units>128)return 0;
    for(unsigned i=0;i<f.units;i++)f.skill[i]=(uint16_t)((unsigned)p[196+i*2]|((unsigned)p[197+i*2]<<8));
    if(!lid_sp_encode(canonical,sizeof(canonical),&f)||!r15_equal(canonical,p,size))return 0;*out=f;return 1;
}
/* Compose from actual per-row payment evidence only, AFTER handler completion.
   Inputs/outputs disjoint; current run ownership is a host precondition. */
static int lid_sp_from_receipt(LidShopPurchaseFrame*out,const R15Context*context,
    const uint8_t catalogue[32],uint64_t sequence,const LidMPRow*row,
    const LidMPReceipt*receipt,unsigned handler_result){
    LidShopPurchaseFrame f={0};
    if(!out||!context||!catalogue||!row||!receipt||!receipt->paid||!receipt->after_valid||
       receipt->native_result!=1||receipt->offer!=row->offer||receipt->price!=row->price||
       row->spirit_price||row->recycle_price)return 0;
    f.context=*context;r15_copy(f.catalogue,catalogue,32);f.sequence=sequence;
    f.kind=LID_SP_PAYMENT;f.offer=row->offer;f.price=row->price;f.before=receipt->before;f.after=receipt->after;
    f.debit_result=receipt->native_result;f.handler_result=handler_result;f.units=row->units;
    if(f.units>128)return 0;for(unsigned i=0;i<=f.units;i++)f.skill[i]=row->skill[i];
    if(!lid_sp_valid(&f))return 0;*out=f;return 1;
}
/* Client's sold-out snapshot after durable receipt/server confirmation. Read
   only on transport worker, then publish owned bitmap to UI. No UI file IO.
   Host separately rejects set bits not present in the immutable catalogue. */
static int lid_sp_state_read(uint8_t bought[512],const uint8_t*p,size_t size,
    const R15Context*expected,const uint8_t catalogue[32]){
    uint32_t hash=2166136261u;
    if(!bought||!p||size!=LID_SP_STATE_SIZE||!r15_context_valid(expected)||!catalogue||
       !r15_equal(p,"LIDSHOP3",8)||r15_r32(p+8)!=1||r15_r32(p+12)!=LID_SP_STATE||
       r15_r32(p+16)!=expected->pid||r15_r32(p+20)!=expected->client_pid||
       r15_r64(p+24)!=expected->birth||!r15_equal(p+32,expected->nonce,16)||
       !r15_equal(p+48,expected->writer,16)||!r15_equal(p+64,expected->run,32)||
       !r15_equal(p+96,expected->config,32)||!r15_equal(p+128,catalogue,32)||
       !r15_nonzero(catalogue,32))return 0;
    for(unsigned i=676;i<LID_SP_STATE_SIZE;i++)if(p[i])return 0;
    for(unsigned i=0;i<LID_SP_STATE_SIZE;i++)if(i<672||i>=676)hash=(hash^p[i])*16777619u;
    if(hash!=r15_r32(p+672))return 0;r15_copy(bought,p+160,512);return 1;
}
#endif
