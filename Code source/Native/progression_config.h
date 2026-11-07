#ifndef LID_PROGRESSION_CONFIG_H
#define LID_PROGRESSION_CONFIG_H
/* Canonical nine AP tracks (start,increase,cap), matching CharacterProgression.
   Pure conversion only. Output is committed only on complete success. */
typedef struct LidCharacterConfig {int tracks[9][3];} LidCharacterConfig;
static int lid_config_valid(const LidCharacterConfig* c){
    long long sum=1;int i;
    if(!c)return 0;
    for(i=0;i<9;i++){
        int start=c->tracks[i][0],step=c->tracks[i][1],cap=c->tracks[i][2];
        if(start<0||step<0||cap<0||cap>=2147483647)return 0;
        if((long long)start+(long long)step*cap>2147483647)return 0;
        sum+=cap;
    }
    return sum<=2147483647;
}
static size_t lid_config_json(const LidCharacterConfig* c,char* out,size_t capacity){
    char buffer[384],digits[10];size_t n=0,k;int i,j;unsigned v;
    if(!out||!capacity)return 0;
    out[0]=0;if(!lid_config_valid(c))return 0;
    buffer[n++]='[';
    for(i=0;i<9;i++){
        if(i)buffer[n++]=',';buffer[n++]='[';
        for(j=0;j<3;j++){
            if(j)buffer[n++]=',';
            v=(unsigned)c->tracks[i][j];k=0;
            do{digits[k++]=(char)('0'+v%10);v/=10;}while(v);
            while(k)buffer[n++]=digits[--k];
        }
        buffer[n++]=']';
    }
    buffer[n++]=']';if(n>=capacity)return 0;
    for(k=0;k<n;k++)out[k]=buffer[k];out[n]=0;return n;
}
static int lid_config_targets(const LidCharacterConfig* c,const unsigned* counts,
                              int* absolute,int* levels){
    static const int native_order[9]={1,5,2,3,4,6,8,7,9};
    int a[9],l[10]={1,1,1,1,1,1,1,0,0,0},i;unsigned gain;
    if(!counts||!absolute||!levels||!lid_config_valid(c))return 0;
    for(i=0;i<9;i++){
        if(counts[i]>100000)return 0;
        gain=counts[i]<(unsigned)c->tracks[i][2]?counts[i]:(unsigned)c->tracks[i][2];
        a[i]=(int)((long long)c->tracks[i][0]+(long long)c->tracks[i][1]*gain);
        l[native_order[i]]+=(int)gain;l[0]+=(int)gain;
    }
    for(i=0;i<9;i++)absolute[i]=a[i];for(i=0;i<10;i++)levels[i]=l[i];return 1;
}
#endif
