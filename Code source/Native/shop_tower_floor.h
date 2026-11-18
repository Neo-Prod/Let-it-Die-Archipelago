#ifndef LID_SHOP_TOWER_FLOOR_H
#define LID_SHOP_TOWER_FLOOR_H
#include "shop_tower_stock_view.h"
#include "shop_tower_event_probe.h"
/* Prepared wrapper, not installed or called by the runtime yet. No Save,
   title/connection/global-stock changes. Host and callbacks must remain resident.
   Admission MUST prove real AP Context/policy/catalogue/Wednesday/owner state.
   Native exceptions propagate; finally never attempts speculative engine cleanup. */
typedef uint64_t(*LidTFOriginal)(void*,void*);
typedef void(*LidTFClone)(LidTSArray*,const LidTSArray*);
typedef void(*LidTFClear)(LidTSArray*,int);
typedef struct LidTFOps {
    void*context;
    int(*verified_build)(void*,uintptr_t);
    int(*read)(void*,uintptr_t,void*,size_t);
    int(*writable)(void*,uintptr_t,size_t);
    int(*owned_original)(void*,uintptr_t,uintptr_t,unsigned);
    uint32_t(*thread)(void*);uint32_t(*error_get)(void*);
    void(*error_set)(void*,uint32_t);
    int(*admit)(void*,unsigned,const char*,unsigned);
    int(*producer)(void*); /* verified native response scope, never a UI call */
} LidTFOps;
typedef struct LidTFKey {char room[64];unsigned shop;uintptr_t active_shop;} LidTFKey;
typedef struct LidTowerFloor {
    LidTFOps ops;uintptr_t image;uint32_t owner;unsigned goal,prepared,active,busy,stopped;
    LidTFOriginal original;LidTFClone clone;LidTFClear clear;
    LidTSArray temporary;int last_result;
} LidTowerFloor;
static int lid_tf_read(const LidTowerFloor*h,uintptr_t p,size_t offset,void*out,size_t n){
    return h&&p&&out&&n&&p<=UINTPTR_MAX-offset&&p+offset<=UINTPTR_MAX-n&&
        h->ops.read(h->ops.context,p+offset,out,n)==1;
}
static int lid_tf_id(const LidTowerFloor*h,uintptr_t p,size_t offset,char out[32]){
    LidTSString s;uint16_t text[32];
    if(!lid_tf_read(h,p,offset,&s,16)||s.count<2||s.count>32||s.capacity<s.count||s.capacity>4096||
       !lid_tf_read(h,(uintptr_t)s.data,0,text,(size_t)s.count*2)||text[s.count-1])return 0;
    for(int i=0;i<s.count-1;i++){
        unsigned c=text[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return 0;
        out[i]=(char)c;
    }out[s.count-1]=0;return 1;
}
static int lid_tf_key_probe(const LidTowerFloor*h,uintptr_t model,LidTFKey*out,unsigned probe){
    uint8_t valid;int32_t master;LidTSString s;uint16_t id[12];char floor[32],area[32];uintptr_t shop;
    if(!out||!lid_tf_read(h,model,0x518,&valid,1)){r299_note(15,probe,0);return 0;}
    if(!valid){r299_note(15,probe+1,0);return 0;}
    if(!lid_tf_read(h,model,0x460,&master,4)||master<1||master>1000){r299_note(15,probe+2,0);return 0;}
    if(!lid_tf_id(h,model,8,floor)||!lid_tf_id(h,model,0x18,area)){r299_note(15,probe+3,0);return 0;}
    if(!lid_tf_read(h,model,0x5a0,&shop,8)||!shop){r299_note(15,probe+4,0);return 0;}
    if(!lid_tf_read(h,shop,8,&s,16)||s.count<2||s.count>11||s.capacity<s.count||s.capacity>4096){r299_note(15,probe+5,0);return 0;}
    if(!lid_tf_read(h,(uintptr_t)s.data,0,id,(size_t)s.count*2)||id[s.count-1]){r299_note(15,probe+6,0);return 0;}
    unsigned actual=0;
    for(int i=0;i<s.count-1;i++){
        if(id[i]<'0'||id[i]>'9'||actual>1000){r299_note(15,probe+7,0);return 0;}actual=actual*10+id[i]-'0';
    }
    if(actual!=(unsigned)master){r299_note(15,probe+8,actual);return 0;}
    unsigned i=0,j=0;while(floor[i]){out->room[i]=floor[i];i++;}
    out->room[i++]='|';while(area[j])out->room[i++]=area[j++];out->room[i]=0;
    out->shop=actual;out->active_shop=shop;r299_note(15,probe+9,actual);return 1;
}
static int lid_tf_key(const LidTowerFloor*h,uintptr_t model,LidTFKey*out){return lid_tf_key_probe(h,model,out,10);}
static int lid_tf_output(const LidTowerFloor*h,uintptr_t output,const LidTFKey*key,LidTSArray*out){
    char floor[32],area[32],room[64];unsigned i=0,j=0;
    if(!key||!out||!lid_tf_id(h,output,0x20,floor)||!lid_tf_id(h,output,0x34,area))return 0;
    while(floor[i]){room[i]=floor[i];i++;}room[i++]='|';while(area[j])room[i++]=area[j++];room[i]=0;
    if(!lid_ts_room(room,key->room)||!lid_tf_read(h,output,0x11c,out,16)||
       out->count<0||out->count>1||out->capacity<out->count||out->capacity>4096||
       (out->capacity&&!out->data)||(!out->capacity&&out->data)||
       (uintptr_t)out->data>UINTPTR_MAX-(size_t)out->capacity*32)return 0;
    return output<=UINTPTR_MAX-0x234&&h->ops.writable(h->ops.context,output+0x11c,16)==1;
}
static int lid_tf_same_array(const LidTSArray*a,const LidTSArray*b){
    return a->data==b->data&&a->count==b->count&&a->capacity==b->capacity;
}
static int lid_tf_prefix(const LidTowerFloor*h){
    static const uint8_t expected[3][24]={
        {0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x70,0x48,0xc7,0x45,0xb0,0xfe},
        {0x48,0x3b,0xca,0x0f,0x84,0xed,0,0,0,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30,0x48},
        {0x40,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89}};
    static const unsigned rva[3]={0x11fd1a0,0xe07fe0,0xe38590};uint8_t bytes[24];
    for(unsigned i=0;i<3;i++){
        if(!lid_tf_read(h,h->image,rva[i],bytes,24))return 0;
        for(unsigned j=0;j<24;j++)if(bytes[j]!=expected[i][j])return 0;
    }return 1;
}
static int lid_tf_prepare(LidTowerFloor*h,uintptr_t image,unsigned goal,uint32_t owner,const LidTFOps*ops){
    if(!h||h->prepared||h->active||h->busy||h->stopped||h->original||
       h->temporary.data||h->temporary.count||h->temporary.capacity||
       !ops||!image||image>UINTPTR_MAX-0x11fd1b8||!owner||
       goal<10||goal>40||goal%10||!ops->verified_build||!ops->read||!ops->writable||
       !ops->owned_original||!ops->thread||!ops->error_get||!ops->error_set||!ops->admit||
       ops->thread(ops->context)!=owner||ops->verified_build(ops->context,image)!=1)return 0;
    h->ops=*ops;h->image=image;h->goal=goal;h->owner=owner;
    if(!lid_tf_prefix(h))return 0;
    h->clone=(LidTFClone)(image+0xe07fe0);h->clear=(LidTFClear)(image+0xe38590);
    h->prepared=1;return 1;
}
static int lid_tf_bind_original(LidTowerFloor*h,LidTFOriginal original){
    if(!h||!h->prepared||h->original||h->active||!original||
       h->ops.thread(h->ops.context)!=h->owner||
       h->ops.owned_original(h->ops.context,h->image+0x11fd1a0,(uintptr_t)original,15)!=1)return 0;
    __atomic_store_n(&h->original,original,__ATOMIC_RELEASE);return 1;
}
/* Resident host storage starts zeroed. No retries after an abnormal native call.
   First clone into OWNED temporary; revalidate admission/output before swapping.
   Displaced old native array remains in the host until native destruction ends. */
static int lid_tf_after(LidTowerFloor*h,uintptr_t model,uintptr_t output){
    LidTFKey key,again;LidTSArray previous,current;LidTowerStockView view;
    if(model>UINTPTR_MAX-0x5a8||output>UINTPTR_MAX-0x234||
       !(model+0x5a8<=output||output+0x234<=model)||h->temporary.data||h->temporary.count||h->temporary.capacity){r299_note(15,1,0);r299_note(1,1,0);return 0;}
    if(!lid_tf_key(h,model,&key)){r299_note(15,2,0);r299_note(1,1,0);return 0;}
    if(h->ops.admit(h->ops.context,h->goal,key.room,key.shop)!=1){r299_note(15,3,key.shop);r299_note(1,1,0);return 0;}
    if(!lid_ts_build(h->goal,key.room,key.shop,&view)){r299_note(15,4,key.shop);r299_note(1,1,0);return 0;}
    if(!lid_tf_output(h,output,&key,&previous)){r299_note(15,5,key.shop);r299_note(1,1,0);return 0;}
    h->clone(&h->temporary,&view.shops);
    if(!h->temporary.data||h->temporary.count!=1||h->temporary.capacity<1||h->temporary.capacity>4096||
       (uintptr_t)h->temporary.data>UINTPTR_MAX-(size_t)h->temporary.capacity*32||
       !((uintptr_t)h->temporary.data+32<=(uintptr_t)&view||
          (uintptr_t)&view+sizeof(view)<=(uintptr_t)h->temporary.data)){r299_note(1,2,0);return -1;}
    if(!lid_tf_key(h,model,&again)||again.shop!=key.shop||again.active_shop!=key.active_shop||
       !lid_ts_room(again.room,key.room)||h->ops.admit(h->ops.context,h->goal,key.room,key.shop)!=1||
       !lid_tf_output(h,output,&key,&current)||!lid_tf_same_array(&previous,&current)){
        h->clear(&h->temporary,0);{r299_note(1,3,0);return 0;}
    }
    *(LidTSArray*)(output+0x11c)=h->temporary;h->temporary=previous;
    h->clear(&h->temporary,0);{r299_note(1,4,0);return 1;}
}
#if defined(LID_SHOP_FLOOR_SEH_ENABLED) && LID_SHOP_FLOOR_SEH_ENABLED
/* No non-finally wrapper fallback. Publisher supplies a proven resident original.
   Original result and GetLastError are retained even when AP normalization skips. */
static uint64_t lid_tf_call(LidTowerFloor*h,void*model,void*output){
    LidTFOriginal original=__atomic_load_n(&h->original,__ATOMIC_ACQUIRE);
    uint64_t result=original(model,output);uint32_t error=h->ops.error_get(h->ops.context);
    if(r299_flag(0,2,result==1)&&r299_flag(0,4,__atomic_load_n(&h->active,__ATOMIC_ACQUIRE))&&
       r299_flag(0,6,!__atomic_load_n(&h->stopped,__ATOMIC_ACQUIRE))&&
       r299_flag(0,8,h->ops.producer?h->ops.producer(h->ops.context)==1:h->ops.thread(h->ops.context)==h->owner)){
        unsigned empty=0;
        if(__atomic_compare_exchange_n(&h->busy,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
            unsigned normal=0;
            __try{
                h->last_result=lid_tf_after(h,(uintptr_t)model,(uintptr_t)output);
                r299_note(0,20+(unsigned)(h->last_result+1),0);
                if(h->last_result<0)__atomic_store_n(&h->stopped,1,__ATOMIC_RELEASE);
                normal=1;
            }__finally{
                if(!normal)__atomic_store_n(&h->stopped,1,__ATOMIC_RELEASE);
                __atomic_store_n(&h->busy,0,__ATOMIC_RELEASE);h->ops.error_set(h->ops.context,error);
            }
        }
    }
    h->ops.error_set(h->ops.context,error);return result;
}
#endif
#endif
