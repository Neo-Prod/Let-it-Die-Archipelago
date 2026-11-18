#ifndef LID_SHOP_TOWER_POLICY_CALLS_H
#define LID_SHOP_TOWER_POLICY_CALLS_H
/* Exact merchant CALL owners. Original repository/SQLite and expiry stay intact.
   Included after R267 platform helpers. Snapshot/relays remain resident. */
typedef struct R296Policy {
    R267Tower*host;R15Context session;unsigned goal,count,state,touched,dirty,active;
    uint16_t floors[7][32],areas[7][32];uintptr_t relays[2];
    uint8_t patch[2][5];unsigned protection[2];
} R296Policy;
static R296Policy g_r296_policy;
static const unsigned r296_sites[2]={0x1361db5,0x11f1580};
static const uint8_t r296_original[2][10]={
    {0xe8,0x16,0xae,0xf6,0xff,0xe8,0xa1,0x48,0x8e,0xff},
    {0xe8,0x3b,0xf1,0x05,0x00,0x8b,0xf0,0x8b,0x0d,0x43}};
static int r296_live(R296Policy*p){
    R26Transport*t=&g_checks_delivery;
    return __atomic_load_n(&p->active,__ATOMIC_ACQUIRE)&&p->host&&
        !__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)&&
        __atomic_load_n(&t->state,__ATOMIC_ACQUIRE)==R26_ACTIVE&&
        lid_sm_mode(&t->shop_policy)==LID_SM_ENABLED&&
        __atomic_load_n(&t->shop_catalogue_result,__ATOMIC_ACQUIRE)==LID_SC_READY&&
        __atomic_load_n(&t->shop_catalogue.catalogue.initialized,__ATOMIC_ACQUIRE)&&
        t->shop_catalogue.catalogue.goal==p->goal&&
        r15_same_context(&p->session,&t->queue.context)&&
        r15_same_context(&p->session,&t->shop_catalogue.catalogue.context);
}
static int r296_equal(R296Policy*p,const uint16_t*value,const uint16_t*expected){
    if(!value||!expected)return 0;
    for(unsigned i=0;i<32;i++){
        uint16_t c;if(!r267_read(p->host,(uintptr_t)value+i*2,&c,2)||c!=expected[i])return 0;
        if(!c)return 1;
    }return 0;
}
static uint8_t r296_minutes(const uint16_t*key,int64_t*out){
    R296Policy*p=&g_r296_policy;R267Tower*h=p->host;
    uint8_t result=((LidTowerConstRead)((uintptr_t)h->parent->image+0x12ccbd0))(key,out);
    uint32_t error=r267_error(h);
    if(result&&out&&r296_live(p)&&r296_equal(p,key,(const uint16_t*)L"SHOP_APPEARANCE_TIME"))*out=1;
    r267_error_set(h,error);return result;
}
static int32_t r296_rate(const uint16_t*rotation,const uint16_t*floor,const uint16_t*area){
    R296Policy*p=&g_r296_policy;R267Tower*h=p->host;
    int32_t result=((LidTowerAppearanceRead)((uintptr_t)h->parent->image+0x12506c0))(rotation,floor,area);
    uint32_t error=r267_error(h);
    if(result>=-1&&result<=100&&r296_live(p)&&r296_equal(p,rotation,(const uint16_t*)L"4HMA")){
        for(unsigned i=0;i<p->count;i++)if(r296_equal(p,floor,p->floors[i])&&r296_equal(p,area,p->areas[i])){result=100;break;}
    }r267_error_set(h,error);return result;
}
static int r296_match(R296Policy*p,unsigned i,int patched){
    uint8_t bytes[10];if(i>=2||!r267_read(p->host,(uintptr_t)p->host->parent->image+r296_sites[i],bytes,10))return 0;
    for(unsigned j=0;j<10;j++)if(bytes[j]!=(patched&&j<5?p->patch[i][j]:r296_original[i][j]))return 0;
    return 1;
}
static int r296_owned(R267Tower*h){
    R296Policy*p=&g_r296_policy;return p->host==h&&p->state==2&&p->touched==3&&!p->dirty&&
        p->goal==h->goal&&r296_live(p)&&r296_match(p,0,1)&&r296_match(p,1,1);
}
static int r296_write(R296Policy*p,unsigned i,const uint8_t*bytes){
    R267Tower*h=p->host;unsigned old,ignored;uintptr_t at=(uintptr_t)h->parent->image+r296_sites[i];
    if(!lid_momoka_install_held(&h->guard)||!r267_protect(h,at,5,0x40,&old))return 0;
    if(!(p->dirty&(1u<<i)))p->protection[i]=old;
    p->dirty|=1u<<i;
    for(unsigned j=0;j<5;j++)((uint8_t*)at)[j]=bytes[j];
    int flushed=r267_flush(h,at,5),restored=r267_protect(h,at,5,p->protection[i],&ignored);
    if(restored)p->dirty&=~(1u<<i);return flushed&&restored;
}
static int r296_rollback(R296Policy*p){
    __atomic_store_n(&p->active,0,__ATOMIC_RELEASE);int ok=1;
    for(unsigned n=2;n;n--){unsigned i=n-1,bit=1u<<i;if(!(p->touched&bit))continue;
        if(r296_match(p,i,1)){if(!r296_write(p,i,r296_original[i])||!r296_match(p,i,0)){ok=0;continue;}}
        else if(!r296_match(p,i,0)){ok=0;continue;}
        if(p->dirty&bit){unsigned ignored;
            if(!r267_protect(p->host,(uintptr_t)p->host->parent->image+r296_sites[i],5,p->protection[i],&ignored)){ok=0;continue;}
            p->dirty&=~bit;
        }p->touched&=~bit;
    }return ok&&!p->touched&&!p->dirty;
}
static int r296_prepare(R267Tower*h){
    R296Policy*p=&g_r296_policy;if(p->state||!r267_live(h)||!r267_unlocked(h)||!r254_verified(h->parent))return 0;
    p->host=h;p->goal=h->goal;p->session=h->context;
    const char*seen[7];
    for(unsigned i=0;i<184;i++){
        const LidTowerStockData*d=&lid_tower_stock_data[i];if(d->floor>p->goal)continue;
        unsigned duplicate=0;for(unsigned j=0;j<p->count;j++)if(lid_ts_room(seen[j],d->room))duplicate=1;
        if(duplicate)continue;
        if(p->count==7||!lid_tap_room(d->room,p->floors[p->count],p->areas[p->count]))goto fail;
        seen[p->count++]=d->room;
    }
    if(p->count!=(p->goal==10?2:p->goal==20?3:p->goal==30?5:7))goto fail;
    uintptr_t wrappers[2]={(uintptr_t)r296_minutes,(uintptr_t)r296_rate};
    for(unsigned i=0;i<2;i++){
        if(!r296_match(p,i,0)||!r254_executable(h->parent,wrappers[i]))goto fail;
        uintptr_t at=(uintptr_t)h->parent->image+r296_sites[i];
        if(lid_tc_relative(at,wrappers[i],p->patch[i]))continue;
        uintptr_t relay=(uintptr_t)r254_near(h->parent,at,14);p->relays[i]=relay;
        MEMORY_BASIC_INFORMATION m;if(!relay||!r267_region(relay,14,&m)||m.Type!=0x20000u||
           (uintptr_t)m.AllocationBase!=relay||(m.Protect&0xff)!=4)goto fail;
        uint8_t code[14]={0xff,0x25,0,0,0,0};for(unsigned j=0;j<8;j++)code[j+6]=(uint8_t)((uint64_t)wrappers[i]>>(8*j));
        for(unsigned j=0;j<14;j++)((uint8_t*)relay)[j]=code[j];
        unsigned old;if(!r267_protect(h,relay,14,0x20,&old)||!r267_flush(h,relay,14)||
            !r254_executable(h->parent,relay)||!lid_tc_relative(at,relay,p->patch[i]))goto fail;
        uint8_t check[14];if(!r267_read(h,relay,check,14))goto fail;
        for(unsigned j=0;j<14;j++)if(check[j]!=code[j])goto fail;
    }p->state=1;return 1;
fail:p->state=3;return 0;
}
static int r296_publish(R267Tower*h){
    R296Policy*p=&g_r296_policy;if(p->host!=h||p->state!=1||!r267_live(h))return LID_MI_INVALID;
    int admitted=lid_momoka_install_begin(&h->guard);if(admitted!=LID_MI_HELD)return admitted;
    volatile unsigned normal=0;int ok=0,released=0;
    __try{
        if(!r296_match(p,0,0)||!r296_match(p,1,0))goto done;
        for(unsigned i=0;i<2;i++){
            p->touched|=1u<<i;
            if(!r296_write(p,i,p->patch[i])||!r296_match(p,i,1))goto done;
        }ok=1;
done:normal=1;
    }__finally{
        if(!normal||!ok){(void)r296_rollback(p);p->state=3;}
        released=lid_momoka_install_end(&h->guard);
        if(!released){__atomic_store_n(&p->active,0,__ATOMIC_RELEASE);p->state=3;}
    }
    if(!ok||!released)return LID_MI_INVALID;
    p->state=2;__atomic_store_n(&p->active,1,__ATOMIC_RELEASE);return LID_MI_HELD;
}
/* Same real wrappers and immutable whitelist as the two installed CALLs.
   This proves EFFECTIVE policy, not that the original SQLite cache changed. */
static int r296_check(R267Tower*h){
    R296Policy*p=&g_r296_policy;if(!r296_owned(h))return 0;
    int64_t minutes=0;if(!r296_minutes((const uint16_t*)L"SHOP_APPEARANCE_TIME",&minutes)||minutes!=1)return 0;
    for(unsigned i=0;i<p->count;i++)if(r296_rate((const uint16_t*)L"4HMA",p->floors[i],p->areas[i])!=100)return 0;
    if(!r296_owned(h))return 0;h->appearance.rooms=p->count;h->appearance.phase=2;return 1;
}
#endif
