#ifndef LID_MATERIAL_CAPABILITY_R99_H
#define LID_MATERIAL_CAPABILITY_R99_H
#include "death_wire.h"
/* Separate from character GRANT. Codec only: no I/O, hooks or implicit arming.
   FNV detects damaged bytes; it is NOT authentication against a local attacker. */
enum {R99_SIZE=256,R99_REQUEST=1,R99_READY=2};
typedef struct R99Capability {R15Context context;uint32_t kind;uint8_t catalogue[32];} R99Capability;
static const uint8_t r99_magic[8]={'L','I','D','M','A','T','0','1'};
static uint32_t r99_sum(const uint8_t*p){uint32_t h=2166136261u;for(unsigned i=0;i<252;i++)h=(h^p[i])*16777619u;return h;}
static int r99_valid(const R99Capability*m){return m&&r15_context_valid(&m->context)&&(m->kind==R99_REQUEST||m->kind==R99_READY)&&r15_nonzero(m->catalogue,32);}
static int r99_encode(uint8_t*out,size_t n,const R99Capability*m){
 if(!out||n!=R99_SIZE||!r99_valid(m))return 0;
 r15_zero(out,n);r15_copy(out,r99_magic,8);r15_w32(out+8,1);r15_w32(out+12,m->kind);
 r15_w32(out+16,m->context.pid);r15_w32(out+20,m->context.client_pid);r15_w64(out+24,m->context.birth);
 r15_copy(out+32,m->context.nonce,16);r15_copy(out+48,m->context.run,32);r15_copy(out+80,m->context.config,32);
 r15_copy(out+112,m->context.writer,16);r15_copy(out+128,m->catalogue,32);r15_copy(out+160,r15_build,32);
 r15_w32(out+192,5);r15_w32(out+252,r99_sum(out));return 1;
}
static int r99_decode(R99Capability*out,const uint8_t*p,size_t n){
 R99Capability m;uint8_t canonical[R99_SIZE];
 if(!out||!p||n!=R99_SIZE||!r15_equal(p,r99_magic,8)||r15_r32(p+8)!=1||r15_r32(p+192)!=5||
 !r15_equal(p+160,r15_build,32)||r15_r32(p+252)!=r99_sum(p))return 0;
 r15_zero(&m,sizeof(m));m.kind=r15_r32(p+12);m.context.pid=r15_r32(p+16);m.context.client_pid=r15_r32(p+20);m.context.birth=r15_r64(p+24);
 r15_copy(m.context.nonce,p+32,16);r15_copy(m.context.run,p+48,32);r15_copy(m.context.config,p+80,32);
 r15_copy(m.context.writer,p+112,16);r15_copy(m.catalogue,p+128,32);
 if(!r99_encode(canonical,sizeof(canonical),&m)||!r15_equal(canonical,p,n))return 0;
 *out=m;return 1;
}
/* Reply construction requires the actual host to attest every listed proof.
   Do not call with compiled constants in place of live evidence. */
typedef struct R99Proof {unsigned owner,current_thread,module_verified,slot_owned,bindings_verified,transport_active,rotation_ready;} R99Proof;
static int r99_ready(R99Capability*out,const R99Capability*request,const R15Context*current,const R99Proof*p){
 R99Capability m;
 if(!out||!r99_valid(request)||request->kind!=R99_REQUEST||!r15_context_valid(current)||
 !r15_same_context(&request->context,current)||!p||!p->owner||p->owner!=p->current_thread||
 p->module_verified!=1||p->slot_owned!=1||p->bindings_verified!=1||p->transport_active!=1||p->rotation_ready!=1)return 0;
 m=*request;m.kind=R99_READY;*out=m;return 1;
}
#endif
