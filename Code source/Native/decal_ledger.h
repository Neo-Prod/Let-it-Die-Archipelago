#ifndef LID_DECAL_LEDGER_H
#define LID_DECAL_LEDGER_H
/* Research pure parser/planner. No I/O, game calls, mutation or save authority.
   Include progression_wire.h, blueprint_wire.h, decal_catalog.h first. */
#define LID_DECAL_FRAME_SIZE 1152u
typedef struct LidDecalLedger {
    LidWireMessage committed_target;unsigned counts[208];
    BYTE identity[32],config[32];
    unsigned loaded,have_ledger,cleanup_pending;
} LidDecalLedger;
static int lid_decal_target_decode(unsigned*counts,LidWireMessage*target,const BYTE*raw,size_t n,const BYTE*expected){
    return raw&&n==1152&&lid_wire_r32(raw+8)==1&&lid_blueprint_decode(counts,208,target,raw,n,expected,lid_decal_catalogue_hash);
}
static int lid_decal_saved_decode(unsigned*counts,LidWireMessage*saved,const BYTE*raw,size_t n,const BYTE*original){
    return raw&&n==1152&&lid_wire_r32(raw+8)==1&&lid_blueprint_saved_decode(counts,208,saved,raw,n,original,lid_decal_catalogue_hash);
}
/* Return1 clean/committed pending can be removed, -1 invalid, -2 uncertain.
   On rejection caller-owned output is unchanged. NEVER resolve from stock. */
static int lid_decal_ledger_parse(LidDecalLedger*out,const BYTE*ledger,size_t ledger_n,const BYTE*pending,size_t pending_n,
                                  const BYTE*identity,const BYTE*config){
    LidWireMessage committed={0},parsed,attempt;BYTE expected[256],pending_target[256];unsigned counts[208]={0},pending_counts[208];size_t i;
    if(!out||out->loaded||!identity||!config||!lid_wire_nonzero(identity,32)||!lid_wire_nonzero(config,32)||
       (ledger_n&&(!ledger||ledger_n!=1152))||(pending_n&&(!pending||pending_n!=1152)))return -1;
    if(ledger_n){
        if(!lid_wire_decode(&parsed,ledger+64,256)||parsed.kind!=LID_WIRE_SAVED||parsed.status!=1||
           !lid_wire_equal(parsed.identity,identity,32)||!lid_wire_equal(parsed.config,config,32))return -1;
        committed=parsed;committed.kind=LID_WIRE_TARGET;committed.status=0;
        if(!lid_wire_encode(expected,256,&committed)||!lid_decal_saved_decode(counts,&parsed,ledger,ledger_n,expected))return -1;
    }
    if(pending_n){
        if(!lid_wire_decode(&attempt,pending+64,256)||attempt.kind!=LID_WIRE_TARGET||
           !lid_wire_equal(attempt.identity,identity,32)||!lid_wire_equal(attempt.config,config,32)||
           !lid_wire_encode(pending_target,256,&attempt)||!lid_decal_target_decode(pending_counts,&parsed,pending,pending_n,pending_target))return -1;
        if(!ledger_n||!lid_wire_equal(expected,pending_target,256))return -2;
        for(i=0;i<208;i++)if(counts[i]!=pending_counts[i])return -2;
    }
    out->committed_target=committed;for(i=0;i<208;i++)out->counts[i]=counts[i];
    lid_wire_copy(out->identity,identity,32);lid_wire_copy(out->config,config,32);
    out->have_ledger=!!ledger_n;out->cleanup_pending=!!pending_n;out->loaded=1;return 1;
}
static int lid_decal_ledger_delta(const LidDecalLedger*ledger,const LidWireMessage*target,const unsigned*counts,unsigned*out){
    unsigned i;unsigned long long sum=0;BYTE frame[256];
    if(!ledger||ledger->loaded!=1||!target||!counts||!out||target->kind!=LID_WIRE_TARGET||!lid_wire_encode(frame,256,target))return 0;
    if(!lid_wire_equal(target->identity,ledger->identity,32)||!lid_wire_equal(target->config,ledger->config,32))return 0;
    if(ledger->have_ledger){
        if(!lid_wire_equal(target->identity,ledger->committed_target.identity,32)||!lid_wire_equal(target->config,ledger->committed_target.config,32)||
           target->history_length<ledger->committed_target.history_length||
           (target->history_length==ledger->committed_target.history_length&&!lid_wire_equal(target->generation,ledger->committed_target.generation,32)))return 0;
    }
    for(i=0;i<208;i++){if(counts[i]>100000||counts[i]<ledger->counts[i])return 0;sum+=counts[i]-ledger->counts[i];}
    for(i=0;i<9;i++){
        unsigned old=ledger->have_ledger?ledger->committed_target.counts[i]:0;
        if(target->counts[i]<old)return 0;sum+=target->counts[i]-old;
    }
    if(sum>target->history_length-(ledger->have_ledger?ledger->committed_target.history_length:0))return 0;
    for(i=0;i<208;i++)out[i]=counts[i]-ledger->counts[i];return 1;
}
#endif
