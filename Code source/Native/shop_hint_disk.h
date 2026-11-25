#ifndef LID_SHOP_HINT_DISK_H
#define LID_SHOP_HINT_DISK_H
#include "checks_disk_io.h"
#include "shop_hint_capture.h"
#include "shop_runtime_catalogue.h"
typedef struct LidShopHintDisk {
    unsigned ready,failed,failures;uint64_t sequence,attempt,next;
    uint32_t committed[LID_SH_WORDS];wchar_t path[1120],temporary[1280];
} LidShopHintDisk;
static LidShopHintDisk g_lid_shop_hint_disk;
/* Existing worker only; changed bitmaps only. No added thread or UI file IO. */
static int lid_sh_worker(R26Disk*parent,const wchar_t*directory,const LidShopRuntimeCatalogue*c,uint64_t now){
    LidShopHintDisk*d=&g_lid_shop_hint_disk;LidShopHintBus*b=&g_lid_shop_hints;
    if(!parent||!parent->lease||!directory||!c||!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||
       parent->ops.thread_id()!=c->worker||d->failed)return 0;
    if(!d->ready){
        uint32_t allowed[LID_SH_WORDS]={0};
        for(unsigned i=0;i<c->momoka_count;i++){
            unsigned offer=c->momoka[i].offer;if(offer>=26)return 0;allowed[offer/32]|=1u<<(offer%32);
        }
        for(unsigned i=0;i<c->tower_count;i++){
            unsigned offer=c->tower[i].offer;if(offer>=184)return 0;unsigned bit=offer+26;allowed[bit/32]|=1u<<(bit%32);
        }
        if(!r26_path(d->path,1120,directory,L"shop_native_hints.bin")||
           !lid_sh_init(b,&c->context,c->full,c->ui,c->worker,allowed)){d->failed=1;return 0;}
        d->ready=1;
    }
    if(!r15_same_context(&b->context,&c->context)||!r15_equal(b->catalogue,c->full,32)||now<d->next)return 0;
    uint32_t mask[LID_SH_WORDS];if(!lid_sh_snapshot(b,c->worker,mask))return 0;
    if(r15_equal(mask,d->committed,sizeof(mask)))return 1;
    uint8_t raw[LID_SH_SIZE];
    if(d->sequence==UINT64_MAX||d->attempt==UINT64_MAX||!lid_sh_encode(b,c->worker,mask,d->sequence+1,raw)){d->failed=1;return 0;}
    d->temporary[0]=0;
    if(!r26_append(d->temporary,1280,parent->stem)||!r26_append(d->temporary,1280,L"shop-hint-")||
       !r26_hex(d->temporary,1280,++d->attempt,16)){d->failed=1;return 0;}
    R26Ops*o=&parent->ops;HANDLE f=o->open(d->temporary,GENERIC_WRITE,0,0,1,FILE_ATTRIBUTE_NORMAL,0);
    int ok=0;
    if(f!=INVALID_HANDLE_VALUE){
        DWORD wrote=0;ok=o->write(f,raw,LID_SH_SIZE,&wrote,0)&&wrote==LID_SH_SIZE;
        if(ok)ok=o->flush(f)!=0;int closed=o->close(f)!=0;if(!closed)ok=0;
        if(ok)ok=o->replace(d->temporary,d->path,9)!=0;
        if(!ok&&closed)o->remove(d->temporary);
    }
    if(ok){r15_copy(d->committed,mask,sizeof(mask));d->sequence++;d->failures=0;d->next=now>UINT64_MAX-250?UINT64_MAX:now+250;return 1;}
    if(++d->failures>=8)d->failed=1;d->next=now>UINT64_MAX-5000?UINT64_MAX:now+5000;return 0;
}
#endif
