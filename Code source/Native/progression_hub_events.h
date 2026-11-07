#ifndef LID_PROGRESSION_HUB_EVENTS_H
#define LID_PROGRESSION_HUB_EVENTS_H
/* Area-set notifications only; no polling, target receipt, mutation or save. */
enum {LID_HUB_NO_EVENT=0,LID_HUB_INITIAL=1,LID_HUB_RETURN=2,LID_HUB_LEFT=3};
typedef struct LidHubEvents {DWORD owner;unsigned serial;int initial_seen,outside;} LidHubEvents;
static int lid_hub_event(LidHubEvents* e,DWORD tid,int area_empty){
    DWORD expected=0;int result;
    if(!e||!tid||(area_empty!=0&&area_empty!=1))return LID_HUB_NO_EVENT;
    if(!__atomic_compare_exchange_n(&e->owner,&expected,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)&&expected!=tid)return LID_HUB_NO_EVENT;
    if(!area_empty){
        if(e->outside)return LID_HUB_NO_EVENT;
        e->outside=1;return LID_HUB_LEFT;
    }
    if(e->initial_seen&&!e->outside)return LID_HUB_NO_EVENT;
    if(e->serial==0xffffffffu)return LID_HUB_NO_EVENT;
    result=e->initial_seen?LID_HUB_RETURN:LID_HUB_INITIAL;
    e->initial_seen=1;e->outside=0;e->serial++;return result;
}
#endif
