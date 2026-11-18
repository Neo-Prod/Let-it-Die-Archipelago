#ifndef LID_BLUEPRINT_ENHANCEMENT_ENGINE_H
#define LID_BLUEPRINT_ENHANCEMENT_ENGINE_H
#include "blueprint_enhancement_catalog.h"
/* Native upsert plus lower-level archival, never the full research completion
   routine (which also changes counters/remodel/timers). Same writer lease. */
static int lid_blueprint_enhanced_guard(void*context){
    static const BYTE prefix[24]={0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,0x48,0x89,0x5c,0x24,0x30,0x8b,0xea,0x4c};
    LidBlueprintEngineAdapter*e=context;
    return lid_blueprint_engine_guard(e)&&e->readable(e->exe+0x1322750,24)&&e->executable(e->exe+0x1322750)&&lid_wire_equal(e->exe+0x1322750,prefix,24);
}
static int lid_blueprint_engine_upsert_enhanced(void*context,const LidBlueprintNativeRecord*r){
    LidBlueprintEngineAdapter*e=context;
    if(!r||!lid_blueprint_enhanced_guard(e)||r->state.part>=LID_BLUEPRINT_PART_COUNT||!r->state.level||
       r->state.level>lid_enhancement_cap[r->state.part]||r->state.checked||r->state.announced>1||
       !lid_bp_utf_equal_ascii(r->ptid,lid_blueprint_parts[r->state.part]))return 0;
    if(r->state.research==LID_BP_FINISHED){
        if(r->state.receive!=LID_BP_CHARGE||r->state.announced!=1||!lid_bp_utf_equal_ascii(r->research_name,"FINISHED")||!lid_bp_utf_equal_ascii(r->receive_name,"CHARGE"))return 0;
    }else if(r->state.research!=LID_BP_LEVELUP||r->state.receive!=LID_BP_UNKNOWN||r->state.announced||
             !lid_bp_utf_equal_ascii(r->research_name,"LEVELUP")||!lid_bp_utf_equal_ascii(r->receive_name,"UNKNOWN"))return 0;
    if(r->before_ptid[0]){
        lid_bp_name_index_init(&e->names);unsigned before=lid_bp_part_index(&e->names,r->before_ptid);
        if(before>=LID_BLUEPRINT_PART_COUNT||!r->before_level||r->before_level>lid_enhancement_cap[before])return 0;
    }else if(r->before_level)return 0;
    return ((LidBlueprintEngineUpsert)(e->exe+0x12fbbe0))(r->ptid,r->state.level,r->research_name,r->before_ptid,r->before_level,r->receive_name,r->state.announced)==1;
}
static int lid_blueprint_engine_archive_enhanced(void*context,unsigned part,unsigned threshold){
    LidBlueprintEngineAdapter*e=context;unsigned short ptid[64];
    if(!lid_blueprint_enhanced_guard(e)||part>=LID_BLUEPRINT_PART_COUNT||threshold<1||threshold>lid_enhancement_cap[part]+1u)return 0;
    lid_bp_utf_from_ascii(ptid,lid_blueprint_parts[part]);
    typedef unsigned long long(WINAPI*Archive)(const unsigned short*,unsigned);
    return ((Archive)(e->exe+0x1322750))(ptid,threshold)==1;
}
#endif
