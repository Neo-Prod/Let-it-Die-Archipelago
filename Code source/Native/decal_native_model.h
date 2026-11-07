#ifndef LID_DECAL_NATIVE_MODEL_H
#define LID_DECAL_NATIVE_MODEL_H
/* Research candidate. Detached typed reader/verifier; no writes, allocations,
   game calls, hooks, save or delivery authority. Include progression_wire.h
   and decal_catalog.h first. Snapshot buffers belong in caller BSS. */
#define LID_DECAL_MAX_ROWS 4096u
typedef struct LidDecalRow {
    unsigned short id[64];unsigned count,checked;unsigned long long updated;
} LidDecalRow;
typedef struct LidDecalSnapshot {
    LidDecalRow* rows;size_t capacity,count;unsigned valid;
} LidDecalSnapshot;
static unsigned lid_decal_fold(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int lid_decal_compare(const unsigned short*a,const unsigned short*b){
    unsigned i;for(i=0;i<64;i++){int diff=(int)lid_decal_fold(a[i])-(int)lid_decal_fold(b[i]);if(diff)return diff;if(!a[i])return 0;}return 0;
}
static int lid_decal_ascii(const unsigned short*a,const char*b){
    unsigned i;for(i=0;i<64;i++){if(lid_decal_fold(a[i])!=lid_decal_fold((unsigned char)b[i]))return 0;if(!a[i])return 1;}return 0;
}
static unsigned lid_decal_index(const unsigned short*id){
    unsigned i;for(i=0;i<208;i++)if(lid_decal_ascii(id,lid_decal_ids[i]))return i;return 208;
}
static void lid_decal_swap(LidDecalRow*a,LidDecalRow*b){LidDecalRow temp=*a;*a=*b;*b=temp;}
static void lid_decal_sift(LidDecalRow*r,size_t root,size_t count){
    size_t child;while(root<count/2){child=root*2+1;if(child+1<count&&lid_decal_compare(r[child].id,r[child+1].id)<0)child++;
        if(lid_decal_compare(r[root].id,r[child].id)>=0)return;lid_decal_swap(r+root,r+child);root=child;}
}
static int lid_decal_sort_unique(LidDecalSnapshot*s){
    size_t i;for(i=s->count/2;i;i--)lid_decal_sift(s->rows,i-1,s->count);
    for(i=s->count;i>1;i--){lid_decal_swap(s->rows,s->rows+i-1);lid_decal_sift(s->rows,0,i-1);}
    for(i=1;i<s->count;i++)if(!lid_decal_compare(s->rows[i-1].id,s->rows[i].id))return 0;return 1;
}
static int lid_decal_string(unsigned short*out,const BYTE*header,int(*readable)(const void*,size_t)){
    const BYTE* data;unsigned n,max,i,c;
    if(!readable(header,16))return 0;data=(const BYTE*)(size_t)lid_wire_r64(header);n=lid_wire_r32(header+8);max=lid_wire_r32(header+12);
    if(n<2||n>64||n>max||max>0x7fffffffu||!data||!readable(data,(size_t)n*2))return 0;
    for(i=0;i<n;i++){c=data[2*i]|((unsigned)data[2*i+1]<<8);if(i+1==n){if(c)return 0;}
        else if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return 0;out[i]=(unsigned short)c;}
    for(;i<64;i++)out[i]=0;return 1;
}
static int lid_decal_read(LidDecalSnapshot*s,const BYTE*header,const void*expected_row_vtable,
                          unsigned view,int(*readable)(const void*,size_t)){
    const BYTE*data;const BYTE*row;unsigned n,max;size_t i,stride=view?0x1c:0x28;
    if(!s)return 0;s->valid=0;s->count=0;
    if(!s->rows||!s->capacity||s->capacity>LID_DECAL_MAX_ROWS||!header||!readable||view>1||!readable(header,16))return 0;
    data=(const BYTE*)(size_t)lid_wire_r64(header);n=lid_wire_r32(header+8);max=lid_wire_r32(header+12);
    if(n>max||n>s->capacity||max>0x7fffffffu||(n&&(!data||!readable(data,(size_t)n*stride))))return 0;
    for(i=0;i<n;i++){
        LidDecalRow*out=s->rows+i;row=data+i*stride;
        if(!view&&(!expected_row_vtable||lid_wire_r64(row)!=(size_t)expected_row_vtable))return 0;
        if(!lid_decal_string(out->id,row+(view?0:8),readable))return 0;
        if(view&&lid_wire_r32(row+0x10)!=1)return 0;
        out->count=lid_wire_r32(row+(view?0x14:0x18));out->checked=lid_wire_r32(row+(view?0x18:0x24));
        out->updated=view?0:lid_wire_r64(row+0x1c);
        if(out->count>0x7fffffffu)return 0;
    }
    s->count=n;if(!lid_decal_sort_unique(s))return 0;s->valid=1;return 1;
}
static int lid_decal_stock_read(LidDecalSnapshot*s,const BYTE*repo,const void*expected_repo_vtable,
                                const void*expected_row_vtable,int(*readable)(const void*,size_t)){
    if(!s)return 0;s->valid=0;
    if(!repo||!readable||!readable(repo,0x60)||lid_wire_r64(repo)!=(size_t)expected_repo_vtable)return 0;
    return lid_decal_read(s,repo+0x50,expected_row_vtable,0,readable);
}
static int lid_decal_snapshot_valid(const LidDecalSnapshot*s){
    size_t i;if(!s||s->valid!=1||!s->rows||s->capacity>LID_DECAL_MAX_ROWS||!s->capacity||s->count>s->capacity)return 0;
    for(i=0;i<s->count;i++)if(!s->rows[i].id[0]||s->rows[i].count>0x7fffffffu||(i&&lid_decal_compare(s->rows[i-1].id,s->rows[i].id)>=0))return 0;return 1;
}
static int lid_decal_row_same(const LidDecalRow*a,const LidDecalRow*b,unsigned view){
    unsigned i;for(i=0;i<64;i++)if(a->id[i]!=b->id[i])return 0;
    return a->count==b->count&&a->checked==b->checked&&(view||a->updated==b->updated);
}
static int lid_decal_unchanged(const LidDecalSnapshot*a,const LidDecalSnapshot*b,unsigned view){
    size_t i;if(!lid_decal_snapshot_valid(a)||!lid_decal_snapshot_valid(b)||a->count!=b->count)return 0;
    for(i=0;i<a->count;i++)if(!lid_decal_row_same(a->rows+i,b->rows+i,view))return 0;return 1;
}
/* Validate all existing rows, including vanilla IDs outside AP. Unknown rows
   cannot disappear/change, newly added unknown rows are never accepted. */
static int lid_decal_delta_verified(const LidDecalSnapshot*before,const LidDecalSnapshot*after,const unsigned*delta){
    size_t i=0,k=0;unsigned seen[208]={0},index,d;unsigned long long total=0;
    if(!lid_decal_snapshot_valid(before)||!lid_decal_snapshot_valid(after)||!delta)return 0;
    for(index=0;index<208;index++){if(delta[index]>100000)return 0;total+=delta[index];}if(total>100000)return 0;
    while(k<after->count){
        const LidDecalRow*row=after->rows+k;
        if(i<before->count&&lid_decal_compare(before->rows[i].id,row->id)<0)return 0;
        index=lid_decal_index(row->id);d=index<208?delta[index]:0;
        if(i<before->count&&!lid_decal_compare(before->rows[i].id,row->id)){
            LidDecalRow expected=before->rows[i];
            if(expected.count>0x7fffffffu-d)return 0;expected.count+=d;
            if(!lid_decal_row_same(&expected,row,0))return 0;i++;
        }else if(index>=208||!d||row->count!=d||row->checked)return 0;
        if(index<208)seen[index]=1;k++;
    }
    if(i!=before->count)return 0;
    for(index=0;index<208;index++)if(delta[index]&&!seen[index])return 0;return 1;
}
#endif
