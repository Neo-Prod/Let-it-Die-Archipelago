#ifndef LID_CHECKS_ROTATION_STAGE_R33_H
#define LID_CHECKS_ROTATION_STAGE_R33_H
#include "checks_rotation.h"
/* Read-only bounded observations. No allocation, engine call, I/O, hook or
   publication of delivery proof. Call only within a validated read window. */
typedef struct R33NodeRef {const char*floor,*area,*elevator;int32_t idx,isdef;uint32_t ofsx;} R33NodeRef;
typedef struct R33EdgeRef {int node,direction;int32_t idx,ci;const char*text[7];} R33EdgeRef;
#include "checks_rotation_reference.h"
typedef struct R33Array {uintptr_t data;int32_t num,capacity;} R33Array;
_Static_assert(sizeof(R33Array)==16,"array ABI");
typedef struct R33Ops {void*context;uint32_t owner;uint32_t(*thread)(void*);int(*window)(void*);
    int(*identity)(void*,R31Identity*);int(*read)(void*,uintptr_t,void*,size_t);} R33Ops;
enum {R33_OK=1,R33_CONTEXT=2,R33_READ=3,R33_BUDGET=4,R33_SHAPE=5,R33_LABEL=6,R33_TOPOLOGY=7,R33_COPY=8};
typedef struct R33Result {unsigned status,reads;size_t bytes;unsigned nodes,edges,settings,repeats,units;uintptr_t stage,repository;unsigned serial;} R33Result;
typedef struct R33Reader {const R33Ops*ops;R33Result*out;} R33Reader;
static int r33_read(R33Reader*c,uintptr_t address,void*out,size_t n){
    if(!address||!n||address>UINTPTR_MAX-n){c->out->status=R33_READ;return 0;}
    if(c->out->reads>=100000||c->out->bytes>8u*1024u*1024u||n>8u*1024u*1024u-c->out->bytes){c->out->status=R33_BUDGET;return 0;}
    c->out->reads++;c->out->bytes+=n;
    if(c->ops->read(c->ops->context,address,out,n)!=1){c->out->status=R33_READ;return 0;}return 1;
}
static int r33_at(R33Reader*c,uintptr_t p,size_t off,void*out,size_t n){
    if(p>UINTPTR_MAX-off){c->out->status=R33_READ;return 0;}return r33_read(c,p+off,out,n);
}
static int r33_array(R33Reader*c,uintptr_t p,unsigned stride,R33Array*out){
    if(!r33_read(c,p,out,16))return 0;
    if(out->num<0||out->num>4096||out->capacity<out->num||out->capacity>16384||
       (out->num&&(!out->data||out->data>UINTPTR_MAX-(size_t)out->num*stride))){c->out->status=R33_SHAPE;return 0;}return 1;
}
static int r33_string(R33Reader*c,uintptr_t p,uint16_t*out,unsigned*length){
    R31String s;if(!r33_read(c,p,&s,16))return 0;
    if(s.num<0||s.num>256||s.capacity<s.num||s.capacity>4096||(s.num&&!s.data)){c->out->status=R33_SHAPE;return 0;}
    if(!s.num){*length=0;out[0]=0;return 1;}
    if(!r33_read(c,(uintptr_t)s.data,out,(unsigned)s.num*2))return 0;
    if(out[s.num-1]){c->out->status=R33_SHAPE;return 0;}
    for(int i=0;i+1<s.num;i++)if(!out[i]){c->out->status=R33_SHAPE;return 0;}
    *length=(unsigned)s.num-1;return 1;
}
static int r33_text_is(const uint16_t*text,unsigned n,const char*expected){
    unsigned i=0;for(;expected[i];i++)if(i>=n||text[i]!=(unsigned char)expected[i])return 0;return i==n;
}
static int r33_ascii(R33Reader*c,uintptr_t p,const char*expected){
    uint16_t text[256];unsigned n;return r33_string(c,p,text,&n)&&r33_text_is(text,n,expected);
}
static int r33_strings_equal(R33Reader*c,uintptr_t a,uintptr_t b){
    uint16_t x[256],y[256];unsigned xn,yn;
    return r33_string(c,a,x,&xn)&&r33_string(c,b,y,&yn)&&xn==yn&&!__builtin_memcmp(x,y,xn*2);
}
enum {R33_SCALAR,R33_STRING,R33_ARRAY};
typedef struct R33Schema R33Schema;
typedef struct R33Field {unsigned type,offset;const R33Schema*child;} R33Field;
struct R33Schema {unsigned size,count;const R33Field*fields;};
static int r33_equal(R33Reader*c,uintptr_t a,uintptr_t b,const R33Schema*s,unsigned depth){
    if(depth>4){c->out->status=R33_SHAPE;return 0;}
    for(unsigned i=0;i<s->count;i++){
        const R33Field*f=&s->fields[i];if(a>UINTPTR_MAX-f->offset||b>UINTPTR_MAX-f->offset)return 0;
        uintptr_t x=a+f->offset,y=b+f->offset;
        if(f->type==R33_SCALAR){uint32_t v,w;if(!r33_read(c,x,&v,4)||!r33_read(c,y,&w,4)||v!=w)return 0;}
        else if(f->type==R33_STRING){if(!r33_strings_equal(c,x,y))return 0;}
        else{R33Array u,v;if(!r33_array(c,x,f->child->size,&u)||!r33_array(c,y,f->child->size,&v)||u.num!=v.num)return 0;
            for(int j=0;j<u.num;j++)if(!r33_equal(c,u.data+(size_t)j*f->child->size,v.data+(size_t)j*f->child->size,f->child,depth+1))return 0;
        }
    }return 1;
}
static const R33Field r33_fs_fields[]={{R33_STRING,0,0}};
static const R33Schema r33_fs={16,1,r33_fs_fields};
static const R33Field r33_pair_fields[]={{R33_STRING,0,0},{R33_STRING,16,0}};
static const R33Schema r33_pair={32,2,r33_pair_fields};
static const R33Field r33_candidate_fields[]={{R33_STRING,0,0},{R33_SCALAR,16,0}};
static const R33Schema r33_candidate={20,2,r33_candidate_fields};
static const R33Field r33_replace_fields[]={{R33_ARRAY,0,&r33_pair},{R33_STRING,16,0},{R33_STRING,32,0},{R33_STRING,48,0}};
static const R33Schema r33_replace={64,4,r33_replace_fields};
static const R33Field r33_setting_fields[]={{R33_STRING,0,0},{R33_ARRAY,16,&r33_candidate},{R33_SCALAR,32,0},
    {R33_ARRAY,36,&r33_pair},{R33_ARRAY,52,&r33_pair},{R33_ARRAY,68,&r33_replace}};
static const R33Schema r33_setting={84,6,r33_setting_fields};
static const R33Field r33_esc_fields[]={{R33_STRING,0,0},{R33_STRING,16,0},{R33_STRING,32,0},{R33_STRING,48,0},
    {R33_SCALAR,64,0},{R33_STRING,68,0},{R33_STRING,84,0},{R33_STRING,100,0},{R33_SCALAR,116,0}};
static const R33Schema r33_esc={120,9,r33_esc_fields};
static const R33Field r33_repeat_fields[]={{R33_STRING,0,0},{R33_SCALAR,16,0},{R33_SCALAR,20,0},{R33_SCALAR,24,0},
    {R33_SCALAR,28,0},{R33_ARRAY,32,&r33_fs},{R33_ARRAY,48,&r33_esc},{R33_ARRAY,64,&r33_esc}};
static const R33Schema r33_repeat={80,8,r33_repeat_fields};
static int r33_edge(R33Reader*c,uintptr_t p,const R33EdgeRef*e){
    static const unsigned offsets[]={0,16,32,48,68,84,100};uint32_t v;
    for(unsigned i=0;i<7;i++)if(!r33_ascii(c,p+offsets[i],e->text[i]))return 0;
    return r33_at(c,p,64,&v,4)&&v==(uint32_t)e->ci&&r33_at(c,p,116,&v,4)&&v==(uint32_t)e->idx;
}
static int r33_is_floor(const uint16_t*s,unsigned n){
    static const char*prefixes[]={"MET_FLR_","ARC_FLR_","AMS_FLR_","RFT_FLR_"};
    if(n!=10)return 0;int prefix=0;
    for(unsigned k=0;k<4;k++){unsigned i=0;for(;i<8&&s[i]==(unsigned char)prefixes[k][i];i++){}if(i==8)prefix=1;}
    return prefix&&((s[8]=='0'&&s[9]>='1'&&s[9]<='9')||(s[8]=='1'&&s[9]=='0'));
}
static int r33_topology(R33Reader*c,const R33Array*a){
    unsigned char nodes[101]={0},edges[268]={0};unsigned nc=0,ec=0;
    for(int i=0;i<a->num;i++){
        uintptr_t row=a->data+(size_t)i*108;uint16_t floor[256],area[256];unsigned n,an;int found=-1;uint32_t value;R33Array flags;
        if(!r33_string(c,row+16,floor,&n))return 0;if(!r33_is_floor(floor,n))continue;
        if(!r33_string(c,row,area,&an))return 0;
        for(unsigned j=0;j<101;j++)if(r33_text_is(floor,n,r33_nodes[j].floor)&&r33_text_is(area,an,r33_nodes[j].area)){found=(int)j;break;}
        if(found<0||nodes[found])return 0;nodes[found]=1;nc++;const R33NodeRef*ref=&r33_nodes[found];
        if(!r33_ascii(c,row+32,ref->elevator)||!r33_at(c,row,80,&value,4)||value!=(uint32_t)ref->isdef||
           !r33_at(c,row,84,&value,4)||value!=ref->ofsx||!r33_at(c,row,104,&value,4)||value!=(uint32_t)ref->idx||
           !r33_array(c,row+88,20,&flags)||flags.num)return 0;
        for(unsigned direction=0;direction<2;direction++){
            R33Array list;unsigned expected=0;
            for(unsigned j=0;j<268;j++)if(r33_edges[j].node==found&&r33_edges[j].direction==(int)direction)expected++;
            /* Native141155110 selects Uesc+40 for dir0, Desc+30 otherwise. */
            if(!r33_array(c,row+(direction?48:64),120,&list)||list.num!=(int)expected)return 0;
            for(int j=0;j<list.num;j++){
                uintptr_t p=list.data+(size_t)j*120;uint32_t idx;if(!r33_at(c,p,116,&idx,4))return 0;int match=-1;
                for(unsigned k=0;k<268;k++)if(r33_edges[k].node==found&&r33_edges[k].direction==(int)direction&&r33_edges[k].idx==(int32_t)idx){match=(int)k;break;}
                if(match<0||edges[match]||!r33_edge(c,p,&r33_edges[match]))return 0;edges[match]=1;ec++;
            }
        }
    }
    if(nc!=101||ec!=268)return 0;c->out->nodes=nc;c->out->edges=ec;return 1;
}
static int r33_pin_valid(const R31Pin*p){
    static const char*prefix="TERMID_";uint64_t v=0;
    if(!p||!r31_valid_identity(&p->identity)||p->timestamp<=0||p->length<9||p->length>31||p->term[p->length-1]||p->term[7]=='0')return 0;
    for(unsigned i=0;i<7;i++)if(p->term[i]!=(unsigned char)prefix[i])return 0;
    for(unsigned i=7;i+1<p->length;i++){unsigned d=p->term[i]-'0';if(d>9||v>(UINT64_MAX-d)/10)return 0;v=v*10+d;}
    return v==(uint64_t)p->timestamp;
}
static int r33_context(const R33Ops*o,const R31Pin*p){
    R31Identity id;return o->owner&&o->thread(o->context)==o->owner&&o->window(o->context)==1&&
      o->identity(o->context,&id)==1&&r31_same(&id,&p->identity);
}
static int r33_stage(const R33Ops*o,uintptr_t base,uintptr_t stage,const R31Pin*pin,unsigned serial,R33Result*out){
    if(!out)return 0;*out=(R33Result){0};out->status=R33_CONTEXT;
    if(!o||!o->read||!o->thread||!o->window||!o->identity||!base||base>UINTPTR_MAX-0xf8b1000||!stage||stage>UINTPTR_MAX-0x5d0||
       !serial||!r33_pin_valid(pin)||!r33_context(o,pin))return 0;
    R33Reader c={o,out};uintptr_t vt,repo,params;R33Array a,b,r,s,u,v,units;
    out->status=R33_READ;
    if(!r33_read(&c,stage,&vt,8))return 0;
    if(vt!=base+0x21c3350){out->status=R33_SHAPE;return 0;}
    if(!r33_at(&c,stage,0x5b0,&repo,8)||!r33_at(&c,stage,0x5a0,&params,8))return 0;
    if(!repo||repo>UINTPTR_MAX-0x38||!params||params>UINTPTR_MAX-24){out->status=R33_SHAPE;return 0;}
    if(!r33_read(&c,repo,&vt,8))return 0;
    if(vt!=base+0x220a9c8){out->status=R33_SHAPE;return 0;}
    if(!r33_read(&c,params,&vt,8))return 0;
    if(vt!=base+0x220a9d0){out->status=R33_SHAPE;return 0;}
    out->status=R33_LABEL;uint16_t term[256];unsigned n;
    if(!r33_ascii(&c,base+0xf8b018c,"4HMA")||!r33_ascii(&c,stage+0x550,"4HMA")||!r33_ascii(&c,stage+0x5c0,"4HMA")||
       !r33_ascii(&c,repo+8,"4HMA")||!r33_string(&c,base+0xf8b019c,term,&n)||n+1!=pin->length||__builtin_memcmp(term,pin->term,pin->length*2))return 0;
    out->status=R33_TOPOLOGY;
    if(!r33_array(&c,stage+0x560,108,&a)||!r33_array(&c,repo+24,108,&b)||a.num!=b.num||!r33_topology(&c,&a)||!r33_topology(&c,&b))return 0;
    out->status=R33_COPY;
    if(!r33_array(&c,stage+0x570,80,&r)||!r33_array(&c,repo+40,80,&s)||r.num!=5||r.num!=s.num)return 0;
    for(int i=0;i<r.num;i++)if(!r33_equal(&c,r.data+(size_t)i*80,s.data+(size_t)i*80,&r33_repeat,0))return 0;
    if(!r33_array(&c,stage+0x580,84,&u)||!r33_array(&c,params+8,84,&v)||u.num<1||u.num!=v.num)return 0;
    for(int i=0;i<u.num;i++)if(!r33_equal(&c,u.data+(size_t)i*84,v.data+(size_t)i*84,&r33_setting,0))return 0;
    if(!r33_array(&c,stage+0x590,48,&units))return 0;
    if(!r33_context(o,pin)){out->status=R33_CONTEXT;return 0;}
    out->settings=(unsigned)u.num;out->repeats=(unsigned)r.num;out->units=(unsigned)units.num;
    out->stage=stage;out->repository=repo;out->serial=serial;out->status=R33_OK;return 1;
}
#endif
