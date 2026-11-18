#ifndef LID_SHOP_TOWER_RECEIPT_DISK_H
#define LID_SHOP_TOWER_RECEIPT_DISK_H
#include "checks_disk_io.h"
#include "shop_tower_receipt_port.h"
/* Same-worker only. Fixed isolated paths, exclusive lease, atomic flushed event. */
typedef struct LidTowerReceiptDisk {
    unsigned initialized;uint32_t worker;uint64_t attempt,awaiting_mark;
    R26Ops ops;HANDLE lease;LidTowerReceiptPort*bridge;
    wchar_t events[1120],ack[1120],state[1120],temporary[1220],stem[1180];
} LidTowerReceiptDisk;
static int lid_trd_start(LidTowerReceiptDisk*d,LidTowerReceiptPort*b,const R26Ops*ops,const wchar_t*directory){
    wchar_t owner[1120];size_t n=0;uint32_t worker;
    if(!d||d->initialized||!b||!b->host||!r26_ops_valid(ops)||!directory||
       !(worker=ops->thread_id())||worker!=b->worker||worker==b->ui)return 0;
    while(n<1000&&directory[n])n++;
    if(n<3||n==1000||directory[1]!=':'||(directory[2]!='\\'&&directory[2]!='/'))return 0;
    if(!r26_path(d->events,1120,directory,L"shop_tower_native_payment.bin")||
       !r26_path(d->ack,1120,directory,L"shop_tower_client_received.bin")||
       !r26_path(d->state,1120,directory,L"shop_tower_client_state.bin")||
       !r26_path(owner,1120,directory,L"shop_tower_native.owner")||
       !r26_path(d->stem,1180,directory,L".shop-tower-")||!r26_nonce(d->stem,1180,&b->context)||
       !r26_append(d->stem,1180,L".tmp."))return 0;
    HANDLE lease=ops->open(owner,GENERIC_READ|GENERIC_WRITE,0,0,4,FILE_ATTRIBUTE_NORMAL,0);
    if(lease==INVALID_HANDLE_VALUE)return 0;
    /* Queue worker ID was fixed by the authenticated catalogue before publication. */
    d->ops=*ops;d->lease=lease;d->bridge=b;d->worker=worker;d->initialized=1;return 1;
}
/* 0 missing, -1 failed/oversized, otherwise bytes read. Paths are fixed at setup. */
static int lid_trd_read(LidTowerReceiptDisk*d,const wchar_t*path,uint8_t*out,unsigned capacity){
    DWORD got=0;HANDLE f=d->ops.open(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(f==INVALID_HANDLE_VALUE){DWORD error=d->ops.error();return error==2||error==3?0:-1;}
    int ok=d->ops.read(f,out,capacity,&got,0)!=0;int closed=d->ops.close(f)!=0;
    return ok&&closed?(got?(int)got:-1):-1;
}
static int lid_trd_write(LidTowerReceiptDisk*d,const uint8_t raw[LID_TPW_SIZE]){
    HANDLE f=INVALID_HANDLE_VALUE;DWORD wrote=0;
    for(unsigned i=0;i<8;i++){
        if(d->attempt==UINT64_MAX)return 0;d->temporary[0]=0;
        if(!r26_append(d->temporary,1220,d->stem)||!r26_hex(d->temporary,1220,++d->attempt,16))return 0;
        f=d->ops.open(d->temporary,GENERIC_WRITE,0,0,1,FILE_ATTRIBUTE_NORMAL,0);
        if(f!=INVALID_HANDLE_VALUE)break;DWORD e=d->ops.error();if(e!=80&&e!=183)return 0;
    }
    if(f==INVALID_HANDLE_VALUE)return 0;
    int ok=d->ops.write(f,raw,LID_TPW_SIZE,&wrote,0)&&wrote==LID_TPW_SIZE;
    if(ok)ok=d->ops.flush(f)!=0;
    int closed=d->ops.close(f)!=0;if(!closed)ok=0;
    if(ok)ok=d->ops.replace(d->temporary,d->events,9)!=0;
    if(!ok&&closed)d->ops.remove(d->temporary);return ok;
}
static int lid_trd_pump(LidTowerReceiptDisk*d){
    uint8_t state[LID_TPW_STATE_SIZE+1],bought[23],raw[LID_TPW_SIZE+1];LidTowerPurchaseFrame ack={0};uint64_t token;
    if(!d||!d->initialized||!d->lease||d->ops.thread_id()!=d->worker)return 0;
    if(d->awaiting_mark){
        if(!d->bridge->published(d->bridge->host,d->awaiting_mark))return 0;
        d->awaiting_mark=0;
    }
    int n=lid_trd_read(d,d->state,state,sizeof(state));
    if(n==LID_TPW_STATE_SIZE&&lid_tpw_state_read(bought,state,(size_t)n,&d->bridge->context,d->bridge->catalogue))
        (void)d->bridge->state(d->bridge->host,bought);
    n=lid_trd_read(d,d->ack,raw,sizeof(raw));
    if(n==LID_TPW_SIZE&&lid_tpw_decode(&ack,raw,(size_t)n))(void)d->bridge->ack(d->bridge->host,&ack);
    if(d->bridge->peek(d->bridge->host,raw,&token)){
        if(!lid_trd_write(d,raw))return 0;
        d->awaiting_mark=token;
        if(!d->bridge->published(d->bridge->host,token))return 0;
        d->awaiting_mark=0;return 1;
    }
    return 1;
}
static int lid_trd_close(LidTowerReceiptDisk*d){
    if(!d||!d->initialized||!d->lease||d->awaiting_mark||d->ops.thread_id()!=d->worker||
       !d->bridge->retire(d->bridge->host)||!d->ops.close(d->lease))return 0;
    d->lease=0;return 1;
}
#endif
