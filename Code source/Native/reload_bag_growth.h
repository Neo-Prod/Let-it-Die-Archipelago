#ifndef LID_RELOAD_BAG_GROWTH_H
#define LID_RELOAD_BAG_GROWTH_H
/* Private comparison copies ONLY. Preserve every original slot via the native
   equality check after removing strictly validated EMPTY appended slots.
   Never write these bytes into any repository or save. */
typedef struct LidBagJsonSpan {const unsigned short*p;size_t n;} LidBagJsonSpan;
typedef struct LidBagJsonOut {unsigned short*p;size_t n,cap;unsigned removed;} LidBagJsonOut;
static int lid_bj_ws(unsigned short c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static void lid_bj_space(LidBagJsonSpan s,size_t*i){while(*i<s.n&&lid_bj_ws(s.p[*i]))(*i)++;}
static int lid_bj_string(LidBagJsonSpan s,size_t*i){
    unsigned short c;size_t j;if(*i>=s.n||s.p[(*i)++]!='"')return 0;
    while(*i<s.n){c=s.p[(*i)++];if(c=='"')return 1;if(c<32)return 0;
        if(c=='\\'){if(*i>=s.n)return 0;c=s.p[(*i)++];
            if(c=='u'){for(j=0;j<4;j++){if(*i>=s.n)return 0;c=s.p[(*i)++];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}}
            else if(!(c=='"'||c=='\\'||c=='/'||c=='b'||c=='f'||c=='n'||c=='r'||c=='t'))return 0;}}
    return 0;
}
static int lid_bj_value(LidBagJsonSpan s,size_t*i,unsigned depth){
    unsigned short c,end;size_t start;int object;
    lid_bj_space(s,i);if(*i>=s.n||depth>8)return 0;c=s.p[*i];
    if(c=='"')return lid_bj_string(s,i);
    if(c=='{'||c=='['){object=c=='{';end=object?'}':']';(*i)++;lid_bj_space(s,i);
        if(*i<s.n&&s.p[*i]==end){(*i)++;return 1;}
        for(;;){if(object){if(!lid_bj_string(s,i))return 0;lid_bj_space(s,i);if(*i>=s.n||s.p[(*i)++]!=':')return 0;}
            if(!lid_bj_value(s,i,depth+1))return 0;lid_bj_space(s,i);if(*i>=s.n)return 0;
            c=s.p[(*i)++];if(c==end)return 1;if(c!=',')return 0;lid_bj_space(s,i);}}
    start=*i;while(*i<s.n&&!lid_bj_ws(s.p[*i])&&s.p[*i]!=','&&s.p[*i]!=']'&&s.p[*i]!='}')(*i)++;
    return *i>start;
}
static int lid_bj_same(LidBagJsonSpan a,LidBagJsonSpan b){size_t i;if(a.n!=b.n)return 0;for(i=0;i<a.n;i++)if(a.p[i]!=b.p[i])return 0;return 1;}
static int lid_bj_text(LidBagJsonSpan s,const char*t){size_t i=0;while(t[i]){if(i>=s.n||s.p[i]!=(unsigned char)t[i])return 0;i++;}return i==s.n;}
static int lid_bj_emit(LidBagJsonOut*out,LidBagJsonSpan s){size_t i;if(s.n>=out->cap-out->n)return 0;for(i=0;i<s.n;i++)out->p[out->n++]=s.p[i];return 1;}
static int lid_bj_char(LidBagJsonOut*out,unsigned short c){if(out->n+1>=out->cap)return 0;out->p[out->n++]=c;return 1;}
/* Map iterator: validates syntax, bounded cardinality, no borrowed objects. */
static int lid_bj_pair(LidBagJsonSpan s,size_t*i,LidBagJsonSpan*k,LidBagJsonSpan*v){
    size_t start;lid_bj_space(s,i);start=*i;if(!lid_bj_string(s,i))return 0;*k=(LidBagJsonSpan){s.p+start,*i-start};
    lid_bj_space(s,i);if(*i>=s.n||s.p[(*i)++]!=':')return 0;lid_bj_space(s,i);start=*i;
    if(!lid_bj_value(s,i,0))return 0;*v=(LidBagJsonSpan){s.p+start,*i-start};return 1;
}
static int lid_bj_find(LidBagJsonSpan s,LidBagJsonSpan key,LidBagJsonSpan*out,unsigned*count){
    size_t i=0;LidBagJsonSpan k,v;unsigned hits=0,n=0;unsigned short c;
    lid_bj_space(s,&i);if(i>=s.n||s.p[i++]!='{')return 0;lid_bj_space(s,&i);
    if(i<s.n&&s.p[i]=='}')i++;
    else for(;;){if(++n>128||!lid_bj_pair(s,&i,&k,&v))return 0;
        if(lid_bj_same(k,key)){hits++;*out=v;}lid_bj_space(s,&i);if(i>=s.n)return 0;
        c=s.p[i++];if(c=='}')break;if(c!=',')return 0;}
    lid_bj_space(s,&i);if(i!=s.n||hits>1)return 0;*count=n;return hits==1;
}
static int lid_bj_uint(LidBagJsonSpan s,unsigned expected){
    unsigned v=0;size_t i;if(!s.n||s.n>10||(s.n>1&&s.p[0]=='0'))return 0;
    for(i=0;i<s.n;i++){if(s.p[i]<'0'||s.p[i]>'9'||v>214748364u)return 0;v=v*10+(unsigned)(s.p[i]-'0');}
    return v==expected;
}
static int lid_bj_empty(LidBagJsonSpan row,LidBagJsonSpan uid,LidBagJsonSpan cid,unsigned slot){
    static const char*const keys[]={"\"uid\"","\"cid\"","\"slot\"","\"type\"","\"eid\"","\"site\"","\"arm_slot\""};
    size_t i=0,j;unsigned mask=0,bit;LidBagJsonSpan k,v,number;unsigned short c;
    if(!row.n||row.p[i++]!='{'||uid.n<3)return 0;
    number=(LidBagJsonSpan){uid.p+1,uid.n-2};
    for(j=0;j<number.n;j++)if(number.p[j]<'0'||number.p[j]>'9')return 0;
    for(;;){if(!lid_bj_pair(row,&i,&k,&v))return 0;
        for(j=0;j<7;j++)if(lid_bj_text(k,keys[j]))break;if(j==7)return 0;bit=1u<<(unsigned)j;if(mask&bit)return 0;mask|=bit;
        if(j==0&&!lid_bj_same(v,number))return 0;
        if(j==1&&!lid_bj_same(v,cid))return 0;
        if(j==2&&!lid_bj_uint(v,slot))return 0;
        if((j==3||j==6)&&!lid_bj_text(v,"-1"))return 0;
        if((j==4||j==5)&&!lid_bj_text(v,"\"\""))return 0;
        lid_bj_space(row,&i);if(i>=row.n)return 0;c=row.p[i++];if(c=='}')break;if(c!=',')return 0;}
    lid_bj_space(row,&i);return i==row.n&&mask==127;
}
static int lid_bj_array_count(LidBagJsonSpan s,unsigned*out){
    size_t i=0;unsigned n=0;unsigned short c;lid_bj_space(s,&i);if(i>=s.n||s.p[i++]!='[')return 0;lid_bj_space(s,&i);
    if(i<s.n&&s.p[i]==']')i++;
    else for(;;){if(++n>99||!lid_bj_value(s,&i,0))return 0;lid_bj_space(s,&i);if(i>=s.n)return 0;c=s.p[i++];if(c==']')break;if(c!=',')return 0;}
    lid_bj_space(s,&i);*out=n;return i==s.n;
}
static int lid_bj_array(LidBagJsonSpan expected,LidBagJsonSpan actual,LidBagJsonSpan uid,LidBagJsonSpan cid,LidBagJsonOut*out){
    unsigned want,have,n=0;size_t i=1,start;LidBagJsonSpan row;unsigned short c;
    if(!lid_bj_array_count(expected,&want)||!lid_bj_array_count(actual,&have)||have<want||!lid_bj_char(out,'['))return 0;
    if(have)for(;;){lid_bj_space(actual,&i);start=i;if(!lid_bj_value(actual,&i,0))return 0;row=(LidBagJsonSpan){actual.p+start,i-start};
        if(n<want){if(n&&!lid_bj_char(out,','))return 0;if(!lid_bj_emit(out,row))return 0;}
        else{if(!lid_bj_empty(row,uid,cid,n))return 0;out->removed++;}
        n++;lid_bj_space(actual,&i);if(i>=actual.n)return 0;c=actual.p[i++];if(c==']')break;if(c!=',')return 0;}
    return lid_bj_char(out,']');
}
static int lid_bj_map(LidBagJsonSpan expected,LidBagJsonSpan actual,LidBagJsonSpan uid,unsigned depth,LidBagJsonOut*out){
    size_t i=0;unsigned n=0,source_count=0,actual_count=0;unsigned short c;LidBagJsonSpan k,v,ev,dummy;
    /* Empty maps do not need extension handling; strict native equality remains authoritative. */
    if(!actual.n||actual.p[i++]!='{'||!lid_bj_char(out,'{'))return 0;lid_bj_space(actual,&i);
    if(i<actual.n&&actual.p[i]=='}')return lid_bj_same(expected,actual)&&lid_bj_char(out,'}');
    for(;;){if(++n>128||!lid_bj_pair(actual,&i,&k,&v)||!lid_bj_find(expected,k,&ev,&source_count)||!lid_bj_find(actual,k,&dummy,&actual_count))return 0;
        if(source_count!=actual_count)return 0;if(n>1&&!lid_bj_char(out,','))return 0;
        if(!lid_bj_emit(out,k)||!lid_bj_char(out,':'))return 0;
        if(depth==0){if(!lid_bj_map(ev,v,k,1,out))return 0;}
        else if(!lid_bj_array(ev,v,uid,k,out))return 0;
        lid_bj_space(actual,&i);if(i>=actual.n)return 0;c=actual.p[i++];if(c=='}')break;if(c!=',')return 0;}
    lid_bj_space(actual,&i);return i==actual.n&&n==source_count&&lid_bj_char(out,'}');
}
static int lid_reload_bag_growth_copy(LidReloadJsonBinding*b,const LidReloadPoolString*expected,
    const LidReloadPoolString*actual,LidReloadPoolString*out,unsigned*removed){
    LidBagJsonOut writer={0};int good;
    if(!out||out->data||!removed||!lid_reload_json_window(b)||!lid_reload_json_string_ok(b,expected)||!lid_reload_json_string_ok(b,actual))return 0;
    writer.cap=(size_t)actual->num+1;writer.p=b->allocate(b->memory_context,writer.cap*2);if(!writer.p)return 0;
    good=lid_bj_map((LidBagJsonSpan){expected->data,(size_t)expected->num-1},(LidBagJsonSpan){actual->data,(size_t)actual->num-1},(LidBagJsonSpan){0},0,&writer);
    if(!good||!writer.removed){b->release(b->memory_context,writer.p);return 0;}
    writer.p[writer.n]=0;*out=(LidReloadPoolString){writer.p,(int)writer.n+1,(int)writer.cap};*removed=writer.removed;return 1;
}
#endif
