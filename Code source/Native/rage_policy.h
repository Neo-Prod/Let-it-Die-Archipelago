#ifndef LID_RAGE_POLICY_H
#define LID_RAGE_POLICY_H

/* Pure policy: only the supplied Rage fields are touched. No game/API calls. */
static int lid_rage_apply(int target,int*base,int*total,int*maximum,int*stock,float*fraction){
    int bounded_stock,changed;
    float bounded_fraction;
    if(target<0)target=0;
    bounded_stock=*stock;
    if(bounded_stock<0)bounded_stock=0;
    if(bounded_stock>target)bounded_stock=target;
    bounded_fraction=*fraction;
    if(target==0||bounded_stock==target||!(bounded_fraction>=0.0f&&bounded_fraction<1.0f))
        bounded_fraction=0.0f;
    changed=(*base!=target||*total!=target||*maximum!=target||*stock!=bounded_stock||*fraction!=bounded_fraction);
    *base=target;
    *total=target;
    *maximum=target;
    *stock=bounded_stock;
    *fraction=bounded_fraction;
    return changed;
}
#endif
