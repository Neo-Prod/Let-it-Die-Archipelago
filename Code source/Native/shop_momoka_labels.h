#ifndef LID_SHOP_MOMOKA_LABELS_H
#define LID_SHOP_MOMOKA_LABELS_H
#include "shop_momoka_current_root.h"
#include "momoka_skill_snapshot.h"
#include "shop_display.h"
/* Source-only consumer, NOT installed. No cached UObject instances. Prepare
   under current metadata lifetime; apply synchronously on the owning UI thread.
   The host must separately prove exact image/prefix/allocator admission and
   supply native FString assignment (RVA19970 for the observed executable).
   No global GetText hook, ProcessEvent, menu reset or script-array mutation. */
typedef struct LidMomokaLabels {
    LidShopMomokaRoot*root;const LidShopDisplay*display;
    uint64_t menu_class,database_class,list_class,explain_class;
    uint32_t top,list,list_mode,explain,explain_mode,info,stride,skill,name;
    unsigned ready;
} LidMomokaLabels;
typedef void*(*LidMomokaStringAssign)(void*,const uint16_t*);
/* Exact typed property, allowing static arrays only when explicitly requested.
   Reflection metadata offsets derive from the existing M195/M205 readers. */
static int lid_ml_property(const M195Ops*o,uint64_t property,const char*type,
    uint32_t dimension,uint32_t size,uint32_t extent,uint32_t minimum,uint32_t*offset){
    uint64_t cls;uint32_t dim,actual,at;
    if(!offset||!dimension||!size||size>extent/dimension||
       !m195_read(o,property,0x50,&cls,8)||!m195_named(o,cls,type)||
       !m195_read(o,property,0x68,&dim,4)||dim!=dimension||
       !m195_read(o,property,0x6c,&actual,4)||actual!=size||
       !m195_read(o,property,0x94,&at,4)||at<minimum||at>extent-size*dimension)return 0;
    *offset=at;return 1;
}
/* ScriptStruct members are read directly, not via structure.Class.Children. */
static int lid_ml_struct_member(const M195Ops*o,uint64_t structure,const char*name,uint64_t*out){
    uint64_t child,next,seen[128],found=0;unsigned count=0;
    if(!out||!m195_read(o,structure,0x80,&child,8))return 0;
    while(child){
        if(count==128)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==child)return 0;seen[count++]=child;
        if(m195_named(o,child,name)){if(found)return 0;found=child;}
        if(!m195_read(o,child,0x60,&next,8))return 0;child=next;
    }
    if(!found)return 0;*out=found;return 1;
}
static int lid_ml_field(const M195Ops*o,uint64_t object,const char*owner,const char*name,
    const char*type,uint32_t size,uint32_t*offset){
    uint64_t p;return m212_member(o,object,owner,name,&p)&&
        lid_ml_property(o,p,type,1,size,65536,0x58,offset);
}
/* Cold metadata only. Absent child objects are WAIT, not invalid schema. */
static int lid_ml_prepare_probe(LidMomokaLabels*out,LidShopMomokaRoot*root,
    const LidShopDisplay*display,uint64_t menu_class,uint64_t database_class,unsigned*stage,unsigned*waiting){
    if(!stage||!waiting)return 0;*stage=1;*waiting=0;
    LidMomokaLabels b={0};uint64_t db,menu,list,explain,p,structure,cls,member;uint32_t extent;
    const M195Ops*o=root?&root->memory:0;
#define LID_ML_PROBE(n,condition) do{*stage=(n);if(!(condition))return 0;}while(0)
    LID_ML_PROBE(1,out&&!out->ready&&root&&display);
    *stage=2;if(!lid_mr_current(root,menu_class,database_class,&db,&menu)){*waiting=1;return 0;}
    LID_ML_PROBE(3,lid_ml_field(o,menu,"BrgUIMenu_SkillExchange","mTopMenu","ByteProperty",1,&b.top));
    LID_ML_PROBE(4,lid_ml_field(o,menu,"BrgUIMenu_SkillExchange","mSelectSkillList","ObjectProperty",8,&b.list));
    LID_ML_PROBE(5,m195_read(o,menu,b.list,&list,8));if(!list){*waiting=1;return 0;}
    LID_ML_PROBE(6,m195_read(o,list,0x50,&b.list_class,8)&&m195_named(o,b.list_class,"BrgUIMenu_SkillExchange_SkillList"));
    LID_ML_PROBE(7,lid_ml_field(o,list,"BrgUIMenu_SkillExchange_SkillList","mMode","ByteProperty",1,&b.list_mode));
    LID_ML_PROBE(8,lid_ml_field(o,list,"BrgUIMenu_SkillExchange_SkillList","mSkillExplane","ObjectProperty",8,&b.explain));
    LID_ML_PROBE(9,m195_read(o,list,b.explain,&explain,8));if(!explain){*waiting=1;return 0;}
    LID_ML_PROBE(10,m195_read(o,explain,0x50,&b.explain_class,8)&&m195_named(o,b.explain_class,"BrgUIMenu_SkillExplane"));
    LID_ML_PROBE(11,lid_ml_field(o,explain,"BrgUIMenu_SkillExplane","mMode","ByteProperty",1,&b.explain_mode));
    LID_ML_PROBE(12,m212_member(o,explain,"BrgUIMenu_SkillExplane","mSkillInfo",&p));
    LID_ML_PROBE(13,m195_read(o,p,0xb0,&structure,8)&&m195_named(o,structure,"BrgUIMenu_SkillExplane_SkillInfo"));
    LID_ML_PROBE(14,m195_read(o,structure,0x50,&cls,8)&&m195_named(o,cls,"ScriptStruct"));
    LID_ML_PROBE(15,m195_read(o,structure,0x88,&extent,4)&&extent>=32&&extent<=32768);
    LID_ML_PROBE(16,lid_ml_property(o,p,"StructProperty",2,extent,65536,0x58,&b.info));
    LID_ML_PROBE(17,lid_ml_struct_member(o,structure,"mSkillId",&member));
    LID_ML_PROBE(18,lid_ml_property(o,member,"StrProperty",1,16,extent,0,&b.skill));
    LID_ML_PROBE(19,lid_ml_struct_member(o,structure,"mExplSkillName",&member));
    LID_ML_PROBE(20,lid_ml_property(o,member,"StrProperty",1,16,extent,0,&b.name));
    LID_ML_PROBE(21,!(b.skill<b.name+16&&b.name<b.skill+16));
#undef LID_ML_PROBE
    b.root=root;b.display=display;b.menu_class=menu_class;b.database_class=database_class;
    b.stride=extent;b.ready=1;*out=b;*stage=0;return 1;
}
static int lid_ml_prepare(LidMomokaLabels*out,LidShopMomokaRoot*root,
    const LidShopDisplay*display,uint64_t menu_class,uint64_t database_class){
    unsigned stage,waiting;return lid_ml_prepare_probe(out,root,display,menu_class,database_class,&stage,&waiting);
}
static int lid_ml_current(LidMomokaLabels*b,uint64_t*out){
    uint64_t db,menu,list,explain,cls;uint8_t mode;
    const M195Ops*o=b&&b->root?&b->root->memory:0;
    if(!b||!b->ready||!out||!lid_mr_current(b->root,b->menu_class,b->database_class,&db,&menu)||
       !m195_read(o,menu,b->top,&mode,1)||mode||
       !m195_read(o,menu,b->list,&list,8)||!m195_read(o,list,0x50,&cls,8)||cls!=b->list_class||
       !m195_read(o,list,b->list_mode,&mode,1)||mode||
       !m195_read(o,list,b->explain,&explain,8)||!m195_read(o,explain,0x50,&cls,8)||cls!=b->explain_class||
       !m195_read(o,explain,b->explain_mode,&mode,1)||mode)return 0;
    *out=explain;return 1;
}
static int lid_ml_header(const M195Ops*o,uint64_t address,M197StringHeader*out){
    M197StringHeader h;
    if(!out||!m195_read(o,address,0,&h,16)||h.count<0||h.capacity<h.count||h.capacity>1048576||
       (h.capacity&&!h.data)||(!h.capacity&&h.data)||h.count>4096)return 0;
    if(h.count){uint16_t end;if(!m195_read(o,h.data,(unsigned)(h.count-1)*2,&end,2)||end)return 0;}
    *out=h;return 1;
}
/* Explicit bounded call, not a periodic poll. Host supplies one validated
   UI-event boundary. Returns number changed, or -1 on admission/read failure.
   Both animation records map independently. All transient pointers are local.
   Runtime integration/event admission is deliberately still absent. */
static int lid_ml_apply(LidMomokaLabels*b,LidMomokaStringAssign assign){
    uint64_t explain;int changed=0;
    if(!b||!assign||!b->display||!b->display->io.thread_id||
       b->display->io.thread_id()!=b->display->ui||
       __atomic_load_n(&b->display->state,__ATOMIC_ACQUIRE)!=LID_SD_READY||
       !lid_ml_current(b,&explain))return -1;
    const M195Ops*o=&b->root->memory;const LidShopRuntimeCatalogue*c=b->display->catalogue;
    if(!c||!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE))return -1;
    for(unsigned i=0;i<2;i++){
        uint64_t at,again;uint16_t id[128],label[LID_UI_NAME_CAP],old[LID_UI_NAME_CAP];unsigned units;
        M195Signature s={0};M197StringHeader before,after;
        if(!lid_ml_current(b,&again)||again!=explain)return -1;
        if(explain>UINT64_MAX-b->info-(uint64_t)i*b->stride)return -1;
        at=explain+b->info+(uint64_t)i*b->stride;
        s.skill_offset=b->skill;s.parms_size=(uint16_t)b->stride;
        /* Empty unused buffer is ordinary; malformed nonempty ID is refused. */
        M197StringHeader id_header;
        if(!m195_read(o,at,b->skill,&id_header,16))return -1;
        if(!id_header.count)continue;
        if(!m197_string(o,at,&s,id,&units))return -1;
        unsigned offer=UINT32_MAX;
        for(unsigned j=0;j<c->momoka_count;j++){
            const LidShopUiSku*sku=&c->momoka[j];
            if(sku->units!=units)continue;
            unsigned k=0;while(k<units&&sku->skill[k]==id[k])k++;
            if(k==units){offer=sku->offer;break;}
        }
        if(offer==UINT32_MAX)continue; /* foreign/native carrier untouched */
        if(!lid_sd_name(b->display,7500001ull+offer,label,LID_UI_NAME_CAP)||at>UINT64_MAX-b->name||
           !lid_ml_header(o,at+b->name,&before))return -1;
        unsigned n=0;while(label[n])n++;
        if(before.count==(int32_t)n+1&&m195_read(o,before.data,0,old,(n+1)*2)){
            unsigned k=0;while(k<=n&&old[k]==label[k])k++;
            if(k==n+1)continue; /* no realloc for an already matching name */
        }
        /* Recheck root, carrier and destination after all reads, before alloc.
           This is not a GC lock: UI-host lifetime admission remains mandatory. */
        uint16_t id_after[128];unsigned units_after;
        if(!lid_ml_current(b,&again)||again!=explain||
           !m197_string(o,at,&s,id_after,&units_after)||units_after!=units||
           !lid_ml_header(o,at+b->name,&after)||after.data!=before.data||
           after.count!=before.count||after.capacity!=before.capacity)return -1;
        for(unsigned k=0;k<units;k++)if(id[k]!=id_after[k])return -1;
        assign((void*)(uintptr_t)(at+b->name),label);changed++;
    }
    return changed;
}
#endif
