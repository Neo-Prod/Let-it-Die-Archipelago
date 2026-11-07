#ifndef LID_MATERIAL_MODULE_WORKER_R106_H
#define LID_MATERIAL_MODULE_WORKER_R106_H
#include "material_module_hash.h"
/* Worker-only cancellation. DONE retains its measurement; FAILED is never
   cleaned twice. Outstanding handles from an uncertain close remain recorded. */
static int r106_hash_cancel(R105Hash*s,const R105Ops*o,uint32_t tid){
 if(!s||!s->owner||!r105_ops(o)||!tid||tid==s->owner||o->io->thread_id()!=tid||(s->worker&&s->worker!=tid))return 0;
 unsigned state=__atomic_load_n(&s->state,__ATOMIC_ACQUIRE);
 if(state==R105_DONE||state==R105_FAILED)return (int)state;
 s->worker=tid;(void)r105_release(s,o);
 __atomic_store_n(&s->state,R105_FAILED,__ATOMIC_RELEASE);return R105_FAILED;
}
#endif
