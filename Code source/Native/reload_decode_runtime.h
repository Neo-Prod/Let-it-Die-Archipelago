#ifndef LID_RELOAD_DECODE_RUNTIME_R37_H
#define LID_RELOAD_DECODE_RUNTIME_R37_H
#ifndef LID_RELOAD_DECODE_TRACE_ENABLED
#define LID_RELOAD_DECODE_TRACE_ENABLED 0
#endif
#include "reload_decode_call_patch.h"
#include "reload_decode_observer.h"
#include "reload_verified_chunks.h"
#if LID_RELOAD_DECODE_TRACE_ENABLED
#if !LID_NATIVE_RELOAD_ENABLED || !LID_CHECKS_ROTATION_ENABLED || !LID_RELOAD_LOAD_TRACE_ENABLED
#error "R37 observation requires the verified R36 reload/rotation/trace build"
#endif
static struct{R37CallSet calls;R37Trace trace;uintptr_t exe;}g_reload_decode;
static R41Chunks g_reload_verified_chunks;
static struct{unsigned calls,selected,refused,expected_size,allocated_size;}g_r42_path;
typedef BOOL (WINAPI *R41OwnProcessRead)(HANDLE,LPCVOID,LPVOID,SIZE_T,SIZE_T*);
static R41OwnProcessRead g_r41_own_process_read;
static int r41_native_copy(void*c,uintptr_t input,void*out,size_t size){
    SIZE_T got=0;(void)c;
    return g_r41_own_process_read&&size&&size<=256&&
        g_r41_own_process_read((HANDLE)(LONG_PTR)-1,(void*)input,out,(SIZE_T)size,&got)&&got==size;
}
static int r41_native_same(void*c,uintptr_t input,const unsigned char*verified,size_t size){
    (void)c;
    return r41_compare_input(r41_native_copy,0,input,verified,size);
}
static int r37_native_read(void*c,uintptr_t p,void*out,size_t n){
    (void)c;if(!out||!p||!n||n>24||!readable_range((void*)p,n))return 0;lid_memcpy(out,(void*)p,n);return 1;
}
static int r37_native_scope(void){
    /* The bridge release-publishes owner_thread and our begin fields before
       active=1. Reject foreign threads BEFORE reading owner-only trace data. */
    if(__atomic_load_n(&g_reload_runtime.bridge.active,__ATOMIC_ACQUIRE)!=1)return 0;
    unsigned tid=progress_reload_thread(0);if(!tid||tid!=g_reload_runtime.bridge.owner_thread)return 0;
    return g_reload_decode.calls.state==R31_PATCH_READY&&r37_scope(&g_reload_decode.trace,tid)&&progress_reload_phase()==LID_RR_LOADING;
}
static const wchar_t*WINAPI r42_reader_path_detour(void){
    typedef const wchar_t*(WINAPI*Fn)(void);
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    const wchar_t*original=((Fn)g_reload_decode.calls.originals[5])();
    if(caller!=g_reload_decode.exe+0x1107eb8||!r37_native_scope())return original;
    LidReloadBoundSource*s=&g_reload_runtime.source;LidReloadSourceHold*h=&s->hold;
    g_r42_path.calls++;g_r42_path.expected_size=h->proof.file_size;
    /* This one reader passes the returned path to BOTH FileSize (FindFirstFileW)
       and CreateReader (CreateFileW). Do not change the cached native getter,
       the file manager, or callers outside the owner-scoped AP reload. */
    if(!g_reload_runtime.proxy_verified||!s->initialized||s->closed||!s->load_attempted||
       !h->bytes||h->bytes_size!=h->proof.file_size||!s->path[0]||
       !lid_reload_source_approved(h,&s->proof.session,s->snapshot.uid,s->proof.uid_length)){
        static const wchar_t denied[]=L"";g_r42_path.refused++;return denied;
    }
    g_r42_path.selected++;return s->path;
}
static int WINAPI r37_size_detour(void*array,int added){
    typedef int(WINAPI*Fn)(void*,int);uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    int result=((Fn)g_reload_decode.calls.originals[0])(array,added);
    if(caller==g_reload_decode.exe+0x1107f0d&&r37_native_scope()){
        g_r42_path.allocated_size=(unsigned)added;
        r37_sized(&g_reload_decode.trace,progress_reload_thread(0),r37_native_read,0,(uintptr_t)array,added,result);
    }
    return result;
}
static uint64_t WINAPI r37_decompress_detour(unsigned flags,void*out,unsigned unpacked,const void*input,unsigned packed,unsigned sixth){
    typedef uint64_t(WINAPI*Fn)(unsigned,void*,unsigned,const void*,unsigned,unsigned);
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    const void*selected=input;int scope=r37_native_scope();
    if(caller==g_reload_decode.exe+0x110859f&&scope){
        LidReloadSourceHold*hold=&g_reload_runtime.source.hold;
        if(flags!=1||sixth||!hold->active||hold->closed||hold->poisoned||
           !hold->bytes||hold->bytes_size!=hold->proof.file_size||
           !g_reload_decode.trace.input_shape||
           !r41_chunk_input(&g_reload_verified_chunks,hold->bytes,hold->bytes_size,
               g_reload_decode.trace.input,g_reload_decode.trace.input_size,
               (uintptr_t)input,(uintptr_t)out,unpacked,packed,r41_native_same,0,&selected)){
            if(!g_reload_verified_chunks.refused)r41_no(&g_reload_verified_chunks,8);
            r37_decoded(&g_reload_decode.trace,progress_reload_thread(0),flags,unpacked,packed,sixth,2,0);
            return 0;
        }
    }
    uint64_t result=((Fn)g_reload_decode.calls.originals[1])(flags,out,unpacked,selected,packed,sixth);
    if(caller==g_reload_decode.exe+0x110859f&&scope&&!result)g_reload_verified_chunks.native_failures++;
    if((caller==g_reload_decode.exe+0x11082d8||caller==g_reload_decode.exe+0x110859f)&&r37_native_scope())
        r37_decoded(&g_reload_decode.trace,progress_reload_thread(0),flags,unpacked,packed,sixth,
          caller==g_reload_decode.exe+0x110859f?2:1,result);
    return result;
}
static void*WINAPI r37_input_free_detour(void*pointer,size_t size,size_t alignment){
    typedef void*(WINAPI*Fn)(void*,size_t,size_t);uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    if(caller==g_reload_decode.exe+0x1108657&&r37_native_scope())
        r37_before_free(&g_reload_decode.trace,progress_reload_thread(0),r37_native_read,0,(uintptr_t)pointer,size);
    return ((Fn)g_reload_decode.calls.originals[3])(pointer,size,alignment);
}
static void*WINAPI r37_parser_detour(void*string){
    typedef void*(WINAPI*Fn)(void*);uintptr_t caller=(uintptr_t)__builtin_return_address(0);int observed=0;
    if(caller==g_reload_decode.exe+0x1108874&&r37_native_scope()){
        r37_parse_input(&g_reload_decode.trace,progress_reload_thread(0),r37_native_read,0,(uintptr_t)string);observed=1;
    }
    void*result=((Fn)g_reload_decode.calls.originals[4])(string);
    if(observed&&r37_native_scope())g_reload_decode.trace.parse_result=result!=0;
    return result;
}
static int checks_load_decode_prepare_at_idle(void){
    if(g_reload_decode.calls.state!=R31_PATCH_EMPTY)return g_reload_decode.calls.state==R31_PATCH_READY;
    if(!g_reload_runtime.proxy_verified||!progress_reload_idle_valid())return 0;
    g_reload_decode.exe=(uintptr_t)g_reload_runtime.exe;
    R32CallPatchOps ops={{&g_reload_runtime,progress_reload_patch_quiet,progress_reload_patch_read,progress_reload_patch_alloc,
        progress_reload_patch_free,progress_reload_patch_write,progress_reload_patch_protect,progress_reload_patch_flush},
        {&g_checks_rotation_runtime,checks_rotation_region,checks_rotation_allocate_at}};
    void*detours[R37_SITE_COUNT]={(void*)r37_size_detour,(void*)r37_decompress_detour,(void*)r37_decompress_detour,
        (void*)r37_input_free_detour,(void*)r37_parser_detour,(void*)r42_reader_path_detour};
    int result=r37_calls_install(&g_reload_decode.calls,g_reload_decode.exe,1,detours,&ops);
    log_ascii(result?"AP RELOAD R42: six reader-local CALL relays ready; scoped AP path for size and read.":
        "AP RELOAD R37: reader-local observation refused; existing reload policy unchanged.");return result;
}
static void checks_load_decode_begin(void){
    g_r41_own_process_read=(R41OwnProcessRead)lid_get_export(lid_find_module_ascii("kernel32.dll"),"ReadProcessMemory");
    lid_memset(&g_reload_verified_chunks,0,sizeof(g_reload_verified_chunks));
    lid_memset(&g_r42_path,0,sizeof(g_r42_path));
    lid_memset(&g_reload_decode.trace,0,sizeof(g_reload_decode.trace));
    if(g_reload_decode.calls.state!=R31_PATCH_READY||progress_reload_phase()!=LID_RR_LOADING)return;
    g_reload_decode.trace.owner=progress_reload_thread(0);g_reload_decode.trace.active=1;
}
static void checks_load_decode_end(void){
    R37Trace*t=&g_reload_decode.trace;char line[700];size_t n=0;r37_end(t);
    append_ascii(line,sizeof(line),&n,"AP RELOAD R37 DECODE:");
#define R37_PRINT(field) append_ascii(line,sizeof(line),&n," " #field "=");append_dec(line,sizeof(line),&n,t->field)
    R37_PRINT(size_calls);R37_PRINT(input_shape);R37_PRINT(header_seen);R37_PRINT(header_suppressed);R37_PRINT(header_format);R37_PRINT(header_zero);R37_PRINT(decode_calls);R37_PRINT(decode_overflow);
    R37_PRINT(parse_calls);R37_PRINT(parse_shape);R37_PRINT(parse_units);R37_PRINT(parse_capacity);R37_PRINT(parse_first);R37_PRINT(parse_last);R37_PRINT(parse_result);
#undef R37_PRINT
    for(unsigned i=0;i<6;i++){append_ascii(line,sizeof(line),&n," h");append_dec(line,sizeof(line),&n,i);append_ascii(line,sizeof(line),&n,"=");append_dec(line,sizeof(line),&n,t->header[i]);}
    line[n]=0;log_ascii(line);
    n=0;append_ascii(line,sizeof(line),&n,"AP RELOAD R42 PATH:");
#define R42_PRINT(field) append_ascii(line,sizeof(line),&n," " #field "=");append_dec(line,sizeof(line),&n,g_r42_path.field)
    R42_PRINT(calls);R42_PRINT(selected);R42_PRINT(refused);R42_PRINT(expected_size);R42_PRINT(allocated_size);
#undef R42_PRINT
    line[n]=0;log_ascii(line);
    n=0;append_ascii(line,sizeof(line),&n,"AP RELOAD R41 SOURCE:");
#define R41_PRINT(field) append_ascii(line,sizeof(line),&n," " #field "=");append_dec(line,sizeof(line),&n,g_reload_verified_chunks.field)
    R41_PRINT(calls);R41_PRINT(matches);R41_PRINT(mismatches);R41_PRINT(unreadable);R41_PRINT(refused);
    R41_PRINT(reason);R41_PRINT(native_failures);R41_PRINT(complete);
#undef R41_PRINT
    append_ascii(line,sizeof(line),&n," comparison_available=");append_dec(line,sizeof(line),&n,g_r41_own_process_read!=0);
    line[n]=0;log_ascii(line);
    for(unsigned i=0;i<t->decode_calls&&i<8;i++){
        n=0;append_ascii(line,sizeof(line),&n,"AP RELOAD R37 CHUNK: index=");append_dec(line,sizeof(line),&n,i);
        static const char*keys[]={" site="," flags="," unpacked="," packed="," sixth="," result="};
        for(unsigned j=0;j<6;j++){append_ascii(line,sizeof(line),&n,keys[j]);append_dec(line,sizeof(line),&n,t->decode[i][j]);}
        line[n]=0;log_ascii(line);
    }
}
#endif
#endif
