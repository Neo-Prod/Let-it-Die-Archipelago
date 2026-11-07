#ifndef LID_KILL_COIN_DIAGNOSTIC_R149_H
#define LID_KILL_COIN_DIAGNOSTIC_R149_H
#ifndef LID_KILL_COIN_DIAGNOSTIC_ENABLED
#define LID_KILL_COIN_DIAGNOSTIC_ENABLED 0
#endif
#if LID_KILL_COIN_DIAGNOSTIC_ENABLED
typedef struct R149Diagnostic {
 unsigned reason,serial,lines,last_reason,last_state,last_index,seen;
 int front,bank,capacity;unsigned long long dirty_rva;
} R149Diagnostic;
static R149Diagnostic g_coin_diagnostic;
static int r149_refuse(unsigned reason){g_coin_diagnostic.reason=reason;return 0;}
#define R149_REFUSE(reason) r149_refuse(reason)
#define R149_VALUE(field,value) (g_coin_diagnostic.field=(value))
static const char*r149_reason(unsigned n){
 switch(n){
 case 0:return "no_refusal_recorded";case 1:return "character_load_not_ready";
 case 2:return "currency_worker_not_ready";case 3:return "protocol_not_actionable";
 case 4:return "transport_stopped_or_not_active";case 5:return "entry_snapshot_not_pinned";
 case 6:return "owner_session_mutex";case 7:return "other_delivery_in_progress";
 case 8:return "reload_not_ready";case 9:return "slot_context";case 10:return "executable_measurement";
 case 11:return "entry_not_completed";case 12:return "game_not_started_or_current";
 case 14:return "bank_function_profile";case 15:return "bank_object_unreadable";
 case 16:return "bank_dirty_virtual";case 17:return "bank_get_set_virtuals";
 case 18:return "bank_uid_or_userdata_unreadable";case 19:return "bank_payload_unreadable";
 case 20:return "bank_frontend_differs";case 21:return "save_identity_changed";
 case 22:return "outside_or_receipt_after_entry";case 23:return "currency_save_busy";
 case 24:return "bank_reservation_unavailable";case 25:return "bank_capacity";
 case 26:return "native_credit_refused";case 27:return "bank_projection_or_readback";
 case 28:return "native_save_start_refused";case 29:return "native_save_poll_refused";
 case 30:return "native_save_or_final_readback_refused";
 case 40:return "carried_userdata_anchor";case 41:return "carried_virtual_table";
 case 42:return "carried_native_prefix";case 43:return "current_character_binding";
 default:return "other";
 }
}
#else
#define R149_REFUSE(reason) 0
#define R149_VALUE(field,value) ((void)0)
#endif
#endif
