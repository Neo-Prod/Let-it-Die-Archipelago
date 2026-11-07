#ifndef LID_NOTIFICATION_NATIVE_R154_H
#define LID_NOTIFICATION_NATIVE_R154_H
#if LID_NOTIFICATIONS_ENABLED
#include "material_owner_resolver.h"
#include "material_read_scope.h"
/* Exact f33e91e6... executable only. No new hook or reflected bytecode edit.
   Native exec/FFrame wrappers are deliberately NOT called as ordinary C APIs. */
typedef struct R154String {const uint16_t*data;int32_t num,max;} R154String;
typedef struct R154Popup {BYTE*image;uint64_t game_class,ui_class;uint32_t game_ui,ui_window;unsigned reason;} R154Popup;
static R154Popup g_notification_popup;
static const uint32_t r154_rva[8]={0x166e40,0x1461b50,0x14616a0,0x145e070,0x147f890,0x147f930,0x147f920,0x147fa00};
static const uint8_t r154_prefix[8][16]={
 {0x48,0x8b,0xc4,0x4c,0x89,0x40,0x18,0x57,0x41,0x54,0x41,0x56,0x48,0x83,0xec,0x70},
 {0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x57,0x41,0x56,0x48,0x83},
 {0x48,0x83,0xec,0x28,0x8b,0x51,0x78,0x85,0xd2,0x79,0x07,0x33,0xc0,0x48,0x83,0xc4},
 {0x48,0x89,0x5c,0x24,0x10,0x56,0x48,0x83,0xec,0x20,0x83,0xb9,0xb0,0,0,0},
 {0x88,0x51,0x79,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc},
 {0x88,0x51,0x7a,0xc3,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc},
 {0x48,0x89,0x91,0xac,0,0,0,0x4c,0x89,0x81,0xb4,0,0,0,0xc3,0xcc},
 {0x48,0x83,0xc1,0x7c,0xe9,0x17,0,0,0,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc}
};
static int r154_api(R154Popup*p){
 uint8_t bytes[16];uint64_t base=(uint64_t)(ULONG_PTR)p->image;
 for(unsigned i=0;i<8;i++)if(!r6_copy(&g_checks_ops,base+r154_rva[i],bytes,16)||!r15_equal(bytes,r154_prefix[i],16))return 0;
 return 1;
}
static int r154_popup_graph(R154Popup*p,uint64_t*ui_out,uint64_t*window_out,uint64_t*class_out){
 R87Identity game,ui,window;uint64_t g=(uint64_t)(ULONG_PTR)progress_native_game(p->image),u,w,cls,meta,framecls,fn;
 uint32_t size,flags;int32_t count,capacity;uint64_t base=(uint64_t)(ULONG_PTR)p->image;
 if(!g||lid_progress_game_started((void*)(ULONG_PTR)g,progress_native_readable)!=1||!r154_api(p)||
 !r87_identity(&g_checks_ops,g,"BrgGameInfoNative",0,&game))return 0;
 if(p->game_class!=game.cls){
  if(!r103_object_field(&g_checks_ops,game.cls,"BrgGameInfoNative","mUIManager",0x1430,&p->game_ui))return 0;
  p->game_class=game.cls;p->ui_class=0;
 }
 if(!r6_at(&g_checks_ops,g,p->game_ui,&u,8)||!r87_identity(&g_checks_ops,u,"BrgUIManager",base+0x21d81b0,&ui))return 0;
 if(p->ui_class!=ui.cls){
  if(!r103_object_field(&g_checks_ops,ui.cls,"BrgUIManager","mMessageWindow",0x30c0,&p->ui_window))return 0;
  p->ui_class=ui.cls;
 }
 if(!r6_at(&g_checks_ops,u,p->ui_window,&w,8)||!r87_identity(&g_checks_ops,w,"BrgUIHUD_MessageWindow",0,&window)||
 !r6_at(&g_checks_ops,window.vtable,0x318,&fn,8)||fn!=base+0x145e070||
 !r6_at(&g_checks_ops,w,0xb0,&count,4)||!r6_at(&g_checks_ops,w,0xb4,&capacity,4)||count<0||count>=16||capacity<count||capacity>4096||
 !r6_at(&g_checks_ops,base,0xfd188b0,&cls,8)||!r6_name(&g_checks_ops,cls,"BrgUIHUD_MessageWindowContents_Message")||
 !r6_at(&g_checks_ops,cls,0x50,&meta,8)||!r6_name(&g_checks_ops,meta,"Class")||
 !r6_at(&g_checks_ops,cls,0x88,&size,4)||size!=0x7c||!r6_at(&g_checks_ops,cls,0x16c,&flags,4)||(flags&1)||
 !r6_at(&g_checks_ops,cls,0x224,&fn,8)||fn!=base+0x1462e90||
 !r6_at(&g_checks_ops,base,0xfd18998,&framecls,8)||!r6_name(&g_checks_ops,framecls,"BrgUIHUD_MessageWindowParts_StringFrame"))return 0;
 *ui_out=u;*window_out=w;*class_out=cls;return 1;
}
static int r154_popup_display(void*opaque,const R154Message*m){
 R154Popup*p=opaque;uint64_t ui,window,cls,log,object,part,vt;int32_t count;int scope;
 if(!p||!m||m->kind!=R154_REQUEST||!r154_valid(m))return 0;
 scope=R152_READ_BEGIN();int ok=r154_popup_graph(p,&ui,&window,&cls)&&r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)p->image,0x280ed20,&log,8);R152_READ_END(scope);
 if(!ok){p->reason=1;return 0;}
 /* Real engine allocation; outer is the current window. All local UObject
    pointers stay inside this synchronous owner callback until native insertion.
    No wait, worker dispatch, file I/O or frame-spanning raw object cache. */
 object=((uint64_t(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t))(p->image+0x166e40))(cls,window,0,0,0,log,0,0);
 if(!object||!r6_copy(&g_checks_ops,object,&vt,8)||vt!=(uint64_t)(ULONG_PTR)(p->image+0x21d1780)){p->reason=2;return 0;}
 ((void(*)(uint64_t,uint64_t,uint64_t))(p->image+0x1461b50))(object,ui,window);
 part=((uint64_t(*)(uint64_t))(p->image+0x14616a0))(object);
 if(!part||!r6_copy(&g_checks_ops,part,&vt,8)||vt!=(uint64_t)(ULONG_PTR)(p->image+0x21d5250)){p->reason=3;return 0;}
 ((void(*)(uint64_t,uint8_t))(p->image+0x147f890))(part,1);
 ((void(*)(uint64_t,uint8_t))(p->image+0x147f930))(part,0);
 ((void(*)(uint64_t,uint64_t,uint64_t))(p->image+0x147f920))(part,0,0);
 R154String text={m->text,(int32_t)m->units+1,(int32_t)m->units+1};
 ((void(*)(uint64_t,const R154String*))(p->image+0x147fa00))(part,&text);
 if(!r6_at(&g_checks_ops,window,0xb0,&count,4)||count<0||count>=16){p->reason=4;return 0;}
 int accepted=((int(*)(uint64_t,uint64_t))(p->image+0x145e070))(window,object)==1;
 p->reason=accepted?0:5;return accepted;
}
static void notifications_native_tick(BYTE*image,int loaded_ready){
 R26Transport*t=&g_checks_delivery;R154Disk*d=&t->notifications;
 /* Idle path reads only plugin storage. No timer syscall or engine walk. */
 if(__atomic_load_n(&d->mailbox.state,__ATOMIC_ACQUIRE)!=R154_PENDING||!loaded_ready||!g_get_tid||
 !__atomic_load_n(&d->ready,__ATOMIC_ACQUIRE)||__atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||
 __atomic_load_n(&t->stop,__ATOMIC_ACQUIRE))return;
 DWORD tid=g_get_tid();R15Context current;
 if(tid!=t->owner||!checks_session()||!r25_host_context(&g_progress_host,&current)||!r15_same_context(&current,&d->mailbox.context))return;
 g_notification_popup.image=image;
 (void)r154_owner_deliver(&d->mailbox,tid,t->disk.ops.clock(),&g_notification_popup,r154_popup_display);
}
#endif
#endif
