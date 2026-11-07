#ifndef LID_RELOAD_VERIFIED_CHUNKS_R41_H
#define LID_RELOAD_VERIFIED_CHUNKS_R41_H
#include <stdint.h>
#include <stddef.h>
/* Borrow snapshot only for this call. No retained input/source pointer. */
typedef struct R41Chunks {
    size_t offset,produced;uintptr_t output_base;
    unsigned bound,declared,calls,matches,mismatches,unreadable,refused,reason,native_failures,complete;
} R41Chunks;
typedef int(*R41Same)(void*,uintptr_t,const unsigned char*,size_t);
typedef int(*R41Read)(void*,uintptr_t,void*,size_t);
/* Diagnostic comparison must not dereference a possibly stale native buffer.
   Host copies through an OS-protected own-process read into this bounded scratch. */
static int r41_compare_input(R41Read read,void*context,uintptr_t input,
    const unsigned char*verified,size_t size){
    unsigned char scratch[256];size_t at=0;
    if(!read||!input||!verified||!size||input>UINTPTR_MAX-size)return -1;
    while(at<size){
        size_t n=size-at;if(n>sizeof(scratch))n=sizeof(scratch);
        if(read(context,input+at,scratch,n)!=1)return -1;
        for(size_t i=0;i<n;i++)if(scratch[i]!=verified[at+i])return 0;
        at+=n;
    }
    return 1;
}
static unsigned r41_u32(const unsigned char*p){return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static int r41_no(R41Chunks*s,unsigned reason){s->refused++;s->reason=reason;return 0;}
static int r41_chunk_input(R41Chunks*s,const unsigned char*source,size_t size,
    uintptr_t native_base,size_t native_size,uintptr_t input,uintptr_t output,
    unsigned unpacked,unsigned packed,R41Same same,void*context,const void**selected){
    if(!s||!selected)return 0;*selected=0;
    if(!source||size<28||size>12u*1024u*1024u||native_size!=size||!native_base||
       native_base>UINTPTR_MAX-size||(uintptr_t)source>UINTPTR_MAX-size||
       !output||!unpacked||!packed||!same)return r41_no(s,1);
    if(r41_u32(source)!=0x00475242u||r41_u32(source+4)!=2||r41_u32(source+12)!=0x42494c5au||
       !r41_u32(source+8)||r41_u32(source+8)>32u*1024u*1024u)return r41_no(s,2);
    if(!s->bound){s->declared=r41_u32(source+8);s->offset=16;s->output_base=output;s->bound=1;}
    if(s->complete||s->calls>=256||s->declared!=r41_u32(source+8)||s->offset>size-8)return r41_no(s,3);
    if(unpacked!=r41_u32(source+s->offset)||packed!=r41_u32(source+s->offset+4)||
       packed>size-s->offset-8||input!=native_base+s->offset+8)return r41_no(s,4);
    if(s->produced>s->declared||unpacked>s->declared-s->produced||
       s->output_base>UINTPTR_MAX-s->declared||output!=s->output_base+s->produced)return r41_no(s,5);
    uintptr_t lo=(uintptr_t)source,hi=lo+size;
    if(output<hi&&output+(size_t)unpacked>lo)return r41_no(s,6);
    size_t next=s->offset+8+packed;unsigned final=s->produced+unpacked==s->declared;
    /* Our AP codec ends exactly at the last stream. Native saves may append
       four zero bytes. Accept only these two bounded, known representations. */
    if(final&&next!=size&&(size-next!=4||r41_u32(source+next)!=0))return r41_no(s,7);
    int equal=same(context,input,source+s->offset+8,packed);
    if(equal==1)s->matches++;else if(equal==0)s->mismatches++;else s->unreadable++;
    *selected=source+s->offset+8;s->offset=next;s->produced+=unpacked;s->calls++;s->complete=final;return 1;
}
#endif
