#ifndef LID_SHOP_SAVE_CAPTURE_H
#define LID_SHOP_SAVE_CAPTURE_H
#include "shop_momoka_host.h"
#include "shop_marker_native.h"
/* Called at the owned fresh root, after the original serializer succeeded and
   BEFORE native Encode/free. Caller acquired the SAVE lease BEFORE the original
   model serializer began reading account data and retains it through writer
   completion. Acquiring only at this callback is too late. This code never
   invokes Save, reads a disk file or sends an ACK.
   Do not use on the optional supplied-text path or a previously saved root. */
typedef struct LidShopSaveCapture {
    unsigned char marker[LID_ST_MARKER_MAX];unsigned marker_size;
    uint16_t uid[129];unsigned uid_units;LidMPMoney bank;
    unsigned data_ready,marker_attempted,marker_confirmed;
} LidShopSaveCapture;
typedef struct LidShopCaptureScratch {
    LidShopSaveCapture pending;LidShopMarkerScratch json;
} LidShopCaptureScratch; /* per-save resident storage, not a large stack local */
static int lid_sc_name(const char*text,uint16_t*storage,LidShopMarkerString*out){
    unsigned n=0;while(text[n]){if(n>=31)return 0;storage[n]=(unsigned char)text[n];n++;}
    storage[n]=0;*out=(LidShopMarkerString){storage,(int)n+1,(int)n+1};return 1;
}
static void*lid_sc_child(const LidShopMarkerNative*b,void*root,const char*name){
    uint16_t storage[32];LidShopMarkerString key;void*child;
    if(!b||!b->object||!lid_sc_name(name,storage,&key)||!lid_sm_root(b,root,0))return 0;
    child=b->object(root,&key);return lid_sm_root(b,child,0)?child:0;
}
/* Native GetString returns an OWNED ValueMap copy, including raw numeric
   backslash-hash representation. It is freed even on copy/window failure. */
static int lid_sc_text(const LidShopMarkerNative*b,void*object,const char*name,
                       uint16_t*out,unsigned capacity,unsigned*length){
    uint16_t storage[32];LidShopMarkerString key,value={0};int ok=0;
    if(!b||!b->has||!b->string||!b->release||!out||!length||!capacity||capacity>131||
       !lid_sc_name(name,storage,&key)||!lid_sm_root(b,object,0))return 0;
    if(b->has(object,&key)!=1||!lid_sm_root(b,object,0))return 0;
    LidShopMarkerString*returned=b->string(object,&value,&key);
    if(returned==&value&&value.data&&value.num>=1&&(unsigned)value.num<=capacity&&
       value.capacity>=value.num&&value.capacity<=1048576&&lid_sm_root(b,object,0)&&
       b->access.copy(b->access.context,value.data,out,(size_t)value.num*2)&&out[value.num-1]==0)ok=1;
    if(value.data)b->release(value.data);
    if(!ok||!lid_sm_root(b,object,0))return 0;*length=(unsigned)value.num-1;return 1;
}
static int lid_sc_uid(const LidShopMarkerNative*b,void*object,const uint16_t*expected,
                       unsigned units,uint16_t*scratch){
    unsigned length=0,start=0;
    if(!lid_sc_text(b,object,"uid",scratch,131,&length))return 0;
    if(length>=2&&scratch[0]=='\\'&&scratch[1]=='#')start=2;
    if(length-start!=units)return 0;
    for(unsigned i=0;i<units;i++)if(scratch[start+i]!=expected[i])return 0;return 1;
}
static int lid_sc_money(const LidShopMarkerNative*b,void*object,const char*name,
                         int32_t*out,uint16_t*scratch){
    unsigned length=0;uint64_t value=0;
    if(!lid_sc_text(b,object,name,scratch,13,&length)||length<3||scratch[0]!='\\'||scratch[1]!='#'||
       (length>3&&scratch[2]=='0'))return 0;
    for(unsigned i=2;i<length;i++){
        if(scratch[i]<'0'||scratch[i]>'9')return 0;value=value*10+(scratch[i]-'0');if(value>INT32_MAX)return 0;
    }
    *out=(int32_t)value;return 1;
}
/* INVALID leaves out untouched and does not insert anything. A post-setter
   WRITE_UNVERIFIED publishes a DATA-READY candidate but never a confirmed
   marker/save; the independent BRG verifier must decide after writer close.
   Work/input/output must be disjoint and owned by this invocation. */
static int lid_shop_capture_fresh(LidShopMomokaHost*h,const LidShopLease*lease,
   const LidShopMarkerNative*b,void*root,const uint16_t*uid,unsigned units,
   LidShopCaptureScratch*work,LidShopSaveCapture*out){
    void*user,*soul;int result;
    if(!work||!out||!uid||!units||units>128||uid[units])return LID_SM_INVALID;
    for(unsigned i=0;i<units;i++)if(uid[i]<'0'||uid[i]>'9')return LID_SM_INVALID;
    __builtin_memset(&work->pending,0,sizeof(work->pending));
    if(!lid_shop_host_snapshot(h,lease,work->pending.marker,sizeof(work->pending.marker),&work->pending.marker_size)||
       !lid_sm_root(b,root,1)||!(user=lid_sc_child(b,root,"user"))||!(soul=lid_sc_child(b,root,"soul"))||
       !lid_sc_uid(b,user,uid,units,work->json.text)||!lid_sc_uid(b,soul,uid,units,work->json.text)||
       !lid_sc_money(b,soul,"free_money",&work->pending.bank.free_money,work->json.text)||
       !lid_sc_money(b,soul,"paid_money",&work->pending.bank.paid_money,work->json.text))return LID_SM_INVALID;
    for(unsigned i=0;i<=units;i++)work->pending.uid[i]=uid[i];work->pending.uid_units=units;work->pending.data_ready=1;
    result=lid_shop_marker_insert_fresh(b,root,work->pending.marker,work->pending.marker_size,&work->json);
    if(result==LID_SM_INVALID)return result;
    work->pending.marker_attempted=1;work->pending.marker_confirmed=result==LID_SM_PRESENT;*out=work->pending;return result;
}
#endif
