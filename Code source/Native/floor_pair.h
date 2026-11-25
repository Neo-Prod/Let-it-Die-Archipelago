#ifndef LID_FLOOR_PAIR_H
#define LID_FLOOR_PAIR_H
#include "floor_wire.h"
/* Decorate existing target reads. All file reads precede engine reservations.
   A missing optional companion preserves legacy transport exactly. */
typedef struct LidFloorPair {
    LidCharacterHostOps delegate;void*context;
    int(*read_floor)(void*,BYTE*,size_t,size_t*);
    int(*prepare_gate)(void*);
    LidFloorTarget pending,current;
    unsigned initialized,have_pending,have_current;
    BYTE scratch[321];
} LidFloorPair;
static int lid_floor_pair_read(void*context,int id,BYTE*out,size_t capacity,size_t*size){
    LidFloorPair*p=context;size_t got=0;int result;LidFloorTarget packet;
    if(!p||!p->initialized)return -1;
    result=p->delegate.read(p->delegate.context,id,out,capacity,size);
    if(result!=1||id!=LID_HOST_TARGET_FILE)return result;
    if(capacity!=256||!size||*size!=256)return -1;
    result=p->read_floor(p->context,p->scratch,sizeof(p->scratch),&got);
    if(result==0){p->have_pending=0;return 1;}
    if(result!=1||!lid_floor_decode(&packet,p->scratch,got,out)){*size=0;return result<0?-1:0;}
    if(packet.enabled&&p->prepare_gate(p->context)!=1){*size=0;return 0;}
    p->pending=packet;p->have_pending=1;return 1;
}
static int lid_floor_pair_publish(void*context,int id,const BYTE*raw,size_t size){
    LidFloorPair*p=context;return p&&p->initialized?p->delegate.publish(p->delegate.context,id,raw,size):0;
}
static int lid_floor_pair_mutex(void*context){
    LidFloorPair*p=context;return p&&p->initialized?p->delegate.own_mutex(p->delegate.context):-1;
}
static int lid_floor_pair_init(LidFloorPair*p,LidCharacterHostOps*wrapped,const LidCharacterHostOps*delegate,
                              void*context,int(*read_floor)(void*,BYTE*,size_t,size_t*),int(*prepare_gate)(void*)){
    if(!p||p->initialized||!wrapped||!delegate||!delegate->read||!delegate->publish||!delegate->own_mutex||!read_floor||!prepare_gate)return 0;
    lid_wire_zero(p,sizeof(*p));p->delegate=*delegate;p->context=context;p->read_floor=read_floor;p->prepare_gate=prepare_gate;p->initialized=1;
    *wrapped=(LidCharacterHostOps){p,lid_floor_pair_read,lid_floor_pair_publish,lid_floor_pair_mutex};return 1;
}
/* Caller proves actual entry completion and native acknowledged frozen target. */
static int lid_floor_pair_commit(LidFloorPair*p,const LidWireMessage*target,unsigned long long acknowledged){
    BYTE raw[256];
    if(!p||!p->initialized||!p->have_pending||!target||target->kind!=LID_WIRE_TARGET||!acknowledged||
       acknowledged!=target->sequence||!lid_wire_encode(raw,256,target)||!lid_wire_equal(raw,p->pending.target,256))return 0;
    if(p->have_current&&lid_wire_equal(p->current.target+40,p->pending.target+40,32)&&p->pending.applied<p->current.applied)return 0;
    p->current=p->pending;p->have_current=1;return 1;
}
#endif
