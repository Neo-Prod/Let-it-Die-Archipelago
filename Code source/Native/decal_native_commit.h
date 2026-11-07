#ifndef LID_DECAL_NATIVE_COMMIT_H
#define LID_DECAL_NATIVE_COMMIT_H
/* Research candidate only. Inert adapter callbacks in tests; no game ABI or
   addresses here. RAM readback is NEVER an own-save/SAVED authority. */
typedef struct LidDecalCommitOps {
    void*context;int(*guard)(void*);
    int(*stock)(void*,LidDecalSnapshot*);int(*view)(void*,LidDecalSnapshot*);
    int(*grant)(void*,const unsigned*);int(*project)(void*);
} LidDecalCommitOps;
typedef struct LidDecalCommit {
    const LidDecalSnapshot*before;LidDecalSnapshot*after,*scratch;
    LidDecalCommitOps ops;unsigned delta[208];
    unsigned prepared,attempted,started,applied,stopped;
} LidDecalCommit;
static int lid_decal_separate(const LidDecalSnapshot*a,const LidDecalSnapshot*b){
    size_t x,y;if(!a||!b||!a->rows||!b->rows||!a->capacity||!b->capacity||a->capacity>4096||b->capacity>4096)return 0;
    x=(size_t)a->rows;y=(size_t)b->rows;return x<=y?y-x>=a->capacity*sizeof(*a->rows):x-y>=b->capacity*sizeof(*b->rows);
}
static int lid_decal_commit_prepare(LidDecalCommit*job,const LidDecalSnapshot*before,LidDecalSnapshot*after,
                                    LidDecalSnapshot*scratch,const unsigned*delta,const LidDecalCommitOps*ops){
    unsigned i,index,seen[208]={0};size_t n;unsigned long long sum=0;
    if(!job||job->prepared||job->attempted||!lid_decal_snapshot_valid(before)||!delta||!ops||!ops->guard||!ops->stock||!ops->view||!ops->grant||!ops->project||
       !lid_decal_separate(before,after)||!lid_decal_separate(before,scratch)||!lid_decal_separate(after,scratch))return 0;
    for(i=0;i<208;i++){if(delta[i]>100000)return 0;sum+=delta[i];}if(sum>100000)return 0;
    for(n=0;n<before->count;n++){index=lid_decal_index(before->rows[n].id);if(index<208){seen[index]=1;if(before->rows[n].count>0x7fffffffu-delta[index])return 0;}}
    n=before->count;for(i=0;i<208;i++)if(delta[i]&&!seen[i])n++;
    if(n>after->capacity||n>scratch->capacity)return 0;
    job->before=before;job->after=after;job->scratch=scratch;job->ops=*ops;
    for(i=0;i<208;i++)job->delta[i]=delta[i];job->prepared=1;return 1;
}
static int lid_decal_commit_run(LidDecalCommit*job){
    unsigned i,total=0;
    if(!job||job->prepared!=1||job->attempted||job->started||job->applied||job->stopped)return 0;
    job->attempted=1;
    if(job->ops.guard(job->ops.context)!=1||job->ops.stock(job->ops.context,job->after)!=1||!lid_decal_unchanged(job->before,job->after,0))return 0;
    for(i=0;i<208;i++)total+=job->delta[i];
    if(total){
        job->started=1;
        if(job->ops.guard(job->ops.context)!=1||job->ops.grant(job->ops.context,job->delta)!=1)goto uncertain;
    }
    if(job->ops.guard(job->ops.context)!=1||job->ops.stock(job->ops.context,job->after)!=1||
       !lid_decal_delta_verified(job->before,job->after,job->delta)||
       job->ops.project(job->ops.context)!=1||job->ops.view(job->ops.context,job->scratch)!=1||
       !lid_decal_unchanged(job->after,job->scratch,1))goto uncertain;
    job->applied=1;return 1;
uncertain:job->stopped=1;return -1;
}
static int lid_decal_commit_verify(LidDecalCommit*job){
    return job&&job->applied==1&&!job->stopped&&job->ops.guard(job->ops.context)==1&&
        job->ops.stock(job->ops.context,job->scratch)==1&&lid_decal_unchanged(job->after,job->scratch,0)&&
        job->ops.view(job->ops.context,job->scratch)==1&&lid_decal_unchanged(job->after,job->scratch,1);
}
#endif
