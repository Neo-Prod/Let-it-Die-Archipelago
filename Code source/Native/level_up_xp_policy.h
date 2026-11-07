#ifndef LID_LEVEL_UP_XP_POLICY_H
#define LID_LEVEL_UP_XP_POLICY_H

/* Preserve missing/invalid prices and vanilla mode; no stat or save mutation. */
static int lid_level_up_xp_cost(int ap_active,int config_valid,int native_cost){
    return ap_active&&config_valid&&native_cost>0 ? 9999999 : native_cost;
}

#endif
