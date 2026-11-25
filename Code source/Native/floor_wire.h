#ifndef LID_FLOOR_WIRE_H
#define LID_FLOOR_WIRE_H
/* Optional companion. No changes to the nine-track character protocol. */
#define LID_FLOOR_FRAME_SIZE 320u
typedef struct LidFloorTarget {unsigned enabled,goal,quantity,received,applied;BYTE target[256];} LidFloorTarget;
static int lid_floor_session(const LidFloorTarget*p,const LidWireMessage*binding){
    return p&&binding&&lid_wire_r32(p->target+16)==binding->pid&&lid_wire_r32(p->target+20)==binding->writer_pid&&
        lid_wire_r64(p->target+244)==binding->creation_time&&lid_wire_equal(p->target+24,binding->nonce,16)&&
        lid_wire_equal(p->target+40,binding->identity,32)&&lid_wire_equal(p->target+72,binding->config,32)&&
        lid_wire_equal(p->target+228,binding->writer,16);
}
static unsigned lid_floor_required(unsigned floor){return floor>=1&&floor<=40?floor-1-(floor-1)/10:0;}
static int lid_floor_exempt(unsigned from,unsigned to){return(from==10&&to==11)||(from==20&&to==21)||(from==30&&to==31)||(from==40&&to==41);}
static int lid_floor_blocked(const LidFloorTarget*p,unsigned from,unsigned to){
    return p&&p->enabled&&from>=1&&from<=40&&to>from&&to<=40&&!lid_floor_exempt(from,to)&&p->applied<lid_floor_required(to);
}
static unsigned lid_floor_checksum(const BYTE*raw){unsigned h=2166136261u;for(unsigned i=0;i<320;i++)if(i<60||i>=64)h=(h^raw[i])*16777619u;return h;}
static int lid_floor_decode(LidFloorTarget*out,const BYTE*raw,size_t size,const BYTE*target){
    static const BYTE magic[8]={'L','I','D','A','P','F','0','1'};LidWireMessage m;LidFloorTarget p;unsigned long long stats=0;
    if(!out||!raw||!target||size!=320||!lid_wire_equal(raw,magic,8)||lid_wire_r32(raw+8)!=1||lid_wire_r32(raw+12)!=320||
       !lid_wire_equal(raw+64,target,256)||!lid_wire_decode(&m,target,256)||m.kind!=LID_WIRE_TARGET||lid_wire_r32(raw+60)!=lid_floor_checksum(raw))return 0;
    for(unsigned i=36;i<60;i++)if(raw[i])return 0;
    p.enabled=lid_wire_r32(raw+16);p.goal=lid_wire_r32(raw+20);p.quantity=lid_wire_r32(raw+24);p.received=lid_wire_r32(raw+28);p.applied=lid_wire_r32(raw+32);
    if(p.enabled>1||(p.goal!=10&&p.goal!=20&&p.goal!=30&&p.goal!=40)||p.quantity>1000||
       (p.enabled&&p.quantity<lid_floor_required(p.goal))||(!p.enabled&&p.received))return 0;
    for(unsigned i=0;i<9;i++)stats+=m.counts[i];
    if(stats>m.history_length||p.received>m.history_length-stats||p.applied!=(p.received<lid_floor_required(p.goal)?p.received:lid_floor_required(p.goal)))return 0;
    lid_wire_copy(p.target,target,256);*out=p;return 1;
}
#endif
