#ifndef LID_PLAYER_STAT_VIEWS_H
#define LID_PLAYER_STAT_VIEWS_H
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include "progression_config.h"

/* Output-only views. Caller must prove owned scope, BAL key, readable/writable
 * destination, and that the native getter already succeeded. No global table,
 * level counter, equipment entry or persistent model is changed here. */
static int32_t lid_ps_i32(const unsigned char *p){int32_t v;__builtin_memcpy(&v,p,4);return v;}
static int64_t lid_ps_i64(const unsigned char *p){int64_t v;__builtin_memcpy(&v,p,8);return v;}
static void lid_ps_w32(unsigned char *p,int v){int32_t n=v;__builtin_memcpy(p,&n,4);}
static void lid_ps_w64(unsigned char *p,int v){int64_t n=v;__builtin_memcpy(p,&n,8);}

/* Preserve legacy AP progression-row semantics exactly: the six statistics
 * start at index1, capacities at0. Recovery is always the native value. A row
 * is not a final player's value and is therefore not clamped to each cap. */
static int lid_ps_row_values(int allowed,const LidCharacterConfig *c,int level,int grade,int result[10]){
    static const unsigned order[9]={0,4,1,2,3,6,8,7,9};int values[10]={0};
    if(allowed!=1||!c||!result||!lid_config_valid(c)||level<1||level>45||
       (grade!=1&&grade!=6)||(grade==1&&level>5))return 0;
    for(unsigned i=0;i<9;i++){
        int64_t value=(int64_t)c->tracks[i][0]+(int64_t)c->tracks[i][1]*(i<6?level-1:level);
        if(value<0||value>INT_MAX)return 0;values[order[i]]=(int)value;
    }
    for(unsigned i=0;i<10;i++)result[i]=values[i];return 1;
}
static int lid_ps_view_ue_row(int allowed,const LidCharacterConfig *c,int level,int grade,
                              void *record,size_t bytes,int native_found){
    unsigned char *p=record;int values[10];
    if(!p||bytes<0x44||native_found!=1||lid_ps_i32(p)!=level||lid_ps_i32(p+0x14)!=grade||
       !lid_ps_row_values(allowed,c,level,grade,values))return 0;
    for(unsigned i=0;i<10;i++)if(i!=5)lid_ps_w32(p+0x1c+4*i,values[i]);
    return 1;
}
static int lid_ps_view_native_row(int allowed,const LidCharacterConfig *c,int level,int grade,
                                  void *record,size_t bytes){
    unsigned char *p=record;int values[10];
    if(!p||bytes<0x81||p[0x80]!=1||lid_ps_i64(p+8)!=level||lid_ps_i64(p+0x20)!=grade||
       !lid_ps_row_values(allowed,c,level,grade,values))return 0;
    for(unsigned i=0;i<10;i++)if(i!=5)lid_ps_w64(p+0x30+8*i,values[i]);
    return 1;
}
/* BrgCharaStatus BEFORE equipment/decal SimulationStatus is accumulated.
 * Keep type, grade, level, stmrecov and every non-AP byte. These six writes
 * must not be applied to final TotalStatus, which would erase native bonuses. */
static int lid_ps_view_body_status(int allowed,const LidCharacterConfig *c,const int levels[10],
                                   void *status,size_t bytes){
    static const unsigned index[9]={1,5,2,3,4,6,8,7,9};
    static const unsigned offset[6]={0x24,0x28,0x14,0x18,0x1c,0x20};
    unsigned counts[9];int values[9],native_levels[10];unsigned char *p=status;
    if(allowed!=1||!c||!levels||!p||bytes<0x34||levels[0]<1)return 0;
    for(unsigned i=0;i<9;i++){
        int min=i<6?1:0,lv=levels[index[i]];
        if(lv<min||lv>min+44)return 0;counts[i]=(unsigned)(lv-min);
    }
    if(!lid_config_targets(c,counts,values,native_levels))return 0;
    for(unsigned i=0;i<6;i++)lid_ps_w32(p+offset[i],values[i]);
    lid_ps_w32(p+0x30,values[8]);return 1;
}
/* Backend BodyDetail represents the old MASTER FILE policy, not the absolute
 * UE GetBodyDetail view. Only BAL/G6/LB0 used to be changed in that file.
 * The string is produced separately so the runtime can use UE-owned memory. */
typedef struct LidPSDetailValues {
    int max_levels,bag,rage;uint16_t skills[184];unsigned skill_units;
} LidPSDetailValues;
static int lid_ps_detail_values(int allowed,const LidCharacterConfig *c,int max_levels,
                                int grade,int limit_break,LidPSDetailValues *out){
    LidPSDetailValues v={0};unsigned n=0;int slots;
    if(allowed!=1||!out||!c||!lid_config_valid(c)||grade!=6||limit_break!=0||
       max_levels<1||max_levels>45)return 0;
    slots=c->tracks[7][0];if(slots<0||slots>64)return 0;
    for(int i=1;i<=slots;i++){
        if(i>1)v.skills[n++]=',';
        if(i>=10)v.skills[n++]=(uint16_t)('0'+i/10);
        v.skills[n++]=(uint16_t)('0'+i%10);
    }
    v.skills[n]=0;v.skill_units=n?n+1:0;v.max_levels=max_levels;
    v.bag=c->tracks[6][0];v.rage=c->tracks[8][0];*out=v;return 1;
}
#endif
