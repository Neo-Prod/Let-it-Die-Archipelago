#ifndef LID_KILL_COIN_DISK_R142_H
#define LID_KILL_COIN_DISK_R142_H
#include "checks_disk_io.h"
#include "kill_coin_worker.h"
/* Long-lived worker storage; do not allocate this large path table on an
   unprobed game-thread stack. Parent owns the existing exclusive slot lease. */
typedef struct R142Disk {
 R26Disk*parent;uint32_t initialized,worker,fault;uint64_t attempt;HANDLE uncertain_handle;
 wchar_t paths[4][1120],stem[1180],temporary[1220];
} R142Disk;
static int r142_ready(const R142Disk*d){return d&&d->initialized==1&&!d->fault&&d->parent&&d->parent->lease&&d->parent->ops.thread_id&&d->parent->ops.thread_id()==d->worker;}
static int r142_init(R142Disk*d,R26Disk*parent,const wchar_t*directory,const R15Context*c,uint32_t worker){
 if(!d||d->initialized||!parent||!parent->lease||!r26_ops_valid(&parent->ops)||!r15_context_valid(c)||!worker||parent->ops.thread_id()!=worker||!directory)return 0;
 size_t n=0;while(n<1000&&directory[n])n++;if(n<3||n==1000||directory[1]!=':'||(directory[2]!='\\'&&directory[2]!='/'))return 0;
 if(!r26_path(d->paths[0],1120,directory,L"death_native_events.bin"))return 0;
 for(unsigned i=0;;i++){if(i==1120||d->paths[0][i]!=parent->events[i])return 0;if(!d->paths[0][i])break;}
 static const wchar_t*names[4]={L"kill_coin_grant.bin",L"kill_coin_ack.bin",L"kill_coin_applied.bin",L"kill_coin_pending.bin"};
 for(unsigned i=0;i<4;i++)if(!r26_path(d->paths[i],1120,directory,names[i]))return 0;
 d->stem[0]=0;
 if(!r26_path(d->stem,1180,directory,L".kill-coins-native-")||!r26_hex(d->stem,1180,c->pid,8)||!r26_append(d->stem,1180,L"-")||!r26_hex(d->stem,1180,c->birth,16)||!r26_append(d->stem,1180,L"-")||!r26_nonce(d->stem,1180,c)||!r26_append(d->stem,1180,L".tmp."))return 0;
 d->parent=parent;d->worker=worker;d->attempt=0;d->initialized=1;return 1;
}
static int r142_read(void*opaque,unsigned which,R138Frame*out){
 R142Disk*d=opaque;if(!r142_ready(d)||!out||which<1||which>4)return -1;
 R26Ops*o=&d->parent->ops;HANDLE h=o->open(d->paths[which-1],GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
 if(h==INVALID_HANDLE_VALUE)return o->error()==2?0:-1;
 uint8_t raw[257];DWORD count=0;int ok=o->read(h,raw,257,&count,0)!=0;int closed=o->close(h)!=0;
 if(!closed){d->fault=1;d->uncertain_handle=h;}
 return ok&&closed&&count==256&&r138_decode(out,raw,count)?1:-1;
}
static int r142_put(void*opaque,unsigned which,const R138Frame*f){
 R142Disk*d=opaque;if(!r142_ready(d)||which<2||which>4||!r138_valid(f))return 0;
 if((which==R141_INTENT_FILE&&f->kind!=R138_GRANT)||(which!=R141_INTENT_FILE&&f->kind!=R138_ACK)||(which==R141_LEDGER_FILE&&f->status!=R138_APPLIED))return 0;
 uint8_t raw[256];if(!r138_encode(raw,256,f))return 0;R26Ops*o=&d->parent->ops;HANDLE h=INVALID_HANDLE_VALUE;
 for(unsigned i=0;i<8;i++){
  if(d->attempt==UINT64_MAX)return 0;d->temporary[0]=0;
  if(!r26_append(d->temporary,1220,d->stem)||!r26_hex(d->temporary,1220,++d->attempt,16))return 0;
  h=o->open(d->temporary,GENERIC_WRITE,0,0,1,FILE_ATTRIBUTE_NORMAL,0);
  if(h!=INVALID_HANDLE_VALUE)break;DWORD e=o->error();if(e!=80&&e!=183)return 0;
 }
 if(h==INVALID_HANDLE_VALUE)return 0;DWORD written=0;
 int ok=o->write(h,raw,256,&written,0)&&written==256;
 if(ok)ok=o->flush(h)!=0;int closed=o->close(h)!=0;if(!closed){ok=0;d->fault=1;d->uncertain_handle=h;}
 if(ok)ok=o->replace(d->temporary,d->paths[which-1],9)!=0;
 if(!ok&&closed)(void)o->remove(d->temporary);return ok;
}
static int r142_remove_intent(void*opaque){
 R142Disk*d=opaque;if(!r142_ready(d))return 0;
 R26Ops*o=&d->parent->ops;if(o->remove(d->paths[R141_INTENT_FILE-1]))return 1;return o->error()==2;
}
#endif
