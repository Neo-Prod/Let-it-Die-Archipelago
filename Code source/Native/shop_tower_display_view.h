#ifndef LID_SHOP_TOWER_DISPLAY_VIEW_H
#define LID_SHOP_TOWER_DISPLAY_VIEW_H
#include "shop_tower_purchase_ui.h"
#include "shop_display.h"
/* Synchronous READ ONLY UI-event view. No slot publication, native call,
   allocation, file IO, stock/price/identity mutation or per-frame polling.
   Engine identities below are comparison tokens only after this call; never
   defer their dereference. Reward pointers refer only to immutable C storage. */
typedef struct LidTowerDisplayRow {
    unsigned logical,original,offer,unavailable;uint64_t location;
    const LidTowerStockData*carrier;const uint8_t*reward;
} LidTowerDisplayRow;
typedef struct LidTowerDisplayView {
    LidTowerUiRoots roots;LidTSArray products,indices;
    unsigned count;LidTowerDisplayRow rows[36];
} LidTowerDisplayView;
static int lid_tdv_headers(const LidTSArray*a,const LidTSArray*b){
    return a->data==b->data&&a->count==b->count&&a->capacity==b->capacity;
}
/* One bounded queue claim for the entire list. Busy/unready returns WAIT;
   occupied reservations and uncertain purchases must not appear buyable.
   Worker/native pointers are never dereferenced here. */
static int lid_tdv_bought(LidTowerQueue*q,uint8_t out[23]){
    uint32_t tid;int ok=0;if(!out||!lid_tq_lock(q,&tid))return 0;
    if(tid!=q->owner||!__atomic_load_n(&q->ready,__ATOMIC_ACQUIRE)||
       q->ops.session(q->ops.context)!=1)goto done;
    r15_copy(out,q->bought,23);
    for(unsigned i=0;i<64;i++){
        LidTowerQueueSlot*s=&q->slots[i];
        if(__atomic_load_n(&s->phase,__ATOMIC_ACQUIRE)!=LID_TQ_EMPTY){
            unsigned offer=s->observation.offer;
            if(offer>=184||!(q->allowed[offer/8]&(1u<<(offer%8))))goto done;
            out[offer/8]|=(uint8_t)(1u<<(offer%8));
        }
    }
    ok=1;
done:lid_tq_unlock(q);return ok;
}
/* 0 unrelated/not-yet-ready/busy, -1 malformed known AP tower view, 1 current.
   No native buy-state assumption: same resolver serves initial list creation
   and selection/rebuild events. Caller must separately prove BUY event scope
   (never a sell/DeathBag panel) before any future rendering mutation. */
static int lid_tdv_collect(LidTowerUiCurrent*c,LidTowerQueue*q,const LidShopDisplay*display,LidTowerDisplayView*out){
    if(!c||!c->prepared||!q||!q->initialized||!display||!out||
       q->ops.thread(q->ops.context)!=q->owner||!display->io.thread_id||display->io.thread_id()!=q->owner||
       __atomic_load_n(&display->state,__ATOMIC_ACQUIRE)!=LID_SD_READY||
       display->catalogue!=c->catalogue||!r15_same_context(&q->context,&c->catalogue->context)||
       !r15_equal(q->catalogue,c->catalogue->tower_hash,32)||!c->ops.live(c->ops.live_context)){r299_note(3,1,0);return 0;}
    const M195Ops*o=&c->meta->memory;LidTowerUiRoots r,again;uint64_t cls;
    if(!lid_tuc_roots(c,&r)||!m195_read(o,r.shop,0x50,&cls,8)||!m195_named(o,cls,"BrgUIMenu_SisterPartShop")){r299_note(3,2,0);return 0;}
    if(!lid_tuc_sister(c,cls)){r299_note(3,3,0);return -1;}
    char floor[32],area[32],room[64];unsigned n=0,j=0;
    if(!lid_tuc_identifier(o,r.stage+c->floor+c->floor_id,floor)||!lid_tuc_identifier(o,r.stage+c->floor+c->floor_area,area)){r299_note(3,4,0);return -1;}
    while(floor[n]){room[n]=floor[n];n++;}room[n++]='|';while(area[j])room[n++]=area[j++];room[n]=0;
    unsigned offers[36],count=0,shopid=0;
    for(unsigned i=0;i<184;i++){
        const LidTowerStockData*d=&lid_tower_stock_data[i];
        if(d->floor>c->catalogue->goal||!lid_ts_room(room,d->room))continue;
        if(count==36||(shopid&&shopid!=d->shopid)){r299_note(3,5,0);return -1;}
        shopid=d->shopid;offers[count++]=i;
    }
    if(!count){r299_note(3,6,0);return 0;}
    LidTSArray shops,full,products,indices,flags;int32_t original[36];uint8_t seen[36]={0},bought[23];
    if(!lid_tuc_array(o,r.stage+c->floor+c->floor_shops,32,1,&shops)||shops.count!=1||
       !lid_tuc_array(o,(uint64_t)(uintptr_t)shops.data+c->shop_products,108,36,&full)||full.count!=(int32_t)count||
       !lid_tuc_array(o,r.shop+c->products,108,36,&products)||!lid_tuc_array(o,r.shop+c->indices,4,36,&indices)||
       products.count!=indices.count||!lid_tuc_array(o,r.game+c->flags,1,36,&flags)||flags.count!=(int32_t)count){r299_note(3,7,0);return -1;}
    if(!lid_tdv_bought(q,bought)){r299_note(3,8,0);return 0;}
    /* A normal native sold-out filter can leave zero visible products. */
    if(indices.count&&!m195_read(o,(uint64_t)(uintptr_t)indices.data,0,original,(unsigned)indices.count*4)){r299_note(3,9,0);return -1;}
    LidTowerDisplayView view={0};view.roots=r;view.products=products;view.indices=indices;view.count=(unsigned)products.count;
    for(unsigned i=0;i<count;i++)if(!lid_tuc_product(o,(uint64_t)(uintptr_t)full.data+i*108,&lid_tower_stock_data[offers[i]])){r299_note(3,10,0);return -1;}
    for(unsigned i=0;i<view.count;i++){
        if(original[i]<0||(unsigned)original[i]>=count||seen[original[i]]++){r299_note(3,11,0);return -1;}
        unsigned ordinal=(unsigned)original[i],offer=offers[ordinal];const LidTowerStockData*d=&lid_tower_stock_data[offer];uint8_t flag;
        if(!lid_tuc_product(o,(uint64_t)(uintptr_t)products.data+i*108,d)||
           !m195_read(o,(uint64_t)(uintptr_t)flags.data,ordinal,&flag,1)||flag>1){r299_note(3,12,0);return -1;}
        const uint8_t*reward=lid_sd_row(display,d->location);
        if(!reward||r15_r32(reward+28)!=2||r15_r32(reward+32)!=offer){r299_note(3,13,0);return -1;}
        view.rows[i]=(LidTowerDisplayRow){i,ordinal,offer,(unsigned)(flag||(bought[offer/8]&(1u<<(offer%8)))),d->location,d,reward};
    }
    LidTSArray p2,i2,s2,f2,flags2;char fl2[32],ar2[32];
    if(!lid_tuc_roots(c,&again)||again.game!=r.game||again.manager!=r.manager||again.stage!=r.stage||
       again.shop!=r.shop||again.window!=r.window||again.item_select!=r.item_select||
       !lid_tuc_array(o,r.shop+c->products,108,36,&p2)||!lid_tdv_headers(&p2,&products)||
       !lid_tuc_array(o,r.shop+c->indices,4,36,&i2)||!lid_tdv_headers(&i2,&indices)||
       !lid_tuc_array(o,r.stage+c->floor+c->floor_shops,32,1,&s2)||!lid_tdv_headers(&s2,&shops)||
       !lid_tuc_array(o,(uint64_t)(uintptr_t)s2.data+c->shop_products,108,36,&f2)||!lid_tdv_headers(&f2,&full)||
       !lid_tuc_array(o,r.game+c->flags,1,36,&flags2)||!lid_tdv_headers(&flags2,&flags)||
       !lid_tuc_identifier(o,r.stage+c->floor+c->floor_id,fl2)||!lid_tuc_identifier(o,r.stage+c->floor+c->floor_area,ar2)||
       !c->ops.live(c->ops.live_context)){r299_note(3,14,0);return -1;}
    for(unsigned k=0;k<32;k++){if(fl2[k]!=floor[k]){r299_note(3,15,0);return -1;}if(!floor[k])break;}
    for(unsigned k=0;k<32;k++){if(ar2[k]!=area[k]){r299_note(3,16,0);return -1;}if(!area[k])break;}
    *out=view;{r299_note(3,17,0);return 1;}
}
/* Resolve native sorted cursor to logical row; no deferred engine instance.
   Recheck against a fresh view in the same event after any engine call. */
static int lid_tdv_selected(LidTowerUiCurrent*c,const LidTowerDisplayView*v,unsigned*out){
    const M195Ops*o=c?&c->meta->memory:0;int32_t raw,logical,selected;LidTSArray sorted,p2,i2;LidTowerUiRoots fresh;
    if(!c||!c->prepared||!v||!out||!v->count||!c->ops.live(c->ops.live_context)||
       !lid_tuc_roots(c,&fresh)||fresh.game!=v->roots.game||fresh.manager!=v->roots.manager||fresh.stage!=v->roots.stage||
       fresh.shop!=v->roots.shop||fresh.window!=v->roots.window||fresh.item_select!=v->roots.item_select||
       !lid_tuc_array(o,fresh.shop+c->products,108,36,&p2)||!lid_tdv_headers(&p2,&v->products)||
       !lid_tuc_array(o,fresh.shop+c->indices,4,36,&i2)||!lid_tdv_headers(&i2,&v->indices)||
       !m195_read(o,fresh.item_select,0x70,&raw,4)||
       !lid_tuc_array(o,fresh.item_select+0x254,4,36,&sorted)||raw<0||raw>=sorted.count||
       !m195_read(o,(uint64_t)(uintptr_t)sorted.data,(unsigned)raw*4,&logical,4)||logical<0||(unsigned)logical>=v->count||
       !m195_read(o,fresh.shop,c->meta->offsets[LID_TUM_SELECT],&selected,4)||selected!=logical)return 0;
    *out=(unsigned)logical;return 1;
}
#endif
