#ifndef LID_SHOP_MOMOKA_GAME_ROOT_H
#define LID_SHOP_MOMOKA_GAME_ROOT_H
#include "momoka_diagnostic_driver.h"
#include "shop_momoka_current_root.h"
/* Concrete root provider for the actual M214 Windows/session binding.
   Same verified GameInfo chain/vtable/started flag as progression_native_adapter
   and progression_reservation, but all reads use the existing bounded RPM.
   UI-owner synchronous VM lifetime only; NOT a GC lock or worker API.
   Driver/source/root stay resident; exact build verification is M214 precondition.
   No image lookup, engine invocation, file IO, save write or cached instance. */
typedef struct LidShopMomokaGame {
    unsigned initialized;M214Driver*driver;uint64_t image;
} LidShopMomokaGame;
static int lid_sg_game_current(void*context,uint64_t*out){
    LidShopMomokaGame*c=context;uint64_t engine,world,actors,first,game,vt,a,b,again;uint32_t flags;
    if(!c||!c->initialized||!out||!c->driver||!c->driver->prepared)return 0;
    M214Driver*d=c->driver;M213Adapter*w=&d->adapter;
    if(!w->api.thread||w->api.thread()!=w->prepared.gate.owner||
       !d->session||!d->session(d->session_context))return 0;
    M195Ops o={w,m213_copy,m213_name};
    if(!m195_read(&o,c->image,0x29759f0,&engine,8)||
       !m195_read(&o,engine,0x80,&world,8)||!m195_read(&o,world,0x60,&actors,8)||
       !m195_read(&o,actors,0,&first,8)||!m195_read(&o,first,0x714,&game,8)||
       !m195_read(&o,game,0,&vt,8)||!m195_read(&o,vt,0xa98,&a,8)||a!=c->image+0x10b5c50||
       !m195_read(&o,vt,0x880,&b,8)||b!=c->image+0x10b35a0||
       !m195_read(&o,game,0x844,&flags,4)||!(flags&0x08000000u)||
       !m195_read(&o,c->image,0x29759f0,&again,8)||again!=engine||
       !m195_read(&o,engine,0x80,&again,8)||again!=world||
       !d->session(d->session_context))return 0;
    *out=game;return 1;
}
static int lid_sg_root_prepare(LidShopMomokaGame*c,LidShopMomokaRoot*root,
                               M214Driver*d,uint64_t image){
    uint64_t game;
    if(!c||c->initialized||!root||root->current_game||!d||!d->prepared||
       !image||image>UINT64_MAX-0x29759f8||
       d->adapter.prepared.native.original!=image+0x107cf80||
       !d->adapter.api.thread||d->adapter.api.thread()!=d->adapter.prepared.gate.owner)return 0;
    c->driver=d;c->image=image;c->initialized=1;
    if(!lid_sg_game_current(c,&game)){c->initialized=0;return 0;}
    root->memory=(M195Ops){&d->adapter,m213_copy,m213_name};
    root->context=c;root->current_game=lid_sg_game_current;return 1;
}
#endif
