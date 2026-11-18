#ifndef LID_UI_ITEM_COLORS_NATIVE_R323_H
#define LID_UI_ITEM_COLORS_NATIVE_R323_H
#include "ui_item_colors.h"
/* Exact 6CF build, synchronous UI owner only. Extends the native engine-owned
   palette, preserving every original entry; no file/resource/save modification.
   Preparation failure is presentation-only and never blocks a transaction. */
static void*r323_color_resize(void*image,void*memory,size_t bytes){
    return ((void*(*)(void*,size_t,unsigned))((BYTE*)image+0xe0460))(memory,bytes,8);
}
static int r323_colors_native_prepare(BYTE*image){
    static const uint8_t prefix[24]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf1,0x48,0x8b,0x0d,0x67,0x37,0x73};
    uint32_t owner=__atomic_load_n(&g_hub_events.owner,__ATOMIC_ACQUIRE);LidUIColorArray a;uint32_t values[100];
    if(!image||!g_get_tid||g_get_tid()!=owner||!g_ap_mode_active||!g_savedata_route_active||
       __atomic_load_n(&g_checks_delivery.stop,__ATOMIC_ACQUIRE)||
       !r6_copy(&g_checks_ops,(uintptr_t)image+0xf8a03d8,&a,sizeof(a))||
       a.count<1||a.count>100||a.capacity<a.count||a.capacity>4096||!a.data||
       !r6_copy(&g_checks_ops,(uintptr_t)a.data,values,(unsigned)a.count*4))return 0;
    if(g_lid_ui_colors.ready&&g_lid_ui_colors.data==(uintptr_t)a.data&&g_lid_ui_colors.count==a.count){
        unsigned ok=1;for(unsigned i=0;i<5;i++)if(g_lid_ui_colors.indices[i]>=(unsigned)a.count||values[g_lid_ui_colors.indices[i]]!=lid_ui_rgb[i])ok=0;
        if(ok)return 1;
    }
    char sha[65];uint8_t actual[24];
    if(!g_material_activation_hash||!r105_verified(g_material_activation_hash,owner,(uintptr_t)image,sha)||
       !r6_copy(&g_checks_ops,(uintptr_t)image+0xe0460,actual,24)||!r15_equal(actual,prefix,24))return 0;
    /* Native allocation is performed once for missing colors, never per draw.
       Source bytes and header are checked before borrowing native storage. */
    int ok=lid_ui_register(&g_lid_ui_colors,(LidUIColorArray*)(image+0xf8a03d8),image,r323_color_resize);
    if(ok)log_ascii("AP UI R323: native item-color palette ready; existing colors preserved.");
    return ok;
}
#endif
