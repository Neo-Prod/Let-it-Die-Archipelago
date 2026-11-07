#ifndef LID_RESEARCH_DEATH_WIRE_H
#define LID_RESEARCH_DEATH_WIRE_H
#include <stdint.h>
#include <stddef.h>
enum {R15_SIZE=1024,R15_EVENT=1,R15_ACK=2,R15_FAULT=3,R39_VISIT=4,R72_CHEST=5};
typedef struct R15Context {uint32_t pid,client_pid;uint64_t birth;uint8_t nonce[16],run[32],config[32],writer[16];} R15Context;
typedef struct R15Frame {
    R15Context context;uint32_t kind,status;uint64_t sequence,epoch;
    uint32_t flags,classes,name_index,name_number,raw_kind;int32_t health,max_health;
    char floor[16],area[32],rotation[16],damage[96],pawn_class[96];
    uint16_t name[96],label[96];uint32_t family,version;
} R15Frame;
static const uint8_t r15_magic[8]={'L','I','D','K','I','L','0','1'};
/* Byte sequence below is the analyzed executable digest, not a hash
   derived from compiler/model ABI. Kept explicit for C/Python cross-check. */
static const uint8_t r15_build[32]={0xf3,0x3e,0x91,0xe6,0xe8,0xd5,0x85,0x66,0xbf,0xab,0xad,0x8d,0x01,0xcd,0x04,0x86,0x58,0x6a,0x0d,0x43,0xfe,0x1c,0x58,0xe6,0x5f,0xc6,0x48,0x5a,0x41,0x7b,0xdd,0xfd};
static void r15_zero(void*p,size_t n){uint8_t*b=p;while(n--)*b++=0;}
static void r15_copy(void*d,const void*s,size_t n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;}
static int r15_equal(const void*a,const void*b,size_t n){const uint8_t*x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static int r15_nonzero(const void*p,size_t n){const uint8_t*b=p;while(n--)if(*b++)return 1;return 0;}
static uint32_t r15_r32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t r15_r64(const uint8_t*p){return r15_r32(p)|((uint64_t)r15_r32(p+4)<<32);}
static void r15_w32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void r15_w64(uint8_t*p,uint64_t v){r15_w32(p,(uint32_t)v);r15_w32(p+4,(uint32_t)(v>>32));}
static uint32_t r15_checksum(const uint8_t*p){uint32_t h=2166136261u;for(unsigned i=0;i<R15_SIZE;i++)if(i<848||i>=852)h=(h^p[i])*16777619u;return h;}
static int r15_ascii(const char*s,unsigned n){unsigned end=n;for(unsigned i=0;i<n;i++){unsigned c=(uint8_t)s[i];if(!c){end=i;break;}if(c<32||c>126)return 0;}if(end==n)return 0;for(unsigned i=end;i<n;i++)if(s[i])return 0;return 1;}
static int r15_utf16(const uint16_t*s){unsigned end=96;for(unsigned i=0;i<96;i++){unsigned c=s[i];if(!c){end=i;break;}if(c<32||c==127)return 0;if(c>=0xd800&&c<=0xdbff){if(++i>=96||s[i]<0xdc00||s[i]>0xdfff)return 0;}else if(c>=0xdc00&&c<=0xdfff)return 0;}if(end==96)return 0;for(unsigned i=end;i<96;i++)if(s[i])return 0;return 1;}
static int r15_context_valid(const R15Context*c){return c&&c->pid&&c->client_pid&&c->birth&&r15_nonzero(c->nonce,16)&&r15_nonzero(c->writer,16)&&r15_nonzero(c->run,32)&&r15_nonzero(c->config,32);}
static int r15_same_context(const R15Context*a,const R15Context*b){return a&&b&&a->pid==b->pid&&a->client_pid==b->client_pid&&a->birth==b->birth&&r15_equal(a->nonce,b->nonce,16)&&r15_equal(a->writer,b->writer,16)&&r15_equal(a->run,b->run,32)&&r15_equal(a->config,b->config,32);}
static int r16_mask_valid(const R15Frame*f){
    uint32_t family=(f->classes>>3)&63u,mini=(f->classes>>9)&15u,major=(f->classes>>13)&15u,ai=f->classes&3u;
    if((family&(family-1))||(mini&(mini-1))||(major&(major-1)))return 0;
    if(family&&(mini||major||ai||(f->classes&(1u<<17))))return 0;
    if((mini||major)&&(ai||(f->classes&((1u<<18)|(1u<<19)))))return 0;
    if(major&&mini&&major!=mini)return 0;
    if((f->classes&(1u<<17))&&major)return 0;
    if((f->classes&4u)&&ai)return 0;
    if(f->family&&family!=(1u<<(f->family-1)))return 0;
    if(f->version&&(f->label[0]!='V'&&f->label[0]!='v'))return 0;
    if(f->version&&(f->label[1]!='-'||f->label[2]!=(uint16_t)('0'+f->version)||f->label[3]))return 0;
    return 1;
}
/* Inputs must have passed bounded canonical ASCII validation. */
static unsigned r22_map_floor(const char*floor,const char*area){
    static const char prefixes[4][4]={"MET","ARC","AMS","RFT"};unsigned block=4;
    for(unsigned i=0;i<4;i++)if(r15_equal(floor,prefixes[i],3)){block=i;break;}
    if(block==4||!r15_equal(floor+3,"_FLR_",5)||floor[8]<'0'||floor[8]>'1'||floor[9]<'0'||floor[9]>'9'||floor[10])return 0;
    unsigned local=(unsigned)(floor[8]-'0')*10+(unsigned)(floor[9]-'0');if(local<1||local>10)return 0;
    if(!r15_equal(area,prefixes[block],3)||!r15_equal(area+3,"_AREA_",6))return 0;
    unsigned start=9;if(area[start]=='V')start++;
    unsigned count=0;for(unsigned i=start;area[i];i++){if(count==12||area[i]<'0'||area[i]>'9')return 0;count++;}
    return count?block*10+local:0;
}
static int r15_valid(const R15Frame*f){
    if(!f||!r15_context_valid(&f->context)||!f->sequence||f->kind<1||f->kind>5||
       !r15_ascii(f->floor,16)||!r15_ascii(f->area,32)||!r15_ascii(f->rotation,16)||!r15_ascii(f->damage,96)||!r15_ascii(f->pawn_class,96)||!r15_utf16(f->name)||!r15_utf16(f->label))return 0;
    if(f->kind==R72_CHEST)return !f->status&&f->epoch&&r22_map_floor(f->floor,f->area)&&r15_equal(f->rotation,"4HMA\0",5)&&
      f->raw_kind>=1&&f->raw_kind<=3&&!f->flags&&!f->classes&&!f->health&&!f->max_health&&!f->family&&!f->version&&
      !f->damage[0]&&!f->pawn_class[0]&&!f->name[0]&&!f->label[0];
    if(f->kind==R39_VISIT)return !f->status&&f->epoch&&r22_map_floor(f->floor,f->area)&&r15_equal(f->rotation,"4HMA\0",5)&&
      !f->flags&&!f->classes&&!f->name_index&&!f->name_number&&!f->raw_kind&&!f->health&&!f->max_health&&!f->family&&!f->version&&
      !f->damage[0]&&!f->pawn_class[0]&&!f->name[0]&&!f->label[0];
    if(f->kind!=R15_EVENT)return (f->kind==R15_ACK?f->status==1:f->status>=1&&f->status<=4)&&
      !f->epoch&&!f->flags&&!f->classes&&!f->name_index&&!f->name_number&&!f->raw_kind&&!f->health&&!f->max_health&&!f->family&&!f->version&&
      !f->floor[0]&&!f->area[0]&&!f->rotation[0]&&!f->damage[0]&&!f->pawn_class[0]&&!f->name[0]&&!f->label[0];
    if(f->status||!f->epoch||(f->flags&~7u)||(f->flags&3)!=3||(f->classes&~0xfffffu)||f->raw_kind>255||f->health!=1||f->max_health<1||
       f->family>6||f->version>5||(!f->family&&f->version))return 0;
    if(!r16_mask_valid(f))return 0;
    if(!r22_map_floor(f->floor,f->area))return 0;
    if(f->rotation[0]&&!r15_equal(f->rotation,"4HMA\0",5))return 0;
    if(!r15_equal(f->pawn_class,"BrgPawn_",8)||!f->pawn_class[8])return 0;
    for(unsigned i=8;f->pawn_class[i];i++){unsigned c=(uint8_t)f->pawn_class[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;}return 1;
}
static int r15_encode(uint8_t*out,size_t n,const R15Frame*f){
    if(!out||n!=R15_SIZE||!r15_valid(f))return 0;
    r15_zero(out,n);r15_copy(out,r15_magic,8);r15_w32(out+8,1);r15_w32(out+12,f->kind);r15_w32(out+16,f->context.pid);r15_w32(out+20,f->status);
    r15_w64(out+24,f->context.birth);r15_copy(out+32,f->context.nonce,16);r15_copy(out+48,f->context.run,32);r15_copy(out+80,f->context.config,32);
    r15_w64(out+112,f->sequence);r15_w64(out+120,f->epoch);r15_copy(out+128,r15_build,32);
    r15_w32(out+160,f->flags);r15_w32(out+164,f->classes);r15_w32(out+168,f->name_index);r15_w32(out+172,f->name_number);r15_w32(out+176,f->raw_kind);
    r15_w32(out+184,(uint32_t)f->health);r15_w32(out+188,(uint32_t)f->max_health);r15_copy(out+192,f->floor,16);r15_copy(out+208,f->area,32);
    r15_copy(out+240,f->rotation,16);r15_copy(out+256,f->damage,96);r15_copy(out+352,f->pawn_class,96);
    for(unsigned i=0;i<96;i++){out[448+2*i]=(uint8_t)f->name[i];out[449+2*i]=(uint8_t)(f->name[i]>>8);out[640+2*i]=(uint8_t)f->label[i];out[641+2*i]=(uint8_t)(f->label[i]>>8);}
    r15_w32(out+832,f->family);r15_w32(out+836,f->version);r15_copy(out+852,f->context.writer,16);r15_w32(out+868,f->context.client_pid);r15_w32(out+848,r15_checksum(out));return 1;
}
static int r15_decode(R15Frame*out,const uint8_t*p,size_t n){
    R15Frame f;
    if(!out||!p||n!=R15_SIZE||!r15_equal(p,r15_magic,8)||r15_r32(p+8)!=1||!r15_equal(p+128,r15_build,32)||r15_r32(p+180)||
       r15_r32(p+840)||r15_r32(p+844)||r15_nonzero(p+872,152)||r15_r32(p+848)!=r15_checksum(p))return 0;
    r15_zero(&f,sizeof(f));f.kind=r15_r32(p+12);f.context.pid=r15_r32(p+16);f.status=r15_r32(p+20);f.context.birth=r15_r64(p+24);
    r15_copy(f.context.nonce,p+32,16);r15_copy(f.context.run,p+48,32);r15_copy(f.context.config,p+80,32);r15_copy(f.context.writer,p+852,16);f.context.client_pid=r15_r32(p+868);f.sequence=r15_r64(p+112);f.epoch=r15_r64(p+120);
    f.flags=r15_r32(p+160);f.classes=r15_r32(p+164);f.name_index=r15_r32(p+168);f.name_number=r15_r32(p+172);f.raw_kind=r15_r32(p+176);f.health=(int32_t)r15_r32(p+184);f.max_health=(int32_t)r15_r32(p+188);
    r15_copy(f.floor,p+192,16);r15_copy(f.area,p+208,32);r15_copy(f.rotation,p+240,16);r15_copy(f.damage,p+256,96);r15_copy(f.pawn_class,p+352,96);
    for(unsigned i=0;i<96;i++){f.name[i]=(uint16_t)(p[448+2*i]|((unsigned)p[449+2*i]<<8));f.label[i]=(uint16_t)(p[640+2*i]|((unsigned)p[641+2*i]<<8));}
    f.family=r15_r32(p+832);f.version=r15_r32(p+836);if(!r15_valid(&f))return 0;*out=f;return 1;
}
static int r15_ack_through(const R15Frame*f,const R15Context*context,uint64_t previous,uint64_t published,uint64_t*out){
    if(!out||!r15_valid(f)||f->kind!=R15_ACK||!r15_same_context(&f->context,context)||previous>published||f->sequence<previous||f->sequence>published)return 0;
    *out=f->sequence;return 1;
}
#endif
