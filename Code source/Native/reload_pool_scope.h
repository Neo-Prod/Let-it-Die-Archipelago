#ifndef LID_RELOAD_POOL_SCOPE_H
#define LID_RELOAD_POOL_SCOPE_H
/* Native ABI scope promoted from tested R26V. Include stdint.h/stddef.h.
   Caller supplies serialized logical ownership and a stable session.
   No runner lock may be held across pool/writer calls (R26Y).
   Native pool return recursively releases ObjectMap/ObjectArray children.
   No sharing, parser, apply, file IO, receipt or ACK in this component. */
typedef struct __attribute__((packed,aligned(4))) LidReloadPoolString {
    unsigned short *data; int num,capacity;
} LidReloadPoolString;
typedef struct LidReloadPoolCalls {
    void* (*allocate)(void*);
    int (*release)(void*,void*); /* 140C2BAE0 returns EAX32. */
    unsigned char (*bodyuser)(void**);
    unsigned char (*soul)(void*,void**);
    void (*attach)(void*,const LidReloadPoolString*,void*);
} LidReloadPoolCalls;
typedef struct LidReloadPoolBinding {
    uintptr_t base; void*pool; void*soul_repository; void*object_vtable;
    void*context; int (*window)(void*); int (*readable)(const void*,size_t);
    LidReloadPoolCalls call; int initialized,poisoned;
} LidReloadPoolBinding;
typedef struct LidReloadPoolScope {
    void*root; void*soul; unsigned attached,closed,ready;
} LidReloadPoolScope;
static int lid_reload_pool_object(const LidReloadPoolBinding*b,const void*p){
    void*vt=0;
    if(!p||!b->readable(p,0x120))return 0;
    __builtin_memcpy(&vt,p,sizeof(vt));return vt==b->object_vtable;
}
static int lid_reload_pool_window(const LidReloadPoolBinding*b){
    void*pool=0;
    if(!b||!b->initialized||b->poisoned||!b->window||!b->readable||
       b->window(b->context)!=1||!b->readable((void*)(b->base+0x298ec78),8))return 0;
    __builtin_memcpy(&pool,(void*)(b->base+0x298ec78),8);return pool==b->pool&&pool;
}
/* Result must be checked even after successful build. A cleanup failure
   poisons this binding and prevents repeated native frees or another scope. */
static int lid_reload_pool_scope_close(LidReloadPoolBinding*b,LidReloadPoolScope*s){
    int good=1;
    if(!b||!s||s->closed||!b->call.release||!lid_reload_pool_window(b))return 0;
    s->closed=1;s->ready=0;
    if(s->soul&&!s->attached&&b->call.release(b->pool,s->soul)!=1)good=0;
    if(s->root&&b->call.release(b->pool,s->root)!=1)good=0;
    s->root=s->soul=0;
    if(!good)b->poisoned=1;
    return good;
}
static int lid_reload_pool_scope_build(LidReloadPoolBinding*b,LidReloadPoolScope*s){
    static unsigned short key[5]={'s','o','u','l',0};
    LidReloadPoolString name={key,5,5};void*root;
    if(!b||!s||s->root||s->soul||s->closed||s->ready||s->attached||
       !lid_reload_pool_window(b)||!b->call.allocate||!b->call.release||!b->call.bodyuser||
       !b->call.soul||!b->call.attach)return 0;
    root=s->root=b->call.allocate(b->pool);
    if(!root){s->closed=1;return 0;}
    if(!lid_reload_pool_object(b,root)){b->poisoned=1;return 0;}
    if(b->call.bodyuser(&s->root)!=1)goto fail;
    if(s->root!=root){b->poisoned=1;return 0;}
    if(b->call.soul(b->soul_repository,&s->soul)!=1)goto fail;
    if(!lid_reload_pool_object(b,s->soul)){b->poisoned=1;return 0;}
    b->call.attach(root,&name,s->soul);s->attached=1;
    s->ready=1;return 1;
fail:
    if(s->root!=root||(s->soul&&!lid_reload_pool_object(b,s->soul))){b->poisoned=1;return 0;}
    (void)lid_reload_pool_scope_close(b,s);return 0;
}
#endif
