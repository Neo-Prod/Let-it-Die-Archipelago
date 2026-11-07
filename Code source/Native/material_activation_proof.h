#ifndef LID_MATERIAL_ACTIVATION_PROOF_R119_H
#define LID_MATERIAL_ACTIVATION_PROOF_R119_H
#include "material_owner_measure.h"
#include "material_capability.h"
enum {R119_NO_PROOF=0,R119_WORKER_PROOF=1,R119_OWNER_PROOF=2};
/* Caller separately proves actual module mapping and owner context. The
   returned proof still requires R91 PE/prefix/slot validation before install. */
static int r119_activation_proof(const R105Hash*worker,R105Hash*owned,const R105Ops*ops,const R99Capability*request,const R15Context*live,uint32_t tid,uint64_t image,char digest[65]){
 if(!owned||!digest||!r105_ops(ops)||!tid||ops->io->thread_id()!=tid||!image||!r99_valid(request)||request->kind!=R99_REQUEST||!r15_context_valid(live)||!r15_same_context(&request->context,live))return R119_NO_PROOF;
 if(r105_verified(worker,tid,image,digest))return R119_WORKER_PROOF;
 if(!owned->owner&&!r105_init(owned,image,tid))return R119_NO_PROOF;
 if(owned->owner!=tid||owned->image!=image||!r119_owner_measure(owned,ops,tid,digest)||!r105_verified(owned,tid,image,digest))return R119_NO_PROOF;
 return R119_OWNER_PROOF;
}
#endif
