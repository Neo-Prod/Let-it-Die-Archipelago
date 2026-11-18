#ifndef LID_SHOP_TOWER_DISPLAY_POINT_H
#define LID_SHOP_TOWER_DISPLAY_POINT_H
#include "shop_tower_display_view.h"
/* One chosen UI row only. Availability setter is called once per unit; do not
   rescan every native product for each of those callbacks. No native call/IO. */
typedef struct LidTowerDisplayPoint {unsigned logical,original,offer,flag;uint64_t location;} LidTowerDisplayPoint;
/* index==-1 uses native sorted cursor (script selected index may not yet have
   committed during pre-SetupItemInfoDispNew). 0 foreign/empty, -1 malformed
   known eligible context, 1 exact current AP BUY row. */
static int lid_tdp_resolve(LidTowerUiCurrent*c,uintptr_t receiver,int index,LidTowerDisplayPoint*out){
    if(!c||!c->prepared||!out||index< -1){r299_note(2,1,0);return 0;}
    const M195Ops*o=&c->meta->memory;LidTowerUiRoots r,again;uint64_t cls,vt;
    if(!lid_tuc_roots(c,&r)||r.item_select!=receiver||!m195_read(o,r.shop,0x50,&cls,8)||
       !m195_named(o,cls,"BrgUIMenu_SisterPartShop")){r299_note(2,2,0);return 0;}
    int32_t state,other;uint32_t ended;
    if(!m195_read(o,r.shop,c->meta->offsets[LID_TUM_STATE],&state,4)||
       !m195_read(o,r.shop,c->meta->offsets[LID_TUM_OTHER],&other,4)||
       !m195_read(o,r.shop,c->meta->offsets[LID_TUM_END],&ended,4)){r299_note(2,3,0);return -1;}
    if((int64_t)state-other<5||(int64_t)state-other>9||(ended&c->meta->end_mask)){r299_note(2,4,0);return 0;}
    if(!c->ops.live(c->ops.live_context)||!lid_tuc_sister(c,cls)||
       !m195_read(o,receiver,0,&vt,8)||vt!=c->meta->image+0x21e1550){r299_note(2,5,0);return -1;}
    char floor[32],area[32],room[64];unsigned n=0,j=0,count=0,shopid=0,offers[36];
    if(!lid_tuc_identifier(o,r.stage+c->floor+c->floor_id,floor)||!lid_tuc_identifier(o,r.stage+c->floor+c->floor_area,area)){r299_note(2,6,0);return -1;}
    while(floor[n]){room[n]=floor[n];n++;}room[n++]='|';while(area[j])room[n++]=area[j++];room[n]=0;
    for(unsigned i=0;i<184;i++){
        const LidTowerStockData*d=&lid_tower_stock_data[i];if(d->floor>c->catalogue->goal||!lid_ts_room(room,d->room))continue;
        if(count==36||(shopid&&shopid!=d->shopid)){r299_note(2,7,0);return -1;}shopid=d->shopid;offers[count++]=i;
    }
    if(!count){r299_note(2,8,0);return 0;}
    LidTSArray shops,full,p,indices,flags;int32_t original[36];uint8_t seen[36]={0};
    if(!lid_tuc_array(o,r.stage+c->floor+c->floor_shops,32,1,&shops)||shops.count!=1||
       !lid_tuc_array(o,(uint64_t)(uintptr_t)shops.data+c->shop_products,108,36,&full)||full.count!=(int32_t)count||
       !lid_tuc_array(o,r.shop+c->products,108,36,&p)||!lid_tuc_array(o,r.shop+c->indices,4,36,&indices)||p.count!=indices.count||
       !lid_tuc_array(o,r.game+c->flags,1,36,&flags)||flags.count!=(int32_t)count){r299_note(2,9,0);return -1;}
    if(!p.count){r299_note(2,10,0);return 0;}
    if(index== -1){
        int32_t raw;LidTSArray sorted;
        if(!m195_read(o,receiver,0x70,&raw,4)||!lid_tuc_array(o,receiver+0x254,4,36,&sorted)){r299_note(2,11,0);return -1;}
        if(raw<0||raw>=sorted.count){r299_note(2,12,0);return 0;}
        if(!m195_read(o,(uint64_t)(uintptr_t)sorted.data,(unsigned)raw*4,&index,4)){r299_note(2,13,0);return -1;}
    }
    if(index<0||index>=p.count){r299_note(2,14,0);return 0;} /* ordinary native empty padding */
    if(!m195_read(o,(uint64_t)(uintptr_t)indices.data,0,original,(unsigned)indices.count*4)){r299_note(2,15,0);return -1;}
    for(int i=0;i<indices.count;i++)if(original[i]<0||(unsigned)original[i]>=count||seen[original[i]]++){r299_note(2,16,0);return -1;}
    unsigned ordinal=(unsigned)original[index],offer=offers[ordinal];const LidTowerStockData*d=&lid_tower_stock_data[offer];uint8_t flag;
    if(!lid_tuc_product(o,(uint64_t)(uintptr_t)p.data+(unsigned)index*108,d)||
       !lid_tuc_product(o,(uint64_t)(uintptr_t)full.data+ordinal*108,d)||
       !m195_read(o,(uint64_t)(uintptr_t)flags.data,ordinal,&flag,1)||flag>1){r299_note(2,17,0);return -1;}
    LidTSArray p2,i2;int32_t check;
    if(!lid_tuc_roots(c,&again)||again.game!=r.game||again.manager!=r.manager||again.stage!=r.stage||again.shop!=r.shop||again.item_select!=receiver||
       !lid_tuc_array(o,r.shop+c->products,108,36,&p2)||!lid_tdv_headers(&p2,&p)||
       !lid_tuc_array(o,r.shop+c->indices,4,36,&i2)||!lid_tdv_headers(&i2,&indices)||
       !m195_read(o,(uint64_t)(uintptr_t)i2.data,(unsigned)index*4,&check,4)||check!=(int32_t)ordinal||
       !c->ops.live(c->ops.live_context)){r299_note(2,18,0);return -1;}
    *out=(LidTowerDisplayPoint){(unsigned)index,ordinal,offer,flag,d->location};{r299_note(2,19,0);return 1;}
}
#endif
