#ifndef LID_SHOP_TOWER_EVENT_PROBE_H
#define LID_SHOP_TOWER_EVENT_PROBE_H
#include <stdint.h>
/* Each real event stage is recorded once per module lifetime. No IO, waits,
   native calls or pointer retention here. Owner drains after publication too. */
enum {LID_TOWER_PROBE_CHANNELS=16};
static unsigned g_r299_events[LID_TOWER_PROBE_CHANNELS][64];
static unsigned g_r299_details[LID_TOWER_PROBE_CHANNELS][64];
static void r299_note(unsigned channel,unsigned stage,unsigned detail){
    if(channel>=LID_TOWER_PROBE_CHANNELS||stage>=64)return;
    unsigned empty=0;
    if(__atomic_compare_exchange_n(&g_r299_events[channel][stage],&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
        g_r299_details[channel][stage]=detail;
        __atomic_store_n(&g_r299_events[channel][stage],2,__ATOMIC_RELEASE);
    }
}
static int r299_flag(unsigned channel,unsigned stage,int value){
    r299_note(channel,stage+(value!=0),0);return value;
}
#endif
