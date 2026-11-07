#ifndef LID_LEVEL_UP_BLOODNIUM_POLICY_H
#define LID_LEVEL_UP_BLOODNIUM_POLICY_H
/* Limit-break leveling can use a zero Bloodnium row even at level one.
   Preserve native errors/vanilla behavior; never mutate the player's balance. */
static int lid_level_up_bloodnium_cost(int ap_active,int config_valid,int native_cost){
    return ap_active&&config_valid&&native_cost>=0 ? 999999999 : native_cost;
}
#endif
