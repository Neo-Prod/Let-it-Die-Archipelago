#ifndef LID_PROGRESSION_EVENTS_H
#define LID_PROGRESSION_EVENTS_H
/* UE3 reflected calls for the current executable. Include winlite.h first.
   No cached UObject pointers; validate again immediately before dispatch.
   Offsets: UObject.Class50, UStruct.Super78/Children80, UField.Next60;
   property ArrayDim68/ElementSize6c/Offset94, BoolProperty.BitMaskb0.
   Unknown metadata means refusal, never an estimated parameter buffer. */
typedef struct LidEventOps {
    int (*readable)(const void*,size_t);
    int (*name_is)(void*,const char*);
    int (*executable)(const void*);
} LidEventOps;
typedef void (WINAPI *LidProcessEvent)(void*,void*,void*,void*);

static int lid_event_class_is(void* object,const char* name,const LidEventOps* ops){
    void* cls;
    if(!object||!ops||!ops->readable||!ops->name_is||!ops->executable||
       !ops->readable(object,0x58))return 0;
    cls=*(void**)((BYTE*)object+0x50);
    return cls&&ops->readable(cls,0x50)&&ops->name_is(cls,name);
}
/* Read-only class walk, deliberately avoids FindFunction's cache mutation and
   FName creation. Fixed visit bounds also terminate cycles/corrupt lists. */
static void* lid_event_field(void* object,const char* name,const char* type,const LidEventOps* ops){
    BYTE* cls;BYTE* child;unsigned classes,fields=0;
    if(!object||!name||!type||!ops||!ops->readable||!ops->name_is||!ops->executable||
       !ops->readable(object,0x58))return 0;
    cls=*(BYTE**)((BYTE*)object+0x50);
    for(classes=0;cls&&classes<32;classes++){
        if(!ops->readable(cls,0x88))return 0;
        child=*(BYTE**)(cls+0x80);
        while(child){
#ifdef LID_PERF_DETAIL
            LID_PERF_DETAIL(2,1);
#endif
            if(++fields>4096||!ops->readable(child,0x68))return 0;
            if(ops->name_is(child,name)){
                if(!lid_event_class_is(child,type,ops))return 0;
                return child;
            }
            child=*(BYTE**)(child+0x60);
        }
        cls=*(BYTE**)(cls+0x78);
    }
    return 0;
}
static void* lid_event_find(void* object,const char* name,const LidEventOps* ops){
    return lid_event_field(object,name,"Function",ops);
}
enum {LID_EVENT_VOID=0,LID_EVENT_BOOL=1,LID_EVENT_BAG_SIZE=2};
static int lid_event_layout(void* function,int shape,const LidEventOps* ops){
    BYTE* property;
    if(!lid_event_class_is(function,"Function",ops)||!ops->readable(function,0x88))return 0;
    property=*(BYTE**)((BYTE*)function+0x80);
    if(shape==LID_EVENT_VOID)return property==0;
    if(shape!=LID_EVENT_BOOL&&shape!=LID_EVENT_BAG_SIZE)return 0;
    if(!property||!ops->readable(property,shape==LID_EVENT_BOOL?0xb4:0x98)||
       *(void**)(property+0x60)||*(int*)(property+0x68)!=1||
       *(int*)(property+0x6c)!=4||*(int*)(property+0x94)!=0)return 0;
    if(shape==LID_EVENT_BOOL)return lid_event_class_is(property,"BoolProperty",ops)&&
        ops->name_is(property,"ReturnValue")&&*(unsigned*)(property+0xb0)==1;
    return lid_event_class_is(property,"IntProperty",ops)&&ops->name_is(property,"inStockNum");
}
/* Exact supported events only: no generic arbitrary-name mutation API. */
static int lid_event_call(void* object,int shape,const LidEventOps* ops,int value){
    const char* name;void* function;void** vt;LidProcessEvent dispatch;int params=0;
    if(shape==LID_EVENT_BOOL)name="IsReady";
    else if(shape==LID_EVENT_BAG_SIZE){if(value<0||value>=100)return -1;name="SetDeathBagSize";params=value;}
    else if(shape==LID_EVENT_VOID)name="StatusUpdateByChangeEquipPart";
    else return -1;
    function=lid_event_find(object,name,ops);
    if(!function||!lid_event_layout(function,shape,ops)||!ops->readable(object,sizeof(void*)))return -1;
    vt=*(void***)object;
    if(!vt||!ops->readable(vt,0x220)||!ops->executable(vt[0x218/8]))return -1;
    dispatch=(LidProcessEvent)vt[0x218/8];
    dispatch(object,function,shape==LID_EVENT_VOID?0:&params,0);
    if(shape==LID_EVENT_BOOL)return params==0?0:params==1?1:-1;
    return 1;
}
#endif
