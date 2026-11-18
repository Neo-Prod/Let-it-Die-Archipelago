#ifndef LID_SHOP_MOMOKA_DESCRIPTION_H
#define LID_SHOP_MOMOKA_DESCRIPTION_H
#include "shop_momoka_labels.h"
/* Exact-build native ParseIntoArray copies input and destroys old strings.
   No borrowed UTF16 pointer is ever installed in a game-owned array. */
typedef int(*LidMomokaParsePages)(const M197StringHeader*,void*,const uint16_t*,int);
typedef struct LidMomokaDescription {
    LidMomokaLabels*labels;LidMomokaParsePages parse;
    uint32_t text,category,page,group,mask;unsigned ready;
} LidMomokaDescription;
static int lid_md_prepare(LidMomokaDescription*d,LidMomokaLabels*b,uint64_t image){
    static const uint8_t prefix[24]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x20};
    uint8_t actual[24];uint64_t explain,p,structure,inner;uint32_t at[6],sizes[6]={16,16,4,4,16,16};
    if(!d||d->ready||!b||!b->ready||!image||!lid_ml_current(b,&explain))return 0;
    const M195Ops*o=&b->root->memory;
    if(!m195_read(o,image,0x140260,actual,24))return 0;
    for(unsigned i=0;i<24;i++)if(actual[i]!=prefix[i])return 0;
    if(!m212_member(o,explain,"BrgUIMenu_SkillExplane","mSkillInfo",&p)||
       !m195_read(o,p,0xb0,&structure,8)||
       !m195_named(o,structure,"BrgUIMenu_SkillExplane_SkillInfo")||
       !lid_ml_struct_member(o,structure,"mExplText",&p)||
       !lid_ml_property(o,p,"ArrayProperty",1,16,b->stride,0,&at[0])||
       !m195_read(o,p,0xb0,&inner,8)||
       !lid_ml_property(o,inner,"StrProperty",1,16,16,0,&d->text)||d->text||
       !lid_ml_struct_member(o,structure,"mExplSkillCategoryName",&p)||
       !lid_ml_property(o,p,"StrProperty",1,16,b->stride,0,&at[1])||
       !lid_ml_struct_member(o,structure,"mNowPage",&p)||
       !lid_ml_property(o,p,"IntProperty",1,4,b->stride,0,&at[2])||
       !lid_ml_struct_member(o,structure,"mExplIsHaveGroup",&p)||
       !lid_ml_property(o,p,"BoolProperty",1,4,b->stride,0,&at[3])||
       !m195_read(o,p,0xb0,&d->mask,4)||!d->mask||(d->mask&(d->mask-1)))return 0;
    at[4]=b->skill;at[5]=b->name;
    for(unsigned i=0;i<6;i++)for(unsigned j=0;j<i;j++)if(at[i]<at[j]+sizes[j]&&at[j]<at[i]+sizes[i])return 0;
    d->text=at[0];d->category=at[1];d->page=at[2];d->group=at[3];
    d->labels=b;d->parse=(LidMomokaParsePages)(uintptr_t)(image+0x140260);d->ready=1;return 1;
}
static int lid_md_same(const M195Ops*o,uint64_t string,const uint16_t*value){
    M197StringHeader h;uint16_t old[192];unsigned n=0;
    if(!value)return 0;while(n<192&&value[n])n++;
    if(n==192||!lid_ml_header(o,string,&h))return 0;
    if(!n&&!h.count)return 1;
    if(h.count!=(int32_t)n+1||!m195_read(o,h.data,0,old,(n+1)*2))return 0;
    for(unsigned i=0;i<=n;i++)if(old[i]!=value[i])return 0;return 1;
}
static int lid_md_page(const char*prefix,const uint16_t*value,uint16_t out[192]){
    unsigned n=0,column=0;
    if(!prefix||!value||!out)return 0;
    for(unsigned i=0;prefix[i];i++){if(n>=190)return 0;out[n++]=(uint8_t)prefix[i];column++;}
    for(unsigned i=0;value[i];i++){
        uint16_t c=value[i];unsigned pair=c>=0xd800&&c<=0xdbff;
        if(column>=35&&c!='\n'){if(n>=190)return 0;out[n++]='\n';column=0;}
        if(n>=190)return 0;out[n++]=c;
        if(pair){uint16_t low=value[++i];if(low<0xdc00||low>0xdfff||n>=190)return 0;out[n++]=low;}
        column=c=='\n'?0:column+1;
    }
    out[n]=0;return 1;
}
static int lid_md_array(const M195Ops*o,uint64_t address,M197StringHeader*out){
    M197StringHeader a;
    if(!out||!m195_read(o,address,0,&a,16)||a.count<0||a.count>4||a.capacity<a.count||a.capacity>4096||
       (a.capacity&&!a.data)||(!a.capacity&&a.data)||a.data>UINT64_MAX-(uint64_t)a.capacity*16)return 0;
    for(int i=0;i<a.count;i++){M197StringHeader s;if(!lid_ml_header(o,a.data+(uint64_t)i*16,&s))return 0;}
    *out=a;return 1;
}
/* Called only at R257's already-proven UI event/initial refresh. Maps EACH
   animation record independently, including previous selection. */
static int lid_md_apply(LidMomokaDescription*d,LidMomokaStringAssign assign){
    if(!d||!d->ready||!assign)return -1;
    LidMomokaLabels*b=d->labels;uint64_t explain;
    if(!b||!b->display||!b->display->io.thread_id||b->display->io.thread_id()!=b->display->ui||
       __atomic_load_n(&b->display->state,__ATOMIC_ACQUIRE)!=LID_SD_READY||!lid_ml_current(b,&explain))return -1;
    const M195Ops*o=&b->root->memory;const LidShopRuntimeCatalogue*c=b->display->catalogue;int changed=0;
    if(!c||!__atomic_load_n(&c->initialized,__ATOMIC_ACQUIRE))return -1;
    for(unsigned index=0;index<2;index++){
        uint64_t again,at;M197StringHeader idh,a,after;M195Signature s={0};unsigned units;
        uint16_t id[128],receiver[128],game[128],pages[2][192],body[384],delimiter[2]={0,0};
        static const uint16_t category[]={'A','r','c','h','i','p','e','l','a','g','o',0};
        if(!lid_ml_current(b,&again)||again!=explain||explain>UINT64_MAX-b->info-(uint64_t)index*b->stride)return -1;
        at=explain+b->info+(uint64_t)index*b->stride;
        s.skill_offset=b->skill;s.parms_size=(uint16_t)b->stride;
        if(!m195_read(o,at,b->skill,&idh,16))return -1;if(!idh.count)continue;
        if(!m197_string(o,at,&s,id,&units))return -1;
        unsigned offer=UINT32_MAX;
        for(unsigned i=0;i<c->momoka_count;i++){
            const LidShopUiSku*sku=&c->momoka[i];if(sku->units!=units)continue;
            unsigned j=0;while(j<units&&id[j]==sku->skill[j])j++;if(j==units){offer=sku->offer;break;}
        }
        if(offer==UINT32_MAX)continue;
        if(!lid_sd_text(b->display,7500001ull+offer,1,receiver,128)||
           !lid_sd_text(b->display,7500001ull+offer,2,game,128)||
           !lid_md_page("",receiver,pages[0])||!lid_md_page("Game: ",game,pages[1])||
           !lid_md_array(o,at+d->text,&a))return -1;
        unsigned same=a.count==2&&lid_md_same(o,a.data,pages[0])&&lid_md_same(o,a.data+16,pages[1]);
        if(!same){
            /* Find a delimiter absent from both pages. Exact content survives
               //, Unicode and control characters in remote aliases/game names. */
            for(unsigned candidate=0xe000;candidate<0xe180;candidate++){
                unsigned found=0;for(unsigned p=0;p<2;p++)for(unsigned i=0;pages[p][i];i++)if(pages[p][i]==candidate)found=1;
                if(!found){delimiter[0]=(uint16_t)candidate;break;}
            }
            if(!delimiter[0])return -1;
            unsigned n=0;for(unsigned p=0;p<2;p++){
                if(p)body[n++]=delimiter[0];for(unsigned i=0;pages[p][i];i++)body[n++]=pages[p][i];
            }body[n]=0;
            uint16_t id_after[128];unsigned units_after;
            if(!lid_ml_current(b,&again)||again!=explain||!m197_string(o,at,&s,id_after,&units_after)||units_after!=units||
               !lid_md_array(o,at+d->text,&after)||after.data!=a.data||after.count!=a.count||after.capacity!=a.capacity)return -1;
            for(unsigned i=0;i<units;i++)if(id[i]!=id_after[i])return -1;
            M197StringHeader input={(uint64_t)(uintptr_t)body,(int32_t)n+1,(int32_t)n+1};
            if(d->parse(&input,(void*)(uintptr_t)(at+d->text),delimiter,1)!=2)return -1;
            *(int32_t*)(uintptr_t)(at+d->page)=0;changed++;
        }
        /* Category/group are transient explanation fields, not inventory.
           Preserve every unrelated packed boolean bit. */
        uint32_t flags;M197StringHeader cat;
        if(!lid_ml_current(b,&again)||again!=explain||!m195_read(o,at,d->group,&flags,4)||
           !lid_ml_header(o,at+d->category,&cat))return -1;
        if(!lid_md_same(o,at+d->category,category))assign((void*)(uintptr_t)(at+d->category),category);
        if(!lid_ml_current(b,&again)||again!=explain||!m195_read(o,at,d->group,&flags,4))return -1;
        if(flags&d->mask)*(uint32_t*)(uintptr_t)(at+d->group)=flags&~d->mask;
    }
    return changed;
}
#endif
