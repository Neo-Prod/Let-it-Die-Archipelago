#ifndef LID_RESEARCH_MOMOKA_WINDOWS_ADAPTER_R213_H
#define LID_RESEARCH_MOMOKA_WINDOWS_ADAPTER_R213_H
/* No windows.h/imports: compatible with winlite-based mod. Runtime supplies
   exact Windows x64 API pointers; this header does not resolve/load modules. */
#include "momoka_prepare_observer.h"
typedef struct M213Api {
 int(*read)(void*,const void*,void*,size_t,size_t*);
 void*(*process)(void);
 uint32_t(*thread)(void);
 uint32_t(*get_error)(void);
 void(*set_error)(uint32_t);
} M213Api;
typedef struct M213Adapter {M213Api api;uint64_t names;uint32_t count;M212Prepared prepared;} M213Adapter;
static int m213_copy(void*context,uint64_t at,void*out,size_t n){
 M213Adapter*a=context;size_t got=0;
 if(!a||!a->api.read||!a->api.process||!out||!n||n>4096||at<0x10000||at>UINT64_MAX-n)return 0;
 return a->api.read(a->api.process(),(const void*)(uintptr_t)at,out,n,&got)&&got==n;
}
static int m213_name(void*context,uint64_t object,const char*wanted){
 M213Adapter*a=context;uint32_t id[2];uint64_t entry;unsigned char flag;
 if(!a||!wanted||!a->names||!a->count||a->count>2000000||object>UINT64_MAX-0x48||
 !m213_copy(a,object+0x48,id,8)||id[1]||id[0]>=a->count||a->names>UINT64_MAX-(uint64_t)id[0]*8||
 !m213_copy(a,a->names+(uint64_t)id[0]*8,&entry,8)||entry>UINT64_MAX-0x114||!m213_copy(a,entry+8,&flag,1))return 0;
 for(unsigned i=0;i<128;i++){
  uint16_t ch=0;unsigned size=(flag&1)?2:1;
  if(!m213_copy(a,entry+0x14+(uint64_t)i*size,&ch,size)||ch!=(unsigned char)wanted[i])return 0;
  if(!ch)return 1;
 }return 0;
}
static uint32_t m213_thread(void*p){M213Adapter*a=p;return a->api.thread?a->api.thread():0;}
/* RPM reads have no region-cache to retain. Scope is an observation boundary,
   NOT a GC lock, snapshot transaction or object lifetime guarantee. */
static int m213_begin(void*p){(void)p;return 1;}
static void m213_end(void*p){(void)p;}
static uint32_t m213_error(void*p){M211Host*g=p;M213Adapter*a=g->provider;return a->api.get_error();}
static void m213_set_error(void*p,uint32_t e){M211Host*g=p;M213Adapter*a=g->provider;a->api.set_error(e);}
static int m213_prepare(M213Adapter*a,uint64_t database,uint64_t expected_exec,uint32_t owner){
 M195Ops memory={a,m213_copy,m213_name};uint32_t error;int result;
 if(!a||!a->api.read||!a->api.process||!a->api.thread||!a->api.get_error||!a->api.set_error||!a->names||!a->count)return 0;
 error=a->api.get_error();result=m212_prepare(&a->prepared,&memory,database,expected_exec,a,owner,m213_thread,m213_begin,m213_end,m213_error,m213_set_error);
 a->api.set_error(error);return result;
}
#endif
