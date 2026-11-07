#ifndef LID_STATUS_SCAN_FAST_H
#define LID_STATUS_SCAN_FAST_H
#include <stddef.h>
#ifndef LID_FAST_MASTER_SCAN_ENABLED
#define LID_FAST_MASTER_SCAN_ENABLED 0
#endif
/* Caller has already verified the whole writable memory region. No VM,
   UObject, thread, allocator, save or retained pointer in this pure kernel. */
typedef struct LidMasterScanTables {
    const int (*grade6)[10];const int (*grade1)[10];
    const int (*target6)[10];const int (*target1)[10];
    int required;unsigned long long*mask;
    void*context;void(*matched)(void*,int,int,int,void*);
} LidMasterScanTables;
unsigned lid_master_scan_region(void*base,size_t size,const LidMasterScanTables*tables);
#endif
