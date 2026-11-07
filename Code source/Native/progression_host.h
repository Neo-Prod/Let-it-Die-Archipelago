#ifndef LID_PROGRESSION_HOST_H
#define LID_PROGRESSION_HOST_H
/* Include progression_wire.h first. Host/session coordination, no game writes.
   The application adapter must explicitly arm this ONLY once mutation and own
   save/readback are available. No offer is published by an unarmed host. */
enum {LID_HOST_OFFER_FILE=0,LID_HOST_QUIESCED_FILE=1,LID_HOST_TARGET_FILE=2,LID_HOST_SAVED_FILE=3};
typedef struct LidCharacterHostOps {
    void* context;
    int (*read)(void*,int,BYTE*,size_t,size_t*);
    int (*publish)(void*,int,const BYTE*,size_t);
    int (*own_mutex)(void*);
} LidCharacterHostOps;
typedef struct LidCharacterHost {
    LidWirePeer peer;
    LidCharacterHostOps ops;
    BYTE expected_config[32];
    LidWireMessage frozen;
    unsigned long long acknowledged;
    int initialized,offer_published,grant_published,poisoned;
    int active,proved;
} LidCharacterHost;
static int lid_host_init(LidCharacterHost* h,const LidCharacterHostOps* ops,unsigned pid,
                        unsigned long long birth,const BYTE* nonce,const BYTE* config){
    LidCharacterHost initial;
    if(!h||h->initialized||!ops||!ops->read||!ops->publish||!ops->own_mutex||
       !config||!lid_wire_nonzero(config,32))return 0;
    lid_wire_zero(&initial,sizeof(initial));
    if(!lid_wire_peer_init(&initial.peer,pid,birth,nonce))return 0;
    initial.ops=*ops;lid_wire_copy(initial.expected_config,config,32);initial.initialized=1;
    *h=initial;return 1;
}
/* 0=wait,1=offer,2=grant,3=new target,4=replay,-1=bad packet/I/O,
   -2=unsafe mutex (permanent). Input failures never mutate the accepted target.
   An absent file is read return0; error is -1; present is1 with exact length. */
static int lid_host_step(LidCharacterHost* h,int adapter_armed){
    BYTE frame[256];LidWireMessage incoming;size_t size=0;int result,owned;
    if(!h||!h->initialized||adapter_armed!=1)return 0;
    if(h->poisoned)return -2;
    if(!h->offer_published){
        if(!lid_wire_encode(frame,256,&h->peer.offer)||h->ops.publish(h->ops.context,LID_HOST_OFFER_FILE,frame,256)!=1)return -1;
        h->offer_published=1;return 1;
    }
    if(!h->grant_published){
        result=h->ops.read(h->ops.context,LID_HOST_QUIESCED_FILE,frame,256,&size);
        if(result==0)return 0;
        if(result!=1||!lid_wire_decode(&incoming,frame,size)||
           !lid_wire_equal(incoming.config,h->expected_config,32)||
           !lid_wire_peer_bind(&h->peer,&incoming))return -1;
        owned=h->ops.own_mutex(h->ops.context);
        if(owned<0){h->poisoned=1;return -2;}
        if(owned!=1)return 0;
        if(!lid_wire_peer_grant(&h->peer,1,frame)||h->ops.publish(h->ops.context,LID_HOST_OFFER_FILE,frame,256)!=1)return -1;
        h->grant_published=1;return 2;
    }
    result=h->ops.read(h->ops.context,LID_HOST_TARGET_FILE,frame,256,&size);
    if(result==0)return 0;
    if(result!=1||!lid_wire_decode(&incoming,frame,size))return -1;
    result=lid_wire_peer_target(&h->peer,&incoming);
    return result==1?3:result==2?4:-1;
}
/* Call only within the application's freshly validated hub/reservation window.
   A second receipt can update peer.last_target while this frozen lot is saved. */
static int lid_host_freeze(LidCharacterHost* h,LidWireMessage* out){
    if(!h||!out||!h->initialized||!h->grant_published||h->poisoned||h->active||
       h->peer.last_target.sequence<=h->acknowledged)return 0;
    h->frozen=h->peer.last_target;h->active=1;h->proved=0;*out=h->frozen;return 1;
}
/* Store the application's OWN save/readback proof for exactly this lot.
   This function never infers proof from a global result or mere receipt. */
static int lid_host_prove(LidCharacterHost* h,unsigned long long sequence,const BYTE* generation,
                         int own_save_and_readback_ok){
    if(!h||!h->active||h->poisoned||own_save_and_readback_ok!=1||!generation||
       h->frozen.sequence!=sequence||!lid_wire_equal(h->frozen.generation,generation,32))return 0;
    h->proved=1;return 1;
}
/* Only a caller that has proved NO application/event started may cancel.
   The received target remains pending and can be frozen again on a later Tick. */
static int lid_host_cancel_unapplied(LidCharacterHost* h,int no_mutation_started){
    if(!h||!h->active||h->proved||no_mutation_started!=1)return 0;
    h->active=0;lid_wire_zero(&h->frozen,sizeof(h->frozen));return 1;
}
/* May retry publication after releasing engine locks. Failed I/O keeps the lot
   and proof; it does NOT require another game save and cannot confirm a newer lot. */
static int lid_host_publish_saved(LidCharacterHost* h){
    BYTE frame[256];
    if(!h||!h->active||!h->proved||h->poisoned||
       !lid_wire_peer_saved(&h->peer,&h->frozen,1,frame)||
       h->ops.publish(h->ops.context,LID_HOST_SAVED_FILE,frame,256)!=1)return 0;
    h->acknowledged=h->frozen.sequence;h->active=h->proved=0;return 1;
}
#endif
