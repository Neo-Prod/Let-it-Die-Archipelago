#ifndef LID_KILL_COIN_WIRE_R138_H
#define LID_KILL_COIN_WIRE_R138_H
#include "death_wire.h"
enum {R138_SIZE=256,R138_GRANT=1,R138_ACK=2,R138_APPLIED=1,R138_REPLAY=2};
typedef struct R138Frame {R15Context context;uint32_t index,item,amount,kind,status,before,after;} R138Frame;
static uint32_t r138_amount(uint32_t item){return item==7130002?2000:item==7130003?10000:item==7130004?50000:0;}
static uint32_t r138_checksum(const uint8_t*p){uint32_t v=2166136261u;for(unsigned i=0;i<252;i++)v=(v^p[i])*16777619u;return v;}
static int r138_valid(const R138Frame*f){
 if(!f||!r15_context_valid(&f->context)||f->index>=100000||!r138_amount(f->item)||f->amount!=r138_amount(f->item)||f->before>2147483647u||f->after>2147483647u)return 0;
 if(f->kind==R138_GRANT)return !f->status&&!f->before&&!f->after;
 return f->kind==R138_ACK&&(f->status==R138_APPLIED||f->status==R138_REPLAY)&&f->after>=f->before&&f->after-f->before==f->amount;
}
static int r138_encode(uint8_t*out,size_t n,const R138Frame*f){
 static const uint8_t magic[8]={'L','I','D','K','C','N','0','1'};
 if(!out||n!=R138_SIZE||!r138_valid(f))return 0;r15_zero(out,n);r15_copy(out,magic,8);
 r15_w32(out+8,1);r15_w32(out+12,f->kind);r15_w32(out+16,f->context.pid);r15_w32(out+20,f->context.client_pid);r15_w64(out+24,f->context.birth);
 r15_copy(out+32,f->context.nonce,16);r15_copy(out+48,f->context.run,32);r15_copy(out+80,f->context.config,32);r15_copy(out+112,f->context.writer,16);
 r15_w32(out+128,f->index);r15_w32(out+132,f->item);r15_w32(out+136,f->amount);r15_w32(out+140,f->status);r15_w32(out+144,f->before);r15_w32(out+148,f->after);
 r15_copy(out+192,r15_build,32);r15_w32(out+252,r138_checksum(out));return 1;
}
static int r138_decode(R138Frame*out,const uint8_t*p,size_t n){
 R138Frame f={0};uint8_t canonical[256];if(!out||!p||n!=256)return 0;
 f.kind=r15_r32(p+12);f.context.pid=r15_r32(p+16);f.context.client_pid=r15_r32(p+20);f.context.birth=r15_r64(p+24);
 r15_copy(f.context.nonce,p+32,16);r15_copy(f.context.run,p+48,32);r15_copy(f.context.config,p+80,32);r15_copy(f.context.writer,p+112,16);
 f.index=r15_r32(p+128);f.item=r15_r32(p+132);f.amount=r15_r32(p+136);f.status=r15_r32(p+140);f.before=r15_r32(p+144);f.after=r15_r32(p+148);
 if(!r138_encode(canonical,sizeof(canonical),&f)||!r15_equal(canonical,p,n))return 0;*out=f;return 1;
}
#endif
