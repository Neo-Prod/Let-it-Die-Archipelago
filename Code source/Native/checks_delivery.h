#ifndef LID_CHECKS_DELIVERY_R27_H
#define LID_CHECKS_DELIVERY_R27_H
#ifndef LID_CHECKS_DELIVERY_ENABLED
#define LID_CHECKS_DELIVERY_ENABLED 0
#endif
#if LID_CHECKS_DELIVERY_ENABLED
#if !defined(LID_CHECKS_DIAGNOSTIC_ENABLED) || !LID_CHECKS_DIAGNOSTIC_ENABLED
#error "Checks delivery requires the validated diagnostic observer"
#endif
#include "checks_native_adapter.h"
#include "checks_visit.h"
static R39VisitState g_checks_visits;
static R26Transport g_checks_delivery;
static unsigned g_checks_delivery_attempted;
static int checks_delivery_rotation_ready(DWORD owner){
#if LID_CHECKS_ROTATION_ENABLED
    R31Identity now;
    if(!g_get_tid||g_get_tid()!=owner||!g_progress_host.acknowledged||
       __atomic_load_n(&g_checks_rotation_runtime.rotation.fault,__ATOMIC_ACQUIRE)||
       !checks_rotation_identity(&g_checks_rotation_runtime,&now))return 0;
    return r35_ready(&g_checks_rotation_proof,owner,g_get_tid(),
        __atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE),&now);
#else
    (void)owner;return 0;
#endif
}
/* Called only at the existing observer initialization/real hub boundary. */
static void checks_delivery_boundary(DWORD owner){
    if(!g_get_tid||g_get_tid()!=owner)return;
    if(g_checks_delivery.thread&&__atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)){
        (void)r26_reap(&g_checks_delivery,owner);return;
    }
    if(!checks_delivery_rotation_ready(owner)){
        if(g_checks_delivery.thread)(void)r26_request_stop(&g_checks_delivery,owner);return;
    }
    if(g_checks_delivery_attempted||!g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||!g_progress_native_ready)return;
    if(!__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE))return;
    g_checks_delivery_attempted=1;void*kernel=lid_find_module_ascii("kernel32.dll");R26Ops ops={0};
    if(!kernel)return;
    ops.open=g_win.CreateFileW;ops.read=g_win.ReadFile;ops.write=g_win.WriteFile;ops.close=g_win.CloseHandle;ops.remove=g_win.DeleteFileW;
    ops.flush=(BOOL (WINAPI *)(HANDLE))lid_get_export(kernel,"FlushFileBuffers");
    ops.replace=(BOOL (WINAPI *)(LPCWSTR,LPCWSTR,DWORD))lid_get_export(kernel,"MoveFileExW");
    ops.error=g_win.GetLastError;ops.thread=g_win.CreateThread;ops.thread_id=g_get_tid;ops.sleep=g_win.Sleep;
    ops.wait=(DWORD (WINAPI *)(HANDLE,DWORD))lid_get_export(kernel,"WaitForSingleObject");
    ops.clock=(unsigned long long (WINAPI *)(void))lid_get_export(kernel,"GetTickCount64");
#if LID_MATERIAL_CAPABILITY_ENABLED
    {
        void*bcrypt=lid_find_module_ascii("bcrypt.dll");
        g_checks_delivery.material_image=(uint64_t)(ULONG_PTR)g_progress_event_exe;
        g_checks_delivery.material_crypto.module_path=g_win.GetModuleFileNameW;
        g_checks_delivery.material_crypto.file_size=(__typeof__(g_checks_delivery.material_crypto.file_size))lid_get_export(kernel,"GetFileSizeEx");
#define LID_MATERIAL_CNG(field,name) g_checks_delivery.material_crypto.field=(__typeof__(g_checks_delivery.material_crypto.field))(bcrypt?lid_get_export(bcrypt,name):0)
        LID_MATERIAL_CNG(open_alg,"BCryptOpenAlgorithmProvider");
        LID_MATERIAL_CNG(property,"BCryptGetProperty");
        LID_MATERIAL_CNG(create_hash,"BCryptCreateHash");
        LID_MATERIAL_CNG(hash_data,"BCryptHashData");
        LID_MATERIAL_CNG(finish_hash,"BCryptFinishHash");
        LID_MATERIAL_CNG(destroy_hash,"BCryptDestroyHash");
        LID_MATERIAL_CNG(close_alg,"BCryptCloseAlgorithmProvider");
#undef LID_MATERIAL_CNG
    }
#endif
    if(!r26_start_from_host(&g_checks_delivery,&ops,g_progress_slot_directory,&g_progress_host,owner,1))
        log_ascii("AP CHECKS R27: native worker preparation refused; connection/items unchanged.");
    else log_ascii("AP CHECKS R35: native transport worker scheduled; receipt and server confirmation remain distinct.");
}
#endif
#endif
