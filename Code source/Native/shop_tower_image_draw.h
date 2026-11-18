#ifndef LID_SHOP_TOWER_IMAGE_DRAW_H
#define LID_SHOP_TOWER_IMAGE_DRAW_H
#include "shop_tower_icon_provider.h"
/* Synchronous presentation primitive only. Caller owns admission and the live
   reflected references throughout this call. No UObject/image retained here.
   The source image is the original native main icon, NOT a layout guide.
   Atlas/map images are refused rather than guessing their packed geometry. */
typedef void(*LidTowerExtImage)(void*,float,float,float,float,void*,unsigned,unsigned);
typedef struct LidTowerImageDraw {
    LidTowerAPImage*ap;uint64_t hud_class;uint32_t size_x,size_y,pos_x,pos_y,map,image_map;
    unsigned prepared;
} LidTowerImageDraw;
static int lid_tid_hud(const M195Ops*o,uint64_t hud,uint64_t expected_class,uint64_t image,uint64_t*out){
    uint64_t cls,vt,method;
    if(!hud||hud>UINT64_MAX-0x58||!m195_read(o,hud,0x50,&cls,8)||
       (expected_class&&cls!=expected_class)||!m195_read(o,hud,0,&vt,8)||
       !m195_read(o,vt,0x7e0,&method,8)||method!=image+0xc13a10)return 0;
    *out=cls;return 1;
}
static int lid_tid_prepare(LidTowerImageDraw*h,LidTowerAPImage*ap,uint64_t hud){
    static const uint8_t prefix[24]={0x48,0x8b,0xc4,0xf3,0x0f,0x11,0x50,0x18,0xf3,0x0f,0x11,0x48,0x10,0x57,0x41,0x57,0x48,0x81,0xec,0x88,0x01,0x00,0x00,0x48};
    LidTowerImageDraw v={0};uint8_t bytes[24];uint64_t cls,next,seen[32];unsigned n=0,found=0;
    if(!h||h->prepared||!lid_tai_live(ap)||ap->phase!=3||ap->image>UINT64_MAX-0xc13a28)return 0;
    const M195Ops*o=ap->ops.memory;
    if(!lid_tid_hud(o,hud,0,ap->image,&v.hud_class)||!m195_read(o,ap->image,0xc13a10,bytes,24))return 0;
    for(unsigned i=0;i<24;i++)if(bytes[i]!=prefix[i])return 0;
    cls=v.hud_class;
    while(cls){
        if(n==32)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==cls)return 0;seen[n++]=cls;
        if(m195_named(o,cls,"BrgHUDBase"))found++;
        if(!m195_read(o,cls,0x78,&next,8))return 0;cls=next;
    }
    if(found!=1)return 0;
    /* Exact primary/blueprint-overlay draw CALLs. A later owning wrapper can
       distinguish them without changing carrier IDs/private DB names or
       assuming the first arbitrary draw in a panel is its main image. */
    const uint32_t sites[4]={0x14c8a3a,0x14c8b14,0x14c6053,0x14c6108};
    static const uint8_t call[7]={0x41,0xff,0x90,0x68,0x02,0x00,0x00};
    for(unsigned i=0;i<4;i++){
        if(!m195_read(o,ap->image,sites[i],bytes,7))return 0;
        for(unsigned j=0;j<7;j++)if(bytes[j]!=call[j])return 0;
    }
    const char*names[6]={"mUseSizeX","mUseSizeY","mPosX","mPosY","mImageMapInfoNo","mImageMapImageInfoNo"};
    const uint32_t expected[6]={0x98,0x9c,0xc4,0xc8,0xb0,0xb4};uint32_t fields[6];
    for(unsigned i=0;i<6;i++)if(!lid_api_field(o,ap->stream_class,"BrgUIImage",names[i],"IntProperty",4,&fields[i],0)||fields[i]!=expected[i])return 0;
    v.ap=ap;v.size_x=fields[0];v.size_y=fields[1];v.pos_x=fields[2];v.pos_y=fields[3];v.map=fields[4];v.image_map=fields[5];v.prepared=1;*h=v;return 1;
}
/* Return 1 primary /2 blueprint overlay /0 other. Not AP admission: an actual
   owner must ALSO prove its live grid/selected-panel render scope and image
   binding. lane 0=ItemSelect grid; lane 1=ItemMiniPanel item renderer. */
static unsigned lid_tid_site(const LidTowerImageDraw*h,uintptr_t caller,unsigned lane){
    if(!h||!h->prepared||lane>1)return 0;
    uint64_t primary=lane?0x14c605a:0x14c8a41,overlay=lane?0x14c610f:0x14c8b1b;
    if(caller==h->ap->image+primary)return 1;
    return caller==h->ap->image+overlay?2:0;
}
/* A manager-owned Stream can be returned before its texture is loaded. Its
   native SetupMIC performs that load when drawn; size is not identity. */
static int lid_tid_stream_identity(LidTowerImageDraw*h,uint64_t object){
    uint64_t cls,vt;int32_t map,image_map;
    if(!object||object>UINT64_MAX-0xcc||
       !m195_read(h->ap->ops.memory,object,0x50,&cls,8)||cls!=h->ap->stream_class||
       !m195_read(h->ap->ops.memory,object,0,&vt,8)||vt!=h->ap->image+0x1ff1e70||
       !m195_read(h->ap->ops.memory,object,h->map,&map,4)||map!=-1||
       !m195_read(h->ap->ops.memory,object,h->image_map,&image_map,4)||image_map!=-1)return 0;
    return 1;
}
static int lid_tid_stream(LidTowerImageDraw*h,uint64_t object,int32_t*width,int32_t*height){
    return lid_tid_stream_identity(h,object)&&
        m195_read(h->ap->ops.memory,object,h->size_x,width,4)&&*width>=1&&*width<=8192&&
        m195_read(h->ap->ops.memory,object,h->size_y,height,4)&&*height>=1&&*height<=8192;
}
/* x/y are Draw_Image_UsePos arguments BEFORE it adds the source mPosX/Y.
   Native Draw_ExtImage retains current HUD transform/color/MIC preparation and
   Stream readiness. Replacement mPosX/Y do not move the carrier's rectangle.
   0 means refused/no draw; caller must preserve original native behavior.
   1 means native method called once (not a claim texture already rendered).
   No exception swallowing/second draw: owning event handles cleanup/LastError. */
static int lid_tid_draw(LidTowerImageDraw*h,uint64_t hud,uint64_t source,uint64_t replacement,
                        float x,float y,unsigned horizontal,unsigned vertical){
    if(!h||!h->prepared||!lid_tai_live(h->ap)||h->ap->phase!=3||
       !(x>=-1048576.0f&&x<=1048576.0f&&y>=-1048576.0f&&y<=1048576.0f))return 0;
    uint64_t cls;int32_t w,height,px,py;
    if(!lid_tid_hud(h->ap->ops.memory,hud,h->hud_class,h->ap->image,&cls)||
       !lid_tid_stream(h,source,&w,&height)||!lid_tid_stream_identity(h,replacement)||
       !m195_read(h->ap->ops.memory,source,h->pos_x,&px,4)||px < -8192||px>8192||
       !m195_read(h->ap->ops.memory,source,h->pos_y,&py,4)||py < -8192||py>8192)return 0;
    float left=x+(float)px,top=y+(float)py;
    ((LidTowerExtImage)(uintptr_t)(h->ap->image+0xc13a10))((void*)(uintptr_t)hud,
        left,top,left+(float)w,top+(float)height,(void*)(uintptr_t)replacement,horizontal,vertical);
    return 1;
}
#endif
