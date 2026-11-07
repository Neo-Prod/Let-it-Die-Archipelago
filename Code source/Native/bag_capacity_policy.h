#ifndef LID_BAG_CAPACITY_POLICY_R40_H
#define LID_BAG_CAPACITY_POLICY_R40_H
/* An index, NOT a count; retain native negative refusals. */
static int r40_bag_space(int original,int capacity,int occupied_floor){
    if(capacity<1||capacity>99)return original;
    if(original<0)return original;
    if(occupied_floor<0||occupied_floor>capacity)return -1;
    return original>=capacity?-1:original;
}
/* Report logical capacity; expose existing overflow only for removal. */
static int r45_bag_size(int original,int capacity,int occupied_floor){
    if(original<0||capacity<1||capacity>99||occupied_floor<0||occupied_floor>99)return original;
    return occupied_floor>capacity?occupied_floor:capacity;
}
#endif
