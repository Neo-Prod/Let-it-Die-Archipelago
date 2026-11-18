#ifndef LID_SHOP_MOMOKA_GATE_H
#define LID_SHOP_MOMOKA_GATE_H
#include "shop_momoka_purchase_exec.h"
#include "shop_runtime_policy.h"
/* Persistent guarded entry for the EXISTING M214/M207 owner. Not installed
   automatically. Prepare before m214_activate publishes the runtime wrapper;
   keep all storage resident. No diagnostic timeout/second getter publication.
   Wrapper calls lid_sg_exec with this resident context. */
typedef struct LidShopMomokaGate {
    unsigned initialized;uint32_t owner;M213Adapter*windows;M207Slot*entry;
    const LidShopPolicy*policy;LidShopPurchaseExec*execution;
    M195Ops memory;M196Binding scripts;uint64_t wrapper;
} LidShopMomokaGate;
static int lid_sg_prepare(LidShopMomokaGate*g,M213Adapter*w,M207Slot*entry,
                          const LidShopPolicy*policy,uint32_t owner,uint64_t wrapper){
    if(!g||g->initialized||!w||!entry||!policy||!owner||!wrapper||
       !w->api.thread||w->api.thread()!=owner||w->prepared.gate.owner!=owner||
       !w->api.get_error||!w->api.set_error||!w->prepared.native.original||
       !w->prepared.native.slot||wrapper==w->prepared.native.original||
       __atomic_load_n(&entry->state,__ATOMIC_ACQUIRE)!=M207_EMPTY)return 0;
    g->windows=w;g->entry=entry;g->policy=policy;g->owner=owner;g->wrapper=wrapper;
    g->memory=(M195Ops){w,m213_copy,m213_name};g->initialized=1;return 1;
}
/* UI owner attaches complete prepared graph before coherent native activation.
   Attached does NOT mean published: gate continues blocking until published.
   No copying/replacing the M207 owner, and no wrapper-changing CAS. */
static int lid_sg_attach(LidShopMomokaGate*g,LidShopPurchaseExec*h){
    LidShopPurchaseExec*empty=0;
    if(!g||!g->initialized||!h||!h->initialized||h->retired||
       g->windows->api.thread()!=g->owner||lid_sm_mode(g->policy)!=LID_SM_ENABLED||
       !h->metadata||h->metadata->entry!=g->entry||h->metadata->windows!=g->windows||
       h->metadata->wrapper!=g->wrapper||h->metadata->owner!=g->owner||
       __atomic_load_n(&h->published,__ATOMIC_ACQUIRE)||
       !lid_mpc_owned(&g->memory,g->windows,g->entry,g->wrapper))return 0;
    if(!__atomic_compare_exchange_n(&g->execution,&empty,h,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    h->entry_gate=&g->execution;return 1;
}
static void lid_sg_exec(LidShopMomokaGate*g,void*receiver,void*frame,void*result){
    if(!g||!g->initialized)return; /* unreachable after valid publication */
    M213Adapter*w=g->windows;
    LidShopPurchaseExec*h=__atomic_load_n(&g->execution,__ATOMIC_ACQUIRE);
    unsigned mode=lid_sm_mode(g->policy);
    uint32_t incoming=w->api.get_error();
    int active=w->api.thread()==g->owner&&mode==LID_SM_ENABLED&&h&&__atomic_load_n(&h->published,__ATOMIC_ACQUIRE)&&
        !h->retired&&!__atomic_load_n(&h->poisoned,__ATOMIC_ACQUIRE)&&
        h->metadata->live(h->metadata->run_context)&&h->bridge->live(h->bridge->provider);
    w->api.set_error(incoming);
    if(active){
        lid_pui_exec(h->ui,h->metadata,receiver,frame,result);return;
    }
    ((M204Exec)(uintptr_t)w->prepared.native.original)(receiver,frame,result);
    uint32_t error=w->api.get_error();
    /* Explicit false policy is vanilla. Unknown/enabled/failed are NOT false:
       exact KC buy boundaries remain blocked until the full path is ready. */
    if(mode!=LID_SM_DISABLED&&w->api.thread()==g->owner&&result&&
       lid_mpc_owned(&g->memory,w,g->entry,g->wrapper)){
        uint64_t node,object,cls;uint64_t f=(uintptr_t)frame;
        if(m195_read(&g->memory,f,0x14,&node,8)&&
           (g->scripts.menu_class?(node==g->scripts.buy.function||node==g->scripts.add.function):
             (m195_named(&g->memory,node,"BuySticker")||m195_named(&g->memory,node,"AddHaveSkillList")))&&
           m195_read(&g->memory,f,0x1c,&object,8)&&
           m195_read(&g->memory,object,0x50,&cls,8)&&
           (g->scripts.menu_class==cls||m195_named(&g->memory,cls,"BrgUIMenu_SkillExchange"))){
            if(g->scripts.menu_class!=cls){
                M196Binding scripts={0};
                if(m196_bind(&g->memory,object,&scripts)){scripts.menu=0;g->scripts=scripts;}
            }
            int block=0;
            if(g->scripts.menu_class==cls){
                M196Binding binding=g->scripts;M196Context context={0};binding.menu=object;
                /* BuySticker's getter is precharge even if its locals cannot
                   be read. ADD requires exact same-object BuySticker parent;
                   sell/exchange/static MakeHaveSkillList remain native. */
                block=node==binding.buy.function;
                if(!block&&m196_context(&g->memory,&binding,f,&context))block=context.kind==M196_BUY_ADD;
            }
            /* Metadata/local-read failure must not authorize the first buy.
               This fallback still requires exact private buy name/class, or
               same-object ADD's private buy parent; never all ADD contexts. */
            if(!block&&m195_named(&g->memory,node,"BuySticker"))block=1;
            if(!block&&m195_named(&g->memory,node,"AddHaveSkillList")){
                uint64_t parent,parent_object,parent_node;
                block=m195_read(&g->memory,f,0x34,&parent,8)&&parent&&parent!=f&&
                    m195_read(&g->memory,parent,0x1c,&parent_object,8)&&parent_object==object&&
                    m195_read(&g->memory,parent,0x14,&parent_node,8)&&
                    m195_named(&g->memory,parent_node,"BuySticker");
            }
            if(block)*(uint32_t*)result=0;
        }
    }
    w->api.set_error(error);
}
#endif
