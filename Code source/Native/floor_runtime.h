#ifndef LID_FLOOR_RUNTIME_H
#define LID_FLOOR_RUNTIME_H
#include "floor_reflection.h"
/* Gameplay-only access wrapper. Original exec always runs exactly once.
   No periodic scene scan, network requests, save writes or native unlocking. */
typedef void(WINAPI*LidFloorExec)(void*,void*,void*);
static Hook g_floor_access_hook;
static LidFloorExec g_floor_access_original;
static LidFloorMeta g_floor_meta;
static DWORD g_floor_owner;
static void(WINAPI*g_floor_error_set)(DWORD);
static unsigned g_floor_install_state,g_floor_busy,g_floor_warning;
static void floor_native_note_block(unsigned from,unsigned to,unsigned applied){
    char text[220];size_t n=0;text[0]=0;
    append_ascii(text,sizeof(text),&n,"AP FLOOR: native barrier from=");append_dec(text,sizeof(text),&n,from);
    append_ascii(text,sizeof(text),&n," to=");append_dec(text,sizeof(text),&n,to);
    append_ascii(text,sizeof(text),&n," applied=");append_dec(text,sizeof(text),&n,applied);
    append_ascii(text,sizeof(text),&n," required=");append_dec(text,sizeof(text),&n,lid_floor_required(to));text[n]=0;log_ascii(text);
}
static void WINAPI floor_access_exec(void*,void*,void*);
static int floor_native_read(void*context,BYTE*out,size_t capacity,size_t*size){
    (void)context;LidBridgeIo*io=&g_progress_bridge_io;static const wchar_t name[]=L"floor_target.bin";
    wchar_t path[1120];size_t n=0;DWORD got=0;BOOL ok,closed;HANDLE f;
    if(size)*size=0;if(!out||!size||capacity!=321||!io->initialized)return -1;
    while(n<999&&io->directory[n]){path[n]=io->directory[n];n++;}if(!n||n==999)return -1;
    for(unsigned i=0;name[i];i++)path[n++]=name[i];path[n]=0;
    f=io->ops.open(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(f==INVALID_HANDLE_VALUE){DWORD e=io->ops.error();return e==2||e==3?0:-1;}
    ok=io->ops.read(f,out,321,&got,0);closed=io->ops.close(f);if(!ok||!closed||got!=320)return -1;*size=got;return 1;
}
static int floor_native_prepare(void*context){
    (void)context;BYTE*exe=g_progress_event_exe;
    static const BYTE prefix[21]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x42,0x24,0xc6,0x44,0x24,0x40,0};
    static const BYTE free_prefix[16]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x0d,0x70,0x40,0x73,0x02};
    if(g_floor_install_state)return g_floor_install_state==1;
    if(!exe||!g_ap_mode_active||!g_progress_runtime_mutex.ops.thread_id||!g_progress_host.grant_published||!g_progress_host.peer.granted||g_progress_host.poisoned)return 0;
    DWORD tid=g_progress_runtime_mutex.ops.thread_id();if(!tid||tid!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return 0;
    void*kernel=lid_find_module_ascii("kernel32.dll");
    if(!kernel||!g_win.GetLastError||(g_floor_error_set=(void(WINAPI*)(DWORD))lid_get_export(kernel,"SetLastError"))==0)return 0;
    g_floor_install_state=2;
    if(!readable_range(exe+0xdfb60,16)||!lid_wire_equal(exe+0xdfb60,free_prefix,16)||!executable_ptr(exe,exe+0xdfb60)||
       !hook_install(&g_floor_access_hook,exe+0xdc6910,(void*)floor_access_exec,prefix,sizeof(prefix))){
        log_ascii("AP FLOOR: native access hook refused; Floor target retained pending.");return 0;
    }
    g_floor_access_original=(LidFloorExec)g_floor_access_hook.trampoline;
    __atomic_store_n(&g_floor_owner,tid,__ATOMIC_RELEASE);g_floor_install_state=1;
    log_ascii("AP FLOOR: scoped upward-escalator access guard installed; existing native restrictions retained.");return 1;
}
static int floor_native_string_valid(const RawFString*s){
    return s&&s->num>=0&&s->num<=4096&&s->max>=s->num&&s->max<=65536&&
        ((!s->num&&!s->data)||(s->data&&readable_range(s->data,s->num?(size_t)s->num*2:2)&&(!s->num||!s->data[s->num-1])));
}
static void floor_native_string_free(RawFString*s){
    if(s->data&&floor_native_string_valid(s))((void(WINAPI*)(void*))(g_progress_event_exe+0xdfb60))(s->data);
    s->data=0;s->num=s->max=0;
}
static unsigned floor_native_number(const RawFString*s){
    if(!floor_native_string_valid(s)||s->num!=11)return 0;
    const wchar_t*p=s->data;unsigned base;
    if(p[0]=='M'&&p[1]=='E'&&p[2]=='T')base=0;
    else if(p[0]=='A'&&p[1]=='R'&&p[2]=='C')base=10;
    else if(p[0]=='A'&&p[1]=='M'&&p[2]=='S')base=20;
    else if(p[0]=='R'&&p[1]=='F'&&p[2]=='T')base=30;else return 0;
    if(p[3]!='_'||p[4]!='F'||p[5]!='L'||p[6]!='R'||p[7]!='_'||p[8]<'0'||p[8]>'1'||p[9]<'0'||p[9]>'9')return 0;
    unsigned n=(p[8]-'0')*10+p[9]-'0';return n>=1&&n<=10?base+n:0;
}
/* Boss exits do not require Progressive Floor. Prove only the current floor
   before querying escalator metadata/destination, which differs in boss rooms.
   Returning native never forces access over combat/story restrictions. */
static int floor_native_boss_exit(void*stage){
    unsigned offset;
    if(!stage||!readable_range(stage,0x58))return 0;
    if(g_floor_meta.ready&&g_floor_meta.stage_class==*(void**)((BYTE*)stage+0x50))offset=g_floor_meta.current_floor;
    else if(!lid_floor_field(stage,"mCurrentFloorId","StrProperty",16,&offset,&g_progress_event_ops))return 0;
    if(!readable_range((BYTE*)stage+offset,16))return 0;
    unsigned from=floor_native_number((RawFString*)((BYTE*)stage+offset));
    return lid_floor_exempt(from,from+1);
}
static int floor_native_outer_name(void*object,wchar_t*out,unsigned capacity){
    BYTE*p=object;BYTE*last=0;unsigned depth=0;
    while(p){if(++depth>32||!readable_range(p,0x50))return 0;last=p;p=*(BYTE**)(p+0x40);}
    if(!last||capacity<2||!readable_range(g_progress_event_exe+0x295fd18,16))return 0;
    BYTE**names=*(BYTE***)(g_progress_event_exe+0x295fd18);int count=*(int*)(g_progress_event_exe+0x295fd20);
    int id=*(int*)(last+0x48);unsigned number=*(unsigned*)(last+0x4c);
    if(id<0||count<1||count>2000000||id>=count||!names||!readable_range(names+id,8))return 0;
    BYTE*entry=names[id];if(!entry||!readable_range(entry+8,1))return 0;unsigned width=(entry[8]&1)?2:1;
    if(!readable_range(entry+0x14,(size_t)capacity*width))return 0;
    unsigned i;for(i=0;i+1<capacity;i++){out[i]=width==2?((wchar_t*)(entry+0x14))[i]:entry[0x14+i];if(!out[i])break;}
    if(i+1>=capacity)return 0;
    if(number){if(number>1000000||i+9>=capacity)return 0;out[i++]='_';out[i]=0;append_dec_w(out,capacity,number-1);}
    return out[0]!=0;
}
static int floor_native_event(void*object,void*function,void*params){
    if(!lid_event_class_is(function,"Function",&g_progress_event_ops)||!readable_range(function,0xf8)||!readable_range(object,8))return 0;
    unsigned flags=*(unsigned*)((BYTE*)function+0xd0);
    if(function==g_floor_meta.unit.function||function==g_floor_meta.connection.function){
        unsigned rva=function==g_floor_meta.unit.function?0x100e750u:0x100b280u;
        if(!(flags&0x400)||*(unsigned short*)((BYTE*)function+0xd4)||*(void**)((BYTE*)function+0xf0)!=g_progress_event_exe+rva)return 0;
    }else if(function!=g_floor_meta.close.function||(flags&0x400)||!(flags&2))return 0;
    void**vt=*(void***)object;
    if(!vt||!readable_range(vt,0x220)||!executable_ptr(g_progress_event_exe,vt[0x218/8]))return 0;
    ((LidProcessEvent)vt[0x218/8])(object,function,params,0);return 1;
}
static int floor_native_destination(void*stage,void*volume,const LidFloorMeta*m,unsigned*from,unsigned*to){
    BYTE unit_args[256]={0},exit_args[256]={0};wchar_t outer[256];RawFString input;int result=0;
    if(!from||!to||!readable_range((BYTE*)stage+m->current_floor,16))return 0;
    RawFString*current=(RawFString*)((BYTE*)stage+m->current_floor);
    if(!floor_native_string_valid(current))return 0;
    *from=floor_native_number(current);if(!*from){*to=0;return 2;} /* Hub/other stages are outside this feature. */
    if(!floor_native_outer_name(volume,outer,256))return 0;
    input.data=outer;input.num=(int)lid_wcslen(outer)+1;input.max=input.num;
    lid_wire_copy(unit_args+m->unit.offset[0],&input,16);
    if(!floor_native_event(stage,m->unit.function,unit_args))return 0;
    RawFString*unit=(RawFString*)(unit_args+m->unit.offset[1]);
    if(*(unsigned*)(unit_args+m->unit.offset[2])!=1||!floor_native_string_valid(unit)||unit->num<2)goto done;
    lid_wire_copy(exit_args+m->connection.offset[0],unit,16);
    if(!floor_native_event(stage,m->connection.function,exit_args))goto done;
    if(*(unsigned*)(exit_args+m->connection.offset[3])==1){
        *to=floor_native_number((RawFString*)(exit_args+m->connection.offset[1]+m->to_floor));
        /* Destinations outside 1..40 must remain native. */
        result=1;
    }
done:
    for(unsigned i=0;i<7;i++)floor_native_string_free((RawFString*)(exit_args+m->connection.offset[1]+m->strings[i]));
    floor_native_string_free(unit);return result;
}
static void floor_native_deny(void*ui){
    void**vt;if(!ui||!readable_range(ui,8)||(vt=*(void***)ui)==0||!readable_range(vt,0x290)||!executable_ptr(g_progress_event_exe,vt[0x288/8]))return;
    ((void(WINAPI*)(void*,BYTE,int))vt[0x288/8])(ui,16,0);
}
static void WINAPI floor_access_exec(void*self,void*frame,void*result){
    if(g_floor_access_original)g_floor_access_original(self,frame,result);
    DWORD error=g_win.GetLastError();
    LidFloorTarget effective;const LidFloorTarget*policy=0;int proved=0;
    DWORD owner=__atomic_load_n(&g_floor_owner,__ATOMIC_ACQUIRE);
    if(!owner||!g_ap_mode_active||!g_progress_runtime_mutex.ops.thread_id||g_progress_runtime_mutex.ops.thread_id()!=owner||
       g_floor_busy||!readable_range(frame,0x24)||!g_progress_host.peer.bound||!g_progress_host.peer.granted)goto done;
    if(g_floor_pair.have_current&&lid_floor_session(&g_floor_pair.current,&g_progress_host.peer.binding)){policy=&g_floor_pair.current;proved=1;}
    else if(g_floor_pair.have_pending&&lid_floor_session(&g_floor_pair.pending,&g_progress_host.peer.binding)){
        effective=g_floor_pair.pending;effective.applied=0;policy=&effective; /* Restrict before ACK, never grant pending receipts. */
    }
    if(!policy||!policy->enabled)goto done;
    BYTE*node=*(BYTE**)((BYTE*)frame+0x14);void*volume=*(void**)((BYTE*)frame+0x1c);
    if((!g_floor_meta.ready||node!=g_floor_meta.touch)&&!progress_event_name_is(node,"Touch"))goto done;
    if(!lid_floor_is_a(volume,"BrgVolume_Escalator",&g_progress_event_ops))goto done;
    BYTE*game=progress_native_game(g_progress_event_exe);BYTE*stage;
    if(!game||(stage=*(BYTE**)(game+0x13e8))==0)goto done;
    unsigned ui_offset;
    if(g_floor_meta.ready&&g_floor_meta.game_class==*(void**)(game+0x50))ui_offset=g_floor_meta.ui;
    else if(!lid_floor_field(game,"mUIManager","ObjectProperty",8,&ui_offset,&g_progress_event_ops))goto done;
    if(!readable_range(game+ui_offset,8)||*(void**)(game+ui_offset)!=self)goto done;
    if(floor_native_boss_exit(stage)){
        if(!(g_floor_warning&4)){g_floor_warning|=4;log_ascii("AP FLOOR: major-boss floor exit retained native; no AP destination query or access denial.");}
        goto done;
    }
    if(!g_floor_meta.ready||g_floor_meta.game_class!=*(void**)(game+0x50)||
       !readable_range(stage,0x58)||g_floor_meta.stage_class!=*(void**)(stage+0x50)||g_floor_meta.volume_class!=*(void**)((BYTE*)volume+0x50)){
        if(!lid_floor_prepare_meta(&g_floor_meta,game,stage,volume,&g_progress_event_ops)){
            floor_native_deny(self);if(!(g_floor_warning&1)){g_floor_warning|=1;log_ascii("AP FLOOR: escalator metadata pending/refused; upward UI access withheld, no transition or save started.");}goto done;
        }
    }
    if(!readable_range(game+g_floor_meta.ui,8)||*(void**)(game+g_floor_meta.ui)!=self||
       !readable_range(game+g_floor_meta.touch_volume,8)||*(void**)(game+g_floor_meta.touch_volume)!=volume||
       !readable_range((BYTE*)volume+g_floor_meta.target,8))goto done;
    BYTE*point=*(BYTE**)((BYTE*)volume+g_floor_meta.target);
    if(!point||!lid_floor_is_a(point,"BrgRandomGenerateEscalatorTargetPoint",&g_progress_event_ops)||!readable_range(point+g_floor_meta.type,1)||point[g_floor_meta.type]!=0)goto done;
    unsigned from=0,to=0;g_floor_busy=1;
    if(!floor_native_destination(stage,volume,&g_floor_meta,&from,&to)){
        floor_native_deny(self);if(!(g_floor_warning&2)){g_floor_warning|=2;log_ascii("AP FLOOR: destination query refused; upward UI access withheld without changing native travel state.");}
    }else if(lid_floor_blocked(policy,from,to)){
        BYTE parameters[256]={0};parameters[g_floor_meta.close.offset[0]]=2;
        floor_native_deny(self);if(proved)floor_native_event(volume,g_floor_meta.close.function,parameters);
        floor_native_note_block(from,to,policy->applied);
    }
    g_floor_busy=0;
done:g_floor_error_set(error);
}
static void floor_native_entry_done(int status){
    if(status!=LID_TX_SAVED&&status!=LID_TX_ENTRY_DONE&&status!=LID_TX_REFRESHED)return;
    if(g_hub_events.outside)return; /* A save may finish after departure; Floor grants wait for the next actual hub entry. */
    if(lid_floor_pair_commit(&g_floor_pair,&g_progress_host.frozen,g_progress_host.acknowledged)&&g_floor_pair.current.enabled){
        char text[200];size_t n=0;text[0]=0;
        append_ascii(text,sizeof(text),&n,"AP FLOOR: acknowledged Waiting Room batch applied=");append_dec(text,sizeof(text),&n,g_floor_pair.current.applied);
        append_ascii(text,sizeof(text),&n," received=");append_dec(text,sizeof(text),&n,g_floor_pair.current.received);
        append_ascii(text,sizeof(text),&n," goal=");append_dec(text,sizeof(text),&n,g_floor_pair.current.goal);text[n]=0;log_ascii(text);
    }
}
#endif
