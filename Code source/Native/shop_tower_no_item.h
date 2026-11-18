#ifndef LID_SHOP_TOWER_NO_ITEM_H
#define LID_SHOP_TOWER_NO_ITEM_H
#include "shop_tower_payment.h"
/* Exact11683E3 CALL adapter only. Native caller checks nonempty FString and
   frees it; it never uses contents as an entity key. AP return names an ACTUAL
   resident PAID receipt, not an equipment EIID, grant or final handler success.
   No CALL publisher/default admission is supplied by this component. */
typedef struct LidTowerNoItemOps {
    void*context;
    int(*owned_call)(void*,uintptr_t); /* actual E8/relay owner, not just SHA */
    /*0 genuine native,-1 denied knownAP,1 actual claimed request token.
       Must still recognize old AP carriers when admission is stopped. */
    int(*adopt)(void*,int,const wchar_t*,uint64_t*);
} LidTowerNoItemOps;
typedef struct LidTowerNoItem {
    unsigned prepared;uintptr_t image;LidTowerPayment*payment;LidTowerNoItemOps ops;
    LidTSString*(*original)(LidTSString*,int,const wchar_t*);
    LidTSString*(*construct)(LidTSString*,const LidTSString*,uint64_t,uint64_t);
} LidTowerNoItem;
static int lid_tni_prepare(LidTowerNoItem*h,LidTowerPayment*p,const M195Ops*memory,
                           const LidTowerNoItemOps*ops,int exact_sha){
    if(!h||h->prepared||!p||!p->prepared||!p->native.prepared||!ops||!ops->owned_call||!ops->adopt||exact_sha!=1)return 0;
    static const uint8_t profiles[2][24]={
        {0x48,0x89,0x4c,0x24,8,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x48},
        {0x48,0x89,0x4c,0x24,8,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x30,0xf5,0xff,0xff}};
    const uint32_t rvas[2]={0x136c0,0x12dc220};uint8_t actual[24];uintptr_t image=p->native.image;
    for(unsigned i=0;i<2;i++){
        if(!m195_read(memory,image,rvas[i],actual,24))return 0;
        for(unsigned j=0;j<24;j++)if(actual[j]!=profiles[i][j])return 0;
    }
    h->image=image;h->payment=p;h->ops=*ops;
    h->original=(__typeof__(h->original))(image+0x12dc220);h->construct=(__typeof__(h->construct))(image+0x136c0);
    h->prepared=1;return 1;
}
/* This is the identifier of an existing in-RAM payment record. The later
   separate durable receipt wire must keep this same Context/token identity. */
static LidTSString lid_tni_name(const LidTowerQueue*q,uint64_t token,wchar_t text[80]){
    static const wchar_t prefix[]=L"AP-TOWER-PAYMENT/",hex[]=L"0123456789ABCDEF";unsigned n=0;
    while(prefix[n]){text[n]=prefix[n];n++;}
    for(unsigned i=0;i<16;i++){text[n++]=hex[q->context.nonce[i]>>4];text[n++]=hex[q->context.nonce[i]&15];}
    text[n++]='/';for(unsigned i=0;i<16;i++)text[n++]=hex[(token>>(60-4*i))&15];text[n]=0;
    return (LidTSString){text,(int32_t)n+1,(int32_t)n+1};
}
/* Host captures ORIGINAL caller at its actual compiled wrapper (before any C
   forwarding). Known native ABI gives out an owned16byte stack destination,
   not a borrowed/shared engine FString. Caller/header/host stay resident. */
static LidTSString*lid_tni_call(LidTowerNoItem*h,LidTSString*out,int uid,const wchar_t*item,uintptr_t caller){
    if(!h||!h->prepared){r299_note(10,1,0);return 0;} /* forbidden after any live publication */
    if(caller!=h->image+0x115e18d){r299_note(10,2,0);return h->original(out,uid,item);}
    uint64_t token=0;int status=h->ops.adopt(h->ops.context,uid,item,&token);
    r299_note(10,40+(unsigned)(status+1),token!=0);
    if(!status&&!token){r299_note(10,3,0);return h->original(out,uid,item);}
    /* Native helper failure contract: empty owned output -> original native
       error response branch. Never fallback to granting a known AP carrier. */
    *out=(LidTSString){0};volatile unsigned ok=0;
    __try{
        if(status!=1||!token||h->ops.owned_call(h->ops.context,caller)!=1)goto end;
        if(!lid_tp_pay(h->payment,token,uid))goto end;
        unsigned index;
        if(!lid_tp_owned(h->payment,token,LID_TQ_PAID,&index)||
           h->payment->records[index].receipt.handler_result)goto end;
        wchar_t text[80];LidTSString id=lid_tni_name(h->payment->queue,token,text);
        /* Engine constructor deep-copies stack text; original native handler
           owns/frees the result. No shared header or literal heap ownership. */
        LidTSString*result=h->construct(out,&id,0,0);
        if(result!=out||!out->data||out->data==text||out->count!=id.count||out->capacity<out->count)goto end;
        ok=1;
end:;
    }__finally{
        if(!ok){lid_tp_fail(h->payment,token);out->count=0;}
        /* Keep any engine-owned allocation for ORIGINAL caller's destructor.
           No allocator/engine recovery calls while propagating an exception. */
    }
    {r299_note(10,4,0);return out;}
}
#endif
