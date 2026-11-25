#ifndef LID_PLAYER_STAT_RUNTIME_H
#define LID_PLAYER_STAT_RUNTIME_H
#include "player_stat_identity.h"
#include "player_stat_views.h"
#include "player_stat_scope.h"

/* Included after progression/reload runtime definitions. Preparation and
 * publication are separate: no address is changed merely by including this
 * file. All original calls run once; scopes unwind even on abnormal exits.
 * The master-view functions touch copies of return records, never repositories.
 */
enum {LID_PSR_EMPTY,LID_PSR_PREPARED,LID_PSR_READY,LID_PSR_FAILED};
typedef struct LidPSRuntime {
    unsigned state,index_offset,index_state;void *index_class;BYTE *exe;
    LidPScope scope;
    DWORD(WINAPI *tls_alloc)(void);
    void *(WINAPI *tls_get)(DWORD);
    BOOL(WINAPI *tls_set)(DWORD,void*);
    void(WINAPI *error_set)(DWORD);
    NativeIntArrayAddFn array_add;
    void *(WINAPI *string_resize)(void*,int,int);
    uintptr_t(WINAPI *bulk_bag)(unsigned);
    uintptr_t(WINAPI *bulk_other)(unsigned);
    void *(WINAPI *backend_detail)(void*,const uint16_t*,int,int);
    void *(WINAPI *backend_status)(void*,int,const uint16_t*,int);
    unsigned(WINAPI *backend_skills)(void*,const uint16_t*,int,int);
    int(WINAPI *ue_status)(void*,int,RawFString*,int,void*);
    void(WINAPI *ui_chara)(void*,void*,void*,void*,void*);
    void(WINAPI *ui_current)(void*,void*,int,int,int);
    void(WINAPI *ui_enemy)(void*,void*,void*,void*);
    void *(WINAPI *dto)(unsigned,const uint16_t*);
    uintptr_t(WINAPI *enemy_dto)(unsigned,void*,void*,const uint16_t*,unsigned,void*,void*,void*,void*,unsigned);
    unsigned(WINAPI *scalar)(unsigned,const uint16_t*,const uint16_t*);
    unsigned(WINAPI *limit_break)(unsigned,const uint16_t*);
    unsigned char(WINAPI *bag_rebuild)(unsigned,const uint16_t*);
    uintptr_t(WINAPI *bag_add)(unsigned,const uint16_t*,int);
    int(WINAPI *bag_base)(const uint16_t*,int,int);
    uintptr_t(WINAPI *distribute)(void*,int,void*,int*,int*,int*);
    ExecFunction exec_status,exec_detail,exec_level;
} LidPSRuntime;
static LidPSRuntime g_player_stats;
static const char g_player_stats_capability[]="LID_PLAYER_ONLY_STATS_v1";
static unsigned g_player_stats_victim_logged;

static int player_stats_mode(void){
    return g_v074_cfg.player_only_stats==1;
}
static void *player_stats_tls_get(void *c,unsigned slot){return ((LidPSRuntime*)c)->tls_get(slot);}
static int player_stats_tls_set(void *c,unsigned slot,void *v){return !!((LidPSRuntime*)c)->tls_set(slot,v);}
static unsigned player_stats_tid(void *c){(void)c;return g_get_tid?g_get_tid():0;}
static unsigned player_stats_error(void *c){(void)c;return g_win.GetLastError();}
static void player_stats_error_set(void *c,unsigned e){((LidPSRuntime*)c)->error_set(e);}
static int player_stats_copy(void *c,uintptr_t at,void *out,size_t n){
    (void)c;if(!at||!out||!n||!readable_range((void*)at,n))return 0;
    lid_memcpy(out,(void*)at,n);return 1;
}
static LidPSSource player_stats_source(void){
    LidPSRuntime *r=&g_player_stats;
    return (LidPSSource){{r,player_stats_copy},(uintptr_t)r->exe,g_progress_creation_time,
        g_progress_creation_time,(unsigned)(player_stats_mode()&&g_ap_mode_active&&g_savedata_route_active&&
        g_v074_cfg.valid&&__atomic_load_n(&r->state,__ATOMIC_ACQUIRE)==LID_PSR_READY),r->exe!=0};
}
static int player_stats_owned(void){
    return player_stats_mode()&&g_ap_mode_active&&g_savedata_route_active&&g_v074_cfg.valid&&
        __atomic_load_n(&g_player_stats.state,__ATOMIC_ACQUIRE)==LID_PSR_READY&&
        lid_pscope_owned(&g_player_stats.scope,g_release_slot_key);
}
static int player_stats_is_a(void *object,const char *wanted){
    BYTE *cls;
    if(!readable_range(object,0x58))return 0;cls=*(BYTE**)((BYTE*)object+0x50);
    for(unsigned i=0;cls&&i<32;i++){
        if(!readable_range(cls,0x80))return 0;
        if(progress_event_name_is(cls,wanted))return 1;
        cls=*(BYTE**)(cls+0x78);
    }
    return 0;
}
static unsigned player_stats_index_offset(const LidPSSource *s){
    uintptr_t u;LidPSArrayCopy records;void *cls;BYTE *property;LidPSRuntime *r=&g_player_stats;
    if(!lid_ps_ue_roster(s,&records,&u)||!readable_range((void*)u,0x58))return 0;
    cls=*(void**)(u+0x50);
    /* One immutable cache entry. A different class refuses; it cannot race to
       replace the offset while another thread consumes the first class. */
    if(__atomic_load_n(&r->index_state,__ATOMIC_ACQUIRE)==2)return r->index_class==cls?r->index_offset:0;
    property=lid_event_field((void*)u,"mCurrentCharaIndex","IntProperty",&g_progress_event_ops);
    if(!property||!readable_range(property,0x98)||*(int*)(property+0x68)!=1||*(int*)(property+0x6c)!=4)return 0;
    unsigned at=*(unsigned*)(property+0x94);
    if(at<0x58||at>0x10000-4)return 0;
    unsigned empty=0;
    if(!__atomic_compare_exchange_n(&r->index_state,&empty,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    r->index_offset=at;r->index_class=cls;__atomic_store_n(&r->index_state,2,__ATOMIC_RELEASE);return at;
}
static int player_stats_current_core(const LidPSSource *s,uintptr_t object,uintptr_t status,LidPSProof *out){
    uintptr_t game;uint64_t pawn,common;
    if(!lid_ps_game(s,&game)||!lid_ps_read(s,game,0x8bc,&pawn,8)||!lid_ps_read(s,(uintptr_t)pawn,0x17f8,&common,8))return 0;
    if(object&&object!=(uintptr_t)pawn&&object!=(uintptr_t)common)return 0;
    if(!object)object=(uintptr_t)pawn;
    unsigned at=player_stats_index_offset(s);
    return at&&lid_ps_actor(s,object,at,status,out);
}
static int player_stats_current(const LidPSSource*s,uintptr_t object,uintptr_t status,LidPSProof*out){
#if R157_READ_SCOPED
    int acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    int result=player_stats_current_core(s,object,status,out);
#if R157_READ_SCOPED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    return result;
}
static void player_stats_enter_proof(const LidPSSource *s,const LidPSProof *proof,LidPScopeToken *token){
    LidPSIdentity identity=lid_ps_proof_identity(s,proof);
    (void)lid_pscope_enter(&g_player_stats.scope,g_release_slot_key,&identity,token);
}
static void player_stats_enter_unknown(LidPScopeToken *token){
    (void)lid_pscope_enter(&g_player_stats.scope,g_release_slot_key,0,token);
}
static void player_stats_enter_enemy(LidPScopeToken *token){
    LidPSIdentity identity={0};identity.role=LID_PS_HATER;
    (void)lid_pscope_enter(&g_player_stats.scope,g_release_slot_key,&identity,token);
}
static void player_stats_enter_service(unsigned uid,const uint16_t *cid,LidPScopeToken *token){
    LidPSSource s=player_stats_source();LidPSProof proof;
#if R157_READ_SCOPED
    int acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    int owned=lid_ps_service(&s,uid,(uintptr_t)cid,&proof);
#if R157_READ_SCOPED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    if(owned)player_stats_enter_proof(&s,&proof,token);
    else player_stats_enter_unknown(token);
}
static void player_stats_enter_roster(unsigned requested_uid,LidPScopeToken *token){
    LidPSSource s=player_stats_source();LidPSRosterProof proof;unsigned uid;
    if(lid_ps_source(&s,&uid)&&lid_ps_model(&s,requested_uid,0,0,0,&proof)){
        LidPSRosterIdentity identity={proof.session,s.expected_session,uid,requested_uid,1,proof.count,s.ap_active,LID_PS_OWN_SERVICE};
        (void)lid_pscope_enter_roster(&g_player_stats.scope,g_release_slot_key,&identity,token);
    }else player_stats_enter_unknown(token);
}
static void player_stats_leave(LidPScopeToken *token){
    if(token->entered)(void)lid_pscope_leave(&g_player_stats.scope,token);
}

/* Actual metadata adapter; only immutable metadata and scalar proof escape. */
#include "player_stat_frame.h"
static int player_stats_frame_is_a(void*c,uintptr_t object,const char*name){(void)c;return player_stats_is_a((void*)object,name);}
static int player_stats_frame_class(void*c,uintptr_t object,const char*name){(void)c;return lid_event_class_is((void*)object,name,&g_progress_event_ops);}
static int player_stats_frame_name(void*c,uintptr_t object,const char*name){(void)c;return progress_event_name_is((void*)object,name);}
static int player_stats_frame_actor(void*c,uintptr_t object,LidPSProof*out){return player_stats_current(c,object,0,out);}
static void player_stats_enter_frame(void*frame,LidPScopeToken*token){
    LidPSSource source=player_stats_source();LidPSProof proof;
    if(!source.ap_active)return;
#if R157_READ_SCOPED
    int acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    LidPSFrameOps ops={source,&source,player_stats_frame_is_a,player_stats_frame_class,player_stats_frame_name,player_stats_frame_actor};
    int origin=lid_ps_frame_origin(&ops,(uintptr_t)frame,&proof);
#if R157_READ_SCOPED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    if(origin==LID_PSF_ENEMY)player_stats_enter_enemy(token);
    else if(origin==LID_PSF_OWNED)player_stats_enter_proof(&source,&proof,token);
    else if(origin!=LID_PSF_INHERIT||!player_stats_owned())player_stats_enter_unknown(token);
}
static void WINAPI player_stats_exec_status(void *self,void *frame,void *result){
    LidPScopeToken token={0};DWORD incoming=g_win.GetLastError();player_stats_enter_frame(frame,&token);g_player_stats.error_set(incoming);
    __try {g_player_stats.exec_status(self,frame,result);} __finally {player_stats_leave(&token);}
}
static void WINAPI player_stats_exec_detail(void *self,void *frame,void *result){
    LidPScopeToken token={0};DWORD incoming=g_win.GetLastError();player_stats_enter_frame(frame,&token);g_player_stats.error_set(incoming);
    __try {g_player_stats.exec_detail(self,frame,result);} __finally {player_stats_leave(&token);}
}
static void WINAPI player_stats_exec_level(void *self,void *frame,void *result){
    LidPScopeToken token={0};DWORD incoming=g_win.GetLastError();player_stats_enter_frame(frame,&token);g_player_stats.error_set(incoming);
    __try {g_player_stats.exec_level(self,frame,result);} __finally {player_stats_leave(&token);}
}
static void player_stats_body_status(void *out,void *levels,RawFString *type,int grade){
    if(__atomic_load_n(&g_player_stats.state,__ATOMIC_ACQUIRE)!=LID_PSR_READY){
        if(g_rage_body_status_original)g_rage_body_status_original(out,levels,type,grade);return;
    }
    LidPScopeToken token={0};LidPSSource s=player_stats_source();LidPSProof proof;DWORD incoming=g_win.GetLastError();
    if(out&&player_stats_current(&s,0,(uintptr_t)out,&proof)){
        /* This role is NEVER inferred from a caller, class or copied CID.
           current() proved out == current owned Pawn + a70 plus UID/roster. */
        proof.role=LID_PS_PLAYER_OUTPUT;player_stats_enter_proof(&s,&proof,&token);
    }
    g_player_stats.error_set(incoming);
    __try {
        g_rage_body_status_original(out,levels,type,grade);DWORD error=g_win.GetLastError();
        if((grade==1||grade==6)&&player_stats_owned()&&v074_skill_fstring_matches((BYTE*)type,"BAL")&&
           readable_range(levels,40)&&progress_native_writable(out,0x34)){
            LidCharacterConfig config;progress_config_from_runtime(&config);
            int applied=lid_ps_view_body_status(1,&config,(int*)levels,out,0x34);
            if(applied&&token.entered&&token.frame->previous&&token.frame->previous->enemy&&
               !__atomic_exchange_n(&g_player_stats_victim_logged,1,__ATOMIC_RELAXED))
                log_ascii("AP PLAYER STATS R348: verified player output retained AP values inside enemy-origin recalculation.");
        }
        g_player_stats.error_set(error);
    } __finally {player_stats_leave(&token);}
}
static int WINAPI player_stats_ue_status(void *self,int level,RawFString *type,int grade,void *out){
    int result=g_player_stats.ue_status(self,level,type,grade,out);DWORD error=g_win.GetLastError();
    if(player_stats_owned()&&v074_skill_fstring_matches((BYTE*)type,"BAL")&&progress_native_writable(out,0x44)&&
       v074_skill_fstring_matches((BYTE*)out+4,"BAL")){
        LidCharacterConfig config;progress_config_from_runtime(&config);
        (void)lid_ps_view_ue_row(1,&config,level,grade,out,0x44,result);
    }
    g_player_stats.error_set(error);return result;
}
static int player_stats_bal(const uint16_t *s){
    uint16_t text[4];return readable_range(s,8)&&(lid_memcpy(text,s,8),text[0]=='B'&&text[1]=='A'&&text[2]=='L'&&!text[3]);
}
static void *WINAPI player_stats_backend_status(void *out,int level,const uint16_t *type,int grade){
    void *result=g_player_stats.backend_status(out,level,type,grade);DWORD error=g_win.GetLastError();
    if(player_stats_owned()&&result==out&&player_stats_bal(type)&&progress_native_writable(out,0x81)&&
       v074_skill_fstring_matches((BYTE*)out+0x10,"BAL")){
        LidCharacterConfig config;progress_config_from_runtime(&config);
        (void)lid_ps_view_native_row(1,&config,level,grade,out,0x81);
    }
    g_player_stats.error_set(error);return result;
}
static int player_stats_set_native_string(RawFString *s,const uint16_t *text,unsigned units){
    if(!s||!text||units>184||!progress_native_writable(s,16)||s->num<0||s->max<s->num||s->max>1048576||
       ((!s->data)!=(s->max==0)))return 0;
    if(units>(unsigned)s->max){
        if(!g_player_stats.string_resize)return 0;
        wchar_t *data=g_player_stats.string_resize(s->data,(int)units*2,8);if(!data)return 0;s->data=data;s->max=(int)units;
    }
    if(units){if(!progress_native_writable(s->data,(size_t)units*2))return 0;lid_memcpy(s->data,text,(size_t)units*2);}
    s->num=(int)units;return 1;
}
static void *WINAPI player_stats_backend_detail(void *out,const uint16_t *type,int grade,int limit_break){
    void *result=g_player_stats.backend_detail(out,type,grade,limit_break);DWORD error=g_win.GetLastError();
    if(player_stats_owned()&&result==out&&player_stats_bal(type)&&progress_native_writable(out,0xa1)&&((BYTE*)out)[0xa0]==1&&
       v074_skill_fstring_matches((BYTE*)out+8,"BAL")&&
       lid_ps_i64((BYTE*)out+0x18)==grade&&lid_ps_i64((BYTE*)out+0x20)==limit_break){
        LidCharacterConfig config;LidPSDetailValues values;progress_config_from_runtime(&config);
        if(lid_ps_detail_values(1,&config,g_v074_cfg.max_levels,grade,limit_break,&values)&&
           player_stats_set_native_string((RawFString*)((BYTE*)out+0x40),values.skills,values.skill_units)){
            lid_ps_w64((BYTE*)out+0x30,values.max_levels);lid_ps_w64((BYTE*)out+0x38,values.bag);lid_ps_w64((BYTE*)out+0x50,values.rage);
        }
    }
    g_player_stats.error_set(error);return result;
}
static void WINAPI player_stats_ui_chara(void *out,void *chara,void *master,void *common,void *user){
    LidPScopeToken token={0};LidPSSource s=player_stats_source();LidPSProof proof;DWORD error=g_win.GetLastError();
    if(lid_ps_ue_chara(&s,(uintptr_t)chara,LID_PS_OWN_UI,&proof))player_stats_enter_proof(&s,&proof,&token);else player_stats_enter_unknown(&token);
    g_player_stats.error_set(error);
    __try {g_player_stats.ui_chara(out,chara,master,common,user);} __finally {player_stats_leave(&token);}
}
static void WINAPI player_stats_ui_current(void *out,void *added,int a,int b,int c){
    LidPScopeToken token={0};LidPSSource s=player_stats_source();LidPSProof proof;DWORD error=g_win.GetLastError();
    if(player_stats_current(&s,0,0,&proof))player_stats_enter_proof(&s,&proof,&token);else player_stats_enter_unknown(&token);
    g_player_stats.error_set(error);
    __try {g_player_stats.ui_current(out,added,a,b,c);} __finally {player_stats_leave(&token);}
}
static void WINAPI player_stats_ui_enemy(void *out,void *zombie,void *master,void *common){
    LidPScopeToken token={0};DWORD error=g_win.GetLastError();player_stats_enter_enemy(&token);g_player_stats.error_set(error);
    __try {g_player_stats.ui_enemy(out,zombie,master,common);} __finally {player_stats_leave(&token);}
}
static void *WINAPI player_stats_dto(unsigned uid,const uint16_t *cid){
    LidPScopeToken token={0};void *result;DWORD error=g_win.GetLastError();player_stats_enter_service(uid,cid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.dto(uid,cid);} __finally {player_stats_leave(&token);}return result;
}
static uintptr_t WINAPI player_stats_enemy_dto(unsigned a,void *b,void *c,const uint16_t *d,unsigned e,void *f,void *g,void *h,void *i,unsigned j){
    LidPScopeToken token={0};uintptr_t result;DWORD error=g_win.GetLastError();player_stats_enter_enemy(&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.enemy_dto(a,b,c,d,e,f,g,h,i,j);} __finally {player_stats_leave(&token);}return result;
}
static unsigned WINAPI player_stats_scalar(unsigned uid,const uint16_t *cid,const uint16_t *key){
    LidPScopeToken token={0};unsigned result;DWORD error=g_win.GetLastError();player_stats_enter_service(uid,cid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.scalar(uid,cid,key);} __finally {player_stats_leave(&token);}return result;
}
static unsigned WINAPI player_stats_limit_break(unsigned uid,const uint16_t *cid){
    LidPScopeToken token={0};unsigned result;DWORD error=g_win.GetLastError();player_stats_enter_service(uid,cid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.limit_break(uid,cid);} __finally {player_stats_leave(&token);}return result;
}
static unsigned char WINAPI player_stats_bag_rebuild(unsigned uid,const uint16_t *cid){
    LidPScopeToken token={0};unsigned char result;DWORD error=g_win.GetLastError();player_stats_enter_service(uid,cid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.bag_rebuild(uid,cid);} __finally {player_stats_leave(&token);}return result;
}
static uintptr_t WINAPI player_stats_bag_add(unsigned uid,const uint16_t *cid,int count){
    LidPScopeToken token={0};uintptr_t result;DWORD error=g_win.GetLastError();player_stats_enter_service(uid,cid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.bag_add(uid,cid,count);} __finally {player_stats_leave(&token);}return result;
}
static int WINAPI player_stats_bag_base(const uint16_t *type,int grade,int limit_break,void *chara){
    LidPScopeToken token={0};int result;LidPSSource s=player_stats_source();LidPSProof proof;DWORD error=g_win.GetLastError();
    if(lid_ps_chara_model(&s,(uintptr_t)chara,&proof))player_stats_enter_proof(&s,&proof,&token);else player_stats_enter_unknown(&token);
    g_player_stats.error_set(error);
    __try {result=g_player_stats.bag_base(type,grade,limit_break);} __finally {player_stats_leave(&token);}return result;
}
static uintptr_t WINAPI player_stats_distribute(void *body,int grade,void *added,int *a,int *b,int *c){
    LidPScopeToken token={0};uintptr_t result;LidPSSource s=player_stats_source();LidPSProof proof;DWORD error=g_win.GetLastError();
    if(lid_ps_body_model(&s,(uintptr_t)body,&proof))player_stats_enter_proof(&s,&proof,&token);else player_stats_enter_unknown(&token);
    g_player_stats.error_set(error);
    __try {result=g_player_stats.distribute(body,grade,added,a,b,c);} __finally {player_stats_leave(&token);}return result;
}

static int player_stats_skill_plan(int level,int *out){
    int floor=0,target;LidReloadCapacityValues restored;LidPSArrayCopy records;uintptr_t userdata;
    if(!out||level<0||level>44||!player_stats_owned())return 0;
    target=v076_capacity_from_level(g_v074_cfg.starting_skill_slots,g_v074_cfg.skill_slots_increase,g_v074_cfg.skill_slots_quantity,level);
    unsigned phase=progress_reload_phase();
    if(phase==LID_RR_LOADING||phase==LID_RR_VIEWS){
        if(!__atomic_load_n(&g_reload_runtime.capacity_ready,__ATOMIC_ACQUIRE)||progress_reload_capacity(&restored)!=1)return 0;
        floor=restored.skill_floor;
    }else{
        LidPSSource s=player_stats_source();
        if(!lid_ps_ue_roster(&s,&records,&userdata))return 0;
        for(int i=0;i<records.num;i++){
            int equipped;BYTE *chara=(BYTE*)(uintptr_t)records.data+(size_t)i*0x220;
            if(!lid_skill_equipped_floor(chara,readable_range,&equipped))return 0;
            if(equipped>floor)floor=equipped;
        }
        if(lid_skill_same_key(&g_v076_skill_capacity_policy,g_release_slot_key)&&g_v076_skill_capacity_policy.applied>floor)
            floor=g_v076_skill_capacity_policy.applied;
    }
    if(target<floor)target=floor;if(target<0||target>64)return 0;*out=target;return 1;
}
static int player_stats_set_slots(void *out,int target){
    V076IntArray *a=out;
    if(target<0||target>64||!progress_native_writable(a,sizeof(*a))||a->num<0||a->max<a->num||a->max>256||
       ((!a->data)!=(a->max==0)))return 0;
    if(target>a->num){if(!g_player_stats.array_add)return 0;g_player_stats.array_add(a,target-a->num);}
    if(a->max<target||(target&&!progress_native_writable(a->data,(size_t)target*4)))return 0;
    for(int i=0;i<target;i++)((int*)a->data)[i]=i+1;a->num=target;return 1;
}
static unsigned WINAPI player_stats_backend_skills(void *out,const uint16_t *type,int grade,int level){
    LidPScopeToken token={0};DWORD incoming=g_win.GetLastError();unsigned result;int target=0,plan=0;
    uintptr_t caller=(uintptr_t)__builtin_return_address(0),base=(uintptr_t)g_player_stats.exe;
    /* Exact native Momoka consumer uses SoulUID and an owned fighter's Skill
       index. This establishes only a roster SERVICE scope, never an actor. */
    if(!player_stats_owned()&&caller==base+0x115793d&&readable_range((void*)(caller-5),5)){
        int32_t rel;BYTE *call=(BYTE*)(caller-5);lid_memcpy(&rel,call+1,4);
        if(call[0]==0xe8&&caller+(intptr_t)rel==base+0x11ad340&&readable_range((void*)(base+0xf8b5090),4))
            player_stats_enter_roster(*(unsigned*)(base+0xf8b5090),&token);
    }
    if(grade==6&&player_stats_bal(type))plan=player_stats_skill_plan(level,&target);
    g_player_stats.error_set(incoming);
    __try {
        /* AP uses this field as a progression index. Query the real existing
           LB0 key ONCE, then project the owned capacity. Native failure stays
           failure; enemy/unknown callers keep their unmodified native key. */
        result=g_player_stats.backend_skills(out,type,grade,plan?0:level);DWORD error=g_win.GetLastError();
        if(plan&&result==1)(void)player_stats_set_slots(out,target);
        g_player_stats.error_set(error);
    } __finally {player_stats_leave(&token);}return result;
}
static uintptr_t WINAPI player_stats_bulk_bag(unsigned uid){
    LidPScopeToken token={0};uintptr_t result;DWORD error=g_win.GetLastError();player_stats_enter_roster(uid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.bulk_bag(uid);} __finally {player_stats_leave(&token);}return result;
}
static uintptr_t WINAPI player_stats_bulk_other(unsigned uid){
    LidPScopeToken token={0};uintptr_t result;DWORD error=g_win.GetLastError();player_stats_enter_roster(uid,&token);g_player_stats.error_set(error);
    __try {result=g_player_stats.bulk_other(uid);} __finally {player_stats_leave(&token);}return result;
}

#include "player_stat_profiles.h"
static LidPSPatchSet g_player_stat_patches;
static int player_stats_region(void*c,uintptr_t address,R32Region*out){
    MEMORY_BASIC_INFORMATION info;(void)c;
    if(!g_win.VirtualQuery||!out||!g_win.VirtualQuery((void*)address,&info,sizeof(info)))return 0;
    out->base=(uintptr_t)info.BaseAddress;out->size=info.RegionSize;out->free=info.State==0x10000u;return 1;
}
static void *player_stats_allocate_near(void*c,uintptr_t address,size_t n){
    (void)c;return g_win.VirtualAlloc((void*)address,n,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
}
static int player_stats_prepare_at_idle(void){
    LidPSRuntime *r=&g_player_stats;
    if(!player_stats_mode())return 1;
    if(r->state!=LID_PSR_EMPTY)return r->state==LID_PSR_READY;
    if(!g_reload_runtime.proxy_verified||!progress_reload_idle_valid()||!g_get_tid||g_get_tid()!=g_reload_runtime.writer)return 0;
    r->exe=g_reload_runtime.exe;r->state=LID_PSR_FAILED;
    void *kernel=lid_find_module_ascii("kernel32.dll");if(!kernel||!g_win.GetLastError)return 0;
    r->tls_alloc=(void*)lid_get_export(kernel,"TlsAlloc");r->tls_get=(void*)lid_get_export(kernel,"TlsGetValue");
    r->tls_set=(void*)lid_get_export(kernel,"TlsSetValue");r->error_set=(void*)lid_get_export(kernel,"SetLastError");
    if(!r->tls_alloc||!r->tls_get||!r->tls_set||!r->error_set)return 0;
    /* The removed global scan used to initialize this existing SEH delegate.
       Owned wrappers and existing reload finally blocks still require it. */
    void *ntdll=lid_find_module_ascii("ntdll.dll");
    if(!ntdll||!lid_master_scan_guard_init(lid_get_export(ntdll,"__C_specific_handler")))return 0;
    r->array_add=(void*)(r->exe+0x79e820);r->string_resize=(void*)(r->exe+0xe0460);
    r->bulk_bag=(void*)(r->exe+0x1337a30);r->bulk_other=(void*)(r->exe+0x1356ae0);
    LidPScopeOps tls={r,player_stats_tls_get,player_stats_tls_set,player_stats_tid,player_stats_error,player_stats_error_set};
    if(!lid_pscope_init(&r->scope,r->tls_alloc(),&tls))return 0;
    R32CallPatchOps ops={{&g_reload_runtime,progress_reload_patch_quiet,progress_reload_patch_read,progress_reload_patch_alloc,
        progress_reload_patch_free,progress_reload_patch_write,progress_reload_patch_protect,progress_reload_patch_flush},
        {r,player_stats_region,player_stats_allocate_near}};
    void *detours[]={player_stats_backend_detail,player_stats_enemy_dto,player_stats_backend_status,player_stats_bag_add,
        player_stats_bag_base,player_stats_bag_rebuild,player_stats_limit_break,player_stats_distribute,player_stats_scalar,
        player_stats_dto,player_stats_ui_chara,player_stats_ui_current,player_stats_ui_enemy,player_stats_backend_skills,
        player_stats_ue_status,player_stats_exec_status,player_stats_exec_detail,player_stats_exec_level,
        player_stats_bulk_bag,player_stats_bulk_bag,player_stats_bulk_bag,player_stats_bulk_other};
    unsigned n=sizeof(detours)/sizeof(detours[0]);
    if(n!=sizeof(lid_ps_profiles)/sizeof(lid_ps_profiles[0])||
       !lid_psp_prepare(&g_player_stat_patches,(uintptr_t)r->exe,1,lid_ps_profiles,detours,n,&ops))return 0;
#define LID_PSR_ORIGINAL(field,index) r->field=(__typeof__(r->field))g_player_stat_patches.entries[index].original
    LID_PSR_ORIGINAL(backend_detail,0);LID_PSR_ORIGINAL(enemy_dto,1);LID_PSR_ORIGINAL(backend_status,2);
    LID_PSR_ORIGINAL(bag_add,3);LID_PSR_ORIGINAL(bag_base,4);LID_PSR_ORIGINAL(bag_rebuild,5);
    LID_PSR_ORIGINAL(limit_break,6);LID_PSR_ORIGINAL(distribute,7);LID_PSR_ORIGINAL(scalar,8);
    LID_PSR_ORIGINAL(dto,9);LID_PSR_ORIGINAL(ui_chara,10);LID_PSR_ORIGINAL(ui_current,11);LID_PSR_ORIGINAL(ui_enemy,12);
    LID_PSR_ORIGINAL(backend_skills,13);LID_PSR_ORIGINAL(ue_status,14);LID_PSR_ORIGINAL(exec_status,15);
    LID_PSR_ORIGINAL(exec_detail,16);LID_PSR_ORIGINAL(exec_level,17);
#undef LID_PSR_ORIGINAL
    r->state=LID_PSR_PREPARED;
    if(!lid_psp_publish(&g_player_stat_patches,&ops)){r->state=LID_PSR_FAILED;return 0;}
    __atomic_store_n(&r->state,LID_PSR_READY,__ATOMIC_RELEASE);
    log_ascii(g_player_stats_capability);
    log_ascii("AP PLAYER STATS R348: subject-aware player views ready; native merchant entry retained.");return 1;
}
#endif
