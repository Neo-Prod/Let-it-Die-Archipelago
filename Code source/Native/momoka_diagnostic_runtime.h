#ifndef LID_MOMOKA_DIAGNOSTIC_RUNTIME_R215_H
#define LID_MOMOKA_DIAGNOSTIC_RUNTIME_R215_H
#include "momoka_diagnostic_driver.h"
/* Diagnostic only: unchanged native result and purchase behavior. */
static M214Driver g_momoka_probe;
static BYTE*g_momoka_image;
static unsigned g_momoka_state;
static char g_momoka_slot[21];static int g_momoka_uid;
static unsigned long long (*g_momoka_clock)(void);
static unsigned long long g_momoka_started,g_momoka_first;
static int momoka_probe_session(void*unused){
 int uid;(void)unused;
 if(!g_momoka_image||!g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid)return 0;
 for(unsigned i=0;i<21;i++)if(g_momoka_slot[i]!=g_release_slot_key[i])return 0;
 return m213_copy(&g_momoka_probe.adapter,(uint64_t)(uintptr_t)(g_momoka_image+0xf8b5090),&uid,4)&&uid==g_momoka_uid;
}
static void momoka_probe_exec(void*receiver,void*frame,void*result){
 m214_call(&g_momoka_probe,receiver,frame,result);
}
static int momoka_probe_writable(uint64_t slot){
 MEMORY_BASIC_INFORMATION m;ULONG_PTR end;
 if(!slot||(slot&7)||!g_win.VirtualQuery||!g_win.VirtualQuery((void*)(uintptr_t)slot,&m,sizeof(m))||
 m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD))return 0;
 unsigned p=m.Protect&0xff;if(p!=4&&p!=8&&p!=0x40&&p!=0x80)return 0;
 end=(ULONG_PTR)m.BaseAddress+(ULONG_PTR)m.RegionSize;
 return end>=(ULONG_PTR)m.BaseAddress&&slot<=UINT64_MAX-8&&slot+8<=end;
}
static void momoka_probe_finish(void){
 M197SkillSnapshot records[16];unsigned count=0;M211Host*g=&g_momoka_probe.adapter.prepared.gate;
 if(!m214_take(g,records,16,&count))return;
 int restored=momoka_probe_writable(g_momoka_probe.adapter.prepared.native.slot)&&m207_restore(&g_momoka_probe.cell.ownership);
 g_momoka_state=2;
 log_ascii(restored?"AP MOMOKA R215: collection stopped; native slot restored.":"AP MOMOKA R215: collection stopped; slot no longer ours, not overwritten.");
 for(unsigned i=0;i<count;i++){
  char line[512]={0};size_t at=0;
  append_ascii(line,sizeof(line),&at,"AP MOMOKA R215: kind=");append_dec(line,sizeof(line),&at,(unsigned)records[i].context.kind);
  append_ascii(line,sizeof(line),&at," skill=");
  for(unsigned j=0;j<records[i].length&&at+1<sizeof(line);j++)line[at++]=(char)records[i].skill[j];line[at]=0;
  append_ascii(line,sizeof(line),&at," num=");append_hex64(line,sizeof(line),&at,(ULONG_PTR)(int64_t)records[i].context.num);
  log_ascii(line);
 }
 log_ascii("AP MOMOKA R215: diagnostic never changes GetSkill results, money, inventory or save data.");
}
static void momoka_probe_tick(BYTE*exe){
 M213Adapter*a=&g_momoka_probe.adapter;
 if(g_momoka_state==2)return;
 if(g_momoka_state==1){
  if(!g_get_tid||g_get_tid()!=a->prepared.gate.owner)return;
  unsigned long long now=g_momoka_clock();unsigned count=a->prepared.gate.count;
  if(count&&!g_momoka_first)g_momoka_first=now;
  if(!momoka_probe_session(0)||count>=16||now-g_momoka_started>=180000||
    (g_momoka_first&&now-g_momoka_first>=60000))momoka_probe_finish();
  return;
 }
 if(!exe||!g_get_tid||!g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||
 g_get_tid()!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return;
 BYTE*game=progress_native_game(exe);if(!game||lid_progress_game_started(game,progress_native_readable)!=1)return;
 void*kernel=lid_find_module_ascii("kernel32.dll");if(!kernel)return;
 a->api.read=(int(*)(void*,const void*,void*,size_t,size_t*))lid_get_export(kernel,"ReadProcessMemory");
 a->api.process=(void*(*)(void))lid_get_export(kernel,"GetCurrentProcess");
 a->api.thread=(uint32_t(*)(void))lid_get_export(kernel,"GetCurrentThreadId");
 a->api.get_error=(uint32_t(*)(void))lid_get_export(kernel,"GetLastError");
 a->api.set_error=(void(*)(uint32_t))lid_get_export(kernel,"SetLastError");
 g_momoka_clock=(unsigned long long(*)(void))lid_get_export(kernel,"GetTickCount64");
 if(!a->api.read||!a->api.process||!a->api.thread||!a->api.get_error||!a->api.set_error||!g_momoka_clock){g_momoka_state=2;return;}
 for(unsigned i=0;i<20;i++){char c=g_release_slot_key[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return;g_momoka_slot[i]=c;}
 if(g_release_slot_key[20])return;g_momoka_slot[20]=0;g_momoka_image=exe;
 if(!m213_copy(a,(uint64_t)(uintptr_t)(exe+0xf8b5090),&g_momoka_uid,4))return;
 if(!m214_prepare(&g_momoka_probe,(uint64_t)(uintptr_t)exe,(uint64_t)(uintptr_t)game,g_get_tid(),0,momoka_probe_session)||
 !momoka_probe_writable(a->prepared.native.slot)||!m214_activate(&g_momoka_probe,(uintptr_t)momoka_probe_exec)){
  g_momoka_state=2;log_ascii("AP MOMOKA R215: diagnostic refused; native behavior retained.");return;
 }
 g_momoka_started=g_momoka_clock();g_momoka_state=1;
 log_ascii("AP MOMOKA R215: bounded pass-through active (max16 samples,180s total,60s after first sample).");
}
#endif
