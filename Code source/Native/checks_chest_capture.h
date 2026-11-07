#ifndef LID_CHECKS_CHEST_CAPTURE_R73_H
#define LID_CHECKS_CHEST_CAPTURE_R73_H
#include "checks_observer_metadata.h"
#include "death_queue.h"
/* Read-only metadata, not an installed hook. Exact game identity lives in the
   caller. No guessed actor-property offsets, allocation, I/O or game dispatch. */
typedef struct R73ChestLayout {uint64_t vtable,object_class,function;R6Field open,color,kind;} R73ChestLayout;
typedef struct R73ChestSample {uint64_t object,object_class;uint32_t name_index,name_number,open;int32_t color;uint8_t kind;} R73ChestSample;
static int r73_field_valid(const R6Field*f,uint32_t size,int boolean){
    return f&&f->size==size&&f->offset>=0x58&&f->offset<=0x5d0-size&&
      (boolean?(f->mask&&!(f->mask&(f->mask-1))):!f->mask);
}
static int r73_layout_valid(const R73ChestLayout*l){
    if(!l||!l->vtable||!l->object_class||!l->function||!r73_field_valid(&l->open,4,1)||
       !r73_field_valid(&l->color,4,0)||!r73_field_valid(&l->kind,1,0))return 0;
    const R6Field*f[3]={&l->open,&l->color,&l->kind};
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<i;j++)
        if(f[i]->offset<f[j]->offset+f[j]->size&&f[j]->offset<f[i]->offset+f[i]->size)return 0;
    return 1;
}
static int r73_bind(const R6Ops*ops,uint64_t object,uint64_t vtable,uint64_t function,R73ChestLayout*out){
    R73ChestLayout l={0};uint64_t cls,seen[32],owners[2]={0},child,next,meta;uint8_t params;
    unsigned count=0,fields=0,found=0,positions[2]={0};
    if(!out||!vtable||!function||!r6_at(ops,object,0,&l.vtable,8)||l.vtable!=vtable||
       !r6_at(ops,object,0x50,&cls,8)||!cls)return 0;
    l.object_class=cls;
    while(cls){
        if(count==32)return 0;
        for(unsigned i=0;i<count;i++)if(seen[i]==cls)return 0;
        seen[count++]=cls;
        for(unsigned i=0;i<2;i++)if(r6_name(ops,cls,i?"BrgActor_Treasure":"BrgActor_Treasure_Box")){
            if(owners[i])return 0;owners[i]=cls;positions[i]=count;
        }
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!owners[0]||!owners[1]||positions[0]>=positions[1])return 0;
    for(unsigned i=0;i<2;i++){
        if(!r6_at(ops,owners[i],0x80,&child,8))return 0;
        while(child){
            if(++fields>4096||!r6_at(ops,child,0x60,&next,8))return 0;
            if(!i&&r6_name(ops,child,"mOpen")){
                if((found&1)||!r6_property(ops,child,"BoolProperty",4,&l.open))return 0;found|=1;
            }else if(!i&&r6_name(ops,child,"mTreasureBoxType")){
                if((found&2)||!r6_property(ops,child,"IntProperty",4,&l.color))return 0;found|=2;
            }else if(i&&r6_name(ops,child,"mTreasureType")){
                if((found&4)||!r6_property(ops,child,"ByteProperty",1,&l.kind))return 0;found|=4;
            }else if(!i&&r6_name(ops,child,"TreasureBoxOpen")){
                if((found&8)||child!=function||!r6_at(ops,child,0x50,&meta,8)||!r6_name(ops,meta,"Function")||
                   !r6_at(ops,child,0xe1,&params,1)||params)return 0;
                l.function=child;found|=8; /* Script locals at +80 are allowed. */
            }
            child=next;
        }
    }
    if(found!=15||!r73_layout_valid(&l))return 0;*out=l;return 1;
}
static int r73_sample(const R6Ops*ops,uint64_t object,const R73ChestLayout*l,R73ChestSample*out){
    R73ChestSample s={0};uint64_t vt;uint32_t bits;
    if(!out||!r73_layout_valid(l)||!r6_at(ops,object,0,&vt,8)||vt!=l->vtable||
       !r6_at(ops,object,0x50,&s.object_class,8)||s.object_class!=l->object_class||
       !r6_at(ops,object,0x48,&s.name_index,4)||!r6_at(ops,object,0x4c,&s.name_number,4)||
       !r6_at(ops,object,l->open.offset,&bits,4)||!r6_at(ops,object,l->color.offset,&s.color,4)||
       !r6_at(ops,object,l->kind.offset,&s.kind,1))return 0;
    s.object=object;s.open=!!(bits&l->open.mask);*out=s;return 1;
}
static int r73_capture(R15Queue*q,uint32_t owner,const R15Context*context,uint64_t epoch,
    const char floor[16],const char area[32],const R73ChestSample*before,const R73ChestSample*after,int verified){
    if(!q||!context||!floor||!area||!before||!after||verified!=1||!owner||owner!=q->owner||!epoch||
       !r15_context_valid(context)||!r15_same_context(context,&q->context)||
       !r15_ascii(floor,16)||!r15_ascii(area,32)||!r22_map_floor(floor,area)||
       !before->object||!before->object_class||before->object!=after->object||before->object_class!=after->object_class||
       before->name_index!=after->name_index||before->name_number!=after->name_number||
       before->open||after->open!=1||before->color<0||before->color>2||before->color!=after->color||
       before->kind==15||before->kind!=after->kind)return 0;
    R15Frame f;r15_zero(&f,sizeof(f));f.context=*context;f.kind=R72_CHEST;f.epoch=epoch;
    f.raw_kind=(uint32_t)before->color+1;f.name_index=before->name_index;f.name_number=before->name_number;
    r15_copy(f.floor,floor,16);r15_copy(f.area,area,32);r15_copy(f.rotation,"4HMA",5);
    return r15_queue_push(q,owner,&f);
}
#endif
