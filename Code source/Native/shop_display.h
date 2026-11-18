#ifndef LID_SHOP_DISPLAY_H
#define LID_SHOP_DISPLAY_H
#include "shop_runtime_catalogue.h"
#include "shop_display_icons.h"
#include "ui_item_colors.h"
/* Immutable AP display data, no rendering/hook/engine calls. Background owns
   its bytes; UI readers require READY acquire. Full selected location set and
   native representative keys checked, never use reward IDs as price carriers. */
enum {LID_SD_HEADER=256,LID_SD_ROW=1792,LID_SD_ROWS=210,LID_SD_MAX=376576};
enum {LID_SD_WAIT=0,LID_SD_READY=1,LID_SD_FAILED=-1};
typedef struct LidShopDisplay {
    unsigned initialized,count,slot;int state;uint32_t ui,worker;
    uint64_t began,next;R26Ops io;const LidShopRuntimeCatalogue*catalogue;
    HANDLE file;wchar_t path[1120];uint8_t bytes[LID_SD_MAX+1];
} LidShopDisplay;
static uint16_t lid_sd_u16(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t lid_sd_checksum(const uint8_t*p,size_t n){
    uint32_t h=2166136261u;for(size_t i=0;i<n;i++)if(i<168||i>=172)h=(h^p[i])*16777619u;return h;
}
static int lid_sd_wide(const uint8_t*p,unsigned units,unsigned capacity){
    if(!units||units>=capacity)return 0;
    for(unsigned i=0;i<units;i++){
        uint16_t c=lid_sd_u16(p+2*i);if(!c)return 0;
        if(c>=0xd800&&c<=0xdbff){
            if(++i>=units)return 0;uint16_t low=lid_sd_u16(p+2*i);if(low<0xdc00||low>0xdfff)return 0;
        }else if(c>=0xdc00&&c<=0xdfff)return 0;
    }
    return !r15_nonzero(p+2*units,(capacity-units)*2);
}
static const LidShopIconProfile*lid_sd_icon(uint64_t code){
    size_t lo=0,hi=LID_SI_COUNT;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;const LidShopIconProfile*p=&lid_si_profiles[mid];
        if(p->code==code)return p;if(p->code<code)lo=mid+1;else hi=mid;}
    return 0;
}
static int lid_sd_carrier(const LidShopRuntimeCatalogue*c,uint64_t location,unsigned scope,unsigned offer){
    if(scope==1){for(unsigned i=0;i<c->momoka_count;i++)if(c->momoka[i].offer==offer&&location==7500001u+offer)return 1;}
    else if(scope==2){for(unsigned i=0;i<c->tower_count;i++)if(c->tower[i].offer==offer&&c->tower[i].location==location)return 1;}
    return 0;
}
static int lid_sd_decode(LidShopDisplay*d,size_t n){
    const uint8_t*p=d->bytes;const LidShopRuntimeCatalogue*c=d->catalogue;R15Context current={0};
    if(n<LID_SD_HEADER||n>LID_SD_MAX||!r15_equal(p,"LIDSHUI1",8)||r15_r32(p+8)!=1||r15_r32(p+12)!=n||
       !r15_equal(p+128,c->full,32)||r15_r32(p+164)!=LID_SD_ROW||
       !r15_r32(p+172)||r15_r32(p+172)>INT32_MAX||r15_nonzero(p+176,80)||
       r15_r32(p+168)!=lid_sd_checksum(p,n))return 0;
    current.pid=r15_r32(p+16);current.client_pid=r15_r32(p+20);current.birth=r15_r64(p+24);
    r15_copy(current.nonce,p+32,16);r15_copy(current.writer,p+48,16);r15_copy(current.run,p+64,32);r15_copy(current.config,p+96,32);
    unsigned count=r15_r32(p+160);
    if(!r15_same_context(&current,&c->context)||!count||count>LID_SD_ROWS||
       count!=c->momoka_count+c->tower_count||n!=LID_SD_HEADER+count*LID_SD_ROW)return 0;
    uint64_t previous=0;
    for(unsigned i=0;i<count;i++){
        const uint8_t*row=p+LID_SD_HEADER+i*LID_SD_ROW;uint64_t location=r15_r64(row),item=r15_r64(row+8);
        unsigned receiver=r15_r32(row+16),flags=r15_r32(row+20),kind=r15_r32(row+24),scope=r15_r32(row+28),offer=r15_r32(row+32);
        unsigned name=r15_r32(row+36),player=r15_r32(row+40),game=r15_r32(row+44);
        if(location<=previous||!receiver||receiver>INT32_MAX||flags>7||kind>3||
           !lid_sd_carrier(c,location,scope,offer)||r15_nonzero(row+48,16)||r15_nonzero(row+1696,96)||
           !lid_sd_wide(row+160,name,512)||!lid_sd_wide(row+1184,player,128)||!lid_sd_wide(row+1440,game,128))return 0;
        previous=location;
        int local=game==10;static const char own[]="LET IT DIE";
        for(unsigned j=0;local&&j<10;j++)if(lid_sd_u16(row+1440+2*j)!=(uint16_t)own[j])local=0;
        const LidShopIconProfile*icon=local?lid_sd_icon(item):0;
        if(icon){if(kind!=icon->kind||!r15_equal(row+64,icon->key,96))return 0;}
        else if(kind||r15_nonzero(row+64,96))return 0;
    }
    d->count=count;d->slot=r15_r32(p+172);return 1;
}
static int lid_sd_prepare(LidShopDisplay*d,const R26Ops*io,const wchar_t*directory,const LidShopRuntimeCatalogue*c){
    if(!d||d->initialized||!r26_ops_valid(io)||!directory||!c||
       !__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||io->thread_id()!=c->worker||
       !r26_path(d->path,1120,directory,L"shop_client_display.bin"))return 0;
    d->io=*io;d->catalogue=c;d->ui=c->ui;d->worker=c->worker;d->began=io->clock();d->initialized=1;return 1;
}
static int lid_sd_poll(LidShopDisplay*d){
    if(!d||!d->initialized||d->io.thread_id()!=d->worker)return LID_SD_FAILED;
    int state=__atomic_load_n(&d->state,__ATOMIC_ACQUIRE);if(state!=LID_SD_WAIT)return state;
    uint64_t now=d->io.clock();
    if(now<d->began||now-d->began>=120000)goto fail;
    if(d->next&&now<d->next)return LID_SD_WAIT;d->next=now>UINT64_MAX-500?UINT64_MAX:now+500;
    d->file=d->io.open(d->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(d->file==INVALID_HANDLE_VALUE){d->file=0;DWORD error=d->io.error();if(error==2||error==3)return LID_SD_WAIT;goto fail;}
    DWORD got=0;int read=d->io.read(d->file,d->bytes,sizeof(d->bytes),&got,0)!=0;
    if(!d->io.close(d->file))goto fail;d->file=0;if(!read)goto fail;
    if(got<128)return LID_SD_WAIT;
    const uint8_t*p=d->bytes;const R15Context*c=&d->catalogue->context;
    if(r15_r32(p+16)!=c->pid||r15_r32(p+20)!=c->client_pid||r15_r64(p+24)!=c->birth||
       !r15_equal(p+32,c->nonce,16)||!r15_equal(p+48,c->writer,16)||
       !r15_equal(p+64,c->run,32)||!r15_equal(p+96,c->config,32))return LID_SD_WAIT;
    if(!lid_sd_decode(d,got))goto fail;
    __atomic_store_n(&d->state,LID_SD_READY,__ATOMIC_RELEASE);return LID_SD_READY;
fail:__atomic_store_n(&d->state,LID_SD_FAILED,__ATOMIC_RELEASE);return LID_SD_FAILED;
}
/* UI side: no file IO or borrowed engine memory. Decode only the chosen row.
   Returned byte pointer stays private/resident for this published Context. */
static const uint8_t*lid_sd_row(const LidShopDisplay*d,uint64_t location){
    if(!d||__atomic_load_n(&d->state,__ATOMIC_ACQUIRE)!=LID_SD_READY||d->io.thread_id()!=d->ui)return 0;
    unsigned lo=0,hi=d->count;
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;const uint8_t*p=d->bytes+LID_SD_HEADER+mid*LID_SD_ROW;
        uint64_t key=r15_r64(p);if(key==location)return p;if(key<location)lo=mid+1;else hi=mid;}
    return 0;
}
static int lid_sd_text(const LidShopDisplay*d,uint64_t location,unsigned field,uint16_t*out,unsigned capacity){
    const uint8_t*p=lid_sd_row(d,location);unsigned at,count;
    if(!p||!out||field>2)return 0;
    at=field==0?160:(field==1?1184:1440);count=r15_r32(p+36+4*field);
    if(count+1>capacity)return 0;for(unsigned i=0;i<count;i++)out[i]=lid_sd_u16(p+at+2*i);out[count]=0;return 1;
}
/* Color decoration is transient and never changes the display wire data. */
static int lid_sd_name(const LidShopDisplay*d,uint64_t location,uint16_t*out,unsigned capacity){
    const uint8_t*row=lid_sd_row(d,location);uint16_t raw[512];
    if(!row||!lid_sd_text(d,location,0,raw,512))return 0;
    return lid_ui_format(&g_lid_ui_colors,raw,r15_r32(row+36),lid_ui_kind(r15_r32(row+20)),out,capacity);
}
#endif
