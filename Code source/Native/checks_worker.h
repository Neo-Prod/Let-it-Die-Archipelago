#ifndef LID_CHECKS_R26_WORKER_H
#define LID_CHECKS_R26_WORKER_H
#include "checks_disk_io.h"
#ifndef LID_NOTIFICATIONS_ENABLED
#define LID_NOTIFICATIONS_ENABLED 0
#endif
#if LID_NOTIFICATIONS_ENABLED
#include "notification_disk.h"
#endif
#ifndef LID_KILL_COIN_DELIVERY_ENABLED
#define LID_KILL_COIN_DELIVERY_ENABLED 0
#endif
#if LID_KILL_COIN_DELIVERY_ENABLED
#include "kill_coin_disk.h"
#endif
#ifndef LID_MATERIAL_CAPABILITY_ENABLED
#define LID_MATERIAL_CAPABILITY_ENABLED 0
#endif
#if LID_MATERIAL_CAPABILITY_ENABLED
#include "material_capability_disk.h"
#include "material_capability_worker.h"
#include "material_module_worker.h"
#endif
enum {R26_NEW,R26_STARTING,R26_ACTIVE,R26_DRAINING,R26_DONE,R26_FAILED,R26_TIMEOUT,R26_CLOSED};
typedef struct R26Transport {
    R15Queue queue;R16Service service;R26Disk disk;HANDLE thread;
    uint32_t state,stop,owner,drain_timeout,last_result,outcome,joined;
#if LID_KILL_COIN_DELIVERY_ENABLED
    R141Coins coins;R142Disk coin_disk;uint32_t coin_ready;int coin_result;
    wchar_t coin_directory[1000];
#endif
#if LID_MATERIAL_CAPABILITY_ENABLED
    R100Mailbox material_mailbox;R100Worker material_worker;R101Disk material_disk;int material_enabled,material_result;
    uint64_t material_image;R105Hash material_hash;R105Ops material_crypto;int material_hash_enabled;
#endif
#if LID_NOTIFICATIONS_ENABLED
    R154Disk notifications;
#endif
} R26Transport;
static DWORD WINAPI r26_worker(LPVOID opaque){
    R26Transport*t=opaque;R26Ops*ops=&t->disk.ops;R16IO io={&t->disk,r26_disk_read,r26_disk_publish};
    DWORD tid=ops->thread_id();
    if(!r16_service_init(&t->service,&t->queue,tid,io)){__atomic_store_n(&t->state,R26_FAILED,__ATOMIC_RELEASE);return 1;}
#if LID_NOTIFICATIONS_ENABLED
    (void)r154_disk_init(&t->notifications,&t->disk,&t->queue.context,t->owner,tid);
#endif
#if LID_KILL_COIN_DELIVERY_ENABLED
    int checks_failed=0;
    if(r142_init(&t->coin_disk,&t->disk,t->coin_directory,&t->queue.context,tid)&&
       r141_init(&t->coins,&t->queue.context,t->owner,tid))
        __atomic_store_n(&t->coin_ready,1,__ATOMIC_RELEASE);
#endif
    __atomic_store_n(&t->state,__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)?R26_DRAINING:R26_ACTIVE,__ATOMIC_RELEASE);
#if LID_MATERIAL_CAPABILITY_ENABLED
    if(t->material_enabled){
        t->material_worker.tid=tid;
        if(!r101_worker_bind(&t->material_disk,tid))t->material_enabled=0;
    }
#endif
    uint64_t started=0;int draining=0;
    for(;;){
        uint64_t now=ops->clock();unsigned result=r16_service_step(&t->service,tid,now);__atomic_store_n(&t->last_result,result,__ATOMIC_RELEASE);
#if LID_NOTIFICATIONS_ENABLED
        if(!__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE))(void)r154_disk_step(&t->notifications,tid);
#endif
        if((result&(R16_FAULT|R16_PUBLISHED))==(R16_FAULT|R16_PUBLISHED)){
#if LID_MATERIAL_CAPABILITY_ENABLED
            if(t->material_hash_enabled)(void)r106_hash_cancel(&t->material_hash,&t->material_crypto,tid);
#endif
#if LID_KILL_COIN_DELIVERY_ENABLED
            checks_failed=1;__atomic_store_n(&t->stop,1,__ATOMIC_RELEASE);
#else
            __atomic_store_n(&t->state,R26_FAILED,__ATOMIC_RELEASE);return 2;
#endif
        }
#if LID_MATERIAL_CAPABILITY_ENABLED
        if(t->material_enabled){
            R100WorkerOps cap={&t->material_disk,r101_read,r101_publish};
            t->material_result=r100_worker_step(&t->material_worker,&t->material_mailbox,&cap,tid,now,
                __atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)!=0);
        }
#endif
#if LID_MATERIAL_CAPABILITY_ENABLED
        if(t->material_hash_enabled){
            if(__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE))
                (void)r106_hash_cancel(&t->material_hash,&t->material_crypto,tid);
            else if(t->material_enabled&&__atomic_load_n(&t->material_mailbox.request_set,__ATOMIC_ACQUIRE))
                (void)r105_step(&t->material_hash,&t->material_crypto,tid);
        }
#endif
#if LID_KILL_COIN_DELIVERY_ENABLED
        if(__atomic_load_n(&t->coin_ready,__ATOMIC_ACQUIRE)){
            R141IO coin_io={&t->coin_disk,r142_read,r142_put,r142_remove_intent};
            t->coin_result=r141_worker(&t->coins,&coin_io,tid,__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)!=0);
        }
#endif
        if(__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)){
            if(!draining){started=now;draining=1;__atomic_store_n(&t->state,R26_DRAINING,__ATOMIC_RELEASE);}
            int drained=__atomic_load_n(&t->queue.tail,__ATOMIC_ACQUIRE)==__atomic_load_n(&t->queue.received,__ATOMIC_ACQUIRE)&&!__atomic_load_n(&t->queue.fault,__ATOMIC_ACQUIRE);
#if LID_KILL_COIN_DELIVERY_ENABLED
            if(checks_failed)drained=1;
            if(__atomic_load_n(&t->coin_ready,__ATOMIC_ACQUIRE)&&!r141_drained(&t->coins))drained=0;
            if(drained&&checks_failed){__atomic_store_n(&t->state,R26_FAILED,__ATOMIC_RELEASE);return 2;}
#endif
            if(drained){
                __atomic_store_n(&t->state,R26_DONE,__ATOMIC_RELEASE);return 0;
            }
            if(now>=started&&now-started>=t->drain_timeout){__atomic_store_n(&t->state,R26_TIMEOUT,__ATOMIC_RELEASE);return 3;}
        }
        ops->sleep(result&(R16_PUBLISHED|R16_RECEIVED)?10:100);
    }
}
/* Startup/reap on proven owner outside combat and DllMain; zero initialized
   long-lived storage. Failure burns a claimed nonce, never resets a queue. */
static int r26_start(R26Transport*t,const R26Ops*ops,const wchar_t*directory,const R15Context*c,uint32_t owner,uint32_t drain_timeout){
    if(!t||t->state!=R26_NEW||t->queue.initialized||!owner||!r26_ops_valid(ops)||ops->thread_id()!=owner||drain_timeout<10||drain_timeout>30000)return 0;
    t->owner=owner;t->drain_timeout=drain_timeout;
#if LID_NOTIFICATIONS_ENABLED
    if(!r26_append(t->notifications.directory,1000,directory))return 0;
#endif
#if LID_KILL_COIN_DELIVERY_ENABLED
    t->coin_directory[0]=0;if(!r26_append(t->coin_directory,1000,directory))return 0;
#endif
    if(!r26_disk_start(&t->disk,ops,directory,c)||!r15_queue_init(&t->queue,c,owner)){
        if(t->disk.lease)r26_disk_close(&t->disk);t->state=R26_FAILED;return 0;
    }
#if LID_MATERIAL_CAPABILITY_ENABLED
    t->material_enabled=r100_init(&t->material_mailbox,c,owner)&&r101_init(&t->material_disk,&t->disk,&t->material_mailbox,directory);
    t->material_crypto.io=&t->disk.ops;
    t->material_hash_enabled=t->material_enabled&&r105_ops(&t->material_crypto)&&r105_init(&t->material_hash,t->material_image,owner);
#endif
    __atomic_store_n(&t->state,R26_STARTING,__ATOMIC_RELEASE);DWORD tid=0;
    t->thread=ops->thread(0,0,r26_worker,t,0,&tid);
    if(!t->thread){r26_disk_close(&t->disk);t->state=R26_FAILED;return 0;}return 1;
}
static int r26_push(R26Transport*t,uint32_t tid,const R15Frame*frame){
    if(!t||tid!=t->owner||t->disk.ops.thread_id()!=tid)return -1;
    if(__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)||__atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE)return 0;
    return r15_queue_push(&t->queue,tid,frame);
}
static int r26_request_stop(R26Transport*t,uint32_t tid){
    if(!t||tid!=t->owner||!t->thread||t->disk.ops.thread_id()!=tid)return 0;
    __atomic_store_n(&t->stop,1,__ATOMIC_RELEASE);return 1;
}
#if LID_KILL_COIN_DELIVERY_ENABLED
/* Owner thread only, no IO, no startup/stop path mutations of coin mailbox. */
static int r26_coin_owner(R26Transport*t,const R138Ops*ops){
    if(!t||!ops||!ops->thread||ops->thread(ops->context)!=t->owner||t->disk.ops.thread_id()!=t->owner)return R138_INVALID;
    if((__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE)&&r141_state(&t->coins)!=R141_NATIVE_SAVE)||__atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||!__atomic_load_n(&t->coin_ready,__ATOMIC_ACQUIRE))return R138_WAIT;
    return r141_owner(&t->coins,ops);
}
#endif
/* Nonblocking join: keep lease/storage alive until the OS confirms exit. */
static int r26_reap(R26Transport*t,uint32_t tid){
    if(!t||tid!=t->owner||t->disk.ops.thread_id()!=tid)return 0;
    if(!t->joined){
        if(!t->thread||t->disk.ops.wait(t->thread,0)!=0 /* WAIT_OBJECT_0 */)return 0;
        t->joined=1;t->outcome=__atomic_load_n(&t->state,__ATOMIC_ACQUIRE);
    }
    if(t->thread){if(!t->disk.ops.close(t->thread))return 0;t->thread=0;}
    if(!t->disk.lease)return 0;
    if(!r26_disk_close(&t->disk))return 0;
    __atomic_store_n(&t->state,R26_CLOSED,__ATOMIC_RELEASE);return 1;
}
#endif
