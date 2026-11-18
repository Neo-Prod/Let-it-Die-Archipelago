#ifndef LID_SHOP_RUNTIME_CATALOGUE_H
#define LID_SHOP_RUNTIME_CATALOGUE_H
#include "shop_momoka_purchase_ui.h"
#include "material_module_hash.h"
#include "shop_runtime_catalogue_profiles.h"
/* Background-only immutable catalogue loader. No game calls, hooks, Save,
   wallet writes or reward/entry dispatch. Own buffers/context remain resident.
   SHA profiles pin frozen row definitions; Context binds existing trusted peer.
   FNV is corruption detection, NOT authentication against hostile local code. */
enum {LID_SC_HEADER=256,LID_SC_MOMOKA_ROW=192,LID_SC_TOWER_ROW=256,LID_SC_MAX=52352};
typedef struct LidShopRuntimeTower {
    unsigned offer,kind,floor,shopid,plan_slot;uint64_t location;
    char room_key[64],product[32],native_item[96];
} LidShopRuntimeTower;
typedef struct LidShopRuntimeCatalogue {
    unsigned initialized,goal,momoka_count,tower_count;uint32_t ui,worker;R15Context context;
    uint8_t full[32],momoka_hash[32],tower_hash[32],allowed[512];
    LidShopUiSku momoka[26];LidShopRuntimeTower tower[184];
} LidShopRuntimeCatalogue;
typedef struct LidShopCatalogueHash {
    unsigned attempted,failed;void*algorithm;void*hash;
    BYTE object[4096] __attribute__((aligned(16)));BYTE digest[32];
} LidShopCatalogueHash;
static uint32_t lid_sc_checksum(const uint8_t*p,size_t n){
    uint32_t h=2166136261u;for(size_t i=0;i<n;i++)if(i<232||i>=236)h=(h^p[i])*16777619u;return h;
}
/* Actual CNG function pointers from existing platform binding; NEVER a fake
   hash callback. Partial failed releases retain resident handles/buffer and
   fail permanently. No re-use or speculative second close. */
static int lid_sc_hash(LidShopCatalogueHash*h,const R105Ops*o,const uint8_t*p,size_t n,uint32_t ui,uint32_t worker){
    DWORD got=0,object_size=0,digest_size=0;int ok=0;
    if(!h||h->attempted||!o||!o->io||!o->io->thread_id||!ui||!worker||ui==worker||
       o->io->thread_id()!=worker||!p||!n||n>LID_SC_MAX||!o->open_alg||!o->property||
       !o->create_hash||!o->hash_data||!o->finish_hash||!o->destroy_hash||!o->close_alg)return 0;
    h->attempted=1;
    if(o->open_alg(&h->algorithm,L"SHA256",0,0)<0||!h->algorithm||
       o->property(h->algorithm,L"ObjectLength",(BYTE*)&object_size,4,&got,0)<0||got!=4||!object_size||object_size>sizeof(h->object)||
       o->property(h->algorithm,L"HashDigestLength",(BYTE*)&digest_size,4,&got,0)<0||got!=4||digest_size!=32||
       o->create_hash(h->algorithm,&h->hash,h->object,object_size,0,0,0)<0||!h->hash||
       o->hash_data(h->hash,(BYTE*)p,(DWORD)n,0)<0||o->finish_hash(h->hash,h->digest,32,0)<0)goto release;
    ok=1;
release:
    if(h->hash){if(o->destroy_hash(h->hash)<0){h->failed=1;return 0;}h->hash=0;}
    if(h->algorithm){if(o->close_alg(h->algorithm,0)<0){h->failed=1;return 0;}h->algorithm=0;}
    h->failed=!ok;return ok;
}
/* Only private storage is written. Output publication is the final release;
   input bytes may be discarded afterwards. Caller owns distinct stable buffers. */
static int lid_sc_decode(LidShopRuntimeCatalogue*out,const uint8_t*p,size_t n,const R15Context*expected,const uint8_t expected_full[32],
                         uint32_t ui,uint32_t worker,LidShopCatalogueHash*hash,const R105Ops*crypto){
    if(!out||__atomic_load_n(&out->initialized,__ATOMIC_ACQUIRE)||!p||n<LID_SC_HEADER||n>LID_SC_MAX||
       !r15_context_valid(expected)||!expected_full||!r15_nonzero(expected_full,32)||
       (uintptr_t)p>UINTPTR_MAX-n||(uintptr_t)out>UINTPTR_MAX-sizeof(*out)||
       !((uintptr_t)p+n<=(uintptr_t)out||(uintptr_t)out+sizeof(*out)<=(uintptr_t)p)||
       !r15_equal(p,"LIDSHPC1",8)||r15_r32(p+8)!=1||r15_r32(p+12)!=n||
       r15_r32(p+232)!=lid_sc_checksum(p,n)||r15_nonzero(p+240,16))return 0;
    unsigned goal=r15_r32(p+236);if(goal<10||goal>40||goal%10)return 0;
    const LidShopCatalogueProfile*profile=&lid_sc_profiles[goal/10-1];
    if(r15_r32(p+224)!=profile->momoka||r15_r32(p+228)!=profile->tower||
       n!=256+profile->momoka*192+profile->tower*256||!r15_nonzero(p+128,32)||
       !r15_nonzero(p+160,32)||!r15_nonzero(p+192,32)||!r15_equal(p+128,expected_full,32))return 0;
    R15Context c={0};c.pid=r15_r32(p+16);c.client_pid=r15_r32(p+20);c.birth=r15_r64(p+24);
    r15_copy(c.nonce,p+32,16);r15_copy(c.writer,p+48,16);r15_copy(c.run,p+64,32);r15_copy(c.config,p+96,32);
    if(!r15_same_context(&c,expected)||!lid_sc_hash(hash,crypto,p+256,n-256,ui,worker)||
       !r15_equal(hash->digest,profile->digest,32))return 0;
    out->context=c;out->goal=goal;out->ui=ui;out->worker=worker;out->momoka_count=profile->momoka;out->tower_count=profile->tower;
    r15_copy(out->full,p+128,32);r15_copy(out->momoka_hash,p+160,32);r15_copy(out->tower_hash,p+192,32);r15_zero(out->allowed,512);
    for(unsigned i=0;i<profile->momoka;i++){
        const uint8_t*row=p+256+i*192;LidShopUiSku*s=&out->momoka[i];r15_zero(s,sizeof(*s));s->offer=r15_r32(row);
        if(s->offer>=26)return 0;
        unsigned units=0;while(units<129&&row[24+units]){if(row[24+units]<33||row[24+units]>126)return 0;s->skill[units]=row[24+units];units++;}
        if(!units||units>128)return 0;s->units=units;out->allowed[s->offer/8]|=(uint8_t)(1u<<(s->offer%8));
    }
    for(unsigned i=0;i<profile->tower;i++){
        const uint8_t*row=p+256+profile->momoka*192+i*256;LidShopRuntimeTower*s=&out->tower[i];
        s->offer=r15_r32(row);s->kind=r15_r32(row+4);s->location=r15_r64(row+8);s->floor=r15_r32(row+16);s->shopid=r15_r32(row+20);s->plan_slot=r15_r32(row+24);
        r15_copy(s->room_key,row+32,64);r15_copy(s->product,row+96,32);r15_copy(s->native_item,row+128,96);
    }
    __atomic_store_n(&out->initialized,1,__ATOMIC_RELEASE);return 1;
}
/* UI owner, after acquire of worker publication. No file/crypto/native call.
   Price comes from existing real UI observations, never the master descriptor. */
static int lid_sc_prepare_bridge(const LidShopRuntimeCatalogue*c,LidShopPurchaseBridge*b,uint32_t ui,
                                 void*provider,uint32_t(*thread)(void*),int(*live)(void*)){
    return c&&__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)&&ui==c->ui&&
      lid_pb_prepare(b,&c->context,c->momoka_hash,c->allowed,ui,provider,thread,live);
}
enum {LID_SC_WAITING=0,LID_SC_READY=1,LID_SC_FAILED=-1};
typedef struct LidShopCatalogueLoader {
    unsigned initialized;int state;uint32_t ui,worker;uint64_t next_poll,began;R15Context expected;uint8_t expected_full[32];
    R26Ops io;R105Ops crypto;HANDLE file;wchar_t path[1120];
    uint8_t bytes[LID_SC_MAX+1];LidShopCatalogueHash hash;LidShopRuntimeCatalogue catalogue;
} LidShopCatalogueLoader;
static int lid_sc_loader_prepare(LidShopCatalogueLoader*l,const R26Ops*io,const R105Ops*crypto,
    const wchar_t*directory,const R15Context*c,const uint8_t full[32],uint32_t ui,uint32_t worker){
    size_t n=0;
    if(!l||l->initialized||!r26_ops_valid(io)||!crypto||!directory||!r15_context_valid(c)||!full||!r15_nonzero(full,32)||
       !ui||!worker||ui==worker||io->thread_id()!=worker)return 0;
    while(n<1000&&directory[n])n++;
    if(n<3||n==1000||directory[1]!=':'||(directory[2]!='\\'&&directory[2]!='/')||
       !r26_path(l->path,1120,directory,L"shop_client_catalogue.bin"))return 0;
    l->io=*io;l->crypto=*crypto;l->crypto.io=&l->io;l->expected=*c;r15_copy(l->expected_full,full,32);
    l->ui=ui;l->worker=worker;l->began=io->clock();l->initialized=1;return 1;
}
/* Established background worker only. Missing producer waits at500ms intervals for at most120seconds;
   READY/FAILED is terminal and never re-reads/re-hashes. No game-thread loop. */
static int lid_sc_loader_poll(LidShopCatalogueLoader*l){
    if(!l||!l->initialized||l->io.thread_id()!=l->worker)return LID_SC_FAILED;
    if(l->state!=LID_SC_WAITING)return l->state;
    uint64_t now=l->io.clock();
    if(now<l->began||now-l->began>=120000){l->state=LID_SC_FAILED;return l->state;}
    if(l->next_poll&&now<l->next_poll)return LID_SC_WAITING;
    l->next_poll=now>UINT64_MAX-500?UINT64_MAX:now+500;
    l->file=l->io.open(l->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(l->file==INVALID_HANDLE_VALUE){l->file=0;DWORD error=l->io.error();if(error==2||error==3)return LID_SC_WAITING;l->state=LID_SC_FAILED;return l->state;}
    DWORD got=0;int read=l->io.read(l->file,l->bytes,sizeof(l->bytes),&got,0)!=0;
    if(!l->io.close(l->file)){l->state=LID_SC_FAILED;return l->state;}l->file=0;
    /* The fixed filename survives reconnect. Old or unknown producer files
       must not permanently poison a new handshake before its atomic publish.
       Do not hash/adopt any row until the complete current identity matches. */
    if(read&&got>=128){
        R15Context observed={0};const uint8_t*p=l->bytes;
        observed.pid=r15_r32(p+16);observed.client_pid=r15_r32(p+20);observed.birth=r15_r64(p+24);
        r15_copy(observed.nonce,p+32,16);r15_copy(observed.writer,p+48,16);
        r15_copy(observed.run,p+64,32);r15_copy(observed.config,p+96,32);
        if(!r15_same_context(&observed,&l->expected))return LID_SC_WAITING;
    }else if(read)return LID_SC_WAITING;
    l->state=read&&lid_sc_decode(&l->catalogue,l->bytes,got,&l->expected,l->expected_full,l->ui,l->worker,&l->hash,&l->crypto)?LID_SC_READY:LID_SC_FAILED;
    return l->state;
}
#endif
