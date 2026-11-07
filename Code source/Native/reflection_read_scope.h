#ifndef LID_REFLECTION_READ_SCOPE_H
#define LID_REFLECTION_READ_SCOPE_H
#ifndef LID_REFLECTION_READ_SCOPE_ENABLED
#define LID_REFLECTION_READ_SCOPE_ENABLED 0
#endif
#if LID_EVENT_PROFILE_ENABLED
static void lid_perf_detail_count(unsigned,unsigned long long);
#define LID_SCOPE_COUNT(id) lid_perf_detail_count(id,1)
#else
#define LID_SCOPE_COUNT(id) ((void)0)
#endif
/* VM metadata only, never UObject/FName values. One synchronous reflection
   traversal, no engine call or allocation while active. The owning thread
   alone accesses the eight cache entries; another thread always queries VM.
   End invalidates all entries. Changing protection concurrently remains a
   TOCTOU risk, as with every validation followed by an ordinary load. */
typedef SIZE_T (WINAPI *LidReadScopeQuery)(LPCVOID,MEMORY_BASIC_INFORMATION*,SIZE_T);
typedef DWORD (WINAPI *LidReadScopeThread)(void);
typedef struct LidReadScope {
    unsigned owner,count,next;
    MEMORY_BASIC_INFORMATION regions[8];
} LidReadScope;
static int lid_read_scope_begin(LidReadScope*s,LidReadScopeThread thread){
    unsigned expected=0,tid;
    if(!s||!thread||!(tid=thread()))return 0;
    if(!__atomic_compare_exchange_n(&s->owner,&expected,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    s->count=0;s->next=0;return 1;
}
static void lid_read_scope_end(LidReadScope*s,LidReadScopeThread thread,int acquired){
    if(!acquired||!s||!thread||__atomic_load_n(&s->owner,__ATOMIC_ACQUIRE)!=thread())return;
    s->count=0;s->next=0;__atomic_store_n(&s->owner,0,__ATOMIC_RELEASE);
}
static SIZE_T lid_read_scope_query(LidReadScope*s,LidReadScopeThread thread,LidReadScopeQuery query,
                                 LPCVOID p,MEMORY_BASIC_INFORMATION*out,SIZE_T size){
    ULONG_PTR point=(ULONG_PTR)p;unsigned i,tid;SIZE_T result;int own=0;
    if(!query)return 0;
    if(s&&thread&&size==sizeof(*out)&&out&&
       (tid=__atomic_load_n(&s->owner,__ATOMIC_ACQUIRE))&&tid==thread())own=1;
    if(own){
        for(i=0;i<s->count;i++){
            ULONG_PTR base=(ULONG_PTR)s->regions[i].BaseAddress;
            ULONG_PTR end=base+(ULONG_PTR)s->regions[i].RegionSize;
            if(end>=base&&point>=base&&point<end){LID_SCOPE_COUNT(6);*out=s->regions[i];return sizeof(*out);}
        }
    }
    if(own)LID_SCOPE_COUNT(7);
    result=query(p,out,size);
    if(own&&result==sizeof(*out)){
        ULONG_PTR base=(ULONG_PTR)out->BaseAddress,end=base+(ULONG_PTR)out->RegionSize;
        if(end>base&&point>=base&&point<end){
            s->regions[s->next]=*out;s->next=(s->next+1)%8;if(s->count<8)s->count++;
        }
    }
    return result;
}
#endif
