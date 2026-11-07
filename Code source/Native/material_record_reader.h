#ifndef LID_RESEARCH_MATERIAL_RECORD_R81_H
#define LID_RESEARCH_MATERIAL_RECORD_R81_H
/* Research candidate. Read-only callbacks, no installation or game writes.
   Offsets are for the verified f33e...bddfd executable and must match metadata. */
#include "checks_observer_metadata.h"
enum {R81_EXTENT=0x92e0c,R81_COUNT=496,R81_STRIDE=0x4b4};
enum {R81_NUM,R81_UNITS,R81_STATE,R81_SELECTED,R81_NEAREST,R81_BAG_SLOT,
      R81_GET,R81_CANCEL,R81_DELETE_CANCEL,R81_PLAYER_DEATH,R81_FIELDS};
typedef struct R81Layout {uint64_t vtable,object_class,owner;R6Field fields[R81_FIELDS];} R81Layout;
typedef struct R81Sample {
    uint64_t manager,actor,actor_class;uint32_t actor_name,actor_number;
    int32_t slot,unique,count,bag_slot,gettime;uint32_t state,flags;
    char item_id[64],entity_id[96];
} R81Sample;
/* CD4 is mCursorPos2, used for free-slot/menu choice by native material pickup.
   The distinct mDeathBagAddSlotIndex belongs atCF4 (equipment path). */
static const char* const r81_names[R81_FIELDS]={"mUnitNum","mUnit","mState","mSelectIndex","mMinDistanceIndex","mCursorPos2","mItemGetFlag","mItemGetCancelFlag","mItemDeleteCancelFlag","mPlayerDeath"};
static const uint32_t r81_offsets[R81_FIELDS]={0x70,0x74,0x92cc0,0x92cc8,0x92cd8,0x92cd4,0x92cf8,0x92cf8,0x92cf8,0x92cf8};
static uint32_t r81_size(unsigned i){return i==R81_UNITS?R81_STRIDE:i==R81_STATE?1:4;}
static uint32_t r81_dimension(unsigned i){return i==R81_UNITS?R81_COUNT:1;}
static const char*r81_type(unsigned i){return i==R81_UNITS?"StructProperty":i==R81_STATE?"ByteProperty":i>=R81_GET?"BoolProperty":"IntProperty";}
static uint32_t r81_mask(unsigned i){return i>=R81_GET?1u<<(i-R81_GET):0;}
static int r81_property(const R6Ops*ops,uint64_t p,unsigned i,R6Field*out){
    uint64_t cls;uint32_t dim,size,off,mask=0;
    if(i>=R81_FIELDS||!out||!r6_at(ops,p,0x50,&cls,8)||!r6_name(ops,cls,r81_type(i))||
       !r6_at(ops,p,0x68,&dim,4)||dim!=r81_dimension(i)||
       !r6_at(ops,p,0x6c,&size,4)||size!=r81_size(i)||
       !r6_at(ops,p,0x94,&off,4)||off!=r81_offsets[i]||
       (uint64_t)off+(uint64_t)size*dim>R81_EXTENT)return 0;
    if(i>=R81_GET&&(!r6_at(ops,p,0xb0,&mask,4)||mask!=r81_mask(i)))return 0;
    *out=(R6Field){off,size,mask};return 1;
}
static int r81_layout_valid(const R81Layout*l){
    if(!l||!l->vtable||!l->object_class||!l->owner)return 0;
    for(unsigned i=0;i<R81_FIELDS;i++)if(l->fields[i].offset!=r81_offsets[i]||
       l->fields[i].size!=r81_size(i)||l->fields[i].mask!=r81_mask(i))return 0;
    return 1;
}
static int r81_bind(const R6Ops*ops,uint64_t object,uint64_t expected_vtable,R81Layout*out){
    R81Layout l={0};uint64_t cls,seen[32],owner=0,next,child,properties[512];unsigned n=0,used=0,found=0;
    if(!out||!expected_vtable||!r6_at(ops,object,0,&l.vtable,8)||l.vtable!=expected_vtable||
       !r6_at(ops,object,0x50,&cls,8)||!cls)return 0;
    l.object_class=cls;
    while(cls){
        if(n==32)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
        if(r6_name(ops,cls,"BrgDropItemManager")){if(owner)return 0;owner=cls;}
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!owner||!r6_at(ops,owner,0x80,&child,8))return 0;l.owner=owner;
    while(child){
        if(used==512)return 0;for(unsigned i=0;i<used;i++)if(properties[i]==child)return 0;properties[used++]=child;
        if(!r6_at(ops,child,0x60,&next,8))return 0;
        for(unsigned i=0;i<R81_FIELDS;i++)if(r6_name(ops,child,r81_names[i])){
            if(found&(1u<<i)||!r81_property(ops,child,i,&l.fields[i]))return 0;found|=1u<<i;
        }
        child=next;
    }
    if(found!=(1u<<R81_FIELDS)-1||!r81_layout_valid(&l))return 0;*out=l;return 1;
}
/* Engine FString: pointer, Num (including terminator), Max. IDs are ASCII,
   never localization text. Empty entity IDs are allowed, malformed ones aren't.
   No truncation and no direct dereference of engine pointers. */
static int r81_id(const R6Ops*ops,uint64_t at,char*out,unsigned cap,int required){
    struct {uint64_t data;int32_t num,max;} s;uint16_t units[96];
    if(!out||!cap||cap>96||!r6_copy(ops,at,&s,sizeof(s))||s.num<0||s.max<0||s.num>s.max||s.max>4096)return 0;
    if(!s.num){if(required)return 0;out[0]=0;return 1;}
    if(!s.data||(unsigned)s.num>cap||!r6_copy(ops,s.data,units,(size_t)s.num*2)||units[s.num-1])return 0;
    for(int i=0;i<s.num-1;i++)if(units[i]<32||units[i]>126)return 0;
    if(required&&s.num<2)return 0;
    for(int i=0;i<s.num;i++)out[i]=(char)units[i];return 1;
}
/* 1=valid material sample,0=valid nonmaterial record,-1=unproved/stale.
   A caller must not interpret -1 as permission to mutate inventory. */
static int r81_sample(const R6Ops*ops,uint64_t manager,const R81Layout*l,R81Sample*out){
    R81Sample s={0};uint64_t vt,cls,record;uint32_t use;uint8_t state,type;
    if(!out||manager>UINT64_MAX-R81_EXTENT||!r81_layout_valid(l)||!r6_at(ops,manager,0,&vt,8)||vt!=l->vtable||
       !r6_at(ops,manager,0x50,&cls,8)||cls!=l->object_class||
       !r6_at(ops,manager,r81_offsets[R81_NUM],&s.count,4)||s.count<1||s.count>R81_COUNT||
       !r6_at(ops,manager,r81_offsets[R81_SELECTED],&s.slot,4)||s.slot<0||s.slot>=R81_COUNT||
       !r6_at(ops,manager,r81_offsets[R81_STATE],&state,1)||state>5||
       !r6_at(ops,manager,r81_offsets[R81_GET],&s.flags,4)||
       !r6_at(ops,manager,r81_offsets[R81_BAG_SLOT],&s.bag_slot,4))return -1;
    uint64_t offset=0x74+(uint64_t)(uint32_t)s.slot*R81_STRIDE;
    if(manager>UINT64_MAX-offset)return -1;record=manager+offset;
    if(!r6_at(ops,record,0,&use,4)||!(use&1)||!r6_at(ops,record,4,&s.unique,4)||s.unique<0||
       !r6_at(ops,record,0xe4,&type,1))return -1;
    if(type!=4)return 0;
    if(!r6_at(ops,record,0x14,&s.actor,8)||!s.actor||
       !r6_at(ops,s.actor,0x50,&s.actor_class,8)||!s.actor_class||
       !r6_at(ops,s.actor,0x48,&s.actor_name,4)||!r6_at(ops,s.actor,0x4c,&s.actor_number,4)||
       !r81_id(ops,record+0x220,s.entity_id,96,0)||!r81_id(ops,record+0x230,s.item_id,64,1)||
       !r6_at(ops,record,0x240,&s.gettime,4))return -1;
    s.manager=manager;s.state=state;*out=s;return 1;
}
#endif
