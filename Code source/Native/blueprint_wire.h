#ifndef LID_BLUEPRINT_WIRE_H
#define LID_BLUEPRINT_WIRE_H
/* Candidate pure companion codec. Include progression_wire.h first.
   Not connected to mod.c. Host must bind TARGET/session and pin both domains
   before mutation. No I/O, allocations, hooks, gameplay calls or save proofs. */
#define LID_BLUEPRINT_HEADER 64u
#define LID_BLUEPRINT_COUNTS 320u
#define LID_BLUEPRINT_MAX_SIZE 4096u
#define LID_BLUEPRINT_MAX_FAMILIES 944u
static const BYTE lid_blueprint_magic[8]={'L','I','D','A','P','B','0','1'};
static unsigned lid_blueprint_checksum(const BYTE* raw,size_t n){
    unsigned value=2166136261u;size_t i;
    for(i=0;i<n;i++)if(i<52||i>=56)value=(value^raw[i])*16777619u;
    return value;
}
static int lid_blueprint_shape(size_t count,const BYTE* catalogue){
    return count>0&&count<=LID_BLUEPRINT_MAX_FAMILIES&&catalogue&&lid_wire_nonzero(catalogue,32);
}
/* V1 stays byte-identical. V2 permits at most three plans for each remaining
   non-stat receipt; it never inflates or fabricates the shared history. */
static unsigned lid_blueprint_budget(unsigned long long stats,unsigned long long plans,unsigned long long history){
    if(stats>history||plans>3*(history-stats))return 0;
    return stats+plans<=history?1u:2u;
}
static int lid_blueprint_frame_encode(BYTE* out,size_t size,const LidWireMessage* target,
                                const unsigned* counts,size_t count,const BYTE* catalogue,unsigned kind,unsigned allow_sets){
    unsigned long long stats=0,plans=0;unsigned version;size_t i;BYTE character[256];
    if(!out||!counts||!lid_blueprint_shape(count,catalogue)||size!=320+4*count||
       !lid_wire_valid(target)||target->kind!=kind||!lid_wire_encode(character,256,target))return 0;
    for(i=0;i<9;i++)stats+=target->counts[i];
    for(i=0;i<count;i++){if(counts[i]>100000)return 0;plans+=counts[i];}
    version=lid_blueprint_budget(stats,plans,target->history_length);
    if(!version||(version==2&&!allow_sets))return 0;
    lid_wire_zero(out,size);lid_wire_copy(out,lid_blueprint_magic,8);
    lid_wire_w32(out+8,version);lid_wire_w32(out+12,64);lid_wire_w32(out+16,(unsigned)count);
    lid_wire_copy(out+20,catalogue,32);lid_wire_copy(out+64,character,256);
    for(i=0;i<count;i++)lid_wire_w32(out+320+4*i,counts[i]);
    lid_wire_w32(out+52,lid_blueprint_checksum(out,size));return 1;
}
/* Caller-owned outputs and inputs must be distinct, readable/writable regions.
   No output is changed on rejection. Size/count bounded before any raw access. */
static int lid_blueprint_frame_decode(unsigned* counts,size_t count,LidWireMessage* target,
                                const BYTE* raw,size_t size,const BYTE* expected_target,const BYTE* catalogue,unsigned kind){
    LidWireMessage parsed;unsigned long long stats=0,plans=0;unsigned version;size_t i;
    if(!counts||!target||!raw||!expected_target||!lid_blueprint_shape(count,catalogue)||
       size!=320+4*count||!lid_wire_equal(raw,lid_blueprint_magic,8)||
       (lid_wire_r32(raw+8)!=1&&lid_wire_r32(raw+8)!=2)||
       lid_wire_r32(raw+12)!=64||lid_wire_r32(raw+16)!=count||
       !lid_wire_equal(raw+20,catalogue,32)||lid_wire_nonzero(raw+56,8)||
       lid_wire_r32(raw+52)!=lid_blueprint_checksum(raw,size)||
       !lid_wire_equal(raw+64,expected_target,256)||!lid_wire_decode(&parsed,raw+64,256)||
       parsed.kind!=kind)return 0;
    for(i=0;i<9;i++)stats+=parsed.counts[i];
    for(i=0;i<count;i++){unsigned value=lid_wire_r32(raw+320+4*i);if(value>100000)return 0;plans+=value;}
    version=lid_blueprint_budget(stats,plans,parsed.history_length);
    if(!version||lid_wire_r32(raw+8)!=version)return 0;
    for(i=0;i<count;i++)counts[i]=lid_wire_r32(raw+320+4*i);
    *target=parsed;return 1;
}
static int lid_blueprint_encode(BYTE* out,size_t size,const LidWireMessage* target,
                                const unsigned* counts,size_t count,const BYTE* catalogue){
    return lid_blueprint_frame_encode(out,size,target,counts,count,catalogue,LID_WIRE_TARGET,0);
}
static int lid_blueprint_encode_sets(BYTE* out,size_t size,const LidWireMessage* target,
                                const unsigned* counts,size_t count,const BYTE* catalogue){
    return lid_blueprint_frame_encode(out,size,target,counts,count,catalogue,LID_WIRE_TARGET,1);
}
static int lid_blueprint_decode(unsigned* counts,size_t count,LidWireMessage* target,
                                const BYTE* raw,size_t size,const BYTE* expected_target,const BYTE* catalogue){
    return lid_blueprint_frame_decode(counts,count,target,raw,size,expected_target,catalogue,LID_WIRE_TARGET);
}
/* Proof codecs do not manufacture authority: transaction must first prove its
   own successful save AND all-domain readback. TARGET can never decode as SAVED. */
static int lid_blueprint_saved_encode(BYTE* out,size_t size,const LidWireMessage* target,
                                      const unsigned* counts,size_t count,const BYTE* catalogue){
    LidWireMessage saved;
    if(!lid_wire_valid(target)||target->kind!=LID_WIRE_TARGET)return 0;
    saved=*target;saved.kind=LID_WIRE_SAVED;saved.status=1;
    return lid_blueprint_frame_encode(out,size,&saved,counts,count,catalogue,LID_WIRE_SAVED,1);
}
static int lid_blueprint_saved_decode(unsigned* counts,size_t count,LidWireMessage* saved,
                                      const BYTE* raw,size_t size,const BYTE* original_target,const BYTE* catalogue){
    LidWireMessage expected;BYTE frame[256];
    if(!original_target||!lid_wire_decode(&expected,original_target,256)||expected.kind!=LID_WIRE_TARGET)return 0;
    expected.kind=LID_WIRE_SAVED;expected.status=1;
    if(!lid_wire_encode(frame,256,&expected))return 0;
    return lid_blueprint_frame_decode(counts,count,saved,raw,size,frame,catalogue,LID_WIRE_SAVED);
}
#endif
