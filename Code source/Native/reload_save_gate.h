#ifndef LID_RELOAD_SAVE_GATE_H
#define LID_RELOAD_SAVE_GATE_H
/* Atomic admission gate for a native SaveFn
   hook, NOT a save writer, loader or proof of persistence. Caller still checks
   exact module/session/profiles and serialized request ownership. No runner
   critical section may be held across native Load/project/original core.
   Every successful save_enter requires exactly one save_leave, all paths. */
enum {LID_RELOAD_SAVE_BYPASS=0,LID_RELOAD_SAVE_ARMED=1,LID_RELOAD_SAVE_LOADING=2,
      LID_RELOAD_SAVE_ACCEPTED=3,LID_RELOAD_SAVE_FAILED=4};
typedef struct LidReloadSaveGate {unsigned phase,in_flight;} LidReloadSaveGate;
static unsigned lid_reload_save_phase(const LidReloadSaveGate*g){return __atomic_load_n(&g->phase,__ATOMIC_ACQUIRE);}
static unsigned lid_reload_save_active(const LidReloadSaveGate*g){return __atomic_load_n(&g->in_flight,__ATOMIC_ACQUIRE);}
static int lid_reload_save_permitted(unsigned p){return p==LID_RELOAD_SAVE_BYPASS||p==LID_RELOAD_SAVE_ACCEPTED;}
static int lid_reload_save_enter(LidReloadSaveGate*g){
    unsigned n;
    if(!g||!lid_reload_save_permitted(lid_reload_save_phase(g)))return 0;
    n=__atomic_add_fetch(&g->in_flight,1,__ATOMIC_ACQ_REL);
    if(n>64||!lid_reload_save_permitted(lid_reload_save_phase(g))){
        __atomic_sub_fetch(&g->in_flight,1,__ATOMIC_ACQ_REL);return 0;
    }
    return 1;
}
static int lid_reload_save_leave(LidReloadSaveGate*g){
    unsigned n;
    if(!g)return 0;n=lid_reload_save_active(g);
    while(n){if(__atomic_compare_exchange_n(&g->in_flight,&n,n-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 1;}
    return 0;
}
/* Once armed, no new original SaveFn body is admitted. An already admitted
   save may finish; begin_load refuses until it has left. No busy wait here. */
static int lid_reload_save_arm(LidReloadSaveGate*g){
    unsigned expected=LID_RELOAD_SAVE_BYPASS;
    return g&&__atomic_compare_exchange_n(&g->phase,&expected,LID_RELOAD_SAVE_ARMED,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static int lid_reload_save_begin_load(LidReloadSaveGate*g){
    unsigned expected=LID_RELOAD_SAVE_ARMED;
    if(!g||lid_reload_save_active(g))return 0;
    if(!__atomic_compare_exchange_n(&g->phase,&expected,LID_RELOAD_SAVE_LOADING,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    if(lid_reload_save_active(g)){
        expected=LID_RELOAD_SAVE_LOADING;
        (void)__atomic_compare_exchange_n(&g->phase,&expected,LID_RELOAD_SAVE_ARMED,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
        return 0;
    }
    return 1;
}
static int lid_reload_save_accept(LidReloadSaveGate*g){
    unsigned expected=LID_RELOAD_SAVE_LOADING;
    return g&&__atomic_compare_exchange_n(&g->phase,&expected,LID_RELOAD_SAVE_ACCEPTED,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void lid_reload_save_fail(LidReloadSaveGate*g){if(g)__atomic_store_n(&g->phase,LID_RELOAD_SAVE_FAILED,__ATOMIC_RELEASE);}
#endif
