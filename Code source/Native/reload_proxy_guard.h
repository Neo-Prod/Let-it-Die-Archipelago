#ifndef LID_RELOAD_PROXY_GUARD_RESEARCH_H
#define LID_RELOAD_PROXY_GUARD_RESEARCH_H
/* Read-only observer for proxy SHA991af68f...18ed44f8 ONLY.
   Requires winlite.h/progression_wire.h. Caller hashes the actual module file
   ONCE, binds its loaded module, and observes under inactive-save reservation.
   No DLL/flag/path writes, no IO, no hook and no acknowledgement. */
typedef struct LidReloadProxyGuard {const BYTE*module;unsigned state;} LidReloadProxyGuard;
static const BYTE lid_reload_proxy_sha[32]={0x99,0x1a,0xf6,0x8f,0x97,0x40,0x38,0x66,0x9f,0x48,0xc4,0x50,0xc2,0x2a,0x98,0x55,0x9e,0x45,0xc3,0xb0,0x11,0x15,0xe2,0x59,0xa0,0xb4,0x84,0xaf,0x18,0xed,0x44,0xf8};
static int lid_reload_proxy_profile(const BYTE*base,const BYTE*file_sha,int(*readable)(const void*,size_t)){
    static const unsigned rvas[4]={0x13f7,0x3556,0x35ce,0x1e91};
    static const BYTE code[4][10]={
        {0xc7,0x05,0x0b,0x7c,0,0,1,0,0,0},
        {0xc7,0x05,0xac,0x5a,0,0,2,0,0,0},
        {0xc7,0x05,0x34,0x5a,0,0,0,0,0,0},
        {0xc7,0x05,0x71,0x71,0,0,0,0,0,0}};
    unsigned nt,size,i;
    if(!base||!file_sha||!readable||!lid_wire_equal(file_sha,lid_reload_proxy_sha,32)||!readable(base,0x40)||
       base[0]!='M'||base[1]!='Z')return 0;
    nt=lid_wire_r32(base+0x3c);
    if(nt<0x40||nt>0x1000||!readable(base+nt,0x90)||lid_wire_r32(base+nt)!=0x4550||
       base[nt+4]!=0x64||base[nt+5]!=0x86||base[nt+24]!=0x0b||base[nt+25]!=0x02)return 0;
    size=lid_wire_r32(base+nt+24+56);if(size<0xa000||size>0x1000000)return 0;
    for(i=0;i<4;i++)if(!readable(base+rvas[i],10)||!lid_wire_equal(base+rvas[i],code[i],10))return 0;
    return 1;
}
static int lid_reload_proxy_guard_read(LidReloadProxyGuard*out,const BYTE*base,const BYTE*file_sha,
        const unsigned short*expected_path,unsigned chars,int(*readable)(const void*,size_t)){
    unsigned before,i,actual,expected;
    if(!out||!lid_reload_proxy_profile(base,file_sha,readable)||!expected_path||chars<2||chars>900||
       !readable(expected_path,(size_t)chars*2)||expected_path[chars-1]||!readable(base+0x9000,16)||
       !readable(base+0x9020,1800))return 0;
    before=lid_wire_r32(base+0x900c);if(before>2)return 0;
    if(lid_wire_r32(base+0x9000)!=1||lid_wire_r32(base+0x9004)!=1||lid_wire_r32(base+0x9008)!=1)return 0;
    for(i=0;i<chars;i++){
        actual=(unsigned)base[0x9020+2*i]|((unsigned)base[0x9021+2*i]<<8);expected=expected_path[i];
        if(actual!=expected||(!expected&&i+1<chars))return 0;
    }
    if(lid_wire_r32(base+0x900c)!=before)return 0;
    *out=(LidReloadProxyGuard){base,before};return 1;
}
static int lid_reload_proxy_guard_can_write(const LidReloadProxyGuard*v,int save_worker_idle_verified){
    return v&&v->module&&v->state==0&&save_worker_idle_verified==1;
}
#endif
