#ifndef LID_PLAYER_STAT_IDENTITY_H
#define LID_PLAYER_STAT_IDENTITY_H
#include "player_stat_policy.h"

/* Read-only identity adapter for EXE SHA6cfadbba...899e42ca. All addresses are
 * copied through the caller's bounded reader, never dereferenced here. Native
 * Chara rows (150), BodyLvl rows (5c) and UE BrgDbChara (220) are DISTINCT.
 * The adapter never calls a native lookup/get-or-create or stores game memory.
 */
typedef struct LidPSReadOps {
    void *context;
    int (*copy)(void *,uintptr_t,void *,size_t);
} LidPSReadOps;
typedef struct LidPSSource {
    LidPSReadOps memory;
    uintptr_t image;
    uint64_t session,expected_session;
    unsigned ap_active,exact_game;
} LidPSSource;
typedef struct LidPSStringCopy { uint16_t text[128];unsigned units; } LidPSStringCopy;
typedef struct LidPSArrayCopy { uint64_t data;int32_t num,capacity; } LidPSArrayCopy;
typedef struct LidPSProof {
    LidPSStringCopy cid;
    unsigned uid;
    enum LidPSRole role;
    uint64_t session;
} LidPSProof;
typedef struct LidPSRosterProof { unsigned uid,count;uint64_t session; } LidPSRosterProof;

static int lid_ps_read(const LidPSSource *s,uintptr_t base,size_t offset,void *out,size_t n){
    return s&&s->memory.copy&&base&&out&&n&&offset<=UINTPTR_MAX-base&&
        n<=UINTPTR_MAX-(base+offset)&&s->memory.copy(s->memory.context,base+offset,out,n)==1;
}
static int lid_ps_source(const LidPSSource *s,unsigned *uid){
    int32_t value;
    if(!s||!uid||!s->ap_active||s->exact_game!=1||!s->session||
       s->session!=s->expected_session||!s->image||s->image>UINTPTR_MAX-0xf8b5094||
       !lid_ps_read(s,s->image,0xf8b5090,&value,4)||value<=0)return 0;
    *uid=(unsigned)value;return 1;
}
static int lid_ps_array(const LidPSSource *s,uintptr_t at,size_t offset,size_t stride,
                         unsigned bound,LidPSArrayCopy *out){
    LidPSArrayCopy a;
    if(!out||!stride||!bound||!lid_ps_read(s,at,offset,&a,16)||a.num<0||
       a.capacity<a.num||(unsigned)a.capacity>bound||
       ((!a.data)!=(a.capacity==0))||a.data>UINTPTR_MAX||
       (a.num&&(size_t)a.num>(UINTPTR_MAX-(uintptr_t)a.data)/stride))return 0;
    *out=a;return 1;
}
static int lid_ps_string_data(const LidPSSource *s,uintptr_t data,unsigned units,LidPSStringCopy *out){
    LidPSStringCopy c={0};unsigned i;
    if(!out||units<2||units>128||!lid_ps_read(s,data,0,c.text,(size_t)units*2))return 0;
    for(i=0;i<units;i++)if(i+1==units?c.text[i]!=0:c.text[i]==0)return 0;
    c.units=units;*out=c;return 1;
}
static int lid_ps_string(const LidPSSource *s,uintptr_t at,size_t offset,LidPSStringCopy *out){
    LidPSArrayCopy d;
    if(!lid_ps_array(s,at,offset,2,1048576,&d)||d.num<2||d.num>128)return 0;
    return lid_ps_string_data(s,(uintptr_t)d.data,(unsigned)d.num,out);
}
/* Native entry points accept NUL-terminated CID rather than FString. Bounded
 * unit reads avoid reading past the terminator across a valid page boundary. */
static int lid_ps_cid_pointer(const LidPSSource *s,uintptr_t at,LidPSStringCopy *out){
    LidPSStringCopy c={0};unsigned i;
    if(!out)return 0;
    for(i=0;i<128;i++){
        if(!lid_ps_read(s,at,(size_t)i*2,&c.text[i],2))return 0;
        if(!c.text[i]){if(!i)return 0;c.units=i+1;*out=c;return 1;}
    }
    return 0;
}
static int lid_ps_cid_equal(const LidPSStringCopy *a,const LidPSStringCopy *b){
    if(!a||!b||a->units<2||a->units>128||a->units!=b->units)return 0;
    for(unsigned i=0;i<a->units;i++)if(a->text[i]!=b->text[i])return 0;
    return 1;
}
static int lid_ps_uid_string(const LidPSStringCopy *c,unsigned uid){
    uint16_t digits[10];unsigned n=0;
    if(!c||!uid)return 0;
    do{digits[n++]=(uint16_t)('0'+uid%10);uid/=10;}while(uid);
    if(c->units!=n+1)return 0;
    for(unsigned i=0;i<n;i++)if(c->text[i]!=digits[n-1-i])return 0;
    return c->text[n]==0;
}
static int lid_ps_array_same(const LidPSArrayCopy *a,const LidPSArrayCopy *b){
    return a->data==b->data&&a->num==b->num&&a->capacity==b->capacity;
}
/* Read the real Chara repository, NOT the BodyLvl/Bag repositories. Both
 * repository and row vtables/UID are checked; duplicate UID groups/CIDs refuse.
 * required_row=0 accepts an owned CID copy; otherwise exact model-row identity
 * is also required. These addresses are consumed only during this call.
 */
static int lid_ps_model(const LidPSSource *s,unsigned requested_uid,const LidPSStringCopy *wanted,
                         uintptr_t required_row,LidPSProof *out,LidPSRosterProof *roster){
    LidPSArrayCopy groups,again,rows,row_again;LidPSStringCopy key,cid;uint64_t seen[64];
    uint64_t vt;unsigned uid,group_hits=0,hits=0,count=0;uintptr_t found_group=0;
    if(!lid_ps_source(s,&uid)||requested_uid!=uid||(!wanted&&!roster)||
       !lid_ps_read(s,s->image,0x27e9d78,&vt,8)||vt!=s->image+0x218b0b0||
       !lid_ps_array(s,s->image+0x27e9d78,8,0x70,64,&groups))return 0;
    for(int g=0;g<groups.num;g++){
        uintptr_t group=(uintptr_t)groups.data+(size_t)g*0x70;
        if(!lid_ps_read(s,group,0,&vt,8)||vt!=s->image+0x218aff0||
           !lid_ps_string(s,group,0x60,&key))return 0;
        if(!lid_ps_uid_string(&key,uid))continue;
        if(++group_hits!=1||!lid_ps_array(s,group,0x50,0x150,64,&rows))return 0;
        found_group=group;
        for(int r=0;r<rows.num;r++){
            uintptr_t row=(uintptr_t)rows.data+(size_t)r*0x150;unsigned row_uid;
            if(!lid_ps_read(s,row,0,&vt,8)||vt!=s->image+0x218aef0||
               !lid_ps_read(s,row,8,&row_uid,4)||row_uid!=uid||
               !lid_ps_string(s,row,0xc,&cid))return 0;
            /* Keep bounded fingerprints, not 64 full CID copies on the game
               stack. The native build disables large-frame stack probes.
               Any collision REFUSES; it can never authorize a foreign CID.
               Fold ASCII case because native repository lookup is wcsicmp. */
            uint64_t hash=UINT64_C(14695981039346656037);
            for(unsigned j=0;j<cid.units;j++){
                uint16_t ch=cid.text[j];if(ch>='A'&&ch<='Z')ch=(uint16_t)(ch+('a'-'A'));
                hash^=ch;hash*=UINT64_C(1099511628211);
            }
            for(unsigned j=0;j<count;j++)if(hash==seen[j])return 0;
            seen[count++]=hash;
            if(wanted&&lid_ps_cid_equal(&cid,wanted)){
                if(++hits!=1||(required_row&&required_row!=row))return 0;
            }
        }
        if(!lid_ps_array(s,group,0x50,0x150,64,&row_again)||!lid_ps_array_same(&rows,&row_again))return 0;
    }
    if(group_hits!=1||!found_group||!count||!lid_ps_source(s,&requested_uid)||requested_uid!=uid||
       !lid_ps_array(s,s->image+0x27e9d78,8,0x70,64,&again)||!lid_ps_array_same(&groups,&again))return 0;
    if(wanted){if(hits!=1||!out)return 0;*out=(LidPSProof){*wanted,uid,LID_PS_OWN_SERVICE,s->session};}
    if(roster)*roster=(LidPSRosterProof){uid,count,s->session};
    return 1;
}
static int lid_ps_service(const LidPSSource *s,unsigned uid,uintptr_t cid,LidPSProof *out){
    LidPSStringCopy copy;
    return out&&lid_ps_cid_pointer(s,cid,&copy)&&lid_ps_model(s,uid,&copy,0,out,0);
}
/* BodyLvl-owned operations accept a 5c model row. First prove its distinct
 * vtable/UID/CID, then find the SAME CID in the real owned Chara repository. */
static int lid_ps_body_model(const LidPSSource *s,uintptr_t row,LidPSProof *out){
    uint64_t vt;unsigned uid,local,groups_found=0,hits=0;LidPSStringCopy cid,key,other;
    LidPSArrayCopy groups,rows;
    if(!out||!lid_ps_read(s,row,0,&vt,8)||vt!=s->image+0x21300d0||
       !lid_ps_read(s,row,8,&uid,4)||!lid_ps_string(s,row,0xc,&cid)||
       !lid_ps_source(s,&local)||uid!=local||
       !lid_ps_read(s,s->image,0x27e2f48,&vt,8)||vt!=s->image+0x21301d0||
       !lid_ps_array(s,s->image+0x27e2f48,8,0x70,64,&groups))return 0;
    for(int g=0;g<groups.num;g++){
        uintptr_t group=(uintptr_t)groups.data+(size_t)g*0x70;
        if(!lid_ps_read(s,group,0,&vt,8)||vt!=s->image+0x2130120||
           !lid_ps_string(s,group,0x60,&key))return 0;
        if(!lid_ps_uid_string(&key,uid))continue;
        if(++groups_found!=1||!lid_ps_array(s,group,0x50,0x5c,64,&rows))return 0;
        for(int r=0;r<rows.num;r++){
            uintptr_t candidate=(uintptr_t)rows.data+(size_t)r*0x5c;unsigned candidate_uid;
            if(!lid_ps_read(s,candidate,0,&vt,8)||vt!=s->image+0x21300d0||
               !lid_ps_read(s,candidate,8,&candidate_uid,4)||candidate_uid!=uid||
               !lid_ps_string(s,candidate,0xc,&other))return 0;
            if(lid_ps_cid_equal(&other,&cid)){if(++hits!=1||row!=candidate)return 0;}
        }
    }
    if(groups_found!=1||hits!=1)return 0;
    return lid_ps_model(s,uid,&cid,0,out,0);
}
static int lid_ps_chara_model(const LidPSSource *s,uintptr_t row,LidPSProof *out){
    unsigned uid;LidPSStringCopy cid;
    return out&&lid_ps_read(s,row,8,&uid,4)&&lid_ps_string(s,row,0xc,&cid)&&
        lid_ps_model(s,uid,&cid,row,out,0);
}
static int lid_ps_game(const LidPSSource *s,uintptr_t *game){
    uint64_t e,w,a,f,g,vt,fn;unsigned uid;
    if(!game||!lid_ps_source(s,&uid)||!lid_ps_read(s,s->image,0x29759f0,&e,8)||
       !lid_ps_read(s,(uintptr_t)e,0x80,&w,8)||!lid_ps_read(s,(uintptr_t)w,0x60,&a,8)||
       !lid_ps_read(s,(uintptr_t)a,0,&f,8)||!lid_ps_read(s,(uintptr_t)f,0x714,&g,8)||
       !lid_ps_read(s,(uintptr_t)g,0,&vt,8)||!lid_ps_read(s,(uintptr_t)vt,0x880,&fn,8)||
       fn!=s->image+0x10b35a0||!lid_ps_read(s,(uintptr_t)vt,0xa98,&fn,8)||fn!=s->image+0x10b5c50)return 0;
    *game=(uintptr_t)g;return 1;
}
static int lid_ps_ue_roster(const LidPSSource *s,LidPSArrayCopy *records,uintptr_t *userdata){
    uintptr_t game;uint64_t u,p,vt;
    return records&&userdata&&lid_ps_game(s,&game)&&lid_ps_read(s,game,0x1428,&u,8)&&
        lid_ps_read(s,(uintptr_t)u,0,&vt,8)&&vt==s->image+0x21fb700&&
        lid_ps_read(s,(uintptr_t)u,0x74,&p,8)&&
        lid_ps_array(s,(uintptr_t)p,0x2ec,0x220,64,records)&&
        (*userdata=(uintptr_t)u)!=0;
}
/* UI may pass a legitimate copied BrgDbChara, but CID must also occur exactly
 * once in both live UserData and the native owned model. Numeric equality,
 * class BAL and grade never authorize a calculation. Enemy ROLE is rejected
 * before reading anything, including a Hater whose CID belongs to the player.
 */
static int lid_ps_ue_chara(const LidPSSource *s,uintptr_t record,enum LidPSRole role,LidPSProof *out){
    LidPSArrayCopy records,again;LidPSStringCopy wanted,cid;LidPSProof proof;
    uintptr_t userdata,again_user;unsigned uid,hits=0;
    if(!out||(role!=LID_PS_PLAYER&&role!=LID_PS_OWN_UI)||!lid_ps_source(s,&uid)||
       !lid_ps_string(s,record,0,&wanted)||!lid_ps_ue_roster(s,&records,&userdata))return 0;
    for(int i=0;i<records.num;i++){
        if(!lid_ps_string(s,(uintptr_t)records.data+(size_t)i*0x220,0,&cid))return 0;
        if(lid_ps_cid_equal(&cid,&wanted))hits++;
    }
    if(hits!=1||!lid_ps_model(s,uid,&wanted,0,&proof,0)||
       !lid_ps_ue_roster(s,&again,&again_user)||userdata!=again_user||!lid_ps_array_same(&records,&again))return 0;
    proof.role=role;*out=proof;return 1;
}
/* current_index_offset must come from the validated IntProperty metadata, not
 * a guessed static offset. Actor identity is checked before CID/record proof.
 * An enemy actor with an owned CID therefore remains ineligible. */
static int lid_ps_actor(const LidPSSource *s,uintptr_t actor,unsigned current_index_offset,
                         uintptr_t status_out,LidPSProof *out){
    uintptr_t game,userdata;uint64_t pawn,common,vt,parent;int32_t index;
    LidPSArrayCopy records;
    if(!out||current_index_offset<0x58||current_index_offset>0x10000-4||
       !lid_ps_game(s,&game)||!lid_ps_read(s,game,0x8bc,&pawn,8)||
       !lid_ps_read(s,(uintptr_t)pawn,0x17f8,&common,8)||
       (actor!=(uintptr_t)pawn&&actor!=(uintptr_t)common)||
       (status_out&&((uintptr_t)pawn>UINTPTR_MAX-0xa70||status_out!=(uintptr_t)pawn+0xa70))||
       !lid_ps_read(s,(uintptr_t)common,0,&vt,8)||vt!=s->image+0x20b0190||
       !lid_ps_read(s,(uintptr_t)common,0x284,&parent,8)||parent!=pawn||
       !lid_ps_ue_roster(s,&records,&userdata)||
       !lid_ps_read(s,userdata,current_index_offset,&index,4)||index<0||index>=records.num)return 0;
    return lid_ps_ue_chara(s,(uintptr_t)records.data+(size_t)index*0x220,LID_PS_PLAYER,out);
}
static LidPSIdentity lid_ps_proof_identity(const LidPSSource *s,const LidPSProof *p){
    LidPSIdentity id={0};unsigned uid;
    if(!p||!lid_ps_source(s,&uid))return id;
    id=(LidPSIdentity){p->session,s->expected_session,uid,p->uid,
        p->cid.text,p->cid.text,p->cid.units,p->cid.units,p->role,s->ap_active,1};
    return id;
}
#endif
