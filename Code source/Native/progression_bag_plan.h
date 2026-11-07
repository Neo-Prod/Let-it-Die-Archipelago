#ifndef LID_PROGRESSION_BAG_PLAN_H
#define LID_PROGRESSION_BAG_PLAN_H

/* Candidate only. Include winlite.h first. No game addresses or engine calls.
   Host must reserve workers, validate object generations and supply correct
   entry ownership/occupancy callbacks. Not wired to the installed mod. */
typedef struct __attribute__((packed,aligned(4))) LidBagArray {
    void* data;
    int num;
    int capacity;
} LidBagArray;
typedef char LidBagArraySize[(sizeof(LidBagArray)==16)?1:-1];

typedef struct LidBagOps {
    void* (WINAPI *allocate)(size_t);
    void (WINAPI *release)(void*);
    int (WINAPI *readable)(const void*,size_t);
    int (WINAPI *valid_entry)(const void*,void*);
    int (WINAPI *empty_entry)(const void*,void*);
    /* A failed constructor must leave an entry safe for destroy_entry. */
    int (WINAPI *construct_entry)(void*,int index,void*);
    void (WINAPI *destroy_entry)(void*,void*);
} LidBagOps;

typedef struct LidBagPlan {
    LidBagArray* array;
    LidBagArray original;
    LidBagOps ops;
    void* context;
    void* replacement;
    size_t stride;
    int target;
    int initialized_end;
    int ready;
} LidBagPlan;

static void lid_bag_bytes(void* dst,const void* src,size_t n){
    BYTE* d=(BYTE*)dst;const BYTE* s=(const BYTE*)src;
    while(n--)*d++=*s++;
}
static void lid_bag_zero(void* dst,size_t n){BYTE* d=(BYTE*)dst;while(n--)*d++=0;}
static int lid_bag_equal(const void* a,const void* b,size_t n){
    const BYTE* x=a;const BYTE* y=b;while(n--)if(*x++!=*y++)return 0;return 1;
}

static void lid_bag_abort(LidBagPlan* plan){
    int i;
    if(!plan||!plan->ready)return;
    if(plan->replacement){
        for(i=plan->original.num;i<plan->initialized_end;i++)
            plan->ops.destroy_entry((BYTE*)plan->replacement+(size_t)i*plan->stride,plan->context);
        plan->ops.release(plan->replacement);
    }
    plan->replacement=0;plan->ready=0;
}

static int lid_bag_prepare(LidBagPlan* plan,LidBagArray* array,int target,
                           size_t stride,const LidBagOps* ops,void* context){
    int i,retained;
    size_t size;
    if(!plan||plan->ready||!ops||!array||target<1||target>=100||!stride||stride>0x2000)return 0;
    if(!ops->allocate||!ops->release||!ops->readable||!ops->valid_entry||
       !ops->empty_entry||!ops->construct_entry||!ops->destroy_entry)return 0;
    if(!ops->readable(array,sizeof(*array)))return 0;
    if(array->num<0||array->num>99||array->capacity<array->num||array->capacity>4096)return 0;
    if((!array->data)!=(array->capacity==0))return 0;
    if(array->num&&(!array->data||!ops->readable(array->data,(size_t)array->num*stride)))return 0;
    for(i=0;i<array->num;i++){
        const void* entry=(BYTE*)array->data+(size_t)i*stride;
        if(!ops->valid_entry(entry,context))return 0;
        if(i>=target&&!ops->empty_entry(entry,context))return 0;
    }
    plan->array=array;plan->original=*array;plan->ops=*ops;
    plan->context=context;plan->stride=stride;plan->target=target;
    plan->replacement=0;plan->initialized_end=array->num;plan->ready=1;
    if(target==array->num)return 1; /* Allocation capacity is not active size. */
    size=(size_t)target*stride;
    plan->replacement=ops->allocate(size);
    if(!plan->replacement){plan->ready=0;return 0;}
    if(plan->replacement==array->data){plan->replacement=0;plan->ready=0;return 0;}
    lid_bag_zero(plan->replacement,size);
    retained=target<array->num?target:array->num;
    if(retained)lid_bag_bytes(plan->replacement,array->data,(size_t)retained*stride);
    for(i=array->num;i<target;i++){
        void* entry=(BYTE*)plan->replacement+(size_t)i*stride;
        plan->initialized_end=i+1;
        if(!ops->construct_entry(entry,i,context)||!ops->valid_entry(entry,context)){
            lid_bag_abort(plan);return 0;
        }
    }
    return 1;
}

/* Validate the whole set BEFORE ANY header write. On refusal caller must abort
   its prepared plans. This is atomic only relative to the host's reservation,
   never a substitute for the engine locks or player-generation check. */
static int lid_bag_commit_all(LidBagPlan* plans,int count){
    int i,j,k;
    if(!plans||count<1||count>4)return 0;
    for(i=0;i<count;i++){
        LidBagPlan* p=&plans[i];LidBagArray* a=p->array;
        if(!p->ready||!a||!p->ops.readable(a,sizeof(*a)))return 0;
        if(a->data!=p->original.data||a->num!=p->original.num||a->capacity!=p->original.capacity)return 0;
        if(a->num&&!p->ops.readable(a->data,(size_t)a->num*p->stride))return 0;
        for(k=0;k<a->num;k++){
            const void* entry=(BYTE*)a->data+(size_t)k*p->stride;
            if(!p->ops.valid_entry(entry,p->context))return 0;
            if(k>=p->target&&!p->ops.empty_entry(entry,p->context))return 0;
        }
        if(p->replacement){
            int retained=p->target<a->num?p->target:a->num;
            if(retained&&!lid_bag_equal(p->replacement,a->data,(size_t)retained*p->stride))return 0;
        }
        for(j=0;j<i;j++){
            if(plans[j].array==a)return 0;
            if(a->data&&a->data==plans[j].original.data)return 0;
            if(p->replacement&&p->replacement==plans[j].replacement)return 0;
        }
    }
    for(i=0;i<count;i++){
        LidBagPlan* p=&plans[i];
        if(p->replacement){p->array->data=p->replacement;p->array->num=p->target;p->array->capacity=p->target;}
    }
    /* Ownership of retained nested allocations has MOVED to replacement.
       Destroy only removed original entries, never the retained copies. */
    for(i=0;i<count;i++){
        LidBagPlan* p=&plans[i];
        if(p->replacement){
            for(k=p->target;k<p->original.num;k++)
                p->ops.destroy_entry((BYTE*)p->original.data+(size_t)k*p->stride,p->context);
            if(p->original.data)p->ops.release(p->original.data);
        }
        p->replacement=0;p->ready=0;
    }
    return 1;
}

#endif
