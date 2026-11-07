#ifndef LID_RELOAD_HANDOFF_RESEARCH_H
#define LID_RELOAD_HANDOFF_RESEARCH_H
/* Research candidate. Include actual progression_host.h. Caller serializes
   access on the game thread; no engine write/target/freeze/save/ACK here. */
typedef struct LidReloadHandoffOps {
    void* context;
    unsigned long long (*clock)(void*);
    unsigned (*thread)(void*);
} LidReloadHandoffOps;
typedef struct LidReloadHandoff {
    LidCharacterHost*host;LidReloadHandoffOps ops;LidWireMessage offer;
    BYTE config[32];unsigned owner,initialized;
    int terminal,last_result;
    unsigned long long start,last,budget,interval;
} LidReloadHandoff;
static int lid_reload_handoff_init(LidReloadHandoff*s,LidCharacterHost*h,const LidReloadHandoffOps*ops,
                                   unsigned long long budget,unsigned long long interval){
    unsigned tid;unsigned long long now;
    if(!s||s->initialized||!h||!h->initialized||!ops||!ops->clock||!ops->thread||
       !budget||budget>120000||!interval||interval>budget)return 0;
    tid=ops->thread(ops->context);if(!tid)return 0;now=ops->clock(ops->context);
    *s=(LidReloadHandoff){0};s->host=h;s->ops=*ops;s->offer=h->peer.offer;
    lid_wire_copy(s->config,h->expected_config,32);s->owner=tid;s->start=s->last=now;
    s->budget=budget;s->interval=interval;s->initialized=1;return 1;
}
/* 1=ownership granted,0=bounded wait,-1=terminal refusal.
   No clock/thread/file callbacks after a terminal result. */
static int lid_reload_handoff_tick(LidReloadHandoff*s){
    LidCharacterHost*h;unsigned long long now;int result;
    if(!s||!s->initialized)return -1;
    if(s->terminal)return s->terminal;
    h=s->host;
    if(s->ops.thread(s->ops.context)!=s->owner||!h||!h->initialized||h->poisoned||h->active||
       !lid_wire_same_session(&s->offer,&h->peer.offer)||!lid_wire_equal(s->config,h->expected_config,32))return s->terminal=-1;
    if(h->grant_published){
        if(!h->peer.bound||!h->peer.granted)return s->terminal=-1;
        return s->terminal=1;
    }
    now=s->ops.clock(s->ops.context);
    if(now<s->last||now<s->start||now-s->start>=s->budget)return s->terminal=-1;
    /* First publication may run immediately; later bridge IO is rate limited. */
    if(s->last_result!=0&&now-s->last<s->interval)return 0;
    if(s->last_result==0&&now==s->start&&h->offer_published)return 0;
    s->last=now;
    result=lid_host_step(h,1);s->last_result=result?result:99;
    if(result==-2)return s->terminal=-1;
    if(h->grant_published){
        if(!h->peer.bound||!h->peer.granted)return s->terminal=-1;
        return s->terminal=1;
    }
    /* -1 can be an old packet or transient publication failure: keep a
       bounded, throttled retry. Never fall through to target-file reading. */
    return 0;
}
#endif
