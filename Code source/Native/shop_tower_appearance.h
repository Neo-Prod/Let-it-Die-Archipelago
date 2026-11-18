#ifndef LID_SHOP_TOWER_APPEARANCE_H
#define LID_SHOP_TOWER_APPEARANCE_H
#include "shop_tower_icon_provider.h"
/* Observe actual loaded native master tables. No native data writes, timer
   reset, spawn hook, clock manipulation or disk-only readiness assertion. */
typedef uint8_t(*LidTowerConstRead)(const uint16_t*,int64_t*);
typedef int32_t(*LidTowerAppearanceRead)(const uint16_t*,const uint16_t*,const uint16_t*);
typedef struct LidTowerAppearance {
    LidTowerAPImageOps ops;const LidShopRuntimeCatalogue*catalogue;
    uint64_t image;unsigned goal,rooms,busy;int phase;
    /*0 empty,1 native ABI prepared,2 actual table values verified,-1 stopped*/
} LidTowerAppearance;
static int lid_tap_prepare(LidTowerAppearance*h,const LidTowerAPImageOps*ops,const LidShopRuntimeCatalogue*c,uint64_t image,int verified){
    static const uint32_t rvas[2]={0x12ccbd0,0x12506c0};
    static const uint8_t profiles[2][24]={
        {0x40,0x53,0x48,0x83,0xec,0x60,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x8b,0xda,0x4c,0x8b,0xc1,0x48,0x8d,0x54},
        {0x40,0x53,0x48,0x81,0xec,0x80,0,0,0,0x4d,0x8b,0xc8,0x4c,0x8b,0xc2,0x48,0x8b,0xd1,0x48,0x8d,0x4c,0x24,0x20,0xe8}};
    if(!h||h->phase||!ops||!ops->memory||!ops->thread||!ops->live||!ops->unlocked||!ops->owner||verified!=1||
       !c||!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE)||c->ui!=ops->owner||
       c->goal<10||c->goal>40||c->goal%10||!image||image>UINT64_MAX-0x12ccbe8||
       ops->thread(ops->context)!=ops->owner||ops->live(ops->context)!=1||ops->unlocked(ops->context)!=1)return 0;
    uint8_t bytes[24];
    for(unsigned i=0;i<2;i++){
        if(!m195_read(ops->memory,image,rvas[i],bytes,24))return 0;
        for(unsigned j=0;j<24;j++)if(bytes[j]!=profiles[i][j])return 0;
    }
    h->ops=*ops;h->catalogue=c;h->image=image;h->goal=c->goal;h->phase=1;return 1;
}
static int lid_tap_room(const char*room,uint16_t floor[32],uint16_t area[32]){
    unsigned n=0,i=0;if(!room)return 0;
    for(;room[i]&&room[i]!='|';i++){
        unsigned c=(uint8_t)room[i];if(n==31||!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;floor[n++]=(uint16_t)c;
    }
    if(!n||room[i]!='|')return 0;floor[n]=0;n=0;i++;
    for(;room[i];i++){
        unsigned c=(uint8_t)room[i];if(n==31||!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;area[n++]=(uint16_t)c;
    }
    if(!n)return 0;area[n]=0;return 1;
}
/* R292 cold observation only; no UObject cache or extra native query. */
typedef struct LidTowerAppearanceProbe {
    unsigned stage,room,timer_present;int64_t minutes;int32_t rate;
} LidTowerAppearanceProbe;
static LidTowerAppearanceProbe g_lid_tap_probe;
static int lid_tap_check(LidTowerAppearance*h){
    LidTowerAppearanceProbe*s=&g_lid_tap_probe;*s=(LidTowerAppearanceProbe){1,~0u,0,0,0};
    if(!h||h->phase<1||h->busy)return 0;
    s->stage=2;if(h->ops.thread(h->ops.context)!=h->ops.owner)return 0;
    s->stage=3;if(h->ops.live(h->ops.context)!=1)return 0;
    s->stage=4;if(h->ops.unlocked(h->ops.context)!=1)return 0;
    if(h->phase==2)return 1;
    volatile unsigned normal=0;int result=0;h->busy=1;
    __try{
        int64_t minutes=0;
        s->stage=10;
        s->timer_present=((LidTowerConstRead)(uintptr_t)(h->image+0x12ccbd0))((const uint16_t*)L"SHOP_APPEARANCE_TIME",&minutes);
        s->minutes=minutes;
        s->stage=11;if(!s->timer_present)goto end;
        s->stage=12;if(minutes!=1)goto end;
        const char*seen[7];unsigned rooms=0;
        for(unsigned i=0;i<184;i++){
            const LidTowerStockData*d=&lid_tower_stock_data[i];if(d->floor>h->goal)continue;
            unsigned duplicate=0;for(unsigned j=0;j<rooms;j++)if(lid_ts_room(seen[j],d->room))duplicate=1;
            if(duplicate)continue;if(rooms==7){result=-1;goto end;}
            uint16_t floor[32],area[32];if(!lid_tap_room(d->room,floor,area)){result=-1;goto end;}
            s->stage=20;s->room=i;
            int32_t rate=((LidTowerAppearanceRead)(uintptr_t)(h->image+0x12506c0))((const uint16_t*)L"4HMA",floor,area);
            s->rate=rate;s->stage=21;
            if(rate< -1||rate>100){result=-1;goto end;}s->stage=22;if(rate!=100)goto end;
            seen[rooms++]=d->room;
        }
        unsigned expected=h->goal==10?2:h->goal==20?3:h->goal==30?5:7;
        s->stage=30;if(rooms!=expected){result=-1;goto end;}h->rooms=rooms;result=1;s->stage=0;
end:normal=1;
    }__finally{
        h->busy=0;if(!normal||result<0)h->phase=-1;else if(result==1)h->phase=2;
    }
    return result;
}
#endif
