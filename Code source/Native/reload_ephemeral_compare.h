#ifndef LID_RELOAD_EPHEMERAL_COMPARE_H
#define LID_RELOAD_EPHEMERAL_COMPARE_H
#include "reload_bag_growth.h"
/* Comparison copies only. -1 is outside the verified unsigned account UID.
   Keep ALL other accounts/characters, stock, gacha and unknown fields strict.
   Never write a filtered copy into a repository, RPC payload or save. */
static int lid_re_uid_key(const unsigned short*uid,unsigned short*out,LidBagJsonSpan*key){
    size_t n=0;if(!uid||!out||!key)return 0;
    out[0]='"';while(n<129&&uid[n]){
        if(n>=128||uid[n]<'0'||uid[n]>'9')return 0;out[n+1]=uid[n];n++;
    }
    if(!n||uid[n]||(n>1&&uid[0]=='0'))return 0;
    out[n+1]='"';*key=(LidBagJsonSpan){out,n+2};return 1;
}
/* nested=1: the SKL object is strict except its eqskl UID map.
   nested=0: a UID map; require owner once, omit exactly key -1. */
static int lid_re_map(LidBagJsonSpan input,LidBagJsonSpan owner,unsigned nested,LidBagJsonOut*out){
    size_t i=0;unsigned count=0,total=0,emitted=0,owners=0,equipped=0;unsigned short c;
    LidBagJsonSpan key,value,found;
    lid_bj_space(input,&i);if(i>=input.n||input.p[i++]!='{'||!lid_bj_char(out,'{'))return 0;
    lid_bj_space(input,&i);if(i>=input.n||input.p[i]=='}')return 0;
    for(;;){
        if(++count>128||!lid_bj_pair(input,&i,&key,&value)||!lid_bj_find(input,key,&found,&total))return 0;
        if(!nested&&lid_bj_same(key,owner))owners++;
        if(nested||!lid_bj_text(key,"\"-1\"")){
            if(emitted++&&!lid_bj_char(out,','))return 0;
            if(!lid_bj_emit(out,key)||!lid_bj_char(out,':'))return 0;
            if(nested&&lid_bj_text(key,"\"eqskl\"")){
                equipped++;if(!lid_re_map(value,owner,0,out))return 0;
            }else if(!lid_bj_emit(out,value))return 0;
        }
        lid_bj_space(input,&i);if(i>=input.n)return 0;c=input.p[i++];
        if(c=='}')break;if(c!=',')return 0;
    }
    lid_bj_space(input,&i);
    return i==input.n&&count==total&&(nested?equipped==1:owners==1)&&lid_bj_char(out,'}');
}
static int lid_reload_ephemeral_copy(LidReloadJsonBinding*b,const LidReloadPoolString*source,
    const unsigned short*uid,unsigned domain,LidReloadPoolString*out){
    unsigned short digits[131];LidBagJsonSpan owner;LidBagJsonOut writer={0};int good;
    if(domain>2||!out||out->data||!lid_reload_json_window(b)||
       !lid_reload_json_string_ok(b,source)||!lid_re_uid_key(uid,digits,&owner))return 0;
    writer.cap=(size_t)source->num+1;writer.p=b->allocate(b->memory_context,writer.cap*2);
    if(!writer.p)return 0;
    good=lid_re_map((LidBagJsonSpan){source->data,(size_t)source->num-1},owner,domain==1,&writer);
    if(!good){b->release(b->memory_context,writer.p);return 0;}
    writer.p[writer.n]=0;*out=(LidReloadPoolString){writer.p,(int)writer.n+1,(int)writer.cap};return 1;
}
/* Signed temporary engine UID, canonical int32 only. Positive account keys,
   escaped strings, -0, leading zeros and out-of-range keys stay strict. */
static int lid_re_negative_key(LidBagJsonSpan key){
    size_t i;unsigned n=0,d;
    if(key.n<4||key.n>13||key.p[0]!='"'||key.p[1]!='-'||key.p[key.n-1]!='"'||key.p[2]<'1'||key.p[2]>'9')return 0;
    for(i=2;i+1<key.n;i++){
        if(key.p[i]<'0'||key.p[i]>'9')return 0;d=(unsigned)(key.p[i]-'0');
        if(n>214748364u||(n==214748364u&&d>8))return 0;n=n*10+d;
    }
    return n!=0;
}
static int lid_re_empty_group(LidBagJsonSpan value,unsigned domain){
    size_t i=0;unsigned short end;lid_bj_space(value,&i);
    if(i>=value.n)return 0;
    if(value.p[i]=='{'&&domain!=2)end='}';
    else if(value.p[i]=='['&&domain!=0)end=']';else return 0;
    i++;lid_bj_space(value,&i);if(i>=value.n||value.p[i++]!=end)return 0;
    lid_bj_space(value,&i);return i==value.n;
}
/* Paired fallback: keep any nonempty native result strict. Only deleted or
   empty signed groups can disappear from comparison copies. The native
   actual map is supplied to BOTH passes; never infer a receipt from stock. */
static int lid_re_pair_map(LidBagJsonSpan input,LidBagJsonSpan actual,LidBagJsonSpan owner,
    unsigned nested,unsigned domain,LidBagJsonOut*out){
    size_t i=0;unsigned count=0,total=0,other_total=0,emitted=0,owners=0,equipped=0;
    unsigned short c;LidBagJsonSpan key,value,found,native;
    lid_bj_space(input,&i);if(i>=input.n||input.p[i++]!='{'||!lid_bj_char(out,'{'))return 0;
    lid_bj_space(input,&i);if(i>=input.n||input.p[i]=='}')return 0;
    for(;;){
        int omit=0;
        if(++count>128||!lid_bj_pair(input,&i,&key,&value)||!lid_bj_find(input,key,&found,&total))return 0;
        if(!nested&&lid_bj_same(key,owner))owners++;
        native=(LidBagJsonSpan){0,0};
        if(!nested&&lid_re_negative_key(key)){
            int present=lid_bj_find(actual,key,&native,&other_total);
            omit=!present||lid_re_empty_group(native,domain);
        }
        if(!omit){
            if(emitted++&&!lid_bj_char(out,','))return 0;
            if(!lid_bj_emit(out,key)||!lid_bj_char(out,':'))return 0;
            if(nested&&lid_bj_text(key,"\"eqskl\"")){
                equipped++;if(!lid_bj_find(actual,key,&native,&other_total)||
                    !lid_re_pair_map(value,native,owner,0,domain,out))return 0;
            }else if(!lid_bj_emit(out,value))return 0;
        }
        lid_bj_space(input,&i);if(i>=input.n)return 0;c=input.p[i++];
        if(c=='}')break;if(c!=',')return 0;
    }
    lid_bj_space(input,&i);
    return i==input.n&&count==total&&(nested?equipped==1:owners==1)&&lid_bj_char(out,'}');
}
static int lid_re_pair_copy(LidReloadJsonBinding*b,const LidReloadPoolString*source,
    const LidReloadPoolString*actual,const unsigned short*uid,unsigned domain,LidReloadPoolString*out){
    unsigned short digits[131];LidBagJsonSpan owner;LidBagJsonOut writer={0};int good;
    if(domain>2||!out||out->data||!lid_reload_json_window(b)||!lid_reload_json_string_ok(b,source)||
       !lid_reload_json_string_ok(b,actual)||!lid_re_uid_key(uid,digits,&owner))return 0;
    writer.cap=(size_t)source->num+1;writer.p=b->allocate(b->memory_context,writer.cap*2);
    if(!writer.p)return 0;
    good=lid_re_pair_map((LidBagJsonSpan){source->data,(size_t)source->num-1},
        (LidBagJsonSpan){actual->data,(size_t)actual->num-1},owner,domain==1,domain,&writer);
    if(!good){b->release(b->memory_context,writer.p);return 0;}
    writer.p[writer.n]=0;*out=(LidReloadPoolString){writer.p,(int)writer.n+1,(int)writer.cap};return 1;
}
static int lid_reload_protected_compare(LidReloadJsonBinding*b,const LidReloadPoolString*expected,
    const LidReloadPoolString*actual,const unsigned short*uid,unsigned domain,
    LidReloadCompareTrace*trace,unsigned*removed){
    LidReloadPoolString e={0},a={0},growth={0},pe={0},pa={0};unsigned n=0;int good=0;
    if(!trace||!removed)return 0;*removed=0;
    if(lid_reload_json_compare_trace(b,expected,actual,trace))return 1;
    if(trace->stage!=LID_JC_EQUAL||!trace->equal_called||trace->equal_result||
       !trace->cleanup_ok||b->pool.poisoned)return 0;
    if(lid_reload_ephemeral_copy(b,expected,uid,domain,&e)&&
       lid_reload_ephemeral_copy(b,actual,uid,domain,&a)){
        good=lid_reload_json_compare_trace(b,&e,&a,trace);
    }
    if(!good&&e.data&&a.data&&trace->stage==LID_JC_EQUAL&&trace->equal_called&&
       !trace->equal_result&&trace->cleanup_ok&&!b->pool.poisoned&&
       lid_re_pair_copy(b,&e,&a,uid,domain,&pe)&&lid_re_pair_copy(b,&a,&a,uid,domain,&pa)){
        good=lid_reload_json_compare_trace(b,&pe,&pa,trace);
        if(!good&&domain==0&&trace->stage==LID_JC_EQUAL&&trace->equal_called&&
           !trace->equal_result&&trace->cleanup_ok&&!b->pool.poisoned){
            if(growth.data){b->release(b->memory_context,growth.data);growth=(LidReloadPoolString){0};}
            if(lid_reload_bag_growth_copy(b,&pe,&pa,&growth,&n))good=lid_reload_json_compare_trace(b,&pe,&growth,trace);
        }
    }
    if(pa.data)b->release(b->memory_context,pa.data);
    if(pe.data)b->release(b->memory_context,pe.data);
    if(growth.data)b->release(b->memory_context,growth.data);
    if(a.data)b->release(b->memory_context,a.data);
    if(e.data)b->release(b->memory_context,e.data);
    if(good)*removed=n;return good;
}
#endif
