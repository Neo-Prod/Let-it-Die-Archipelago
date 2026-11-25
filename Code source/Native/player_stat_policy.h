#ifndef LID_PLAYER_STAT_POLICY_H
#define LID_PLAYER_STAT_POLICY_H
/* Pure ownership and nine-track calculation policy. Native adapters must prove
 * identity facts before using this policy. No actor, table or save writes. */
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

enum LidPSRole { LID_PS_UNKNOWN=0, LID_PS_PLAYER=1, LID_PS_OWN_UI=2,
    LID_PS_OWN_SERVICE=3, LID_PS_SCREAMER=4, LID_PS_HATER=5,
    /* Strong subject proof: ONLY the current owned Pawn's mBodyStatus output.
       A player hit by an enemy remains the subject of its own recalculation. */
    LID_PS_PLAYER_OUTPUT=6 };
typedef struct LidPSIdentity {
    uint64_t session, expected_session;
    unsigned local_uid, requested_uid;
    const uint16_t *owned_cid, *requested_cid;
    size_t owned_units, requested_units;
    enum LidPSRole role;
    unsigned ap_active, verified_owned_record;
} LidPSIdentity;

/* Separate contract for a native service that iterates THIS UID's existing
 * owned roster. This is never an actor/UI identity, nor a fabricated CID. */
typedef struct LidPSRosterIdentity {
    uint64_t session,expected_session;
    unsigned local_uid,requested_uid,verified_owned_roster,owned_count,ap_active;
    enum LidPSRole role;
} LidPSRosterIdentity;
static int lid_ps_roster_owned(const LidPSRosterIdentity *p){
    return p&&p->session&&p->session==p->expected_session&&p->local_uid&&
        p->requested_uid==p->local_uid&&p->verified_owned_roster==1&&
        p->owned_count>=1&&p->owned_count<=64&&p->ap_active==1&&p->role==LID_PS_OWN_SERVICE;
}

static int lid_ps_owned(const LidPSIdentity *p){
    size_t i;
    if(!p || !p->ap_active || !p->verified_owned_record || !p->session ||
       p->session!=p->expected_session || !p->local_uid ||
       p->requested_uid!=p->local_uid ||
       (p->role!=LID_PS_PLAYER && p->role!=LID_PS_OWN_UI && p->role!=LID_PS_OWN_SERVICE && p->role!=LID_PS_PLAYER_OUTPUT) ||
       !p->owned_cid || !p->requested_cid || p->owned_units<2 || p->owned_units>128 ||
       p->owned_units!=p->requested_units)return 0;
    for(i=0;i<p->owned_units;i++){
        if(p->owned_cid[i]!=p->requested_cid[i])return 0;
        if(i+1==p->owned_units ? p->owned_cid[i]!=0 : p->owned_cid[i]==0)return 0;
    }
    return 1;
}

/* Native BodyLvl indices: lvl,hp,str,dex,vit,stm,luk,skill,bag,rage.
 * AP order: hp,stm,str,dex,vit,luk,bag,skill,rage.
 * Caller receives a complete result or no write, including on bad configuration.
 */
static int lid_ps_values(const LidPSIdentity *owner,const int tracks[9][3],
                         const int levels[10],int output[9]){
    static const unsigned order[9]={1,5,2,3,4,6,8,7,9};
    int result[9];unsigned i;
    if(!tracks || !levels || !output || !lid_ps_owned(owner) || levels[0]<1)return 0;
    for(i=0;i<9;i++){
        int64_t start=tracks[i][0],step=tracks[i][1],cap=tracks[i][2],count;
        int native=levels[order[i]],minimum=i<6?1:0;
        if(start<0 || step<0 || cap<0 || cap>44 || native<minimum ||
           native>minimum+44 || start+step*cap>INT_MAX)return 0;
        count=(int64_t)native-minimum;
        if(count>cap)count=cap;
        result[i]=(int)(start+step*count);
    }
    for(i=0;i<9;i++)output[i]=result[i];
    return 1;
}
#endif
