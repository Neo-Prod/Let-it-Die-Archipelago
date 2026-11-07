#ifndef LID_SKILL_CAPACITY_POLICY_H
#define LID_SKILL_CAPACITY_POLICY_H
/* Read-only equipped-slot inspection. Does not write equipment or grant items.
   Requires progression_wire.h. Chara eqskl +84, stride20, slot index+10. */
typedef struct LidSkillCapacityPolicy {
    char slot_key[21];unsigned ready;int applied;
} LidSkillCapacityPolicy;
static int lid_skill_key_valid(const char*key){
    unsigned i;if(!key)return 0;
    for(i=0;i<20;i++)if(!((key[i]>='0'&&key[i]<='9')||(key[i]>='a'&&key[i]<='f')))return 0;
    return key[20]==0;
}
static int lid_skill_same_key(const LidSkillCapacityPolicy*p,const char*key){
    unsigned i;if(!p||!p->ready||!lid_skill_key_valid(key))return 0;
    for(i=0;i<21;i++)if(p->slot_key[i]!=key[i])return 0;return 1;
}
static int lid_skill_equipped_floor(const BYTE*chara,int(*readable)(const void*,size_t),int*out){
    const BYTE*data,*row,*id;unsigned n,max,i,j,chars,string_max;int slot,floor=0;
    if(!chara||!readable||!out||!readable(chara+0x84,16))return 0;
    data=(const BYTE*)(size_t)lid_wire_r64(chara+0x84);n=lid_wire_r32(chara+0x8c);max=lid_wire_r32(chara+0x90);
    if(n>64||n>max||max>4096||(!data&&max)||(n&&(!data||!readable(data,(size_t)n*0x20))))return 0;
    for(i=0;i<n;i++){
        row=data+(size_t)i*0x20;slot=(int)lid_wire_r32(row+0x10);
        id=(const BYTE*)(size_t)lid_wire_r64(row);chars=lid_wire_r32(row+8);string_max=lid_wire_r32(row+12);
        if(slot<0||slot>=64||chars<2||chars>64||chars>string_max||string_max>0x7fffffffu||!id||!readable(id,(size_t)chars*2))return 0;
        for(j=0;j<chars;j++){
            unsigned c=(unsigned)id[2*j]|((unsigned)id[2*j+1]<<8);
            if(j+1==chars?c!=0:c==0)return 0;
        }
        if(slot+1>floor)floor=slot+1;
    }
    *out=floor;return 1;
}
static int lid_skill_can_apply(const LidSkillCapacityPolicy*p,const char*key,int target,int floor){
    return p&&lid_skill_key_valid(key)&&target>=0&&target<=64&&floor>=0&&floor<=64&&target>=floor&&
        (!lid_skill_same_key(p,key)||target>=p->applied);
}
static int lid_skill_record_application(LidSkillCapacityPolicy*p,const char*key,int target,int floor){
    unsigned i;if(!lid_skill_can_apply(p,key,target,floor))return 0;
    for(i=0;i<21;i++)p->slot_key[i]=key[i];p->applied=target;p->ready=1;return 1;
}
/* 0=leave native metadata untouched; 1=apply protected capacity; -1=invalid.
   Before first AP application, grow only to protect an equipped high slot. */
static int lid_skill_detail_target(const LidSkillCapacityPolicy*p,const char*key,int calculated,int native,int floor,int*out){
    int target;
    if(!p||!out||!lid_skill_key_valid(key)||calculated<0||calculated>64||native<0||native>64||floor<0||floor>64)return -1;
    if(!lid_skill_same_key(p,key)){
        if(native>=floor)return 0;
        *out=floor;return 1;
    }
    target=calculated;if(target<p->applied)target=p->applied;if(target<floor)target=floor;
    *out=target;return 1;
}
#endif
