#ifndef LID_BLUEPRINT_STARTER_RUNTIME_H
#define LID_BLUEPRINT_STARTER_RUNTIME_H
#include "blueprint_starter_policy.h"
static int g_starter_policy_state; /* 0 pending, 1 proved done, -1 marker invalid */
static unsigned g_starter_cleanup_mask,g_starter_committed;
static int starter_marker_paths(LidBridgeIo*io){
    static const wchar_t name[]=L"blueprint_starter_r22.bin";size_t n=0,i;
    if(!io||!io->initialized)return 0;
    while(n<1000&&io->directory[n]){io->path[n]=io->directory[n];n++;}
    if(!n||n==1000)return 0;
    for(i=0;name[i];i++)io->path[n++]=name[i];io->path[n]=0;
    for(i=0;i<n;i++)io->temporary[i]=io->path[i];
    for(i=0;i<40&&io->suffix[i];i++)io->temporary[n++]=io->suffix[i];
    if(!i||i==40)return 0;io->temporary[n]=0;return 1;
}
static int starter_marker_bytes(BYTE*out){
    static const BYTE magic[8]={'L','I','D','S','T','R','2','2'};unsigned i;
    lid_wire_zero(out,32);lid_wire_copy(out,magic,8);
    for(i=0;i<20;i++){
        char c=g_release_slot_key[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;
        out[8+i]=(BYTE)c;
    }
    if(g_release_slot_key[20])return 0;out[28]=22;return 1;
}
static int starter_marker_read(void){
    LidBridgeIo*io=g_blueprint_reader.bridge;BYTE expected[32],data[33];HANDLE h;DWORD n=0,e;BOOL ok,closed;
    if(g_progress_apply_lease.active||g_progress_apply_lease.window||!starter_marker_bytes(expected)||
       !starter_marker_paths(io))return -1;
    h=io->ops.open(io->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE){e=io->ops.error();return e==2||e==3?0:-1;}
    ok=io->ops.read(h,data,33,&n,0);closed=io->ops.close(h);
    return ok&&closed&&n==32&&lid_wire_equal(data,expected,32)?1:-1;
}
static int starter_marker_publish(void){
    LidBridgeIo*io=g_blueprint_reader.bridge;BYTE data[32];HANDLE h;DWORD n=0;BOOL ok,closed;
    if(g_progress_apply_lease.active||g_progress_apply_lease.window||!g_starter_committed||
       !starter_marker_bytes(data)||!starter_marker_paths(io))return 0;
    h=io->ops.open(io->temporary,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE)return 0;
    ok=io->ops.write(h,data,32,&n,0);if(ok&&n==32)ok=io->ops.flush(h);else ok=0;
    closed=io->ops.close(h);
    if(!ok||!closed||!io->ops.replace(io->temporary,io->path,9)){io->ops.remove(io->temporary);return 0;}
    return 1;
}
static int starter_native_profile(LidBlueprintDomain*d){
    static const BYTE prefix[16]={0x48,0x83,0xec,0x28,0x48,0x85,0xd2,0x75,0x07,0x32,0xc0,0x48,0x83,0xc4,0x28,0xc3};
    BYTE*exe=g_progress_event_exe;
    return exe&&d->native.guard(d->native.context)==1&&readable_range(exe+0x131ac20,16)&&
        lid_wire_equal(exe+0x131ac20,prefix,16)&&executable_ptr(exe,exe+0x131ac20)&&
        executable_ptr(exe,exe+0x130c7d0);
}
static int starter_native_erase(void*context,unsigned part,unsigned level){
    typedef void*(WINAPI*Find)(void*,const unsigned short*,unsigned);
    typedef unsigned char(WINAPI*Erase)(void*,void*);
    LidBlueprintDomain*d=context;BYTE*exe=g_progress_event_exe;BYTE*row;unsigned short ptid[64];
    if((part!=16&&part!=20)||(level!=1&&level!=2)||!starter_native_profile(d))return 0;
    lid_bp_utf_from_ascii(ptid,lid_blueprint_parts[part]);
    row=((Find)(exe+0x130c7d0))(exe+0x27e3000,ptid,level);
    if(!row||!readable_range(row,0x58)||*(unsigned*)(row+0x18)!=level||
       !v074_skill_fstring_matches(row+8,lid_blueprint_parts[part]))return 0;
    return ((Erase)(exe+0x131ac20))(exe+0x27e3000,row)==1;
}
static int starter_guard(void*context){LidBlueprintDomain*d=context;return starter_native_profile(d);}
static int starter_read(void*context,LidBlueprintNativeSnapshot*out){
    LidBlueprintDomain*d=context;return d->native.read_model(d->native.context,out);
}
static int starter_prepare(void*context,const LidWireMessage*target){
    LidBlueprintDomain*d=context;g_starter_cleanup_mask=0;g_starter_committed=0;
    if(!lid_blueprint_domain_prepare(d,target))return 0;
    if(g_starter_policy_state==0){
        g_starter_cleanup_mask=lid_starter_mask(&d->before,d->counts,d->waiting_parts);
        if(g_starter_cleanup_mask&&!starter_native_profile(d))return 0;
    }
    return 1;
}
static int starter_commit(void*context){
    LidBlueprintDomain*d=context;unsigned mask=g_starter_cleanup_mask;
    if(g_starter_policy_state==0&&mask){
        LidStarterOps ops={d,starter_guard,starter_read,starter_native_erase};
        lid_wire_zero(d->waiting_parts,sizeof(d->waiting_parts));
        if(d->waiting(d->waiting_context,d->waiting_parts,976)!=1||
           lid_starter_mask(&d->before,d->counts,d->waiting_parts)!=mask||
           !lid_starter_cleanup(&d->before,&d->after,mask,&ops))return 0;
        log_ascii("AP STARTER R22: inherited default blueprint pairs removed; AP receipts and other plans preserved.");
    }
    if(!lid_blueprint_domain_commit(d))return 0;
    if(g_starter_policy_state==0)g_starter_committed=1;
    return 1;
}
static int starter_readback(void*context){
    LidBlueprintDomain*d=context;
    return lid_blueprint_domain_readback(d)&&lid_starter_absent(&d->after,g_starter_cleanup_mask);
}
#endif
