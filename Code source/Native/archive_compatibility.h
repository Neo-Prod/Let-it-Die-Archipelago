#ifndef LID_ARCHIVE_COMPATIBILITY_R310_H
#define LID_ARCHIVE_COMPATIBILITY_R310_H
/* Durable-file boundary ONLY. Never use for live grants/handshakes/ACKs.
   Verify original integrity before interpreting the two proven compatible
   builds. Normalize a private byte buffer; never rewrite archived evidence. */
static unsigned lid_archive_sum(const unsigned char*p,size_t n,size_t checksum){
    unsigned v=2166136261u;for(size_t i=0;i<n;i++)if(i<checksum||i>=checksum+4)v=(v^p[i])*16777619u;return v;
}
static int lid_archive_rebind(unsigned char*out,const unsigned char*raw,size_t n,size_t build,size_t checksum){
    static const unsigned char old_build[32]={0xf3,0x3e,0x91,0xe6,0xe8,0xd5,0x85,0x66,0xbf,0xab,0xad,0x8d,0x01,0xcd,0x04,0x86,0x58,0x6a,0x0d,0x43,0xfe,0x1c,0x58,0xe6,0x5f,0xc6,0x48,0x5a,0x41,0x7b,0xdd,0xfd};
    static const unsigned char current_build[32]={0x6c,0xfa,0xdb,0xba,0x9d,0xa3,0xe2,0xf3,0xb3,0xa4,0x46,0x31,0x94,0xd2,0x47,0xa5,0x0d,0x25,0x37,0xa3,0xad,0x70,0x41,0x74,0x73,0x0a,0x0b,0xbf,0x89,0x9e,0x42,0xca};
    if(!out||!raw||(n!=256&&n!=1024)||build>n-32||checksum>n-4)return 0;
    unsigned same_old=1,same_current=1;
    for(unsigned i=0;i<32;i++){if(raw[build+i]!=old_build[i])same_old=0;if(raw[build+i]!=current_build[i])same_current=0;}
    unsigned sum=(unsigned)raw[checksum]|((unsigned)raw[checksum+1]<<8)|((unsigned)raw[checksum+2]<<16)|((unsigned)raw[checksum+3]<<24);
    if((!same_old&&!same_current)||sum!=lid_archive_sum(raw,n,checksum))return 0;
    for(size_t i=0;i<n;i++)out[i]=raw[i];for(unsigned i=0;i<32;i++)out[build+i]=current_build[i];
    sum=lid_archive_sum(out,n,checksum);for(unsigned i=0;i<4;i++)out[checksum+i]=(unsigned char)(sum>>(8*i));return 1;
}
#endif
