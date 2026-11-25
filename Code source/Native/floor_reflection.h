#ifndef LID_FLOOR_REFLECTION_H
#define LID_FLOOR_REFLECTION_H
/* Bounded cold metadata, with exact property and parameter layouts. No calls. */
typedef struct LidFloorLayout {void*function;unsigned size,offset[4];} LidFloorLayout;
typedef struct LidFloorMeta {
    void*volume_class;void*stage_class;void*game_class;void*touch;
    unsigned target,type,ui,touch_volume,current_floor,to_floor,strings[7];
    LidFloorLayout unit,connection,close;
    unsigned ready;
} LidFloorMeta;
static int lid_floor_is_a(void*object,const char*name,const LidEventOps*o){
    if(!object||!o||!o->readable(object,0x58))return 0;
    BYTE*c=*(BYTE**)((BYTE*)object+0x50);
    for(unsigned n=0;c&&n<32;n++){
        if(!o->readable(c,0x80))return 0;
        if(o->name_is(c,name))return 1;c=*(BYTE**)(c+0x78);
    }return 0;
}
static int lid_floor_property(void*property,const char*type,unsigned width,unsigned extent,unsigned*offset,const LidEventOps*o){
    if(!offset||!property||!lid_event_class_is(property,type,o)||!o->readable(property,0x98))return 0;
    unsigned dim=*(unsigned*)((BYTE*)property+0x68),size=*(unsigned*)((BYTE*)property+0x6c),at=*(unsigned*)((BYTE*)property+0x94);
    if(dim!=1||size!=width||at>extent||width>extent-at)return 0;*offset=at;return 1;
}
static int lid_floor_field(void*object,const char*name,const char*type,unsigned width,unsigned*offset,const LidEventOps*o){
    void*p=lid_event_field(object,name,type,o);BYTE*cls;
    if(!object||!o->readable(object,0x58)||(cls=*(BYTE**)((BYTE*)object+0x50))==0||!o->readable(cls,0x8c))return 0;
    unsigned extent=*(unsigned*)(cls+0x88);return extent<=1048576&&lid_floor_property(p,type,width,extent,offset,o)&&*offset>=0x58;
}
static int lid_floor_function(void*object,const char*name,const char*const*names,const char*const*types,const unsigned*widths,unsigned count,LidFloorLayout*out,const LidEventOps*o){
    LidFloorLayout m={0};BYTE*p;unsigned found=0,visits=0;
    if(!out||count>4||(m.function=lid_event_find(object,name,o))==0||!o->readable(m.function,0xe6))return 0;
    m.size=*(unsigned short*)((BYTE*)m.function+0xe2);
    if(!m.size||m.size>256||*(unsigned*)((BYTE*)m.function+0x88)<m.size)return 0;
    p=*(BYTE**)((BYTE*)m.function+0x80);
    while(p){
        if(++visits>128||!o->readable(p,0x98))return 0;
        unsigned long long flags=*(unsigned long long*)(p+0x70);
        if(flags&0x80){
            unsigned which=count;for(unsigned i=0;i<count;i++)if(o->name_is(p,names[i])){which=i;break;}
            if(which==count||(found&(1u<<which))||!lid_floor_property(p,types[which],widths[which],m.size,m.offset+which,o))return 0;
            if(o->name_is(p,"ReturnValue")){
                if(!(flags&0x400)||!o->readable(p,0xb4)||*(unsigned*)(p+0xb0)!=1||
                   *(unsigned short*)((BYTE*)m.function+0xe4)!=m.offset[which])return 0;
            }else if(flags&0x400)return 0;
            found|=1u<<which;
        }
        p=*(BYTE**)(p+0x60);
    }
    if(found!=(1u<<count)-1)return 0;
    for(unsigned i=0;i<count;i++)for(unsigned j=i+1;j<count;j++)
        if(m.offset[i]<m.offset[j]+widths[j]&&m.offset[j]<m.offset[i]+widths[i])return 0;
    *out=m;return 1;
}
static int lid_floor_prepare_meta(LidFloorMeta*out,void*game,void*stage,void*volume,const LidEventOps*o){
    static const char*unit_names[]={"lvlname","unit","ReturnValue"};
    static const char*unit_types[]={"StrProperty","StrProperty","BoolProperty"};static const unsigned unit_width[]={16,16,4};
    static const char*exit_names[]={"unitid","escalator","dirUp","ReturnValue"};
    static const char*exit_types[]={"StrProperty","StructProperty","IntProperty","BoolProperty"};static const unsigned exit_width[]={16,120,4,4};
    static const char*close_names[]={"disableEffect"};static const char*close_types[]={"ByteProperty"};static const unsigned close_width[]={1};
    LidFloorMeta m={0};void*point;void*parameter;BYTE*structure;BYTE*field;unsigned seen=0,found=0;
    if(!out||!lid_floor_is_a(volume,"BrgVolume_Escalator",o)||!lid_floor_is_a(stage,"BrgStageManager",o)||!lid_floor_is_a(game,"BrgGameInfo",o)||
       !lid_floor_field(volume,"mEscalatorTargetPoint","ObjectProperty",8,&m.target,o)||
       !o->readable((BYTE*)volume+m.target,8)||(point=*(void**)((BYTE*)volume+m.target))==0||
       !lid_floor_field(point,"mType","ByteProperty",1,&m.type,o)||
       !lid_floor_field(game,"mUIManager","ObjectProperty",8,&m.ui,o)||
       !lid_floor_field(game,"mTouchEscalatorVolume","ObjectProperty",8,&m.touch_volume,o)||
       !lid_floor_field(stage,"mCurrentFloorId","StrProperty",16,&m.current_floor,o)||
       !lid_floor_function(stage,"GetUnitNameInCurrentArea",unit_names,unit_types,unit_width,3,&m.unit,o)||
       !lid_floor_function(stage,"GetCurrentAreaEscalatorAndDir",exit_names,exit_types,exit_width,4,&m.connection,o)||
       !lid_floor_function(volume,"SetToDisable",close_names,close_types,close_width,1,&m.close,o))return 0;
    if(!o->readable(m.unit.function,0xd6)||!o->readable(m.connection.function,0xd6)||!o->readable(m.close.function,0xd6)||
       !(*(unsigned*)((BYTE*)m.unit.function+0xd0)&0x400)||!(*(unsigned*)((BYTE*)m.connection.function+0xd0)&0x400)||
       (*(unsigned*)((BYTE*)m.close.function+0xd0)&0x400)||!(*(unsigned*)((BYTE*)m.close.function+0xd0)&2))return 0;
    /* Function children belong to its own UStruct, not to the Function class. */
    parameter=0;field=*(BYTE**)((BYTE*)m.connection.function+0x80);
    while(field){if(++seen>128||!o->readable(field,0xb8))return 0;if(o->name_is(field,"escalator")){if(parameter)return 0;parameter=field;}field=*(BYTE**)(field+0x60);}
    if(!parameter||(structure=*(BYTE**)((BYTE*)parameter+0xb0))==0||!o->readable(structure,0x8c)||
       !lid_event_class_is(structure,"ScriptStruct",o)||!o->name_is(structure,"BrgDbAreaConnectEscalator")||*(unsigned*)(structure+0x88)!=120)return 0;
    field=*(BYTE**)(structure+0x80);seen=0;unsigned string_count=0;
    while(field){
        if(++seen>128||!o->readable(field,0x98))return 0;
        if(o->name_is(field,"mToflr")){if(found++||!lid_floor_property(field,"StrProperty",16,120,&m.to_floor,o))return 0;}
        if(lid_event_class_is(field,"StrProperty",o)){
            if(string_count==7||!lid_floor_property(field,"StrProperty",16,120,m.strings+string_count,o))return 0;string_count++;
        }
        field=*(BYTE**)(field+0x60);
    }
    if(found!=1||string_count!=7||(m.touch=lid_event_find(volume,"Touch",o))==0)return 0;
    for(unsigned i=0;i<7;i++)for(unsigned j=i+1;j<7;j++)
        if(m.strings[i]<m.strings[j]+16&&m.strings[j]<m.strings[i]+16)return 0;
    m.game_class=*(void**)((BYTE*)game+0x50);m.stage_class=*(void**)((BYTE*)stage+0x50);m.volume_class=*(void**)((BYTE*)volume+0x50);m.ready=1;*out=m;return 1;
}
#endif
