#ifndef LID_SHOP_TRANSACTION_STATE_H
#define LID_SHOP_TRANSACTION_STATE_H
#include <stdint.h>
#include <stddef.h>
/* Production C state, no game calls/IO/network. Ledger access requires the
   host's admission. Thread IDs must come from the real current-thread API.
   Gate BUSY is not a native Save result and must never silently lose a request.
   No wait, engine lock or reentrant acquisition is implemented here. */
enum {LID_ST_EMPTY,LID_ST_PENDING,LID_ST_CLAIMED,LID_ST_PAYING,
      LID_ST_PAID,LID_ST_VERIFIED,LID_ST_UNCERTAIN};
enum {LID_ST_PURCHASE=1,LID_ST_SAVE=2,LID_ST_METADATA=3};
enum {LID_ST_MAX=4096,LID_ST_BATCH=128,LID_ST_MARKER_MAX=4212};
typedef struct LidShopAdmission {uint64_t owner,next;unsigned kind;} LidShopAdmission;
typedef struct LidShopLease {uint64_t token;uint32_t thread;unsigned kind;} LidShopLease;
typedef struct LidShopLedger {
    unsigned initialized,count,blocked;uint64_t revision;
    unsigned char phase[LID_ST_MAX];uint32_t price[LID_ST_MAX];
    uint64_t paid_revision[LID_ST_MAX];
} LidShopLedger;
static int lid_st_equal(const void*a,const void*b,size_t n){
    const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static int lid_st_acquire(LidShopAdmission*g,uint32_t thread,unsigned kind,LidShopLease*out){
    uint64_t expected=0,next,token;
    if(!g||!out||!thread||kind<LID_ST_PURCHASE||kind>LID_ST_METADATA||
       __atomic_load_n(&g->owner,__ATOMIC_ACQUIRE))return 0;
    next=__atomic_load_n(&g->next,__ATOMIC_ACQUIRE);
    do{if(next>=UINT32_MAX)return 0;}
    while(!__atomic_compare_exchange_n(&g->next,&next,next+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE));
    token=((uint64_t)thread<<32)|(next+1);
    if(!__atomic_compare_exchange_n(&g->owner,&expected,token,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    /* Inspect the output only AFTER owning admission, not while its previous
       owner may still be publishing/releasing the shared host handle. */
    if(out->token){__atomic_store_n(&g->owner,0,__ATOMIC_RELEASE);return 0;}
    __atomic_store_n(&g->kind,kind,__ATOMIC_RELEASE);
    *out=(LidShopLease){token,thread,kind};return 1;
}
static int lid_st_held(const LidShopAdmission*g,const LidShopLease*l,uint32_t current){
    if(!g||!l||!current||(__atomic_load_n(&g->owner,__ATOMIC_ACQUIRE)>>32)!=current)return 0;
    return l->token&&current==l->thread&&__atomic_load_n(&g->owner,__ATOMIC_ACQUIRE)==l->token&&
      __atomic_load_n(&g->kind,__ATOMIC_ACQUIRE)==l->kind&&
      __atomic_load_n(&g->owner,__ATOMIC_ACQUIRE)==l->token;
}
static int lid_st_release(LidShopAdmission*g,LidShopLease*l,uint32_t current){
    if(!lid_st_held(g,l,current))return 0;LidShopLease old=*l;uint64_t expected=l->token;
    /* Clear the handle BEFORE publishing owner=0, so a new owner can safely
       reuse host storage. Never clear kind after another owner may claim it. */
    *l=(LidShopLease){0};
    if(!__atomic_compare_exchange_n(&g->owner,&expected,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){*l=old;return 0;}
    return 1;
}
static int lid_st_init(LidShopLedger*l,unsigned count){
    if(!l||l->initialized||!count||count>LID_ST_MAX)return 0;
    __builtin_memset(l,0,sizeof(*l));l->count=count;l->initialized=1;return 1;
}
static int lid_st_reserve(LidShopLedger*l,unsigned id,uint32_t price){
    if(!l||!l->initialized||l->blocked||id>=l->count||!price||price>INT32_MAX||l->phase[id]!=LID_ST_EMPTY)return 0;
    l->price[id]=price;l->phase[id]=LID_ST_PENDING;return 1;
}
static int lid_st_claim(LidShopLedger*l,const unsigned*ids,unsigned count){
    if(!l||!l->initialized||l->blocked||!ids||!count||count>LID_ST_BATCH)return 0;
    for(unsigned i=0;i<count;i++){
        if(ids[i]>=l->count||l->phase[ids[i]]!=LID_ST_PENDING)return 0;
        for(unsigned j=0;j<i;j++)if(ids[i]==ids[j])return 0;
    }
    for(unsigned i=0;i<count;i++)l->phase[ids[i]]=LID_ST_CLAIMED;return 1;
}
static int lid_st_unclaim(LidShopLedger*l,unsigned id){
    if(!l||!l->initialized||id>=l->count||l->phase[id]!=LID_ST_CLAIMED)return 0;
    l->phase[id]=LID_ST_PENDING;return 1;
}
static int lid_st_cancel(LidShopLedger*l,unsigned id){
    if(!l||!l->initialized||id>=l->count||l->phase[id]!=LID_ST_PENDING)return 0;
    l->phase[id]=LID_ST_EMPTY;l->price[id]=0;return 1;
}
static int lid_st_begin(LidShopLedger*l,unsigned id,uint32_t price,uint32_t free_money){
    if(!l||!l->initialized||l->blocked||id>=l->count||l->phase[id]!=LID_ST_CLAIMED||
       !price||l->price[id]!=price||free_money>INT32_MAX||free_money<price)return 0;
    l->phase[id]=LID_ST_PAYING;return 1;
}
static void lid_st_uncertain(LidShopLedger*l,unsigned id){
    if(!l||!l->initialized)return;
    l->blocked=1;if(id<l->count)l->phase[id]=LID_ST_UNCERTAIN;
}
static int lid_st_paid(LidShopLedger*l,unsigned id){
    if(!l||!l->initialized||l->blocked||id>=l->count||l->phase[id]!=LID_ST_PAYING)return 0;
    if(l->revision==UINT64_MAX){lid_st_uncertain(l,id);return 0;}
    l->paid_revision[id]=++l->revision;l->phase[id]=LID_ST_PAID;return 1;
}
static int lid_st_serial_phase(unsigned p){
    if(p==LID_ST_PAYING||p>LID_ST_UNCERTAIN)return -1;
    if(p==LID_ST_UNCERTAIN)return 2;
    return p==LID_ST_PAID||p==LID_ST_VERIFIED?1:0;
}
static int lid_st_snapshot_valid(const LidShopLedger*l){
    unsigned uncertain=0;
    if(!l||!l->initialized||!l->count||l->count>LID_ST_MAX||l->blocked>1)return 0;
    for(unsigned i=0;i<l->count;i++){
        int p=lid_st_serial_phase(l->phase[i]);if(p<0)return 0;
        if(p==1&&(!l->paid_revision[i]||l->paid_revision[i]>l->revision))return 0;
        if(p==2)uncertain=1;
    }
    return uncertain==l->blocked;
}
static int lid_st_encode(const LidShopLedger*l,const unsigned char identity[96],
                          unsigned char*out,unsigned cap,unsigned*size){
    if(!identity||!out||!size||!lid_st_snapshot_valid(l)||cap<116+l->count)return 0;
    __builtin_memcpy(out,"LIDSHOP2",8);__builtin_memcpy(out+8,identity,96);
    for(unsigned i=0;i<8;i++)out[104+i]=(unsigned char)(l->revision>>(8*i));
    for(unsigned i=0;i<4;i++)out[112+i]=(unsigned char)(l->count>>(8*i));
    for(unsigned i=0;i<l->count;i++)out[116+i]=(unsigned char)lid_st_serial_phase(l->phase[i]);
    *size=116+l->count;return 1;
}
/* Restore to a NEW zeroed ledger, before publication and any request. */
static int lid_st_restore(LidShopLedger*out,const unsigned char*data,unsigned size,
                          const unsigned char identity[96],unsigned expected_count){
    uint64_t revision=0;unsigned count=0;
    if(!out||out->initialized||!data||!identity||size<117||size>LID_ST_MARKER_MAX||
       !lid_st_equal(data,"LIDSHOP2",8)||!lid_st_equal(data+8,identity,96))return 0;
    for(unsigned i=0;i<8;i++)revision|=(uint64_t)data[104+i]<<(8*i);
    for(unsigned i=0;i<4;i++)count|=(unsigned)data[112+i]<<(8*i);
    if(!count||count>LID_ST_MAX||count!=expected_count||size!=116+count)return 0;
    for(unsigned i=116;i<size;i++)if(data[i]>2||(data[i]==1&&!revision))return 0;
    if(!lid_st_init(out,count))return 0;out->revision=revision;
    for(unsigned i=0;i<count;i++){
        unsigned p=data[116+i];out->phase[i]=p==2?LID_ST_UNCERTAIN:p==1?LID_ST_PAID:LID_ST_EMPTY;
        if(p==1)out->paid_revision[i]=revision;if(p==2)out->blocked=1;
    }
    return 1;
}
static int lid_st_marker_matches(const LidShopLedger*l,const unsigned char identity[96],const unsigned char*data,unsigned size){
    if(!identity||!data||!lid_st_snapshot_valid(l)||size!=116+l->count||
       !lid_st_equal(data,"LIDSHOP2",8)||!lid_st_equal(data+8,identity,96))return 0;
    for(unsigned i=0;i<8;i++)if(data[104+i]!=(unsigned char)(l->revision>>(8*i)))return 0;
    for(unsigned i=0;i<4;i++)if(data[112+i]!=(unsigned char)(l->count>>(8*i)))return 0;
    for(unsigned i=0;i<l->count;i++)if(data[116+i]!=(unsigned)lid_st_serial_phase(l->phase[i]))return 0;
    return 1;
}
/* Caller must FIRST independently verify native BRG, bank balances, current
   peer/session and immutable expectation. These bytes alone are NOT an ACK. */
static int lid_st_verify(LidShopLedger*l,const unsigned char identity[96],const unsigned char*data,unsigned size,
                          const unsigned*ids,unsigned count){
    if(!l||l->blocked||!ids||!count||count>LID_ST_BATCH||!lid_st_marker_matches(l,identity,data,size))return 0;
    for(unsigned i=0;i<count;i++){
        unsigned id=ids[i];if(id>=l->count||(l->phase[id]!=LID_ST_PAID&&l->phase[id]!=LID_ST_VERIFIED))return 0;
        for(unsigned j=0;j<i;j++)if(ids[j]==id)return 0;
    }
    for(unsigned i=0;i<count;i++)l->phase[ids[i]]=LID_ST_VERIFIED;return 1;
}
static int lid_st_can_send(const LidShopLedger*l,unsigned id){
    return l&&l->initialized&&!l->blocked&&id<l->count&&l->phase[id]==LID_ST_VERIFIED;
}
#endif
