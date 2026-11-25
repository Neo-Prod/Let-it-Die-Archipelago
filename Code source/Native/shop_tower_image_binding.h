#ifndef LID_SHOP_TOWER_IMAGE_BINDING_H
#define LID_SHOP_TOWER_IMAGE_BINDING_H
#include "shop_tower_auto_view.h"
#include "shop_tower_image_draw.h"
/* C entries are comparison tokens ONLY. The actual reflected private
   AutoInfo -> UIImage property keeps borrowed native images reachable.
   Render lookup reads only the CURRENT incoming engine-owned AutoInfo. */
typedef struct LidTowerImageToken {uint64_t automatic,image;unsigned offer;} LidTowerImageToken;
typedef struct LidTowerImageBinding {
    LidTowerAutoView*text;LidTowerIconProvider*provider;LidTowerImageDraw draw;
    uint32_t hold;unsigned prepared,busy,stopped,count;uint64_t item_select;
    LidTowerImageToken rows[36];
} LidTowerImageBinding;
static int lid_tib_prepare(LidTowerImageBinding*h,LidTowerAutoView*text,LidTowerIconProvider*provider,uint64_t hud){
    if(!h||h->prepared||!text||!text->prepared||!provider||!provider->ready||
       provider->current!=text->current||provider->display!=text->display)return 0;
    const M195Ops*o=&text->current->meta->memory;LidTowerImageBinding v={0};
    uint64_t p,structure,cls,image_class,image_prop,guide,unit,draw,mini;uint32_t offset,at,size,extent;
    /* Current new-info list path: ItemSelect.mItemMiniPanel.RenderMiniPanel.
       Verify the native by-value draw record before using these offsets. */
    if(!m195_read(o,text->current->meta->item_class,0x88,&extent,4)||
       !lid_mle_member(o,text->current->meta->item_class,"mItemMiniPanel",&p)||
       !lid_ml_property(o,p,"ObjectProperty",1,8,extent,0x58,&at)||at!=0xd44||
       !m195_read(o,p,0xb0,&mini,8)||!m195_named(o,mini,"BrgUIMenuPart_ItemMiniPanel")||
       !lid_tum_array(o,text->current->meta->item_class,"mUnits",extent,"BrgUIMenu_ItemSelect_Unit",0x3e8,&at,&unit)||at!=text->units||
       !lid_tav_struct(o,unit,"mDrawInfo",0xcc,0x3e8,0x310,&at,&draw)||
       !lid_tav_string(o,draw,"mItemImageId",0xcc,0,0x34,&at)||
       !lid_ml_struct_member(o,draw,"mItemType",&p)||!lid_ml_property(o,p,"ByteProperty",1,1,0xcc,0,&at)||at!=0x30)return 0;
    if(!lid_tum_owner(o,provider->ap->stream_class,"BrgUIImage",&image_class)||
       !lid_ml_struct_member(o,text->auto_class,"mDbMushroomEfc_C_p_Icon",&p)||
       !lid_ml_property(o,p,"StructProperty",1,0x20,0xd74,0x58,&offset)||
       !m195_read(o,p,0xb0,&structure,8)||!m195_named(o,structure,"BrgUIImageWithGuide")||
       !m195_read(o,structure,0x50,&cls,8)||!m195_named(o,cls,"ScriptStruct")||
       !m195_read(o,structure,0x88,&size,4)||size!=0x20||
       !lid_ml_struct_member(o,structure,"mImage",&image_prop)||
       !lid_ml_property(o,image_prop,"ObjectProperty",1,8,0x20,0,&at)||at!=0x10||
       !m195_read(o,image_prop,0xb0,&cls,8)||cls!=image_class||
       !lid_ml_struct_member(o,structure,"mGuide",&guide)||
       !lid_ml_property(o,guide,"ObjectProperty",1,8,0x20,0,&size)||size!=0x18||
       !lid_tid_prepare(&v.draw,provider->ap,hud))return 0;
    v.text=text;v.provider=provider;v.hold=offset+at;v.prepared=1;*h=v;return 1;
}
/* At the admitted completed-list/selection event, before native panel copies.
   Rebuild token array only here; image construction/requests never in draw. */
static int lid_tib_list(LidTowerImageBinding*h){
    if(!h||!h->prepared||h->busy||h->stopped){r299_note(13,1,0);return 0;}
    LidTowerDisplayView v;int scope=lid_tdv_collect(h->text->current,h->text->queue,h->text->display,&v);
    /* No private objects touched yet: a rejected read-only snapshot defers
       this event. Failures after mutation below remain terminal. */
    if(scope!=1){r299_note(13,2,(unsigned)(scope+1));return 0;}
    /* Moving the cursor must not rebuild all private objects or invoke the
       per-row full resolver again. One fresh list snapshot, then bounded
       current Unit/AutoInfo/property identity checks: no native resource call. */
    if(h->count==v.count&&h->item_select==v.roots.item_select){
        const M195Ops*o=&h->text->current->meta->memory;LidTSArray units;unsigned same=1;
        if(!lid_tuc_array(o,v.roots.item_select+h->text->units,0x3e8,36,&units)||units.count!=(int32_t)v.count)same=0;
        for(unsigned i=0;same&&i<v.count;i++){
            uint64_t automatic,image;
            if(h->rows[i].offer!=v.rows[i].offer||
               !m195_read(o,(uint64_t)(uintptr_t)units.data+i*0x3e8+h->text->base,h->text->auto_at,&automatic,8)||automatic!=h->rows[i].automatic||
               !lid_tav_marked(h->text,automatic,v.rows[i].offer)||
               !m195_read(o,automatic,h->hold,&image,8)||image!=h->rows[i].image)same=0;
        }
        if(same){r299_note(13,3,v.count);return 1;}
    }
    LidTowerImageToken previous[36];unsigned old_count=h->count;
    __builtin_memcpy(previous,h->rows,sizeof(previous));
    h->count=0;h->item_select=0;h->busy=1;volatile unsigned normal=0;int ok=0;
    __try{
        for(unsigned i=0;i<v.count;i++){
            int text=lid_tav_apply(h->text,i);
            if(text!=1){r299_note(13,4,i|((unsigned)(text+1)<<16));goto end;}
            const M195Ops*o=&h->text->current->meta->memory;LidTSArray units;uint64_t automatic,base,image,again;
            if(!lid_tuc_array(o,v.roots.item_select+h->text->units,0x3e8,36,&units)||i>=(unsigned)units.count||
               !m195_read(o,(uint64_t)(uintptr_t)units.data+i*0x3e8+h->text->base,h->text->auto_at,&automatic,8)||
               !lid_tav_marked(h->text,automatic,v.rows[i].offer)||
               !lid_tav_anchor(h->text,&v,i,&units,automatic,&base)){r299_note(13,5,i);goto end;}
            /* An existing private reflected binding is reused without another
               resource lookup on cursor changes. No old C pointer dereference. */
            unsigned retained=0;
            if(m195_read(o,automatic,h->hold,&image,8)&&lid_tid_stream_identity(&h->draw,image))
                for(unsigned j=0;j<old_count;j++)if(previous[j].automatic==automatic&&previous[j].image==image&&previous[j].offer==v.rows[i].offer)retained=1;
            if(!retained){
                if(!lid_tip_get(h->provider,v.rows[i].location,&image)||
                   !lid_tav_anchor(h->text,&v,i,&units,automatic,&base)||
                   !lid_tav_marked(h->text,automatic,v.rows[i].offer)){r299_note(13,6,i);goto end;}
                __builtin_memcpy((void*)(uintptr_t)(automatic+h->hold),&image,8);
            }
            if(!lid_tav_anchor(h->text,&v,i,&units,automatic,&base)||
               !m195_read(o,automatic,h->hold,&again,8)||again!=image){r299_note(13,7,i);goto end;}
            h->rows[i]=(LidTowerImageToken){automatic,image,v.rows[i].offer};
        }
        h->item_select=v.roots.item_select;h->count=v.count;ok=1;r299_note(13,8,v.count);
end:normal=1;
    }__finally{
        h->busy=0;if(!normal||!ok){h->count=0;h->item_select=0;h->stopped=1;}
    }
    return ok?1:-1;
}
/* Input is native's current live Unit/Info AutoInfo, never a cached pointer.
   No root/product/scout/queue scan or native allocation. At most36 C tokens. */
static int lid_tib_get(LidTowerImageBinding*h,uint64_t automatic,uint64_t*out){
    if(!h||!h->prepared||h->busy||h->stopped||!automatic||!out||!lid_tai_live(h->provider->ap))return 0;
    for(unsigned i=0;i<h->count;i++)if(h->rows[i].automatic==automatic){
        uint64_t image;
        if(!lid_tav_marked(h->text,automatic,h->rows[i].offer)||
           !m195_read(&h->text->current->meta->memory,automatic,h->hold,&image,8)||image!=h->rows[i].image||
           !lid_tid_stream_identity(&h->draw,image))return 0;
        *out=image;return 1;
    }
    return 0;
}
/* Pure bounded C lookup. Native CreateItemMiniPanelInfo_Item copies the
   original DB item ID into DrawInfo.mItemImageId. Duplicate keys refuse. */
static int lid_tib_key(const LidTowerImageBinding*h,const uint16_t*id,unsigned units,unsigned*out){
    if(!h||!id||!out||!units||units>=96||!h->count||h->count>36)return 0;
    unsigned found=36;
    for(unsigned i=0;i<h->count;i++){
        if(h->rows[i].offer>=184)return 0;
        const wchar_t*expected=lid_tower_stock_data[h->rows[i].offer].item;unsigned j=0;
        while(j<units&&expected[j]&&(uint16_t)expected[j]==id[j])j++;
        if(j==units&&!expected[j]){if(found!=36)return 0;found=i;}
    }
    if(found==36)return 0;*out=found;return 1;
}
/* Synchronous incoming MiniPanel/Info only. No scan/reflection/resource load
   or borrowed C-token dereference. Resolve fresh current owner/Unit first. */
static int lid_tib_mini(LidTowerImageBinding*h,uint64_t self,uint64_t info,uint64_t*out){
    if(!h||!h->prepared||h->busy||h->stopped||!h->count||h->count>36||!out||!info)return 0;
    LidTowerUiCurrent*c=h->text->current;const M195Ops*o=&c->meta->memory;
    uint64_t vt,manager,cls,menu,select,mini,automatic;uint8_t type;
    if(!c->sister_class||!m195_read(o,self,0,&vt,8)||vt!=h->text->image+0x21e1200||
       !m195_read(o,self,0x60,&manager,8)||!m195_read(o,manager,0x50,&cls,8)||cls!=c->manager_class||
       !m195_read(o,manager,c->shop,&menu,8)||!m195_read(o,menu,0x50,&cls,8)||cls!=c->sister_class||
       !m195_read(o,menu,c->meta->offsets[LID_TUM_ITEMSELECT],&select,8)||select!=h->item_select||
       !m195_read(o,select,0xd44,&mini,8)||mini!=self||
       !m195_read(o,info,0x30,&type,1)||(type!=4&&type!=5))return 0;
    LidTSString id;uint16_t text[96];unsigned row;
    if(!m195_read(o,info,0x34,&id,16)||id.count<2||id.count>96||id.capacity<id.count||id.capacity>4096||
       !m195_read(o,(uint64_t)(uintptr_t)id.data,0,text,(unsigned)id.count*2)||text[id.count-1])return 0;
    if(!lid_tib_key(h,text,(unsigned)id.count-1,&row))return 0;
    LidTSArray units;
    if(!lid_tuc_array(o,select+h->text->units,0x3e8,36,&units)||units.count<(int32_t)h->count||
       !m195_read(o,(uint64_t)(uintptr_t)units.data+row*0x3e8+h->text->base,h->text->auto_at,&automatic,8)||
       automatic!=h->rows[row].automatic||
       !lid_tuc_literal(o,(uint64_t)(uintptr_t)units.data+row*0x3e8+0x310+0x34,
           lid_tower_stock_data[h->rows[row].offer].item))return 0;
    return lid_tib_get(h,automatic,out);
}
#endif
