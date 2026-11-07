#ifndef LID_PROGRESSION_HUB_OBSERVER_H
#define LID_PROGRESSION_HUB_OBSERVER_H
#include "progression_hub_events.h"
/* Current executable f33e91e6...17bddfd only. Observe existing area setter.
   This diagnostic NEVER gives items or starts a new native save. */
typedef void (WINAPI *LidNativeAreaSetter)(void*,RawFString*);
static Hook g_hub_area_hook;
static LidNativeAreaSetter g_hub_area_original;
static BYTE* g_hub_event_exe;
static int g_hub_area_install_state;
static LidHubEvents g_hub_events;
typedef struct LidNativeEntrySignal {unsigned serial;void* game;void* stage;void* body_before;} LidNativeEntrySignal;
static LidNativeEntrySignal g_hub_entry_signal;
#if LID_CHECKS_DELIVERY_ENABLED
static void checks_visit_area_observed(void* stage);
#endif
static int progress_hub_area_view(void* self,int* empty,int* native_hub){
    BYTE* exe=g_hub_event_exe;BYTE* game;BYTE* stage=self;void** vt;LidGameBagContext strings={0};RawFString* area;
    BYTE (WINAPI *is_hub)(void*);
    if(!exe||!stage||!empty||!native_hub||!g_progress_runtime_mutex.ops.thread_id||
       !(game=progress_native_game(exe))||lid_progress_game_started(game,progress_native_readable)!=1||
       *(void**)(game+0x13e8)!=self||!readable_range(stage,0x5f0)||*(void**)stage!=exe+0x21c3350)return 0;
    vt=*(void***)stage;
    if(!readable_range(vt,0x438)||vt[0x430/8]!=exe+0x143b2f0||vt[0x3d0/8]!=exe+0x1433900)return 0;
    strings.readable=progress_native_readable;area=(RawFString*)(stage+0x5e0);
    if(!lid_game_bag_string_valid((LidBagString*)area,&strings))return 0;
    *empty=area->num<=1;*native_hub=0;
    if(*empty){is_hub=(BYTE (WINAPI *)(void*))vt[0x3d0/8];*native_hub=!!is_hub(stage);}
    return 1;
}
static void WINAPI progress_hub_area_set(void* self,RawFString* area){
    int empty,native_hub,event;char line[220];size_t pos=0;
    if(g_hub_area_original)g_hub_area_original(self,area);
    /* Original always runs first, unmodified arguments. Never dispatch AP
       mutation inside the setter: caller has not finished loading user data. */
    if(!progress_hub_area_view(self,&empty,&native_hub))return;
#if LID_CHECKS_DELIVERY_ENABLED
    if(!empty)checks_visit_area_observed(self);
#endif
    event=lid_hub_event(&g_hub_events,g_progress_runtime_mutex.ops.thread_id(),empty);
    if(!event)return;
    if(event==LID_HUB_INITIAL||event==LID_HUB_RETURN){
        g_hub_entry_signal.game=progress_native_game(g_hub_event_exe);
        g_hub_entry_signal.stage=self;g_hub_entry_signal.body_before=g_v070_last_body;
        __atomic_store_n(&g_hub_entry_signal.serial,g_hub_events.serial,__ATOMIC_RELEASE);
    }
    line[0]=0;append_ascii(line,sizeof(line),&pos,"AP HUB R17: event=");
    append_ascii(line,sizeof(line),&pos,event==LID_HUB_INITIAL?"initial_candidate":event==LID_HUB_RETURN?"return_candidate":"left");
    append_ascii(line,sizeof(line),&pos," serial=");append_dec(line,sizeof(line),&pos,g_hub_events.serial);
    append_ascii(line,sizeof(line),&pos," native_hub_at_event=");append_dec(line,sizeof(line),&pos,(unsigned)native_hub);
    append_ascii(line,sizeof(line),&pos,"; notification only, no mutation inside setter.");line[pos]=0;log_ascii(line);
}
static int progress_install_hub_observer(void* module){
    static const BYTE prefix[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8d,0x99,0xe0,0x05,0x00,0x00};
    BYTE* exe=module;
    if(g_hub_area_install_state)return g_hub_area_install_state==1;
    g_hub_area_install_state=-1;
    if(!exe||!g_win.VirtualAlloc||!g_win.VirtualProtect||!g_win.FlushInstructionCache||
       !g_progress_runtime_mutex.ops.thread_id||!executable_ptr(exe,exe+0x143b2f0))return 0;
    g_hub_event_exe=exe;
    g_progress_event_exe=exe;
    if(!hook_install(&g_hub_area_hook,exe+0x143b2f0,(void*)progress_hub_area_set,prefix,sizeof(prefix))){
        log_ascii("AP HUB R17: area observer rejected; no fallback or guessed offset.");return 0;
    }
    g_hub_area_original=(LidNativeAreaSetter)g_hub_area_hook.trampoline;g_hub_area_install_state=1;
    log_ascii("AP HUB R17: area observer installed at verified RVA0x143B2F0; no continuous readiness probe.");
    return 1;
}
#endif
