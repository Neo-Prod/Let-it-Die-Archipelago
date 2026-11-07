#include <windows.h>
#include "status_scan_guard.h"
typedef EXCEPTION_DISPOSITION (__cdecl *LidLanguageHandler)(
    EXCEPTION_RECORD*,void*,CONTEXT*,DISPATCHER_CONTEXT*);
static LidLanguageHandler lid_language_handler;
/* Local compiler scope tables only; never registers a global handler. */
EXCEPTION_DISPOSITION __cdecl __C_specific_handler(EXCEPTION_RECORD *r,
    void *f,CONTEXT *c,DISPATCHER_CONTEXT *d) {
    return lid_language_handler?lid_language_handler(r,f,c,d):ExceptionContinueSearch;
}
int lid_master_scan_guard_init(void *handler) {
    if(!handler || handler==(void*)__C_specific_handler)return 0;
    if(lid_language_handler)return (void*)lid_language_handler==handler;
    lid_language_handler=(LidLanguageHandler)handler;
    return 1;
}
typedef struct LidScanGuardContext {
    void *base;size_t size;const LidMasterScanTables *original;
    volatile unsigned completed,read_faults;
} LidScanGuardContext;
static void lid_guard_matched(void *context,int width,int grade,int row,void *address) {
    LidScanGuardContext *g=(LidScanGuardContext*)context;
    ++g->completed;
    if(g->original->matched)
        g->original->matched(g->original->context,width,grade,row,address);
}
static int lid_guard_filter(EXCEPTION_POINTERS *e,LidScanGuardContext *g) {
    EXCEPTION_RECORD *r=e?e->ExceptionRecord:0;
    if(!r || r->NumberParameters<2 ||
       !lid_scan_guard_accept(r->ExceptionCode,r->ExceptionInformation[0],
        (uintptr_t)r->ExceptionAddress,r->ExceptionInformation[1],
        (uintptr_t)lid_master_scan_region,LID_MASTER_SCAN_KERNEL_BYTES,
        (uintptr_t)g->base,g->size))return EXCEPTION_CONTINUE_SEARCH;
    ++g->read_faults;return EXCEPTION_EXECUTE_HANDLER;
}
__attribute__((noinline)) LidMasterScanGuardResult lid_master_scan_guarded(
    void *base,size_t size,const LidMasterScanTables *tables) {
    LidMasterScanGuardResult result={0,0,0};
    LidScanGuardContext context={base,size,tables,0,0};
    LidMasterScanTables local;
    if(!lid_language_handler){result.handler_missing=1;return result;}
    local=*tables;local.context=&context;local.matched=lid_guard_matched;
    __try {result.matches=lid_master_scan_region(base,size,&local);}
    __except(lid_guard_filter(GetExceptionInformation(),&context)) {
        result.matches=context.completed;
    }
    result.read_faults=context.read_faults;return result;
}
