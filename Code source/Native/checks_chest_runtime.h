#ifndef LID_CHECKS_CHEST_RUNTIME_R74_H
#define LID_CHECKS_CHEST_RUNTIME_R74_H
#if LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
#include "checks_chest_observer.h"
static R74Observer g_checks_chest;
static unsigned g_checks_chest_attempted;
static unsigned g_checks_chest_flushed,g_checks_chest_bindings_logged;
static uint32_t checks_chest_thread(void*unused){(void)unused;return g_get_tid?g_get_tid():0;}
static int checks_chest_stamp(void*unused,R74Stamp*out){
    (void)unused;R74Stamp s={0};char map[3][64]={{0}};unsigned owner=g_checks_diag.owner;
    if(!out||!owner||!g_get_tid||g_get_tid()!=owner||!checks_session()||
       !__atomic_load_n(&g_checks_diag.armed,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&g_checks_delivery.state,__ATOMIC_ACQUIRE)!=R26_ACTIVE||
       __atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)||
       !checks_delivery_rotation_ready(owner)||!r25_host_context(&g_progress_host,&s.context)||
       !r15_same_context(&s.context,&g_checks_delivery.queue.context))return 0;
    s.epoch=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE);s.revision=r35_revision(&g_checks_rotation_proof);
    checks_map(map);
    for(unsigned i=0;i<16;i++){s.floor[i]=map[1][i];if(!s.floor[i])break;}
    for(unsigned i=0;i<32;i++){s.area[i]=map[2][i];if(!s.area[i])break;}
    if(!s.epoch||!r15_ascii(s.floor,16)||!r15_ascii(s.area,32)||!r22_map_floor(s.floor,s.area))return 0;
    int known=0;
    for(unsigned i=0;i<101;i++){
        size_t nf=lid_strlen(r33_nodes[i].floor),na=lid_strlen(r33_nodes[i].area);
        if(nf<16&&na<32&&!lid_memcmp(s.floor,r33_nodes[i].floor,nf+1)&&!lid_memcmp(s.area,r33_nodes[i].area,na+1)){known=1;break;}
    }
    if(!known||s.epoch!=__atomic_load_n(&g_hub_entry_signal.serial,__ATOMIC_ACQUIRE)||
       s.revision!=r35_revision(&g_checks_rotation_proof)||!checks_delivery_rotation_ready(owner))return 0;
    *out=s;return 1;
}
static int checks_chest_scope_begin(void*unused){
    (void)unused;
#if LID_REFLECTION_READ_SCOPE_ENABLED
    return lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
#else
    return 0;
#endif
}
static void checks_chest_scope_end(void*unused,int acquired){
    (void)unused;
#if LID_REFLECTION_READ_SCOPE_ENABLED
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
#else
    (void)acquired;
#endif
}
static void WINAPI checks_chest_dispatch(void*object,void*function,void*params,void*result){
    R74Ops ops={g_checks_ops,0,checks_chest_thread,checks_chest_stamp,checks_chest_scope_begin,checks_chest_scope_end,
                &g_checks_delivery.queue,(uint64_t)(ULONG_PTR)(g_checks_diag.exe+0x20971b0)};
    r74_observe(&g_checks_chest,&ops,object,function,params,result);
}
static int checks_chest_protect(void*unused,void*slot,size_t size,uint32_t mode,uint32_t*old){
    (void)unused;DWORD prior=0;
    if(!g_win.VirtualProtect||!old||!g_win.VirtualProtect(slot,size,(DWORD)mode,&prior))return 0;
    *old=(uint32_t)prior;return 1;
}
/* Called only at existing check-observer boundaries, never in an open callback. */
static void checks_chest_boundary(DWORD owner){
    if(!owner||!g_get_tid||g_get_tid()!=owner||owner!=g_checks_diag.owner)return;
    if(g_checks_chest_attempted){
        if(g_checks_chest.seen!=g_checks_chest_flushed&&!g_checks_chest.busy){
            char line[700];size_t pos=0;line[0]=0;
            append_ascii(line,700,&pos,"AP CHESTS R74 SUMMARY");
            checks_log_number(line,&pos," observed=",g_checks_chest.seen);
            checks_log_number(line,&pos," queued=",g_checks_chest.queued);
            checks_log_number(line,&pos," bindings=",g_checks_chest.count);
            checks_log_number(line,&pos," binding_refused=",g_checks_chest.binding_refused);
            checks_log_number(line,&pos," context_refused=",g_checks_chest.context_refused);
            checks_log_number(line,&pos," queue_refused=",g_checks_chest.refused);log_ascii(line);
            g_checks_chest_flushed=g_checks_chest.seen;
            while(g_checks_chest_bindings_logged<g_checks_chest.count){
                R73ChestLayout*l=&g_checks_chest.cache[g_checks_chest_bindings_logged++];pos=0;line[0]=0;
                append_ascii(line,700,&pos,"AP CHESTS R74 BINDING");
                checks_log_number(line,&pos," open_offset=",l->open.offset);
                checks_log_number(line,&pos," open_mask=",l->open.mask);
                checks_log_number(line,&pos," color_offset=",l->color.offset);
                checks_log_number(line,&pos," kind_offset=",l->kind.offset);log_ascii(line);
            }
        }
        return;
    }
    if(
       !__atomic_load_n(&g_checks_diag.armed,__ATOMIC_ACQUIRE)||!checks_session()||!g_checks_diag.exe)return;
    g_checks_chest_attempted=1;g_checks_chest.owner=owner;
    BYTE*exe=g_checks_diag.exe;R74Dispatch expected=(R74Dispatch)(exe+0x3bf4c0);
    R74Dispatch*slot=(R74Dispatch*)(exe+0x20973c8);
    if(!executable_ptr(exe,(void*)expected)||
       !r74_patch(&g_checks_chest,&g_checks_ops,checks_chest_protect,0,slot,expected,(R74Dispatch)checks_chest_dispatch)){
        log_ascii("AP CHESTS R74: Box observer installation refused; original dispatch preserved.");return;
    }
    log_ascii("AP CHESTS R74: Box opening observer armed; capture requires active verified tower context.");
}
#endif
#endif
