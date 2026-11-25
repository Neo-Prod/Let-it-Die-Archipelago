#ifndef LID_MASTERY_POLICY_H
#define LID_MASTERY_POLICY_H
#include "mastery_identity.h"
/* Optional policy I/O is worker-only, bounded and session-bound. */
typedef struct LidMasteryPolicy {unsigned enabled,checks,fists,multiplier;uint64_t mask;} LidMasteryPolicy;
typedef struct LidMasteryWorker {LidMasteryPolicy policy;unsigned state;uint64_t next,deadline;wchar_t path[1120];} LidMasteryWorker;
static uint32_t lid_mastery_checksum(const BYTE*p){uint32_t h=2166136261u;for(unsigned i=0;i<188;i++)h=(h^p[i])*16777619u;return h;}
static int lid_mastery_decode(LidMasteryPolicy*out,const BYTE*p,size_t size,const R15Context*c){
    LidMasteryPolicy m;
    if(!out||!p||size!=192||!r15_context_valid(c)||!r15_equal(p,"LIDMST01",8)||r15_r32(p+8)!=1||r15_r32(p+12)!=192||
       r15_r32(p+16)!=c->pid||r15_r32(p+20)!=c->client_pid||r15_r64(p+24)!=c->birth||
       !r15_equal(p+32,c->nonce,16)||!r15_equal(p+48,c->run,32)||!r15_equal(p+80,c->config,32)||!r15_equal(p+112,c->writer,16)||
       !r15_equal(p+152,lid_mastery_catalogue,32)||r15_nonzero(p+184,4)||r15_r32(p+188)!=lid_mastery_checksum(p))return 0;
    m.enabled=r15_r32(p+128);m.checks=r15_r32(p+132);m.fists=r15_r32(p+136);m.multiplier=r15_r32(p+140);m.mask=r15_r64(p+144);
    if(m.enabled>1||m.checks<1||m.checks>20||m.fists>1||m.multiplier<1||m.multiplier>100||
       (m.mask>>LID_MASTERY_FAMILIES)||(!m.enabled&&m.mask)||(!m.fists&&(m.mask&1)))return 0;
    *out=m;return 1;
}
static int lid_mastery_worker_init(LidMasteryWorker*w,const wchar_t*directory){
    return w&&!w->state&&r26_path(w->path,1120,directory,L"mastery_policy.bin");
}
static void lid_mastery_worker_poll(LidMasteryWorker*w,R26Ops*o,const R15Context*c,uint64_t now){
    BYTE raw[193];DWORD got=0;LidMasteryPolicy p;
    if(!w||__atomic_load_n(&w->state,__ATOMIC_ACQUIRE)||now<w->next)return;
    w->next=now+500;if(!w->deadline)w->deadline=now+60000;
    HANDLE f=o->open(w->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(f!=INVALID_HANDLE_VALUE){int ok=o->read(f,raw,193,&got,0);int closed=o->close(f);
        if(ok&&closed&&lid_mastery_decode(&p,raw,got,c)){w->policy=p;__atomic_store_n(&w->state,1,__ATOMIC_RELEASE);return;}}
    if(now>=w->deadline)__atomic_store_n(&w->state,2,__ATOMIC_RELEASE); /* Legacy slot: no hooks or permanent I/O polling. */
}
static int lid_mastery_scaled_xp(int xp,unsigned multiplier,int old_abp){
    if(xp<=0||multiplier<1||multiplier>100)return xp;
    if(old_abp<0)old_abp=0;
    uint64_t result=(uint64_t)(unsigned)xp*multiplier,room=2147483647u-(unsigned)old_abp;
    return (int)(result<room?result:room);
}
#endif
