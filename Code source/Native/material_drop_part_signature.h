#ifndef LID_MATERIAL_DROP_PART_SIGNATURE_R111_H
#define LID_MATERIAL_DROP_PART_SIGNATURE_R111_H
#include "material_drop_part_layout.h"
#include "material_read_scope.h"
/* Static provenance: ProcessEvent core VA14010F0C0 reads UProperty.Flags70,
   ParmSize E2, ReturnOffset E4 and UFunction.Flags D0. No guessed NumParms. */
typedef struct R111DropSignature {R110DropLayout layout;uint16_t parms_size,return_offset;uint32_t function_flags;} R111DropSignature;
static int r111_drop_signature_core(const R6Ops*o,uint64_t player,R111DropSignature*out){
 R111DropSignature s={0};uint64_t child,next,seen[512];unsigned n=0,count=0,found=0;
 if(!out||!r110_drop_layout(o,player,&s.layout)||
 !r6_at(o,s.layout.function,0xd0,&s.function_flags,4)||!(s.function_flags&2)||(s.function_flags&0x400)||
 !r6_at(o,s.layout.function,0xe2,&s.parms_size,2)||s.parms_size<s.layout.extent||s.parms_size>128||
 !r6_at(o,s.layout.function,0xe4,&s.return_offset,2)||s.return_offset!=s.layout.offset[R110_RETURN]||
 !r6_at(o,s.layout.function,0x80,&child,8))return 0;
 while(child){
  uint64_t flags;if(n==512)return 0;for(unsigned i=0;i<n;i++)if(seen[i]==child)return 0;seen[n++]=child;
  if(!r6_at(o,child,0x60,&next,8)||!r6_at(o,child,0x70,&flags,8))return 0;
  if(flags&0x80){
   unsigned i;for(i=0;i<8;i++)if(r6_name(o,child,r110_names[i]))break;
   if(i==8||(found&(1u<<i))||((flags&0x400)!=0)!=(i==R110_RETURN)||(i!=R110_RETURN&&(flags&0x100)))return 0;
   found|=1u<<i;count++;
  }else{
   for(unsigned i=0;i<8;i++)if(r6_name(o,child,r110_names[i]))return 0;
  }
  child=next;
 }
 if(count!=8||found!=255)return 0;*out=s;return 1;
}
static int r111_drop_signature(const R6Ops*o,uint64_t player,R111DropSignature*out){
 int scope=R152_READ_BEGIN();int result=r111_drop_signature_core(o,player,out);
 R152_READ_END(scope);return result;
}
#endif
