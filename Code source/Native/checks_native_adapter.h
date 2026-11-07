#ifndef LID_CHECKS_R26_NATIVE_ADAPTER_H
#define LID_CHECKS_R26_NATIVE_ADAPTER_H
/* Include the existing progression_wire/host first. This file does not install
   a hook, call the game, or decide whether AP/rotation is actually active. */
#include "checks_worker.h"
#include "checks_capture.h"
static int r26_start_from_host(R26Transport*t,const R26Ops*ops,const wchar_t*directory,const LidCharacterHost*host,uint32_t owner,int activation_verified){
    R15Context context;
    if(activation_verified!=1||!host||!host->acknowledged||!r25_host_context(host,&context))return 0;
    return r26_start(t,ops,directory,&context,owner,5000);
}
static int r26_capture_observed(R26Transport*t,const LidCharacterHost*host,uint32_t tid,const R25Observation*observation,uint64_t node,uint64_t origin){
    if(!t||tid!=t->owner||t->disk.ops.thread_id()!=tid)return -1;
    if(__atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||__atomic_load_n(&t->stop,__ATOMIC_ACQUIRE))return 0;
    R15Context context;
    if(!host||!host->acknowledged||!r25_host_context(host,&context))return 0;
    return r25_capture(&t->queue,tid,&context,observation,node,origin);
}
#endif
