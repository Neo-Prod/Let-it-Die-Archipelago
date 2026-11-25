#ifndef LID_MASTERY_RUNTIME_H
#define LID_MASTERY_RUNTIME_H
#include "floor_reflection.h"
/* Native exec pointer hooks only; no VM/global event hook, save/stat/level writes.
   Files are read by the existing worker, never on the game thread. */
typedef void(WINAPI*LidMasteryExec)(void*,void*,void*);
enum {LID_MASTERY_READWRITE=4};
typedef struct LidMasteryMeta {void*userdata_class;void*get_exp;void*set_level;LidFloorLayout query;} LidMasteryMeta;
static LidMasteryMeta g_mastery_meta;
static LidMasteryExec g_mastery_exp_original,g_mastery_set_original;
static void(WINAPI*g_mastery_error_set)(DWORD);
static unsigned g_mastery_installed,g_mastery_busy,g_mastery_snapshot,g_mastery_warn;
static unsigned g_mastery_xp_logged;
static unsigned g_mastery_observed[LID_MASTERY_FAMILIES];
static uint64_t g_mastery_next_boundary;
static void*g_mastery_game_class;static unsigned g_mastery_user_offset;
static void*g_mastery_caller_node;static void*g_mastery_pawn_class;
static unsigned g_mastery_xp_offset,g_mastery_part_offset,g_mastery_record_offset;
static int g_mastery_local_layout;
static void WINAPI mastery_exp_exec(void*,void*,void*);
static void WINAPI mastery_set_exec(void*,void*,void*);
static int mastery_native_string(const RawFString*s){return s&&s->num>=0&&s->num<=256&&s->max>=s->num&&s->max<=65536&&
    ((!s->num&&!s->data)||(s->data&&readable_range(s->data,s->num?(size_t)s->num*2:2)&&(!s->num||!s->data[s->num-1])));}
static int mastery_native_family(const RawFString*s){
    char text[11];if(!mastery_native_string(s)||s->num!=11)return -1;
    for(unsigned i=0;i<11;i++){if(s->data[i]>127)return -1;text[i]=(char)s->data[i];}return lid_mastery_family_index(text);
}
static int mastery_native_part(const RawFString*s){
    if(!mastery_native_string(s)||s->num<2)return -1;
    unsigned lo=0,hi=sizeof(lid_mastery_parts)/sizeof(lid_mastery_parts[0]);
    while(lo<hi){unsigned mid=lo+(hi-lo)/2,i=0;int cmp=0;
        while(i<(unsigned)s->num){wchar_t a=s->data[i],b=lid_mastery_parts[mid].part[i];if(a!=b){cmp=a<b?-1:1;break;}if(!a)break;i++;}
        if(!cmp)return (int)lid_mastery_parts[mid].family;if(cmp<0)hi=mid;else lo=mid+1;
    }return -1;
}
static int mastery_native_ready(DWORD owner){
    R15Context context;
    return owner&&g_get_tid&&g_get_tid()==owner&&owner==g_checks_diag.owner&&checks_session()&&
        __atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)==R26_ACTIVE&&!__atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)&&
        __atomic_load_n(&g_checks_delivery.mastery.state,__ATOMIC_ACQUIRE)==1&&r25_host_context(&g_progress_host,&context)&&
        r15_same_context(&context,&g_checks_delivery.queue.context);
}
static void*mastery_native_userdata(void){
    BYTE*game=progress_native_game(g_progress_event_exe);unsigned offset;
    if(!game||!readable_range(game,0x58))return 0;void*cls=*(void**)(game+0x50);if(!cls)return 0;
    if(g_mastery_game_class!=cls){
        if(!lid_floor_field(game,"mUserData","ObjectProperty",8,&offset,&g_progress_event_ops))return 0;
        g_mastery_game_class=cls;g_mastery_user_offset=offset;
    }
    if(!readable_range(game+g_mastery_user_offset,8))return 0;
    void*u=*(void**)(game+g_mastery_user_offset);
    return u&&readable_range(u,0x58)&&((g_mastery_meta.userdata_class&&*(void**)((BYTE*)u+0x50)==g_mastery_meta.userdata_class)||
        lid_floor_is_a(u,"BrgUserData",&g_progress_event_ops))?u:0;
}
static int mastery_native_field(void*node,const char*name,const char*type,unsigned width,unsigned*offset,void**property){
    if(!node||!readable_range(node,0x8c))return 0;unsigned extent=*(unsigned*)((BYTE*)node+0x88);
    if(!extent||extent>65536)return 0;BYTE*p=*(BYTE**)((BYTE*)node+0x80);unsigned visits=0,found=0,at=0;void*match=0;
    while(p){if(++visits>128||!readable_range(p,0x98))return 0;
        if(progress_event_name_is(p,name)){if(found++||!lid_floor_property(p,type,width,extent,&at,&g_progress_event_ops))return 0;match=p;}
        p=*(BYTE**)(p+0x60);
    }if(found!=1)return 0;*offset=at;if(property)*property=match;return 1;
}
static int mastery_native_structure(void*property){
    if(!property||!readable_range(property,0xb8))return 0;BYTE*s=*(BYTE**)((BYTE*)property+0xb0);
    if(!s||!lid_event_class_is(s,"ScriptStruct",&g_progress_event_ops)||!progress_event_name_is(s,"BrgDbMasterLvl")||!readable_range(s,0x8c)||*(unsigned*)(s+0x88)!=28)return 0;
    const char*names[]={"mPtarmtp","mAbp","mLvl","mIs_checked"};const char*types[]={"StrProperty","IntProperty","IntProperty","IntProperty"};
    unsigned offsets[]={0,16,20,24},widths[]={16,4,4,4};
    for(unsigned i=0;i<4;i++){unsigned at;void*p;
        if(!mastery_native_field(s,names[i],types[i],widths[i],&at,&p)||at!=offsets[i])return 0;
    }return 1;
}
static int mastery_native_query(void*user,unsigned family,int*level,int*abp){
    BYTE out[28]={0};wchar_t id[11];RawFString input;LidFloorLayout*m=&g_mastery_meta.query;void**vt;
    if(family>=LID_MASTERY_FAMILIES||!user||!readable_range(user,0x58)||*(void**)((BYTE*)user+0x50)!=g_mastery_meta.userdata_class||
       !m->function||!readable_range(m->function,0xf8)||*(void**)((BYTE*)m->function+0xf0)!=g_progress_event_exe+0x1034950)return 0;
    vt=*(void***)user;
    if(!vt||!readable_range(vt,0x818)||vt[0x810/8]!=g_progress_event_exe+0x156e2f0||!executable_ptr(g_progress_event_exe,vt[0x810/8]))return 0;
    for(unsigned i=0;i<11;i++)id[i]=(wchar_t)lid_mastery_families[family][i];input=(RawFString){id,11,11};
    /* Same native backend/ABI as execGetMasterLevel. Avoid nested script VM
       dispatch inside AddExp; the getter copies the persisted28-byte row. */
    unsigned returned=((unsigned(WINAPI*)(void*,const RawFString*,void*))vt[0x810/8])(user,&input,out);
    int ok=returned==1&&mastery_native_family((RawFString*)out)==(int)family;
    if(ok){*abp=*(int*)(out+16);*level=*(int*)(out+20);ok=*level>=1&&*level<=20;}
    RawFString*owned=(RawFString*)out;if(owned->data&&mastery_native_string(owned))((void(WINAPI*)(void*))(g_progress_event_exe+0xdfb60))(owned->data);
    return returned==0?2:ok; /* A missing native record is not an earned mastery. */
}
static int mastery_native_emit(unsigned family,int level,int abp){
    const LidMasteryPolicy*p=&g_checks_delivery.mastery.policy;R15Frame f;
    if(!p->enabled||family>=LID_MASTERY_FAMILIES||!(p->mask&(1ull<<family))||level<1||level>20||abp<0||g_mastery_observed[family]>=(unsigned)level)return 1;
    r15_zero(&f,sizeof(f));f.kind=7;f.context=g_checks_delivery.queue.context;f.epoch=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
    if(!f.epoch)return 0;f.health=level;f.max_health=20;r15_copy(f.damage,lid_mastery_families[family],11);
    /* Back-pressure does not fault the existing queue. Snapshot retries later. */
    uint64_t tail=__atomic_load_n(&g_checks_delivery.queue.tail,__ATOMIC_RELAXED),received=__atomic_load_n(&g_checks_delivery.queue.received,__ATOMIC_ACQUIRE);
    if(tail-received>=R15_CAPACITY-1){g_mastery_snapshot=0;return 0;}
    if(r15_queue_push(&g_checks_delivery.queue,g_checks_diag.owner,&f)!=1){g_mastery_snapshot=0;return 0;}
    unsigned previous=g_mastery_observed[family];g_mastery_observed[family]=(unsigned)level;
    if(!previous||level==20){char text[140];size_t n=0;text[0]=0;
        append_ascii(text,sizeof(text),&n,"AP MASTERY R331: queued native level family=");append_ascii(text,sizeof(text),&n,lid_mastery_families[family]);
        append_ascii(text,sizeof(text),&n," level=");append_dec(text,sizeof(text),&n,(unsigned)level);text[n]=0;log_ascii(text);}
    return 1;
}
static int mastery_native_caller(void*self,void*frame,BYTE**locals,void**node){
    BYTE*pawn=0,*common=0;
    if(!mastery_native_ready(g_checks_diag.owner)||!g_mastery_installed||g_mastery_busy||!readable_range(frame,0x34))return 0;
    *node=*(void**)((BYTE*)frame+0x14);void*object=*(void**)((BYTE*)frame+0x1c);*locals=*(BYTE**)((BYTE*)frame+0x2c);
    if(!*node||!*locals||!object||!readable_range(object,0x58)||
       !rage_player_objects(g_progress_event_exe,&pawn,&common)||pawn!=object||self!=mastery_native_userdata())return 0;
    void*cls=*(void**)((BYTE*)object+0x50);
    if(g_mastery_caller_node!=*node||g_mastery_pawn_class!=cls){void*property;
        if(!progress_event_name_is(*node,"AddExp")||!lid_floor_is_a(object,"BrgPawn_CustomCharaPlayer",&g_progress_event_ops)||
           !readable_range(*node,0x48)||!progress_event_name_is(*(void**)((BYTE*)*node+0x40),"BrgPawn_CustomCharaPlayer")||
           lid_event_find(object,"AddExp",&g_progress_event_ops)!=*node||
           !mastery_native_field(*node,"inAbp","IntProperty",4,&g_mastery_xp_offset,0)||
           !mastery_native_field(*node,"inDbPtid","StrProperty",16,&g_mastery_part_offset,0)||
           !mastery_native_field(*node,"lDbMasterLvl","StructProperty",28,&g_mastery_record_offset,&property)||!mastery_native_structure(property))return 0;
        g_mastery_caller_node=*node;g_mastery_pawn_class=cls;g_mastery_local_layout=1;
    }return g_mastery_local_layout;
}
static void WINAPI mastery_exp_exec(void*self,void*frame,void*result){
    g_mastery_exp_original(self,frame,result);DWORD error=g_win.GetLastError();BYTE*locals;void*node;
    const LidMasteryPolicy*p=&g_checks_delivery.mastery.policy;
    if(p->multiplier!=1&&mastery_native_caller(self,frame,&locals,&node)&&
       progress_native_writable(locals+g_mastery_xp_offset,4)&&readable_range(locals+g_mastery_part_offset,16)){
        int family=mastery_native_part((RawFString*)(locals+g_mastery_part_offset)),level,abp;int*xp=(int*)(locals+g_mastery_xp_offset);
        if(family>=0&&*xp>0){g_mastery_busy=1;if(mastery_native_query(self,(unsigned)family,&level,&abp)==1){
            int before=*xp;*xp=lid_mastery_scaled_xp(*xp,p->multiplier,abp);
            if(!g_mastery_xp_logged++){char text[180];size_t n=0;text[0]=0;
                append_ascii(text,sizeof(text),&n,"AP MASTERY R331: native AddExp mastery XP base=");append_dec(text,sizeof(text),&n,(unsigned)before);
                append_ascii(text,sizeof(text),&n," scaled=");append_dec(text,sizeof(text),&n,(unsigned)*xp);text[n]=0;log_ascii(text);}
        }g_mastery_busy=0;}
    }g_mastery_error_set(error);
}
static void WINAPI mastery_set_exec(void*self,void*frame,void*result){
    g_mastery_set_original(self,frame,result);DWORD error=g_win.GetLastError();BYTE*locals;void*node;
    if(g_checks_delivery.mastery.policy.enabled&&readable_range(result,4)&&*(unsigned*)result==1&&mastery_native_caller(self,frame,&locals,&node)&&
       readable_range(locals+g_mastery_record_offset,28)){
        BYTE*record=locals+g_mastery_record_offset;int family=mastery_native_family((RawFString*)record);
        if(family>=0)(void)mastery_native_emit((unsigned)family,*(int*)(record+20),*(int*)(record+16));
    }g_mastery_error_set(error);
}
static int mastery_native_swap(void*function,unsigned rva,LidMasteryExec hook,LidMasteryExec*original){
    if(!function||!readable_range(function,0xf8)||!(*(unsigned*)((BYTE*)function+0xd0)&0x400)||*(unsigned short*)((BYTE*)function+0xd4)||
       *(void**)((BYTE*)function+0xf0)!=g_progress_event_exe+rva)return 0;
    DWORD protection,ignored;void**slot=(void**)((BYTE*)function+0xf0);
    if(!g_win.VirtualProtect(slot,8,LID_MASTERY_READWRITE,&protection))return 0;
    *original=(LidMasteryExec)*slot;__atomic_store_n(slot,(void*)hook,__ATOMIC_RELEASE);
    if(!g_win.VirtualProtect(slot,8,protection,&ignored)){__atomic_store_n(slot,(void*)*original,__ATOMIC_RELEASE);return 0;}
    return 1;
}
static int mastery_native_install(void*user){
    static const char*names[]={"ptarmtp","MstLvl","ReturnValue"};static const char*types[]={"StrProperty","StructProperty","BoolProperty"};
    static const unsigned widths[]={16,28,4};LidMasteryMeta m={0};unsigned at;void*property;
    if(!user||!readable_range(user,0x58)||!lid_floor_function(user,"GetMasterLevel",names,types,widths,3,&m.query,&g_progress_event_ops)||
       !mastery_native_field(m.query.function,"MstLvl","StructProperty",28,&at,&property)||!mastery_native_structure(property)||
       !readable_range(m.query.function,0xf8)||!(*(unsigned*)((BYTE*)m.query.function+0xd0)&0x400)||
       *(unsigned short*)((BYTE*)m.query.function+0xd4)||*(void**)((BYTE*)m.query.function+0xf0)!=g_progress_event_exe+0x1034950)return 0;
    m.userdata_class=*(void**)((BYTE*)user+0x50);m.get_exp=lid_event_find(user,"GetExp",&g_progress_event_ops);m.set_level=lid_event_find(user,"SetMasterLevel",&g_progress_event_ops);
    void*kernel=lid_find_module_ascii("kernel32.dll");if(!kernel||!g_win.GetLastError||!g_win.VirtualProtect||
       (g_mastery_error_set=(void(WINAPI*)(DWORD))lid_get_export(kernel,"SetLastError"))==0)return 0;
    static const BYTE release_prefix[16]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x0d,0x70,0x40,0x73,0x02};
    if(!readable_range(g_progress_event_exe+0xdfb60,16)||!lid_wire_equal(g_progress_event_exe+0xdfb60,release_prefix,16))return 0;
    g_mastery_meta=m;
    if(!mastery_native_swap(m.set_level,0x1035300,mastery_set_exec,&g_mastery_set_original))return 0;
    if(!mastery_native_swap(m.get_exp,0x107afa0,mastery_exp_exec,&g_mastery_exp_original)){
        DWORD protection,ignored;void**slot=(void**)((BYTE*)m.set_level+0xf0);
        if(g_win.VirtualProtect(slot,8,LID_MASTERY_READWRITE,&protection)){__atomic_store_n(slot,(void*)g_mastery_set_original,__ATOMIC_RELEASE);(void)g_win.VirtualProtect(slot,8,protection,&ignored);}return 0;
    }
    g_mastery_installed=1;log_ascii("AP MASTERY R331: scoped native XP/level observers installed; native reward/display/save path retained.");return 1;
}
static void mastery_native_boundary(DWORD owner){
    if(g_mastery_snapshot||!owner||!g_get_tid||g_get_tid()!=owner||g_mastery_busy||
       __atomic_load_n(&g_checks_delivery.mastery.state,__ATOMIC_ACQUIRE)!=1||!g_checks_delivery.disk.ops.clock)return;
    uint64_t now=g_checks_delivery.disk.ops.clock();
    if(now<g_mastery_next_boundary)return;g_mastery_next_boundary=now+1000;
    if(!mastery_native_ready(owner))return;
    const LidMasteryPolicy*p=&g_checks_delivery.mastery.policy;if(!p->enabled&&p->multiplier==1){g_mastery_snapshot=1;return;}
    void*user=mastery_native_userdata();
    /* Exact read-only equivalent of current UBrgUserData.IsValidNative
       VA14157d540: loaded bit1, not-invalid bit2, payload pointer74. */
    if(!user||!readable_range(user,0x480)||(*(unsigned*)((BYTE*)user+0x47c)&3)!=1||!*(void**)((BYTE*)user+0x74))return;
    if(!g_mastery_installed&&!mastery_native_install(user)){if(!g_mastery_warn++){log_ascii("AP MASTERY R331: typed native metadata/exec binding refused; existing gameplay untouched.");}return;}
    if(!p->enabled){g_mastery_snapshot=1;return;}
    g_mastery_busy=1;int complete=1;
    for(unsigned i=0;i<LID_MASTERY_FAMILIES;i++)if(p->mask&(1ull<<i)){int level,abp;int found=mastery_native_query(user,i,&level,&abp);
        if(!found||(found==1&&!mastery_native_emit(i,level,abp)))complete=0;}
    g_mastery_busy=0;if(complete){g_mastery_snapshot=1;log_ascii("AP MASTERY R331: persisted native mastery snapshot observed; checks queued through existing delivery.");}
}
#endif
