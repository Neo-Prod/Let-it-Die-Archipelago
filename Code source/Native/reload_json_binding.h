#ifndef LID_RELOAD_JSON_BINDING_H
#define LID_RELOAD_JSON_BINDING_H
#include <stdint.h>
#include <stddef.h>
#include "reload_pool_binding.h"
/* Exact native ABI from R26AB Ghidra/PE evidence. No locks held across
   pool/native calls: window is logical serialized ownership only. */
typedef struct LidReloadJsonBinding {
    LidReloadPoolBinding pool;
    void*(*get)(void*,const LidReloadPoolString*);
    LidReloadPoolString*(*string)(void*,LidReloadPoolString*,const LidReloadPoolString*);
    void(*encode)(LidReloadPoolString*,void*);
    void*(*parse)(LidReloadPoolString*);
    int(*equal)(void*,void*,int);
    void(*native_free)(void*);
    void*memory_context;void*(*allocate)(void*,size_t);void(*release)(void*,void*);
} LidReloadJsonBinding;
static int lid_reload_json_bind_ex(LidReloadJsonBinding*out,uintptr_t base,int exact_sha_verified,
    void*ctx,int(*window)(void*),int(*readable)(const void*,size_t),
    void*(*allocate)(void*,size_t),void(*release)(void*,void*),const LidReloadAttachOverride*owned){
    LidReloadJsonBinding b={0};size_t i;void*slot;
    if(!out||out->pool.initialized||!allocate||!release||!lid_reload_pool_bind_ex(&b.pool,base,exact_sha_verified,ctx,window,readable,owned))return 0;
    {static const unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8d,0x99,0xa8,0x0,0x0,0x0,0x4c,0x8b,0xc2};
    const unsigned char*code=(const unsigned char*)(base+0x251b30);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x48,0x8b,0xc4,0x48,0x89,0x50,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83};
    const unsigned char*code=(const unsigned char*)(base+0x253ab0);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x40,0x55,0x56,0x57,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0x90,0x0,0x0,0x0};
    const unsigned char*code=(const unsigned char*)(base+0x24bc40);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x48,0x89,0x4c,0x24,0x8,0x55,0x53,0x56,0x57,0x48,0x8d,0x6c,0x24,0xc8,0x48,0x81};
    const unsigned char*code=(const unsigned char*)(base+0x248b00);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x44,0x89,0x44,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    const unsigned char*code=(const unsigned char*)(base+0x257350);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    {static const unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0xd,0xe0,0xed,0x72,0x2};
    const unsigned char*code=(const unsigned char*)(base+0xdfdf0);
    if(!readable(code,sizeof(expected)))return 0;
    for(i=0;i<sizeof(expected);i++)if(code[i]!=expected[i])return 0;}
    if(!readable((void*)(base+0x1feaec8),8))return 0;
    __builtin_memcpy(&slot,(void*)(base+0x1feaec8),8);if(slot!=(void*)(base+0x251b30))return 0;
    if(!readable((void*)(base+0x1feaee8),8))return 0;
    __builtin_memcpy(&slot,(void*)(base+0x1feaee8),8);if(slot!=(void*)(base+0x253ab0))return 0;
    if(!readable((void*)(base+0x1feaf88),8))return 0;
    __builtin_memcpy(&slot,(void*)(base+0x1feaf88),8);if(slot!=(void*)(base+0x257350))return 0;
    b.get=(void*(*)(void*,const LidReloadPoolString*))(base+0x251b30);
    b.string=(LidReloadPoolString*(*)(void*,LidReloadPoolString*,const LidReloadPoolString*))(base+0x253ab0);
    b.encode=(void(*)(LidReloadPoolString*,void*))(base+0x24bc40);
    b.parse=(void*(*)(LidReloadPoolString*))(base+0x248b00);
    b.equal=(int(*)(void*,void*,int))(base+0x257350);
    b.native_free=(void(*)(void*))(base+0xdfdf0);
    b.memory_context=ctx;b.allocate=allocate;b.release=release;*out=b;return 1;}
static int lid_reload_json_bind(LidReloadJsonBinding*out,uintptr_t base,int exact_sha_verified,
    void*ctx,int(*window)(void*),int(*readable)(const void*,size_t),
    void*(*allocate)(void*,size_t),void(*release)(void*,void*)){
    return lid_reload_json_bind_ex(out,base,exact_sha_verified,ctx,window,readable,allocate,release,0);
}
#endif
