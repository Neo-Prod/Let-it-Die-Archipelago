#ifndef LID_SHOP_COORDINATOR_H
#define LID_SHOP_COORDINATOR_H
/* Include codec/adapter, ledger, price_view first. No hooks/OS/game calls here.
   Caller must prove source ownership, session and capture context. Output key
   buffers are private, non-overlapping, and live for the complete handler.
   Future detours must verify call sites and manage thread-local handler depth.
   NOT included by mod.c yet. */
typedef struct LidShopCoordinatorOps {
    void* context;
    LidShopHistoryAdapterOps reader;
    unsigned long long (*session)(void*);
    unsigned (*thread)(void*);
    unsigned (*depth)(void*);
    int (*enter)(void*);
    void (*leave)(void*);
    void (*poison)(void*);
} LidShopCoordinatorOps;
typedef struct LidShopCoordinator {
    LidShopBatchLedger ledger;
    unsigned armed;
} LidShopCoordinator;
typedef struct LidShopRequestScope {
    LidShopBatchScope permission;
    const unsigned char* key;
    unsigned length,cursor,remaining,purchase_active;
} LidShopRequestScope;
static int lid_shop_coord_ops(const LidShopCoordinatorOps* o){
    return o&&o->session&&o->thread&&o->depth&&o->enter&&o->leave&&o->poison;
}
static int lid_shop_coord_live(LidShopCoordinator* c,const LidShopCoordinatorOps* o,unsigned long long session){
    return c&&__atomic_load_n(&c->armed,__ATOMIC_ACQUIRE)==1&&lid_shop_coord_ops(o)&&session&&o->session(o->context)==session;
}
static int lid_shop_coord_equal(const unsigned char* a,const unsigned char* b,unsigned n){
    unsigned i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;
}
/* Caller arms ONLY after complete detour installation; no partial feature. */
static int lid_shop_coord_init(LidShopCoordinator* c,const LidShopCoordinatorOps* o,unsigned long long session){
    if(!c||!lid_shop_coord_ops(o)||!session||o->session(o->context)!=session||!o->enter(o->context))return 0;
    __atomic_store_n(&c->armed,0,__ATOMIC_RELEASE);lid_shop_batch_init(&c->ledger,session);o->leave(o->context);
    return !c->ledger.disabled;
}
/* 0=valid empty ignored; 1=hub; 2=unknown/non-hub retained; -1=failed. */
static int lid_shop_coord_capture(LidShopCoordinator* c,const LidShopCoordinatorOps* o,
        unsigned long long session,const LidShopHistorySource* json,
        const LidShopHistorySource* owned_hub_history,int verified_hub,
        void* key_a,void* key_b,unsigned capacity){
    unsigned a=0,b=0,origin=LID_SHOP_ORIGIN_UNKNOWN;int result;
    if(!lid_shop_coord_live(c,o,session)||!json)return -1;
    if(json->count==0)return 0;
    if(!key_a||!key_b||!capacity||capacity>1048576||
       (size_t)key_a>(size_t)-1-capacity||(size_t)key_b>(size_t)-1-capacity||
       ((size_t)key_a<(size_t)key_b+capacity&&(size_t)key_b<(size_t)key_a+capacity)){
        o->poison(o->context);return -1;
    }
    if(!lid_shop_history_encode_source(json,&o->reader,session,key_a,capacity,&a)){
        o->poison(o->context);return -1;
    }
    if(verified_hub==1&&owned_hub_history&&
       lid_shop_history_encode_source(owned_hub_history,&o->reader,session,key_b,capacity,&b)&&
       a==b&&lid_shop_coord_equal(key_a,key_b,a))origin=LID_SHOP_ORIGIN_HUB;
    if(!lid_shop_coord_live(c,o,session)||!o->enter(o->context))return -1;
    if(!lid_shop_coord_live(c,o,session)){o->leave(o->context);return -1;}
    result=lid_shop_batch_capture(&c->ledger,session,key_a,a,origin);
    if(c->ledger.disabled){o->leave(o->context);o->poison(o->context);return -1;}
    o->leave(o->context);return result?1:2;
}
static int lid_shop_coord_begin(LidShopCoordinator* c,const LidShopCoordinatorOps* o,
        unsigned long long session,const LidShopHistorySource* json,void* key,unsigned capacity,
        LidShopRequestScope* scope){
    unsigned n=0;int result;
    if(!scope)return 0;
    scope->permission.closed=1;scope->purchase_active=0;scope->key=0;scope->length=scope->cursor=scope->remaining=0;
    if(!lid_shop_coord_live(c,o,session)||!json||!json->count||o->depth(o->context)!=1||
       !lid_shop_history_encode_source(json,&o->reader,session,key,capacity,&n)||!o->enter(o->context))return 0;
    result=lid_shop_coord_live(c,o,session)&&lid_shop_batch_begin(&c->ledger,session,key,n,
        o->thread(o->context),o->depth(o->context),&scope->permission);
    o->leave(o->context);
    if(!result)return 0;
    scope->key=key;scope->length=n;scope->cursor=8;scope->remaining=json->count;return 1;
}
static int lid_shop_coord_type_buy(const unsigned char* s,unsigned n){
    static const char text[]="buy_part";unsigned i,v;
    if(n!=8)return 0;
    for(i=0;i<8;i++){
        v=(unsigned)s[2*i]|((unsigned)s[2*i+1]<<8);
        if(v>='A'&&v<='Z')v+=32;
        if(v!=(unsigned char)text[i])return 0;
    }
    return 1;
}
/* Consume the NEXT expected buy_part (including native case-insensitive type
   matching). Mismatch permanently closes this scope. All other rows skipped.
   utf16_ptid is an owned local snapshot including its terminator, not a raw
   unchecked game pointer. Caller must separately verify the native call site. */
static int lid_shop_coord_purchase(LidShopRequestScope* s,const unsigned short* utf16_ptid,
                                  unsigned ptid_units,int level){
    const unsigned char* fields[8];unsigned lengths[8],i,j,at,n;int buy;
    if(!s||s->permission.closed||s->purchase_active||!utf16_ptid||ptid_units>256)return 0;
    while(s->remaining){
        at=s->cursor;
        for(i=0;i<8;i++){
            if(at>s->length||s->length-at<4)goto bad;
            n=lid_shop_history_u32(s->key+at);at+=4;
            if(n>256||n>(s->length-at)/2)goto bad;
            fields[i]=s->key+at;lengths[i]=n;at+=n*2;
        }
        if(s->length-at<20)goto bad;
        buy=lid_shop_coord_type_buy(fields[0],lengths[0]);
        s->cursor=at+20;s->remaining--;
        if(!buy)continue;
        if((unsigned)level!=lid_shop_history_u32(s->key+at+4)||lengths[3]!=ptid_units||utf16_ptid[ptid_units])goto bad;
        for(j=0;j<ptid_units;j++)if(utf16_ptid[j]!=((unsigned)fields[3][2*j]|((unsigned)fields[3][2*j+1]<<8)))goto bad;
        s->purchase_active=1;return 1;
    }
bad:
    s->permission.closed=1;s->purchase_active=0;return 0;
}
static int lid_shop_coord_price(LidShopCoordinator* c,const LidShopCoordinatorOps* o,
        unsigned long long session,LidShopRequestScope* s,void* detached,unsigned size){
    int result;
    if(!s||!s->purchase_active||!lid_shop_coord_live(c,o,session)||!o->enter(o->context))return 0;
    result=lid_shop_coord_live(c,o,session)&&lid_shop_batch_allows(&c->ledger,&s->permission,session,
        o->thread(o->context),o->depth(o->context),1)&&lid_shop_price_native(detached,size,1);
    o->leave(o->context);return result;
}
static void lid_shop_coord_purchase_end(LidShopRequestScope* s){if(s)s->purchase_active=0;}
static void lid_shop_coord_end(LidShopRequestScope* s){
    if(s){lid_shop_batch_end(&s->permission);s->purchase_active=0;s->key=0;s->length=s->cursor=s->remaining=0;}
}
#endif
