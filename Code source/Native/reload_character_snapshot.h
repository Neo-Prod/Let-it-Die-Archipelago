#ifndef LID_RELOAD_CHARACTER_SNAPSHOT_H
#define LID_RELOAD_CHARACTER_SNAPSHOT_H
#include "reload_json_binding.h"
#include "reload_json_numbers.h"
#include "reload_load_trace.h"
/* Character persistence proof only. NEVER apply a normalized comparison copy.
   Source BRG/schema/representation preflight must already have succeeded for
   this epoch/request. This is not a complete schema validator or save/ACK proof.
   Borrowed loader-root/children are never attached, released or retained. */
typedef struct LidReloadCharacterSnapshot {
    LidReloadPoolString expected[2];
    unsigned short uid[129];
    unsigned long long epoch,request;
    unsigned initialized,captured,closed,verified;
} LidReloadCharacterSnapshot;
static int lid_reload_json_window(LidReloadJsonBinding*b){
    return b&&b->get&&b->string&&b->encode&&b->parse&&b->equal&&b->native_free&&
        b->allocate&&b->release&&lid_reload_pool_window(&b->pool);
}
static int lid_reload_json_string_ok(LidReloadJsonBinding*b,const LidReloadPoolString*s){
    size_t i;
    if(!b||!s||s->num<1||s->num>s->capacity||s->capacity>16*1024*1024||!s->data||
       !b->pool.readable(s->data,(size_t)s->num*2)||s->data[s->num-1])return 0;
    for(i=0;i<(size_t)s->num-1;i++)if(!s->data[i])return 0;
    return 1;
}
static int lid_reload_json_free_string(LidReloadJsonBinding*b,LidReloadPoolString*s){
    if(!b||!s||!b->native_free)return 0;
    if(s->data){
        /* Unknown outputs are leaked/refused rather than freed speculatively. */
        if(!b->pool.readable(s->data,2)){b->pool.poisoned=1;return 0;}
        b->native_free(s->data);
    }
    *s=(LidReloadPoolString){0};return 1;
}
static void*lid_reload_json_child(LidReloadJsonBinding*b,void*parent,const char*key){
    unsigned short storage[64];LidReloadPoolString name;size_t n=0;void*child;
    if(!lid_reload_json_window(b)||!key||!lid_reload_pool_object(&b->pool,parent))return 0;
    while(key[n]){if(n>=63||(unsigned char)key[n]>127)return 0;storage[n]=(unsigned char)key[n];n++;}
    storage[n]=0;name=(LidReloadPoolString){storage,(int)n+1,(int)n+1};
    child=b->get(parent,&name);
    if(child&&!lid_reload_pool_object(&b->pool,child)){b->pool.poisoned=1;return 0;}
    return child;
}
static int lid_reload_json_uid(LidReloadJsonBinding*b,void*object,const unsigned short*expected){
    unsigned short key[4]={'u','i','d',0};LidReloadPoolString name={key,4,4},out={0};
    size_t i,start=0;int good=0;
    if(!expected||!lid_reload_json_window(b)||!lid_reload_pool_object(&b->pool,object))return 0;
    if(b->string(object,&out,&name)!=&out)goto done;
    if(!lid_reload_json_string_ok(b,&out)||out.num>131)goto done;
    /* JsonObject stores numeric primitives as raw ValueMap strings prefixed
       by backslash-hash (GetIntValue140251390 strips these two units).
       Literal JSON strings with this prefix are refused by source preflight. */
    if(out.num>=3&&out.data[0]=='\\'&&out.data[1]=='#')start=2;
    if((size_t)out.num-start>129)goto done;
    for(i=start;i<(size_t)out.num;i++){
        if(out.data[i]!=expected[i-start])goto done;
        if(!out.data[i]){good=1;break;}
    }
done:
    if(!lid_reload_json_free_string(b,&out))good=0;
    return good;
}
static int lid_reload_snapshot_init(LidReloadCharacterSnapshot*s,unsigned long long epoch,
                                   unsigned long long request,const unsigned short*uid,size_t length){
    size_t i;
    if(!s||s->initialized||!epoch||!request||!uid||!length||length>128)return 0;
    for(i=0;i<length;i++)if(uid[i]<'0'||uid[i]>'9')return 0;
    *s=(LidReloadCharacterSnapshot){0};
    for(i=0;i<length;i++)s->uid[i]=uid[i];
    s->epoch=epoch;s->request=request;s->initialized=1;return 1;
}
static int lid_reload_snapshot_close(LidReloadJsonBinding*b,LidReloadCharacterSnapshot*s){
    int good=1;unsigned i;
    if(!b||!s||!s->initialized||s->closed)return 0;
    s->closed=1;s->verified=0;
    for(i=0;i<2;i++)if(!lid_reload_json_free_string(b,&s->expected[i]))good=0;
    if(!good)b->pool.poisoned=1;
    return good;
}
static int lid_reload_snapshot_capture(LidReloadJsonBinding*b,LidReloadCharacterSnapshot*s,
    void*borrowed_root,unsigned long long epoch,unsigned long long request,int source_preflight_approved){
    static const char*const required[]={"user","soul","gameflg","bodyuser","floor","diedchara",
        "zombie","part","item","mushroom","beast","playlog","gi","dummy","ranking","warteam","fort","team"};
    void*user=0,*soul=0,*body=0,*child;size_t i;
    LID_R36_SET(capture_stage,1);
    if(!s||!s->initialized||s->captured||s->closed||s->epoch!=epoch||s->request!=request||
       source_preflight_approved!=1||!lid_reload_json_window(b))return 0;
    LID_R36_SET(capture_stage,2);
    for(i=0;i<sizeof(required)/sizeof(required[0]);i++){
        LID_R36_SET(capture_index,i+1);
        child=lid_reload_json_child(b,borrowed_root,required[i]);if(!child)goto fail;
        if(i==0)user=child;if(i==1)soul=child;if(i==3)body=child;
    }
    if(LID_R36_PHASE(capture_stage,3,!lid_reload_json_uid(b,user,s->uid))||
       LID_R36_PHASE(capture_stage,4,!lid_reload_json_uid(b,soul,s->uid)))goto fail;
    LID_R36_SET(capture_stage,5);
    b->encode(&s->expected[0],soul);
    if(!lid_reload_json_string_ok(b,&s->expected[0]))goto fail;
    LID_R36_SET(capture_stage,6);
    b->encode(&s->expected[1],body);
    if(!lid_reload_json_string_ok(b,&s->expected[1]))goto fail;
    s->captured=1;LID_R36_SET(capture_stage,7);return 1;
fail:
    (void)lid_reload_snapshot_close(b,s);return 0;
}
typedef struct LidReloadCompareTrace {
    unsigned stage,side,cleanup_ok,equal_called,equal_result;
    unsigned expected_units,actual_units;
} LidReloadCompareTrace;
enum {LID_JC_NONE=0,LID_JC_WINDOW,LID_JC_STRING,LID_JC_ALLOCATE,LID_JC_NORMALIZE,
      LID_JC_PARSE,LID_JC_PROFILE,LID_JC_ALIAS,LID_JC_EQUAL,LID_JC_CLEANUP,LID_JC_POISON,LID_JC_VERIFIED};
static const char*lid_reload_compare_reason(const LidReloadCompareTrace*t){
    static const char*const names[]={"not_entered","window","string","allocate","normalize","parse",
        "object_profile","object_alias","different","cleanup","pool_poison","verified"};
    return t&&t->stage<sizeof(names)/sizeof(names[0])?names[t->stage]:"unknown";
}
static int lid_reload_json_compare_trace(LidReloadJsonBinding*b,const LidReloadPoolString*expected,
                                   const LidReloadPoolString*actual,LidReloadCompareTrace*trace){
    const LidReloadPoolString*source[2]={expected,actual};
    LidReloadPoolString normalized[2]={{0},{0}};void*object[2]={0,0};
    size_t n,cap,written;unsigned i;int good=0,cleanup=1;
    LidReloadCompareTrace local={0};if(!trace)trace=&local;
    *trace=(LidReloadCompareTrace){0};trace->stage=LID_JC_WINDOW;
    if(!lid_reload_json_window(b))return 0;
    for(i=0;i<2;i++){
        trace->side=i+1;trace->stage=LID_JC_STRING;
        if(!lid_reload_json_string_ok(b,source[i])||source[i]->num<3)goto done;
        if(i)trace->actual_units=(unsigned)source[i]->num-1;
        else trace->expected_units=(unsigned)source[i]->num-1;
        n=(size_t)source[i]->num-1;cap=n*2+32;
        trace->stage=LID_JC_ALLOCATE;
        normalized[i].data=b->allocate(b->memory_context,cap*2);
        if(!normalized[i].data)goto done;
        trace->stage=LID_JC_NORMALIZE;
        if(!lid_reload_json_numbers(source[i]->data,n,normalized[i].data,cap,&written))goto done;
        normalized[i].num=normalized[i].capacity=(int)written+1;
        trace->stage=LID_JC_PARSE;
        object[i]=b->parse(&normalized[i]);if(!object[i])goto done;
        trace->stage=LID_JC_PROFILE;
        if(!lid_reload_pool_object(&b->pool,object[i])){b->pool.poisoned=1;goto done;}
        trace->stage=LID_JC_ALIAS;
        if(i&&object[i]==object[0]){object[i]=0;b->pool.poisoned=1;goto done;}
    }
    trace->side=0;trace->stage=LID_JC_EQUAL;trace->equal_called=1;
    trace->equal_result=(unsigned)b->equal(object[0],object[1],0);
    good=trace->equal_result==1;
done:
    for(i=2;i>0;i--){unsigned j=i-1;
        if(object[j]){
            if(!lid_reload_pool_window(&b->pool)||!lid_reload_pool_object(&b->pool,object[j]))cleanup=0;
            else if(b->pool.call.release(b->pool.pool,object[j])!=1)cleanup=0;
        }
        if(normalized[j].data)b->release(b->memory_context,normalized[j].data);
    }
    if(!cleanup)b->pool.poisoned=1;
    trace->cleanup_ok=(unsigned)cleanup;
    if(good&&!cleanup)trace->stage=LID_JC_CLEANUP;
    else if(good&&b->pool.poisoned)trace->stage=LID_JC_POISON;
    else if(good)trace->stage=LID_JC_VERIFIED;
    return good&&cleanup&&!b->pool.poisoned;
}
static int lid_reload_json_compare(LidReloadJsonBinding*b,const LidReloadPoolString*expected,
                                   const LidReloadPoolString*actual){
    return lid_reload_json_compare_trace(b,expected,actual,0);
}
static int lid_reload_snapshot_readback(LidReloadJsonBinding*b,LidReloadCharacterSnapshot*s,
                                       unsigned long long epoch,unsigned long long request){
    LidReloadPoolScope scope={0};LidReloadPoolString actual[2]={{0},{0}};
    void*children[2];unsigned i;int good=0,cleanup=1;
    if(!s||!s->initialized||!s->captured||s->closed||s->verified||
       s->epoch!=epoch||s->request!=request||!lid_reload_json_window(b))return 0;
    if(!lid_reload_pool_scope_build(&b->pool,&scope))return 0;
    children[0]=lid_reload_json_child(b,scope.root,"soul");
    children[1]=lid_reload_json_child(b,scope.root,"bodyuser");
    if(!children[0]||!children[1]||!lid_reload_json_uid(b,children[0],s->uid))goto done;
    for(i=0;i<2;i++){
        b->encode(&actual[i],children[i]);
        if(!lid_reload_json_string_ok(b,&actual[i])||!lid_reload_json_compare(b,&s->expected[i],&actual[i]))goto done;
    }
    good=1;
done:
    for(i=0;i<2;i++)if(!lid_reload_json_free_string(b,&actual[i]))cleanup=0;
    if(!lid_reload_pool_scope_close(&b->pool,&scope))cleanup=0;
    s->verified=(unsigned)(good&&cleanup&&!b->pool.poisoned);return (int)s->verified;
}
#endif
