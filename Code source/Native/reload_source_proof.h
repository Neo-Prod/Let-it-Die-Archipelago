#ifndef LID_RELOAD_SOURCE_PROOF_H
#define LID_RELOAD_SOURCE_PROOF_H
/* Include progression_wire.h first. Metadata only: no save contents or IO.
   A decoded proof alone MUST NOT authorize Load/Apply: the actual source must
   be opened without write/delete sharing, hashed and held until Load returns. */
#define LID_RELOAD_SOURCE_SIZE 640
#define LID_RELOAD_SOURCE_MAX (12u*1024u*1024u)
typedef struct LidReloadSourceProof {
    LidWireMessage session;
    unsigned file_size,uid_length;
    BYTE sha256[32];unsigned short uid[129];
} LidReloadSourceProof;
static int lid_reload_source_decode(LidReloadSourceProof*out,const BYTE*raw,size_t size,
                                    const LidWireMessage*quiesced){
    static const BYTE magic[8]={'L','I','D','A','P','R','S','1'};
    LidReloadSourceProof proof={0};BYTE expected[256];unsigned i,n;
    if(!out||!raw||size!=640||!quiesced||quiesced->kind!=LID_WIRE_QUIESCED||
       !lid_wire_encode(expected,256,quiesced)||!lid_wire_equal(raw,magic,8)||
       lid_wire_r32(raw+8)!=1||lid_wire_r32(raw+12)!=640||lid_wire_r32(raw+20)!=3||
       lid_wire_r32(raw+28)||!lid_wire_equal(expected,raw+32,256)||
       !lid_wire_decode(&proof.session,raw+32,256))return 0;
    proof.file_size=lid_wire_r32(raw+16);n=proof.uid_length=lid_wire_r32(raw+24);
    if(proof.file_size<24||proof.file_size>LID_RELOAD_SOURCE_MAX||!n||n>128||
       !lid_wire_nonzero(raw+288,32))return 0;
    lid_wire_copy(proof.sha256,raw+288,32);
    for(i=0;i<n;i++){
        unsigned c=(unsigned)raw[320+i*2]|((unsigned)raw[321+i*2]<<8);
        if(c<'0'||c>'9')return 0;proof.uid[i]=(unsigned short)c;
    }
    for(i=320+n*2;i<640;i++)if(raw[i])return 0;
    *out=proof;return 1;
}
#endif
