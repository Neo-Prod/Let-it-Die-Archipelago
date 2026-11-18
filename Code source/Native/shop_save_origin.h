#ifndef LID_SHOP_SAVE_ORIGIN_H
#define LID_SHOP_SAVE_ORIGIN_H
#include <stdint.h>
#include <stddef.h>
/* Read-only classification during current native Save call. No hooks, waits,
   saves, gates, original calls or pointer ownership. exact_sha and established
   game-writer ID must come from trusted host preparation. Not thread identity
   authentication outside this exact normal execution profile. Native or owned
   AP save jobs outside these two callables need their own explicit binding;
   UNKNOWN is never a native Save failure or permission for AP mutation.
   caller must be captured at ORIGINAL Save hook entry and forwarded through
   the existing reload bridge: a proxy's own return address is not equivalent.
   Compare explicit kind values, not truthiness: supplied text is NOT permission
   to insert a fresh-root marker. */
enum {LID_SO_UNKNOWN=0,LID_SO_NATIVE_ROOT=1,LID_SO_SUPPLIED_TEXT=2};
typedef struct LidShopSaveOriginOps {
    void*context;int(*copy)(void*,uintptr_t,void*,size_t);
    uint32_t(*thread)(void*);
} LidShopSaveOriginOps;
typedef struct LidShopSaveOrigin {
    unsigned kind,text_callable,worker_published;uint32_t thread;
} LidShopSaveOrigin;
static int lid_so_read(const LidShopSaveOriginOps*o,uintptr_t p,size_t at,void*out,size_t n){
    return p&&out&&n&&p<=UINTPTR_MAX-at&&p+at<=UINTPTR_MAX-n&&o->copy(o->context,p+at,out,n)==1;
}
static int lid_so_bytes(const LidShopSaveOriginOps*o,uintptr_t p,const unsigned char*expected,unsigned n){
    unsigned char bytes[16];if(!n||n>sizeof(bytes)||!lid_so_read(o,p,0,bytes,n))return 0;
    for(unsigned i=0;i<n;i++)if(bytes[i]!=expected[i])return 0;return 1;
}
/* Output published ONLY when recognised. No native API/JSON/heap calls.
   Root and supplied-text paths have the SAME Run return address because the
   callable invokes Save by tail JMP. Validate current owned callable as well.
   Does not retain the borrowed callable or text pointer in output. */
static int lid_shop_save_origin(const LidShopSaveOriginOps*o,uintptr_t base,int exact_sha,
    uint32_t verified_game_writer,uintptr_t runner,uintptr_t optional_text,uintptr_t caller,LidShopSaveOrigin*out){
    static const unsigned char run_call[]={0xff,0x50,0x10};
    static const unsigned char root_thunk[]={0x48,0x8b,0x49,0x08,0x33,0xd2,0xe9,0x45,0x5e,0xff,0xff};
    static const unsigned char text_jump[]={0xe9,0x7c,0x5e,0xff,0xff},empty_jump[]={0xe9,0x67,0x5e,0xff,0xff};
    uintptr_t vt,callable,again,slot,bound_runner,worker;uint32_t tid;
    if(!o||!o->copy||!o->thread||!out||!base||base>UINTPTR_MAX-0xf8a5508||exact_sha!=1||
       !verified_game_writer||runner!=base+0xf8a5490||caller!=base+0x11bd64f||
       !(tid=o->thread(o->context))||tid==verified_game_writer||
       !lid_so_bytes(o,base+0x11bd64c,run_call,sizeof(run_call))||
       !lid_so_read(o,runner,0,&vt,8)||vt!=base+0x212e0e8||
       !lid_so_read(o,vt,8,&slot,8)||slot!=base+0x11bd630||
       !lid_so_read(o,runner,0x64,&callable,8)||!lid_so_read(o,callable,0,&vt,8)||
       !lid_so_read(o,callable,8,&bound_runner,8)||bound_runner!=runner||
       !lid_so_read(o,vt,0x10,&slot,8)||!lid_so_read(o,runner,0x40,&worker,8))return LID_SO_UNKNOWN;
    LidShopSaveOrigin result={LID_SO_UNKNOWN,0,worker!=0,tid};
    if(vt==base+0x2114598&&slot==base+0x1118710){
        if(optional_text||!lid_so_bytes(o,slot,root_thunk,sizeof(root_thunk)))return LID_SO_UNKNOWN;
        result.kind=LID_SO_NATIVE_ROOT;
    }else if(vt==base+0x2114608&&slot==base+0x11186a0){
        struct {uintptr_t data;int32_t num,capacity;} text;
        if(!lid_so_bytes(o,base+0x11186df,text_jump,sizeof(text_jump))||
           !lid_so_bytes(o,base+0x11186f4,empty_jump,sizeof(empty_jump))||
           !lid_so_read(o,callable,0x10,&text,16)||text.num<0||text.capacity<text.num||text.capacity>1048576)return LID_SO_UNKNOWN;
        result.text_callable=1;
        if(!text.num){
            if(optional_text!=base+0x1ba70f8)return LID_SO_UNKNOWN;
            result.kind=LID_SO_NATIVE_ROOT;
        }else{
            uint16_t first,last;
            if(!text.data||optional_text!=text.data||
               !lid_so_read(o,text.data,0,&first,2)||
               !lid_so_read(o,text.data,(size_t)(text.num-1)*2,&last,2)||last)return LID_SO_UNKNOWN;
            result.kind=first?LID_SO_SUPPLIED_TEXT:LID_SO_NATIVE_ROOT;
        }
    }else return LID_SO_UNKNOWN;
    /* Structural recheck is NOT synchronization. Native Run owns this callable
       through invocation; host must not use this helper from a polling thread. */
    if(!lid_so_read(o,runner,0x64,&again,8)||again!=callable)return LID_SO_UNKNOWN;
    *out=result;return (int)result.kind;
}
#endif
