#ifndef LID_SHOP_MOMOKA_LABEL_EVENT_H
#define LID_SHOP_MOMOKA_LABEL_EVENT_H
#include "shop_momoka_labels.h"
#include "shop_momoka_description.h"
#include "momoka_owned_slot.h"
/* Exact observed build only, host verifies SHA before preparation. No install
   in this header. Metadata/host remain resident after slot publication. */
typedef struct LidMomokaLabelEvent {
    LidMomokaLabels labels;M207Slot ownership;
    uint64_t function,slot,script;uintptr_t original;
    LidMomokaStringAssign assign;
    void*context;uint32_t owner;
    uint32_t(*thread)(void*);uint32_t(*error_get)(void*);
    void(*error_set)(void*,uint32_t);int(*live)(void*);
    unsigned prepared,active,busy,stopped;int last_result;
    LidMomokaDescription description;
    void*visual_context;void(*visual_after)(void*);
} LidMomokaLabelEvent;
static int lid_mle_member(const M195Ops*o,uint64_t owner,const char*name,uint64_t*out){
    uint64_t child,next,found=0,seen[1024];unsigned n=0;
    if(!out||!m195_read(o,owner,0x80,&child,8))return 0;
    while(child){
        if(n==1024)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==child)return 0;seen[n++]=child;
        if(m195_named(o,child,name)){if(found)return 0;found=child;}
        if(!m195_read(o,child,0x60,&next,8))return 0;child=next;
    }
    if(!found)return 0;*out=found;return 1;
}
static int lid_mle_script(const M195Ops*o,uint64_t explain,uint64_t*out){
    uint64_t f,cls,p,next,flags,seen[128];uint32_t function_flags,extent,at[2]={0,0};
    uint16_t parms,ret;unsigned found=0,n=0;
    if(!out||!m212_member(o,explain,"BrgUIMenu_SkillExplane","SetSkillId",&f)||
       !m195_read(o,f,0x50,&cls,8)||!m195_named(o,cls,"Function")||
       !m195_read(o,f,0xd0,&function_flags,4)||(function_flags&0x400)||
       !m195_read(o,f,0x88,&extent,4)||extent>65536||
       !m195_read(o,f,0xe2,&parms,2)||parms<20||parms>extent||
       !m195_read(o,f,0xe4,&ret,2)||!m195_read(o,f,0x80,&p,8))return 0;
    while(p){
        if(n==128)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==p)return 0;seen[n++]=p;
        if(!m195_read(o,p,0x60,&next,8)||!m195_read(o,p,0x70,&flags,8))return 0;
        if(flags&0x80){
            unsigned i=m195_named(o,p,"SkillID")?0:m195_named(o,p,"ReturnValue")?1:2;
            uint32_t mask;
            if(i==2||(found&(1u<<i))||((flags&0x400)!=0)!=(i==1)||
               (!i&&(flags&0x100))||
               !lid_ml_property(o,p,i?"BoolProperty":"StrProperty",1,i?4:16,parms,0,&at[i])||
               (i&&(!m195_read(o,p,0xb0,&mask,4)||mask!=1)))return 0;
            found|=1u<<i;
        }p=next;
    }
    if(found!=3||ret!=at[1]||(at[0]<at[1]+4&&at[1]<at[0]+16))return 0;
    *out=f;return 1;
}
static int lid_mle_native(const M195Ops*o,uint64_t image,uint64_t*out,uint64_t*slot){
    uint64_t owner,f,cls,object_flags,p,next,pf,structure;uint32_t flags,extent,size,at;
    uint16_t index,parms;unsigned count=0,found=0;uint64_t seen[128],original;
    if(!out||!slot||!image||image>UINT64_MAX-0xfd1dd08||
       !m195_read(o,image,0xfd1dd00,&owner,8)||!m195_named(o,owner,"BrgUI_Utility")||
       !m195_read(o,owner,0x50,&cls,8)||!m195_named(o,cls,"Class")||
       !m195_read(o,owner,0x10,&object_flags,8)||!(object_flags&0x4000)||
       !lid_mle_member(o,owner,"StartChangeEffect",&f)||
       !m195_read(o,f,0x50,&cls,8)||!m195_named(o,cls,"Function")||
       !m195_read(o,f,0x10,&object_flags,8)||!(object_flags&0x4000)||
       !m195_read(o,f,0xd0,&flags,4)||!(flags&0x400)||
       !m195_read(o,f,0xd4,&index,2)||index||
       !m195_read(o,f,0xe2,&parms,2)||!parms||parms>352||
       !m195_read(o,f,0x88,&extent,4)||extent<parms||extent>65536||
       !m195_read(o,f,0xf0,&original,8)||original!=image+0x10483c0||
       f>UINT64_MAX-0xf0||((f+0xf0)&7)||!m195_read(o,f,0x80,&p,8))return 0;
    while(p){
        if(count==128)return 0;for(unsigned i=0;i<count;i++)if(seen[i]==p)return 0;seen[count++]=p;
        if(!m195_read(o,p,0x60,&next,8)||!m195_read(o,p,0x70,&pf,8))return 0;
        if(pf&0x80){
            if(found++||!m195_named(o,p,"Info")||(pf&0x180)!=0x180||(pf&0x400)||
               !m195_read(o,p,0xb0,&structure,8)||!m195_named(o,structure,"BrgUI_ChangeEffectInfo")||
               !m195_read(o,structure,0x50,&cls,8)||!m195_named(o,cls,"ScriptStruct")||
               !m195_read(o,structure,0x88,&size,4)||!size||size>352||
               !lid_ml_property(o,p,"StructProperty",1,size,parms,0,&at))return 0;
        }p=next;
    }
    if(found!=1)return 0;*out=f;*slot=f+0xf0;return 1;
}
static int lid_mle_prefix(const M195Ops*o,uint64_t image){
    static const uint8_t exec[24]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x81,0xec,0x90,0x01,0,0,0x48,0x8b,0x05,0x27,0x63,0x7b};
    static const uint8_t assign[24]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x09,0x48,0x8b,0xfa,0x48,0x3b,0xca,0x74,0x53};
    uint8_t actual[24];
    if(!m195_read(o,image,0x10483c0,actual,24))return 0;
    for(unsigned i=0;i<24;i++)if(actual[i]!=exec[i])return 0;
    if(!m195_read(o,image,0x19c40,actual,24))return 0;
    for(unsigned i=0;i<24;i++)if(actual[i]!=assign[i])return 0;return 1;
}
/* One UI-owner cold diagnostic; no game object or frame retained. */
typedef struct LidMomokaPrepareStatus {unsigned stage,member,waiting;} LidMomokaPrepareStatus;
static LidMomokaPrepareStatus g_lid_mle_prepare_status;
static int lid_mle_prepare(LidMomokaLabelEvent*h,uint64_t image,LidShopMomokaRoot*root,
    const LidShopDisplay*display,uint64_t menu_class,uint64_t database_class){
    LidMomokaPrepareStatus*s=&g_lid_mle_prepare_status;*s=(LidMomokaPrepareStatus){1,0,0};uint64_t explain;
    if(!h||h->prepared||!h->thread||!h->error_get||!h->error_set||!h->live||!h->owner||h->thread(h->context)!=h->owner||!root)return 0;
    if(!h->live(h->context)){s->waiting=1;return 0;}
    s->stage=2;if(!lid_mle_prefix(&root->memory,image))return 0;
    s->stage=3;
    if(!h->labels.ready&&!lid_ml_prepare_probe(&h->labels,root,display,menu_class,database_class,&s->member,&s->waiting))return 0;
    s->stage=4;if(!lid_ml_current(&h->labels,&explain)){s->waiting=1;return 0;}
    s->stage=5;if(!lid_md_prepare(&h->description,&h->labels,image))return 0;
    s->stage=6;if(!lid_mle_script(&root->memory,explain,&h->script))return 0;
    s->stage=7;if(!lid_mle_native(&root->memory,image,&h->function,&h->slot))return 0;
    h->original=(uintptr_t)image+0x10483c0;h->assign=(LidMomokaStringAssign)(uintptr_t)(image+0x19c40);
    h->prepared=1;s->stage=0;return 1;
}
/* A tiny numeric filter before rooted traversal; only StartChangeEffect events,
   never TickChangeEffect/per-frame. No callback stores borrowed object/frame. */
static int lid_mle_scope(LidMomokaLabelEvent*h,void*frame){
    uint64_t node,object,explain;const M195Ops*o=&h->labels.root->memory;
    return m195_read(o,(uint64_t)(uintptr_t)frame,0x14,&node,8)&&node==h->script&&
        m195_read(o,(uint64_t)(uintptr_t)frame,0x1c,&object,8)&&
        lid_ml_current(&h->labels,&explain)&&object==explain;
}
static int lid_mle_refresh(LidMomokaLabelEvent*h){
    int names=lid_ml_apply(&h->labels,h->assign);if(names<0)return -1;
    int description=lid_md_apply(&h->description,h->assign);
    if(description>=0&&h->visual_after)h->visual_after(h->visual_context);
    return description<0?-1:names+description;
}
static void lid_mle_call(LidMomokaLabelEvent*h,void*receiver,void*frame,void*result){
    /* Only a fully prepared resident host may be published by the runtime. */
    ((M204Exec)h->original)(receiver,frame,result);
    uint32_t error=h->error_get(h->context);
    if(__atomic_load_n(&h->active,__ATOMIC_ACQUIRE)&&h->thread(h->context)==h->owner&&
       !h->busy&&!h->stopped&&h->live(h->context)&&lid_mle_scope(h,frame)){
        volatile unsigned completed=0;h->busy=1;
#if LID_SHOP_EXEC_SEH_ENABLED
        __try {
#endif
            h->last_result=lid_mle_refresh(h);completed=1;
            if(h->last_result<0)h->stopped=1; /* display only; no payment/transport halt */
#if LID_SHOP_EXEC_SEH_ENABLED
        } __finally {
#endif
            if(!completed)h->stopped=1;h->busy=0;h->error_set(h->context,error);
#if LID_SHOP_EXEC_SEH_ENABLED
        }
#endif
    }
    h->error_set(h->context,error);
}
#endif
