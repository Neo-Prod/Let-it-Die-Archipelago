#ifndef LID_UI_ITEM_COLORS_R323_H
#define LID_UI_ITEM_COLORS_R323_H
/* Presentation only. Engine palette uses BGRA FColor. No reward/state logic. */
enum {LID_UI_COLOR_COUNT=5,LID_UI_NAME_CAP=1040};
typedef struct LidUIColorPalette {uint32_t indices[5];uintptr_t data;int32_t count;unsigned ready;} LidUIColorPalette;
typedef struct LidUIColorArray {uint32_t*data;int32_t count,capacity;} LidUIColorArray;
static LidUIColorPalette g_lid_ui_colors;
static const uint32_t lid_ui_rgb[5]={0xff00eeeeu,0xffaf99efu,0xff6d8be8u,0xfffa8072u,0xffffffffu};
static unsigned lid_ui_kind(unsigned flags){return flags&1?1:(flags&2?2:(flags&4?3:0));}
/* Caller proves UI ownership, native palette header/range and engine allocator.
   Realloc follows the same native TArray ownership as its initializer. Existing
   entries remain byte-identical; only missing colors are appended, count last. */
static int lid_ui_register(LidUIColorPalette*p,LidUIColorArray*a,void*context,void*(*resize)(void*,void*,size_t)){
    if(!p||!a||!resize||a->count<1||a->count>100||a->capacity<a->count||a->capacity>4096||!a->data)return 0;
    if(p->ready&&p->data==(uintptr_t)a->data&&p->count==a->count){
        unsigned ok=1;for(unsigned i=0;i<5;i++)if(p->indices[i]>=(unsigned)a->count||a->data[p->indices[i]]!=lid_ui_rgb[i])ok=0;
        if(ok)return 1;
    }
    p->ready=0;
    uint32_t values[105],indices[5];int count=a->count;
    for(int i=0;i<count;i++)values[i]=a->data[i];
    for(unsigned i=0;i<5;i++){
        unsigned found=(unsigned)count;
        for(unsigned j=0;j<(unsigned)count;j++)if(values[j]==lid_ui_rgb[i]){found=j;break;}
        if(found==(unsigned)count){if(count>=100)return 0;values[count++]=lid_ui_rgb[i];}
        indices[i]=found;
    }
    if(count>a->capacity){
        void*memory=resize(context,a->data,(size_t)count*4);if(!memory)return 0;
        a->data=memory;a->capacity=count;
    }
    for(int i=a->count;i<count;i++)a->data[i]=values[i];
    a->count=count;for(unsigned i=0;i<5;i++)p->indices[i]=indices[i];
    p->data=(uintptr_t)a->data;p->count=count;p->ready=1;return 1;
}
static int lid_ui_put(uint16_t*out,unsigned cap,unsigned*n,uint16_t c){if(*n+1>=cap)return 0;out[(*n)++]=c;return 1;}
static int lid_ui_tag(const LidUIColorPalette*p,unsigned kind,uint16_t*out,unsigned cap,unsigned*n){
    if(!p||!p->ready)return 1;if(kind>=5||p->indices[kind]>=100)return 0;
    const uint16_t tag[5]={'#','C','L',(uint16_t)('0'+p->indices[kind]/10),(uint16_t)('0'+p->indices[kind]%10)};
    for(unsigned i=0;i<5;i++)if(!lid_ui_put(out,cap,n,tag[i]))return 0;return 1;
}
/* Input can carry private RS+[0..4] color spans from NotificationLayout only.
   Literal '#' is escaped for native parser, never interpreted as game markup.
   For a shop name, kind sets the entire name's color; 5 means span-only text. */
static int lid_ui_format(const LidUIColorPalette*p,const uint16_t*input,unsigned units,unsigned kind,uint16_t*out,unsigned cap){
    unsigned n=0;if(!input||!out||!cap||units>4095||kind>5)return 0;
    if(kind<5&&!lid_ui_tag(p,kind,out,cap,&n))return 0;
    for(unsigned i=0;i<units;i++){
        uint16_t c=input[i];if(!c)return 0;
        if(kind==5&&c==30&&i+1<units){
            uint16_t next=input[i+1];
            if(next>='0'&&next<='4'){if(!lid_ui_tag(p,next-'0',out,cap,&n))return 0;i++;continue;}
            if(next==30)i++;
        }
        if(!lid_ui_put(out,cap,&n,c)||(c=='#'&&!lid_ui_put(out,cap,&n,c)))return 0;
    }
    if(!lid_ui_tag(p,4,out,cap,&n))return 0;out[n]=0;return 1;
}
#endif
