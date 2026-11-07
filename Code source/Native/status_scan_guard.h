#ifndef LID_STATUS_SCAN_GUARD_H
#define LID_STATUS_SCAN_GUARD_H
#include <stdint.h>
#include "status_scan_fast.h"
#ifndef LID_MASTER_SCAN_GUARD_ENABLED
#define LID_MASTER_SCAN_GUARD_ENABLED 0
#endif
/* Exact unchanged fast COFF function size; must be checked by final builder. */
#define LID_MASTER_SCAN_KERNEL_BYTES 2099u
typedef struct LidMasterScanGuardResult {
    unsigned matches, read_faults, handler_missing;
} LidMasterScanGuardResult;
static int lid_scan_guard_accept(uint32_t code,uintptr_t operation,
    uintptr_t pc,uintptr_t fault,uintptr_t kernel,size_t kernel_size,
    uintptr_t region,size_t region_size) {
    return code==0xc0000005u && operation==0 && kernel_size && region_size &&
        kernel<=UINTPTR_MAX-kernel_size && region<=UINTPTR_MAX-region_size &&
        pc>=kernel && pc-kernel<kernel_size &&
        fault>=region && fault-region<region_size;
}
int lid_master_scan_guard_init(void *handler);
LidMasterScanGuardResult lid_master_scan_guarded(void *base,size_t size,
    const LidMasterScanTables *tables);
#endif
