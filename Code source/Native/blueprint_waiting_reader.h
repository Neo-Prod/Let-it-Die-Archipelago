#ifndef LID_BLUEPRINT_WAITING_READER_H
#define LID_BLUEPRINT_WAITING_READER_H
/* Include engine adapter first. Snapshot read ONLY under its reservation.
   Never complete/cancel a paid research or remove an expired row here. */
static int lid_blueprint_engine_waiting(void* context,unsigned char* out,size_t capacity){
    LidBlueprintEngineAdapter* engine=context;const BYTE *repo,*data;unsigned count,max,i,low,high,middle,part;
    unsigned short type[32],ptid[64];int cmp;
    if(!out||capacity!=976)return 0;lid_wire_zero(out,capacity);
    if(!lid_blueprint_engine_guard(engine))return 0;
    repo=engine->exe+0x27e5d6c;
    if(!engine->readable(repo,0x60)||(const void*)(size_t)lid_wire_r64(repo)!=engine->exe+0x2191560)return 0;
    count=lid_wire_r32(repo+0x58);max=lid_wire_r32(repo+0x5c);data=(const BYTE*)(size_t)lid_wire_r64(repo+0x50);
    if(count>max||max>LID_BLUEPRINT_MAX_ROWS||(count&&(!data||!engine->readable(data,(size_t)count*0x4c))))return 0;
    lid_bp_name_index_init(&engine->names);
    for(i=0;i<count;i++){
        const BYTE* row=data+(size_t)i*0x4c;
        if((const void*)(size_t)lid_wire_r64(row)!=engine->exe+0x21914f0||
           !lid_bp_read_string(type,32,row+8,engine->readable)||!lid_bp_read_string(ptid,64,row+0x18,engine->readable))return 0;
        if(!lid_bp_utf_equal_ascii(type,"RESEARCHPART")&&!lid_bp_utf_equal_ascii(type,"BUYPART"))continue;
        part=lid_bp_part_index(&engine->names,ptid);
        if(part==976){low=0;high=LID_BLUEPRINT_ALIAS_COUNT;
            while(low<high){middle=low+(high-low)/2;cmp=lid_bp_utf_ascii_compare(ptid,lid_blueprint_aliases[middle].ptid);
                if(!cmp){part=lid_blueprint_aliases[middle].owner;break;}if(cmp<0)high=middle;else low=middle+1;}
        }
        if(part<976){unsigned level=lid_wire_r32(row+0x28);if(!level||level>1000)return 0;out[part]=1;}
    }
    return 1;
}
#endif
