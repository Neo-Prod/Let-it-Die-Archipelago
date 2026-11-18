#ifndef LID_RESEARCH_DIAGNOSTIC_DRIVER_R214_H
#define LID_RESEARCH_DIAGNOSTIC_DRIVER_R214_H
#include "momoka_windows_adapter.h"
#include "momoka_host_publication.h"
/* Candidate driver core, no automatic installation, no logger/file APIs.
   Build identity and current GameInfo lifetime must be supplied by mod host. */
typedef struct M214Driver {
 M213Adapter adapter;M208Cell cell;
 void*session_context;int(*session)(void*);unsigned prepared;
} M214Driver;
static int m214_prepare_impl(M214Driver*d,uint64_t image,uint64_t game,uint32_t owner,
 void*session_context,int(*session)(void*)){
 static const unsigned char prefix[12]={0x48,0x8b,0xc4,0x41,0x56,0x48,0x81,0xec,0x10,0x01,0,0};
 unsigned char actual[12];uint64_t field,cls,database;uint32_t size,dim,offset;
 M195Ops o={0};if(!d)return 0;o=(M195Ops){&d->adapter,m213_copy,m213_name};
 if(d->prepared||!session||!session(session_context)||!image||image>UINT64_MAX-0x295fd24||
 !m213_copy(&d->adapter,image+0x107cf80,actual,12))return 0;
 for(unsigned i=0;i<12;i++)if(actual[i]!=prefix[i])return 0;
 if(!m213_copy(&d->adapter,image+0x295fd18,&d->adapter.names,8)||
 !m213_copy(&d->adapter,image+0x295fd20,&d->adapter.count,4)||!d->adapter.names||
 !d->adapter.count||d->adapter.count>2000000||
 !m212_member(&o,game,"BrgGameInfoNative","mDatabase",&field)||
 !m195_read(&o,field,0x50,&cls,8)||!m195_named(&o,cls,"ObjectProperty")||
 !m195_read(&o,field,0x68,&dim,4)||dim!=1||!m195_read(&o,field,0x6c,&size,4)||size!=8||
 !m195_read(&o,field,0x94,&offset,4)||offset<0x60||offset>0x10000||
 !m195_read(&o,game,offset,&database,8)||!m213_prepare(&d->adapter,database,image+0x107cf80,owner))return 0;
 d->session_context=session_context;d->session=session;d->prepared=1;return 1;
}
static int m214_prepare(M214Driver*d,uint64_t image,uint64_t game,uint32_t owner,
 void*context,int(*session)(void*)){
 if(!d||!d->adapter.api.get_error||!d->adapter.api.set_error)return 0;
 uint32_t error=d->adapter.api.get_error();int result=m214_prepare_impl(d,image,game,owner,context,session);
 d->adapter.api.set_error(error);return result;
}
static int m214_activate(M214Driver*d,uintptr_t wrapper){
 if(!d||!d->prepared||!d->session||!d->adapter.api.get_error||!d->adapter.api.set_error)return 0;
 uint32_t error=d->adapter.api.get_error();int result=0;
 /* Caller separately verifies writable slot memory and stable rooted owner.
    This call writes the supplied slot; NEVER call during read-only analysis. */
 if(d->session(d->session_context))result=m208_publish(&d->cell,&d->adapter.prepared.host,(uintptr_t*)(uintptr_t)d->adapter.prepared.native.slot,
  (uintptr_t)d->adapter.prepared.native.original,wrapper);
 d->adapter.api.set_error(error);return result;
}
static void m214_call(M214Driver*d,void*receiver,void*frame,void*result){
 const M204Host*h=__atomic_load_n(&d->cell.host,__ATOMIC_ACQUIRE);uint32_t error;
 if(!h||!h->original)return; /* Forbidden if wrapper has been published. */
 error=d->adapter.api.get_error();
 int active=d->session&&d->session(d->session_context);d->adapter.api.set_error(error);
 if(active)m208_call(&d->cell,receiver,frame,result);else h->original(receiver,frame,result);
}
/* Nonblocking stop-and-copy, outside original native callback. Stop remains
   permanent; storage/wrapper still resident for late original invocations. */
static int m214_take(M211Host*h,M197SkillSnapshot*out,unsigned capacity,unsigned*count){
 unsigned free=0;
 if(!h||!out||capacity<16||!count||!__atomic_compare_exchange_n(&h->busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
 if(h->drained||h->count>16){__atomic_store_n(&h->busy,0,__ATOMIC_RELEASE);return 0;}
 h->stopped=1;*count=h->count;
 for(unsigned i=0;i<h->count;i++)out[i]=h->records[i];
 h->drained=1;__atomic_store_n(&h->busy,0,__ATOMIC_RELEASE);return 1;
}
#endif
