#ifndef LID_SHOP_HISTORY_CODEC_H
#define LID_SHOP_HISTORY_CODEC_H
/* Content-only UE3 shop history codec. No allocation, mutation, or game calls.
   Include <stddef.h> before this header. x64 little endian source layout only.
   reader(ctx, source, destination, size) must copy or refuse without faults.
   Source strings remain owned by the caller, live throughout row encoding.
   Not integrated into mod.c yet. */
typedef int (*LidShopHistoryRead)(void*,const void*,void*,unsigned);
typedef struct LidShopHistoryKey {
    unsigned char* bytes;
    unsigned capacity,used,expected,rows,failed;
} LidShopHistoryKey;
static unsigned lid_shop_history_u32(const unsigned char* p){
    return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);
}
static unsigned long long lid_shop_history_u64(const unsigned char* p){
    return (unsigned long long)lid_shop_history_u32(p)|((unsigned long long)lid_shop_history_u32(p+4)<<32);
}
static int lid_shop_history_write(LidShopHistoryKey* k,const unsigned char* p,unsigned n){
    unsigned i;
    if(!k||k->failed)return 0;
    if(!p||k->used>k->capacity||n>k->capacity-k->used){k->failed=1;return 0;}
    for(i=0;i<n;i++)k->bytes[k->used+i]=p[i];
    k->used+=n;return 1;
}
static int lid_shop_history_write32(LidShopHistoryKey* k,unsigned n){
    unsigned char b[4];unsigned i;
    for(i=0;i<4;i++)b[i]=(unsigned char)(n>>(8*i));
    return lid_shop_history_write(k,b,4);
}
static int lid_shop_history_begin(LidShopHistoryKey* k,void* out,unsigned capacity,unsigned rows){
    static const unsigned char magic[4]={'L','S','H','1'};
    if(!k)return 0;
    k->bytes=out;k->capacity=capacity;k->used=0;k->rows=0;k->expected=rows;
    k->failed=!out||rows==0||rows>256||capacity>1048576;
    return !k->failed&&lid_shop_history_write(k,magic,4)&&lid_shop_history_write32(k,rows);
}
/* Read FString descriptor copied from the record; source bytes need not align.
   Empty Num0 ignores unused capacity/pointer but still validates counters. */
static int lid_shop_history_string(const unsigned char* descriptor,LidShopHistoryRead read,
                                   void* ctx,unsigned char* text,unsigned* units){
    unsigned n=lid_shop_history_u32(descriptor+8),cap=lid_shop_history_u32(descriptor+12),i,u,v;
    unsigned long long address=lid_shop_history_u64(descriptor);
    *units=0;
    if(n>257||cap<n||cap>4096)return 0;
    if(!n)return 1;
    if(!address||address>~0ULL-(unsigned long long)n*2||
       !read(ctx,(const void*)(size_t)address,text,n*2))return 0;
    if(text[(n-1)*2]||text[(n-1)*2+1])return 0;
    for(i=0;i+1<n;i++){
        u=(unsigned)text[2*i]|((unsigned)text[2*i+1]<<8);
        if(!u)return 0;
        if(u>=0xd800&&u<=0xdbff){
            if(i+2>=n)return 0;
            i++;v=(unsigned)text[2*i]|((unsigned)text[2*i+1]<<8);
            if(v<0xdc00||v>0xdfff)return 0;
        }else if(u>=0xdc00&&u<=0xdfff)return 0;
    }
    *units=n-1;return 1;
}
static int lid_shop_history_append(LidShopHistoryKey* k,const void* record,
                                   LidShopHistoryRead read,void* ctx){
    static const unsigned offsets[8]={0,0x10,0x20,0x30,0x44,0x60,0x70,0x80};
    static const unsigned integers[3]={0x40,0x54,0x90};
    unsigned char row[0x94],text[514];unsigned i,units;
    if(!k||k->failed)return 0;
    if(!record||!read||k->rows>=k->expected||!read(ctx,record,row,sizeof(row))){k->failed=1;return 0;}
    for(i=0;i<8;i++){
        if(!lid_shop_history_string(row+offsets[i],read,ctx,text,&units)||
           !lid_shop_history_write32(k,units)||!lid_shop_history_write(k,text,units*2)){
            k->failed=1;return 0;
        }
    }
    for(i=0;i<3;i++)if(!lid_shop_history_write(k,row+integers[i],4))return 0;
    if(!lid_shop_history_write(k,row+0x58,8))return 0;
    k->rows++;return 1;
}
static int lid_shop_history_finish(LidShopHistoryKey* k,unsigned* length){
    if(!length)return 0;
    *length=0;
    if(!k||k->failed||k->rows!=k->expected)return 0;
    *length=k->used;return 1;
}
#endif
