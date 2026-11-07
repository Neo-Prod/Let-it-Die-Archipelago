#ifndef LID_RELOAD_POOL_BINDING_H
#define LID_RELOAD_POOL_BINDING_H
/* Generated from real Ghidra bytes + exact Steam SHA. Research only. */
#include "reload_pool_scope.h"
typedef struct LidReloadAttachOverride {void*detour;void*trampoline;} LidReloadAttachOverride;
/* Only the adapter may supply its OWN recorded shop hook. Validate every byte
   of that patch and its original-code trampoline; never admit arbitrary hooks. */
static int lid_reload_attach_override_valid(uintptr_t base,int(*readable)(const void*,size_t),
    const LidReloadAttachOverride*owned){
    static const unsigned char original[16]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57};
    const unsigned char*entry=(const unsigned char*)(base+0x26ad90);const unsigned char*tr;
    void*target=0;size_t i;
    if(!base||!readable||!owned||!owned->detour||!owned->trampoline||
       owned->detour==(void*)entry||owned->trampoline==(void*)entry||owned->detour==owned->trampoline)return 0;
    tr=owned->trampoline;
    if(!readable(entry,16)||!readable(tr,30)||!readable(owned->detour,1))return 0;
    if(entry[0]!=0xff||entry[1]!=0x25||entry[14]!=0x90||entry[15]!=0x90)return 0;
    for(i=2;i<6;i++)if(entry[i])return 0;
    __builtin_memcpy(&target,entry+6,8);if(target!=owned->detour)return 0;
    for(i=0;i<16;i++)if(tr[i]!=original[i])return 0;
    if(tr[16]!=0xff||tr[17]!=0x25)return 0;
    for(i=18;i<22;i++)if(tr[i])return 0;
    __builtin_memcpy(&target,tr+22,8);return target==(void*)(entry+16);
}
static int lid_reload_pool_bind_ex(LidReloadPoolBinding*out,uintptr_t base,int exact_sha_verified,
    void*ctx,int(*window)(void*),int(*readable)(const void*,size_t),const LidReloadAttachOverride*owned){
    LidReloadPoolBinding b={0};void*pool=0;void*slot=0;size_t i;
    if(!out||out->initialized||!base||base>UINTPTR_MAX-0xf8b1000||exact_sha_verified!=1||!window||!readable||window(ctx)!=1)return 0;
    {static const unsigned char expected[]={0x4c,0x8b,0xdc,0x55,0x56,0x57,0x48,0x83,0xec,0x70,0x49,0xc7,0x43,0x98,0xfe,0xff};
    const unsigned char*code=(const unsigned char*)(base+0xc2bae0);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x40,0x55,0x56,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff};
    const unsigned char*code=(const unsigned char*)(base+0xbf61d0);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57};
    const unsigned char*code=(const unsigned char*)(base+0x26ad90);
    if(owned){if(!lid_reload_attach_override_valid(base,readable,owned))return 0;}
    else{if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}}
    {static const unsigned char expected[]={0x40,0x55,0x57,0x41,0x56,0x48,0x8b,0xec,0x48,0x83,0xec,0x40,0x48,0xc7,0x45,0xe0};
    const unsigned char*code=(const unsigned char*)(base+0x135aed0);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x40,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x40,0x48,0xc7,0x44};
    const unsigned char*code=(const unsigned char*)(base+0x11c0630);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    if(!readable((void*)(base+0x298ec78),8)||!readable((void*)(base+0x1feaf40),8)||!readable((void*)(base+0x1feaf88),8)||!readable((void*)(base+0x1feafa0),8))return 0;
    __builtin_memcpy(&pool,(void*)(base+0x298ec78),8);
    if(!pool||!readable(pool,0xf0)||!readable((void*)(base+0xf8b0080),0x400))return 0;
    __builtin_memcpy(&slot,(void*)(base+0x1feaf40),8);if(slot!=(void*)(base+0x26ad90))return 0;
    __builtin_memcpy(&slot,(void*)(base+0x1feaf88),8);if(slot!=(void*)(base+0x257350))return 0;
    __builtin_memcpy(&slot,(void*)(base+0x1feafa0),8);if(slot!=(void*)(base+0x246700))return 0;
    b.base=base;b.pool=pool;b.soul_repository=(void*)(base+0xf8b0080);b.object_vtable=(void*)(base+0x1feac70);
    b.context=ctx;b.window=window;b.readable=readable;
    b.call.release=(int(*)(void*,void*))(base+0xc2bae0);
    b.call.allocate=(void*(*)(void*))(base+0xbf61d0);
    b.call.attach=(void(*)(void*,const LidReloadPoolString*,void*))(owned?(uintptr_t)owned->trampoline:base+0x26ad90);
    b.call.soul=(unsigned char(*)(void*,void**))(base+0x135aed0);
    b.call.bodyuser=(unsigned char(*)(void**))(base+0x11c0630);
    b.initialized=1;*out=b;return 1;}
static int lid_reload_pool_bind(LidReloadPoolBinding*out,uintptr_t base,int exact_sha_verified,
    void*ctx,int(*window)(void*),int(*readable)(const void*,size_t)){
    return lid_reload_pool_bind_ex(out,base,exact_sha_verified,ctx,window,readable,0);
}
#endif
