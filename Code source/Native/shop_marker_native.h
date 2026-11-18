#ifndef LID_SHOP_MARKER_NATIVE_H
#define LID_SHOP_MARKER_NATIVE_H
#include <stdint.h>
#include <stddef.h>
/* Passive native JSON adapter. No hooks, save scheduling, game state writes,
   payment, AP sends or automatic initialization of an absent marker.
   Host must provide stable logical ownership, without holding engine locks.
   A successful insertion proves JSON readback ONLY, never durable Save.
   Native exceptions remain native; do not blindly retry after a setter call. */
enum {LID_SM_MAX=4212,LID_SM_TEXT=8425,LID_SM_INVALID=-1,
      LID_SM_ABSENT=0,LID_SM_PRESENT=1,LID_SM_WRITE_UNVERIFIED=-2};
typedef struct __attribute__((packed,aligned(4))) LidShopMarkerString {
    uint16_t*data;int32_t num,capacity;
} LidShopMarkerString;
typedef struct LidShopMarkerAccess {
    void*context;
    /* root==NULL is binding preparation; write==1 requires a fresh OWNED root.
       This callback must reject main-thread mutation/invalid session scopes. */
    int(*window)(void*,const void*root,int write);
    int(*copy)(void*,const void*source,void*destination,size_t);
} LidShopMarkerAccess;
typedef struct LidShopMarkerNative {
    uintptr_t base;void*vtable;unsigned initialized;LidShopMarkerAccess access;
    uint64_t(*has)(void*,const LidShopMarkerString*);
    void*(*object)(void*,const LidShopMarkerString*);
    LidShopMarkerString*(*string)(void*,LidShopMarkerString*,const LidShopMarkerString*);
    void(*set)(void*,const LidShopMarkerString*,const LidShopMarkerString*);
    void(*release)(void*);
} LidShopMarkerNative;
/* Caller-owned, per invocation, disjoint from input/output and other calls. */
typedef struct LidShopMarkerScratch {
    uint16_t text[LID_SM_TEXT];
    unsigned char decoded[LID_SM_MAX],expected[LID_SM_MAX];
} LidShopMarkerScratch;
static int lid_sm_same(const void*a,const void*b,size_t n){
    const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static int lid_sm_payload(const unsigned char*p,unsigned n){
    static const unsigned char magic[8]={'L','I','D','S','H','O','P','2'};
    unsigned count=0;uint64_t revision=0;
    if(!p||n<117||n>LID_SM_MAX||!lid_sm_same(p,magic,8))return 0;
    for(unsigned i=0;i<4;i++)count|=(unsigned)p[112+i]<<(i*8);
    for(unsigned i=0;i<8;i++)revision|=(uint64_t)p[104+i]<<(i*8);
    if(!count||count>4096||n!=116+count)return 0;
    for(unsigned i=116;i<n;i++)if(p[i]>2||(p[i]==1&&!revision))return 0;
    return 1;
}
static LidShopMarkerString lid_sm_key(void){
    static uint16_t text[]={'l','i','d','_','a','p','_','s','h','o','p','_','v','2',0};
    LidShopMarkerString s={text,(int32_t)(sizeof(text)/2),(int32_t)(sizeof(text)/2)};return s;
}
static int lid_sm_root(const LidShopMarkerNative*b,void*root,int write){
    unsigned char header[0x120];void*vt=0;
    if(!b||!b->initialized||!root||!b->access.window||!b->access.copy||
       b->access.window(b->access.context,root,write)!=1||
       !b->access.copy(b->access.context,root,header,sizeof(header)))return 0;
    __builtin_memcpy(&vt,header,sizeof(vt));return vt==b->vtable;
}
/* Must prepare a private zeroed binding; publish immutable only after success.
   Full game SHA verification belongs to the host, not these short profiles. */
static int lid_shop_marker_bind(LidShopMarkerNative*out,uintptr_t base,int exact_sha,
                                const LidShopMarkerAccess*access){
    static const unsigned rvas[5]={0x26c750,0x257c20,0x253ec0,0x251f50,0xdfb60};
    static const unsigned char bytes[5][16]={
      {0x48,0x83,0xec,0x38,0x48,0x89,0x54,0x24,0x20,0x4c,0x89,0x44,0x24,0x28,0x4c,0x8d},
      {0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48},
      {0x48,0x8b,0xc4,0x48,0x89,0x50,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83},
      {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8d,0x99,0xa8,0,0,0,0x4c,0x8b,0xc2},
      {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x0d,0x70,0x40,0x73,0x02}};
    static const unsigned slots[4]={0x2d8,0x320,0x278,0x258};
    LidShopMarkerNative b={0};unsigned char got[16];uintptr_t slot;
    if(!out||out->initialized||!base||base>UINTPTR_MAX-0x1fee5a0||exact_sha!=1||!access||
       !access->copy||!access->window||access->window(access->context,0,0)!=1)return 0;
    for(unsigned i=0;i<5;i++)if(!access->copy(access->context,(void*)(base+rvas[i]),got,16)||
      !lid_sm_same(got,bytes[i],16))return 0;
    for(unsigned i=0;i<4;i++)if(!access->copy(access->context,(void*)(base+0x1fee270+slots[i]),&slot,sizeof(slot))||
      slot!=base+rvas[i])return 0;
    b.base=base;b.vtable=(void*)(base+0x1fee270);b.access=*access;
    b.set=(void(*)(void*,const LidShopMarkerString*,const LidShopMarkerString*))(base+rvas[0]);
    b.has=(uint64_t(*)(void*,const LidShopMarkerString*))(base+rvas[1]);
    b.string=(LidShopMarkerString*(*)(void*,LidShopMarkerString*,const LidShopMarkerString*))(base+rvas[2]);
    b.object=(void*(*)(void*,const LidShopMarkerString*))(base+rvas[3]);
    b.release=(void(*)(void*))(base+rvas[4]);b.initialized=1;*out=b;return 1;
}
static int lid_sm_hex(uint16_t c){
    if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;return -1;
}
/* Only PRESENT publishes bytes/length. ABSENT must be evaluated by run/catalogue
   recovery policy; it never authorizes a fresh ledger by itself. */
static int lid_shop_marker_read(const LidShopMarkerNative*b,void*root,
  unsigned char*out,unsigned capacity,unsigned*length,LidShopMarkerScratch*s){
    LidShopMarkerString key=lid_sm_key(),value={0};uint64_t has;unsigned n=0;int ok=0;
    if(!out||!length||!s||capacity<LID_SM_MAX||!lid_sm_root(b,root,0)||
       !b->has||!b->object||!b->string||!b->release)return LID_SM_INVALID;
    has=b->has(root,&key);
    if(has>1||!lid_sm_root(b,root,0))return LID_SM_INVALID;
    if(b->object(root,&key)!=0||!lid_sm_root(b,root,0))return LID_SM_INVALID;
    if(!has)return LID_SM_ABSENT;
    LidShopMarkerString*returned=b->string(root,&value,&key);
    if(returned==&value&&value.data&&value.num>=235&&value.num<=LID_SM_TEXT&&
       value.capacity>=value.num&&value.capacity<=1048576&&lid_sm_root(b,root,0)&&
       b->access.copy(b->access.context,value.data,s->text,(size_t)value.num*2)&&
       s->text[value.num-1]==0&&!(value.num%2==0)){
        n=(unsigned)(value.num-1)/2;ok=1;
        for(unsigned i=0;i<n;i++){
            int hi=lid_sm_hex(s->text[i*2]),lo=lid_sm_hex(s->text[i*2+1]);
            if(hi<0||lo<0){ok=0;break;}s->decoded[i]=(unsigned char)((hi<<4)|lo);
        }
        if(ok&&!lid_sm_payload(s->decoded,n))ok=0;
    }
    /* Returned buffers are owned even on malformed return/window loss. */
    if(value.data)b->release(value.data);
    if(!ok||!lid_sm_root(b,root,0))return LID_SM_INVALID;
    for(unsigned i=0;i<n;i++)out[i]=s->decoded[i];*length=n;return LID_SM_PRESENT;
}
/* Fresh roots ONLY. Existing field of ANY type is refused, never overwritten.
   Supplied JSON text must be verified, not merged with a newer payment ledger.
   After WRITE_UNVERIFIED caller must assume mutation and must not retry blindly. */
static int lid_shop_marker_insert_fresh(const LidShopMarkerNative*b,void*root,
  const unsigned char*payload,unsigned size,LidShopMarkerScratch*s){
    static const char hex[]="0123456789ABCDEF";
    LidShopMarkerString key=lid_sm_key(),value;unsigned read_size=0;int read;
    if(!s||!lid_sm_payload(payload,size)||!lid_sm_root(b,root,1)||!b->set||!b->has||!b->object||!b->string||!b->release)return LID_SM_INVALID;
    if(b->has(root,&key)!=0||!lid_sm_root(b,root,1)||b->object(root,&key)!=0||!lid_sm_root(b,root,1))return LID_SM_INVALID;
    for(unsigned i=0;i<size;i++){s->expected[i]=payload[i];s->text[i*2]=hex[payload[i]>>4];s->text[i*2+1]=hex[payload[i]&15];}
    s->text[size*2]=0;value=(LidShopMarkerString){s->text,(int32_t)(size*2+1),(int32_t)(size*2+1)};
    b->set(root,&key,&value);
    read=lid_shop_marker_read(b,root,s->decoded,LID_SM_MAX,&read_size,s);
    if(read!=LID_SM_PRESENT||read_size!=size||!lid_sm_same(s->decoded,s->expected,size))return LID_SM_WRITE_UNVERIFIED;
    return LID_SM_PRESENT;
}
#endif
