#ifndef LID_SHOP_BATCH_LEDGER_H
#define LID_SHOP_BATCH_LEDGER_H
/* Pure bounded provenance store. NOT integrated into mod.c yet.
   Caller serializes ALL access and supplies complete canonical batch bytes.
   No game pointers, game mutations, allocation, timers, or OS calls here.
   Zero-initialize the store before FIRST init. Reinitialization invalidates
   every outstanding scope, even if the caller reuses the same session id.
   Session must identify the AP/save session, NOT the current floor/location.
   Complete canonical row equality and mixed-row buy_part filtering belong to
   the adapter. Hashes never authorize without exact byte equality. */
#ifndef LID_SHOP_BATCH_CAPACITY
#define LID_SHOP_BATCH_CAPACITY 128
#endif
#ifndef LID_SHOP_BATCH_BYTES
#define LID_SHOP_BATCH_BYTES 1048576
#endif
enum { LID_SHOP_ORIGIN_HUB=1, LID_SHOP_ORIGIN_OTHER=2, LID_SHOP_ORIGIN_UNKNOWN=4 };
typedef struct LidShopBatchEntry {
    unsigned offset,length,origins;
    unsigned long long hash,revision;
} LidShopBatchEntry;
typedef struct LidShopBatchLedger {
    unsigned long long session,generation;
    unsigned count,used,disabled;
    LidShopBatchEntry entries[LID_SHOP_BATCH_CAPACITY];
    unsigned char bytes[LID_SHOP_BATCH_BYTES];
} LidShopBatchLedger;
typedef struct LidShopBatchScope {
    unsigned long long session,generation,revision;
    unsigned index,owner,closed;
} LidShopBatchScope;
static void lid_shop_batch_init(LidShopBatchLedger* l,unsigned long long session){
    if(!l)return;
    if(l->generation==~0ULL){l->disabled=1;return;}
    l->generation++;l->session=session;l->count=0;l->used=0;l->disabled=!session;
}
static unsigned long long lid_shop_batch_hash(const unsigned char* b,unsigned n){
    unsigned i;unsigned long long h=14695981039346656037ULL;
    for(i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}
    return h;
}
static int lid_shop_batch_valid(LidShopBatchLedger* l,unsigned long long session){
    return l&&!l->disabled&&session&&session==l->session&&
        l->count<=LID_SHOP_BATCH_CAPACITY&&l->used<=LID_SHOP_BATCH_BYTES;
}
static int lid_shop_batch_find(LidShopBatchLedger* l,const unsigned char* b,unsigned n){
    unsigned i,j;unsigned long long hash;LidShopBatchEntry* e;
    if(!b||!n||n>LID_SHOP_BATCH_BYTES)return -1;
    hash=lid_shop_batch_hash(b,n);
    for(i=0;i<l->count;i++){
        e=l->entries+i;
        if(e->hash!=hash||e->length!=n)continue;
        if(e->offset>l->used||n>l->used-e->offset){l->disabled=1;return -1;}
        for(j=0;j<n&&l->bytes[e->offset+j]==b[j];j++){}
        if(j==n)return (int)i;
    }
    return -1;
}
/* origin MUST be supplied from freshly verified capture context, never the
   location at debit time. Unknown captures are retained as tombstones. */
static int lid_shop_batch_capture(LidShopBatchLedger* l,unsigned long long session,
                                 const void* key,unsigned n,unsigned origin){
    int found;unsigned i,merged;LidShopBatchEntry* e;const unsigned char* b=key;
    if(!lid_shop_batch_valid(l,session))return 0;
    if(!b||!n||n>LID_SHOP_BATCH_BYTES){l->disabled=1;return 0;}
    if(origin!=LID_SHOP_ORIGIN_HUB&&origin!=LID_SHOP_ORIGIN_OTHER)origin=LID_SHOP_ORIGIN_UNKNOWN;
    found=lid_shop_batch_find(l,b,n);if(l->disabled)return 0;
    if(found>=0){
        e=l->entries+found;merged=e->origins|origin;
        if(merged!=e->origins){
            if(e->revision==~0ULL){l->disabled=1;return 0;}
            e->revision++;e->origins=merged;
        }
        return merged==LID_SHOP_ORIGIN_HUB;
    }
    if(l->count>=LID_SHOP_BATCH_CAPACITY||n>LID_SHOP_BATCH_BYTES-l->used){l->disabled=1;return 0;}
    e=l->entries+l->count++;
    e->offset=l->used;e->length=n;e->hash=lid_shop_batch_hash(b,n);e->origins=origin;e->revision=1;
    for(i=0;i<n;i++)l->bytes[l->used+i]=b[i];
    l->used+=n;return origin==LID_SHOP_ORIGIN_HUB;
}
/* depth==1 is the root handler on this thread; nested scopes are refused. */
static int lid_shop_batch_begin(LidShopBatchLedger* l,unsigned long long session,
                               const void* key,unsigned n,unsigned thread,unsigned depth,
                               LidShopBatchScope* s){
    int found;LidShopBatchEntry* e;
    if(!s)return 0;
    s->session=0;s->generation=0;s->revision=0;s->index=0;s->owner=0;s->closed=1;
    if(!thread||depth!=1||!lid_shop_batch_valid(l,session))return 0;
    found=lid_shop_batch_find(l,key,n);if(found<0||l->disabled)return 0;
    e=l->entries+found;if(e->origins!=LID_SHOP_ORIGIN_HUB)return 0;
    s->session=session;s->generation=l->generation;s->revision=e->revision;s->index=(unsigned)found;s->owner=thread;s->closed=0;
    return 1;
}
static int lid_shop_batch_allows(LidShopBatchLedger* l,const LidShopBatchScope* s,
                                unsigned long long session,unsigned thread,unsigned depth,
                                int exact_buy_part){
    LidShopBatchEntry* e;
    if(!s||s->closed||exact_buy_part!=1||depth!=1||!thread||s->owner!=thread||
       s->session!=session||!lid_shop_batch_valid(l,session)||s->generation!=l->generation||s->index>=l->count)return 0;
    e=l->entries+s->index;
    return e->origins==LID_SHOP_ORIGIN_HUB&&e->revision==s->revision;
}
static void lid_shop_batch_end(LidShopBatchScope* s){if(s)s->closed=1;}
#endif
