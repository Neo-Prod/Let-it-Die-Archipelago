#ifndef LID_SHOP_HISTORY_ADAPTER_H
#define LID_SHOP_HISTORY_ADAPTER_H
/* Include shop_history_codec.h first. Bounded one-call source adapter.
   No hooks installed and not included by mod.c yet.
   The caller owns source lifetimes and provides a stable nonzero AP session.
   Decoding may allocate ONLY owned row contents; release_row MUST handle
   zero/partially initialized records. Every decode attempt is released once.
   C callbacks must return errors normally: native exceptions are not caught. */
typedef struct LidShopHistoryAdapterOps {
    void* context;
    LidShopHistoryRead read;
    unsigned long long (*session)(void*);
    int (*decode_json)(void*,void*,unsigned char*);
    void (*release_row)(void*,unsigned char*);
} LidShopHistoryAdapterOps;
typedef struct LidShopHistorySource {
    const void* data;
    unsigned count,stride,json_pointers;
} LidShopHistorySource;
static int lid_shop_history_encode_source(const LidShopHistorySource* source,
        const LidShopHistoryAdapterOps* ops,unsigned long long session,
        void* output,unsigned capacity,unsigned* length){
    LidShopHistoryKey key;unsigned i,j;size_t base,offset;unsigned long long address;
    unsigned char descriptor[8],row[0x94];int decoded,encoded;
    if(!length)return 0;
    *length=0;
    if(!source||!ops||!ops->read||!ops->session||!session||
       ops->session(ops->context)!=session||!source->data||
       source->json_pointers>1||source->count==0||source->count>256||
       source->stride!=(source->json_pointers?8:0x94)||
       (source->json_pointers&&(!ops->decode_json||!ops->release_row)))return 0;
    base=(size_t)source->data;
    if(base>(size_t)-1-(size_t)source->count*source->stride)return 0;
    if(!lid_shop_history_begin(&key,output,capacity,source->count))return 0;
    for(i=0;i<source->count;i++){
        if(ops->session(ops->context)!=session)return 0;
        offset=(size_t)i*source->stride;
        if(!source->json_pointers){
            if(!lid_shop_history_append(&key,(const void*)(base+offset),ops->read,ops->context))return 0;
            continue;
        }
        if(!ops->read(ops->context,(const void*)(base+offset),descriptor,8))return 0;
        address=lid_shop_history_u64(descriptor);
        if(!address)return 0;
        for(j=0;j<sizeof(row);j++)row[j]=0;
        decoded=ops->decode_json(ops->context,(void*)(size_t)address,row);
        encoded=decoded==1&&ops->session(ops->context)==session&&
            lid_shop_history_append(&key,row,ops->read,ops->context);
        ops->release_row(ops->context,row);
        if(!encoded)return 0;
    }
    if(ops->session(ops->context)!=session)return 0;
    return lid_shop_history_finish(&key,length);
}
#endif
