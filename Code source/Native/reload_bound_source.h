#ifndef LID_RELOAD_BOUND_SOURCE_H
#define LID_RELOAD_BOUND_SOURCE_H
#include "reload_source_hold.h"
#include "reload_character_snapshot.h"
#include "reload_bag_growth.h"
#include "reload_native_bridge.h"
#include "reload_ephemeral_compare.h"
/* Concrete composition replacing the old unbound preflight boolean.
   Caller supplies the metadata read from the SAME verified AP slot directory,
   current granted peer, and a fresh verified proxy route to this exact path.
   Source hold uses real file bytes/SHA, not an attestation alone. */
typedef struct LidReloadBoundSource {
    LidReloadSourceProof proof;LidReloadSourceHold hold;LidReloadSourceIo io;
    LidReloadCharacterSnapshot snapshot;LidReloadJsonBinding*json;
    wchar_t path[1120];unsigned initialized,load_attempted,loaded,closed;
    LidReloadPoolString protected_content[2];
    unsigned require_core_proof,protected_captured,core_called,core_verified;
    unsigned core_stage,core_native_result,postcheck_domain,core_cleanup_ok;
    LidReloadCompareTrace compare_trace;
    unsigned empty_bag_growth;
} LidReloadBoundSource;
enum {LID_RC_NONE=0,LID_RC_PRECONDITION,LID_RC_NATIVE_CALL,LID_RC_NATIVE_RETURN,LID_RC_BOUNDARY,
      LID_RC_POST_PRECONDITION,LID_RC_POOL_BUILD,LID_RC_UID,LID_RC_CHILD,LID_RC_ENCODE,
      LID_RC_COMPARE,LID_RC_CLEANUP,LID_RC_POISON,LID_RC_VERIFIED};
static const char*lid_reload_bound_core_reason(const LidReloadBoundSource*s){
    static const char*const names[]={"not_entered","core_precondition","native_call","native_return_false","boundary_changed",
        "postcheck_precondition","pool_build","soul_or_uid","protected_child_missing","protected_encode",
        "protected_compare","cleanup","pool_poison","verified"};
    return s&&s->core_stage<sizeof(names)/sizeof(names[0])?names[s->core_stage]:"unknown";
}
static const char*lid_reload_bound_core_domain(const LidReloadBoundSource*s){
    static const char*const names[]={"none","deathbag","skl","bodyuser"};
    return s&&s->postcheck_domain<4?names[s->postcheck_domain]:"unknown";
}
static int lid_reload_bound_source_protect(LidReloadBoundSource*s){
    if(!s||!s->initialized||s->closed||s->load_attempted||s->require_core_proof)return 0;
    s->require_core_proof=1;return 1;
}
static int lid_reload_bound_source_init(LidReloadBoundSource*s,LidReloadJsonBinding*json,
    const LidReloadSourceIo*io,const BYTE*metadata,size_t size,const LidWirePeer*peer,
    const wchar_t*verified_ap_path,unsigned long long epoch,unsigned long long request){
    LidReloadSourceProof proof;size_t n=0;
    if(!s||s->initialized||!json||!io||!peer||!peer->bound||!peer->granted||!verified_ap_path||
       !lid_wire_valid(&peer->offer)||peer->offer.kind!=LID_WIRE_OFFER||
       !lid_wire_same_session(&peer->offer,&peer->binding)||
       !lid_reload_source_decode(&proof,metadata,size,&peer->binding)||!lid_reload_json_window(json))return 0;
    while(n<1119&&verified_ap_path[n])n++;
    if(!n||n>=1119)return 0;
    *s=(LidReloadBoundSource){0};
    if(!lid_reload_snapshot_init(&s->snapshot,epoch,request,proof.uid,proof.uid_length))return 0;
    __builtin_memcpy(s->path,verified_ap_path,(n+1)*sizeof(wchar_t));
    s->proof=proof;s->io=*io;s->json=json;s->initialized=1;return 1;
}
/* Use as bridge.capture, with bridge.context pointing at this source (or an
   adapter that forwards to it). No literal approved=1 without the live hold. */
static int lid_reload_bound_source_capture(void*context,void*borrowed_root){
    LidReloadBoundSource*s=context;void*soul;void*children[2];unsigned i;
    LID_R36_SET(bound_capture_stage,1);
    if(!s||!s->initialized||s->closed||!s->load_attempted||
       !lid_reload_source_approved(&s->hold,&s->proof.session,s->snapshot.uid,s->proof.uid_length))return 0;
    if(!lid_reload_snapshot_capture(s->json,&s->snapshot,borrowed_root,
        s->snapshot.epoch,s->snapshot.request,1))return 0;
    LID_R36_SET(bound_capture_stage,2);
    if(!s->require_core_proof)return 1;
    soul=lid_reload_json_child(s->json,borrowed_root,"soul");
    children[0]=lid_reload_json_child(s->json,soul,"deathbag");
    children[1]=lid_reload_json_child(s->json,soul,"skl");
    LID_R36_SET(bound_capture_stage,3);
    if(!children[0]||!children[1])return 0;
    for(i=0;i<2;i++){
        LID_R36_SET(bound_capture_stage,4+i);
        s->json->encode(&s->protected_content[i],children[i]);
        if(!lid_reload_json_string_ok(s->json,&s->protected_content[i]))return 0;
    }
    s->protected_captured=1;LID_R36_SET(bound_capture_stage,6);return 1;
}
static unsigned char lid_reload_bound_source_load(LidReloadBoundSource*s,LidReloadNativeBridge*b){
    unsigned char result;int closed;
    LID_R36_SET(load_stage,1);
    if(!s||!s->initialized||s->closed||s->load_attempted||!b||
       b->context!=s||b->capture!=lid_reload_bound_source_capture)return 0;
    s->load_attempted=1;
    LID_R36_SET(load_stage,2);
    if(!lid_reload_source_hold(&s->hold,&s->io,s->path,&s->proof))return 0;
    LID_R36_SET(load_stage,3);
    result=lid_reload_bridge_load(b);
    /* Reader must have returned before opening normal save admission again. */
    LID_R36_SET(load_stage,4);
    closed=lid_reload_source_close(&s->hold,&s->io);
    LID_R36_SET(load_stage,5);
    s->loaded=result==1&&closed==1;return (unsigned char)s->loaded;
}
static int lid_reload_bound_source_readback(LidReloadBoundSource*s){
    if(!s||!s->initialized||s->closed||!s->loaded||!s->hold.closed||s->hold.active||s->hold.poisoned)return 0;
    return lid_reload_snapshot_readback(s->json,&s->snapshot,s->snapshot.epoch,s->snapshot.request);
}
static int lid_reload_bound_source_close(LidReloadBoundSource*s){
    int good=1;unsigned i;
    if(!s||!s->initialized||s->closed)return 0;
    s->closed=1;
    if(s->hold.handle&&!s->hold.closed&&!lid_reload_source_close(&s->hold,&s->io))good=0;
    if(s->hold.poisoned)good=0;
    if(!lid_reload_snapshot_close(s->json,&s->snapshot))good=0;
    for(i=0;i<2;i++)if(!lid_reload_json_free_string(s->json,&s->protected_content[i]))good=0;
    return good;
}
static int lid_reload_bound_source_postcheck(LidReloadBoundSource*s){
    LidReloadPoolScope scope={0};LidReloadPoolString actual[3]={{0},{0},{0}};
    void*soul;void*children[3];unsigned i;int good=0,cleanup=1;
    const LidReloadPoolString*expected[3];
    if(!s)return 0;s->core_stage=LID_RC_POST_PRECONDITION;s->postcheck_domain=0;s->core_cleanup_ok=0;
    if(!s->initialized||s->closed||!s->require_core_proof||!s->protected_captured||
       !s->loaded||!s->snapshot.verified||!s->core_called||s->core_verified||!lid_reload_json_window(s->json))return 0;
    s->core_stage=LID_RC_POOL_BUILD;
    if(!lid_reload_pool_scope_build(&s->json->pool,&scope))return 0;
    s->core_stage=LID_RC_UID;
    soul=lid_reload_json_child(s->json,scope.root,"soul");
    if(!soul||!lid_reload_json_uid(s->json,soul,s->snapshot.uid))goto done;
    children[0]=lid_reload_json_child(s->json,soul,"deathbag");
    children[1]=lid_reload_json_child(s->json,soul,"skl");
    children[2]=lid_reload_json_child(s->json,scope.root,"bodyuser");
    expected[0]=&s->protected_content[0];expected[1]=&s->protected_content[1];expected[2]=&s->snapshot.expected[1];
    for(i=0;i<3;i++){
        s->postcheck_domain=i+1;s->core_stage=LID_RC_CHILD;
        if(!children[i])goto done;
        s->core_stage=LID_RC_ENCODE;
        s->json->encode(&actual[i],children[i]);
        if(!lid_reload_json_string_ok(s->json,&actual[i]))goto done;
        s->core_stage=LID_RC_COMPARE;
        {unsigned removed=0;
            if(!lid_reload_protected_compare(s->json,expected[i],&actual[i],s->snapshot.uid,i,
                &s->compare_trace,&removed))goto done;
            if(i==0)s->empty_bag_growth=removed;
        }
    }
    good=1;
done:
    for(i=0;i<3;i++)if(!lid_reload_json_free_string(s->json,&actual[i]))cleanup=0;
    if(!lid_reload_pool_scope_close(&s->json->pool,&scope))cleanup=0;
    s->core_cleanup_ok=(unsigned)(cleanup&&!s->json->pool.poisoned);
    if(good&&!cleanup)s->core_stage=LID_RC_CLEANUP;
    else if(good&&s->json->pool.poisoned)s->core_stage=LID_RC_POISON;
    s->core_verified=(unsigned)(good&&cleanup&&!s->json->pool.poisoned);
    if(s->core_verified){s->core_stage=LID_RC_VERIFIED;s->postcheck_domain=0;}
    return (int)s->core_verified;
}
/* The boundary's original callback must forward here through its adapter.
   It must not reopen SaveFn after the native core's bool alone. */
static unsigned char lid_reload_bound_source_core(LidReloadBoundSource*s,LidReloadNativeBridge*b,
                                                  const LidBoundaryArgs*a){
    unsigned char result;
    if(!s)return 0;s->core_stage=LID_RC_PRECONDITION;
    if(!s->initialized||s->closed||!s->require_core_proof||!s->protected_captured||
       !s->loaded||!s->snapshot.verified||s->core_called||!a||!lid_reload_bridge_valid(b)||
       b->context!=s||b->boundary->stage!=LID_BOUNDARY_BUSY||
       lid_reload_save_phase(&b->boundary->save)!=LID_RELOAD_SAVE_LOADING)return 0;
    s->core_called=1;
    s->core_stage=LID_RC_NATIVE_CALL;
    result=b->hooks->core(a->self,a->response,a->request_json,a->soul,a->entry_item,a->use_elevator,a->debug_template);
    s->core_native_result=result;s->core_stage=LID_RC_NATIVE_RETURN;
    if(result!=1)return 0;
    s->core_stage=LID_RC_BOUNDARY;
    if(b->boundary->stage!=LID_BOUNDARY_BUSY||
       lid_reload_save_phase(&b->boundary->save)!=LID_RELOAD_SAVE_LOADING)return 0;
    return (unsigned char)lid_reload_bound_source_postcheck(s);
}
/* Repeat only the protected readback, never Core. Used after a neutral save
   completion before delivery; caller transfers ownership to the game thread
   and holds a logical manager reservation with runner locks RELEASED. */
static int lid_reload_bound_source_recheck(LidReloadBoundSource*s){
    if(!s||!s->core_verified||s->closed)return 0;
    s->core_verified=0;return lid_reload_bound_source_postcheck(s);
}
/* Read only the fixed metadata name beside the AP save; resolve both paths
   from the caller's freshly verified slot directory. No arbitrary target from
   metadata, and no save-file write. Caller establishes route/session first. */
static int lid_reload_bound_source_prepare(LidReloadBoundSource*s,LidReloadJsonBinding*json,
    const LidReloadSourceIo*io,const LidWirePeer*peer,const wchar_t*verified_slot_directory,
    unsigned long long epoch,unsigned long long request){
    static const wchar_t metadata_name[]=L"character_reload_source.bin";
    static const wchar_t save_name[]=L"brggame.sav";
    BYTE metadata[641];wchar_t*path=0;HANDLE h;DWORD got=0;BOOL read,closed;
    size_t n=0,i;int good=0;
    if(!s||s->initialized||!json||!io||!io->open||!io->read||!io->close||!io->allocate||!io->release||
       !peer||!peer->bound||!peer->granted||!verified_slot_directory)return 0;
    while(n<999&&verified_slot_directory[n])n++;
    if(!n||n>=999)return 0;
    path=io->allocate(1120*sizeof(wchar_t));if(!path)return 0;
    __builtin_memcpy(path,verified_slot_directory,n*sizeof(wchar_t));
    if(path[n-1]!=L'\\'&&path[n-1]!=L'/')path[n++]=L'\\';
    for(i=0;metadata_name[i];i++)path[n+i]=metadata_name[i];path[n+i]=0;
    h=io->open(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE||!h)goto done;
    read=io->read(h,metadata,sizeof(metadata),&got,0);closed=io->close(h);
    if(!read||!closed||got!=640)goto done;
    for(i=0;save_name[i];i++)path[n+i]=save_name[i];path[n+i]=0;
    good=lid_reload_bound_source_init(s,json,io,metadata,640,peer,path,epoch,request);
done:
    io->release(path);return good;
}
#endif
