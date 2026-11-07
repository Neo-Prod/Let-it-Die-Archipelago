#ifndef LID_CHECKS_CAPTURE_R25_H
#define LID_CHECKS_CAPTURE_R25_H
#include "death_queue.h"
#include "checks_call_context.h"
/* This is a converter, not an installed hook. The caller must supply copied,
   validated samples and a class mask built under the real reflection scope. */
typedef struct R25Observation {
    R7CallContext call;R6Sample before,after;int argument;
    uint64_t epoch;uint32_t classes,rotation_verified;
    char floor[16],area[32],rotation[16],damage[96],pawn_class[96];
    uint16_t name[96],label[96];
} R25Observation;
/* Called only outside the combat callback. Host type is the actual
   LidCharacterHost; include progression_wire/host before this header. */
static int r25_host_context(const LidCharacterHost*h,R15Context*out){
    if(!h||!out||!h->initialized||h->poisoned||!h->grant_published||!h->peer.bound||!h->peer.granted)return 0;
    const LidWireMessage*b=&h->peer.binding;
    if(b->kind!=LID_WIRE_QUIESCED||!lid_wire_valid(b)||!lid_wire_same_session(&h->peer.offer,b)||
       !lid_wire_equal(b->config,h->expected_config,32))return 0;
    R15Context c;r15_zero(&c,sizeof(c));c.pid=b->pid;c.birth=b->creation_time;c.client_pid=b->writer_pid;
    r15_copy(c.nonce,b->nonce,16);r15_copy(c.run,b->identity,32);r15_copy(c.config,b->config,32);r15_copy(c.writer,b->writer,16);
    if(!r15_context_valid(&c))return 0;*out=c;return 1;
}
static int r25_class_mask(const R6Ops*ops,uint64_t cls,uint32_t*out){
    static const char*names[20]={"BrgPawn_CustomCharaAI","BrgPawn_CustomCharaAIZombie","BrgPawn_NormalEnemyBase","BrgPawn_Scratch","BrgPawn_Hovering","BrgPawn_Turret","BrgPawn_Bone","BrgPawn_Reversal","BrgPawn_Treasure","BrgPawn_MiddleBossHunk","BrgPawn_MiddleBossJindie","BrgPawn_MiddleBossGoto09","BrgPawn_MiddleBossU10","BrgPawn_BossMax","BrgPawn_BossJackson","BrgPawn_BossCrowley","BrgPawn_BossGunkan","BrgPawn_MiddleBossU10Tail","BrgPawn_Jackal","BrgPawn_FourForcemenBase"};
    uint64_t seen[32],next;unsigned count=0;uint32_t mask=0;
    if(!out||!cls)return 0;
    while(cls){
        if(count==32)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==cls)return 0;seen[count++]=cls;
        for(unsigned i=0;i<20;i++)if(r6_name(ops,cls,names[i]))mask|=1u<<i;
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    *out=mask;return 1;
}
/* Return 1 queued, 0 ineligible/unverified, -1 refused/fault. No disk,
   allocation, wait, game call, or dependency on diagnostic count128. */
static int r25_capture(R15Queue*q,uint32_t tid,const R15Context*bound,const R25Observation*o,uint64_t expected_node,uint64_t expected_origin){
    if(!q||!o||!bound||!tid||tid!=q->owner||!__atomic_load_n(&q->initialized,__ATOMIC_ACQUIRE))return -1;
    if(!r15_same_context(&q->context,bound)){r15_queue_fault(q,tid,3);return -1;}
    if(!expected_node||!expected_origin||o->call.node!=expected_node||o->call.origin!=expected_origin||
       !r7_accepted_setdeath_label(&o->call,o->argument,&o->before,&o->after))return 0;
    if(o->rotation_verified!=1||!r15_equal(o->rotation,"4HMA\0",5)||!o->epoch)return 0;
    if(o->classes&((1u<<17)|(1u<<18)|(1u<<19)))return 0;
    if(!(o->classes&0x1fffbu))return 0; /* No known enemy classification. */
    R15Frame f;r15_zero(&f,sizeof(f));f.context=*bound;f.kind=R15_EVENT;f.epoch=o->epoch;f.flags=3;
    f.classes=o->classes;f.name_index=o->before.name_index;f.name_number=o->before.name_number;f.raw_kind=o->after.kind;
    f.health=o->after.health;f.max_health=o->after.max_health;
    r15_copy(f.floor,o->floor,sizeof(f.floor));r15_copy(f.area,o->area,sizeof(f.area));r15_copy(f.rotation,o->rotation,sizeof(f.rotation));
    r15_copy(f.damage,o->damage,sizeof(f.damage));r15_copy(f.pawn_class,o->pawn_class,sizeof(f.pawn_class));r15_copy(f.name,o->name,sizeof(f.name));r15_copy(f.label,o->label,sizeof(f.label));
    for(unsigned i=0;i<6;i++)if(o->classes&(1u<<(i+3)))f.family=i+1;
    /* The raw V-label is authoritative downstream. Do not reuse the old
       diagnostic MK parser or infer a version from combat level/grade. */
    return r15_queue_push(q,tid,&f)==1?1:-1;
}
#endif
