#ifndef LID_SHOP_MOMOKA_ICONS_H
#define LID_SHOP_MOMOKA_ICONS_H
#include "shop_momoka_icon_provider.h"
#include "shop_ap_image.h"
#include "shop_momoka_stock.h"
typedef struct LidMomokaIcons {
    LidMomokaIconProvider native;LidAPImage*ap;LidMomokaStock*stock;
    uint32_t explanation_image,explanation_premium,explanation_mask,grade;
    uint32_t list_image,list_premium,list_mask;unsigned ready;
    uint32_t filter;uint64_t last_list_identity;uint8_t last_filter;unsigned listed;
} LidMomokaIcons;
static int lid_mic_image(const M195Ops*o,uint64_t structure,const char*name,uint32_t extent,
    uint64_t expected_class,uint32_t*out){
    uint64_t property,cls;
    return lid_ml_struct_member(o,structure,name,&property)&&
        lid_ml_property(o,property,"ObjectProperty",1,8,extent,0,out)&&
        m195_read(o,property,0xb0,&cls,8)&&cls==expected_class;
}
static int lid_mic_bool(const M195Ops*o,uint64_t structure,const char*name,uint32_t extent,
    uint32_t*out,uint32_t*mask){
    uint64_t property;return lid_ml_struct_member(o,structure,name,&property)&&
        lid_ml_property(o,property,"BoolProperty",1,4,extent,0,out)&&
        m195_read(o,property,0xb0,mask,4)&&*mask&&!(*mask&(*mask-1));
}
static int lid_mic_prepare(LidMomokaIcons*v,LidAPImage*ap,LidMomokaStock*stock,int verified){
    uint64_t fallback,explain,list,property,structure,inner;
    if(!v||v->ready||!ap||!stock||!stock->ready||!lid_api_get(ap,&fallback))return 0;
    LidMomokaLabels*b=stock->labels;const M195Ops*o=&b->root->memory;
    if(ap->host->labels.root!=b->root||!lid_ml_current(b,&explain)||!lid_ms_current(b,&list)||
       !lid_ml_field(o,list,"BrgUIMenu_SkillExchange_SkillList","mFilter","ByteProperty",1,&v->filter)||
       !lid_mip_prepare(&v->native,b,ap->image,verified)||
       !m212_member(o,explain,"BrgUIMenu_SkillExplane","mSkillInfo",&property)||
       !m195_read(o,property,0xb0,&structure,8)||
       !lid_mic_image(o,structure,"mExplStickerImage",b->stride,ap->stream_class,&v->explanation_image)||
       !lid_mic_bool(o,structure,"mExplIsPremium",b->stride,&v->explanation_premium,&v->explanation_mask)||
       !lid_ml_struct_member(o,structure,"mExplGrade",&property)||
       !lid_ml_property(o,property,"IntProperty",1,4,b->stride,0,&v->grade)||
       !m212_member(o,list,"BrgUIMenu_SkillExchange_SkillList","mSkillListDetail",&property)||
       !m195_read(o,property,0xb0,&inner,8)||!m195_read(o,inner,0xb0,&structure,8)||
       !lid_mic_image(o,structure,"stickerImage",stock->stride,ap->stream_class,&v->list_image)||
       !lid_mic_bool(o,structure,"isPremium",stock->stride,&v->list_premium,&v->list_mask)||
       (v->list_premium==stock->enabled&&(v->list_mask&stock->mask)))return 0;
    uint32_t at[7]={b->skill,b->name,ap->host->description.text,ap->host->description.category,
                   v->explanation_image,v->grade,v->explanation_premium};
    const uint32_t sizes[7]={16,16,16,16,8,4,4};
    for(unsigned i=0;i<7;i++)for(unsigned j=0;j<i;j++)
        if(at[i]<at[j]+sizes[j]&&at[j]<at[i]+sizes[i])return 0;
    uint32_t group=ap->host->description.group;
    if(v->explanation_premium<group+4&&group<v->explanation_premium+4&&
       (v->explanation_premium!=group||(v->explanation_mask&ap->host->description.mask)))return 0;
    if((v->list_image<stock->skill+16&&stock->skill<v->list_image+8)||
       (v->list_image<v->list_premium+4&&v->list_premium<v->list_image+8)||
       (v->list_image<stock->enabled+4&&stock->enabled<v->list_image+8)||
       (v->list_premium<stock->skill+16&&stock->skill<v->list_premium+4))return 0;
    v->ap=ap;v->stock=stock;v->ready=1;return 1;
}
static int lid_mic_offer(LidMomokaLabels*b,const M195Ops*o,uint64_t at,uint32_t skill,uint32_t stride,unsigned*out){
    M195Signature sig={0};uint16_t id[128];unsigned units;
    if(!out||stride>65535)return 0;sig.skill_offset=skill;sig.parms_size=(uint16_t)stride;
    if(!m197_string(o,at,&sig,id,&units))return 0;
    const LidShopRuntimeCatalogue*c=b->display->catalogue;
    for(unsigned i=0;i<c->momoka_count;i++){
        const LidShopUiSku*s=&c->momoka[i];if(s->units!=units)continue;
        unsigned j=0;while(j<units&&id[j]==s->skill[j])j++;
        if(j==units){*out=s->offer;return 1;}
    }
    *out=UINT32_MAX;return 1;
}
static int lid_mic_resolve(LidMomokaIcons*v,unsigned offer,uint64_t*out,unsigned*premium){
    const uint8_t*row=lid_sd_row(v->native.labels->display,7500001ull+offer);
    if(!row||!out||!premium)return 0;
    *premium=r15_r32(row+24)==2; /* actual local premium-decal catalogue kind */
    int result=lid_mip_get(&v->native,7500001ull+offer,out);
    if(result==LID_MIP_NATIVE)return 1;
    /* No carrier-art fallback. For absent/unavailable native art use AP only
       if the current rooted buy scope is STILL valid after that native call. */
    return lid_api_get(v->ap,out);
}
static int lid_mic_store(const M195Ops*o,uint64_t at,uint32_t image_at,uint32_t premium_at,uint32_t mask,
    uint64_t image,unsigned premium){
    uint64_t old;uint32_t bits;
    if(!m195_read(o,at,image_at,&old,8)||!m195_read(o,at,premium_at,&bits,4))return 0;
    if(old!=image)__builtin_memcpy((void*)(uintptr_t)(at+image_at),&image,8);
    uint32_t desired=premium?bits|mask:bits&~mask;
    if(desired!=bits)__builtin_memcpy((void*)(uintptr_t)(at+premium_at),&desired,4);
    return 1;
}
/* Native images immediately enter reflected UObject properties before another
   native getter can allocate. No C-only cache of borrowed Stream pointers. */
static int lid_mic_explanation(LidMomokaIcons*v){
    uint64_t explain,again;if(!v||!v->ready||!lid_ml_current(v->native.labels,&explain))return 0;
    LidMomokaLabels*b=v->native.labels;const M195Ops*o=&b->root->memory;
    for(unsigned i=0;i<2;i++){
        if(explain>UINT64_MAX-b->info-(uint64_t)i*b->stride)return 0;
        uint64_t at=explain+b->info+(uint64_t)i*b->stride,image;unsigned offer,after,premium;M197StringHeader id;
        if(!lid_ml_current(b,&again)||again!=explain||!m195_read(o,at,b->skill,&id,16))return 0;
        if(!id.count)continue;
        if(!lid_mic_offer(b,o,at,b->skill,b->stride,&offer))return 0;
        if(offer==UINT32_MAX)continue;
        if(!lid_mic_resolve(v,offer,&image,&premium)||!lid_ml_current(b,&again)||again!=explain||
           !lid_mic_offer(b,o,at,b->skill,b->stride,&after)||after!=offer||
           !lid_mic_store(o,at,v->explanation_image,v->explanation_premium,v->explanation_mask,image,premium))return 0;
        /* Carrier's native star count must not pretend to describe AP reward. */
        int32_t grade;if(!m195_read(o,at,v->grade,&grade,4))return 0;
        if(grade){grade=0;__builtin_memcpy((void*)(uintptr_t)(at+v->grade),&grade,4);}
    }
    return 1;
}
static int lid_mic_list(LidMomokaIcons*v){
    uint64_t list,again;if(!v||!v->ready||!lid_ms_current(v->native.labels,&list))return 0;
    LidMomokaStock*s=v->stock;LidMomokaLabels*b=v->native.labels;const M195Ops*o=&b->root->memory;
    for(unsigned array=0;array<2;array++){
        M197StringHeader a,after;
        if(!lid_ms_current(b,&again)||again!=list||!lid_ms_array(o,list+s->arrays[array],s->stride,&a))return 0;
        for(int i=0;i<a.count;i++){
            uint64_t at=a.data+(uint64_t)i*s->stride,image;unsigned offer,check,premium;
            if(!lid_mic_offer(b,o,at,s->skill,s->stride,&offer))return 0;
            if(offer==UINT32_MAX)continue;
            if(!lid_mic_resolve(v,offer,&image,&premium)||!lid_ms_current(b,&again)||again!=list||
               !lid_ms_array(o,list+s->arrays[array],s->stride,&after)||after.data!=a.data||after.count!=a.count||after.capacity!=a.capacity||
               !lid_mic_offer(b,o,at,s->skill,s->stride,&check)||check!=offer||
               !lid_mic_store(o,at,v->list_image,v->list_premium,v->list_mask,image,premium))return 0;
        }
    }
    uint8_t filter;
    if(!lid_ms_current(b,&again)||again!=list||!m195_read(o,list,v->filter,&filter,1))return 0;
    /* Identity token only, NEVER dereferenced later and not a GC reference. */
    v->last_list_identity=list;v->last_filter=filter;v->listed=1;return 1;
}
/* Filter rebuild clears stickerImage even when a master image was populated.
   Check only on an actual selection event, never a frame/timer poll. */
static int lid_mic_list_needed(LidMomokaIcons*v){
    uint64_t list;uint8_t filter;
    if(!v||!v->ready||!lid_ms_current(v->native.labels,&list)||
       !m195_read(&v->native.labels->root->memory,list,v->filter,&filter,1))return -1;
    return !v->listed||list!=v->last_list_identity||filter!=v->last_filter;
}
#endif
