#ifndef LID_RESEARCH_OBSERVER_METADATA_H
#define LID_RESEARCH_OBSERVER_METADATA_H
/* RESEARCH ONLY. No hook, game API, allocator, file/network I/O or writes to
   the observed memory. All reads supplied by a caller-owned copy callback.
   Metadata offsets use the existing UE3 reader. Pawn field offsets other than
   Health/HealthMax are discovered, never guessed. No check classification. */
#include <stdint.h>
#include <stddef.h>
enum { R6_HEALTH,R6_MAX_HEALTH,R6_DEAD,R6_REMOVE,R6_KIND,R6_DAMAGE,R6_FIELDS };
enum { R6_NATIVE_EXTENT=0xcd8 }; /* constructor writes 8 bytes at +0xcd0 */
typedef struct R6Ops {
    void*context;
    int(*copy)(void*,uint64_t,void*,size_t);
    int(*name_is)(void*,uint64_t,const char*);
} R6Ops;
typedef struct R6Field {uint32_t offset,size,mask;} R6Field;
typedef struct R6Layout {uint64_t vtable,object_class;R6Field field[R6_FIELDS];} R6Layout;
typedef struct R6Sample {
    uint64_t object,object_class,damage_type;
    uint32_t name_index,name_number;
    int32_t health,max_health;
    unsigned dead,removing,kind;
} R6Sample;
static int r6_copy(const R6Ops*ops,uint64_t p,void*out,size_t n){
    return ops&&ops->copy&&out&&p&&n&&p<=UINT64_MAX-n&&ops->copy(ops->context,p,out,n);
}
static int r6_at(const R6Ops*ops,uint64_t p,uint32_t off,void*out,size_t n){
    return p<=UINT64_MAX-off&&r6_copy(ops,p+off,out,n);
}
static int r6_name(const R6Ops*ops,uint64_t p,const char*name){
    return p&&ops&&ops->name_is&&ops->name_is(ops->context,p,name);
}
static int r6_property(const R6Ops*ops,uint64_t prop,const char*type,uint32_t size,R6Field*out){
    uint64_t cls;uint32_t dim,element,offset,mask=0;
    if(!r6_at(ops,prop,0x50,&cls,8)||!r6_name(ops,cls,type)||
       !r6_at(ops,prop,0x68,&dim,4)||dim!=1||
       !r6_at(ops,prop,0x6c,&element,4)||element!=size||
       !r6_at(ops,prop,0x94,&offset,4)||offset>R6_NATIVE_EXTENT-size)return 0;
    if(type[0]=='B'&&type[1]=='o'){
        if(!r6_at(ops,prop,0xb0,&mask,4)||!mask||(mask&(mask-1)))return 0;
    }
    *out=(R6Field){offset,size,mask};return 1;
}
static int r6_layout_valid(const R6Layout*layout){
    static const uint32_t sizes[R6_FIELDS]={4,4,4,4,1,8};unsigned i,j;
    if(!layout||!layout->vtable||!layout->object_class)return 0;
    if(layout->field[0].offset!=0x3cc||layout->field[1].offset!=0x3d0)return 0;
    for(i=0;i<R6_FIELDS;i++){
        const R6Field*a=&layout->field[i];
        if(a->size!=sizes[i]||a->offset>R6_NATIVE_EXTENT-a->size)return 0;
        if(i==R6_DEAD||i==R6_REMOVE){if(!a->mask||(a->mask&(a->mask-1)))return 0;}
        else if(a->mask)return 0;
        for(j=0;j<i;j++){
            const R6Field*b=&layout->field[j];
            if(a->offset<b->offset+b->size&&b->offset<a->offset+a->size){
                if(!a->mask||!b->mask||a->offset!=b->offset||(a->mask&b->mask))return 0;
            }
        }
    }
    return 1;
}
static int r6_bind(const R6Ops*ops,uint64_t object,uint64_t expected_vtable,R6Layout*out){
    static const char*names[R6_FIELDS]={"Health","HealthMax","mbDeath","mbRequestDestroy","meCharaType","mDamageType"};
    static const char*types[R6_FIELDS]={"IntProperty","IntProperty","BoolProperty","BoolProperty","ByteProperty","ClassProperty"};
    static const uint32_t sizes[R6_FIELDS]={4,4,4,4,1,8};
    R6Layout layout={0};uint64_t cls,seen[32],owners[2]={0},child,next;unsigned count=0,fields=0,found=0,i,j;
    unsigned owner_positions[2]={0};
    if(!out||!expected_vtable||!r6_at(ops,object,0,&layout.vtable,8)||layout.vtable!=expected_vtable||
       !r6_at(ops,object,0x50,&cls,8)||!cls)return 0;
    layout.object_class=cls;
    while(cls){
        if(count==32)return 0;
        for(i=0;i<count;i++)if(seen[i]==cls)return 0;
        seen[count++]=cls;
        for(i=0;i<2;i++)if(r6_name(ops,cls,i?"Pawn":"BrgPawn_BaseNative")){
            if(owners[i])return 0;owners[i]=cls;owner_positions[i]=count;
        }
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!owners[0]||!owners[1]||owner_positions[0]>=owner_positions[1])return 0;
    for(i=0;i<2;i++){
        if(!r6_at(ops,owners[i],0x80,&child,8))return 0;
        while(child){
            if(++fields>4096||!r6_at(ops,child,0x60,&next,8))return 0;
            for(j=0;j<R6_FIELDS;j++)if(r6_name(ops,child,names[j])){
                if(((j<2)?1u:0u)!=i||(found&(1u<<j))||!r6_property(ops,child,types[j],sizes[j],&layout.field[j]))return 0;
                found|=1u<<j;
            }
            child=next;
        }
    }
    if(found!=((1u<<R6_FIELDS)-1)||!r6_layout_valid(&layout))return 0;
    *out=layout;return 1;
}
static int r6_sample(const R6Ops*ops,uint64_t object,const R6Layout*layout,R6Sample*out){
    R6Sample sample={0};uint64_t vt;uint32_t flags,remove;uint8_t kind;
    if(!r6_layout_valid(layout)||!out||!r6_at(ops,object,0,&vt,8)||vt!=layout->vtable||
       !r6_at(ops,object,0x50,&sample.object_class,8)||sample.object_class!=layout->object_class||
       !r6_at(ops,object,0x48,&sample.name_index,4)||!r6_at(ops,object,0x4c,&sample.name_number,4)||
       !r6_at(ops,object,layout->field[0].offset,&sample.health,4)||
       !r6_at(ops,object,layout->field[1].offset,&sample.max_health,4)||
       !r6_at(ops,object,layout->field[2].offset,&flags,4)||
       !r6_at(ops,object,layout->field[3].offset,&remove,4)||
       !r6_at(ops,object,layout->field[4].offset,&kind,1)||
       !r6_at(ops,object,layout->field[5].offset,&sample.damage_type,8))return 0;
    sample.object=object;sample.dead=!!(flags&layout->field[2].mask);
    sample.removing=!!(remove&layout->field[3].mask);sample.kind=kind;
    *out=sample;return 1;
}
#endif
