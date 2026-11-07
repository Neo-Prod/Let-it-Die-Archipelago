#ifndef LID_PERFORMANCE_EVENTS_H
#define LID_PERFORMANCE_EVENTS_H
#ifndef LID_EVENT_PROFILE_ENABLED
#define LID_EVENT_PROFILE_ENABLED 0
#endif
#if LID_EVENT_PROFILE_ENABLED
typedef struct LidPerfMark {unsigned long long wall,cpu;unsigned thread,valid,cpu_valid;} LidPerfMark;
typedef struct LidPerfEvents {
    unsigned long long (WINAPI *clock)(void);
    unsigned (WINAPI *thread)(void);
    BOOL (WINAPI *times)(HANDLE,void*,void*,void*,void*);
    int state;unsigned counts[48];
    struct {unsigned long long calls,wall,cpu,cpu_calls,max_wall; const char*label;} total[48];
    unsigned detail_thread,summary_count;unsigned long long detail[8];
} LidPerfEvents;
static LidPerfEvents g_perf_events;
static int lid_perf_prepare(void){
    int expected=0;void*kernel;
    int state=__atomic_load_n(&g_perf_events.state,__ATOMIC_ACQUIRE);if(state)return state==1;
    if(!__atomic_compare_exchange_n(&g_perf_events.state,&expected,-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    kernel=lid_find_module_ascii("kernel32.dll");
    if(kernel){g_perf_events.clock=(void*)lid_get_export(kernel,"GetTickCount64");
        g_perf_events.thread=(void*)lid_get_export(kernel,"GetCurrentThreadId");
        g_perf_events.times=(void*)lid_get_export(kernel,"GetThreadTimes");}
    __atomic_store_n(&g_perf_events.state,g_perf_events.clock&&g_perf_events.thread?1:-2,__ATOMIC_RELEASE);
    return __atomic_load_n(&g_perf_events.state,__ATOMIC_ACQUIRE)==1;
}
static LidPerfMark lid_perf_mark(void){
    LidPerfMark mark={0};unsigned long long created,exited,kernel,user;
    if(!lid_perf_prepare())return mark;
    mark.thread=g_perf_events.thread();if(!mark.thread)return mark;
    mark.wall=g_perf_events.clock();mark.valid=1;
    if(g_perf_events.times&&g_perf_events.times((HANDLE)(LONG_PTR)-2,&created,&exited,&kernel,&user)){
        mark.cpu=kernel+user;mark.cpu_valid=1;}
    return mark;
}

static void lid_perf_u64(char*out,size_t cap,size_t*pos,unsigned long long n){
    char digits[24];unsigned k=0;if(!n){append_ascii(out,cap,pos,"0");return;}
    while(n){digits[k++]=(char)('0'+n%10);n/=10;}
    while(k&&*pos+1<cap)out[(*pos)++]=digits[--k];out[*pos]=0;
}
static void lid_perf_detail_begin(void){
    if(lid_perf_prepare())__atomic_store_n(&g_perf_events.detail_thread,g_perf_events.thread(),__ATOMIC_RELEASE);
}
static void lid_perf_detail_count(unsigned id,unsigned long long amount){
    unsigned owner=__atomic_load_n(&g_perf_events.detail_thread,__ATOMIC_ACQUIRE);
    if(id<8&&owner&&g_perf_events.thread&&g_perf_events.thread()==owner)
        __atomic_fetch_add(&g_perf_events.detail[id],amount,__ATOMIC_RELAXED);
}
static void lid_perf_detail_stop(void){__atomic_store_n(&g_perf_events.detail_thread,0,__ATOMIC_RELEASE);}
static void lid_perf_summary(const char*boundary){
    unsigned i;size_t p;char line[384];
    if(__atomic_fetch_add(&g_perf_events.summary_count,1,__ATOMIC_RELAXED)>=16)return;
    for(i=0;i<48;i++){
        unsigned long long calls=__atomic_load_n(&g_perf_events.total[i].calls,__ATOMIC_RELAXED);
        const char*label=__atomic_load_n(&g_perf_events.total[i].label,__ATOMIC_RELAXED);
        if(!calls||!label)continue;
        p=0;append_ascii(line,sizeof(line),&p,"AP PERF R26AY TOTAL: boundary=");append_ascii(line,sizeof(line),&p,boundary);
        append_ascii(line,sizeof(line),&p," section=");append_ascii(line,sizeof(line),&p,label);
        append_ascii(line,sizeof(line),&p," calls=");lid_perf_u64(line,sizeof(line),&p,calls);
        append_ascii(line,sizeof(line),&p," wall_ms=");lid_perf_u64(line,sizeof(line),&p,__atomic_load_n(&g_perf_events.total[i].wall,__ATOMIC_RELAXED));
        append_ascii(line,sizeof(line),&p," max_ms=");lid_perf_u64(line,sizeof(line),&p,__atomic_load_n(&g_perf_events.total[i].max_wall,__ATOMIC_RELAXED));
        append_ascii(line,sizeof(line),&p," cpu_samples=");lid_perf_u64(line,sizeof(line),&p,__atomic_load_n(&g_perf_events.total[i].cpu_calls,__ATOMIC_RELAXED));
        append_ascii(line,sizeof(line),&p," cpu_ms=");lid_perf_u64(line,sizeof(line),&p,__atomic_load_n(&g_perf_events.total[i].cpu,__ATOMIC_RELAXED)/10000);
        log_ascii(line);
    }
    p=0;append_ascii(line,sizeof(line),&p,"AP PERF R26AY COUNTERS: boundary=");append_ascii(line,sizeof(line),&p,boundary);
    {const char*labels[8]={" readable_queries="," name_comparisons="," visited_fields="," scan_queries="," scanned_regions="," scanned_bytes="," scope_hits="," scope_misses="};
    for(i=0;i<8;i++){append_ascii(line,sizeof(line),&p,labels[i]);lid_perf_u64(line,sizeof(line),&p,__atomic_load_n(&g_perf_events.detail[i],__ATOMIC_RELAXED));}}
    log_ascii(line);
}

static unsigned lid_perf_clamp(unsigned long long n){return n>0xffffffffu?0xffffffffu:(unsigned)n;}
static void lid_perf_end(LidPerfMark start,unsigned category,const char*label,int result){
    LidPerfMark end;unsigned n;size_t pos=0;char line[256];
    if(!start.valid||category>=48||!label)return;
    end=lid_perf_mark();if(!end.valid||end.thread!=start.thread||end.wall<start.wall)return;

    {unsigned long long wall=end.wall-start.wall,old;const char*nil=0;
    __atomic_compare_exchange_n(&g_perf_events.total[category].label,&nil,label,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED);
    __atomic_fetch_add(&g_perf_events.total[category].calls,1,__ATOMIC_RELAXED);
    __atomic_fetch_add(&g_perf_events.total[category].wall,wall,__ATOMIC_RELAXED);
    old=__atomic_load_n(&g_perf_events.total[category].max_wall,__ATOMIC_RELAXED);
    while(old<wall&&!__atomic_compare_exchange_n(&g_perf_events.total[category].max_wall,&old,wall,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
    if(start.cpu_valid&&end.cpu_valid&&end.cpu>=start.cpu){
        __atomic_fetch_add(&g_perf_events.total[category].cpu,end.cpu-start.cpu,__ATOMIC_RELAXED);
        __atomic_fetch_add(&g_perf_events.total[category].cpu_calls,1,__ATOMIC_RELAXED);}}
    /* Pickup instrumentation is summary-only: no file IO on the pickup path. */
    n=__atomic_fetch_add(&g_perf_events.counts[category],1,__ATOMIC_RELAXED);if(n>=16||category>=38)return;
    append_ascii(line,sizeof(line),&pos,"AP PERF R26AQ: section=");append_ascii(line,sizeof(line),&pos,label);
    append_ascii(line,sizeof(line),&pos," tid=");append_dec(line,sizeof(line),&pos,start.thread);
    append_ascii(line,sizeof(line),&pos," start_ms_low32=");append_dec(line,sizeof(line),&pos,(unsigned)start.wall);
    append_ascii(line,sizeof(line),&pos," elapsed_ms=");append_dec(line,sizeof(line),&pos,lid_perf_clamp(end.wall-start.wall));
    append_ascii(line,sizeof(line),&pos," cpu_valid=");
    append_dec(line,sizeof(line),&pos,start.cpu_valid&&end.cpu_valid&&end.cpu>=start.cpu);
    if(start.cpu_valid&&end.cpu_valid&&end.cpu>=start.cpu){
        append_ascii(line,sizeof(line),&pos," cpu_ms=");append_dec(line,sizeof(line),&pos,lid_perf_clamp((end.cpu-start.cpu)/10000));}
    append_ascii(line,sizeof(line),&pos," result_u32=");append_dec(line,sizeof(line),&pos,(unsigned)result);
    line[pos]=0;log_ascii(line);
}
#define LID_PERF_DETAIL(id,n) lid_perf_detail_count(id,n)
#define LID_PERF_DETAIL_BEGIN() lid_perf_detail_begin()
#define LID_PERF_DETAIL_STOP() lid_perf_detail_stop()
#define LID_PERF_SUMMARY(label) lid_perf_summary(label)
#define LID_PERF_BEGIN(name) LidPerfMark name=lid_perf_mark()
#define LID_PERF_END(name,category,label,result) lid_perf_end(name,category,label,result)
#else
#define LID_PERF_DETAIL(id,n) ((void)0)
#define LID_PERF_DETAIL_BEGIN() ((void)0)
#define LID_PERF_DETAIL_STOP() ((void)0)
#define LID_PERF_SUMMARY(label) ((void)0)
#define LID_PERF_BEGIN(name) ((void)0)
#define LID_PERF_END(name,category,label,result) ((void)0)
#endif
#endif
