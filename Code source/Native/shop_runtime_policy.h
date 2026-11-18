#ifndef LID_SHOP_RUNTIME_POLICY_H
#define LID_SHOP_RUNTIME_POLICY_H
#include "checks_disk_io.h"
/* Private background IO only; no hooks/game calls/Save. One terminal explicit
   mode per native Context. Context binding is not protection from hostile local
   code. No absent-file=>disabled fallback. Publication uses release/acquire. */
enum {LID_SM_UNKNOWN=0,LID_SM_DISABLED=1,LID_SM_ENABLED=2,LID_SM_FAILED=3};
enum {LID_SM_SIZE=192};
typedef struct LidShopPolicy {
    unsigned initialized,mode,goal;uint32_t ui,worker;R15Context context;
    uint8_t full[32],bytes[LID_SM_SIZE+1];R26Ops io;
    wchar_t path[1120];uint64_t began,next_poll;HANDLE file;
} LidShopPolicy;
static unsigned lid_sm_mode(const LidShopPolicy*p){
    return p?__atomic_load_n(&p->mode,__ATOMIC_ACQUIRE):LID_SM_UNKNOWN;
}
static uint32_t lid_sm_checksum(const uint8_t*p){
    uint32_t h=2166136261u;for(unsigned i=0;i<LID_SM_SIZE;i++)if(i<168||i>=172)h=(h^p[i])*16777619u;return h;
}
static int lid_sm_prepare(LidShopPolicy*p,const R26Ops*io,const wchar_t*directory,
                          const R15Context*c,uint32_t ui,uint32_t worker){
    size_t n=0;
    if(!p||p->initialized||!r26_ops_valid(io)||!directory||!r15_context_valid(c)||
       !ui||!worker||ui==worker||io->thread_id()!=worker)return 0;
    while(n<1000&&directory[n])n++;
    if(n<3||n==1000||directory[1]!=':'||(directory[2]!='\\'&&directory[2]!='/')||
       !r26_path(p->path,1120,directory,L"shop_client_policy.bin"))return 0;
    p->io=*io;p->context=*c;p->ui=ui;p->worker=worker;p->began=io->clock();p->initialized=1;return 1;
}
/* Noncurrent file/short atomic predecessor waits. A malformed CURRENT file
   fails this feature only. No reread after a terminal state, max2 probes/sec. */
static unsigned lid_sm_poll(LidShopPolicy*p){
    if(!p||!p->initialized||p->io.thread_id()!=p->worker)return LID_SM_FAILED;
    unsigned mode=lid_sm_mode(p);if(mode!=LID_SM_UNKNOWN)return mode;
    uint64_t now=p->io.clock();
    if(now<p->began||now-p->began>=120000)goto fail;
    if(p->next_poll&&now<p->next_poll)return LID_SM_UNKNOWN;
    p->next_poll=now>UINT64_MAX-500?UINT64_MAX:now+500;
    p->file=p->io.open(p->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(p->file==INVALID_HANDLE_VALUE){p->file=0;DWORD error=p->io.error();if(error==2||error==3)return LID_SM_UNKNOWN;goto fail;}
    DWORD got=0;int read=p->io.read(p->file,p->bytes,sizeof(p->bytes),&got,0)!=0;
    if(!p->io.close(p->file))goto fail;p->file=0;
    if(!read)goto fail;
    if(got<128)return LID_SM_UNKNOWN;
    uint8_t*b=p->bytes;R15Context c={0};
    c.pid=r15_r32(b+16);c.client_pid=r15_r32(b+20);c.birth=r15_r64(b+24);
    r15_copy(c.nonce,b+32,16);r15_copy(c.writer,b+48,16);r15_copy(c.run,b+64,32);r15_copy(c.config,b+96,32);
    if(!r15_same_context(&c,&p->context))return LID_SM_UNKNOWN;
    unsigned enabled=r15_r32(b+160),goal=r15_r32(b+164);
    if(got!=LID_SM_SIZE||!r15_equal(b,"LIDSHPM1",8)||r15_r32(b+8)!=1||
       r15_r32(b+12)!=LID_SM_SIZE||enabled>1||goal<10||goal>40||goal%10||
       !r15_nonzero(b+128,32)||r15_nonzero(b+172,20)||r15_r32(b+168)!=lid_sm_checksum(b))goto fail;
    r15_copy(p->full,b+128,32);p->goal=goal;
    mode=enabled?LID_SM_ENABLED:LID_SM_DISABLED;
    __atomic_store_n(&p->mode,mode,__ATOMIC_RELEASE);return mode;
fail:__atomic_store_n(&p->mode,LID_SM_FAILED,__ATOMIC_RELEASE);return LID_SM_FAILED;
}
#endif
