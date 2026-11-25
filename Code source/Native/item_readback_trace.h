#ifndef LID_ITEM_READBACK_TRACE_H
#define LID_ITEM_READBACK_TRACE_H
/* Detached diagnostic state only; no engine calls, ACKs or acceptance changes. */
static unsigned g_item_readback_reason,g_item_readback_index,g_item_readback_byte,g_item_readback_has_actual;
static LidBlueprintNativeRecord g_item_readback_expected,g_item_readback_actual;
static void item_readback_record(const LidBlueprintNativeRecord*expected,const LidBlueprintNativeRecord*actual,size_t index){
    g_item_readback_index=(unsigned)index;g_item_readback_byte=0;g_item_readback_has_actual=actual!=0;
    g_item_readback_expected=*expected;lid_wire_zero(&g_item_readback_actual,sizeof(g_item_readback_actual));
    if(actual){g_item_readback_actual=*actual;
        const BYTE*a=(const BYTE*)expected,*b=(const BYTE*)actual;
        while(g_item_readback_byte<sizeof(*expected)&&a[g_item_readback_byte]==b[g_item_readback_byte])g_item_readback_byte++;
    }
}
#define LID_ITEM_READBACK_FAIL(code) (g_item_readback_reason=(code),0)
#define LID_ITEM_READBACK_RECORD(expected,actual,index) item_readback_record(expected,actual,index)
#endif
