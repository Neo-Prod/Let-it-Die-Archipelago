#ifndef LID_RELOAD_LOAD_TRACE_R36_H
#define LID_RELOAD_LOAD_TRACE_R36_H
#ifndef LID_RELOAD_LOAD_TRACE_ENABLED
#define LID_RELOAD_LOAD_TRACE_ENABLED 0
#endif
#if LID_RELOAD_LOAD_TRACE_ENABLED
/* Diagnostics only: no pointers, paths, save contents or identity strings.
   Foreign/reentrant ApplyRoot can only update atomic scalar diagnostics. */
typedef struct LidReloadLoadTrace {
    unsigned load_stage,hold_stage,os_error,close_error,native_result,
      bridge_stage,apply_stage,capture_stage,capture_index,bound_capture_stage,
      hash_match,cng_open_status,cng_hash_status,cng_close_status,apply_reject,bridge_reject;
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
    unsigned apply_entered,apply_invalid,apply_inactive;
#endif
} LidReloadLoadTrace;
static LidReloadLoadTrace g_reload_load_trace;
#define LID_R36_SET(field,value) __atomic_store_n(&g_reload_load_trace.field,(unsigned)(value),__ATOMIC_RELAXED)
#define LID_R36_GET(field) __atomic_load_n(&g_reload_load_trace.field,__ATOMIC_RELAXED)
#define LID_R36_RESULT(field,expression) __extension__ ({__auto_type _r36_value=(expression);LID_R36_SET(field,_r36_value);_r36_value;})
#define LID_R36_PHASE(field,value,expression) (LID_R36_SET(field,value),(expression))
#define LID_R36_IO(expression) __extension__ ({__auto_type _r36_io=(expression);if(!_r36_io)LID_R36_SET(os_error,LID_R36_OS_ERROR());_r36_io;})
#ifndef LID_R36_OS_ERROR
#define LID_R36_OS_ERROR() 0
#endif
#else
#define LID_R36_SET(field,value) ((void)0)
#define LID_R36_RESULT(field,expression) (expression)
#define LID_R36_PHASE(field,value,expression) (expression)
#define LID_R36_IO(expression) (expression)
#endif
#endif
