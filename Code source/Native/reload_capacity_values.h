#ifndef LID_RELOAD_CAPACITY_VALUES_H
#define LID_RELOAD_CAPACITY_VALUES_H
#ifndef LID_NATIVE_RELOAD_ENABLED
#define LID_NATIVE_RELOAD_ENABLED 0
#endif
/* Owned scalar facts only; no RPC strings, arrays or equipment pointers. */
typedef struct LidReloadCapacityValues {
    int skill_level,bag_level,rage_level,skill_floor,bag_floor;
} LidReloadCapacityValues;
#if LID_NATIVE_RELOAD_ENABLED
/* 0 normal live path, 1 immutable restored facts, -1 refuse passive mutation. */
static int progress_reload_capacity(LidReloadCapacityValues*);
static void progress_reload_fail(const char*);
#endif
#endif
