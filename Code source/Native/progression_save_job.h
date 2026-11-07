#ifndef LID_PROGRESSION_SAVE_JOB_H
#define LID_PROGRESSION_SAVE_JOB_H

/* Candidate, not wired into mod.c. Closed ABI for the verified UE3 runnable.
   See Research/Reports/CHARACTER_PROGRESSIVES_NATIVE_PLAN_R8.md.
   Include winlite.h first. Job/ops must outlive ALL callable copies.
   The game-thread host must reserve request+save work before starting and must
   hold the save-runner lock when polling thread inactivity. This file DOES NOT
   implement those guards or claim that received items have reached a save. */

typedef struct LidProgressSaveJob LidProgressSaveJob;
typedef struct LidProgressSaveCallable LidProgressSaveCallable;
typedef struct LidProgressSaveBox LidProgressSaveBox;

typedef struct LidProgressSaveOps {
    void* (WINAPI *allocate)(size_t);
    void (WINAPI *release)(void*);
    void (WINAPI *mark_dirty)(void);
    BYTE (WINAPI *save)(void* runner,void* optional_json);
    BYTE (WINAPI *start)(void* runner,LidProgressSaveBox*,const wchar_t* name);
} LidProgressSaveOps;

typedef struct LidProgressSaveVtable {
    LidProgressSaveCallable* (WINAPI *copy)(LidProgressSaveCallable*,void*);
    LidProgressSaveCallable* (WINAPI *move)(LidProgressSaveCallable*,void*);
    BYTE (WINAPI *invoke)(LidProgressSaveCallable*);
    void* (WINAPI *target_type)(LidProgressSaveCallable*);
    void (WINAPI *destroy)(LidProgressSaveCallable*,BYTE free_storage);
    void (WINAPI *destroy_inner)(LidProgressSaveCallable*,BYTE);
    void* (WINAPI *target)(LidProgressSaveCallable*,void*);
} LidProgressSaveVtable;

struct __attribute__((packed,aligned(4))) LidProgressSaveCallable {
    const LidProgressSaveVtable* vtable;
    LidProgressSaveJob* job;
    unsigned ticket;
    unsigned reserved;
};

struct __attribute__((packed,aligned(4))) LidProgressSaveBox {
    LidProgressSaveCallable storage;
    LidProgressSaveCallable* object;
};

struct LidProgressSaveJob {
    LidProgressSaveOps ops;
    void* runner;
    unsigned long long batch;
    unsigned ticket;
    unsigned active;
    /* 0 awaiting callback, -1 executing, 1 success, 2 failure. */
    int completion;
};

typedef char LidSaveCallableSize[(sizeof(LidProgressSaveCallable)==24)?1:-1];
typedef char LidSaveBoxSize[(sizeof(LidProgressSaveBox)==32)?1:-1];
typedef char LidSaveBoxPointerOffset[(__builtin_offsetof(LidProgressSaveBox,object)==24)?1:-1];

static LidProgressSaveCallable* WINAPI lid_progress_save_copy(LidProgressSaveCallable* source,void* destination){
    LidProgressSaveCallable* out=(LidProgressSaveCallable*)destination;
    if(!source||!source->job)return 0;
    if(!out)out=(LidProgressSaveCallable*)source->job->ops.allocate(sizeof(*out));
    if(!out)return 0;
    *out=*source;
    return out;
}

static BYTE WINAPI lid_progress_save_invoke(LidProgressSaveCallable* self){
    LidProgressSaveJob* job;
    int expected=0;
    BYTE result;
    if(!self||!(job=self->job)||!job->active||job->ticket!=self->ticket)return 0;
    /* Single invocation, even if the same callable has been copied. */
    if(!__atomic_compare_exchange_n(&job->completion,&expected,-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    job->ops.mark_dirty();
    result=job->ops.save(job->runner,0);
    __atomic_store_n(&job->completion,result?1:2,__ATOMIC_RELEASE);
    return result?1:0;
}

static void* WINAPI lid_progress_save_target_type(LidProgressSaveCallable* self){
    /* No RTTI lookup is used by the closed runnable/copy/destruction path. */
    (void)self;return 0;
}

static void WINAPI lid_progress_save_destroy(LidProgressSaveCallable* self,BYTE free_storage){
    LidProgressSaveJob* job;
    if(!self)return;
    job=self->job;
    if(free_storage&&job)job->ops.release(self);
}

static void WINAPI lid_progress_save_destroy_inner(LidProgressSaveCallable* self,BYTE unused){
    (void)self;(void)unused;
}

static void* WINAPI lid_progress_save_target(LidProgressSaveCallable* self,void* unused){
    (void)self;(void)unused;return 0;
}

static const LidProgressSaveVtable lid_progress_save_vtable={
    lid_progress_save_copy,lid_progress_save_copy,lid_progress_save_invoke,
    lid_progress_save_target_type,lid_progress_save_destroy,
    lid_progress_save_destroy_inner,lid_progress_save_target
};

/* Returns our new ticket or0 on rejection. A rejected attempt still consumes a
   ticket, so a late copy from that attempt cannot match a subsequent attempt. */
static unsigned lid_progress_save_begin(LidProgressSaveJob* job,void* runner,
                                        unsigned long long batch){
    LidProgressSaveBox box;
    unsigned ticket;
    if(!job||!runner||!batch||job->active||job->ticket==0xffffffffu)return 0;
    if(!job->ops.allocate||!job->ops.release||!job->ops.mark_dirty||
       !job->ops.save||!job->ops.start)return 0;
    ticket=job->ticket+1;
    job->ticket=ticket;job->runner=runner;job->batch=batch;
    __atomic_store_n(&job->completion,0,__ATOMIC_RELEASE);
    job->active=1;
    box.storage.vtable=&lid_progress_save_vtable;
    box.storage.job=job;box.storage.ticket=ticket;box.storage.reserved=0;
    box.object=&box.storage;
    if(!job->ops.start(runner,&box,0)){
        job->active=0;
        return 0;
    }
    return ticket;
}

/* NOT_OURS=-2, PENDING=0, SAVE_OK=1, SAVE_FAILED=2.
   The supplied active-thread observation MUST be taken under runner+18 lock.
   SAVE_OK means the serializer returned true for this attempt; final readback
   and acknowledgement belong to the host, not this job helper. */
static int lid_progress_save_poll(LidProgressSaveJob* job,unsigned ticket,
                                 unsigned long long batch,int thread_active){
    int completion;
    if(!job||!job->active||!ticket||ticket!=job->ticket||batch!=job->batch)return -2;
    if(thread_active)return 0;
    completion=__atomic_load_n(&job->completion,__ATOMIC_ACQUIRE);
    return completion>0?completion:0;
}

static int lid_progress_save_finish(LidProgressSaveJob* job,unsigned ticket,
                                   unsigned long long batch,int thread_active){
    int result=lid_progress_save_poll(job,ticket,batch,thread_active);
    if(result==1||result==2)job->active=0;
    return result;
}

#endif
