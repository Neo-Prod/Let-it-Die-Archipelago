#ifndef LID_MATERIAL_OWNER_MEASURE_R119_H
#define LID_MATERIAL_OWNER_MEASURE_R119_H
#include "material_module_hash.h"
/* Separate owner-owned proof storage, NEVER the worker's R105Hash object.
   Synchronous one-time fallback; fixed exact build file size, no sleeps.
   This is bounded in bytes, NOT in duration of a blocking filesystem call. */
static int r119_owner_step(R105Hash*s,const R105Ops*o,uint32_t tid){
 if(!s||!s->owner||!r105_ops(o)||!tid||tid!=s->owner||o->io->thread_id()!=tid)return 0;
 if(s->worker&&s->worker!=tid)return 0;
 unsigned state=__atomic_load_n(&s->state,__ATOMIC_ACQUIRE);
 if(state==R105_DONE||state==R105_FAILED)return (int)state;
 s->worker=tid;
 if(state==R105_NEW){
  wchar_t path[1000];DWORD n=o->module_path((HMODULE)(ULONG_PTR)s->image,path,1000),got=0,object_size=0,digest_size=0;long long size;
  if(!n||n>=1000||path[n]!=0)goto failed;
  s->file=o->io->open(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(s->file==INVALID_HANDLE_VALUE){s->file=0;goto failed;}
  if(!o->file_size(s->file,&size)||size!=45069312ll)goto failed;s->expected=(uint64_t)size;
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

static int r119_owner_measure(R105Hash*s,const R105Ops*o,uint32_t tid,char digest[65]){
 if(!s||!digest||!r105_ops(o)||!tid||s->owner!=tid||o->io->thread_id()!=tid)return 0;
 unsigned state=__atomic_load_n(&s->state,__ATOMIC_ACQUIRE);
 if(state==R105_DONE)return r105_verified(s,tid,s->image,digest);
 if(state!=R105_NEW)return 0;
 for(unsigned n=0;n<64;n++){
  int result=r119_owner_step(s,o,tid);
  if(result==R105_DONE)return r105_verified(s,tid,s->image,digest);
  if(result!=R105_ACTIVE)return 0;
 }
 (void)r105_release(s,o);__atomic_store_n(&s->state,R105_FAILED,__ATOMIC_RELEASE);return 0;
}
#endif
