#ifndef LID_CHECKS_R26_DISK_H
#define LID_CHECKS_R26_DISK_H
#include "winlite.h"
#include "death_service.h"
typedef struct R26Ops {
    HANDLE (WINAPI *open)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
    BOOL (WINAPI *read)(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
    BOOL (WINAPI *write)(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
    BOOL (WINAPI *close)(HANDLE);BOOL (WINAPI *remove)(LPCWSTR);BOOL (WINAPI *flush)(HANDLE);
    BOOL (WINAPI *replace)(LPCWSTR,LPCWSTR,DWORD);DWORD (WINAPI *error)(void);
    HANDLE (WINAPI *thread)(LPVOID,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,DWORD*);
    DWORD (WINAPI *thread_id)(void);void (WINAPI *sleep)(DWORD);
    DWORD (WINAPI *wait)(HANDLE,DWORD);unsigned long long (WINAPI *clock)(void);
} R26Ops;
typedef struct R26Disk {
    R26Ops ops;HANDLE lease;uint64_t attempt;
    wchar_t events[1120],ack[1120],stem[1180],temporary[1220],claim[1120];
} R26Disk;
static int r26_append(wchar_t*out,size_t cap,const wchar_t*s){
    size_t n=0,i=0;if(!out||!s)return 0;while(n<cap&&out[n])n++;if(n==cap)return 0;
    while(s[i]){if(n+1>=cap)return 0;out[n++]=s[i++];}out[n]=0;return 1;
}
static int r26_hex(wchar_t*out,size_t cap,uint64_t value,unsigned digits){
    static const wchar_t hex[]=L"0123456789abcdef";wchar_t text[17];if(!digits||digits>16)return 0;
    for(unsigned i=0;i<digits;i++)text[i]=hex[(value>>(4*(digits-i-1)))&15];text[digits]=0;return r26_append(out,cap,text);
}
static int r26_path(wchar_t*out,size_t cap,const wchar_t*directory,const wchar_t*name){out[0]=0;return r26_append(out,cap,directory)&&r26_append(out,cap,L"\\")&&r26_append(out,cap,name);}
static int r26_nonce(wchar_t*out,size_t cap,const R15Context*c){for(unsigned i=0;i<16;i++)if(!r26_hex(out,cap,c->nonce[i],2))return 0;return 1;}
static int r26_ops_valid(const R26Ops*o){return o&&o->open&&o->read&&o->write&&o->close&&o->remove&&o->flush&&o->replace&&o->error&&o->thread&&o->thread_id&&o->sleep&&o->wait&&o->clock;}
static int r26_disk_close(R26Disk*d){if(!d||!d->lease||!d->ops.close(d->lease))return 0;d->lease=0;return 1;}
/* Existing trusted absolute slot directory only. Startup I/O, never capture. */
static int r26_disk_start(R26Disk*d,const R26Ops*ops,const wchar_t*dir,const R15Context*c){
    if(!d||d->lease||!r26_ops_valid(ops)||!dir||!r15_context_valid(c))return 0;
    size_t n=0;while(n<1000&&dir[n])n++;if(n<3||n==1000||dir[1]!=':'||(dir[2]!='\\'&&dir[2]!='/'))return 0;
    d->ops=*ops;d->attempt=0;
    if(!r26_path(d->events,1120,dir,L"death_native_events.bin")||!r26_path(d->ack,1120,dir,L"death_client_received.bin")||
       !r26_path(d->stem,1180,dir,L".death-")||!r26_hex(d->stem,1180,c->pid,8)||!r26_append(d->stem,1180,L"-")||
       !r26_hex(d->stem,1180,c->birth,16)||!r26_append(d->stem,1180,L"-")||!r26_nonce(d->stem,1180,c)||!r26_append(d->stem,1180,L".tmp.")||
       !r26_path(d->claim,1120,dir,L".death-queue-")||!r26_nonce(d->claim,1120,c)||!r26_append(d->claim,1120,L".used"))return 0;
    wchar_t lease_path[1120];if(!r26_path(lease_path,1120,dir,L"death_native.owner"))return 0;
    HANDLE lease=ops->open(lease_path,GENERIC_READ|GENERIC_WRITE,0,0,4 /* OPEN_ALWAYS */,FILE_ATTRIBUTE_NORMAL,0);
    if(lease==INVALID_HANDLE_VALUE)return 0;d->lease=lease;
    HANDLE file=ops->open(d->claim,GENERIC_WRITE,0,0,1 /* CREATE_NEW */,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE){r26_disk_close(d);return 0;}
    uint8_t record[128];r15_zero(record,sizeof(record));r15_copy(record,"LIDQCTX1",8);r15_w32(record+8,1);
    r15_w32(record+16,c->pid);r15_w64(record+20,c->birth);r15_w32(record+28,c->client_pid);
    r15_copy(record+32,c->nonce,16);r15_copy(record+48,c->writer,16);r15_copy(record+64,c->run,32);r15_copy(record+96,c->config,32);
    DWORD wrote=0;int ok=ops->write(file,record,128,&wrote,0)&&wrote==128;
    if(ok)ok=ops->flush(file)!=0;int closed=ops->close(file)!=0;
    if(!ok||!closed){r26_disk_close(d);return 0;}return 1;
}
static int r26_disk_read(void*opaque,uint8_t*out,size_t capacity){
    R26Disk*d=opaque;DWORD read=0;if(!d||!d->lease||capacity!=R15_SIZE+1)return -1;
    HANDLE file=d->ops.open(d->ack,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4 /* DELETE */,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE){DWORD e=d->ops.error();return e==2||e==3?0:-1;}
    int ok=d->ops.read(file,out,(DWORD)capacity,&read,0)!=0;int closed=d->ops.close(file)!=0;
    return ok&&closed?(read?(int)read:R15_SIZE+1):-1;
}
static int r26_disk_publish(void*opaque,const uint8_t*data,size_t size){
    R26Disk*d=opaque;if(!d||!d->lease||!data||!size||size>R15_SIZE*R15_BATCH||size%R15_SIZE)return 0;
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned i=0;i<8;i++){
        if(d->attempt==UINT64_MAX)return 0;d->temporary[0]=0;
        if(!r26_append(d->temporary,1220,d->stem)||!r26_hex(d->temporary,1220,++d->attempt,16))return 0;
        file=d->ops.open(d->temporary,GENERIC_WRITE,0,0,1,FILE_ATTRIBUTE_NORMAL,0);
        if(file!=INVALID_HANDLE_VALUE)break;DWORD e=d->ops.error();if(e!=80&&e!=183)return 0;
    }
    if(file==INVALID_HANDLE_VALUE)return 0;DWORD wrote=0;
    int ok=d->ops.write(file,data,(DWORD)size,&wrote,0)&&wrote==size;
    if(ok)ok=d->ops.flush(file)!=0;int closed=d->ops.close(file)!=0;if(!closed)ok=0;
    if(ok)ok=d->ops.replace(d->temporary,d->events,9 /* REPLACE|WRITE_THROUGH */)!=0;
    if(!ok&&closed)d->ops.remove(d->temporary);return ok;
}
#endif
