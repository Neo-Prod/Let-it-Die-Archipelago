#ifndef LID_PROGRESSION_WIRE_H
#define LID_PROGRESSION_WIRE_H
/* Fixed256-byte little-endian character bridge. Include winlite.h first.
   No I/O, RNG, hooks or gameplay calls. Host must own the shared runtime mutex
   before grant and must prove its own successful save/readback before SAVED. */
#define LID_WIRE_SIZE 256
#define LID_WIRE_OFFER 1
#define LID_WIRE_QUIESCED 2
#define LID_WIRE_TARGET 3
#define LID_WIRE_SAVED 4
#define LID_WIRE_GRANT 5
typedef struct LidWireMessage {
    unsigned kind,pid,writer_pid;
    BYTE nonce[16],writer[16],identity[32],config[32],generation[32];
    unsigned history_length,counts[9],status;
    unsigned long long sequence,creation_time;
} LidWireMessage;
typedef struct LidWirePeer {
    LidWireMessage offer,binding,last_target;
    int bound,granted;
} LidWirePeer;
static const BYTE lid_wire_magic[8]={'L','I','D','A','P','C','0','1'};
static const BYTE lid_wire_game[32]={0xf3,0x3e,0x91,0xe6,0xe8,0xd5,0x85,0x66,0xbf,0xab,0xad,0x8d,0x01,0xcd,0x04,0x86,0x58,0x6a,0x0d,0x43,0xfe,0x1c,0x58,0xe6,0x5f,0xc6,0x48,0x5a,0x41,0x7b,0xdd,0xfd};
static void lid_wire_zero(void* p,size_t n){BYTE* b=p;while(n--)*b++=0;}
static void lid_wire_copy(void* d,const void* s,size_t n){BYTE* a=d;const BYTE* b=s;while(n--)*a++=*b++;}
static int lid_wire_equal(const void* a,const void* b,size_t n){const BYTE* x=a;const BYTE* y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static int lid_wire_nonzero(const BYTE* a,size_t n){while(n--)if(*a++)return 1;return 0;}
static unsigned lid_wire_r32(const BYTE* p){return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static unsigned long long lid_wire_r64(const BYTE* p){return lid_wire_r32(p)|((unsigned long long)lid_wire_r32(p+4)<<32);}
static void lid_wire_w32(BYTE* p,unsigned v){int i;for(i=0;i<4;i++)p[i]=(BYTE)(v>>(i*8));}
static void lid_wire_w64(BYTE* p,unsigned long long v){lid_wire_w32(p,(unsigned)v);lid_wire_w32(p+4,(unsigned)(v>>32));}
static unsigned lid_wire_checksum(const BYTE* p){
    unsigned v=2166136261u;int i;for(i=0;i<256;i++)if(i<224||i>=228)v=(v^p[i])*16777619u;return v;
}
static int lid_wire_valid(const LidWireMessage* m){
    unsigned long long sum=0;int i;
    if(!m||m->kind<1||m->kind>5||!m->pid||!m->creation_time||!lid_wire_nonzero(m->nonce,16))return 0;
    if(m->history_length>100000)return 0;
    for(i=0;i<9;i++){if(m->counts[i]>100000)return 0;sum+=m->counts[i];}
    if(sum>m->history_length||m->status!=(m->kind==LID_WIRE_SAVED?1u:0u))return 0;
    if(m->kind==LID_WIRE_OFFER){
        if(m->writer_pid||lid_wire_nonzero(m->writer,16)||lid_wire_nonzero(m->identity,32)||lid_wire_nonzero(m->config,32))return 0;
    }else if(!m->writer_pid||!lid_wire_nonzero(m->writer,16)||!lid_wire_nonzero(m->identity,32)||!lid_wire_nonzero(m->config,32))return 0;
    if(m->kind==LID_WIRE_OFFER||m->kind==LID_WIRE_QUIESCED||m->kind==LID_WIRE_GRANT){
        if(m->history_length||sum||m->sequence||lid_wire_nonzero(m->generation,32))return 0;
    }else if(!m->sequence||!lid_wire_nonzero(m->generation,32))return 0;
    return 1;
}
static int lid_wire_encode(BYTE* out,size_t n,const LidWireMessage* m){
    int i;if(!out||n!=256||!lid_wire_valid(m))return 0;
    lid_wire_zero(out,n);lid_wire_copy(out,lid_wire_magic,8);
    lid_wire_w32(out+8,1);lid_wire_w32(out+12,m->kind);lid_wire_w32(out+16,m->pid);lid_wire_w32(out+20,m->writer_pid);
    lid_wire_copy(out+24,m->nonce,16);lid_wire_copy(out+40,m->identity,32);lid_wire_copy(out+72,m->config,32);
    lid_wire_w32(out+104,m->history_length);for(i=0;i<9;i++)lid_wire_w32(out+108+i*4,m->counts[i]);
    lid_wire_copy(out+144,m->generation,32);lid_wire_w64(out+176,m->sequence);lid_wire_w32(out+184,m->status);
    lid_wire_copy(out+192,lid_wire_game,32);lid_wire_copy(out+228,m->writer,16);lid_wire_w64(out+244,m->creation_time);
    lid_wire_w32(out+224,lid_wire_checksum(out));return 1;
}
static int lid_wire_decode(LidWireMessage* out,const BYTE* raw,size_t n){
    LidWireMessage m;int i;
    if(!out||!raw||n!=256||!lid_wire_equal(raw,lid_wire_magic,8)||lid_wire_r32(raw+8)!=1||
       lid_wire_r32(raw+188)||lid_wire_r32(raw+252)||!lid_wire_equal(raw+192,lid_wire_game,32)||
       lid_wire_r32(raw+224)!=lid_wire_checksum(raw))return 0;
    lid_wire_zero(&m,sizeof(m));m.kind=lid_wire_r32(raw+12);m.pid=lid_wire_r32(raw+16);m.writer_pid=lid_wire_r32(raw+20);
    lid_wire_copy(m.nonce,raw+24,16);lid_wire_copy(m.identity,raw+40,32);lid_wire_copy(m.config,raw+72,32);
    m.history_length=lid_wire_r32(raw+104);for(i=0;i<9;i++)m.counts[i]=lid_wire_r32(raw+108+i*4);
    lid_wire_copy(m.generation,raw+144,32);m.sequence=lid_wire_r64(raw+176);m.status=lid_wire_r32(raw+184);
    lid_wire_copy(m.writer,raw+228,16);m.creation_time=lid_wire_r64(raw+244);
    if(!lid_wire_valid(&m))return 0;*out=m;return 1;
}
static int lid_wire_peer_init(LidWirePeer* p,unsigned pid,unsigned long long creation,const BYTE* nonce){
    LidWireMessage offer;if(!p||!nonce)return 0;
    lid_wire_zero(&offer,sizeof(offer));offer.kind=LID_WIRE_OFFER;offer.pid=pid;offer.creation_time=creation;
    lid_wire_copy(offer.nonce,nonce,16);if(!lid_wire_valid(&offer))return 0;
    lid_wire_zero(p,sizeof(*p));p->offer=offer;return 1;
}
static int lid_wire_same_session(const LidWireMessage* a,const LidWireMessage* b){
    return a->pid==b->pid&&a->creation_time==b->creation_time&&lid_wire_equal(a->nonce,b->nonce,16);
}
static int lid_wire_same_writer(const LidWireMessage* a,const LidWireMessage* b){
    return lid_wire_same_session(a,b)&&a->writer_pid==b->writer_pid&&lid_wire_equal(a->writer,b->writer,16)&&
           lid_wire_equal(a->identity,b->identity,32)&&lid_wire_equal(a->config,b->config,32);
}
static int lid_wire_peer_bind(LidWirePeer* p,const LidWireMessage* stopped){
    if(!p||!lid_wire_valid(stopped)||stopped->kind!=LID_WIRE_QUIESCED||!lid_wire_same_session(&p->offer,stopped))return 0;
    if(p->bound)return lid_wire_same_writer(&p->binding,stopped);
    p->binding=*stopped;p->bound=1;return 1;
}
static int lid_wire_peer_grant(LidWirePeer* p,int runtime_mutex_owned,BYTE* frame){
    LidWireMessage grant;
    if(!p||!p->bound||runtime_mutex_owned!=1)return 0;
    grant=p->binding;grant.kind=LID_WIRE_GRANT;
    if(!lid_wire_encode(frame,256,&grant))return 0;p->granted=1;return 1;
}
/* 1=new target,2=identical replay,0=refused. Caller freezes a copy for its job. */
static int lid_wire_peer_target(LidWirePeer* p,const LidWireMessage* m){
    int i;
    if(!p||!p->granted||!lid_wire_valid(m)||m->kind!=LID_WIRE_TARGET||!lid_wire_same_writer(&p->binding,m))return 0;
    if(m->sequence<p->last_target.sequence)return 0;
    if(m->sequence==p->last_target.sequence){
        if(m->history_length!=p->last_target.history_length||!lid_wire_equal(m->generation,p->last_target.generation,32))return 0;
        for(i=0;i<9;i++)if(m->counts[i]!=p->last_target.counts[i])return 0;
        return 2;
    }
    if(m->history_length<p->last_target.history_length)return 0;
    for(i=0;i<9;i++)if(m->counts[i]<p->last_target.counts[i])return 0;
    if(p->last_target.sequence&&m->history_length==p->last_target.history_length&&
       !lid_wire_equal(m->generation,p->last_target.generation,32))return 0;
    p->last_target=*m;return 1;
}
static int lid_wire_peer_saved(LidWirePeer* p,const LidWireMessage* frozen,int own_save_and_readback_ok,BYTE* frame){
    LidWireMessage saved;int i;
    if(!p||!p->granted||own_save_and_readback_ok!=1||!lid_wire_valid(frozen)||frozen->kind!=LID_WIRE_TARGET||
       !lid_wire_same_writer(&p->binding,frozen)||frozen->sequence>p->last_target.sequence)return 0;
    if(frozen->history_length>p->last_target.history_length)return 0;
    for(i=0;i<9;i++)if(frozen->counts[i]>p->last_target.counts[i])return 0;
    if(frozen->sequence==p->last_target.sequence&&
       (frozen->history_length!=p->last_target.history_length||!lid_wire_equal(frozen->generation,p->last_target.generation,32)))return 0;
    if(frozen->sequence==p->last_target.sequence)for(i=0;i<9;i++)if(frozen->counts[i]!=p->last_target.counts[i])return 0;
    saved=*frozen;saved.kind=LID_WIRE_SAVED;saved.status=1;
    return lid_wire_encode(frame,256,&saved);
}
#endif
