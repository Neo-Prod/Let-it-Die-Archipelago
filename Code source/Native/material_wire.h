#ifndef LID_MATERIAL_WIRE_R94_H
#define LID_MATERIAL_WIRE_R94_H
#include "death_wire.h"
#include "material_identity.h"
enum {R94_MATERIAL=6};
typedef struct R94MaterialFrame {
 R15Context context;uint64_t sequence,epoch;uint32_t slot;int32_t unique;
 uint32_t actor_name,actor_number,ordinal;int32_t gettime;uint32_t source_kind;
 char floor[16],area[32],rotation[16],entity_id[96],item_id[96];
} R94MaterialFrame;
static int r94_material_valid(const R94MaterialFrame*f){
 return f&&r15_context_valid(&f->context)&&f->sequence&&f->epoch&&f->slot<496&&f->unique>=0&&f->source_kind<=4&&
 f->ordinal>=1&&f->ordinal<=46&&r15_ascii(f->floor,16)&&r15_ascii(f->area,32)&&r15_ascii(f->rotation,16)&&
 r15_ascii(f->entity_id,96)&&r15_ascii(f->item_id,96)&&r22_map_floor(f->floor,f->area)&&
 r15_equal(f->rotation,"4HMA\0",5)&&r82_material_lookup(f->item_id)==f->ordinal;
}
static int r94_material_encode(uint8_t*out,size_t n,const R94MaterialFrame*f){
 if(!out||n!=R15_SIZE||!r94_material_valid(f))return 0;
 r15_zero(out,n);r15_copy(out,r15_magic,8);r15_w32(out+8,1);r15_w32(out+12,R94_MATERIAL);r15_w32(out+16,f->context.pid);
 r15_w64(out+24,f->context.birth);r15_copy(out+32,f->context.nonce,16);r15_copy(out+48,f->context.run,32);r15_copy(out+80,f->context.config,32);
 r15_w64(out+112,f->sequence);r15_w64(out+120,f->epoch);r15_copy(out+128,r15_build,32);
 r15_w32(out+160,f->slot);r15_w32(out+164,(uint32_t)f->unique);r15_w32(out+168,f->actor_name);r15_w32(out+172,f->actor_number);r15_w32(out+176,f->ordinal);
 r15_w32(out+184,(uint32_t)f->gettime);r15_w32(out+188,f->source_kind);r15_copy(out+192,f->floor,16);r15_copy(out+208,f->area,32);r15_copy(out+240,f->rotation,16);
 r15_copy(out+256,f->entity_id,96);r15_copy(out+352,f->item_id,96);r15_copy(out+852,f->context.writer,16);r15_w32(out+868,f->context.client_pid);
 r15_w32(out+848,r15_checksum(out));return 1;
}
static int r94_material_decode(R94MaterialFrame*out,const uint8_t*p,size_t n){
 R94MaterialFrame f;uint8_t canonical[R15_SIZE];
 if(!out||!p||n!=R15_SIZE||!r15_equal(p,r15_magic,8)||r15_r32(p+8)!=1||r15_r32(p+12)!=R94_MATERIAL||
 !r15_equal(p+128,r15_build,32)||r15_r32(p+848)!=r15_checksum(p))return 0;
 r15_zero(&f,sizeof(f));f.context.pid=r15_r32(p+16);f.context.birth=r15_r64(p+24);r15_copy(f.context.nonce,p+32,16);r15_copy(f.context.run,p+48,32);r15_copy(f.context.config,p+80,32);
 r15_copy(f.context.writer,p+852,16);f.context.client_pid=r15_r32(p+868);f.sequence=r15_r64(p+112);f.epoch=r15_r64(p+120);
 f.slot=r15_r32(p+160);f.unique=(int32_t)r15_r32(p+164);f.actor_name=r15_r32(p+168);f.actor_number=r15_r32(p+172);f.ordinal=r15_r32(p+176);f.gettime=(int32_t)r15_r32(p+184);f.source_kind=r15_r32(p+188);
 r15_copy(f.floor,p+192,16);r15_copy(f.area,p+208,32);r15_copy(f.rotation,p+240,16);r15_copy(f.entity_id,p+256,96);r15_copy(f.item_id,p+352,96);
 if(!r94_material_encode(canonical,sizeof(canonical),&f)||!r15_equal(canonical,p,n))return 0;*out=f;return 1;
}
/* Catalogue version is negotiated separately from wire protocol version1.
   Legacy codec remains byte-for-byte unchanged and still rejects kind6. */
static int r94_frame_valid_for_catalogue(const uint8_t*p,size_t n,unsigned catalogue){
 R94MaterialFrame material;R15Frame old;
 if(!p||n!=R15_SIZE||catalogue<1||catalogue>5)return 0;
 if(r15_r32(p+12)==R94_MATERIAL)return catalogue==5&&r94_material_decode(&material,p,n);
 return r15_decode(&old,p,n);
}
#endif
