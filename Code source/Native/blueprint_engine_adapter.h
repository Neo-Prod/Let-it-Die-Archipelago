#ifndef LID_BLUEPRINT_ENGINE_ADAPTER_H
#define LID_BLUEPRINT_ENGINE_ADAPTER_H
/* Candidate engine binding; include winlite.h and blueprint_native_commit.h.
   Host must verify the supported SHA before setting verified_build=1 and prove
   current owner/thread/mutex/request+save reservation in reserved().
   Not included by mod.c. No hook, worker, save or filesystem operation. */
typedef unsigned char (WINAPI *LidBlueprintEngineUpsert)(const unsigned short*,unsigned,
    const unsigned short*,const unsigned short*,unsigned,const unsigned short*,unsigned);
typedef struct LidBlueprintEngineAdapter {
    BYTE* exe;
    void* owner;
    int (*reserved)(void*);
    int (*readable)(const void*,size_t);
    int (*executable)(const void*);
    LidBlueprintNameIndex names;
    unsigned verified_build;
} LidBlueprintEngineAdapter;
static int lid_blueprint_engine_guard(void* context){
    static const BYTE prefix[18]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xb9,0x48,0x81};
    LidBlueprintEngineAdapter* engine=context;BYTE* repo;
    if(!engine||engine->verified_build!=1||!engine->exe||!engine->reserved||!engine->readable||!engine->executable||
       engine->reserved(engine->owner)!=1)return 0;
    repo=engine->exe+0x27e3000;
    if(!engine->readable(engine->exe+0x12fb880,18)||!engine->executable(engine->exe+0x12fb880)||
       !lid_wire_equal(engine->exe+0x12fb880,prefix,18)||!engine->readable(repo,0x60)||
       (const void*)(size_t)lid_wire_r64(repo)!=engine->exe+0x2167760||
       !engine->readable(engine->exe+0x2167760,8)||
       (const void*)(size_t)lid_wire_r64(engine->exe+0x2167760)!=engine->exe+0x12fa7a0)return 0;
    return 1;
}
static int lid_blueprint_engine_read(void* context,LidBlueprintNativeSnapshot* out){
    LidBlueprintEngineAdapter* engine=context;
    if(out){out->valid=0;out->count=out->known=0;}
    if(!out||!lid_blueprint_engine_guard(engine))return 0;
    return lid_blueprint_native_read(out,&engine->names,engine->exe+0x27e3000,
        engine->exe+0x2167760,engine->exe+0x2167700,engine->readable);
}
static int lid_blueprint_engine_apply_with(void* context,const LidBlueprintNativeRecord* desired,
                                          LidBlueprintEngineUpsert upsert){
    LidBlueprintEngineAdapter* engine=context;
    LidBlueprintNativeRecord expected;LidBlueprintAction action;
    /* Also used by isolated ABI tests; production wrapper below always binds
       the verified RVA, never a function pointer received from AP/files. */
    if(!desired||!upsert||!lid_blueprint_engine_guard(engine)||desired->state.part>=976||
       (desired->state.level!=1&&desired->state.level!=2))return 0;
    action.row=desired->state;action.replaces_existing=0;
    lid_bp_expected_action(&expected,&action);
    if(desired->state.level==1){
        if(desired->state.research!=LID_BP_FINISHED||desired->state.receive!=LID_BP_CHARGE||
           desired->state.announced!=1||desired->state.checked)return 0;
    }else if(desired->state.research!=LID_BP_LEVELUP||desired->state.receive!=LID_BP_UNKNOWN||
              desired->state.announced||desired->state.checked)return 0;
    if(!lid_wire_equal((const BYTE*)&expected,(const BYTE*)desired,sizeof(expected)))return 0;
    return upsert(desired->ptid,desired->state.level,desired->research_name,desired->before_ptid,
                  desired->before_level,desired->receive_name,desired->state.announced)==1;
}
static int lid_blueprint_engine_upsert(void* context,const LidBlueprintNativeRecord* desired){
    LidBlueprintEngineAdapter* engine=context;
    if(!engine||!engine->exe)return 0;
    return lid_blueprint_engine_apply_with(engine,desired,(LidBlueprintEngineUpsert)(engine->exe+0x12fb880));
}
static int lid_blueprint_engine_ops(LidBlueprintEngineAdapter* engine,LidBlueprintCommitOps* out){
    if(!out||!lid_blueprint_engine_guard(engine))return 0;
    *out=(LidBlueprintCommitOps){engine,lid_blueprint_engine_guard,lid_blueprint_engine_read,lid_blueprint_engine_upsert};return 1;
}
#endif
