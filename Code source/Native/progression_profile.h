#ifndef LID_PROGRESSION_PROFILE_H
#define LID_PROGRESSION_PROFILE_H
/* Diagnostic counters only. No game pointers, mutation, or file API. */
enum {LID_PROFILE_FULL,LID_PROFILE_LOOKUP,LID_PROFILE_MASTER,LID_PROFILE_LOADED,
      LID_PROFILE_RAGE,LID_PROFILE_PROBE,LID_PROFILE_TRANSACTION,LID_PROFILE_COUNT};
typedef struct LidProfileStat {unsigned long long sum,max;unsigned count;} LidProfileStat;
typedef struct LidProfileOps {
    BOOL (WINAPI *now)(long long*);
    DWORD (WINAPI *thread)(void);
    void (*emit)(const char*);
} LidProfileOps;
typedef struct LidCharacterProfile {
    LidProfileOps ops;unsigned long long frequency,window_start;
    LidProfileStat stats[LID_PROFILE_COUNT];
    DWORD owner;unsigned windows;int entered,initialized;
} LidCharacterProfile;
static int lid_profile_init(LidCharacterProfile* p,const LidProfileOps* ops,long long frequency){
    if(!p||p->initialized||!ops||!ops->now||!ops->thread||!ops->emit||
       frequency<=0||frequency>1000000000000LL)return 0;
    p->ops=*ops;p->frequency=(unsigned long long)frequency;p->initialized=1;return 1;
}
static unsigned long long lid_profile_now(LidCharacterProfile* p){
    long long now;
    if(!p||!p->initialized||p->ops.thread()!=__atomic_load_n(&p->owner,__ATOMIC_ACQUIRE)||p->windows>=12||!p->entered||
       !p->ops.now(&now)||now<=0)return 0;
    return (unsigned long long)now;
}
static unsigned long long lid_profile_enter(LidCharacterProfile* p){
    DWORD expected=0,tid;unsigned long long now;
    if(!p||!p->initialized)return 0;
    tid=p->ops.thread();if(!tid)return 0;
    if(!__atomic_compare_exchange_n(&p->owner,&expected,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)&&expected!=tid)return 0;
    if(p->windows>=12||p->entered)return 0;
    p->entered=1;now=lid_profile_now(p);
    if(!now){p->entered=0;return 0;}
    if(!p->window_start)p->window_start=now;
    return now;
}
static void lid_profile_record(LidCharacterProfile* p,int category,unsigned long long start){
    unsigned long long now,duration;LidProfileStat* s;
    if(!start||category<0||category>=LID_PROFILE_COUNT||!(now=lid_profile_now(p))||now<start)return;
    duration=now-start;s=&p->stats[category];
    if(~0ULL-s->sum<duration)s->sum=~0ULL;else s->sum+=duration;
    if(duration>s->max)s->max=duration;
    if(s->count!=0xffffffffu)s->count++;
}
static unsigned lid_profile_us(const LidCharacterProfile* p,unsigned long long ticks){
    unsigned long long whole=ticks/p->frequency,remainder=ticks%p->frequency,result;
    if(whole>4294)return 0xffffffffu;
    result=whole*1000000ULL+remainder*1000000ULL/p->frequency;
    return result>0xffffffffu?0xffffffffu:(unsigned)result;
}
static int lid_profile_rearm(LidCharacterProfile* p){
    unsigned i;
    if(!p||!p->initialized||p->ops.thread()!=__atomic_load_n(&p->owner,__ATOMIC_ACQUIRE)||p->entered)return 0;
    p->windows=0;p->window_start=0;
    for(i=0;i<LID_PROFILE_COUNT;i++)p->stats[i]=(LidProfileStat){0};
    return 1;
}
static void lid_profile_leave(LidCharacterProfile* p,unsigned long long start){
    static const char* names[LID_PROFILE_COUNT]={"watcher","lookup","master_scan","loaded_levels","rage_sync","readiness_probe","transaction"};
    unsigned long long now;unsigned j;char line[196];size_t pos;
    if(!start||!p||!p->entered||p->ops.thread()!=p->owner)return;
    lid_profile_record(p,LID_PROFILE_FULL,start);now=lid_profile_now(p);
    if(now>=p->window_start&&now-p->window_start>=p->frequency*5){
        p->windows++;
        for(j=0;j<LID_PROFILE_COUNT;j++){
            pos=0;line[0]=0;
            append_ascii(line,sizeof(line),&pos,"AP PERF R16: window=");append_dec(line,sizeof(line),&pos,p->windows);
            append_ascii(line,sizeof(line),&pos," wall_us=");append_dec(line,sizeof(line),&pos,lid_profile_us(p,now-p->window_start));
            append_ascii(line,sizeof(line),&pos," section=");append_ascii(line,sizeof(line),&pos,names[j]);
            append_ascii(line,sizeof(line),&pos," count=");append_dec(line,sizeof(line),&pos,p->stats[j].count);
            append_ascii(line,sizeof(line),&pos," sum_us=");append_dec(line,sizeof(line),&pos,lid_profile_us(p,p->stats[j].sum));
            append_ascii(line,sizeof(line),&pos," max_us=");append_dec(line,sizeof(line),&pos,lid_profile_us(p,p->stats[j].max));
            line[pos]=0;p->ops.emit(line);
            p->stats[j]=(LidProfileStat){0};
        }
        p->window_start=now;
    }
    p->entered=0;
}
#endif
