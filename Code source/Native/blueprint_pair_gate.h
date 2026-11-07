#ifndef LID_BLUEPRINT_PAIR_GATE_H
#define LID_BLUEPRINT_PAIR_GATE_H
/* Candidate read decorator. Include progression_host.h, blueprint_wire.h and
   blueprint_catalog.h first. Not active in mod.c. All I/O before engine locks.
   Original host still owns session/writer/anti-replay checks. */
#define LID_BLUEPRINT_CATALOG_FRAME_SIZE (320u+4u*LID_BLUEPRINT_FAMILY_COUNT)
typedef struct LidBlueprintPairGate {
    LidCharacterHostOps delegate;
    void* companion_context;
    int (*read_companion)(void*,BYTE*,size_t,size_t*);
    BYTE companion[LID_BLUEPRINT_CATALOG_FRAME_SIZE];
    BYTE accepted_target[256];
    unsigned counts[LID_BLUEPRINT_FAMILY_COUNT];
    unsigned initialized,have_pair;
} LidBlueprintPairGate;
static int lid_blueprint_pair_read(void* context,int id,BYTE* out,size_t capacity,size_t* size){
    LidBlueprintPairGate* gate=context;LidWireMessage target;size_t got=0;int result;
    if(size)*size=0;
    if(!gate||!gate->initialized||!out||!size||capacity!=256)return -1;
    if(id!=LID_HOST_TARGET_FILE)return gate->delegate.read(gate->delegate.context,id,out,capacity,size);
    result=gate->delegate.read(gate->delegate.context,id,out,capacity,size);
    if(result!=1)return result;
    if(*size!=256)return -1;
    result=gate->read_companion(gate->companion_context,gate->companion,sizeof(gate->companion),&got);
    if(result!=1){*size=0;return result==0?0:-1;}
    if(!lid_blueprint_decode(gate->counts,LID_BLUEPRINT_FAMILY_COUNT,&target,gate->companion,got,out,lid_blueprint_catalogue_hash)){
        *size=0;return 0; /* transient pair mismatch must never reach host */
    }
    lid_wire_copy(gate->accepted_target,out,256);gate->have_pair=1;
    return 1;
}
static int lid_blueprint_pair_publish(void* context,int id,const BYTE* data,size_t size){
    LidBlueprintPairGate* gate=context;
    if(!gate||!gate->initialized)return 0;
    return gate->delegate.publish(gate->delegate.context,id,data,size);
}
static int lid_blueprint_pair_mutex(void* context){
    LidBlueprintPairGate* gate=context;
    if(!gate||!gate->initialized)return -1;
    return gate->delegate.own_mutex(gate->delegate.context);
}
static int lid_blueprint_pair_init(LidBlueprintPairGate* gate,LidCharacterHostOps* wrapped,
                                   const LidCharacterHostOps* delegate,void* context,
                                   int (*read_companion)(void*,BYTE*,size_t,size_t*)){
    if(!gate||gate->initialized||!wrapped||!delegate||!delegate->read||!delegate->publish||!delegate->own_mutex||!read_companion)return 0;
    gate->delegate=*delegate;gate->companion_context=context;gate->read_companion=read_companion;
    gate->have_pair=0;gate->initialized=1;
    *wrapped=(LidCharacterHostOps){gate,lid_blueprint_pair_read,lid_blueprint_pair_publish,lid_blueprint_pair_mutex};return 1;
}
/* Called only for the host-accepted TARGET, before the event stops transport
   reads. Copy out once and retain with the entry; never refetch a latest file.
   No output change on failure. A staged but host-rejected session cannot match
   a different accepted TARGET. This function is not a save acknowledgement. */
static int lid_blueprint_pair_counts(const LidBlueprintPairGate* gate,const LidWireMessage* target,
                                     unsigned* out,size_t count){
    BYTE frame[256];size_t i;
    if(!gate||!gate->initialized||!gate->have_pair||!target||target->kind!=LID_WIRE_TARGET||
       !out||count!=LID_BLUEPRINT_FAMILY_COUNT||!lid_wire_encode(frame,256,target)||
       !lid_wire_equal(frame,gate->accepted_target,256))return 0;
    for(i=0;i<count;i++)out[i]=gate->counts[i];return 1;
}
#endif
