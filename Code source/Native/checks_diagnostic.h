#ifndef LID_CHECKS_DIAGNOSTIC_H
#define LID_CHECKS_DIAGNOSTIC_H
#ifndef LID_CHECKS_DIAGNOSTIC_ENABLED
#define LID_CHECKS_DIAGNOSTIC_ENABLED 0
#endif
#if LID_CHECKS_DIAGNOSTIC_ENABLED
#include "checks_call_context.h"
#include "checks_level_label.h"
/* Diagnostic ONLY. No AP checks, mutation of actor state, or combat-time I/O.
   Evidence/version: Research/Reports/CHECKS_R7_CALL_CONTEXT_RESULTS.md.
   The verified virtual slot is exchanged atomically; game code is not patched. */
typedef void (WINAPI *ChecksHealthFn)(void*,int);
typedef struct ChecksClass {
    uint64_t object_class,meta;uint32_t name,number;int valid;
    R6Layout layout;char text[96];unsigned tuber_family,tuber_binding;R11Layout tuber_layout;
#if LID_CHECKS_DELIVERY_ENABLED
    uint32_t delivery_mask;
#endif
} ChecksClass;
typedef struct ChecksRecord {
    R6Sample before,after;unsigned cls,label,reason;int argument;
    char map[3][64],damage[96];unsigned tuber_status;R11Sample tuber;
} ChecksRecord;
static struct {
    int attempted,armed,busy,installed,protection_restored;DWORD owner;BYTE*exe;uint64_t node;
    ChecksHealthFn original;unsigned classes,count,flushed,classes_logged;
    unsigned foreign,reentrant,other_origin,overflow,failed_context,failed_sample;
#if LID_CHECKS_DELIVERY_ENABLED
    BYTE config[32],identity[32];ChecksClass cls[64];ChecksRecord records[128];
#else
    BYTE config[32],identity[32];ChecksClass cls[16];ChecksRecord records[128];
#endif
} g_checks_diag;
static int checks_copy(void*unused,uint64_t address,void*out,size_t size){
    (void)unused;
    if(!out||!address||!size||address>UINT64_MAX-size||!readable_range((void*)(ULONG_PTR)address,size))return 0;
    lid_memcpy(out,(void*)(ULONG_PTR)address,size);return 1;
}
static int checks_name(void*unused,uint64_t object,const char*name){
    (void)unused;return progress_event_name_is((void*)(ULONG_PTR)object,name);
}
static const R6Ops g_checks_ops={0,checks_copy,checks_name};
static void checks_object_name(uint64_t object,char out[96]){
    uint64_t names,entry;int32_t index,count;unsigned char wide;uint32_t number;
    if(!r6_at(&g_checks_ops,object,0x48,&index,4)||index<0||
       !r6_at(&g_checks_ops,object,0x4c,&number,4)||number||
       !r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)g_checks_diag.exe,0x295ad18,&names,8)||
       !r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)g_checks_diag.exe,0x295ad20,&count,4)||
       count<=0||count>2000000||index>=count||!names||
       !r6_at(&g_checks_ops,names,(uint32_t)index*8,&entry,8)||!r6_at(&g_checks_ops,entry,8,&wide,1))return;
    for(unsigned i=0;i<96;i++){
        uint16_t ch=0;
        if(!r6_at(&g_checks_ops,entry,0x14+i*((wide&1)?2:1),&ch,(wide&1)?2:1)||ch>126||(ch&&ch<32)){out[0]=0;return;}
        out[i]=(char)ch;if(!ch)return;
    }
    out[0]=0;
}
static int checks_session(void){
    return g_ap_mode_active&&g_savedata_route_active&&g_v074_cfg.valid&&
      g_progress_host.initialized&&!g_progress_host.poisoned&&g_progress_host.acknowledged&&g_progress_host.peer.bound&&g_progress_host.peer.granted&&
      lid_memcmp(g_checks_diag.config,g_progress_host.expected_config,32)==0&&
      lid_memcmp(g_checks_diag.identity,g_progress_host.peer.binding.identity,32)==0;
}
static int checks_bind(uint64_t pawn,R6Layout*layout,uint64_t*node){
    int result,acquired=0;
#if LID_REFLECTION_READ_SCOPE_ENABLED
    acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    result=r6_bind(&g_checks_ops,pawn,(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0x21b9730),layout)&&
      r7_bind_death_function(&g_checks_ops,pawn,(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0x21b9730),node);
#if LID_REFLECTION_READ_SCOPE_ENABLED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    return result;
}
static void checks_tuber_binding(ChecksClass*c,uint64_t pawn){
    static const char*family[6]={"BrgPawn_Scratch","BrgPawn_Hovering","BrgPawn_Turret","BrgPawn_Bone","BrgPawn_Reversal","BrgPawn_Treasure"};
    uint64_t cls=c->object_class,next,seen[32];unsigned count=0;int acquired=0;
    c->tuber_binding=1; /* non-Tuber/unknown; never prevents the death record */
#if LID_REFLECTION_READ_SCOPE_ENABLED
    acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    while(cls){
        if(count==32){c->tuber_binding=3;break;}
        for(unsigned i=0;i<count;i++)if(seen[i]==cls){c->tuber_binding=3;goto done;}
        seen[count++]=cls;
        for(unsigned i=0;i<6;i++)if(r6_name(&g_checks_ops,cls,family[i])){c->tuber_family=i+1;break;}
        if(c->tuber_family){
            c->tuber_binding=r11_bind_tuber(&g_checks_ops,pawn,c->layout.vtable,&c->tuber_layout)?2:3;break;
        }
        if(!r6_at(&g_checks_ops,cls,0x78,&next,8)){c->tuber_binding=3;break;}cls=next;
    }
done:
#if LID_REFLECTION_READ_SCOPE_ENABLED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    (void)acquired;
}
static int checks_class(uint64_t pawn){
    uint64_t cls,meta,node=0;uint32_t name,number;ChecksClass*c;
    if(!r6_at(&g_checks_ops,pawn,0x50,&cls,8)||!cls||
       !r6_at(&g_checks_ops,cls,0x50,&meta,8)||
       !r6_at(&g_checks_ops,cls,0x48,&name,4)||!r6_at(&g_checks_ops,cls,0x4c,&number,4))return -1;
    for(unsigned i=0;i<g_checks_diag.classes;i++){
        c=&g_checks_diag.cls[i];
        if(c->object_class==cls&&c->meta==meta&&c->name==name&&c->number==number)return c->valid?(int)i:-1;
    }
#if LID_CHECKS_DELIVERY_ENABLED
    if(g_checks_diag.classes==64){
        if(g_checks_delivery.queue.initialized)r15_queue_fault(&g_checks_delivery.queue,g_checks_diag.owner,4);return -1;
    }
#else
    if(g_checks_diag.classes==16)return -1;
#endif
    unsigned i=g_checks_diag.classes++;c=&g_checks_diag.cls[i];
    c->object_class=cls;c->meta=meta;c->name=name;c->number=number;
    checks_object_name(cls,c->text);
    c->valid=checks_bind(pawn,&c->layout,&node)&&node==g_checks_diag.node;
    if(c->valid)checks_tuber_binding(c,pawn);
#if LID_CHECKS_DELIVERY_ENABLED
    if(c->valid){
        int scoped=0;
#if LID_REFLECTION_READ_SCOPE_ENABLED
        scoped=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
        c->valid=r25_class_mask(&g_checks_ops,c->object_class,&c->delivery_mask);
#if LID_REFLECTION_READ_SCOPE_ENABLED
        lid_read_scope_end(&g_reflection_read_scope,g_get_tid,scoped);
#endif
        (void)scoped;
    }
#endif
    return c->valid?(int)i:-1;
}
static void checks_map(char out[3][64]){
    static const unsigned offset[3]={0x5c0,0x5d0,0x5e0};BYTE*game;uint64_t stage,vt;
    if(!(game=progress_native_game(g_checks_diag.exe))||
       !r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)game,0x13e8,&stage,8)||
       !r6_at(&g_checks_ops,stage,0,&vt,8)||vt!=(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0x21c3350))return;
    for(unsigned i=0;i<3;i++){
        RawFString str,again;wchar_t text[64];
        if(!r6_at(&g_checks_ops,stage,offset[i],&str,sizeof(str))||!str.data||str.num<1||str.num>64||str.max<str.num||
           !checks_copy(0,(uint64_t)(ULONG_PTR)str.data,text,(size_t)str.num*2)||text[str.num-1]||
           !r6_at(&g_checks_ops,stage,offset[i],&again,sizeof(again))||lid_memcmp(&str,&again,sizeof(str)))continue;
        unsigned j;for(j=0;j<(unsigned)str.num-1;j++)if(text[j]<32||text[j]>126)break;
        if(j!=(unsigned)str.num-1)continue;
        for(j=0;j<(unsigned)str.num;j++)out[i][j]=(char)text[j];
    }
}
#if LID_CHECKS_DELIVERY_ENABLED
static void checks_visit_area_observed(void* stage){
    R15Context context;char map[3][64]={{0}};char floor[16]={0},area[32]={0};
    unsigned owner=g_checks_diag.owner,serial;uint64_t revision;BYTE*game;
    if(!g_get_tid||!owner||g_get_tid()!=owner||!g_ap_mode_active||!g_savedata_route_active||
       !__atomic_load_n(&g_checks_diag.armed,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||
       !checks_delivery_rotation_ready(owner)||!r25_host_context(&g_progress_host,&context)||
       !(game=progress_native_game(g_checks_diag.exe))||!readable_range(game+0x13e8,8)||
       *(void**)(game+0x13e8)!=stage)return;
    serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);revision=r35_revision(&g_checks_rotation_proof);
    checks_map(map);
    for(unsigned i=0;i<sizeof(floor);i++){floor[i]=map[1][i];if(!floor[i])break;}
    for(unsigned i=0;i<sizeof(area);i++){area[i]=map[2][i];if(!area[i])break;}
    int known=0;
    for(unsigned i=0;i<101;i++){
        size_t nf=lid_strlen(r33_nodes[i].floor),na=lid_strlen(r33_nodes[i].area);
        if(nf<sizeof(floor)&&na<sizeof(area)&&!lid_memcmp(floor,r33_nodes[i].floor,nf+1)&&!lid_memcmp(area,r33_nodes[i].area,na+1)){known=1;break;}
    }
    if(!r15_ascii(floor,16)||!r15_ascii(area,32)||!r22_map_floor(floor,area)||
       !known||
       serial!=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE)||
       revision!=r35_revision(&g_checks_rotation_proof)||!checks_delivery_rotation_ready(owner))return;
    (void)r39_visit_capture(&g_checks_visits,&g_checks_delivery.queue,owner,&context,serial,floor,area,1);
}
static void checks_delivery_record(const ChecksRecord*r,const ChecksClass*c,const R7CallContext*call,uint64_t revision,unsigned serial){
    if(!r||!c||r->reason||!r->label||!checks_delivery_rotation_ready(g_checks_diag.owner)||
       __atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)!=R26_ACTIVE)return;
    if(c->tuber_family&&r->tuber_status!=1){r15_queue_fault(&g_checks_delivery.queue,g_checks_diag.owner,4);return;}
    R25Observation observation={0};observation.call=*call;observation.before=r->before;observation.after=r->after;observation.argument=r->argument;
    observation.epoch=serial;
    observation.classes=c->delivery_mask;observation.rotation_verified=1;
    lid_memcpy(observation.rotation,"4HMA",5);
    for(unsigned i=0;i<sizeof(observation.floor);i++){observation.floor[i]=r->map[1][i];if(!r->map[1][i])break;}
    for(unsigned i=0;i<sizeof(observation.area);i++){observation.area[i]=r->map[2][i];if(!r->map[2][i])break;}
    lid_memcpy(observation.damage,r->damage,sizeof(observation.damage));lid_memcpy(observation.pawn_class,c->text,sizeof(observation.pawn_class));
    if(c->tuber_family){lid_memcpy(observation.name,r->tuber.name.text,sizeof(observation.name));lid_memcpy(observation.label,r->tuber.label.text,sizeof(observation.label));}
    if(revision!=r35_revision(&g_checks_rotation_proof)||serial!=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE)||
       !checks_delivery_rotation_ready(g_checks_diag.owner))return;
    (void)r26_capture_observed(&g_checks_delivery,&g_progress_host,g_checks_diag.owner,&observation,g_checks_diag.node,(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0xfedd22));
}
#endif
/* No instrumentation, allocation or logging may be added to this leaf thunk.
   RBX is a FFrame ONLY for the exact execUpdateHealth return address. */
__attribute__((noinline,used)) static void WINAPI checks_diag_health_observed(void*pawn,int value,uint64_t frame,uint64_t origin){
    ChecksHealthFn original=__atomic_load_n(&g_checks_diag.original,__ATOMIC_ACQUIRE);
    R7CallContext ctx;R6Sample before,after;ChecksRecord*record;int cls,ok;
#if LID_CHECKS_DELIVERY_ENABLED
    ChecksRecord local_record;
#endif
    /* original is published before exchanging the slot, and never freed. */
    if(!original)return;
    if(value!=1||!__atomic_load_n(&g_checks_diag.armed,__ATOMIC_ACQUIRE)){original(pawn,value);return;}
    if(!g_progress_runtime_mutex.ops.thread_id||g_progress_runtime_mutex.ops.thread_id()!=g_checks_diag.owner){
        __atomic_fetch_add(&g_checks_diag.foreign,1,__ATOMIC_RELAXED);original(pawn,value);return;
    }
    if(g_checks_diag.busy){g_checks_diag.reentrant++;original(pawn,value);return;}
    if(origin!=(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0xfedd22)){g_checks_diag.other_origin++;original(pawn,value);return;}
    if(!checks_session()){original(pawn,value);return;}
#if LID_CHECKS_DELIVERY_ENABLED
    uint64_t proof_revision=r35_revision(&g_checks_rotation_proof);
    unsigned proof_serial=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);
#endif
    g_checks_diag.busy=1;
#if !LID_CHECKS_DELIVERY_ENABLED
    if(g_checks_diag.count==128){g_checks_diag.overflow++;goto forward;}
#endif
    if(!r7_call_context(&g_checks_ops,frame,(uint64_t)(ULONG_PTR)pawn,origin,
       (uint64_t)(ULONG_PTR)(g_checks_diag.exe+0xfedd22),g_checks_diag.node,&ctx)){
        g_checks_diag.failed_context++;goto forward;
    }
    cls=checks_class((uint64_t)(ULONG_PTR)pawn);
    if(cls<0||!r6_sample(&g_checks_ops,(uint64_t)(ULONG_PTR)pawn,&g_checks_diag.cls[cls].layout,&before)){
        g_checks_diag.failed_sample++;goto forward;
    }
#if LID_CHECKS_DELIVERY_ENABLED
    if(g_checks_diag.count<128)record=&g_checks_diag.records[g_checks_diag.count++];
    else{g_checks_diag.overflow++;lid_memset(&local_record,0,sizeof(local_record));record=&local_record;}
#else
    record=&g_checks_diag.records[g_checks_diag.count++];
#endif
    record->before=before;
    record->cls=(unsigned)cls;record->argument=value;checks_map(record->map);
    checks_object_name(before.damage_type,record->damage);
    if(g_checks_diag.cls[cls].tuber_family){
        ChecksClass*c=&g_checks_diag.cls[cls];record->tuber_status=2;
        if(c->tuber_binding==2)record->tuber_status=r11_sample_tuber(&g_checks_ops,(uint64_t)(ULONG_PTR)pawn,&c->tuber_layout,&record->tuber)?1:3;
    }
    original(pawn,value);
    ok=r6_sample(&g_checks_ops,(uint64_t)(ULONG_PTR)pawn,&g_checks_diag.cls[cls].layout,&after);
    record->reason=ok?0:1;if(ok){record->after=after;record->label=r7_accepted_setdeath_label(&ctx,value,&before,&after);}
#if LID_CHECKS_DELIVERY_ENABLED
    checks_delivery_record(record,&g_checks_diag.cls[cls],&ctx,proof_revision,proof_serial);
#endif
    g_checks_diag.busy=0;return;
forward:
    original(pawn,value);g_checks_diag.busy=0;
}
__attribute__((naked,noinline,used)) static void WINAPI checks_diag_health_thunk(void*pawn,int value){
    __asm__ volatile("movq %rbx, %r8\n\tmovq (%rsp), %r9\n\tjmp checks_diag_health_observed");
}
static int checks_install(void){
    static const BYTE expected[16]={0x44,0x8b,0x81,0xd0,3,0,0,0x89,0x91,0xcc,3,0,0,0x41,0x3b,0xd0};
    BYTE*target=g_checks_diag.exe+0x14244f0;
    ChecksHealthFn*slot=(ChecksHealthFn*)(g_checks_diag.exe+0x21b9730+0xb98);
    ChecksHealthFn prior=(ChecksHealthFn)target;DWORD old=0,ignored=0;
    if(((ULONG_PTR)slot&7)||!readable_range(target,16)||lid_memcmp(target,expected,16)||
       !readable_range(slot,8)||__atomic_load_n(slot,__ATOMIC_ACQUIRE)!=prior||!g_win.VirtualProtect)return 0;
    __atomic_store_n(&g_checks_diag.original,prior,__ATOMIC_RELEASE);
    if(!g_win.VirtualProtect(slot,8,0x04 /* PAGE_READWRITE */,&old))return 0;
    g_checks_diag.installed=__atomic_compare_exchange_n(slot,&prior,checks_diag_health_thunk,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    g_checks_diag.protection_restored=g_win.VirtualProtect(slot,8,old,&ignored)!=0;
    /* A failed restoration leaves a valid slot/original, but disables observing.
       Never restore a pointer that another module might have replaced. */
    return g_checks_diag.installed&&g_checks_diag.protection_restored;
}
static void checks_log_number(char*line,size_t*pos,const char*key,unsigned value){
    append_ascii(line,700,pos,key);append_dec(line,700,pos,value);
}
static void checks_log_utf16(unsigned seq,const char*field,const R11Label*label){
    static const char hex[]="0123456789ABCDEF";char line[700];size_t pos=0;
    append_ascii(line,700,&pos,"AP CHECKS R12 TEXT");checks_log_number(line,&pos," seq=",seq);
    append_ascii(line,700,&pos," field=");append_ascii(line,700,&pos,field);
    checks_log_number(line,&pos," mk=",label->mk);checks_log_number(line,&pos," units=",label->length);
    append_ascii(line,700,&pos," utf16hex=");
    for(unsigned i=0;i<label->length&&i<R11_LABEL_CAP-1;i++){
        uint16_t c=label->text[i];for(int shift=12;shift>=0;shift-=4)line[pos++]=hex[(c>>shift)&15];
    }
    line[pos]=0;log_ascii(line);
}
static void checks_flush(void){
    char line[700];size_t pos;
    while(g_checks_diag.classes_logged<g_checks_diag.classes){
        unsigned i=g_checks_diag.classes_logged++;ChecksClass*c=&g_checks_diag.cls[i];pos=0;
        append_ascii(line,700,&pos,"AP CHECKS R12 CLASS");checks_log_number(line,&pos," index=",i);
        checks_log_number(line,&pos," name=",c->name);checks_log_number(line,&pos," number=",c->number);
        checks_log_number(line,&pos," valid=",c->valid);append_ascii(line,700,&pos," class_name=");append_ascii(line,700,&pos,c->text);log_ascii(line);
        if(c->tuber_family||c->tuber_binding==3){
            pos=0;append_ascii(line,700,&pos,"AP CHECKS R12 TUBER_BIND");checks_log_number(line,&pos," class=",i);
            checks_log_number(line,&pos," family=",c->tuber_family);checks_log_number(line,&pos," state=",c->tuber_binding);
            if(c->tuber_binding==2){checks_log_number(line,&pos," name_offset=",c->tuber_layout.name.offset);checks_log_number(line,&pos," level_offset=",c->tuber_layout.label.offset);}
            log_ascii(line);
        }
        if(c->valid)for(unsigned f=0;f<R6_FIELDS;f++){
            pos=0;append_ascii(line,700,&pos,"AP CHECKS R12 FIELD");checks_log_number(line,&pos," class=",i);
            checks_log_number(line,&pos," field=",f);checks_log_number(line,&pos," offset=",c->layout.field[f].offset);
            checks_log_number(line,&pos," size=",c->layout.field[f].size);checks_log_number(line,&pos," mask=",c->layout.field[f].mask);log_ascii(line);
        }
    }
    while(g_checks_diag.flushed<g_checks_diag.count){
        unsigned i=g_checks_diag.flushed++;ChecksRecord*r=&g_checks_diag.records[i];pos=0;
        append_ascii(line,700,&pos,"AP CHECKS R12 OBSERVATION");checks_log_number(line,&pos," seq=",i);
        checks_log_number(line,&pos," class=",r->cls);checks_log_number(line,&pos," label=",r->label);
        checks_log_number(line,&pos," reason=",r->reason);checks_log_number(line,&pos," kind=",r->before.kind);
        checks_log_number(line,&pos," name=",r->before.name_index);checks_log_number(line,&pos," number=",r->before.name_number);
        checks_log_number(line,&pos," dead_before=",r->before.dead);checks_log_number(line,&pos," dead_after=",r->after.dead);
        checks_log_number(line,&pos," remove_before=",r->before.removing);checks_log_number(line,&pos," remove_after=",r->after.removing);
        checks_log_number(line,&pos," argument=",(unsigned)r->argument);checks_log_number(line,&pos," hp_before=",(unsigned)r->before.health);
        checks_log_number(line,&pos," hp_after=",(unsigned)r->after.health);checks_log_number(line,&pos," hp_max=",(unsigned)r->before.max_health);log_ascii(line);
        pos=0;append_ascii(line,700,&pos,"AP CHECKS R12 CONTEXT");checks_log_number(line,&pos," seq=",i);
        append_ascii(line,700,&pos," origin_rva=0xFEDD22 function=BrgPawn_Base.SetDeath node=");
        append_hex64(line,700,&pos,(ULONG_PTR)g_checks_diag.node);
        append_ascii(line,700,&pos," damage=");append_ascii(line,700,&pos,r->damage);
        append_ascii(line,700,&pos," template=");append_ascii(line,700,&pos,r->map[0]);
        append_ascii(line,700,&pos," floor=");append_ascii(line,700,&pos,r->map[1]);
        append_ascii(line,700,&pos," area=");append_ascii(line,700,&pos,r->map[2]);log_ascii(line);
        if(r->tuber_status){
            pos=0;append_ascii(line,700,&pos,"AP CHECKS R12 TUBER");checks_log_number(line,&pos," seq=",i);
            checks_log_number(line,&pos," family=",g_checks_diag.cls[r->cls].tuber_family);checks_log_number(line,&pos," status=",r->tuber_status);log_ascii(line);
            if(r->tuber_status==1){checks_log_utf16(i,"mName",&r->tuber.name);checks_log_utf16(i,"mLevelString",&r->tuber.label);}
        }
    }
    pos=0;append_ascii(line,700,&pos,"AP CHECKS R12 SUMMARY");checks_log_number(line,&pos," records=",g_checks_diag.count);
    checks_log_number(line,&pos," foreign=",__atomic_load_n(&g_checks_diag.foreign,__ATOMIC_RELAXED));
    checks_log_number(line,&pos," reentrant=",g_checks_diag.reentrant);checks_log_number(line,&pos," other_origin=",g_checks_diag.other_origin);
    checks_log_number(line,&pos," overflow=",g_checks_diag.overflow);checks_log_number(line,&pos," context_refused=",g_checks_diag.failed_context);
    checks_log_number(line,&pos," sample_refused=",g_checks_diag.failed_sample);log_ascii(line);
}
/* Copied diagnostic data has no dependency on item mutation/save readiness.
   Called only for a real, active entry epoch, before the transaction tick.
   No arming here and no file I/O while an engine lease/save job is active. */
static unsigned g_checks_flushed_entry;
static void checks_diagnostic_entry_flush(unsigned serial){
    if(!serial||serial==g_checks_flushed_entry||!__atomic_load_n(&g_checks_diag.armed,__ATOMIC_ACQUIRE)||
       !g_progress_runtime_mutex.ops.thread_id||g_progress_runtime_mutex.ops.thread_id()!=g_checks_diag.owner||
       g_hub_events.outside||g_checks_diag.busy||g_progress_apply_lease.active||g_progress_save_job.active)return;
    g_checks_flushed_entry=serial;
    if(g_checks_diag.count>g_checks_diag.flushed){
        log_ascii("AP CHECKS R13: copied combat records flushed at hub entry; no dependency on item transaction completion.");
        checks_flush();
    }
}
static void checks_diagnostic_boundary(int status){
    BYTE*pawn,*common;R6Layout layout;uint64_t node,vt;DWORD tid;char line[700],name[96]={0};size_t pos=0;
    if(!g_progress_runtime_mutex.ops.thread_id)return;
    tid=g_progress_runtime_mutex.ops.thread_id();if(!tid||tid!=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE))return;
#if LID_CHECKS_ROTATION_ENABLED
    checks_rotation_observe_boundary(tid);
#endif
    if(g_checks_diag.attempted){if(tid==g_checks_diag.owner&&!g_checks_diag.busy){
        checks_flush();
#if LID_CHECKS_DELIVERY_ENABLED
        checks_delivery_boundary(tid);
        checks_chest_boundary(tid);
#endif
    }return;}
    if(status!=LID_TX_SAVED||!g_progress_native_ready||!g_ap_mode_active||!g_savedata_route_active||
       !g_v074_cfg.valid||!g_progress_host.initialized||g_progress_host.poisoned||!g_progress_host.acknowledged||
       !g_progress_host.peer.bound||!g_progress_host.peer.granted)return;
    g_checks_diag.attempted=1;g_checks_diag.owner=tid;g_checks_diag.exe=g_progress_event_exe;
    if(!g_checks_diag.exe||!progress_native_game(g_checks_diag.exe)){
        log_ascii("AP CHECKS R12: init refused stage=game_root; observer disabled.");return;
    }
    if(!rage_player_objects(g_checks_diag.exe,&pawn,&common)){
        log_ascii("AP CHECKS R12: init refused stage=player_lookup; observer disabled.");return;
    }
    uint64_t cls=0;
    if(r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)pawn,0x50,&cls,8))checks_object_name(cls,name);
    if(!r6_at(&g_checks_ops,(uint64_t)(ULONG_PTR)pawn,0,&vt,8)){
        log_ascii("AP CHECKS R12: init refused stage=pawn_vtable_read; observer disabled.");return;
    }
    append_ascii(line,700,&pos,"AP CHECKS R12 INIT class=");append_ascii(line,700,&pos,name);
    append_ascii(line,700,&pos," vtable=");append_hex64(line,700,&pos,(ULONG_PTR)vt);
    append_ascii(line,700,&pos," expected=");append_hex64(line,700,&pos,(ULONG_PTR)(g_checks_diag.exe+0x21b9730));log_ascii(line);
    if(vt!=(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0x21b9730)){
        log_ascii("AP CHECKS R12: init refused stage=pawn_vtable_match; observer disabled.");return;
    }
    int fields_ok,acquired=0;
#if LID_REFLECTION_READ_SCOPE_ENABLED
    acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#endif
    fields_ok=r6_bind(&g_checks_ops,(uint64_t)(ULONG_PTR)pawn,vt,&layout);
#if LID_REFLECTION_READ_SCOPE_ENABLED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#endif
    if(!fields_ok){
        log_ascii("AP CHECKS R12: init refused stage=field_binding; observer disabled.");return;
    }
    if(!checks_bind((uint64_t)(ULONG_PTR)pawn,&layout,&node)){
        log_ascii("AP CHECKS R12: init refused stage=function_binding; observer disabled.");return;
    }
    g_checks_diag.node=node;lid_memcpy(g_checks_diag.config,g_progress_host.expected_config,32);
    lid_memcpy(g_checks_diag.identity,g_progress_host.peer.binding.identity,32);
    (void)checks_class((uint64_t)(ULONG_PTR)pawn);
    if(!checks_install()){log_ascii("AP CHECKS R12: init refused stage=virtual_slot; observer disabled.");checks_flush();return;}
    __atomic_store_n(&g_checks_diag.armed,1,__ATOMIC_RELEASE);
#if LID_CHECKS_DELIVERY_ENABLED
    checks_delivery_boundary(tid);
    checks_chest_boundary(tid);
#endif
#if LID_CHECKS_DELIVERY_ENABLED
    log_ascii("AP CHECKS R27: observer armed; diagnostic keeps 128 records/64 classes; capture continues; delivery awaits verified rotation.");
#else
    log_ascii("AP CHECKS R12: diagnostic armed; no AP checks/rotation changes; 128 records/16 classes, flush on hub boundary.");
#endif
    checks_flush();
}
#endif
#endif
