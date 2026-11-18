#ifndef LID_SHOP_TOWER_BACKEND_H
#define LID_SHOP_TOWER_BACKEND_H
#include "shop_tower_sale.h"
#include "shop_tower_request.h"
#include "shop_tower_handler_route.h"
#include "shop_tower_no_item.h"
/* One final-address resident old-run host. Sole1167730 route supplies request
   lifetime; no worker UObject traversal, fabricated admission, new detour,
   journal/AP send, retry/refund or field writes to source JSON/game Save. */
typedef struct LidTowerBackendOps {
    void*context;uint32_t(*thread)(void*);uint32_t(*error)(void*);void(*error_set)(void*,uint32_t);
    int(*session)(void*); /* pinned C identity, safe on request worker */
    int(*quiet)(void*);int(*coherent)(void*);int(*owned_call)(void*,uintptr_t);
} LidTowerBackendOps;
typedef struct LidTowerBackend {
    unsigned prepared,binding,stopped,completed;uint32_t owner;int uid;
    uint64_t next,ticket;uintptr_t api,response,root;unsigned cursor,current;
    LidTowerBackendOps ops;M195Ops memory;LidTowerQueue*queue;
    LidTowerSale sale;LidShopMarkerNative json;LidTowerRequest request;
    LidTowerPayment payment;LidTowerNoItem no_item;LidTowerHandlerRoute route;
    uint64_t tokens[128];uint8_t used[128];
} LidTowerBackend;
static uint32_t lid_tb_thread(void*context){LidTowerBackend*h=context;return h->ops.thread(h->ops.context);}
static uint32_t lid_tb_error(void*context){LidTowerBackend*h=context;return h->ops.error(h->ops.context);}
static void lid_tb_error_set(void*context,uint32_t error){LidTowerBackend*h=context;h->ops.error_set(h->ops.context,error);}
static int lid_tb_copy(void*context,uintptr_t at,void*out,size_t size){
    LidTowerBackend*h=context;return at&&size&&at<=UINTPTR_MAX-size&&m195_read(&h->memory,at,0,out,size);
}
static int lid_tb_api(LidTowerBackend*h,uintptr_t api){
    uintptr_t vt,slot;return api&&lid_tb_copy(h,api,&vt,8)&&vt==h->payment.native.image+0x2112f60&&
        lid_tb_copy(h,vt+0x80,&slot,8)&&slot==h->payment.native.image+0x115d510;
}
static int lid_tb_lease(void*context){
    LidTowerBackend*h=context;uint32_t tid=lid_tb_thread(h);
    return tid&&__atomic_load_n(&h->owner,__ATOMIC_ACQUIRE)==tid&&tid!=h->queue->owner&&tid!=h->queue->worker&&
        h->ticket&&h->api&&h->root&&h->ops.session(h->ops.context)==1&&lid_tb_api(h,h->api);
}
static int lid_tb_json_window(void*context,const void*root,int write){
    LidTowerBackend*h=context;
    if(write)return 0;
    if(!root)return h->binding&&lid_tb_thread(h)==h->queue->owner&&h->ops.session(h->ops.context)==1;
    return lid_tb_lease(h); /* native JSON adapter separately checks exact vtable */
}
static int lid_tb_json_copy(void*context,const void*at,void*out,size_t size){return lid_tb_copy(context,(uintptr_t)at,out,size);}
static int lid_tb_offer(void*context,const uint16_t*item,unsigned units,unsigned*out){
    LidTowerBackend*h=context;
    if(h->sale.active)return lid_tsa_offer(&h->sale,item,units,out);
    /* A failed/retired native floor read is NOT permission to grant a known
       AP carrier. Genuine unknown native rows may still follow native flow. */
    for(unsigned i=0;i<184;i++)if((h->queue->allowed[i/8]&(1u<<(i%8)))&&lid_tp_item(item,units,lid_tower_stock_data[i].item))return -1;
    return 0;
}
static int lid_tb_window(void*context,uint64_t token,unsigned offer,int uid){
    LidTowerBackend*h=context;
    return offer<184&&lid_tb_lease(h)&&!h->stopped&&uid==h->uid&&h->request.ready&&h->current<h->request.count&&
        h->tokens[h->current]==token&&h->used[h->current]&&h->request.rows[h->current].ap&&
        h->request.rows[h->current].offer==offer&&h->sale.checked[offer]&&lid_tsa_same(&h->sale);
}
static int lid_tb_owned(void*context,uintptr_t caller){
    LidTowerBackend*h=context;return h->ops.owned_call(h->ops.context,caller)==1;
}
static int lid_tb_adopt(void*context,int uid,const wchar_t*item,uint64_t*out){
    LidTowerBackend*h=context;uint16_t text[96];unsigned units=0;
    if(!out||!item){r299_note(7,1,0);return -1;}
    for(;units<96;units++)if(!lid_tb_copy(h,(uintptr_t)item+units*2,&text[units],2)){r299_note(7,2,0);return -1;}else if(!text[units])break;
    if(!units||units==96){r299_note(7,3,0);return -1;}
    if(!lid_tb_lease(h)||!h->request.ready){
        unsigned unused;{r299_note(7,4,0);return lid_tb_offer(h,text,units,&unused)==0?0:-1;}
    }
    unsigned row=h->cursor;
    while(row<h->request.count&&!h->request.rows[row].buy)row++;
    if(row==h->request.count){r299_note(7,5,0);return -1;}
    LidTowerRequestRow*r=&h->request.rows[row];
    if(r->units!=units){r299_note(7,6,0);return -1;}for(unsigned i=0;i<units;i++)if(text[i]!=r->item[i]){r299_note(7,7,0);return -1;}
    h->cursor=row+1;
    if(!r->ap){r299_note(7,8,0);return 0;}
    unsigned refused=0;
    if(uid!=h->uid)refused=31;
    else if(h->stopped)refused=32;
    else if(h->used[row])refused=33;
    else if(!h->tokens[row])refused=34;
    else if(!lid_tsa_validate(&h->sale,r->offer))refused=35;
    if(refused){r299_note(7,refused,r->offer);r299_note(7,9,0);return -1;}
    h->used[row]=1;h->current=row;*out=h->tokens[row];{r299_note(7,10,0);return 1;}
}
static unsigned long long lid_tb_reject(void*context,void*api,void*response){
    LidTowerBackend*h=context;uintptr_t vt,target;
    if(!lid_tb_api(h,(uintptr_t)api)||!lid_tb_copy(h,(uintptr_t)api,&vt,8)||
       !lid_tb_copy(h,vt+0x68,&target,8)||target!=h->payment.native.image+0x10fd3b0)return 0;
    return ((unsigned long long(*)(void*,void*))target)(api,response);
}
static void lid_tb_end(void*context,uint64_t ticket,unsigned returned){
    LidTowerBackend*h=context;uint32_t tid=lid_tb_thread(h);
    if(!ticket||__atomic_load_n(&h->owner,__ATOMIC_ACQUIRE)!=tid||h->ticket!=ticket)return;
    if(!returned||!h->completed)for(unsigned i=0;i<128;i++)if(h->tokens[i]){
        lid_tp_fail(&h->payment,h->tokens[i]);h->stopped=1;
    }
    lid_tsa_end(&h->sale);h->api=h->response=h->root=0;h->ticket=0;h->request.ready=0;
    h->current=128;__atomic_store_n(&h->owner,0,__ATOMIC_RELEASE);
}
static int lid_tb_begin(void*context,void*api,void*response,void*root,uint64_t*ticket){
    LidTowerBackend*h=context;uint32_t tid=lid_tb_thread(h),empty=0;
    if(!ticket||!lid_tb_api(h,(uintptr_t)api)){r299_note(6,1,0);return 0;}
    if(!tid||tid==h->queue->owner||tid==h->queue->worker||h->next==UINT64_MAX||
       !__atomic_compare_exchange_n(&h->owner,&empty,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){r299_note(6,2,0);return -1;}
    h->ticket=++h->next;*ticket=h->ticket;h->api=(uintptr_t)api;h->response=(uintptr_t)response;h->root=(uintptr_t)root;
    h->completed=0;h->cursor=0;h->current=128;
    __builtin_memset(h->tokens,0,sizeof(h->tokens));__builtin_memset(h->used,0,sizeof(h->used));
    int sale=lid_tsa_begin(&h->sale);r299_note(6,sale?11:10,0);
    LidTowerRequestOps offers={h,lid_tb_offer};
    int status=lid_tr_copy(&h->json,root,&offers,&h->request);
    if(status==0){h->completed=1;lid_tb_end(h,h->ticket,1);*ticket=0;{r299_note(6,3,0);return 0;}}
    if(status!=1){r299_note(6,12,(unsigned)(status+1));r299_note(6,4,0);return -1;}
    if(h->stopped){r299_note(6,13,0);r299_note(6,4,0);return -1;}
    if(h->ops.session(h->ops.context)!=1){r299_note(6,14,0);r299_note(6,4,0);return -1;}
    for(unsigned i=0;i<h->request.count;i++)if(h->request.rows[i].ap){
        LidTowerRequestRow*r=&h->request.rows[i];
        if(!lid_tp_claim(&h->payment,r->offer,r->item,r->units,&h->tokens[i])){r299_note(6,5,0);return -1;}
    }
    {r299_note(6,6,0);return 1;}
}
static void lid_tb_complete(void*context,uint64_t ticket,unsigned char result){
    LidTowerBackend*h=context;
    if(!lid_tb_lease(h)||ticket!=h->ticket){r299_note(9,1,0);return;}
    if(result!=1){h->stopped=1;{r299_note(9,2,0);return;}}
    /* Validate every expected helper completed BEFORE publishing ANY row.
       No AP send/disk write here; READY is proven native evidence only. */
    for(unsigned i=0;i<h->request.count;i++)if(h->request.rows[i].ap){
        unsigned index;
        if(!h->used[i]||!h->tokens[i]||!lid_tp_owned(&h->payment,h->tokens[i],LID_TQ_PAID,&index)){
            h->stopped=1;{r299_note(9,3,0);return;}
        }
    }
    for(unsigned i=0;i<h->request.count;i++)if(h->request.rows[i].ap&&!lid_tp_finish(&h->payment,h->tokens[i],result)){
        h->stopped=1;{r299_note(9,4,0);return;}
    }
    h->completed=1;
}
static int lid_tb_quiet(void*context){LidTowerBackend*h=context;return h->ops.quiet(h->ops.context);}
static int lid_tb_coherent(void*context){LidTowerBackend*h=context;return !h->stopped&&h->ops.coherent(h->ops.context)==1;}
static int lid_tb_prepare(LidTowerBackend*h,LidTowerQueue*q,const LidTowerFloor*f,const LidTowerPaymentNative*n,
                           const M195Ops*m,int uid,int exact_sha,const LidTowerBackendOps*ops){
    if(!h||h->prepared||!q||!q->initialized||!f||!f->prepared||!n||!n->prepared||!m||!ops||
       !ops->thread||ops->thread(ops->context)!=q->owner||!ops->session||!ops->quiet||!ops->coherent||
       !ops->owned_call||!ops->error||!ops->error_set||ops->session(ops->context)!=1||exact_sha!=1)return 0;
    h->ops=*ops;h->memory=*m;h->queue=q;h->uid=uid;h->current=128;
    LidTowerPaymentOps payment={h,lid_tb_window,lid_tb_copy};
    LidTowerNoItemOps noitem={h,lid_tb_owned,lid_tb_adopt};
    LidShopMarkerAccess json={h,lid_tb_json_window,lid_tb_json_copy};
    LidTowerHandlerOps route={h,lid_tb_thread,lid_tb_error,lid_tb_error_set,lid_tb_quiet,lid_tb_coherent,
        lid_tb_begin,lid_tb_reject,lid_tb_complete,lid_tb_end};
    h->binding=1;
    int ok=lid_tp_prepare(&h->payment,q,n,&payment)&&lid_tsa_prepare(&h->sale,f,m,exact_sha,h,lid_tb_lease)&&
        lid_shop_marker_bind(&h->json,n->image,exact_sha,&json)&&
        lid_tni_prepare(&h->no_item,&h->payment,m,&noitem,exact_sha)&&lid_thr_prepare(&h->route,q->owner,&route);
    h->binding=0;if(!ok){h->stopped=1;return 0;}h->prepared=1;return 1;
}
#endif
