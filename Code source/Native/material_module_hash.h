#ifndef LID_MATERIAL_MODULE_HASH_R105_H
#define LID_MATERIAL_MODULE_HASH_R105_H
#include "checks_disk_io.h"
/* Read-only file measurement. Worker only, max 1 MiB per step, fixed storage.
   Initialize before worker exposure; no reset until joined and owner idle. */
typedef struct R105Ops {
 R26Ops*io;
 DWORD(WINAPI*module_path)(HMODULE,wchar_t*,DWORD);
 BOOL(WINAPI*file_size)(HANDLE,long long*);
 LONG(WINAPI*open_alg)(void**,const wchar_t*,const wchar_t*,DWORD);
 LONG(WINAPI*property)(void*,const wchar_t*,BYTE*,DWORD,DWORD*,DWORD);
 LONG(WINAPI*create_hash)(void*,void**,BYTE*,DWORD,BYTE*,DWORD,DWORD);
 LONG(WINAPI*hash_data)(void*,BYTE*,DWORD,DWORD);
 LONG(WINAPI*finish_hash)(void*,BYTE*,DWORD,DWORD);
 LONG(WINAPI*destroy_hash)(void*);
 LONG(WINAPI*close_alg)(void*,DWORD);
} R105Ops;
enum {R105_NEW=0,R105_ACTIVE,R105_DONE,R105_FAILED};
typedef struct R105Hash {
 uint32_t state,owner,worker;uint64_t image,total,expected;
 HANDLE file;void*algorithm;void*hash;
 BYTE object[4096],buffer[65536],digest[32];
} R105Hash;
static int r105_init(R105Hash*s,uint64_t image,uint32_t owner){
 if(!s||s->owner||s->state||!image||!owner)return 0;s->image=image;s->owner=owner;return 1;
}
static int r105_ops(const R105Ops*o){return o&&o->io&&o->io->open&&o->io->read&&o->io->close&&o->io->thread_id&&o->module_path&&o->file_size&&o->open_alg&&o->property&&o->create_hash&&o->hash_data&&o->finish_hash&&o->destroy_hash&&o->close_alg;}
/* Each release attempted once. Failed handles retained for diagnosis, not
   blindly closed again: a failed close may have ambiguous ownership. */
static int r105_release(R105Hash*s,const R105Ops*o){
 int ok=1;
 if(s->hash){if(o->destroy_hash(s->hash)<0)ok=0;else s->hash=0;}
 if(s->algorithm){if(o->close_alg(s->algorithm,0)<0)ok=0;else s->algorithm=0;}
 if(s->file){if(!o->io->close(s->file))ok=0;else s->file=0;}
 return ok;
}
static int r105_step(R105Hash*s,const R105Ops*o,uint32_t tid){
 if(!s||!s->owner||!r105_ops(o)||!tid||tid==s->owner||o->io->thread_id()!=tid)return 0;
 if(s->worker&&s->worker!=tid)return 0;
 unsigned state=__atomic_load_n(&s->state,__ATOMIC_ACQUIRE);
 if(state==R105_DONE||state==R105_FAILED)return (int)state;
 s->worker=tid;
 if(state==R105_NEW){
  wchar_t path[1000];DWORD n=o->module_path((HMODULE)(ULONG_PTR)s->image,path,1000),got=0,object_size=0,digest_size=0;long long size;
  if(!n||n>=1000||path[n]!=0)goto failed;
  s->file=o->io->open(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(s->file==INVALID_HANDLE_VALUE){s->file=0;goto failed;}
  if(!o->file_size(s->file,&size)||size<=0||size>0x7fffffffll)goto failed;s->expected=(uint64_t)size;
  if(o->open_alg(&s->algorithm,L"SHA256",0,0)<0||!s->algorithm||
  o->property(s->algorithm,L"ObjectLength",(BYTE*)&object_size,4,&got,0)<0||got!=4||!object_size||object_size>sizeof(s->object)||
  o->property(s->algorithm,L"HashDigestLength",(BYTE*)&digest_size,4,&got,0)<0||got!=4||digest_size!=32||
  o->create_hash(s->algorithm,&s->hash,s->object,object_size,0,0,0)<0||!s->hash)goto failed;
  __atomic_store_n(&s->state,R105_ACTIVE,__ATOMIC_RELEASE);
 }
 for(unsigned i=0;i<16;i++){
  DWORD got=0;
  if(!o->io->read(s->file,s->buffer,sizeof(s->buffer),&got,0)||got>sizeof(s->buffer)||s->total>s->expected||got>s->expected-s->total)goto failed;
  if(!got){
   if(s->total!=s->expected||o->finish_hash(s->hash,s->digest,32,0)<0)goto failed;
   if(!r105_release(s,o))goto failed_no_release;
   __atomic_store_n(&s->state,R105_DONE,__ATOMIC_RELEASE);return R105_DONE;
  }
  if(o->hash_data(s->hash,s->buffer,got,0)<0)goto failed;s->total+=got;
 }
 return R105_ACTIVE;
failed:(void)r105_release(s,o);
failed_no_release:__atomic_store_n(&s->state,R105_FAILED,__ATOMIC_RELEASE);return R105_FAILED;
}
/* An owner acquire reads the digest only after completed hashing and clean
   resource release. image binds measurement to the module requested at init. */
static int r105_verified(const R105Hash*s,uint32_t tid,uint64_t image,char out[65]){
 static const char hex[]="0123456789abcdef";
 if(!s||!out||tid!=s->owner||image!=s->image||__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=R105_DONE||
 !r15_equal(s->digest,r15_build,32))return 0;
 for(unsigned i=0;i<32;i++){out[2*i]=hex[s->digest[i]>>4];out[2*i+1]=hex[s->digest[i]&15];}out[64]=0;return 1;
}
#endif
