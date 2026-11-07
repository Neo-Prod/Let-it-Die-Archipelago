#ifndef LID_SHOP_PRICE_VIEW_H
#define LID_SHOP_PRICE_VIEW_H
/* Both reads use the same detached Part during the proven buy_part scope:
   first required-funds map, then the final debit. */
static int lid_shop_price_site(unsigned long long rva){
    return rva==0x1309bdeULL||rva==0x134f38eULL;
}
/* Pure scalar edits for caller-proven DETACHED temporary records ONLY.
   No includes from mod.c yet. Never pass master/cache memory to these helpers.
   These functions do not prove ownership, hub origin, AP mode or a live scope.
   Proof inputs must be supplied by the adapter, not current location at debit.
   UI helper edits the existing by-value temporary consumed by AddUnitFromDbPart;
   it does NOT copy/free that temporary or alter the original destructor path. */
static int lid_shop_price_ui(void* detached,unsigned size,int verified_hub,int buy_price){
    unsigned char* p=detached;unsigned i;
    if(!p||size<0x234||verified_hub!=1||buy_price!=1||(p[0x203]&0x80))return 0;
    for(i=0x200;i<0x204;i++)p[i]=0;
    return 1;
}
static int lid_shop_price_native(void* detached,unsigned size,int verified_scope){
    unsigned char* p=detached;unsigned i;
    if(!p||size<0x340||verified_scope!=1||p[0x33c]!=1||(p[0x183]&0x80))return 0;
    for(i=0x17c;i<0x184;i++)p[i]=0;
    return 1;
}
#endif
