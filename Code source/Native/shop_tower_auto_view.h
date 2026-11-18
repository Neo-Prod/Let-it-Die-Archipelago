#ifndef LID_SHOP_TOWER_AUTO_VIEW_H
#define LID_SHOP_TOWER_AUTO_VIEW_H
#include "shop_tower_display_view.h"
#include "shop_tower_auto_profiles.h"
/* Native-owned presentation ONLY. The original pooled AutoInfo, inventory,
   floor products, billing IDs and price fields are never modified. This helper
   has no hook/event publisher. Its future owning wrapper must forward native
   behavior, preserve LastError and prove synchronous UI lifetime. */
typedef struct LidTowerAutoView {
    LidTowerUiCurrent*current;LidTowerQueue*queue;const LidShopDisplay*display;
    uintptr_t image;uint64_t auto_class;uint32_t units,base,auto_at,compare_at,entity,item_id;
    uint32_t name,description,part_name,db_item,db_id,db_type;
    void*(*copy)(void*,const void*);void(*clear)(void*);void(*build)(void*,int);
    LidMomokaStringAssign assign;unsigned prepared,busy,stopped;uint32_t marker;
} LidTowerAutoView;
static int lid_tav_struct(const M195Ops*o,uint64_t owner,const char*name,unsigned size,unsigned extent,
                          unsigned expected,uint32_t*offset,uint64_t*out){
    uint64_t p,s,cls;uint32_t actual;
    if(!lid_ml_struct_member(o,owner,name,&p)||!lid_ml_property(o,p,"StructProperty",1,size,extent,0,offset)||*offset!=expected||
       !m195_read(o,p,0xb0,&s,8)||!m195_read(o,s,0x50,&cls,8)||!m195_named(o,cls,"ScriptStruct")||
       !m195_read(o,s,0x88,&actual,4)||actual!=size)return 0;*out=s;return 1;
}
static int lid_tav_string(const M195Ops*o,uint64_t owner,const char*name,unsigned extent,unsigned minimum,unsigned expected,uint32_t*out){
    uint64_t p;return lid_ml_struct_member(o,owner,name,&p)&&
        lid_ml_property(o,p,"StrProperty",1,16,extent,minimum,out)&&*out==expected;
}
/*0 class not loaded yet,-1 malformed,1 immutable metadata ready. No native
   allocation or current tower shop instance needed; no fabricated readiness. */
/* One UI-owner cold diagnostic; no borrowed UObject retained. */
static unsigned g_lid_tav_prepare_stage;
static int lid_tav_prepare(LidTowerAutoView*h,LidTowerUiCurrent*c,LidTowerQueue*q,const LidShopDisplay*d,uintptr_t image,int verified){
    g_lid_tav_prepare_stage=1;
    if(!h||h->prepared||!c||!c->prepared||!q||!q->initialized||!d||d->catalogue!=c->catalogue||
       verified!=1||image!=c->meta->image||image>UINTPTR_MAX-0xf8a2c98||
       q->ops.thread(q->ops.context)!=q->owner||q->owner!=c->catalogue->ui||
       !r15_same_context(&q->context,&c->catalogue->context)||!r15_equal(q->catalogue,c->catalogue->tower_hash,32))return -1;
    g_lid_tav_prepare_stage=2;if(!c->ops.live(c->ops.live_context))return 0;
    const M195Ops*o=&c->meta->memory;LidTowerAutoView v={0};uint64_t structure,info,base,entity,db,p,property_class;
    uint32_t extent,at;uint8_t actual[24];
#define LID_TAV_CHECK(n,condition) do{g_lid_tav_prepare_stage=(n);if(!(condition))return -1;}while(0)
    LID_TAV_CHECK(3,m195_read(o,image,0xf8a2c90,&v.auto_class,8));if(!v.auto_class)return 0;
    LID_TAV_CHECK(4,lid_tum_class(o,v.auto_class,"BrgDropItemBaseInfo_Auto")&&m195_read(o,v.auto_class,0x88,&extent,4)&&extent==0xd74);
    for(unsigned i=0;i<4;i++){
        LID_TAV_CHECK(10+i,m195_read(o,image,lid_tav_profiles[i].rva,actual,24));
        for(unsigned j=0;j<24;j++)LID_TAV_CHECK(10+i,actual[j]==lid_tav_profiles[i].bytes[j]);
    }
    LID_TAV_CHECK(20,m195_read(o,c->meta->item_class,0x88,&extent,4)&&extent>=0x254&&extent<=65536);
    LID_TAV_CHECK(21,lid_tum_array(o,c->meta->item_class,"mUnits",extent,"BrgUIMenu_ItemSelect_Unit",0x3e8,&v.units,&structure)&&v.units==0x244);
    LID_TAV_CHECK(22,lid_tav_struct(o,structure,"mInfo",0x30c,0x3e8,4,&at,&info));
    LID_TAV_CHECK(23,lid_tav_struct(o,info,"mBaseInfo",0x174,0x30c,0x198,&v.base,&base));
    LID_TAV_CHECK(24,m195_named(o,base,"BrgDropItemBaseInfo"));
    LID_TAV_CHECK(25,lid_ml_struct_member(o,base,"mAutoInfo",&p)&&lid_ml_property(o,p,"ObjectProperty",1,8,0x174,0,&v.auto_at)&&v.auto_at==0x164&&m195_read(o,p,0xb0,&property_class,8)&&property_class==v.auto_class);
    LID_TAV_CHECK(26,lid_ml_struct_member(o,base,"mCmpAutoInfo",&p)&&lid_ml_property(o,p,"ObjectProperty",1,8,0x174,0,&v.compare_at)&&v.compare_at==0x16c&&m195_read(o,p,0xb0,&property_class,8)&&property_class==v.auto_class);
    LID_TAV_CHECK(27,lid_tav_struct(o,base,"mDbItemEntity",0x24,0x174,0x13c,&v.entity,&entity));
    LID_TAV_CHECK(28,lid_tav_string(o,entity,"mItemId",0x24,0,0x10,&v.item_id));
    v.base+=4;
    LID_TAV_CHECK(30,lid_tav_string(o,v.auto_class,"mDbItemName",0xd74,0x58,0xaec,&v.name));
    LID_TAV_CHECK(31,lid_tav_string(o,v.auto_class,"mDbItemDesc",0xd74,0x58,0xafc,&v.description));
    LID_TAV_CHECK(32,lid_tav_string(o,v.auto_class,"mItemPartName",0xd74,0x58,0xd64,&v.part_name));
    LID_TAV_CHECK(33,lid_tav_struct(o,v.auto_class,"mDbItem",0x8c,0xd74,0xa60,&v.db_item,&db));
    LID_TAV_CHECK(34,m195_named(o,db,"BrgDbItem"));
    LID_TAV_CHECK(35,lid_tav_string(o,db,"mItemId",0x8c,0,0,&v.db_id));
    LID_TAV_CHECK(36,lid_tav_string(o,db,"mItemType",0x8c,0,0x10,&v.db_type));
    LID_TAV_CHECK(37,lid_tav_string(o,v.auto_class,"mMoneyName",0xd74,0x58,0x60,&v.marker));
#undef LID_TAV_CHECK
    v.current=c;v.queue=q;v.display=d;v.image=image;
    v.copy=(__typeof__(v.copy))(image+lid_tav_profiles[0].rva);v.clear=(__typeof__(v.clear))(image+lid_tav_profiles[1].rva);
    v.build=(__typeof__(v.build))(image+lid_tav_profiles[2].rva);v.assign=(LidMomokaStringAssign)(image+lid_tav_profiles[3].rva);
    v.prepared=1;*h=v;g_lid_tav_prepare_stage=0;return 1;
}
/* No borrowed-array dereference after allocation: resolve roots/header/row
   afresh. Input snapshot pointers are compared only against current headers. */
static int lid_tav_anchor(LidTowerAutoView*h,const LidTowerDisplayView*v,unsigned logical,
                         const LidTSArray*expected,uint64_t auto_info,uint64_t*out){
    LidTowerUiCurrent*c=h->current;const M195Ops*o=&c->meta->memory;LidTowerUiRoots r;LidTSArray p,i,u;
    int32_t original,state,other;uint32_t ended;uint64_t current;
    if(logical>=v->count||!c->ops.live(c->ops.live_context)||!lid_tuc_roots(c,&r)||r.game!=v->roots.game||
       r.manager!=v->roots.manager||r.stage!=v->roots.stage||r.shop!=v->roots.shop||r.item_select!=v->roots.item_select||
       !m195_read(o,r.shop,c->meta->offsets[LID_TUM_STATE],&state,4)||
       !m195_read(o,r.shop,c->meta->offsets[LID_TUM_OTHER],&other,4)||(int64_t)state-other<5||(int64_t)state-other>9||
       !m195_read(o,r.shop,c->meta->offsets[LID_TUM_END],&ended,4)||(ended&c->meta->end_mask)||
       !lid_tuc_array(o,r.shop+c->products,108,36,&p)||!lid_tdv_headers(&p,&v->products)||
       !lid_tuc_array(o,r.shop+c->indices,4,36,&i)||!lid_tdv_headers(&i,&v->indices)||
       !m195_read(o,(uint64_t)(uintptr_t)i.data,logical*4,&original,4)||original!=(int32_t)v->rows[logical].original||
       !lid_tuc_product(o,(uint64_t)(uintptr_t)p.data+logical*108,v->rows[logical].carrier)||
       !lid_tuc_array(o,r.item_select+h->units,0x3e8,36,&u)||logical>=(unsigned)u.count||
       (expected&&!lid_tdv_headers(&u,expected)))return 0;
    uint64_t at=(uint64_t)(uintptr_t)u.data+logical*0x3e8+h->base;
    if(!lid_tuc_literal(o,at+h->entity+h->item_id,v->rows[logical].carrier->item)||
       !m195_read(o,at,h->auto_at,&current,8)||current!=auto_info)return 0;
    *out=at;return 1;
}
static int lid_tav_own(LidTowerAutoView*h,uint64_t object,const LidTowerStockData*d){
    uint64_t cls;return object&&object<=UINT64_MAX-0xd74&&
        m195_read(&h->current->meta->memory,object,0x50,&cls,8)&&cls==h->auto_class&&
        lid_tuc_literal(&h->current->meta->memory,object+h->db_item+h->db_id,d->item);
}
static int lid_tav_text(const M195Ops*o,uint64_t at,const uint16_t*expected,unsigned cap){
    M197StringHeader s;uint16_t text[LID_UI_NAME_CAP];unsigned units=0;
    if(!expected||!cap||cap>LID_UI_NAME_CAP)return 0;while(units<cap&&expected[units])units++;
    if(units==cap||!lid_ml_header(o,at,&s)||s.count!=(int32_t)units+1||
       !m195_read(o,s.data,0,text,(units+1)*2))return 0;
    for(unsigned i=0;i<=units;i++)if(text[i]!=expected[i])return 0;return 1;
}
/* Called ONLY by future admitted BUY list/selection event, never render/Tick.
   Existing native producer false branch constructs a fresh UObject (ordinary
   GC ownership). Attach to reflected unit BEFORE allocating replacement text.
   0 unrelated/wait/busy,1 applied,-1 terminal known-view failure.
   This function is not currently invoked by a published event owner. */
static void lid_tav_marker(LidTowerAutoView*h,uint64_t location,uint16_t out[64]){
    const char prefix[]="LIDAP1:";const char hex[]="0123456789abcdef";unsigned n=0;
    for(unsigned i=0;prefix[i];i++)out[n++]=(uint16_t)prefix[i];
    for(unsigned i=0;i<16;i++){uint8_t b=h->queue->context.nonce[i];out[n++]=hex[b>>4];out[n++]=hex[b&15];}
    out[n++]=':';for(unsigned i=0;i<16;i++)out[n++]=hex[(location>>((15-i)*4))&15];out[n]=0;
}
static int lid_tav_marked(LidTowerAutoView*h,uint64_t object,unsigned offer){
    uint16_t marker[64];if(offer>=184||!lid_tav_own(h,object,&lid_tower_stock_data[offer]))return 0;
    lid_tav_marker(h,lid_tower_stock_data[offer].location,marker);
    return lid_tav_text(&h->current->meta->memory,object+h->marker,marker,64);
}
static int lid_tav_apply(LidTowerAutoView*h,unsigned logical){
    if(!h||!h->prepared||h->stopped||h->busy||h->queue->ops.thread(h->queue->ops.context)!=h->queue->owner){r299_note(12,1,0);return 0;}
    LidTowerDisplayView view;int status=lid_tdv_collect(h->current,h->queue,h->display,&view);
    if(status<0){h->stopped=1;r299_note(12,2,0);return -1;}if(!status||logical>=view.count){r299_note(12,3,logical);return 0;}
    const M195Ops*o=&h->current->meta->memory;LidTSArray units;uint64_t base,old,owned=0;
    uint16_t name[LID_UI_NAME_CAP],recipient[128],game[128],description[260];unsigned n=0;
    if(!lid_tuc_array(o,view.roots.item_select+h->units,0x3e8,36,&units)){r299_note(12,4,logical);return 0;}
    if(logical>=(unsigned)units.count){r299_note(12,5,logical);return 0;}
    if(!m195_read(o,(uint64_t)(uintptr_t)units.data+logical*0x3e8+h->base,h->auto_at,&old,8)){r299_note(12,6,logical);return 0;}
    if(!old){r299_note(12,7,logical);return 0;}
    if(!lid_tav_anchor(h,&view,logical,&units,old,&base)){r299_note(12,8,logical);return 0;}
    uint64_t location=view.rows[logical].location;
    if(!lid_sd_name(h->display,location,name,LID_UI_NAME_CAP)||!lid_sd_text(h->display,location,1,recipient,128)||
       !lid_sd_text(h->display,location,2,game,128)){h->stopped=1;r299_note(12,9,logical);return -1;}
    description[n++]='T';description[n++]='o';description[n++]=':';description[n++]=' ';
    for(unsigned i=0;recipient[i];i++)description[n++]=recipient[i];description[n++]='\n';
    for(unsigned i=0;game[i];i++)description[n++]=game[i];description[n]=0;
    if(lid_tav_marked(h,old,view.rows[logical].offer)&&lid_tav_text(o,old+h->name,name,LID_UI_NAME_CAP)&&
       lid_tav_text(o,old+h->part_name,name,LID_UI_NAME_CAP)&&lid_tav_text(o,old+h->description,description,260)&&
       lid_tav_text(o,old+h->db_item+h->db_type,(const uint16_t*)L"ITTP_MATERIAL",32)&&
       lid_tav_anchor(h,&view,logical,&units,old,&base)){r299_note(12,10,logical);return 1;} /* no repeated native allocation */
    uint8_t temporary[0x174] __attribute__((aligned(16)))={0};
    volatile unsigned normal=0,copied=0;int ok=0;h->busy=1;
    __try{
        r299_note(12,11,logical);h->copy(temporary,(void*)(uintptr_t)base);copied=1;
        /* Local copied pointers only; never clear the original pooled object. */
        r15_zero(temporary+h->auto_at,8);r15_zero(temporary+h->compare_at,8);
        r299_note(12,12,logical);h->build(temporary,0); /* explicitly NON-POOLED native branch */
        __builtin_memcpy(&owned,temporary+h->auto_at,8);
        if(owned==old){r299_note(12,13,logical);goto end;}
        if(!lid_tav_own(h,owned,view.rows[logical].carrier)){r299_note(12,14,logical);goto end;}
        if(!lid_tav_anchor(h,&view,logical,&units,old,&base)){r299_note(12,15,logical);goto end;}
        __builtin_memcpy((void*)(uintptr_t)(base+h->auto_at),&owned,8);
        /* Fresh object now held by actual reflected UI unit. Other aliases to
           the old pooled object stay untouched. No caller C image/object cache. */
        uint16_t marker[64];lid_tav_marker(h,location,marker);
        const uint32_t fields[5]={h->name,h->part_name,h->description,h->db_item+h->db_type,h->marker};
        const uint16_t*texts[5]={name,name,description,(const uint16_t*)L"ITTP_MATERIAL",marker};
        for(unsigned i=0;i<5;i++){
            M197StringHeader before;
            if(!lid_tav_anchor(h,&view,logical,&units,owned,&base)||
               !lid_tav_own(h,owned,view.rows[logical].carrier)||!lid_ml_header(o,owned+fields[i],&before)){r299_note(12,16,i);goto end;}
            h->assign((void*)(uintptr_t)(owned+fields[i]),texts[i]);
        }
        /* mType normalization is PRIVATE PRESENTATION only: native panel name
           otherwise displays generic BLUEPRINT for research-map carriers.
           Unit item type/ID, product and master record remain native values. */
        if(!lid_tav_anchor(h,&view,logical,&units,owned,&base)){r299_note(12,17,logical);goto end;}
        ok=1;r299_note(12,18,logical);
end:normal=1;
    }__finally{
        /* Clear owned native FString copies only on ordinary completion.
           Abnormal native construction: stop/propagate, no speculative native
           cleanup or UObject free/unroot after an uncertain exception. */
        h->stopped=1;volatile unsigned cleaned=0;
        __try{
            if(normal&&copied)h->clear(temporary);cleaned=1;
        }__finally{h->busy=0;}
        if(normal&&ok&&cleaned)h->stopped=0;
    }
    return ok?1:-1;
}
#endif
