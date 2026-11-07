#ifndef LID_CHECKS_ROTATION_R31_H
#define LID_CHECKS_ROTATION_R31_H
/* Isolated policy/core only. No hook installation, game addresses, file I/O,
   waits or delivery proof publication. Native callbacks require a separately
   tested adapter. A prepared pin alone is NOT a verified Stage rotation. */
#include <stdint.h>
#include <stddef.h>
#ifndef LID_CHECKS_ROTATION_ENABLED
#define LID_CHECKS_ROTATION_ENABLED 0
#endif
typedef struct R31Identity {
    uint32_t pid; uint64_t birth;
    unsigned char nonce[16],identity[32],config[32];
} R31Identity;
typedef struct R31String {uint16_t *data;int32_t num,capacity;} R31String;
typedef struct R31Selector {
    void *vtable;R31String term;int64_t timestamp;R31String template_id;unsigned char valid;
} R31Selector;
_Static_assert(sizeof(R31String)==16,"FString x64");
_Static_assert(offsetof(R31Selector,term)==8,"term offset");
_Static_assert(offsetof(R31Selector,timestamp)==0x18,"timestamp offset");
_Static_assert(offsetof(R31Selector,template_id)==0x20,"template offset");
_Static_assert(offsetof(R31Selector,valid)==0x30,"valid offset");
typedef struct R31Ops {
    void *context;
    uint32_t (*thread)(void*);
    /* Must derive a fresh granted, matching, immutable AP binding. Not a
       substitute for validation from the real host/runtime adapter. */
    int (*identity)(void*,R31Identity*);
    int (*read)(void*,const void*,void*,size_t);
    int (*assign)(void*,R31String*,const uint16_t*,unsigned);
    void (*destroy)(void*,R31String*);
    void* (*selector)(void*,void*);
    uint64_t (*writer)(void*,uint32_t,const uint16_t*,unsigned char);
} R31Ops;
enum {R31_FAULT_NONE,R31_FAULT_CONCURRENT,R31_FAULT_IDENTITY,R31_FAULT_OBJECT,
      R31_FAULT_TERM,R31_FAULT_ALLOCATION};
typedef struct R31Pin {
    R31Identity identity;int64_t timestamp;unsigned length;uint16_t term[32];
} R31Pin;
typedef struct R31Rotation {
    R31Ops ops;unsigned initialized,ready,scoped,fault;uint32_t owner;R31Pin pin;
} R31Rotation;
static int r31_nonzero(const unsigned char*p,unsigned n){while(n--)if(*p++)return 1;return 0;}
static int r31_valid_identity(const R31Identity*i){
    return i&&i->pid&&i->birth&&r31_nonzero(i->nonce,16)&&r31_nonzero(i->identity,32)&&r31_nonzero(i->config,32);
}
static int r31_same(const R31Identity*a,const R31Identity*b){
    return a->pid==b->pid&&a->birth==b->birth&&!__builtin_memcmp(a->nonce,b->nonce,16)&&
      !__builtin_memcmp(a->identity,b->identity,32)&&!__builtin_memcmp(a->config,b->config,32);
}
static void r31_fault(R31Rotation*s,unsigned fault){
    unsigned empty=0;(void)__atomic_compare_exchange_n(&s->fault,&empty,fault,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
}
static int r31_init(R31Rotation*s,const R31Ops*o){
    if(!s||s->initialized||s->owner||s->ready||s->fault||s->scoped||!o||!o->thread||!o->identity||!o->read||!o->assign||!o->destroy||!o->selector||!o->writer)return 0;
    s->ops=*o;s->initialized=1;return 1; /* caller owns zeroed, unpublished storage */
}
static int r31_binding(R31Rotation*s,R31Identity*out){
    return s->ops.identity(s->ops.context,out)==1&&r31_valid_identity(out);
}
static int r31_lock(R31Rotation*s,uint32_t tid){
    uint32_t empty=0;return tid&&__atomic_compare_exchange_n(&s->owner,&empty,tid,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED);
}
static void r31_unlock(R31Rotation*s){__atomic_store_n(&s->owner,0,__ATOMIC_RELEASE);}
static int r31_current(R31Rotation*s,const R31Identity*expected){
    R31Identity now;
    if(!r31_binding(s,&now)||!r31_same(&now,expected)){r31_fault(s,R31_FAULT_IDENTITY);return 0;}
    return !__atomic_load_n(&s->fault,__ATOMIC_ACQUIRE);
}
static uint64_t r31_writer(R31Rotation*s,uintptr_t caller_rva,uint32_t uid,const uint16_t*explicit_template,unsigned char flag){
    R31Identity id;uint32_t tid;uint64_t result;
    if(!s||!s->initialized)return 0;
    if((caller_rva!=0x111dee5&&caller_rva!=0x111e8a5)||
       __atomic_load_n(&s->fault,__ATOMIC_ACQUIRE)||!r31_binding(s,&id)||
       !(tid=s->ops.thread(s->ops.context)))
        return s->ops.writer(s->ops.context,uid,explicit_template,flag);
    if(!r31_lock(s,tid)){
        r31_fault(s,R31_FAULT_CONCURRENT);
        return s->ops.writer(s->ops.context,uid,explicit_template,flag);
    }
    if(__atomic_load_n(&s->ready,__ATOMIC_ACQUIRE)&&!r31_same(&s->pin.identity,&id)){
        r31_fault(s,R31_FAULT_IDENTITY);r31_unlock(s);
        return s->ops.writer(s->ops.context,uid,explicit_template,flag);
    }
    /* Candidate normalization is confined to the internal rotation writer;
       the Core function and its seven arguments are not wrapped/rewritten. */
    s->scoped=1;s->pin.identity=id;
    result=s->ops.writer(s->ops.context,uid,0,flag);
    (void)r31_current(s,&id);s->scoped=0;r31_unlock(s);return result;
}
/* R34 entry guard, preserving the R31 callback ABI. The native adapter must
   confirm tutorial completion before an explicit argument becomes NULL.
   Missing/denied eligibility always preserves the original argument tuple. */
static uint64_t r34_writer_guard(R31Rotation*s,uintptr_t caller_rva,uint32_t uid,const uint16_t*explicit_template,
                                 unsigned char flag,int(*eligible)(void*)){
    R31Identity id;
    if(!s||!s->initialized)return 0;
    if((caller_rva!=0x111dee5&&caller_rva!=0x111e8a5)||__atomic_load_n(&s->fault,__ATOMIC_ACQUIRE)||
       !r31_binding(s,&id)||!s->ops.thread(s->ops.context))
        return s->ops.writer(s->ops.context,uid,explicit_template,flag);
    if(__atomic_load_n(&s->owner,__ATOMIC_ACQUIRE)){
        r31_fault(s,R31_FAULT_CONCURRENT);return s->ops.writer(s->ops.context,uid,explicit_template,flag);
    }
    if(!eligible||eligible(s->ops.context)!=1||!r31_current(s,&id))
        return s->ops.writer(s->ops.context,uid,explicit_template,flag);
    return r31_writer(s,caller_rva,uid,explicit_template,flag);
}
static int r31_term(R31Rotation*s,const R31Selector*object,R31Pin*out){
    static const uint16_t prefix[]={'T','E','R','M','I','D','_'};
    uint64_t timestamp=0;unsigned n;uint16_t template_copy[32];
    if(object->valid!=1||object->timestamp<=0||object->term.num<9||object->term.num>31||
       object->term.capacity<object->term.num||object->term.capacity>4096||!object->term.data)return 0;
    if(object->template_id.num<2||object->template_id.num>32||
       object->template_id.capacity<object->template_id.num||object->template_id.capacity>4096||
       !object->template_id.data||!s->ops.read(s->ops.context,object->template_id.data,template_copy,(unsigned)object->template_id.num*2)||
       template_copy[object->template_id.num-1])return 0;
    n=(unsigned)object->term.num;
    if(!s->ops.read(s->ops.context,object->term.data,out->term,n*2)||out->term[n-1])return 0;
    for(unsigned i=0;i<7;i++)if(out->term[i]!=prefix[i])return 0;
    /* Canonical decimal, no leading zeros, overflow or embedded terminator. */
    if(out->term[7]=='0')return 0;
    for(unsigned i=7;i+1<n;i++){
        unsigned digit=out->term[i]-'0';if(digit>9||timestamp>(UINT64_MAX-digit)/10)return 0;
        timestamp=timestamp*10+digit;
    }
    if(timestamp!=(uint64_t)object->timestamp)return 0;
    out->timestamp=object->timestamp;out->length=n;return 1;
}
static void* r31_selector(R31Rotation*s,uintptr_t caller_rva,void*out){
    R31Selector original;R31Pin prepared;R31Identity id;R31String term={0},tmpl={0};
    static const uint16_t wednesday[]={'4','H','M','A',0};
    uint32_t tid,owner;int held=0;void*result;
    if(!s||!s->initialized)return 0;
    result=s->ops.selector(s->ops.context,out); /* exactly one on every path */
    if(result!=out||!out||(caller_rva!=0x1360b31&&caller_rva!=0x12132ba)||
       __atomic_load_n(&s->fault,__ATOMIC_ACQUIRE)||!r31_binding(s,&id)||
       !(tid=s->ops.thread(s->ops.context)))return result;
    owner=__atomic_load_n(&s->owner,__ATOMIC_ACQUIRE);
    if(owner==tid){if(caller_rva!=0x1360b31||!s->scoped)return result;}
    else{
        if(owner||caller_rva!=0x12132ba||!__atomic_load_n(&s->ready,__ATOMIC_ACQUIRE)||!r31_lock(s,tid))return result;
        held=1;
    }
    if(!r31_same(&s->pin.identity,&id)){r31_fault(s,R31_FAULT_IDENTITY);goto done;}
    if(!s->ops.read(s->ops.context,out,&original,0x31)){
        r31_fault(s,R31_FAULT_OBJECT);goto done;
    }
    if(!r31_term(s,&original,&prepared)){
        r31_fault(s,R31_FAULT_TERM);goto done;
    }
    if(__atomic_load_n(&s->ready,__ATOMIC_ACQUIRE))prepared=s->pin;
    else prepared.identity=id;
    /* Allocate independent descriptors first. The original output remains
       byte-for-byte unchanged on either callback failure or binding drift. */
    if(s->ops.assign(s->ops.context,&term,prepared.term,prepared.length)!=1||
       s->ops.assign(s->ops.context,&tmpl,wednesday,5)!=1){
        r31_fault(s,R31_FAULT_ALLOCATION);goto cleanup;
    }
    if(!r31_current(s,&id))goto cleanup;
    __builtin_memcpy((unsigned char*)out+8,&term,sizeof(term));
    __builtin_memcpy((unsigned char*)out+0x18,&prepared.timestamp,8);
    __builtin_memcpy((unsigned char*)out+0x20,&tmpl,sizeof(tmpl));
    /* Ownership transfers to the native output; old native allocations are
       released with the adapter's matching allocator, never libc free. */
    s->ops.destroy(s->ops.context,&original.term);
    s->ops.destroy(s->ops.context,&original.template_id);
    if(!__atomic_load_n(&s->ready,__ATOMIC_ACQUIRE)){
        s->pin=prepared;__atomic_store_n(&s->ready,1,__ATOMIC_RELEASE);
    }
    goto done;
cleanup:
    s->ops.destroy(s->ops.context,&term);s->ops.destroy(s->ops.context,&tmpl);
done:
    if(held)r31_unlock(s);return result;
}
/* A pin can be inspected for diagnostics under ownership, but cannot publish
   the R27 Stage/rotation delivery proof. No such API exists in this module. */
static int r31_snapshot(R31Rotation*s,R31Pin*out){
    uint32_t tid;R31Identity id;int good;
    if(!s||!s->initialized||!out||!(tid=s->ops.thread(s->ops.context))||!r31_lock(s,tid))return 0;
    good=__atomic_load_n(&s->ready,__ATOMIC_ACQUIRE)&&!__atomic_load_n(&s->fault,__ATOMIC_ACQUIRE)&&
      r31_binding(s,&id)&&r31_same(&id,&s->pin.identity);
    if(good)*out=s->pin;r31_unlock(s);return good;
}
#endif
