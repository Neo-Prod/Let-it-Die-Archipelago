#ifndef LID_RELOAD_JSON_NUMBERS_H
#define LID_RELOAD_JSON_NUMBERS_H
/* Research candidate. Numeric lexical normalization on PRIVATE comparison
   copies ONLY. Never apply this output to game repositories: native int
   readers need not interpret exponents. Caller provides valid UTF16 buffers.
   This is a bounded lexer, NOT a full JSON schema/grammar validator. */
static int lid_reload_json_put(unsigned short*out,size_t cap,size_t*at,unsigned short c){
    if(*at>=cap-1)return 0;out[(*at)++]=c;return 1;
}
static int lid_reload_json_digit(unsigned short c){return c>='0'&&c<='9';}
static int lid_reload_json_end(unsigned short c){return c==','||c==']'||c=='}'||c==' '||c=='\t'||c=='\r'||c=='\n';}
static int lid_reload_json_number(const unsigned short*in,size_t n,size_t*cursor,unsigned short*out,size_t cap,size_t*at){
    size_t i=*cursor,start,mantissa_end,j,digits=0,leading=0,trailing=0,frac=0,emit;
    int negative=0,exponent=0,exp_negative=0,scale;char reversed[16];unsigned v,count;
    if(in[i]=='-'){negative=1;if(++i==n)return 0;}
    start=i;
    if(!lid_reload_json_digit(in[i]))return 0;
    if(in[i]=='0'){i++;if(i<n&&lid_reload_json_digit(in[i]))return 0;}
    else while(i<n&&lid_reload_json_digit(in[i]))i++;
    if(i<n&&in[i]=='.'){
        i++;j=i;while(i<n&&lid_reload_json_digit(in[i]))i++;
        if(i==j)return 0;frac=i-j;
    }
    mantissa_end=i;
    if(i<n&&(in[i]=='e'||in[i]=='E')){
        i++;if(i<n&&(in[i]=='+'||in[i]=='-')){exp_negative=in[i]=='-';i++;}
        j=i;while(i<n&&lid_reload_json_digit(in[i])){
            exponent=exponent*10+(int)(in[i]-'0');if(exponent>4096)return 0;i++;
        }
        if(i==j)return 0;if(exp_negative)exponent=-exponent;
    }
    if(i<n&&!lid_reload_json_end(in[i]))return 0;
    for(j=start;j<mantissa_end;j++)if(in[j]!='.'){
        digits++;if(in[j]=='0'){if(leading==digits-1)leading++;trailing++;}else trailing=0;
    }
    if(leading==digits){if(!lid_reload_json_put(out,cap,at,'0'))return 0;}
    else{
        if(negative&&!lid_reload_json_put(out,cap,at,'-'))return 0;
        emit=digits-leading-trailing;count=0;
        for(j=start;j<mantissa_end&&emit;j++)if(in[j]!='.'){
            if(count++<leading)continue;
            if(!lid_reload_json_put(out,cap,at,in[j]))return 0;emit--;
        }
        scale=exponent-(int)frac+(int)trailing;
        if(scale){
            if(!lid_reload_json_put(out,cap,at,'e'))return 0;
            if(scale<0&&!lid_reload_json_put(out,cap,at,'-'))return 0;
            v=(unsigned)(scale<0?-scale:scale);count=0;
            do{reversed[count++]=(char)('0'+v%10);v/=10;}while(v);
            while(count)if(!lid_reload_json_put(out,cap,at,(unsigned short)reversed[--count]))return 0;
        }
    }
    *cursor=i;return 1;
}
static int lid_reload_json_numbers(const unsigned short*in,size_t n,unsigned short*out,size_t cap,size_t*written){
    size_t i=0,at=0,j,k;unsigned short c;const char*word;int closed;
    if(!in||!out||!written||!cap||n>32u*1024u*1024u||cap>64u*1024u*1024u)return 0;
    /* Disallow overlap, including partial overlap. Sizes bounded above. */
    if((size_t)in<(size_t)out+cap*2&&(size_t)out<(size_t)in+n*2)return 0;
    while(i<n){
        c=in[i];
        if(c=='"'){
            closed=0;
            if(!lid_reload_json_put(out,cap,&at,in[i++]))return 0;
            while(i<n){
                c=in[i++];if(c<0x20||!lid_reload_json_put(out,cap,&at,c))return 0;
                if(c=='"'){closed=1;break;}
                if(c=='\\'){if(i==n||!lid_reload_json_put(out,cap,&at,in[i++]))return 0;}
            }
            if(!closed)return 0;
        }else if(c=='-'||lid_reload_json_digit(c)){
            if(!lid_reload_json_number(in,n,&i,out,cap,&at))return 0;
        }else if(c=='t'||c=='f'||c=='n'){
            word=c=='t'?"true":c=='f'?"false":"null";
            for(j=0;word[j];j++)if(i+j>=n||in[i+j]!=(unsigned short)word[j])return 0;
            k=i+j;if(k<n&&!lid_reload_json_end(in[k]))return 0;
            while(i<k)if(!lid_reload_json_put(out,cap,&at,in[i++]))return 0;
        }else{
            if(!(c=='{'||c=='['||c=='}'||c==']'||c==':'||c==','||c==' '||c=='\t'||c=='\r'||c=='\n'))return 0;
            if(!lid_reload_json_put(out,cap,&at,c))return 0;i++;
        }
    }
    out[at]=0;*written=at;return 1;
}
#endif
