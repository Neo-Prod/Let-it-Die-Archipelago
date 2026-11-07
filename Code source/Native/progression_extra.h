#ifndef LID_PROGRESSION_EXTRA_H
#define LID_PROGRESSION_EXTRA_H
/* Optional extra domain for a common entry/save. No extension is attached by
   default. Prepare/commit/readback/refresh run inside the validated reservation;
   publish_saved runs after releasing engine locks; reset is memory-only. */
typedef struct LidProgressExtraOps {
    void* context;
    int (*prepare)(void*,const LidWireMessage*);
    int (*commit)(void*);
    int (*readback)(void*);
    int (*refresh)(void*,const LidWireMessage*);
    int (*publish_saved)(void*,const LidWireMessage*);
    void (*reset)(void*);
} LidProgressExtraOps;
#endif
