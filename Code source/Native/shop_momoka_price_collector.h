#ifndef LID_SHOP_MOMOKA_PRICE_COLLECTOR_H
#define LID_SHOP_MOMOKA_PRICE_COLLECTOR_H
#include "shop_momoka_price_binding.h"
#include "momoka_windows_adapter.h"
#include "momoka_owned_slot.h"
/* Passive resident host. No publication, engine writes, allocations or IO.
   Runtime must prepare at final zeroed address on established game writer,
   verify build/lifetime, then publish an owned getter slot separately. Never
   rebind or free while a wrapper can refer to this host. RPM is not a GC lock. */
enum { LID_MPC_LIMIT=32, LID_MPC_NO_OFFER=4096 };
typedef struct LidMPCollectedPrice {
    unsigned catalogued;
    LidMPPriceObservation price;
} LidMPCollectedPrice;
typedef struct LidMPPriceCollector {
    unsigned ready,busy,stopped,drained,attempts,count,scoped;
    uint32_t owner;
    uint64_t database,database_class,wrapper;
    M213Adapter*windows;M195Ops memory;LidMPPriceBinding binding;LidMPPostHost host;
    void*run_context;int(*live)(void*);
    void*catalogue_context;int(*offer)(void*,const uint16_t*,unsigned,unsigned*);
    LidMPCollectedPrice records[LID_MPC_LIMIT];
    const M207Slot*entry; /* Optional existing resident GUARDED owner, not a copy. */
} LidMPPriceCollector;
/* Borrow only the exact published owner. No arbitrary replacement callback or
   permission bit: all identities and the actual native slot are rechecked. */
static int lid_mpc_owned(const M195Ops*o,const M213Adapter*w,
                         const M207Slot*entry,uint64_t wrapper){
    uint64_t actual=0;
    return o&&w&&entry&&__atomic_load_n(&entry->state,__ATOMIC_ACQUIRE)==M207_INSTALLED&&
      (uintptr_t)entry->slot==w->prepared.native.slot&&
      entry->original==w->prepared.native.original&&entry->wrapper==wrapper&&
      wrapper&&wrapper!=entry->original&&
      m195_read(o,w->prepared.native.slot,0,&actual,8)&&actual==wrapper&&
      __atomic_load_n(&entry->state,__ATOMIC_ACQUIRE)==M207_INSTALLED;
}
static uint32_t lid_mpc_error(void*context){
    LidMPPriceCollector*c=context;return c->windows->api.get_error();
}
static void lid_mpc_error_set(void*context,uint32_t error){
    LidMPPriceCollector*c=context;c->windows->api.set_error(error);
}
static void lid_mpc_leave(void*context){
    LidMPPriceCollector*c=context;c->scoped=0;
    __atomic_store_n(&c->busy,0,__ATOMIC_RELEASE);
}
static int lid_mpc_enter(void*context,uint64_t receiver,uint64_t frame,
                         M195Ops*out,const LidMPPriceBinding**binding,unsigned*offer){
    LidMPPriceCollector*c=context;unsigned empty=0;uint64_t cls,node,slot;
    if(!c||!c->ready||__atomic_load_n(&c->stopped,__ATOMIC_ACQUIRE)||
       !out||!binding||!offer||!frame||receiver!=c->database||
       c->windows->api.thread()!=c->owner)return 0;
    if(!__atomic_compare_exchange_n(&c->busy,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    if(__atomic_load_n(&c->stopped,__ATOMIC_ACQUIRE)||c->attempts>=LID_MPC_LIMIT||
       c->count>=LID_MPC_LIMIT)goto refuse;
    /* Numeric frame-node filter first: unrelated database getters do not
       query session, native slot, object classes or property/name chains. */
    if(!m195_read(&c->memory,frame,0x14,&node,8)||
       (node!=c->binding.buy.function&&node!=c->binding.add.function))goto refuse;
    if(!c->live(c->run_context))goto retire;
    if(!m195_read(&c->memory,c->windows->prepared.native.slot,0,&slot,8)||slot!=c->wrapper||
       !m195_read(&c->memory,receiver,0x50,&cls,8)||cls!=c->database_class||
       !m195_read(&c->memory,c->binding.scripts.menu,0x50,&cls,8)||cls!=c->binding.scripts.menu_class)goto retire;
    c->attempts++;c->scoped=1;
    if(c->attempts==LID_MPC_LIMIT)__atomic_store_n(&c->stopped,1,__ATOMIC_RELEASE);
    /* Zero is temporary reader input ONLY, never emitted as a catalogue ID.
       A raw diagnostic record is explicitly overwritten to NO_OFFER. */
    *offer=0;*out=c->memory;*binding=&c->binding;return 1;
retire:__atomic_store_n(&c->stopped,1,__ATOMIC_RELEASE);
refuse:lid_mpc_leave(c);return 0;
}
static void lid_mpc_record(void*context,const LidMPPriceObservation*observation){
    LidMPPriceCollector*c=context;LidMPCollectedPrice record={0};unsigned offer=LID_MPC_NO_OFFER;
    if(!c||!observation||!c->scoped||c->count>=LID_MPC_LIMIT||
       c->windows->api.thread()!=c->owner||!c->live(c->run_context))return;
    record.price=*observation;record.price.row.offer=LID_MPC_NO_OFFER;
    if(c->offer){
        int result=c->offer(c->catalogue_context,observation->row.skill,observation->row.units,&offer);
        if(result<0||result>1||(result==1&&offer>=LID_MPC_NO_OFFER))return;
        if(result==1){record.catalogued=1;record.price.row.offer=offer;}
    }
    c->records[c->count++]=record;
    if(c->count==LID_MPC_LIMIT)__atomic_store_n(&c->stopped,1,__ATOMIC_RELEASE);
}
/* Derive shared skill struct from verified getter's actual output property,
   not from a guessed object address or caller-local offset. Cold traversal. */
static int lid_mpc_structure(const M195Ops*o,uint64_t function,uint64_t*out){
    uint64_t p,next,seen[512],structure=0;unsigned n=0;uint32_t size;
    if(!out||!m195_read(o,function,0x80,&p,8))return 0;
    while(p){
        if(n==512)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==p)return 0;seen[n++]=p;
        if(!m195_read(o,p,0x60,&next,8))return 0;
        if(m195_named(o,p,"skl")){
            if(structure||!m195_read(o,p,0x6c,&size,4)||size!=196||
               !m195_read(o,p,0xb0,&structure,8)||!m195_named(o,structure,"BrgDbSkill"))return 0;
        }
        p=next;
    }
    if(!structure)return 0;*out=structure;return 1;
}
/* Adapter must already have verified the exact native signature before any
   replacement. No slot write here. live validates captured run/root lifetime;
   it is NOT supplied as a generic always-true quiescence flag. offer is optional
   for raw diagnostics only; real transactions require authenticated catalogue. */
static int lid_mpc_prepare_impl(LidMPPriceCollector*c,M213Adapter*windows,
    uint64_t database,uint64_t menu,uint32_t owner,uint64_t expected_wrapper,
    void*run_context,int(*live)(void*),void*catalogue_context,
    int(*offer)(void*,const uint16_t*,unsigned,unsigned*),const M207Slot*entry){
    M196Binding scripts={0};uint64_t structure,cls,slot;uint32_t error;int ok=0;
    if(!c||c->ready||!windows||!windows->api.get_error||!windows->api.set_error||
       !windows->api.thread||!windows->api.read||!windows->api.process||!owner||
       !expected_wrapper||!live||!windows->prepared.native.original||
       !windows->prepared.native.function||!windows->prepared.native.slot||
       expected_wrapper==windows->prepared.native.original)return 0;
    error=windows->api.get_error();
    M195Ops memory={windows,m213_copy,m213_name};
    if(windows->api.thread()!=owner||windows->prepared.gate.owner!=owner||!live(run_context)||
       !m195_read(&memory,database,0x50,&cls,8)||!m195_named(&memory,cls,"BrgDatabase")||
       !m195_read(&memory,windows->prepared.native.slot,0,&slot,8)||
       (entry?!lid_mpc_owned(&memory,windows,entry,expected_wrapper):slot!=windows->prepared.native.original)||
       !m196_bind(&memory,menu,&scripts)||
       !lid_mpc_structure(&memory,windows->prepared.native.function,&structure)||
       !lid_mp_price_bind(&memory,&scripts,structure,&c->binding))goto done;
    c->windows=windows;c->memory=memory;c->owner=owner;c->database=database;c->database_class=cls;
    c->wrapper=expected_wrapper;c->entry=entry;c->run_context=run_context;c->live=live;
    c->catalogue_context=catalogue_context;c->offer=offer;
    c->host=(LidMPPostHost){c,(void(*)(void*,void*,void*))(uintptr_t)windows->prepared.native.original,
        lid_mpc_error,lid_mpc_error_set,lid_mpc_enter,lid_mpc_leave,lid_mpc_record};
    c->ready=1;ok=1;
done:windows->api.set_error(error);return ok;
}
static int lid_mpc_prepare(LidMPPriceCollector*c,M213Adapter*windows,
    uint64_t database,uint64_t menu,uint32_t owner,uint64_t expected_wrapper,
    void*run_context,int(*live)(void*),void*catalogue_context,
    int(*offer)(void*,const uint16_t*,unsigned,unsigned*)){
    return lid_mpc_prepare_impl(c,windows,database,menu,owner,expected_wrapper,
        run_context,live,catalogue_context,offer,0);
}
static int lid_mpc_prepare_owned(LidMPPriceCollector*c,M213Adapter*windows,
    uint64_t database,uint64_t menu,uint32_t owner,const M207Slot*entry,
    void*run_context,int(*live)(void*),void*catalogue_context,
    int(*offer)(void*,const uint16_t*,unsigned,unsigned*)){
    return entry&&lid_mpc_prepare_impl(c,windows,database,menu,owner,entry->wrapper,
        run_context,live,catalogue_context,offer,entry);
}
/* Consumer is established writer, outside native callback. Single nonblocking
   claim; no partial copy, borrowed pointer or repeated drain. Stop is permanent.
   Does not restore or unpublish a slot (that is a separate owned operation). */
static int lid_mpc_take(LidMPPriceCollector*c,LidMPCollectedPrice*out,unsigned cap,unsigned*count){
    unsigned empty=0;
    if(!c||!c->ready||!out||!count||cap<LID_MPC_LIMIT||c->windows->api.thread()!=c->owner||
       !__atomic_compare_exchange_n(&c->busy,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    int ok=0;
    if(!c->scoped&&!c->drained&&c->count<=LID_MPC_LIMIT){
        __atomic_store_n(&c->stopped,1,__ATOMIC_RELEASE);
        for(unsigned i=0;i<c->count;i++)out[i]=c->records[i];
        *count=c->count;c->drained=1;ok=1;
    }
    __atomic_store_n(&c->busy,0,__ATOMIC_RELEASE);return ok;
}
#endif
