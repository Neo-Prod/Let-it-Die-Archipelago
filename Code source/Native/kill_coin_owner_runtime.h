#ifndef LID_KILL_COIN_OWNER_RUNTIME_R144_H
#define LID_KILL_COIN_OWNER_RUNTIME_R144_H
#if LID_KILL_COIN_DELIVERY_ENABLED
#if !LID_CHECKS_DELIVERY_ENABLED || !LID_MATERIAL_CAPABILITY_ENABLED || !LID_NATIVE_RELOAD_ENABLED
#error Currency requires verified session, module measurement and native reload guards
#endif
#include "kill_coin_diagnostic.h"
typedef int (WINAPI *R144GetMoney)(void*,int);
typedef int (WINAPI *R144AddMoney)(void*,int,int);
typedef struct R144Wallet {BYTE*image,*game,*userdata,*body;int index,money;} R144Wallet;
static uint32_t r144_thread(void*unused){(void)unused;return g_get_tid?g_get_tid():0;}
static int r144_policy(R144Wallet*w){
 R15Context context;char digest[65];DWORD owner=g_checks_delivery.owner;
 if(!w||!owner||r144_thread(0)!=owner||!g_progress_native_ready||!checks_session()||
    !g_progress_runtime_mutex.owned||g_progress_runtime_mutex.poisoned||g_progress_runtime_mutex.owner_thread!=owner)return R149_REFUSE(6);
 if(g_progress_host.active||g_progress_save_job.active||g_progress_apply_lease.active||g_entry_driver.active)return R149_REFUSE(7);
 if(!progress_reload_delivery_ready())return R149_REFUSE(8);
 if(!r25_host_context(&g_progress_host,&context)||!r15_same_context(&context,&g_checks_delivery.coins.context))return R149_REFUSE(9);
 if(!r105_verified(&g_material_owner_measurement,owner,(uint64_t)(ULONG_PTR)w->image,digest))return R149_REFUSE(10);
 if(!g_hub_events.outside&&__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE)>g_entry_driver.completed_serial)return R149_REFUSE(11);
 if(progress_native_game(w->image)!=w->game||lid_progress_game_started(w->game,progress_native_readable)!=1)return R149_REFUSE(12);
 return 1;
}
static int r144_anchor(R144Wallet*w){
 static const BYTE add_prefix[15]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
 static const BYTE get_prefix[10]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
 if(!w||!w->image||!w->game||!w->userdata||!w->body||w->index<0||w->index>=64||
    progress_native_game(w->image)!=w->game||!readable_range(w->game+0x1428,8)||*(BYTE**)(w->game+0x1428)!=w->userdata||
    !readable_range(w->userdata,0x7c)||*(void**)w->userdata!=w->image+0x21f7ac0||!readable_range(w->body,0x54))return R149_REFUSE(40);
 void**vt=*(void***)w->userdata;
 if(!readable_range(vt,0x1138)||vt[0x2f8/8]!=w->image+0x156c5c0||vt[0x308/8]!=w->image+0x155fde0||
    vt[0x530/8]!=w->image+0x15651a0||vt[0x1130/8]!=w->image+0x1568a70||
    !executable_ptr(w->image,vt[0x280/8])||!executable_ptr(w->image,vt[0x2f8/8])||!executable_ptr(w->image,vt[0x308/8])||
    !executable_ptr(w->image,vt[0x530/8])||!executable_ptr(w->image,vt[0x1130/8]))return R149_REFUSE(41);
 if(!readable_range(vt[0x308/8],15)||!readable_range(vt[0x2f8/8],10)||
    lid_memcmp(vt[0x308/8],add_prefix,15)||lid_memcmp(vt[0x2f8/8],get_prefix,10))return R149_REFUSE(42);
 if(((NativeUserDataBool)vt[0x280/8])(w->userdata)<=0||
    ((NativeUserDataNoArg)vt[0x530/8])(w->userdata)!=w->index||
    ((NativeUserDataPtr)vt[0x1130/8])(w->userdata)!=w->body)return R149_REFUSE(43);
 return 1;
}
static int r144_ready(void*context){R144Wallet*w=context;return r144_policy(w)&&r144_anchor(w);}
static int r144_read(void*context,uint64_t*subject,int32_t*money){
 R144Wallet*w=context;if(!subject||!money||!r144_anchor(w))return 0;
 int value=((R144GetMoney)(*(void***)w->userdata)[0x2f8/8])(w->userdata,-1);
 if(value<0)return 0;*subject=(uint64_t)(ULONG_PTR)w->body;*money=w->money=value;return 1;
}
#ifndef LID_KILL_COIN_PICKUP_ENABLED
#define LID_KILL_COIN_PICKUP_ENABLED 0
#endif
#if LID_KILL_COIN_PICKUP_ENABLED
#include "kill_coin_pickup_runtime.h"
#endif
static int r144_add(void*context,uint64_t subject,uint32_t amount){
 R144Wallet*w=context;LidProgressGateView gate,again;int result=0,before;
 if(!w||subject!=(uint64_t)(ULONG_PTR)w->body||!amount||amount>50000||!r144_ready(w)||
    !progress_native_gate_view(&gate,0)||gate.generation!=w->game||gate.game_started!=1||
    !lid_progress_gate_view_valid(&gate,&g_progress_gate_ops)||!lid_progress_gate_try_locks(&gate,&g_progress_gate_ops))return 0;
 before=w->money;
 if(!progress_native_gate_view(&again,0)||!lid_progress_gate_same_view(&gate,&again)||
    !lid_progress_gate_view_valid(&gate,&g_progress_gate_ops)||!lid_progress_gate_arrays_valid(&gate)||
    (*(unsigned*)((BYTE*)gate.manager+0xa0)&8)||*(int*)((BYTE*)gate.manager+0xc0)||*(int*)((BYTE*)gate.manager+0xd4)||
    *(void**)((BYTE*)gate.request_runner+0x40)||*(void**)((BYTE*)gate.save_runner+0x40)||
    !r144_ready(w)||!progress_native_writable(w->body+0x50,4))goto done;
 int now=((R144GetMoney)(*(void***)w->userdata)[0x2f8/8])(w->userdata,-1);
 if(now<0||now!=before||(uint32_t)now>2147483647u-amount)goto done;
#if LID_KILL_COIN_PICKUP_ENABLED
 result=r146_pickup(w,amount,before);
#else
 /* Verified native amount API; -1 selects CURRENT carried wallet. No bank,
    save-file write, inventory reconstruction, or custom resource UI here. */
 result=((R144AddMoney)(*(void***)w->userdata)[0x308/8])(w->userdata,(int)amount,-1)==1;
#endif
done:lid_progress_gate_unlock(&gate,&g_progress_gate_ops);return result;
}
#ifndef LID_KILL_COIN_HUB_ENABLED
#define LID_KILL_COIN_HUB_ENABLED 0
#endif
#if LID_KILL_COIN_HUB_ENABLED
#include "kill_coin_hub_runtime.h"
#endif
static void coins_native_tick(BYTE*image,BYTE*userdata,BYTE*body,int index,int loaded_ready){
#if LID_KILL_COIN_HUB_ENABLED
 r147_tick(image,userdata,body,index,loaded_ready);
#else
 if(!loaded_ready||!__atomic_load_n(&g_checks_delivery.coin_ready,__ATOMIC_ACQUIRE))return;
 uint32_t state=r141_state(&g_checks_delivery.coins);
 if(state!=R141_PROBE&&state!=R141_AUTHORIZED&&state!=R141_DURABLE)return;
 R144Wallet wallet={image,progress_native_game(image),userdata,body,index,0};
 R138Ops ops={&wallet,r144_thread,r144_ready,r144_read,r144_add};
 /* Application evidence is logged by client AFTER durable native ACK. No
    file logging or file polling on the game-thread currency path. */
 (void)r26_coin_owner(&g_checks_delivery,&ops);
#endif
}
#endif
#endif
