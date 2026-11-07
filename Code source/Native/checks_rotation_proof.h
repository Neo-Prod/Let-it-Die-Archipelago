#ifndef LID_CHECKS_ROTATION_PROOF_R35_H
#define LID_CHECKS_ROTATION_PROOF_R35_H
/* The boundary owner is the ONLY publisher/reader of payload fields. Other
   threads may only invalidate the atomic revision/fault. No borrowed pointer. */
typedef struct R35Proof {
    uint64_t revision,accepted_revision;
    unsigned fault,published,owner,serial;
    R31Identity identity;
} R35Proof;
static uint64_t r35_revision(const R35Proof*p){return __atomic_load_n(&p->revision,__ATOMIC_ACQUIRE);}
static void r35_revoke(R35Proof*p){
    if(!p)return;
    uint64_t old=r35_revision(p);
    for(unsigned i=0;i<8;i++){
        if(old==UINT64_MAX){__atomic_store_n(&p->fault,1,__ATOMIC_RELEASE);return;}
        if(__atomic_compare_exchange_n(&p->revision,&old,old+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
    }
    __atomic_store_n(&p->fault,1,__ATOMIC_RELEASE);
}
static int r35_publish(R35Proof*p,uint32_t owner,uint32_t current,unsigned serial,
                       uint64_t observed_revision,const R31Identity*observed,const R31Identity*now,int stage_ok){
    if(!p||!owner||current!=owner||!serial||!stage_ok||!observed||!now||
       !r31_valid_identity(observed)||!r31_same(observed,now)||
       __atomic_load_n(&p->fault,__ATOMIC_ACQUIRE)||r35_revision(p)!=observed_revision)return 0;
    /* Payload is owner-thread only; foreign revokers never touch it. */
    if(p->published&&p->owner!=owner)return 0;
    p->owner=owner;p->serial=serial;p->identity=*now;p->accepted_revision=observed_revision;p->published=1;
    return !__atomic_load_n(&p->fault,__ATOMIC_ACQUIRE)&&r35_revision(p)==observed_revision;
}
static int r35_ready(const R35Proof*p,uint32_t owner,uint32_t current,unsigned serial,const R31Identity*now){
    if(!p||!owner||current!=owner||!serial||!now||__atomic_load_n(&p->fault,__ATOMIC_ACQUIRE))return 0;
    uint64_t revision=r35_revision(p);
    return p->published&&p->owner==owner&&p->serial==serial&&p->accepted_revision==revision&&
      r31_valid_identity(now)&&r31_same(&p->identity,now)&&r35_revision(p)==revision&&
      !__atomic_load_n(&p->fault,__ATOMIC_ACQUIRE);
}
#if LID_CHECKS_DELIVERY_ENABLED
static R35Proof g_checks_rotation_proof;
#endif
#endif
