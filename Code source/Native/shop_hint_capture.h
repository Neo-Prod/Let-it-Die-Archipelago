#ifndef LID_SHOP_HINT_CAPTURE_H
#define LID_SHOP_HINT_CAPTURE_H
#include "death_wire.h"
/* Scalar-only, optional observation channel. Never authorizes a transaction. */
enum {LID_SH_WORDS=7,LID_SH_BITS=210,LID_SH_SIZE=256};
typedef struct LidShopHintBus {
    unsigned initialized;uint32_t ui,worker;R15Context context;
    uint8_t catalogue[32];uint32_t allowed[LID_SH_WORDS],seen[LID_SH_WORDS];
} LidShopHintBus;
static LidShopHintBus g_lid_shop_hints;
static int lid_sh_init(LidShopHintBus*b,const R15Context*c,const uint8_t hash[32],
                       uint32_t ui,uint32_t worker,const uint32_t allowed[LID_SH_WORDS]){
    if(!b||__atomic_load_n(&b->initialized,__ATOMIC_ACQUIRE)||!r15_context_valid(c)||!hash||
       !r15_nonzero(hash,32)||!ui||!worker||ui==worker||!allowed||
       (allowed[6]&0xfffc0000u)||!r15_nonzero(allowed,LID_SH_WORDS*4))return 0;
    b->context=*c;b->ui=ui;b->worker=worker;r15_copy(b->catalogue,hash,32);
    for(unsigned i=0;i<LID_SH_WORDS;i++){b->allowed[i]=allowed[i];b->seen[i]=0;}
    __atomic_store_n(&b->initialized,1,__ATOMIC_RELEASE);return 1;
}
/* Entire list admitted before any publication. UI event only, no engine/IO. */
static int lid_sh_observe(LidShopHintBus*b,const R15Context*c,const uint8_t hash[32],
                          uint32_t tid,unsigned scope,const unsigned*offers,unsigned count){
    if(!b||!__atomic_load_n(&b->initialized,__ATOMIC_ACQUIRE)||tid!=b->ui||!c||!hash||
       !r15_same_context(c,&b->context)||!r15_equal(hash,b->catalogue,32)||
       (scope!=1&&scope!=2)||count>(scope==1?26u:36u)||(count&&!offers))return 0;
    uint32_t mask[LID_SH_WORDS]={0};
    for(unsigned i=0;i<count;i++){
        if(offers[i]>=(scope==1?26u:184u))return 0;
        unsigned bit=offers[i]+(scope==1?0:26),word=bit/32;uint32_t flag=1u<<(bit%32);
        if(!(b->allowed[word]&flag)||(mask[word]&flag))return 0;mask[word]|=flag;
    }
    for(unsigned i=0;i<LID_SH_WORDS;i++)if(mask[i])__atomic_fetch_or(&b->seen[i],mask[i],__ATOMIC_RELEASE);
    return 1;
}
static uint32_t lid_sh_checksum(const uint8_t raw[LID_SH_SIZE]){
    uint32_t h=2166136261u;for(unsigned i=0;i<LID_SH_SIZE;i++)if(i<196||i>=200)h=(h^raw[i])*16777619u;return h;
}
static int lid_sh_snapshot(const LidShopHintBus*b,uint32_t tid,uint32_t mask[LID_SH_WORDS]){
    if(!b||!mask||!__atomic_load_n(&b->initialized,__ATOMIC_ACQUIRE)||tid!=b->worker)return 0;
    for(unsigned i=0;i<LID_SH_WORDS;i++)mask[i]=__atomic_load_n(&b->seen[i],__ATOMIC_ACQUIRE);
    return 1;
}
static int lid_sh_encode(const LidShopHintBus*b,uint32_t tid,const uint32_t mask[LID_SH_WORDS],
                         uint64_t sequence,uint8_t raw[LID_SH_SIZE]){
    if(!b||!raw||!mask||!sequence||!__atomic_load_n(&b->initialized,__ATOMIC_ACQUIRE)||tid!=b->worker)return 0;
    for(unsigned i=0;i<LID_SH_WORDS;i++)if(mask[i]&~b->allowed[i])return 0;
    if(!r15_nonzero(mask,LID_SH_WORDS*4))return 0;
    r15_zero(raw,LID_SH_SIZE);r15_copy(raw,"LIDSHNT1",8);r15_w32(raw+8,1);r15_w32(raw+12,LID_SH_SIZE);
    const R15Context*c=&b->context;r15_w32(raw+16,c->pid);r15_w32(raw+20,c->client_pid);r15_w64(raw+24,c->birth);
    r15_copy(raw+32,c->nonce,16);r15_copy(raw+48,c->writer,16);r15_copy(raw+64,c->run,32);r15_copy(raw+96,c->config,32);
    r15_copy(raw+128,b->catalogue,32);for(unsigned i=0;i<LID_SH_WORDS;i++)r15_w32(raw+160+4*i,mask[i]);
    r15_w64(raw+188,sequence);r15_w32(raw+196,lid_sh_checksum(raw));return 1;
}
#endif
