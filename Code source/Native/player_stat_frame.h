#ifndef LID_PLAYER_STAT_FRAME_H
#define LID_PLAYER_STAT_FRAME_H
#include "player_stat_identity.h"
enum {LID_PSF_NATIVE=0,LID_PSF_OWNED=1,LID_PSF_ENEMY=2,LID_PSF_INHERIT=3,LID_PSF_INVALID=-1};
typedef struct LidPSFrameOps {
    LidPSSource source;void *context;
    int (*is_a)(void*,uintptr_t,const char*);
    int (*class_is)(void*,uintptr_t,const char*);
    int (*name_is)(void*,uintptr_t,const char*);
    int (*actor)(void*,uintptr_t,LidPSProof*);
} LidPSFrameOps;
/* Never accept a partial chain as positive. Bounded/cyclic/malformed chains
 * close the scope. Enemy vetoes are searched through every complete ancestor.
 * An uninitialised local BrgDbChara is skipped, never ownership evidence.
 */
static int lid_ps_frame_origin(const LidPSFrameOps *o,uintptr_t frame,LidPSProof *out){
    uintptr_t seen_frames[8];unsigned depth=0,positive=0,foreign=0,transparent=0;
    LidPSProof proof={0};
    if(!o||!out||!o->is_a||!o->class_is||!o->name_is||!o->actor)return LID_PSF_INVALID;
    while(frame){
        uint64_t node,object,locals,previous;unsigned extent;
        if(depth==8)return LID_PSF_INVALID;
        for(unsigned i=0;i<depth;i++)if(seen_frames[i]==frame)return LID_PSF_INVALID;
        seen_frames[depth++]=frame;
        if(!lid_ps_read(&o->source,frame,0x14,&node,8)||!lid_ps_read(&o->source,frame,0x1c,&object,8)||
           !lid_ps_read(&o->source,frame,0x2c,&locals,8)||!lid_ps_read(&o->source,frame,0x34,&previous,8))return LID_PSF_INVALID;
        if(object){
            if(o->is_a(o->context,(uintptr_t)object,"BrgPawn_CustomCharaAI")||
               o->is_a(o->context,(uintptr_t)object,"BrgAICtrl_CustomChara"))return LID_PSF_ENEMY;
            if(o->is_a(o->context,(uintptr_t)object,"BrgCommonPawn_CustomCharaNative")){
                uint64_t parent;
                if(!lid_ps_read(&o->source,(uintptr_t)object,0x284,&parent,8))return LID_PSF_INVALID;
                if(parent&&o->is_a(o->context,(uintptr_t)parent,"BrgPawn_CustomCharaAI"))return LID_PSF_ENEMY;
            }
            LidPSProof p;if(o->actor(o->context,(uintptr_t)object,&p)==1){if(!positive)proof=p;positive=1;}
        }
        if(!node||o->class_is(o->context,(uintptr_t)node,"Function")!=1||
           !lid_ps_read(&o->source,(uintptr_t)node,0x88,&extent,4)||extent>0x10000)return LID_PSF_INVALID;
        if(o->name_is(o->context,(uintptr_t)node,"SimulationStatus")){
            uint64_t owner;
            if(!lid_ps_read(&o->source,(uintptr_t)node,0x40,&owner,8))return LID_PSF_INVALID;
            if(owner&&(o->name_is(o->context,(uintptr_t)owner,"BrgCommonPawn_CustomChara")||
                       o->name_is(o->context,(uintptr_t)owner,"BrgCommonPawn_CustomCharaNative")))transparent=1;
        }
        uint64_t child;uintptr_t seen_fields[256];unsigned fields=0;
        if(!lid_ps_read(&o->source,(uintptr_t)node,0x80,&child,8))return LID_PSF_INVALID;
        while(child){
            uint64_t next;
            if(fields==256)return LID_PSF_INVALID;
            for(unsigned i=0;i<fields;i++)if(seen_fields[i]==child)return LID_PSF_INVALID;
            seen_fields[fields++]=(uintptr_t)child;
            if(!lid_ps_read(&o->source,(uintptr_t)child,0x60,&next,8))return LID_PSF_INVALID;
            if(o->class_is(o->context,(uintptr_t)child,"StructProperty")){
                unsigned dim,width,at,struct_size;uint64_t flags,struc;
                if(!lid_ps_read(&o->source,(uintptr_t)child,0x68,&dim,4)||
                   !lid_ps_read(&o->source,(uintptr_t)child,0x6c,&width,4)||
                   !lid_ps_read(&o->source,(uintptr_t)child,0x70,&flags,8)||
                   !lid_ps_read(&o->source,(uintptr_t)child,0x94,&at,4)||
                   !lid_ps_read(&o->source,(uintptr_t)child,0xb0,&struc,8))return LID_PSF_INVALID;
                if(struc&&o->class_is(o->context,(uintptr_t)struc,"ScriptStruct")){
                    if(o->name_is(o->context,(uintptr_t)struc,"BrgDbZombie"))return LID_PSF_ENEMY;
                    if(o->name_is(o->context,(uintptr_t)struc,"BrgDbChara")&&dim==1&&!(flags&0x100u)){
                        LidPSArrayCopy cid;LidPSProof p;
                        if(width!=0x220||at>extent||width>extent-at||!locals||
                           !lid_ps_read(&o->source,(uintptr_t)struc,0x88,&struct_size,4)||struct_size!=width||
                           !lid_ps_read(&o->source,(uintptr_t)locals,at,&cid,16))return LID_PSF_INVALID;
                        if(cid.data||cid.num||cid.capacity){
                            if(lid_ps_ue_chara(&o->source,(uintptr_t)locals+at,LID_PS_OWN_UI,&p)){
                                if(!positive)proof=p;positive=1;
                            }else foreign=1;
                        }
                    }
                }
            }
            child=next;
        }
        frame=(uintptr_t)previous;
    }
    if(foreign)return LID_PSF_NATIVE;
    if(positive){*out=proof;return LID_PSF_OWNED;}
    return transparent?LID_PSF_INHERIT:LID_PSF_NATIVE;
}
#endif
