#ifndef LID_KILL_COIN_PICKUP_RUNTIME_R146_H
#define LID_KILL_COIN_PICKUP_RUNTIME_R146_H
#include "kill_coin_pickup_binding.h"
/* Included after R144Wallet/readiness helpers. Synchronous native receiver:
   Spawn -> original pickup events -> Destroy, without Coin.Initialize/meshes.
   External AP income enters the matching native authoritative carried model;
   existing starthub code alone remains responsible for the eventual deposit. */
typedef struct R146Cache {
 uint64_t game_class,ui_class,pawn_class,coin_class;uint32_t game_ui,ui_data,ui_pawn,ui_common;
 R146Signature spawn,destroy,get_money,message,event;unsigned valid;
} R146Cache;
typedef struct R146View {uint64_t ui,pawn,common,coin_class;uint32_t uid;R146Authority authority;} R146View;
static R146Cache g_coin_pickup_cache;
static int r146_profile(BYTE*image){
 static const BYTE event_prefix[17]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x19,0x89,0x54,0x24,0x38};
 static const BYTE income_prefix[15]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xda,0x48,0x8b,0xf9};
 return image&&readable_range(image+0xf8a270,17)&&readable_range(image+0xf8a050,17)&&
 readable_range(image+0x1368c60,15)&&!lid_memcmp(image+0xf8a270,event_prefix,17)&&
 !lid_memcmp(image+0xf8a050,event_prefix,17)&&!lid_memcmp(image+0x1368c60,income_prefix,15)&&
 executable_ptr(image,image+0xf8a270)&&executable_ptr(image,image+0xf8a050)&&
 executable_ptr(image,image+0xf8a510)&&executable_ptr(image,image+0x1368c60)&&executable_ptr(image,image+0x11ca300);
}
static int r146_view(R144Wallet*w,R146View*out){
 const R6Ops*o=&g_checks_ops;R146View v={0};R146Cache*c=&g_coin_pickup_cache;R87Identity game,ui,pawn,common;uint64_t meta,ui_data,owner;
 BYTE*pp=0,*cp=0;
 if(!w||!out||!r144_ready(w)||!r146_profile(w->image)||!rage_player_objects(w->image,&pp,&cp)||
 !r87_identity(o,(uint64_t)(ULONG_PTR)w->game,"BrgGameInfoNative",0,&game)||
 !r87_identity(o,(uint64_t)(ULONG_PTR)pp,"BrgPawn_CustomCharaPlayer",0,&pawn)||
 !r87_identity(o,(uint64_t)(ULONG_PTR)cp,"BrgCommonPawn_CustomCharaNative",(uint64_t)(ULONG_PTR)(w->image+0x20ac950),&common)||
 !r6_at(o,(uint64_t)(ULONG_PTR)w->image,0xf89d848,&v.coin_class,8)||!v.coin_class||
 !r6_name(o,v.coin_class,"BrgActor_Coin")||!r6_at(o,v.coin_class,0x50,&meta,8)||!r6_name(o,meta,"Class")||
 !r87_owner(o,v.coin_class,"Actor",&owner)||
 !r6_at(o,(uint64_t)(ULONG_PTR)w->image,0xf8b0088,&v.uid,4)||!v.uid||v.uid>2147483647u)return 0;
 if(!c->valid||c->game_class!=game.cls){
  c->valid=0;if(!r103_object_field(o,game.cls,"BrgGameInfoNative","mUIManager",0x1430,&c->game_ui))return 0;
 }
 if(!r6_at(o,game.object,c->game_ui,&v.ui,8)||!r87_identity(o,v.ui,"BrgUIManager",(uint64_t)(ULONG_PTR)(w->image+0x21d81b0),&ui))return 0;
 if(!c->valid||c->game_class!=game.cls||c->ui_class!=ui.cls||c->pawn_class!=pawn.cls||c->coin_class!=v.coin_class){
  c->valid=0;
  if(!r103_object_field(o,ui.cls,"BrgUIManager","mUserData",0x30c0,&c->ui_data)||
  !r103_object_field(o,ui.cls,"BrgUIManager","mPawnCustomCharaPlayer",0x30c0,&c->ui_pawn)||
  !r103_object_field(o,ui.cls,"BrgUIManager","mPlayerCommonPawnNative",0x30c0,&c->ui_common)||
  !r146_signature(o,pawn.cls,R146_SPAWN,&c->spawn)||!r146_signature(o,v.coin_class,R146_DESTROY,&c->destroy)||
  !r146_signature(o,v.coin_class,R146_GET_MONEY,&c->get_money)||!r146_signature(o,ui.cls,R146_MESSAGE,&c->message)||
  !r146_signature(o,v.coin_class,R146_EVENT,&c->event))return 0;
  c->game_class=game.cls;c->ui_class=ui.cls;c->pawn_class=pawn.cls;c->coin_class=v.coin_class;c->valid=1;
 }
 if(!r6_at(o,ui.object,c->ui_data,&ui_data,8)||ui_data!=(uint64_t)(ULONG_PTR)w->userdata||
 !r6_at(o,ui.object,c->ui_pawn,&v.pawn,8)||v.pawn!=pawn.object||
 !r6_at(o,ui.object,c->ui_common,&v.common,8)||v.common!=common.object||
 r146_authority(o,(uint64_t)(ULONG_PTR)w->image,v.uid,(uint64_t)(ULONG_PTR)w->body,&v.authority)!=1)return 0;
 *out=v;return 1;
}
static int r146_dispatch(BYTE*image,uint64_t object,uint64_t*out){
 uint64_t vt,target;if(!out||!r6_copy(&g_checks_ops,object,&vt,8)||!r6_at(&g_checks_ops,vt,0x218,&target,8)||
 target!=(uint64_t)(ULONG_PTR)(image+0x3bf4c0)||!executable_ptr(image,(void*)(ULONG_PTR)target))return 0;*out=target;return 1;
}
static int r146_destroy(BYTE*image,uint64_t actor){
 R146Signature*s=&g_coin_pickup_cache.destroy;uint8_t params[128]={0};uint64_t dispatch;uint32_t result=0;
 if(!s->function||!r146_dispatch(image,actor,&dispatch))return 0;
 ((void(WINAPI*)(void*,void*,void*,void*))(ULONG_PTR)dispatch)((void*)(ULONG_PTR)actor,(void*)(ULONG_PTR)s->function,params,0);
 memcpy(&result,params+s->offset[0],4);return (result&s->mask[0])!=0;
}
static int r146_same_view(const R146View*a,const R146View*b){return a->ui==b->ui&&a->pawn==b->pawn&&a->common==b->common&&a->coin_class==b->coin_class&&a->uid==b->uid&&a->authority.row==b->authority.row&&a->authority.group==b->authority.group;}
/* Caller already owns short request/save locks. 0=proved no monetary effect,
   -1=partial/uncertain effect (R138 must quarantine),1=both models read back. */
static int r146_pickup(R144Wallet*w,uint32_t amount,int before){
 R146View v,end;uint64_t actor=0,dispatch,actor_ui,actor_cls,vt,dirty;uint8_t params[128]={0};int result=0,invoked=0;int32_t picked;
 int scope=checks_chest_scope_begin(0);int ready=r146_view(w,&v);checks_chest_scope_end(0,scope);
 if(!ready||!amount||amount>50000||(uint32_t)v.authority.money>2147483647u-amount||
 !progress_native_writable((void*)(ULONG_PTR)(v.authority.row+0x54),4)||
 !progress_native_writable((void*)(ULONG_PTR)(v.common+0x446c),4)||
 !r6_at(&g_checks_ops,v.common,0x446c,&picked,4)||picked<0||(uint32_t)picked>2147483647u-amount||
 !r6_copy(&g_checks_ops,v.authority.row,&vt,8)||!r6_at(&g_checks_ops,vt,0x38,&dirty,8)||dirty!=(uint64_t)(ULONG_PTR)(w->image+0x11ca300)||
 !r146_dispatch(w->image,v.pawn,&dispatch))return 0;
 R146Signature*s=&g_coin_pickup_cache.spawn;memcpy(params+s->offset[0],&v.coin_class,8);memcpy(params+s->offset[1],&v.pawn,8);
 if(!r6_at(&g_checks_ops,v.pawn,0x80,params+s->offset[3],12))return 0;
 uint32_t mask=s->mask[6];memcpy(params+s->offset[6],&mask,4);
 ((void(WINAPI*)(void*,void*,void*,void*))(ULONG_PTR)dispatch)((void*)(ULONG_PTR)v.pawn,(void*)(ULONG_PTR)s->function,params,0);
 memcpy(&actor,params+s->offset[7],8);if(!actor)return 0;
 if(!r6_copy(&g_checks_ops,actor,&vt,8)||vt!=(uint64_t)(ULONG_PTR)(w->image+0x2091310)||
 !r6_at(&g_checks_ops,actor,0x50,&actor_cls,8)||actor_cls!=v.coin_class||
 !r6_at(&g_checks_ops,actor,0x290,&actor_ui,8)||actor_ui!=v.ui||!r144_ready(w))goto done;
 scope=checks_chest_scope_begin(0);ready=r146_view(w,&end);checks_chest_scope_end(0,scope);
 if(!ready||!r146_same_view(&v,&end)||end.authority.money!=v.authority.money||
 ((R144GetMoney)(*(void***)w->userdata)[0x2f8/8])(w->userdata,-1)!=before)goto done;
 /* Exact normal pickup ordering. No Coin.Initialize: no floor currency mesh,
    animation simulation or native Tick call on an uninitialized actor. */
 invoked=1;*(int32_t*)(ULONG_PTR)(v.common+0x446c)=picked+(int32_t)amount;
 ((void(WINAPI*)(void*,int))(w->image+0xf8a050))((void*)(ULONG_PTR)v.ui,(int)amount);
 if(!r144_ready(w))goto done;
 ((void(WINAPI*)(void*,int))(w->image+0xf8a270))((void*)(ULONG_PTR)actor,(int)amount);
 if(!r144_anchor(w)||((R144GetMoney)(*(void***)w->userdata)[0x2f8/8])(w->userdata,-1)!=before+(int)amount)goto done;
 /* Register EXTERNAL AP income in the same carried model used by native
    deposit; never fabricate a normal enemy/chest reward source identifier. */
 if(((BYTE(WINAPI*)(void*,int,const wchar_t*))(w->image+0x1368c60))((void*)(ULONG_PTR)v.authority.row,(int)amount,L"total_get_money")!=1)goto done;
 ((void(WINAPI*)(void*))(ULONG_PTR)dirty)((void*)(ULONG_PTR)v.authority.row);
 ((void(WINAPI*)(void*,int,int,int))(w->image+0xf8a510))((void*)(ULONG_PTR)actor,6,(int)amount,0);
 scope=checks_chest_scope_begin(0);ready=r146_view(w,&end);checks_chest_scope_end(0,scope);
 if(!ready||!r146_same_view(&v,&end)||end.authority.money!=v.authority.money+(int)amount||
 !r144_anchor(w)||((R144GetMoney)(*(void***)w->userdata)[0x2f8/8])(w->userdata,-1)!=before+(int)amount)goto done;
 result=1;
done:if(!r146_destroy(w->image,actor))return -1;
 return result?1:invoked?-1:0;
}
#endif
