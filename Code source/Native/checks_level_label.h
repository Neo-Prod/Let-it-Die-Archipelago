#ifndef LID_RESEARCH_LEVEL_LABEL_H
#define LID_RESEARCH_LEVEL_LABEL_H
#include "checks_observer_metadata.h"
enum {R11_LABEL_CAP=96};
typedef struct R11String {uint64_t data;int32_t num,max;} R11String;
typedef struct R11Label {uint16_t text[R11_LABEL_CAP];unsigned length,mk;} R11Label;
static int r11_space(uint16_t c){return c==32||c==9||c==0xa0||c==0x3000;}
/* Explicit MK token only, never a guess based on combat level or texture.
   Accepted punctuation is provisional until a real label is captured. */
static unsigned r11_parse_mk(const uint16_t*s,unsigned n,unsigned family){
    unsigned result=0,markers=0;
    if(!s||n>=R11_LABEL_CAP||family<1||family>6)return 0;
    for(unsigned i=0;i+1<n;i++){
        if((s[i]!='M'&&s[i]!='m')||(s[i+1]!='K'&&s[i+1]!='k')||(i&&!r11_space(s[i-1])))continue;
        markers++;unsigned j=i+2;
        while(j<n&&r11_space(s[j]))j++;
        if(j<n&&(s[j]=='.'||s[j]=='-'))j++;
        while(j<n&&r11_space(s[j]))j++;
        if(j>=n||s[j]<'1'||s[j]>'5')continue;
        unsigned mk=s[j++]-'0';while(j<n&&r11_space(s[j]))j++;
        if(j==n)result=mk;
    }
    return markers==1?result:0;
}
static int r11_utf16(const uint16_t*s,unsigned n){
    for(unsigned i=0;i<n;i++){
        unsigned c=s[i];if(!c||c<32||c==127)return 0;
        if(c>=0xd800&&c<=0xdbff){if(++i>=n||s[i]<0xdc00||s[i]>0xdfff)return 0;}
        else if(c>=0xdc00&&c<=0xdfff)return 0;
    }
    return 1;
}
/* address is a validated reflected FString property's address, NOT a Pawn.
   Metadata binding, object lifetime, death context and family are the caller's
   responsibility. This helper alone cannot validate those prerequisites. */
static int r11_read_label(const R6Ops*ops,uint64_t address,unsigned family,R11Label*out){
    R11String a,b;R11Label sample={0};uint16_t again[R11_LABEL_CAP];
    if(!out||!r6_copy(ops,address,&a,sizeof(a))||a.num<0||a.num>R11_LABEL_CAP||a.max<a.num)return 0;
    if(a.num){
        if(!a.data||(a.data&1)||!r6_copy(ops,a.data,sample.text,(size_t)a.num*2)||sample.text[a.num-1])return 0;
        sample.length=(unsigned)a.num-1;if(!r11_utf16(sample.text,sample.length))return 0;
        if(!r6_copy(ops,a.data,again,(size_t)a.num*2))return 0;
        for(int i=0;i<a.num;i++)if(again[i]!=sample.text[i])return 0;
    }
    if(!r6_copy(ops,address,&b,sizeof(b))||a.data!=b.data||a.num!=b.num||a.max!=b.max)return 0;
    sample.mk=r11_parse_mk(sample.text,sample.length,family);*out=sample;return 1;
}
typedef struct R11Layout {R6Layout common;R6Field name,label;unsigned family;} R11Layout;
static int r11_layout_valid(const R11Layout*l){
    if(!l||!r6_layout_valid(&l->common)||l->family<1||l->family>6)return 0;
    const R6Field*fields[2]={&l->name,&l->label};
    for(unsigned i=0;i<2;i++){
        const R6Field*a=fields[i];if(a->size!=16||a->mask||a->offset>R6_NATIVE_EXTENT-16)return 0;
        for(unsigned j=0;j<R6_FIELDS;j++){
            const R6Field*b=&l->common.field[j];if(a->offset<b->offset+b->size&&b->offset<a->offset+16)return 0;
        }
    }
    return !(l->name.offset<l->label.offset+16&&l->label.offset<l->name.offset+16);
}
static int r11_bind_tuber(const R6Ops*ops,uint64_t object,uint64_t vtable,R11Layout*out){
    static const char*families[6]={"BrgPawn_Scratch","BrgPawn_Hovering","BrgPawn_Turret","BrgPawn_Bone","BrgPawn_Reversal","BrgPawn_Treasure"};
    R11Layout layout={0};uint64_t cls,owner=0,seen[32],child,next;unsigned count=0,normal=0,native=0,family_position=0,found=0,steps=0;
    if(!out||!r6_bind(ops,object,vtable,&layout.common))return 0;
    cls=layout.common.object_class;
    while(cls){
        if(count==32)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==cls)return 0;seen[count++]=cls;
        for(unsigned i=0;i<6;i++)if(r6_name(ops,cls,families[i])){if(layout.family)return 0;layout.family=i+1;family_position=count;}
        if(r6_name(ops,cls,"BrgPawn_NormalEnemyBase")){if(normal)return 0;normal=count;}
        if(r6_name(ops,cls,"BrgPawn_BaseNative")){if(owner)return 0;owner=cls;native=count;}
        if(!r6_at(ops,cls,0x78,&next,8))return 0;cls=next;
    }
    if(!layout.family||!normal||!owner||family_position>=normal||normal>=native||!r6_at(ops,owner,0x80,&child,8))return 0;
    while(child){
        if(++steps>4096||!r6_at(ops,child,0x60,&next,8))return 0;
        for(unsigned i=0;i<2;i++)if(r6_name(ops,child,i?"mLevelString":"mName")){
            R6Field*field=i?&layout.label:&layout.name;
            if((found&(1u<<i))||!r6_property(ops,child,"StrProperty",16,field))return 0;
            for(unsigned j=0;j<R6_FIELDS;j++){
                R6Field other=layout.common.field[j];
                if(field->offset<other.offset+other.size&&other.offset<field->offset+field->size)return 0;
            }
            found|=1u<<i;
        }
        child=next;
    }
    if(found!=3||!r11_layout_valid(&layout))return 0;
    *out=layout;return 1;
}
typedef struct R11Sample {R6Sample common;R11Label name,label;} R11Sample;
static int r11_sample_tuber(const R6Ops*ops,uint64_t object,const R11Layout*layout,R11Sample*out){
    R11Sample sample={0};R6Sample after;
    if(!out||!r11_layout_valid(layout)||
       !r6_sample(ops,object,&layout->common,&sample.common)||
       layout->name.size!=16||layout->label.size!=16||layout->name.offset>R6_NATIVE_EXTENT-16||layout->label.offset>R6_NATIVE_EXTENT-16||
       object>UINT64_MAX-layout->name.offset||object>UINT64_MAX-layout->label.offset||
       !r11_read_label(ops,object+layout->name.offset,layout->family,&sample.name)||
       !r11_read_label(ops,object+layout->label.offset,layout->family,&sample.label)||
       !r6_sample(ops,object,&layout->common,&after)||
       after.name_index!=sample.common.name_index||after.name_number!=sample.common.name_number||after.object_class!=sample.common.object_class)return 0;
    *out=sample;return 1;
}
#endif
