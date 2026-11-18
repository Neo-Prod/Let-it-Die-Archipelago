#ifndef LID_DECAL_ENGINE_ADAPTER_H
#define LID_DECAL_ENGINE_ADAPTER_H
/* Candidate static-ABI binding. Host proves SHA/session/current character and
   short reservation. No hooks, I/O, RNG, purchase, save or UI calls. Large
   fields MUST live in BSS, never as an automatic stack variable. */
typedef struct LidDecalEngineString {const unsigned short*data;unsigned num,max;} LidDecalEngineString;
typedef struct LidDecalEngineArray {LidDecalEngineString*data;unsigned num,max;} LidDecalEngineArray;
typedef unsigned char(WINAPI*LidDecalEngineGrant)(void*,const LidDecalEngineArray*,unsigned char);
typedef unsigned char(WINAPI*LidDecalEngineProject)(void*,void*);
typedef struct LidDecalEngineAdapter {
    BYTE*exe,*userdata,*payload;void*owner;
    int(*reserved)(void*);void*(*current_userdata)(void*);
    int(*readable)(const void*,size_t);int(*executable)(const void*);
    unsigned verified_build,names_ready;
    LidDecalSnapshot*scratch;
    unsigned short names[208][64];LidDecalEngineString items[100000];
} LidDecalEngineAdapter;
static int lid_decal_engine_guard(void*context){
    static const BYTE grant[16]={0x48,0x8b,0xc4,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x50,0x48,0xc7,0x40,0xb8};
    static const BYTE project[16]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20};
    LidDecalEngineAdapter*e=context;BYTE*repo;BYTE*vt;
    if(!e||!e->exe||e->verified_build!=1||!e->reserved||!e->current_userdata||!e->readable||!e->executable||
       e->reserved(e->owner)!=1||!e->userdata||e->current_userdata(e->owner)!=e->userdata)return 0;
    if(!e->readable(e->exe+0x134b210,16)||!e->readable(e->exe+0x13c1aa0,16)||
       !e->executable(e->exe+0x134b210)||!e->executable(e->exe+0x13c1aa0)||
       !lid_wire_equal(e->exe+0x134b210,grant,16)||!lid_wire_equal(e->exe+0x13c1aa0,project,16))return 0;
    repo=e->exe+0x27eac78;
    if(!e->readable(repo,0x60)||lid_wire_r64(repo)!=(size_t)(e->exe+0x2195460)||
       !e->readable(e->exe+0x2195460,8)||lid_wire_r64(e->exe+0x2195460)!=(size_t)(e->exe+0x13979a0)||
       !e->readable(e->exe+0x2195410+0x38,8)||lid_wire_r64(e->exe+0x2195410+0x38)!=(size_t)(e->exe+0x126c7e0))return 0;
    if(!e->readable(e->userdata,0x7c)||lid_wire_r64(e->userdata)!=(size_t)(e->exe+0x21fb700)||
       (BYTE*)(size_t)lid_wire_r64(e->userdata+0x74)!=e->payload||!e->payload||!e->readable(e->payload+0x32c,16))return 0;
    vt=e->exe+0x21fb700;
    return e->readable(vt+0x698,16)&&lid_wire_r64(vt+0x698)==(size_t)(e->exe+0x1573c10)&&
        lid_wire_r64(vt+0x6a0)==(size_t)(e->exe+0x1573b50);
}
static int lid_decal_engine_stock(void*context,LidDecalSnapshot*out){
    LidDecalEngineAdapter*e=context;if(out)out->valid=0;
    return out&&lid_decal_engine_guard(e)&&lid_decal_stock_read(out,e->exe+0x27eac78,e->exe+0x2195460,e->exe+0x2195410,e->readable);
}
static int lid_decal_engine_view(void*context,LidDecalSnapshot*out){
    LidDecalEngineAdapter*e=context;if(out)out->valid=0;
    return out&&lid_decal_engine_guard(e)&&lid_decal_read(out,e->payload+0x32c,0,1,e->readable);
}
static int lid_decal_engine_grant_with(void*context,const unsigned*delta,LidDecalEngineGrant grant){
    LidDecalEngineAdapter*e=context;unsigned i,j,k,total=0,index;size_t n;LidDecalEngineArray list;
    /* Test seam supplies an inert callback. The production wrapper ALWAYS
       binds its static verified RVA, never a pointer received from a file. */
    if(!delta||!grant||!lid_decal_engine_guard(e)||!e->scratch)return 0;
    for(i=0;i<208;i++){if(delta[i]>100000||total>100000-delta[i])return 0;total+=delta[i];}
    if(!total)return 1;
    if(!lid_decal_engine_stock(e,e->scratch))return 0;
    for(n=0;n<e->scratch->count;n++){index=lid_decal_index(e->scratch->rows[n].id);if(index<208&&e->scratch->rows[n].count>0x7fffffffu-delta[index])return 0;}
    if(!e->names_ready){
        for(i=0;i<208;i++){for(j=0;lid_decal_ids[i][j];j++){if(j>=63)return 0;e->names[i][j]=(unsigned char)lid_decal_ids[i][j];}e->names[i][j]=0;}
        e->names_ready=1;
    }
    k=0;for(i=0;i<208;i++){for(j=0;e->names[i][j];j++){}for(n=0;n<delta[i];n++)e->items[k++]=(LidDecalEngineString){e->names[i],j+1,j+1};}
    list=(LidDecalEngineArray){e->items,k,k};
    return lid_decal_engine_guard(e)&&grant(e->exe+0x27eac78,&list,0)==1;
}
static int lid_decal_engine_grant(void*context,const unsigned*delta){
    LidDecalEngineAdapter*e=context;
    return e&&e->exe&&lid_decal_engine_grant_with(e,delta,(LidDecalEngineGrant)(e->exe+0x134b210));
}
static int lid_decal_engine_project_with(void*context,LidDecalEngineProject project){
    LidDecalEngineAdapter*e=context;
    return project&&lid_decal_engine_guard(e)&&project(e->exe+0x27eac78,e->payload+0x32c)==1;
}
static int lid_decal_engine_project(void*context){
    LidDecalEngineAdapter*e=context;
    return e&&e->exe&&lid_decal_engine_project_with(e,(LidDecalEngineProject)(e->exe+0x13c1aa0));
}
static int lid_decal_engine_ops(LidDecalEngineAdapter*e,LidDecalCommitOps*out){
    if(!out||!lid_decal_engine_guard(e))return 0;
    *out=(LidDecalCommitOps){e,lid_decal_engine_guard,lid_decal_engine_stock,lid_decal_engine_view,lid_decal_engine_grant,lid_decal_engine_project};return 1;
}
#endif
