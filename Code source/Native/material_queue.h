#ifndef LID_MATERIAL_QUEUE_R94_H
#define LID_MATERIAL_QUEUE_R94_H
#include "death_queue.h"
#include "material_wire.h"
/* Additional typed producer entry. Existing queue worker/ACK/legacy producers
   remain unchanged. Catalogue5 is an externally bound configuration gate. */
static int r94_queue_push_material(R15Queue*q,uint32_t tid,unsigned catalogue,const R94MaterialFrame*source){
 if(!q||!tid||tid!=q->owner||!__atomic_load_n(&q->initialized,__ATOMIC_ACQUIRE))return -1;
 if(catalogue!=5)return -3;
 if(__atomic_load_n(&q->fault,__ATOMIC_ACQUIRE))return -2;
 uint64_t tail=__atomic_load_n(&q->tail,__ATOMIC_RELAXED),received=__atomic_load_n(&q->received,__ATOMIC_ACQUIRE);
 if(tail==UINT64_MAX||tail-received>=R15_CAPACITY){r15_queue_fault(q,tid,1);return -2;}
 if(!source||source->sequence||!r15_same_context(&source->context,&q->context)){r15_queue_fault(q,tid,3);return -2;}
 R94MaterialFrame frame=*source;frame.sequence=tail+1;
 if(!r94_material_encode(q->data[tail%R15_CAPACITY],R15_SIZE,&frame)){r15_queue_fault(q,tid,4);return -2;}
 __atomic_store_n(&q->tail,tail+1,__ATOMIC_RELEASE);return 1;
}
#endif
