#ifndef LID_PROGRESSION_RUNTIME_MUTEX_H
#define LID_PROGRESSION_RUNTIME_MUTEX_H
/* Cross-process writer ownership. Include winlite.h first. The host primes this
   before fresh NON_TITLE logs, owns it before GRANT and keeps it until process
   exit. Abandoned ownership is never released to another live writer. */
typedef struct LidRuntimeMutexOps {
    HANDLE (WINAPI *create)(void*,BOOL,const wchar_t*);
    DWORD (WINAPI *wait)(HANDLE,DWORD);
    DWORD (WINAPI *thread_id)(void);
} LidRuntimeMutexOps;
typedef struct LidRuntimeMutex {
    HANDLE handle;
    LidRuntimeMutexOps ops;
    DWORD owner_thread;
    int owned,poisoned;
} LidRuntimeMutex;
static int lid_runtime_mutex_name(unsigned pid,unsigned long long creation,wchar_t* out,size_t capacity){
    static const wchar_t prefix[]=L"Local\\LID_AP_CHARACTER_";
    static const wchar_t hex[]=L"0123456789ABCDEF";
    size_t i,n=0;int shift;
    if(!pid||!creation||!out||capacity<64)return 0;
    for(i=0;prefix[i];i++)out[n++]=prefix[i];
    for(shift=28;shift>=0;shift-=4)out[n++]=hex[(pid>>shift)&15];
    out[n++]='_';
    for(shift=60;shift>=0;shift-=4)out[n++]=hex[(creation>>shift)&15];
    out[n]=0;return 1;
}
static int lid_runtime_mutex_prime(LidRuntimeMutex* mutex,const LidRuntimeMutexOps* ops,
                                    unsigned pid,unsigned long long creation){
    wchar_t name[64];HANDLE handle;
    if(!mutex||!ops||!ops->create||!ops->wait||!ops->thread_id)return 0;
    if(mutex->handle)return !mutex->poisoned;
    if(!lid_runtime_mutex_name(pid,creation,name,64))return 0;
    handle=ops->create(0,0,name);if(!handle)return 0;
    mutex->handle=handle;mutex->ops=*ops;return 1;
}
/* 1=owned,0=another writer,-1=unsafe. No release routine by design: process
   exit owns cleanup. This is not one of the short engine critical sections. */
static int lid_runtime_mutex_try_own(LidRuntimeMutex* mutex){
    DWORD tid,result;
    if(!mutex||!mutex->handle||mutex->poisoned)return -1;
    tid=mutex->ops.thread_id();if(!tid)return -1;
    if(mutex->owned)return mutex->owner_thread==tid?1:-1;
    result=mutex->ops.wait(mutex->handle,0);
    if(result==0||result==0x80){
        mutex->owned=1;mutex->owner_thread=tid;
        if(result==0x80){mutex->poisoned=1;return -1;}
        return 1;
    }
    if(result==258)return 0;
    mutex->poisoned=1;return -1;
}
#endif
