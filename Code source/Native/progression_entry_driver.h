#ifndef LID_PROGRESSION_ENTRY_DRIVER_H
#define LID_PROGRESSION_ENTRY_DRIVER_H
#include "performance_events.h"
#if LID_EVENT_PROFILE_ENABLED
static LidPerfMark g_entry_span;
#endif
/* Include from mod.c after native adapter, observer and host preparation.
   Idle means no clock/gate/bridge I/O. Only explicit entry epochs do work. */
typedef unsigned long long (WINAPI *LidEntryClock)(void);
typedef struct LidNativeEntryDriver {
    unsigned serial,completed_serial;int active,stopped,clock_state,pinned_logged;
    void* generation;void* stage;void* body_before;
    unsigned long long start_ms,last_ms;int scheduled;
    LidEntryClock clock;
} LidNativeEntryDriver;
static LidNativeEntryDriver g_entry_driver;
static int progress_entry_clock_prepare(void){
    void* kernel;
    if(g_entry_driver.clock_state)return g_entry_driver.clock_state==1;
    g_entry_driver.clock_state=-1;kernel=lid_find_module_ascii("kernel32.dll");
    if(!kernel)return 0;
    g_entry_driver.clock=(LidEntryClock)lid_get_export(kernel,"GetTickCount64");
    if(!g_entry_driver.clock)return 0;
    g_entry_driver.clock_state=1;return 1;
}
static int progress_entry_body_ready(void){
    BYTE* game;
    if(!g_entry_driver.active||!g_v070_last_body||g_v070_last_body==g_entry_driver.body_before||
       !(game=progress_native_game(g_hub_event_exe)))return 0;
    return game==g_entry_driver.generation&&*(void**)(game+0x13e8)==g_entry_driver.stage;
}
static void progress_entry_finish(const char* message){
    LID_PERF_END(g_entry_span,8,"entry_span_including_async_wait",0);
    LID_PERF_DETAIL_STOP();
    LID_PERF_SUMMARY("entry_complete");
    if(g_entry_driver.serial>g_entry_driver.completed_serial)g_entry_driver.completed_serial=g_entry_driver.serial;
    g_entry_driver.active=0;g_entry_driver.scheduled=0;log_ascii(message);
}
static void progress_native_entry_tick(void){
#if LID_NATIVE_RELOAD_ENABLED
    if(!progress_reload_delivery_ready())return;
#endif
#if LID_CHARACTER_DELIVERY_ENABLED && LID_CHARACTER_ENTRY_DELIVERY_READY && !LID_CHARACTER_DIAGNOSTIC_ONLY
    unsigned serial;DWORD tid;unsigned long long now;int status;
    serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
    if(g_entry_driver.stopped||(!g_entry_driver.active&&(!serial||serial<=g_entry_driver.completed_serial)))return;
    if(!g_progress_runtime_mutex.ops.thread_id)return;
    tid=g_progress_runtime_mutex.ops.thread_id();
    if(!tid||tid!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return;
    if(!g_entry_driver.active&&g_hub_events.outside){g_entry_driver.completed_serial=serial;return;}
    if(!progress_entry_clock_prepare()){
        g_entry_driver.stopped=1;log_ascii("AP ENTRY R17: clock unavailable; receipts pending, no delivery.");return;
    }
    now=g_entry_driver.clock();
    if(!g_entry_driver.active){
        g_entry_driver.serial=serial;g_entry_driver.generation=g_hub_entry_signal.game;
        g_entry_driver.stage=g_hub_entry_signal.stage;g_entry_driver.body_before=g_hub_entry_signal.body_before;
        g_entry_driver.active=1;g_entry_driver.start_ms=now;g_entry_driver.scheduled=0;g_entry_driver.pinned_logged=0;
#if LID_EVENT_PROFILE_ENABLED
        g_entry_span=lid_perf_mark();
        LID_PERF_DETAIL_BEGIN();
#endif
        if(!progress_prepare_host()||!progress_init_native_adapter()||
           !lid_transaction_open_entry(&g_progress_transaction,serial)){
            progress_entry_finish("AP ENTRY R17: entry preparation refused; pending until a later real return.");return;
        }
        log_ascii("AP ENTRY R17: entry opened; one TARGET snapshot only, waiting for reloaded body and readiness.");
    }
    if((g_hub_events.outside||serial>g_entry_driver.serial)&&
       lid_transaction_cancel_entry(&g_progress_transaction)){
        progress_entry_finish("AP ENTRY R17: left/superseded before application; receipts remain pending.");return;
    }
#if LID_CHECKS_DIAGNOSTIC_ENABLED
    if(serial==g_entry_driver.serial&&!g_hub_events.outside)checks_diagnostic_entry_flush(serial);
#endif
    if(g_entry_driver.scheduled&&now-g_entry_driver.last_ms<100)return;
    g_entry_driver.scheduled=1;g_entry_driver.last_ms=now;
    /* Timeout is a BEFORE-MUTATION cancellation only. Never a save proof. */
    if(now-g_entry_driver.start_ms>=120000&&lid_transaction_cancel_entry(&g_progress_transaction)){
        progress_entry_finish("AP ENTRY R17: load/transport readiness timed out before mutation; receipts pending for next return.");return;
    }
    status=progress_native_transaction_tick();
    if(g_progress_transaction.entry_pinned&&!g_entry_driver.pinned_logged){
        g_entry_driver.pinned_logged=1;
        log_ascii("AP ENTRY R17: entry TARGET pinned; later receipts cannot join this entry batch.");
    }
    if(status==LID_TX_SAVED||status==LID_TX_ENTRY_DONE||status==LID_TX_REFRESHED||
       status==LID_TX_REFRESH_REFUSED||status==LID_TX_REFUSED||status==LID_TX_ENTRY_IDLE){
        if(g_progress_transaction.entry_open){
            g_entry_driver.stopped=1;log_ascii("AP ENTRY R17: inconsistent entry close; dispatcher suspended.");return;
        }
#if LID_CHECKS_DIAGNOSTIC_ENABLED
        checks_diagnostic_boundary(status);
#endif
        progress_entry_finish("AP ENTRY R17: entry closed; no further gate/file work until an actual return.");
    }else if(g_progress_transaction.phase==LID_TX_STOPPED||g_progress_transaction.phase==LID_TX_LOST){
        g_entry_driver.stopped=1;log_ascii("AP ENTRY R17: transaction suspended; retained worker lifetime, no invented acknowledgement.");
    }
#else
    (void)progress_native_transaction_tick();
#endif
}
#endif
