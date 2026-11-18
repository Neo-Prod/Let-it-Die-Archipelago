#ifndef LID_SHOP_TOWER_AP_IMAGE_H
#define LID_SHOP_TOWER_AP_IMAGE_H
#include "shop_ap_image.h"
/* Own tower image, independent of visiting Momoka. Reuses verified native
   profiles/pixels/pure field helpers; not a fabricated Momoka labels host.
   Allocation algorithm retained from R260, own objects RootSet at construction,
   one attempt and resident retention even after failure. Never free/unroot.
   No generation/scout/native payment/Save or rendering hook here. */
typedef struct LidTowerAPImageOps {
    void*context;uint32_t(*thread)(void*);int(*live)(void*);int(*unlocked)(void*);
    const M195Ops*memory;uint32_t owner;
} LidTowerAPImageOps;
typedef struct LidTowerAPImage {
    LidTowerAPImageOps ops;uint64_t image,texture_class,stream_class,texture,stream;
    LidAPBool settings[6];uint32_t compression,filter,group,address_x,address_y;
    uint32_t loaded,loaded_mask,request,request_mask,owner,bases;
    int phase;
} LidTowerAPImage;
static int lid_tai_prepare(LidTowerAPImage*a,const LidTowerAPImageOps*ops,uint64_t image,int verified){
    uint64_t flags,cls;uint8_t bytes[16];
    if(!a||a->phase||!ops||!ops->memory||!ops->thread||!ops->live||!ops->unlocked||!ops->owner||
       verified!=1||!image||image>UINT64_MAX-0x29c7118||
       ops->thread(ops->context)!=ops->owner||ops->live(ops->context)!=1||ops->unlocked(ops->context)!=1)return 0;
    const M195Ops*o=ops->memory;
    for(unsigned p=0;p<sizeof(lid_ap_prefixes)/sizeof(lid_ap_prefixes[0]);p++){
        if(!m195_read(o,image,lid_ap_prefixes[p].rva,bytes,16))return 0;
        for(unsigned i=0;i<16;i++)if(bytes[i]!=lid_ap_prefixes[p].bytes[i])return 0;
    }
    if(!m195_read(o,image,0x2963b50,&a->texture_class,8)||!m195_named(o,a->texture_class,"Texture2D")||
       !m195_read(o,a->texture_class,0x50,&cls,8)||!m195_named(o,cls,"Class")||
       !m195_read(o,a->texture_class,0x10,&flags,8)||!(flags&0x4000)||
       !m195_read(o,image,0x29c7110,&a->stream_class,8)||!m195_named(o,a->stream_class,"BrgUIImage_Stream")||
       !m195_read(o,a->stream_class,0x50,&cls,8)||!m195_named(o,cls,"Class")||
       !m195_read(o,a->stream_class,0x10,&flags,8)||!(flags&0x4000))return 0;
    const char*names[6]={"CompressionNone","CompressionNoAlpha","DeferCompression","NeverStream","bNoTiling","SRGB"};
    const unsigned values[6]={1,0,0,1,1,0};
    for(unsigned i=0;i<6;i++){
        if(!lid_api_field(o,a->texture_class,"Texture",names[i],"BoolProperty",4,&a->settings[i].at,&a->settings[i].mask))return 0;
        a->settings[i].value=values[i];
    }
#define LID_API_BYTE(cls,owner,name,dst) if(!lid_api_field(o,cls,owner,name,"ByteProperty",1,&a->dst,0))return 0
    LID_API_BYTE(a->texture_class,"Texture","CompressionSettings",compression);
    LID_API_BYTE(a->texture_class,"Texture","Filter",filter);
    LID_API_BYTE(a->texture_class,"Texture","LODGroup",group);
    LID_API_BYTE(a->texture_class,"Texture2D","AddressX",address_x);
    LID_API_BYTE(a->texture_class,"Texture2D","AddressY",address_y);
#undef LID_API_BYTE
    if(!lid_api_field(o,a->stream_class,"BrgUIImage_Stream","mLoaded","BoolProperty",4,&a->loaded,&a->loaded_mask)||
       !lid_api_field(o,a->stream_class,"BrgUIImage_Stream","mLoadRequest","BoolProperty",4,&a->request,&a->request_mask)||
       !lid_api_field(o,a->stream_class,"BrgUIImage_Stream","mOwner","ObjectProperty",8,&a->owner,0)||
       !lid_api_field(o,a->stream_class,"BrgUIImage","mBaseData","ArrayProperty",16,&a->bases,0)||
       a->bases!=0x80||a->loaded!=0xf8||a->request!=0xf8||a->loaded_mask!=1||a->request_mask!=2)return 0;
    a->ops=*ops;a->image=image;a->phase=1;return 1;
}
static int lid_tai_live(LidTowerAPImage*a){
    return a&&a->ops.memory&&a->ops.thread&&a->ops.live&&
        a->ops.thread(a->ops.context)==a->ops.owner&&a->ops.live(a->ops.context)==1;
}
static int lid_tai_get(LidTowerAPImage*a,uint64_t*out){
    if(!out||!a||a->phase!=3||!lid_tai_live(a))return 0;
    const M195Ops*o=a->ops.memory;uint32_t loaded;
    if(!lid_api_object(o,a->texture,a->texture_class,a->image+0x1d2d760)||
       !lid_api_object(o,a->stream,a->stream_class,a->image+0x1ff1e70)||
       !m195_read(o,a->stream,a->loaded,&loaded,4)||!(loaded&a->loaded_mask)||(loaded&a->request_mask))return 0;
    *out=a->stream;return 1;
}
/* No UI disk IO/hash/decode. Native allocator/init/bulk/update/MIC calls only.
   No price/save/bridge lock may span this call; real game rendering unvalidated. */
static __attribute__((noinline)) void lid_tai_copy_pixels(void*dst,const void*src,size_t n){
    __builtin_memcpy(dst,src,n);
}
static int lid_tai_create(LidTowerAPImage*a){
    if(!a||a->phase!=1||!lid_tai_live(a)||a->ops.unlocked(a->ops.context)!=1)return 0;
    a->phase=2;uint64_t image=a->image,error_device,outer,mip_data,mip,vt,entry,base_object,base_class,texture_field,held;
    uint32_t count,capacity,flags;M197StringHeader bases;
    const M195Ops*o=a->ops.memory;
    if(!m195_read(o,image,0x2813d20,&error_device,8)||!error_device)goto fail;
    outer=((uint64_t(*)(void))(uintptr_t)(image+0x1587f0))();if(!outer)goto fail;
    LidAPConstruct construct=(LidAPConstruct)(uintptr_t)(image+0x166810);
    /* Same Transient flag as native texture example + RootSet for OUR object. */
    a->texture=construct(a->texture_class,outer,0,0x400000004000ull,0,error_device,0,0);
    if(!lid_api_object(o,a->texture,a->texture_class,image+0x1d2d760)||!lid_tai_live(a))goto fail;
    ((void(*)(void*,int,int,uint8_t))(uintptr_t)(image+0x3777f0))((void*)(uintptr_t)a->texture,512,512,2);
    for(unsigned i=0;i<6;i++){
        LidAPBool*b=&a->settings[i];if(!m195_read(o,a->texture,b->at,&flags,4))goto fail;
        flags=b->value?flags|b->mask:flags&~b->mask;
        __builtin_memcpy((void*)(uintptr_t)(a->texture+b->at),&flags,4);
    }
    *(uint8_t*)(uintptr_t)(a->texture+a->compression)=0;
    *(uint8_t*)(uintptr_t)(a->texture+a->filter)=1;
    *(uint8_t*)(uintptr_t)(a->texture+a->group)=13; /* native dynamic-texture example */
    *(uint8_t*)(uintptr_t)(a->texture+a->address_x)=1;
    *(uint8_t*)(uintptr_t)(a->texture+a->address_y)=1;
    if(!m195_read(o,a->texture,0x13c,&mip_data,8)||!mip_data||
       !m195_read(o,a->texture,0x144,&count,4)||count!=1||
       !m195_read(o,a->texture,0x148,&capacity,4)||capacity<count||capacity>4||
       !m195_read(o,mip_data,0,&mip,8)||!mip||
       ((int(*)(void*))(uintptr_t)(image+0x106a20))((void*)(uintptr_t)mip)!=1048576)goto fail;
    void*destination=((void*(*)(void*,int))(uintptr_t)(image+0x10cf90))((void*)(uintptr_t)mip,2);
    volatile unsigned copied=0;
    /* Preserve a compiler-observable potentially exceptional call in the
       protected region. An intrinsic-only memcpy caused Clang to omit its
       finally unwind table even with the production SEH flags. */
    void(*volatile copy_pixels)(void*,const void*,size_t)=lid_tai_copy_pixels;
#if LID_SHOP_EXEC_SEH_ENABLED
    __try {
#endif
        if(destination){copy_pixels(destination,lid_ap_pixels,sizeof(lid_ap_pixels));copied=1;}
#if LID_SHOP_EXEC_SEH_ENABLED
    } __finally {
#endif
        ((void(*)(void*))(uintptr_t)(image+0x118f20))((void*)(uintptr_t)mip);
#if LID_SHOP_EXEC_SEH_ENABLED
    }
#endif
    if(!copied)goto fail;
    if(!m195_read(o,a->texture,0,&vt,8)||
       !m195_read(o,vt,0x2a8,&entry,8)||entry!=image+0x387e00)goto fail;
    ((void(*)(void*,const void*,uint32_t))(uintptr_t)entry)((void*)(uintptr_t)a->texture,lid_ap_pixels,sizeof(lid_ap_pixels));
    if(!m195_read(o,vt,0x278,&entry,8)||entry!=image+0x389080)goto fail;
    ((void(*)(void*))(uintptr_t)entry)((void*)(uintptr_t)a->texture);
    if(!lid_tai_live(a))goto fail;
    a->stream=construct(a->stream_class,outer,0,0x400000004000ull,0,error_device,0,0);
    if(!lid_api_object(o,a->stream,a->stream_class,image+0x1ff1e70)||
       !m195_read(o,a->stream,a->owner,&held,8)||held||
       !m195_read(o,a->stream,a->loaded,&flags,4)||(flags&(a->loaded_mask|a->request_mask))||
       !m195_read(o,a->stream,0,&vt,8)||!m195_read(o,vt,0x270,&entry,8)||entry!=image+0xc49600)goto fail;
    ((void(*)(void*,void*,void*,int,int,int,int,int))(uintptr_t)entry)
        ((void*)(uintptr_t)a->stream,(void*)(uintptr_t)a->texture,0,0,0,-1,-1,1);
    if(!m195_read(o,a->stream,a->bases,&bases,16)||bases.count!=1||bases.capacity<1||bases.capacity>64||
       !m195_read(o,bases.data,0,&base_object,8)||
       !m195_read(o,base_object,0x50,&base_class,8)||!m195_named(o,base_class,"BrgUIImageBase")||
       !m212_member(o,base_object,"BrgUIImageBase","mTexture",&texture_field))goto fail;
    uint32_t texture_at;
    if(!lid_ml_property(o,texture_field,"ObjectProperty",1,8,65536,0x58,&texture_at)||
       !m195_read(o,base_object,texture_at,&held,8)||held!=a->texture)goto fail;
    /* Explicit normal material attributes=0 for STRAIGHT alpha. Do not apply
       Enma sticker effects or premultiplication to the AP fallback. */
    if(!((uint64_t(*)(void*,const uint32_t*,void*,float,float))(uintptr_t)(image+0xd7bba0))
          ((void*)(uintptr_t)a->stream,0,0,0.0f,0.0f)||!lid_tai_live(a))goto fail;
    if(!m195_read(o,a->stream,a->loaded,&flags,4))goto fail;
    flags=(flags|a->loaded_mask)&~a->request_mask;
    __builtin_memcpy((void*)(uintptr_t)(a->stream+a->loaded),&flags,4);
    a->phase=3;return 1;
fail:a->phase=-1;return 0;
}
#endif
