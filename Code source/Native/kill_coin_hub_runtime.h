#ifndef LID_KILL_COIN_HUB_RUNTIME_R147_H
#define LID_KILL_COIN_HUB_RUNTIME_R147_H
/* Waiting-room-only AP income. No coin actors, carried-wallet writes or HUD
   imitation. Native bank API + native client projection + owned native save. */
typedef unsigned char(WINAPI*R147Credit)(void*,int,const wchar_t*,const wchar_t*,int);
typedef int(WINAPI*R147Capacity)(int);
typedef int(WINAPI*R147SafeGet)(void*);
typedef int(WINAPI*R147SafeSet)(void*,int);
typedef struct R147Hub {
 unsigned serial,history,uid,ticket,saved;unsigned long long sequence;
 BYTE*image,*game,*userdata,*payload;int expected,dirty_failed,outcome;
 LidProgressLease lease;LidProgressSaveJob job;
} R147Hub;
static R147Hub g_coin_hub;
#if LID_KILL_COIN_DIAGNOSTIC_ENABLED
static void r149_signed(char*line,size_t capacity,size_t*pos,int value){
 if(value<0){append_ascii(line,capacity,pos,"-");append_dec(line,capacity,pos,(unsigned)(-(long long)value));}
 else append_dec(line,capacity,pos,(unsigned)value);
}
static void r149_emit(void){
 R149Diagnostic*d=&g_coin_diagnostic;unsigned serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
 if(!g_get_tid||g_get_tid()!=g_checks_delivery.owner||g_coin_hub.lease.window||g_progress_apply_lease.window)return;
 unsigned state=__atomic_load_n(&g_checks_delivery.coin_ready,__ATOMIC_ACQUIRE)?r141_state(&g_checks_delivery.coins):R141_BOOT,index=0xffffffffu;
 if(__atomic_load_n(&g_checks_delivery.coin_ready,__ATOMIC_ACQUIRE)&&
    (state==R141_PROBE||state==R141_AUTHORIZED||state==R141_NATIVE_SAVE||state==R141_APPLIED||state==R141_DURABLE||state==R141_UNCERTAIN))index=g_checks_delivery.coins.request.index;
 else if(state==R141_EMPTY||state==R141_STOPPED)return;
 if(d->serial!=serial){d->serial=serial;d->lines=d->seen=0;}
 if(d->lines>=16||(d->seen&&d->last_reason==d->reason&&d->last_state==state&&d->last_index==index))return;
 d->seen=1;d->last_reason=d->reason;d->last_state=state;d->last_index=index;d->lines++;
 char line[640];size_t pos=0;line[0]=0;
 append_ascii(line,sizeof(line),&pos,"AP KC R149: reason=");append_ascii(line,sizeof(line),&pos,r149_reason(d->reason));
#define R149_NUMBER(name,value) do{append_ascii(line,sizeof(line),&pos," " name "=");append_dec(line,sizeof(line),&pos,(unsigned)(value));}while(0)
 R149_NUMBER("code",d->reason);R149_NUMBER("protocol",state);R149_NUMBER("transport",__atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE));
 R149_NUMBER("index",index);R149_NUMBER("entry",serial);R149_NUMBER("completed",g_entry_driver.completed_serial);
 R149_NUMBER("pinned",g_coin_hub.serial);R149_NUMBER("history",g_coin_hub.history);R149_NUMBER("outside",g_hub_events.outside);
 R149_NUMBER("worker_ready",__atomic_load_n(&g_checks_delivery.coin_ready,__ATOMIC_ACQUIRE));
#undef R149_NUMBER
 append_ascii(line,sizeof(line),&pos," frontend=");r149_signed(line,sizeof(line),&pos,d->front);
 append_ascii(line,sizeof(line),&pos," bank=");r149_signed(line,sizeof(line),&pos,d->bank);
 append_ascii(line,sizeof(line),&pos," capacity=");r149_signed(line,sizeof(line),&pos,d->capacity);
 append_ascii(line,sizeof(line),&pos," dirty_rva=");append_hex64(line,sizeof(line),&pos,(ULONG_PTR)d->dirty_rva);
 log_ascii(line);
}
#define R149_EMIT() r149_emit()
#else
#define R149_EMIT() ((void)0)
#endif
static int r147_profile(BYTE*image){
 static const BYTE credit[16]={0x48,0x8b,0xc4,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x50};
 static const BYTE get[16]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x01,0x48,0x8b,0xd9,0xff,0x90,0x80,0x02};
 static const BYTE set[16]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x01,0x8b,0xfa,0x48};
 static const BYTE bank_dirty[10]={0x8b,0x41,0x5c,0xc6,0x05,0xd6,0x01,0x54,0x0e,0x01};
 return image&&readable_range(image+0x132d630,16)&&!lid_memcmp(image+0x132d630,credit,16)&&
  readable_range(image+0x1574ab0,16)&&!lid_memcmp(image+0x1574ab0,get,16)&&
  readable_range(image+0x1582cc0,16)&&!lid_memcmp(image+0x1582cc0,set,16)&&
  readable_range(image+0x13616b0,10)&&!lid_memcmp(image+0x13616b0,bank_dirty,10)&&executable_ptr(image,image+0x13616b0)&&
  executable_ptr(image,image+0x132d630)&&executable_ptr(image,image+0x11ff550)&&
  executable_ptr(image,image+0x11ca300)&&executable_ptr(image,image+0x110f110)&&executable_ptr(image,image+0x11ca160);
}
static int r147_anchor(R144Wallet*w){
 if(!r144_anchor(w))return 0;
 if(!r147_profile(w->image))return R149_REFUSE(14);
 if(!readable_range(w->image+0xf8b0080,0x130))return R149_REFUSE(15);
 BYTE*user=w->image+0xf8b0080;void**vt=*(void***)w->userdata;void**server=*(void***)user;
 if(!server||!readable_range(server,0x40))return R149_REFUSE(16);
 R149_VALUE(dirty_rva,(unsigned long long)(ULONG_PTR)server[0x38/8]-(unsigned long long)(ULONG_PTR)w->image);
 if(server[0x38/8]!=w->image+0x13616b0)return R149_REFUSE(16);
 if(!readable_range(vt,0x2e8)||vt[0x2d8/8]!=w->image+0x1574ab0||vt[0x2e0/8]!=w->image+0x1582cc0)return R149_REFUSE(17);
 if(*(int*)(user+8)<=0||!readable_range(w->userdata+0x74,8))return R149_REFUSE(18);
 BYTE*payload=*(BYTE**)(w->userdata+0x74);
 if(!payload||!readable_range(payload+0x254,4))return R149_REFUSE(19);
 if(g_coin_hub.job.active&&(g_coin_hub.image!=w->image||g_coin_hub.game!=w->game||
    g_coin_hub.userdata!=w->userdata||g_coin_hub.payload!=payload||g_coin_hub.uid!=*(unsigned*)(user+8)))return R149_REFUSE(21);
 int bank=((R147SafeGet)vt[0x2d8/8])(w->userdata);
 R149_VALUE(front,bank);
 if(bank<0)return R149_REFUSE(20);
 int authority=*(int*)(user+0xc);R149_VALUE(bank,authority);
 if(bank!=authority)return R149_REFUSE(20);
 return 1;
}
static int r153_entry_eligible(void){
 R147Hub*h=&g_coin_hub;
 if(g_hub_events.outside||!h->serial||h->serial!=g_entry_driver.completed_serial||
    h->serial!=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE)||
    g_checks_delivery.coins.request.index>=h->history)return R149_REFUSE(22);
 return 1;
}
static int r147_ready(void*context){
 R144Wallet*w=context;R147Hub*h=&g_coin_hub;
 if(!r153_entry_eligible()||!r144_policy(w))return 0;
 if(h->job.active||h->lease.active)return R149_REFUSE(23);
 return r147_anchor(w);
}
static int r147_read(void*context,uint64_t*subject,int32_t*money){
 R144Wallet*w=context;if(!subject||!money||!r147_anchor(w))return 0;
 *subject=(uint64_t)(ULONG_PTR)(w->image+0xf8b0080);
 *money=((R147SafeGet)(*(void***)w->userdata)[0x2d8/8])(w->userdata);return 1;
}
static void WINAPI r147_dirty(void){
 R147Hub*h=&g_coin_hub;h->dirty_failed=1;
 if(!h->job.active||__atomic_load_n(&h->job.completion,__ATOMIC_ACQUIRE)!=-1||
    !h->image||!progress_native_writable(h->image+0xf8a1890,1))return;
 ((void(WINAPI*)(void))(h->image+0x11ca300))();h->dirty_failed=0;
}
static BYTE WINAPI r147_save(void*runner,void*json){
 R147Hub*h=&g_coin_hub;if(h->dirty_failed||!h->job.active||runner!=h->job.runner)return 0;
 return ((BYTE(WINAPI*)(void*,void*))(h->image+0x110f110))(runner,json);
}
static BYTE WINAPI r147_start(void*runner,LidProgressSaveBox*box,const wchar_t*name){
 R147Hub*h=&g_coin_hub;
 if(!h->job.active||!h->lease.window||runner!=h->lease.view.save_runner||runner!=h->job.runner)return 0;
 if(!((BYTE(WINAPI*)(void*,LidProgressSaveBox*,const wchar_t*))(h->image+0x11ca160))(runner,box,name))return 0;
 return readable_range((BYTE*)runner+0x40,8)&&*(void**)((BYTE*)runner+0x40)!=0;
}
static int r147_add(void*context,uint64_t subject,uint32_t amount){
 R144Wallet*w=context;R147Hub*h=&g_coin_hub;LidProgressGateView gate;uint64_t current;int32_t before;
 if(!amount||amount>50000||!r147_ready(w)||!r147_read(w,&current,&before)||subject!=current||
    !progress_native_gate_view(&gate,1)||gate.generation!=w->game||!g_tbb_malloc||!g_tbb_free||
    lid_progress_lease_begin(&h->lease,&gate,&g_progress_gate_ops)!=1){
#if LID_KILL_COIN_DIAGNOSTIC_ENABLED
     if(!g_coin_diagnostic.reason)r149_refuse(24);
#endif
     return 0;}
 int result=0;BYTE*user=w->image+0xf8b0080;
 if(!r147_anchor(w)||*(int*)(user+0xc)!=before)goto done;
 int cap=((R147Capacity)(w->image+0x11ff550))(*(int*)(user+0x12c));
 R149_VALUE(capacity,cap);
 if(cap<before||amount>(unsigned)(cap-before)){R149_REFUSE(25);goto done;}
 h->image=w->image;h->game=w->game;h->userdata=w->userdata;h->payload=*(BYTE**)(w->userdata+0x74);
 h->uid=*(unsigned*)(user+8);h->expected=before+(int)amount;h->saved=0;h->outcome=0;
 /* Native bank credit used by vanilla waiting-room deposits. */
 if(!((R147Credit)(w->image+0x132d630))(user,(int)amount,L"nolog",L"nolog",0)){
  R149_REFUSE(26);
  result=*(int*)(user+0xc)==before?0:-1;goto done;
 }
 result=-1;
 if(*(int*)(user+0xc)!=h->expected||
    !((R147SafeSet)(*(void***)w->userdata)[0x2e0/8])(w->userdata,h->expected)||!r147_anchor(w)){R149_REFUSE(27);goto done;}
 ((void(WINAPI*)(void*))(*(void***)user)[0x38/8])(user);
 h->job.ops=(LidProgressSaveOps){progress_native_allocate,progress_native_release,r147_dirty,r147_save,r147_start};
 h->ticket=lid_progress_save_begin(&h->job,gate.save_runner,(unsigned long long)g_checks_delivery.coins.request.index+1);
 if(!h->ticket){R149_REFUSE(28);goto done;}
 lid_progress_lease_leave_window(&h->lease);return 2; /* ACK waits for poll. */
done:
 if(!lid_progress_lease_close(&h->lease))result=-1;
 return result;
}
static int r147_poll(void*context){
 R144Wallet*w=context;R147Hub*h=&g_coin_hub;LidProgressGateView gate;
 /* Finish/release an already owned save even if the AP session stops. Never
    abandon a live runnable or leave its engine reservation solely on that. */
 if((!h->job.active&&!h->saved)||!h->lease.active||w->image!=h->image||w->game!=h->game||
    !progress_native_gate_view(&gate,0)||!lid_progress_gate_same_view(&gate,&h->lease.view)){R149_REFUSE(29);return -1;}
 int held=lid_progress_lease_reenter(&h->lease,&gate);if(!held)return 0;if(held<0)return -1;
 int result=h->saved?1:lid_progress_save_finish(&h->job,h->ticket,h->job.batch,lid_progress_lease_save_active(&h->lease));
 if(result==0){lid_progress_lease_leave_window(&h->lease);return 0;}
 if(!h->saved){
  h->outcome=result==1&&r144_policy(w)&&r147_anchor(w)&&w->userdata==h->userdata&&
   *(BYTE**)(w->userdata+0x74)==h->payload&&*(unsigned*)(h->image+0xf8b0088)==h->uid&&
   *(int*)(h->image+0xf8b008c)==h->expected?1:-1;
  h->saved=1;
  if(h->outcome!=1)R149_REFUSE(30);
 }
 return lid_progress_lease_close(&h->lease)?h->outcome:0;
}
static void r147_tick(BYTE*image,BYTE*userdata,BYTE*body,int index,int loaded_ready){
 if(!loaded_ready){
#if LID_KILL_COIN_DIAGNOSTIC_ENABLED
  if(g_get_tid&&g_get_tid()==g_checks_delivery.owner){g_coin_diagnostic.reason=1;r149_emit();}
#endif
  return;
 }
 if(!g_get_tid||g_get_tid()!=g_checks_delivery.owner)return;
#if LID_KILL_COIN_DIAGNOSTIC_ENABLED
 g_coin_diagnostic.reason=0;g_coin_diagnostic.front=g_coin_diagnostic.bank=g_coin_diagnostic.capacity=-1;g_coin_diagnostic.dirty_rva=0;
#endif
 if(!__atomic_load_n(&g_checks_delivery.coin_ready,__ATOMIC_ACQUIRE)){R149_REFUSE(2);R149_EMIT();return;}
 R147Hub*h=&g_coin_hub;uint32_t state=r141_state(&g_checks_delivery.coins);
 unsigned serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
 if(!h->job.active&&!h->lease.active&&!g_hub_events.outside&&!g_entry_driver.active&&serial&&serial==g_entry_driver.completed_serial&&
    serial!=h->serial&&g_progress_host.acknowledged&&g_progress_host.peer.last_target.sequence==g_progress_host.acknowledged){
  h->serial=serial;h->history=g_progress_host.peer.last_target.history_length;h->sequence=g_progress_host.acknowledged;
 }
 if(state!=R141_PROBE&&state!=R141_AUTHORIZED&&state!=R141_DURABLE&&state!=R141_NATIVE_SAVE){R149_REFUSE(3);R149_EMIT();return;}
 /* A deferred receipt cannot apply in the tower or after the frozen entry.
    Do not rebuild the engine graph on every UI Tick just to refuse it. */
 int eligible=r153_entry_eligible();
 if(state==R141_PROBE&&!eligible){R149_EMIT();return;}
 /* AUTHORIZED still visits the protocol so a now-ineligible armed intent is
    cancelled/cleaned. DURABLE only commits the acknowledged receipt. Neither
    path needs a game pointer. An owned NATIVE_SAVE always keeps its poll. */
 BYTE*game=(state==R141_DURABLE||(state==R141_AUTHORIZED&&!eligible))?0:progress_native_game(image);
 R144Wallet w={image,game,userdata,body,index,0};
 R138Ops ops={&w,r144_thread,r147_ready,r147_read,r147_add,r147_poll};
 (void)r26_coin_owner(&g_checks_delivery,&ops);
 R149_EMIT();
}
#endif
