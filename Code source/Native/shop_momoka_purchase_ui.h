#ifndef LID_SHOP_MOMOKA_PURCHASE_UI_H
#define LID_SHOP_MOMOKA_PURCHASE_UI_H
#include "shop_momoka_price_binding.h"
#include "shop_purchase_bridge.h"
#include "shop_momoka_price_collector.h"
#include "shop_momoka_current_root.h"
/* Compose existing verified post-original proof with real queue reservation.
   No hook install or native result store here. Runtime must prove own getter,
   same UI thread/menu/lifetime, writable four-byte return and explicit bound
   catalogue before applying FALSE decisions. NEVER give SKU/AP reward here.
   Not a general GetSkill override; native/foreign offers stay untouched. */
enum {LID_PUI_NATIVE=0,LID_PUI_ALLOW=1,LID_PUI_FALSE_PRECHARGE=2,LID_PUI_FALSE_ADD=3,LID_PUI_UNCERTAIN=4};
typedef struct LidShopUiSku {uint16_t skill[129];unsigned units,offer;} LidShopUiSku;
typedef struct LidShopPurchaseUi {
    unsigned initialized,count;const LidMPPriceBinding*binding;
    LidShopPurchaseBridge*bridge;const LidShopUiSku*rows;
    LidShopMomokaRoot*root; /* Set before exec preparation/publication, UI-only. */
    void*admission_context;int(*before_charge)(void*,unsigned);
} LidShopPurchaseUi;
static int lid_pui_prepare(LidShopPurchaseUi*u,const LidMPPriceBinding*binding,
    LidShopPurchaseBridge*bridge,const LidShopUiSku*rows,unsigned count){
    if(!u||u->initialized||!binding||!binding->ready||!bridge||!bridge->initialized||
       !rows||!count||count>4096||bridge->thread(bridge->provider)!=bridge->ui)return 0;
    uint8_t allowed[512]={0};
    for(unsigned i=0;i<count;i++){
        if(!lid_mp_id(rows[i].skill,rows[i].units)||rows[i].offer>=4096||
           !lid_pb_bit(bridge->allowed,rows[i].offer)||lid_pb_bit(allowed,rows[i].offer))return 0;
        for(unsigned j=0;j<i;j++)if(lid_mp_equal(rows[i].skill,rows[i].units,rows[j].skill,rows[j].units))return 0;
        allowed[rows[i].offer/8]|=(uint8_t)(1u<<(rows[i].offer%8));
    }
    if(!r15_equal(allowed,bridge->allowed,512))return 0;
    u->binding=binding;u->bridge=bridge;u->rows=rows;u->count=count;u->initialized=1;return 1;
}
/* AFTER real GetSkill, BEFORE caller script resumes. Host owns read scope.
   Unknown catalogue SKU returns NATIVE. Known AP before-charge failures must
   return false to BuySticker; matched ADD must never add its owned decal even
   on uncertainty. Host must log/block the shop on UNCERTAIN, not silently grant.
   Native result itself is NEVER changed by this helper. */
static unsigned lid_pui_after(LidShopPurchaseUi*u,const M195Ops*memory,
    uint64_t frame,uint64_t getter_result,uint64_t*reserved){
    M197SkillSnapshot s={0};M196Context context={0};LidMPPriceObservation price={0};unsigned offer=4096;
    if(!u||!u->initialized||!memory||!reserved||
       u->bridge->thread(u->bridge->provider)!=u->bridge->ui||
       !m196_context(memory,&u->binding->scripts,frame,&context))return LID_PUI_NATIVE;
    if(context.kind!=M196_PRICE&&context.kind!=M196_BUY_ADD)return LID_PUI_NATIVE;
    /* The native halted path cannot debit even an unknown later-unlocked KC
       SKU. Block this exact BuySticker/add scope too, before its UI cache can
       gain an unpaid vanilla decal. Other getter/sell/gacha scopes passed the
       context filter above and remain original. */
    if(__atomic_load_n(&u->bridge->halted,__ATOMIC_ACQUIRE))
        return context.kind==M196_PRICE?LID_PUI_FALSE_PRECHARGE:LID_PUI_UNCERTAIN;
    if(!m197_capture(memory,&u->binding->scripts,frame,&s))
        return context.kind==M196_PRICE?LID_PUI_FALSE_PRECHARGE:LID_PUI_UNCERTAIN;
    if(s.context.kind!=M196_PRICE&&s.context.kind!=M196_BUY_ADD)return LID_PUI_NATIVE;
    for(unsigned i=0;i<u->count;i++)if(lid_mp_equal(s.skill,s.length,u->rows[i].skill,u->rows[i].units)){
        offer=u->rows[i].offer;break;
    }
    if(offer==4096)return LID_PUI_NATIVE;
    /* A graph can be published before scouted names/images are available.
       Require exact host readiness BEFORE creating any reservation. This is
       deliberately not used for ADD: an already paid UI purchase must finish
       confirmation/suppression even if display readiness has since changed. */
    if(s.context.kind==M196_PRICE&&
       (!u->before_charge||u->before_charge(u->admission_context,offer)!=1))
        return LID_PUI_FALSE_PRECHARGE;
    if(!lid_mp_price_post(memory,u->binding,frame,getter_result,offer,&price)){
        if(s.context.kind==M196_PRICE)return LID_PUI_FALSE_PRECHARGE;
        (void)lid_pb_uncertain_ui(u->bridge,offer);return LID_PUI_UNCERTAIN;
    }
    if(price.boundary==LID_MPB_BEFORE_UI_CHARGE){
        uint64_t token=0;
        if(!lid_pb_reserve(u->bridge,&price.row,frame,&token))return LID_PUI_FALSE_PRECHARGE;
        *reserved=token;return LID_PUI_ALLOW;
    }
    if(price.boundary==LID_MPB_AFTER_UI_CHARGE&&
       lid_pb_confirm_ui(u->bridge,&price.row,s.context.parent))return LID_PUI_FALSE_ADD;
    (void)lid_pb_uncertain_ui(u->bridge,offer);
    return LID_PUI_UNCERTAIN;
}
/* Actual execGetSkill wrapper composition, not published automatically.
   Metadata must be fully prepared at final address BEFORE slot publication;
   original/API pointers stay resident even after owner/session retirement.
   Normal original return guarantees result was a writable DWORD in this ABI.
   Only proven AP decisions may store zero; no price/skill/owned-array writes,
   refund, ReceivedItems fabrication or entry/delivery invocation. */
static void lid_pui_exec(LidShopPurchaseUi*u,const LidMPPriceCollector*metadata,
                         void*receiver,void*frame,void*result){
    if(!metadata||!metadata->windows||!metadata->windows->prepared.native.original)return;
    M213Adapter*w=metadata->windows;
    M204Exec original=(M204Exec)(uintptr_t)w->prepared.native.original;
    original(receiver,frame,result); /* exactly once; no read/queue guard held */
    if(!w->api.get_error||!w->api.set_error)return;
    uint32_t error=w->api.get_error();uint64_t slot,node,database,menu,object,cls;uint64_t token=0;
    if(u&&u->initialized&&u->binding==&metadata->binding&&metadata->ready&&
       w->api.thread&&w->api.thread()==metadata->owner&&metadata->owner==u->bridge->ui&&
       m195_read(&metadata->memory,(uint64_t)(uintptr_t)frame,0x14,&node,8)&&
       (node==metadata->binding.buy.function||node==metadata->binding.add.function)&&
       metadata->live(metadata->run_context)&&
       m195_read(&metadata->memory,w->prepared.native.slot,0,&slot,8)&&slot==metadata->wrapper&&
       m195_read(&metadata->memory,(uint64_t)(uintptr_t)frame,0x1c,&object,8)&&
       m195_read(&metadata->memory,object,0x50,&cls,8)&&cls==metadata->binding.scripts.menu_class){
        /* Do not mutate resident metadata or retain the previous UI instance.
           Parent ADD checks must also refer to this freshly rooted menu. */
        LidMPPriceBinding binding=metadata->binding;
        LidShopPurchaseUi current=*u;
        binding.scripts.menu=object;current.binding=&binding;
        M196Context context={0};unsigned decision=LID_PUI_NATIVE;
        if(m196_context(&metadata->memory,&binding.scripts,(uint64_t)(uintptr_t)frame,&context)&&
           (context.kind==M196_PRICE||context.kind==M196_BUY_ADD)){
            if(u->root&&lid_mr_current(u->root,binding.scripts.menu_class,
                metadata->database_class,&database,&menu)&&object==menu&&
                (uint64_t)(uintptr_t)receiver==database)
                decision=lid_pui_after(&current,&metadata->memory,(uint64_t)(uintptr_t)frame,
                                      (uint64_t)(uintptr_t)result,&token);
            else{
                /* Proven buy scope, but no longer the rooted active menu:
                   never fall back to an unpaid local owned-decal addition. */
                decision=context.kind==M196_PRICE?LID_PUI_FALSE_PRECHARGE:LID_PUI_UNCERTAIN;
                if(context.kind==M196_BUY_ADD)
                    __atomic_store_n(&u->bridge->halted,1,__ATOMIC_RELEASE);
            }
        }
        if(decision==LID_PUI_FALSE_PRECHARGE||decision==LID_PUI_FALSE_ADD||decision==LID_PUI_UNCERTAIN)
            *(uint32_t*)result=0;
    }
    w->api.set_error(error);
}
#endif
