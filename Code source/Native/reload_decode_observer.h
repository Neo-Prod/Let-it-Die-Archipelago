#ifndef LID_RELOAD_DECODE_OBSERVER_R37_H
#define LID_RELOAD_DECODE_OBSERVER_R37_H
#include <stdint.h>
#include <stddef.h>
typedef struct R37Array{uintptr_t data;int num,capacity;}R37Array;
typedef struct R37Trace{
    unsigned active,owner;uintptr_t input;unsigned input_size;
    unsigned size_calls,input_shape,header_seen,header_suppressed,header_format,header_zero,header[6];
    unsigned decode_calls,decode_overflow,decode[8][6];
    unsigned parse_calls,parse_shape,parse_units,parse_capacity,parse_first,parse_last,parse_result;
}R37Trace;
typedef int(*R37Read)(void*,uintptr_t,void*,size_t);
static int r37_scope(const R37Trace*t,unsigned tid){return t&&t->active==1&&tid&&t->owner==tid;}
static void r37_sized(R37Trace*t,unsigned tid,R37Read read,void*c,uintptr_t array,int added,int result){
    R37Array a;
    if(!r37_scope(t,tid))return;t->size_calls++;
    if(!read||result!=0||added<24||added>12*1024*1024||!read(c,array,&a,sizeof(a))||!a.data||a.num!=added||a.capacity<a.num)return;
    t->input=a.data;t->input_size=(unsigned)added;t->input_shape=1;
}
static void r37_before_free(R37Trace*t,unsigned tid,R37Read read,void*c,uintptr_t pointer,size_t size){
    if(!r37_scope(t,tid)||size||!pointer||pointer!=t->input)return;
    /* Only the compressed branches retain the initial input allocation
       unchanged. NONE can remove its header/reallocate the input in place.
       A known local decompression call proves we took a compressed branch. */
    if(!t->decode_calls){t->header_suppressed=1;t->input=0;t->input_size=0;return;}
    unsigned words[6];
    if(t->input_size>=24&&read&&read(c,pointer,words,sizeof(words))){
        t->header_seen=1;t->header_zero=1;
        for(unsigned i=0;i<6;i++)if(words[i])t->header_zero=0;
        /* Never log six arbitrary words: an uncompressed reader can remove
           its header in place, exposing plaintext JSON at this address. */
        if(words[0]==0x00475242u&&(words[1]==1||words[1]==2)&&
           (words[3]==0x42494c5au||words[3]==0x454e4f4eu)){
            t->header_format=1;for(unsigned i=0;i<6;i++)t->header[i]=words[i];
        }
    }
    t->input=0;t->input_size=0; /* Never retain a borrow across native free. */
}
static void r37_decoded(R37Trace*t,unsigned tid,unsigned flags,unsigned out,unsigned in,unsigned sixth,unsigned site,uint64_t result){
    if(!r37_scope(t,tid))return;
    unsigned i=t->decode_calls++;if(i>=8){t->decode_overflow++;return;}
    t->decode[i][0]=site;t->decode[i][1]=flags;t->decode[i][2]=out;
    t->decode[i][3]=in;t->decode[i][4]=sixth;t->decode[i][5]=(unsigned)result;
}
static unsigned r37_char_kind(uint16_t ch){return ch==0?0:ch=='{'?1:ch=='}'?2:ch=='['?3:ch==']'?4:ch==0xfeff?5:(ch==' '||ch=='\t'||ch=='\r'||ch=='\n')?6:7;}
static void r37_parse_input(R37Trace*t,unsigned tid,R37Read read,void*c,uintptr_t string){
    R37Array s;uint16_t first,last,zero;
    if(!r37_scope(t,tid))return;t->parse_calls++;
    if(!read||!read(c,string,&s,sizeof(s))||!s.data||s.num<2||s.num>16*1024*1024||s.capacity<s.num||s.capacity>16*1024*1024||
       s.data>UINTPTR_MAX-(size_t)s.num*2||!read(c,s.data,&first,2)||
       !read(c,s.data+2*(size_t)(s.num-2),&last,2)||!read(c,s.data+2*(size_t)(s.num-1),&zero,2)||zero)return;
    t->parse_shape=1;t->parse_units=(unsigned)s.num;t->parse_capacity=(unsigned)s.capacity;
    t->parse_first=r37_char_kind(first);t->parse_last=r37_char_kind(last);
}
static void r37_end(R37Trace*t){if(t){t->input=0;t->input_size=0;t->active=0;}}
#endif
