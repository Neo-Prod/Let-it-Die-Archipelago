#include "status_scan_fast.h"
#include <stdint.h>
#include <emmintrin.h>
/* Necessary HP condition only. Exact row and ten-field checks remain below.
   SSE2 is part of the x86-64 baseline. No speculative read beyond 16 bytes. */
static int possible_hp_block(uintptr_t address,int width64){
    __m128i values=_mm_loadu_si128((const __m128i*)address);
    __m128i possible=_mm_and_si128(_mm_cmpgt_epi32(values,_mm_set1_epi32(199)),
                                  _mm_cmpgt_epi32(_mm_set1_epi32(26671),values));
    if(width64){
        __m128i high_zero=_mm_cmpeq_epi32(_mm_srli_epi64(values,32),_mm_setzero_si128());
        possible=_mm_and_si128(possible,high_zero);
        return _mm_movemask_ps(_mm_castsi128_ps(possible))&5;
    }
    return _mm_movemask_ps(_mm_castsi128_ps(possible));
}
static int grade6_row(int hp){
    int d;
    if(hp>=2800&&hp<=11790){d=hp-2800;return d%310?-1:d/310;}
    if(hp>=12720&&hp<=21090){d=hp-12720;return d%930?-1:30+d/930;}
    if(hp>=22950&&hp<=26670){d=hp-22950;return d%930?-1:40+d/930;}
    return -1;
}
static int grade1_row(int hp){return hp>=200&&hp<=320&&(hp-200)%30==0?(hp-200)/30:-1;}
static int match32(const int*p,const int*v){int j;for(j=0;j<10;j++)if(p[j]!=v[j])return 0;return 1;}
static int match64(const long long*p,const int*v){int j;for(j=0;j<10;j++)if(p[j]!=(long long)v[j])return 0;return 1;}
unsigned lid_master_scan_region(void*base,size_t size,const LidMasterScanTables*t){
    uintptr_t p,end,start=(uintptr_t)base;unsigned matches=0;int row,grade,j;
    const int*target;long long hp;
    if(!base||size<40||size>UINTPTR_MAX-start||!t||!t->grade6||!t->grade1||!t->target6||!t->target1||!t->mask||t->required<1||t->required>45)return 0;
    end=start+size;p=(start+3u)&~(uintptr_t)3u;
    while(end-p>=40){
        if(end-p>=52&&!possible_hp_block(p,0)){p+=16;continue;}
        grade=6;row=grade6_row(*(int*)p);
        if(!(row>=0&&row<t->required&&match32((const int*)p,t->grade6[row]))){
            grade=1;row=grade1_row(*(int*)p);if(!(row>=0&&match32((const int*)p,t->grade1[row]))){p+=4;continue;}}
        target=grade==6?t->target6[row]:t->target1[row];for(j=0;j<10;j++)((int*)p)[j]=target[j];
        matches++;if(grade==6)*t->mask|=1ULL<<(unsigned)row;
        if(t->matched)t->matched(t->context,32,grade,row,(void*)p);p+=40;
    }
    p=(start+7u)&~(uintptr_t)7u;
    while(end-p>=80){
        if(end-p>=88&&!possible_hp_block(p,1)){p+=16;continue;}
        hp=*(long long*)p;row=hp>=INT32_MIN&&hp<=INT32_MAX?grade6_row((int)hp):-1;grade=6;
        if(!(row>=0&&row<t->required&&match64((const long long*)p,t->grade6[row]))){
            grade=1;row=hp>=INT32_MIN&&hp<=INT32_MAX?grade1_row((int)hp):-1;
            if(!(row>=0&&match64((const long long*)p,t->grade1[row]))){p+=8;continue;}}
        target=grade==6?t->target6[row]:t->target1[row];for(j=0;j<10;j++)((long long*)p)[j]=(long long)target[j];
        matches++;if(grade==6)*t->mask|=1ULL<<(unsigned)row;
        if(t->matched)t->matched(t->context,64,grade,row,(void*)p);p+=80;
    }
    return matches;
}
