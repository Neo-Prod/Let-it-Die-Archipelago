#ifndef LID_SHOP_TOWER_MEMORY_H
#define LID_SHOP_TOWER_MEMORY_H
#include "momoka_windows_adapter.h"
/* Tower-only name view. Script classes may be loaded after the Momoka adapter
   captured its initial name array. Never update that shared adapter or retain
   pointers from a refreshed table. A concurrent table replacement rejects. */
typedef struct LidTowerMemory {M213Adapter*adapter;uint64_t image;} LidTowerMemory;
static int lid_tm_copy(void*context,uint64_t at,void*out,size_t size){
    LidTowerMemory*m=context;return m&&m->adapter&&m213_copy(m->adapter,at,out,size);
}
static int lid_tm_name(void*context,uint64_t object,const char*wanted){
    LidTowerMemory*m=context;uint64_t first[2],last[2];
    if(!m||!m->adapter||!m->image||m->image>UINT64_MAX-0x295fd28||
       !m213_copy(m->adapter,m->image+0x295fd18,first,16))return 0;
    uint32_t count=(uint32_t)first[1],capacity=(uint32_t)(first[1]>>32);
    if(!first[0]||!count||count>2000000||capacity<count||capacity>2000000)return 0;
    /* m213_name only accesses these initialized members; prepared is unused. */
    M213Adapter local;local.api=m->adapter->api;local.names=first[0];local.count=count;
    int result=m213_name(&local,object,wanted);
    return m213_copy(m->adapter,m->image+0x295fd18,last,16)&&
        first[0]==last[0]&&first[1]==last[1]&&result;
}
#endif
