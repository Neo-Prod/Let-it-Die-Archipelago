#ifndef LID_NOTIFICATION_WIRE_R154_H
#define LID_NOTIFICATION_WIRE_R154_H
#include "death_wire.h"
/* Independent presentation packet. Not included by the active runtime yet. */
enum {R154_SIZE=8448,R154_HEADER=256,R154_MAX_UNITS=4095,R154_REQUEST=1,R154_ACK=2};
typedef struct R154Message {R15Context context;uint64_t sequence;uint32_t kind,units;uint16_t text[4096];} R154Message;
static uint32_t r154_checksum(const uint8_t*p){
 uint32_t h=2166136261u;for(unsigned i=0;i<R154_SIZE;i++)if(i<176||i>=180)h=(h^p[i])*16777619u;return h;
}
static int r154_valid(const R154Message*m){
 if(!m||!r15_context_valid(&m->context)||!m->sequence||m->kind<R154_REQUEST||m->kind>R154_ACK||m->units>R154_MAX_UNITS||
 (m->kind==R154_REQUEST&&!m->units)||(m->kind==R154_ACK&&m->units))return 0;
 for(unsigned i=0;i<m->units;i++){
  unsigned c=m->text[i];if(!c)return 0;
  if(c>=0xd800&&c<=0xdbff){if(++i>=m->units||m->text[i]<0xdc00||m->text[i]>0xdfff)return 0;}
  else if(c>=0xdc00&&c<=0xdfff)return 0;
 }
 for(unsigned i=m->units;i<4096;i++)if(m->text[i])return 0;
 return 1;
}
static int r154_encode(const R154Message*m,uint8_t*p){
 if(!p||!r154_valid(m))return 0;const R15Context*c=&m->context;r15_zero(p,R154_SIZE);
 r15_copy(p,"LIDNOTE1",8);r15_w32(p+8,1);r15_w32(p+12,m->kind);r15_w32(p+16,c->pid);r15_w32(p+20,c->client_pid);r15_w64(p+24,c->birth);
 r15_copy(p+32,c->nonce,16);r15_copy(p+48,c->writer,16);r15_copy(p+64,c->run,32);r15_copy(p+96,c->config,32);
 r15_w64(p+128,m->sequence);r15_w32(p+136,m->units);r15_copy(p+144,r15_build,32);
 for(unsigned i=0;i<m->units;i++){p[256+i*2]=(uint8_t)m->text[i];p[257+i*2]=(uint8_t)(m->text[i]>>8);}
 r15_w32(p+176,r154_checksum(p));return 1;
}
/* Output may be scratch on failure; caller publishes only after success. */
static int r154_decode(const uint8_t*p,size_t size,R154Message*m){
 if(!p||!m||size!=R154_SIZE||!r15_equal(p,"LIDNOTE1",8)||r15_r32(p+8)!=1||!r15_equal(p+144,r15_build,32)||r15_r32(p+176)!=r154_checksum(p))return 0;
 for(unsigned i=140;i<144;i++)if(p[i])return 0;for(unsigned i=180;i<256;i++)if(p[i])return 0;
 r15_zero(m,sizeof(*m));R15Context*c=&m->context;c->pid=r15_r32(p+16);c->client_pid=r15_r32(p+20);c->birth=r15_r64(p+24);
 r15_copy(c->nonce,p+32,16);r15_copy(c->writer,p+48,16);r15_copy(c->run,p+64,32);r15_copy(c->config,p+96,32);
 m->sequence=r15_r64(p+128);m->kind=r15_r32(p+12);m->units=r15_r32(p+136);
 for(unsigned i=0;i<4096;i++)m->text[i]=(uint16_t)(p[256+i*2]|((unsigned)p[257+i*2]<<8));
 return r154_valid(m);
}
#endif
