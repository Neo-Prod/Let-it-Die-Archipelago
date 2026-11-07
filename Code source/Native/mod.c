int _fltused = 0;
#include "winlite.h"
#include "rage_policy.h"
#if LID_EVENT_PROFILE_ENABLED
static void lid_perf_detail_count(unsigned,unsigned long long);
#define LID_PERF_DETAIL(id,n) lid_perf_detail_count(id,n)
#endif
#include "progression_events.h"
#include "progression_runtime_mutex.h"
#include "progression_config.h"
#include "progression_wire.h"
#include "progression_host.h"
#include "progression_bridge_io.h"
#include "progression_bag_plan.h"
#include "progression_game_bag.h"
#include "progression_models.h"
#include "progression_character_apply.h"
#include "progression_reservation.h"
#include "progression_save_job.h"
#include "progression_transaction.h"
#ifndef LID_BLUEPRINT_DELIVERY_ENABLED
#define LID_BLUEPRINT_DELIVERY_ENABLED 0
#endif
#ifndef LID_DECAL_DELIVERY_ENABLED
#define LID_DECAL_DELIVERY_ENABLED 0
#endif
#if LID_DECAL_DELIVERY_ENABLED && !LID_BLUEPRINT_DELIVERY_ENABLED
#error Decal delivery requires common blueprint entry/save
#endif
#if LID_BLUEPRINT_DELIVERY_ENABLED
#include "blueprint_catalog.h"
#include "blueprint_wire.h"
#include "blueprint_plan.h"
#include "blueprint_native_model.h"
#include "blueprint_native_commit.h"
#include "blueprint_engine_adapter.h"
#include "blueprint_waiting_reader.h"
#include "blueprint_pair_gate.h"
#include "blueprint_bridge_reader.h"
#include "blueprint_transaction_domain.h"
#if LID_DECAL_DELIVERY_ENABLED
#include "decal_catalog.h"
#include "decal_ledger.h"
#include "decal_files.h"
#include "decal_native_model.h"
#include "decal_native_commit.h"
#include "decal_engine_adapter.h"
#endif
#endif
#ifndef LID_CHARACTER_DELIVERY_ENABLED
#define LID_CHARACTER_DELIVERY_ENABLED 0
#endif

/* ---------------- UE3 menu structures ---------------- */
typedef struct RawArray { void* data; int num; int max; } RawArray;
typedef struct RawFString { wchar_t* data; int num; int max; } RawFString;
typedef RawFString* (WINAPI *NativeSavedataDirFn)(RawFString*);

typedef int (WINAPI *ProxyActivateOverlayA)(const char*);
typedef int (WINAPI *ProxyGetInt)(void);

typedef void (WINAPI *ExecFunction)(void*,void*,void*);
typedef void (WINAPI *NativeSetUnitArray)(void*,RawArray*);
typedef void (WINAPI *NativeStartCommonTop2)(void*,BYTE,RawArray*,int,int,int);
typedef void (WINAPI *NativeInitialize)(void*);
typedef int (WINAPI *NativeUserDataBool)(void*);
typedef void* (WINAPI *NativeUserDataPtr)(void*);
typedef int (WINAPI *NativeUserDataIntArg)(void*,int);
typedef int (WINAPI *NativeUserDataNoArg)(void*);
typedef void* (WINAPI *StaticFindObjectFn)(void*,void*,const wchar_t*,int);
typedef void (WINAPI *NativeSetTopMenu)(void*,BYTE);
typedef void (WINAPI *NativeChangeMenuName)(void*,RawFString*,BYTE);
typedef void (WINAPI *NativeResetChoice)(void*);
typedef int (WINAPI *NativeCheckEnd)(void*);
typedef void (WINAPI *NativeSetSelectIndex)(void*,int);
typedef int (WINAPI *NativeGetSelectIndex)(void*);
typedef int (WINAPI *NativeGetUnitNum)(void*);

static HMODULE g_self;
static void** g_vtable;
static NativeSetUnitArray g_set_units;
static NativeStartCommonTop2 g_start_common;
static NativeInitialize g_native_initialize;
static NativeSetTopMenu g_native_set_top;
static NativeChangeMenuName g_change_name;
static NativeResetChoice g_reset_choice;
static NativeCheckEnd g_check_end;
static NativeSetSelectIndex g_set_index;
static NativeGetSelectIndex g_get_index;
static NativeGetUnitNum g_get_num;
static ExecFunction g_exec_set_units;
static ExecFunction g_exec_change_name;
static ExecFunction g_exec_start;
static ExecFunction g_exec_initialize;
static ExecFunction g_exec_set_top;

static void* g_title_self;
static int g_title_injected;
static void* g_pending_self;
static RawArray g_pending_array;
static void* g_pending_block;
static volatile LONG g_dialog_open;
static int g_scan_log_budget=24;

static wchar_t g_server[512];
static wchar_t g_player[256];
static wchar_t g_password[256];

static void inject(void*self,RawArray*src);
static int show_dialog(void);

/* ---------------- Logging ---------------- */
static void game_dir(wchar_t* out,size_t cap){size_t i,n;out[0]=0;if(!g_win.GetModuleFileNameW)return;g_win.GetModuleFileNameW(0,out,(DWORD)cap);n=lid_wcslen(out);for(i=n;i>0;i--){if(out[i-1]==L'\\'||out[i-1]==L'/'){out[i-1]=0;return;}}}
static void log_ascii(const char* text){wchar_t p[700];HANDLE h;DWORD wrote=0;size_t n;if(!g_win.CreateFileW)return;game_dir(p,700);lid_wappend(p,700,L"\\Archipelago");g_win.CreateDirectoryW(p,0);lid_wappend(p,700,L"\\mod.log");h=g_win.CreateFileW(p,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return;n=lid_strlen(text);g_win.WriteFile(h,text,(DWORD)n,&wrote,0);g_win.WriteFile(h,"\r\n",2,&wrote,0);g_win.CloseHandle(h);}

/* ---------------- PE resolver for game reflected execs ---------------- */
typedef struct SecRange { BYTE* begin; size_t size; } SecRange;
static DWORD r32(const BYTE*p){return *(const DWORD*)p;} static WORD r16(const BYTE*p){return *(const WORD*)p;}
static int section_name_eq(const BYTE* raw,const char* wanted){int k;for(k=0;k<8;k++){char a=(char)raw[k];char b=wanted[k];if(a!=b)return 0;if(b==0)return 1;}return wanted[8]==0;}
static int section(void* module,const char* wanted,SecRange*out){BYTE*b=(BYTE*)module,*nt,*s;DWORD pe;WORD count,opt;int i;if(!b||!wanted||!out||r16(b)!=0x5A4D)return 0;pe=r32(b+0x3c);nt=b+pe;if(r32(nt)!=0x4550)return 0;count=r16(nt+6);opt=r16(nt+20);s=nt+24+opt;for(i=0;i<count;i++,s+=40){DWORD vsize,rawsize;if(!section_name_eq(s,wanted))continue;out->begin=b+r32(s+12);vsize=r32(s+8);rawsize=r32(s+16);/* Reflected native tables and strings are file-backed. Prefer raw size so .data BSS does not force a 200+ MB scan. */out->size=(size_t)(rawsize?rawsize:vsize);return out->begin&&out->size;}return 0;}
static void* find_ascii(void*module,const char*txt){SecRange r;size_t len=lid_strlen(txt),i;if(!section(module,".rdata",&r))return 0;for(i=0;i+len+1<=r.size;i++)if(lid_memcmp(r.begin+i,txt,len)==0&&r.begin[i+len]==0)return r.begin+i;return 0;}
static int executable_ptr(void*module,void*p){SecRange r;BYTE*v=(BYTE*)p;if(!section(module,".text",&r))return 0;return v>=r.begin&&v<r.begin+r.size;}
static void* resolve_exec(void*module,const char*name){void*n=find_ascii(module,name);SecRange d;size_t off;if(!n||!section(module,".data",&d))return 0;for(off=0;off+16<=d.size;off+=8){unsigned long long*pair=(unsigned long long*)(d.begin+off);if((void*)(ULONG_PTR)pair[0]==n){void*f=(void*)(ULONG_PTR)pair[1];if(executable_ptr(module,f))return f;}}return 0;}

/* ---------------- Simple x64 hooks ---------------- */
typedef struct Hook { BYTE* target; void* trampoline; size_t size; BYTE original[32]; } Hook;
static Hook h_set,h_name,h_start,h_initialize,h_set_top;

static Hook h_title_base_tick,h_title_base_confirm;
static Hook h_savedata_dir;
static NativeSavedataDirFn g_savedata_dir_original;
static volatile LONG g_savedata_route_active=0;
static volatile LONG g_v068_savedata_trace_calls=0;
static ExecFunction g_exec_title_base_tick;
static ExecFunction g_exec_title_base_confirm;

static void* g_direct_title_self;
static RawArray* g_direct_title_menu;
static RawArray* g_direct_title_states;
static int* g_direct_title_select;
static void* g_direct_title_menu_block;
static void* g_direct_title_state_block;
static void* g_direct_title_guide_block;
static int g_direct_title_injected;
static int g_direct_tick_logged;
static int g_direct_confirm_logged;

/* V0.32: action hook is deliberately independent from button injection.
   It is installed lazily only AFTER the exact title has already been
   recognized and the AP label has been published successfully. */
static NativeCheckEnd g_direct_check_end_orig;
static NativeResetChoice g_direct_reset_choice;
static void** g_direct_check_end_slot;
static void** g_direct_action_vtable;
static int g_direct_check_end_hooked;

/* ---------------- V0.24 game allocator bridge ----------------
   UE3 owns the memory referenced by its live TArray/FString descriptors.
   Those allocations must come from the same TBB allocator as the game.
   VirtualAlloc-backed FString buffers are not safe because UE3/tbbmalloc
   may later free each FString buffer independently. */
typedef void* (*TbbMallocFn)(size_t);
typedef void (*TbbFreeFn)(void*);
static TbbMallocFn g_tbb_malloc;
static TbbFreeFn g_tbb_free;
static void* g_tbb_module;

static int init_tbb_allocator(void){
    void* probe;
    g_tbb_module=lid_find_module_ascii("tbbmalloc.dll");
    if(!g_tbb_module && g_win.LoadLibraryW)
        g_tbb_module=g_win.LoadLibraryW(L"tbbmalloc.dll");
    if(!g_tbb_module){
        log_ascii("V0.27 ALLOC: tbbmalloc.dll not found; title injection disabled.");
        return 0;
    }
    g_tbb_malloc=(TbbMallocFn)lid_get_export(g_tbb_module,"scalable_malloc");
    g_tbb_free=(TbbFreeFn)lid_get_export(g_tbb_module,"scalable_free");
    if(!g_tbb_malloc||!g_tbb_free){
        log_ascii("V0.27 ALLOC: scalable_malloc/scalable_free exports not found; title injection disabled.");
        g_tbb_malloc=0;g_tbb_free=0;
        return 0;
    }
    probe=g_tbb_malloc(64);
    if(!probe){
        log_ascii("V0.27 ALLOC: tbbmalloc self-test allocation failed; title injection disabled.");
        return 0;
    }
    lid_memset(probe,0xA5,64);
    g_tbb_free(probe);
    log_ascii("V0.27 ALLOC: using game tbbmalloc scalable_malloc/scalable_free; self-test passed.");
    return 1;
}

/* Resolves/tests the game allocator only after the exact live title menu has
   been identified. This avoids touching tbbmalloc during the very early DLL
   worker startup while preserving the allocator required by UE3-owned arrays. */
static int ensure_title_allocator(void){
    if(g_tbb_malloc&&g_tbb_free)return 1;
    log_ascii("V0.40 ALLOC: exact four-entry title found; initializing game tbbmalloc now.");
    return init_tbb_allocator();
}

/* V0.22: V0.22 reste la base. On conserve les descripteurs vanilla afin de
   les restaurer AVANT que le code du jeu traite un choix normal. */
static RawArray g_vanilla_menu_desc;
static RawArray g_vanilla_state_desc;
static RawArray g_vanilla_guide_desc;
static RawArray* g_direct_title_guides;
static int g_vanilla_desc_valid;
static int g_guide_desc_patched;
static int g_suspend_reinjection;
static int g_v22_post_tick_enter_logged;
static int g_v22_post_tick_exit_logged;
static void* g_v22_locked_title_self;
static int g_v22_title_locked;

/* V0.27: no visual/logical bridge. The engine owns mSelectIndex directly:
   vanilla buttons remain 0..3 and AP is index 4. */
/* V0.45: BrgUIMenu_Title.mMenuEntries is proven for this executable at +0x3D0.
   Never rediscover it by scanning UObject memory. */
static size_t g_v26_menu_offset=0x3D0;
static int g_v26_offset_known=1;
static unsigned g_v26_title_generation;


/* ---------------- V0.23 crash diagnostics ----------------
   V0.22 menu mutation is intentionally unchanged in this build. */
typedef struct LID_EXCEPTION_RECORD {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    struct LID_EXCEPTION_RECORD* ExceptionRecord;
    void* ExceptionAddress;
    DWORD NumberParameters;
    ULONG_PTR ExceptionInformation[15];
} LID_EXCEPTION_RECORD;

typedef struct LID_EXCEPTION_POINTERS {
    LID_EXCEPTION_RECORD* ExceptionRecord;
    void* ContextRecord;
} LID_EXCEPTION_POINTERS;

typedef LONG (WINAPI *LidVectoredHandler)(LID_EXCEPTION_POINTERS*);
typedef void* (WINAPI *AddVectoredExceptionHandlerFn)(ULONG_PTR,LidVectoredHandler);
typedef ULONG_PTR (WINAPI *RemoveVectoredExceptionHandlerFn)(void*);
typedef LidVectoredHandler (WINAPI *SetUnhandledExceptionFilterFn)(LidVectoredHandler);
typedef DWORD (WINAPI *GetCurrentThreadIdFn)(void);

typedef struct LID_MINIDUMP_EXCEPTION_INFORMATION {
    DWORD ThreadId;
    LID_EXCEPTION_POINTERS* ExceptionPointers;
    BOOL ClientPointers;
} LID_MINIDUMP_EXCEPTION_INFORMATION;

typedef BOOL (WINAPI *MiniDumpWriteDumpFn)(
    HANDLE,DWORD,HANDLE,DWORD,LID_MINIDUMP_EXCEPTION_INFORMATION*,void*,void*);

static AddVectoredExceptionHandlerFn g_add_veh;
static RemoveVectoredExceptionHandlerFn g_remove_veh;
static SetUnhandledExceptionFilterFn g_set_uef;
static GetCurrentThreadIdFn g_get_tid;
static MiniDumpWriteDumpFn g_minidump;
static HMODULE g_dbghelp;
static void* g_veh_handle;
static volatile LONG g_crash_captured;
static unsigned g_v23_title_tick_count;
static void jump14(BYTE*d,void*t){d[0]=0xff;d[1]=0x25;d[2]=d[3]=d[4]=d[5]=0;*(unsigned long long*)(d+6)=(unsigned long long)(ULONG_PTR)t;}
static int hook_install(Hook*h,void*target,void*detour,const BYTE*prefix,size_t size){BYTE*tr;DWORD old=0,ig=0;size_t i;if(!target||!detour||size<14||size>32)return 0;if(lid_memcmp(target,prefix,size)!=0)return 0;tr=(BYTE*)g_win.VirtualAlloc(0,size+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tr)return 0;lid_memcpy(h->original,target,size);lid_memcpy(tr,target,size);jump14(tr+size,(BYTE*)target+size);if(!g_win.VirtualProtect(target,size,PAGE_EXECUTE_READWRITE,&old))return 0;jump14((BYTE*)target,detour);for(i=14;i<size;i++)((BYTE*)target)[i]=0x90;g_win.FlushInstructionCache(g_win.GetCurrentProcess(),target,size);g_win.VirtualProtect(target,size,old,&ig);h->target=(BYTE*)target;h->trampoline=tr;h->size=size;return 1;}
static void hook_remove(Hook*h){DWORD old=0,ig=0;if(!h->target)return;if(g_win.VirtualProtect(h->target,h->size,PAGE_EXECUTE_READWRITE,&old)){lid_memcpy(h->target,h->original,h->size);g_win.FlushInstructionCache(g_win.GetCurrentProcess(),h->target,h->size);g_win.VirtualProtect(h->target,h->size,old,&ig);}if(h->trampoline)g_win.VirtualFree(h->trampoline,0,MEM_RELEASE);lid_memset(h,0,sizeof(*h));}

/* ---------------- Array duplication/injection ---------------- */
static void free_pending(void){if(g_pending_block){g_win.VirtualFree(g_pending_block,0,MEM_RELEASE);g_pending_block=0;}g_pending_self=0;g_pending_array.data=0;g_pending_array.num=g_pending_array.max=0;}
static int copy_array(void*self,RawArray*src){int i;size_t desc,total,off;RawFString*in,*out;BYTE*block;if(!src||src->num<0||src->num>64||src->max<src->num||(!src->data&&src->num))return 0;free_pending();in=(RawFString*)src->data;desc=(size_t)src->num*sizeof(RawFString);total=desc;for(i=0;i<src->num;i++){if(in[i].num<0||in[i].num>2048||in[i].max<in[i].num||(!in[i].data&&in[i].num))return 0;total+=(size_t)in[i].num*sizeof(wchar_t);}if(total==0)total=16;block=(BYTE*)g_win.VirtualAlloc(0,total,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!block)return 0;out=(RawFString*)block;off=desc;for(i=0;i<src->num;i++){out[i].num=in[i].num;out[i].max=in[i].num;out[i].data=(wchar_t*)(block+off);if(in[i].num)lid_memcpy(out[i].data,in[i].data,(size_t)in[i].num*sizeof(wchar_t));off+=(size_t)in[i].num*sizeof(wchar_t);}g_pending_self=self;g_pending_block=block;g_pending_array.data=out;g_pending_array.num=src->num;g_pending_array.max=src->num;return 1;}
static RawArray* build_injected(void*self,RawArray*src){static const wchar_t label[]=L"Connect to Archipelago";int i,n=src->num+1;size_t desc=(size_t)n*sizeof(RawFString),total=desc+(sizeof(label));size_t off;RawFString*in=(RawFString*)src->data,*out;BYTE*block;RawArray*arr;for(i=0;i<src->num;i++)total+=(size_t)in[i].num*sizeof(wchar_t);block=(BYTE*)g_win.VirtualAlloc(0,sizeof(RawArray)+total,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!block)return 0;arr=(RawArray*)block;out=(RawFString*)(block+sizeof(RawArray));off=sizeof(RawArray)+desc;out[0].data=(wchar_t*)(block+off);out[0].num=(int)(sizeof(label)/sizeof(label[0]));out[0].max=out[0].num;lid_memcpy(out[0].data,label,sizeof(label));off+=sizeof(label);for(i=0;i<src->num;i++){out[i+1].data=(wchar_t*)(block+off);out[i+1].num=in[i].num;out[i+1].max=in[i].num;if(in[i].num)lid_memcpy(out[i+1].data,in[i].data,(size_t)in[i].num*sizeof(wchar_t));off+=(size_t)in[i].num*sizeof(wchar_t);}arr->data=out;arr->num=n;arr->max=n;(void)self;return arr;}
static int fstring_eq(RawFString*s,const wchar_t*t){int i=0,n;if(!s||!t||!s->data)return 0;n=s->num;if(n>0&&s->data[n-1]==0)n--;while(t[i])i++;if(i!=n)return 0;for(i=0;i<n;i++)if(s->data[i]!=t[i])return 0;return 1;}


/* ---------------- Main-title menu recognition ---------------- */
static int wstr_eq_n(const wchar_t* a,int an,const wchar_t* b){
    int i=0,bn=0;
    if(!a||!b)return 0;
    while(b[bn])bn++;
    if(an>0&&a[an-1]==0)an--;
    if(an!=bn)return 0;
    for(i=0;i<an;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static int plausible_string_array(RawArray*a);

static int title_label(RawFString*s){
    int n;
    if(!s||!s->data||s->num<=0||s->num>256)return 0;
    n=s->num;
    /* Exact title labels/keys used by LET IT DIE in the supported languages.
       Avoid generic "Start" so debug and in-game SimpleMenus are excluded. */
    if(wstr_eq_n(s->data,n,L"Game Start"))return 1;
    if(wstr_eq_n(s->data,n,L"GAME START"))return 1;
    if(wstr_eq_n(s->data,n,L"Début du jeu"))return 1;
    if(wstr_eq_n(s->data,n,L"Spielstart"))return 1;
    if(wstr_eq_n(s->data,n,L"Comenzar Juego"))return 1;
    if(wstr_eq_n(s->data,n,L"Comincia"))return 1;
    if(wstr_eq_n(s->data,n,L"Iniciar Jogo"))return 1;
    if(wstr_eq_n(s->data,n,L"게임 시작"))return 1;
    if(wstr_eq_n(s->data,n,L"TXT_TITLE_GAMESTART"))return 1;
    if(wstr_eq_n(s->data,n,L"TITLE.TXT_TITLE_GAMESTART"))return 1;
    return 0;
}
static int is_main_title_array(RawArray*a){
    int i;
    RawFString*s;
    if(!a||a->num<1||a->num>32||a->max<a->num||!a->data)return 0;
    s=(RawFString*)a->data;
    for(i=0;i<a->num;i++)if(title_label(&s[i]))return 1;
    return 0;
}

static int ap_title_label(RawFString*s){
    return s&&s->data&&wstr_eq_n(s->data,s->num,L"Connect to Archipelago");
}

/* The supported visible title has four vanilla entries and Game Start first. */
static int is_v26_vanilla_title_array(RawArray*a){
    RawFString*s;
    if(!plausible_string_array(a) || a->num!=4)return 0;
    s=(RawFString*)a->data;
    return title_label(&s[0]);
}

/* V0.27: keep the four vanilla entries at indices 0..3 and append AP at 4. */
static int is_v26_injected_title_array(RawArray*a){
    RawFString*s;
    if(!plausible_string_array(a) || a->num!=5)return 0;
    s=(RawFString*)a->data;
    return title_label(&s[0]) && ap_title_label(&s[4]);
}


/* ---------------- Safe CommonTop2 object inspection ---------------- */
#include "reflection_read_scope.h"
#if LID_REFLECTION_READ_SCOPE_ENABLED
static LidReadScope g_reflection_read_scope;
#endif
#if LID_EVENT_PROFILE_ENABLED
static void lid_perf_detail_count(unsigned,unsigned long long);
#endif
static int readable_range(const void* p,size_t bytes){
    MEMORY_BASIC_INFORMATION mbi;
    ULONG_PTR begin,end,region_end;
    if(!p||!bytes||!g_win.VirtualQuery)return 0;
    lid_memset(&mbi,0,sizeof(mbi));
#if LID_EVENT_PROFILE_ENABLED
    lid_perf_detail_count(0,1);
#endif
#if LID_REFLECTION_READ_SCOPE_ENABLED
    if(lid_read_scope_query(&g_reflection_read_scope,g_get_tid,g_win.VirtualQuery,p,&mbi,sizeof(mbi))==0)return 0;
#else
    if(g_win.VirtualQuery(p,&mbi,sizeof(mbi))==0)return 0;
#endif
    if(mbi.State!=MEM_COMMIT)return 0;
    if((mbi.Protect&PAGE_GUARD)||mbi.Protect==PAGE_NOACCESS)return 0;
    begin=(ULONG_PTR)p;
    end=begin+(ULONG_PTR)bytes;
    if(end<begin)return 0;
    region_end=(ULONG_PTR)mbi.BaseAddress+(ULONG_PTR)mbi.RegionSize;
    return end<=region_end;
}

static int plausible_string_array(RawArray*a){
    int i,nonempty=0;
    RawFString*s;
    if(!readable_range(a,sizeof(*a)))return 0;
    if(a->num<1||a->num>32||a->max<a->num||a->max>128||!a->data)return 0;
    if(!readable_range(a->data,(size_t)a->num*sizeof(RawFString)))return 0;
    s=(RawFString*)a->data;
    for(i=0;i<a->num;i++){
        size_t bytes;
        if(s[i].num<0||s[i].num>256||s[i].max<s[i].num||s[i].max>1024)return 0;
        if(s[i].num==0)continue;
        if(!s[i].data)return 0;
        bytes=(size_t)s[i].num*sizeof(wchar_t);
        if(!readable_range(s[i].data,bytes))return 0;
        nonempty++;
    }
    return nonempty>0;
}

static void append_ascii(char*out,size_t cap,size_t*pos,const char*text){
    size_t i=0;
    if(!out||!cap||!pos||!text)return;
    while(text[i]&&*pos+1<cap)out[(*pos)++]=text[i++];
    out[*pos]=0;
}

static void append_dec(char*out,size_t cap,size_t*pos,unsigned value){
    char temp[16];int n=0,i;
    if(value==0){if(*pos+1<cap){out[(*pos)++]='0';out[*pos]=0;}return;}
    while(value&&n<(int)sizeof(temp)){temp[n++]=(char)('0'+(value%10));value/=10;}
    for(i=n-1;i>=0&&*pos+1<cap;i--)out[(*pos)++]=temp[i];
    out[*pos]=0;
}


#include "performance_events.h"
static void append_hex64(char*out,size_t cap,size_t*pos,ULONG_PTR value){
    static const char hex[]="0123456789ABCDEF";
    int shift;
    append_ascii(out,cap,pos,"0x");
    for(shift=(int)(sizeof(ULONG_PTR)*8)-4;shift>=0;shift-=4){
        unsigned digit=(unsigned)((value>>(unsigned)shift)&0xFULL);
        if(*pos+1<cap)out[(*pos)++]=hex[digit];
    }
    out[*pos]=0;
}

static void append_module_ascii(char*out,size_t cap,size_t*pos,void*address){
    MEMORY_BASIC_INFORMATION mbi;
    wchar_t path[700];
    size_t i,start=0,n;
    if(!address||!g_win.VirtualQuery||!g_win.GetModuleFileNameW)return;
    lid_memset(&mbi,0,sizeof(mbi));
    if(!g_win.VirtualQuery(address,&mbi,sizeof(mbi))||!mbi.AllocationBase)return;
    path[0]=0;
    if(!g_win.GetModuleFileNameW((HMODULE)mbi.AllocationBase,path,700))return;
    n=lid_wcslen(path);
    for(i=0;i<n;i++)if(path[i]==L'\\'||path[i]==L'/')start=i+1;
    append_ascii(out,cap,pos," module=");
    for(i=start;i<n&&*pos+1<cap;i++){
        unsigned ch=(unsigned)path[i];
        out[(*pos)++]=(ch>=32&&ch<=126)?(char)ch:'?';
    }
    out[*pos]=0;
    append_ascii(out,cap,pos," moduleBase=");
    append_hex64(out,cap,pos,(ULONG_PTR)mbi.AllocationBase);
    append_ascii(out,cap,pos," moduleOffset=");
    append_hex64(out,cap,pos,(ULONG_PTR)address-(ULONG_PTR)mbi.AllocationBase);
}

static void write_crash_dump(LID_EXCEPTION_POINTERS* ep){
    wchar_t path[700];
    HANDLE file;
    LID_MINIDUMP_EXCEPTION_INFORMATION info;
    DWORD tid=0;
    const DWORD GENERIC_WRITE_LID=0x40000000u;
    const DWORD CREATE_ALWAYS_LID=2u;
    if(!g_minidump||!g_win.CreateFileW||!g_win.GetCurrentProcess||!g_win.GetCurrentProcessId)return;
    game_dir(path,700);
    lid_wappend(path,700,L"\\Archipelago");
    g_win.CreateDirectoryW(path,0);
    lid_wappend(path,700,L"\\crash_v027.dmp");
    file=g_win.CreateFileW(path,GENERIC_WRITE_LID,FILE_SHARE_READ,0,CREATE_ALWAYS_LID,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE)return;
    if(g_get_tid)tid=g_get_tid();
    info.ThreadId=tid;
    info.ExceptionPointers=ep;
    info.ClientPointers=FALSE;
    if(g_minidump(g_win.GetCurrentProcess(),g_win.GetCurrentProcessId(),file,0,&info,0,0))
        log_ascii("V0.27 EXCEPTION OBSERVED: crash_v027.dmp written successfully.");
    else
        log_ascii("V0.27 EXCEPTION OBSERVED: MiniDumpWriteDump failed.");
    g_win.CloseHandle(file);
}

static LONG WINAPI v23_exception_handler(LID_EXCEPTION_POINTERS* ep){
    LID_EXCEPTION_RECORD* er;
    char line[900];size_t pos=0;
    DWORD code;
    if(!ep||!(er=ep->ExceptionRecord))return 0;
    code=er->ExceptionCode;

    /* Capture only serious faults after the unique title object has been locked. */
    if(!g_v22_title_locked)return 0;
    if(code!=0xC0000005u && /* access violation */
       code!=0xC000001Du && /* illegal instruction */
       code!=0xC0000094u && /* integer divide by zero */
       code!=0xC00000FDu && /* stack overflow */
       code!=0xC0000374u && /* heap corruption */
       code!=0xC0000409u)   /* fail fast */
        return 0;

    if(g_crash_captured)return 0;
    g_crash_captured=1;

    line[0]=0;
    append_ascii(line,sizeof(line),&pos,"V0.27 EXCEPTION OBSERVED: exception code=");
    append_hex64(line,sizeof(line),&pos,(ULONG_PTR)code);
    append_ascii(line,sizeof(line),&pos," address=");
    append_hex64(line,sizeof(line),&pos,(ULONG_PTR)er->ExceptionAddress);
    append_module_ascii(line,sizeof(line),&pos,er->ExceptionAddress);
    log_ascii(line);

    if(code==0xC0000005u && er->NumberParameters>=2){
        pos=0;line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.27 EXCEPTION OBSERVED: access=");
        if(er->ExceptionInformation[0]==0)append_ascii(line,sizeof(line),&pos,"READ");
        else if(er->ExceptionInformation[0]==1)append_ascii(line,sizeof(line),&pos,"WRITE");
        else if(er->ExceptionInformation[0]==8)append_ascii(line,sizeof(line),&pos,"EXECUTE");
        else append_hex64(line,sizeof(line),&pos,er->ExceptionInformation[0]);
        append_ascii(line,sizeof(line),&pos," target=");
        append_hex64(line,sizeof(line),&pos,er->ExceptionInformation[1]);
        log_ascii(line);
    }

    pos=0;line[0]=0;
    append_ascii(line,sizeof(line),&pos,"V0.27 EXCEPTION OBSERVED: titleSelf=");
    append_hex64(line,sizeof(line),&pos,(ULONG_PTR)g_v22_locked_title_self);
    append_ascii(line,sizeof(line),&pos," tickCount=");
    append_dec(line,sizeof(line),&pos,g_v23_title_tick_count);
    if(g_direct_title_menu&&readable_range(g_direct_title_menu,sizeof(RawArray))){
        append_ascii(line,sizeof(line),&pos," menuNum=");append_dec(line,sizeof(line),&pos,(unsigned)(g_direct_title_menu->num<0?0:g_direct_title_menu->num));
        append_ascii(line,sizeof(line),&pos," menuMax=");append_dec(line,sizeof(line),&pos,(unsigned)(g_direct_title_menu->max<0?0:g_direct_title_menu->max));
    }
    if(g_direct_title_states&&readable_range(g_direct_title_states,sizeof(RawArray))){
        append_ascii(line,sizeof(line),&pos," stateNum=");append_dec(line,sizeof(line),&pos,(unsigned)(g_direct_title_states->num<0?0:g_direct_title_states->num));
        append_ascii(line,sizeof(line),&pos," stateMax=");append_dec(line,sizeof(line),&pos,(unsigned)(g_direct_title_states->max<0?0:g_direct_title_states->max));
    }
    if(g_direct_title_select&&readable_range(g_direct_title_select,sizeof(int))){
        append_ascii(line,sizeof(line),&pos," select=");append_dec(line,sizeof(line),&pos,(unsigned)(*g_direct_title_select<0?0:*g_direct_title_select));
    }
    log_ascii(line);

    write_crash_dump(ep);
    return 0; /* EXCEPTION_CONTINUE_SEARCH: diagnostics never swallow the crash. */
}

static void install_crash_diagnostics(void){
    void*k=lid_find_module_ascii("kernel32.dll");
    if(!k){log_ascii("V0.27 DIAG: kernel32 not found; crash capture unavailable.");return;}
    g_add_veh=(AddVectoredExceptionHandlerFn)lid_get_export(k,"AddVectoredExceptionHandler");
    g_remove_veh=(RemoveVectoredExceptionHandlerFn)lid_get_export(k,"RemoveVectoredExceptionHandler");
    g_set_uef=(SetUnhandledExceptionFilterFn)lid_get_export(k,"SetUnhandledExceptionFilter");
    g_get_tid=(GetCurrentThreadIdFn)lid_get_export(k,"GetCurrentThreadId");
    if(g_win.LoadLibraryW){
        g_dbghelp=g_win.LoadLibraryW(L"dbghelp.dll");
        if(g_dbghelp)g_minidump=(MiniDumpWriteDumpFn)lid_get_export(g_dbghelp,"MiniDumpWriteDump");
    }
    if(g_add_veh)g_veh_handle=g_add_veh(1,v23_exception_handler);
    if(g_set_uef)g_set_uef(v23_exception_handler);
    if(g_veh_handle)log_ascii("V0.27 DIAG: vectored crash handler installed.");
    else log_ascii("V0.27 DIAG: vectored crash handler unavailable.");
    if(g_minidump)log_ascii("V0.27 DIAG: MiniDumpWriteDump ready.");
    else log_ascii("V0.27 DIAG: dbghelp minidump support unavailable; text crash log still active.");
}

static void append_fstring_ascii(char*out,size_t cap,size_t*pos,RawFString*s){
    int i,n;
    if(!s||!s->data)return;
    n=s->num;
    if(n>0&&s->data[n-1]==0)n--;
    if(n>80)n=80;
    for(i=0;i<n&&*pos+1<cap;i++){
        unsigned ch=(unsigned)s->data[i];
        if(ch>=32&&ch<=126)out[(*pos)++]=(char)ch;
        else if(ch==0x00E9||ch==0x00E8||ch==0x00EA||ch==0x00EB)out[(*pos)++]='e';
        else if(ch==0x00E0||ch==0x00E2||ch==0x00E4)out[(*pos)++]='a';
        else if(ch==0x00F9||ch==0x00FB||ch==0x00FC)out[(*pos)++]='u';
        else if(ch==0x00EE||ch==0x00EF)out[(*pos)++]='i';
        else if(ch==0x00F4||ch==0x00F6)out[(*pos)++]='o';
        else if(ch==0x00E7)out[(*pos)++]='c';
        else out[(*pos)++]='?';
    }
    out[*pos]=0;
}

static void log_candidate_array(size_t offset,RawArray*a){
    char line[1400];size_t pos=0;int i,limit;
    RawFString*s;
    if(g_scan_log_budget<=0||!plausible_string_array(a))return;
    g_scan_log_budget--;
    line[0]=0;
    append_ascii(line,sizeof(line),&pos,"Candidate CommonTop2 FString array @+");
    append_dec(line,sizeof(line),&pos,(unsigned)offset);
    append_ascii(line,sizeof(line),&pos," num=");
    append_dec(line,sizeof(line),&pos,(unsigned)a->num);
    append_ascii(line,sizeof(line),&pos," [");
    s=(RawFString*)a->data;
    limit=a->num<10?a->num:10;
    for(i=0;i<limit;i++){
        if(i)append_ascii(line,sizeof(line),&pos," | ");
        append_fstring_ascii(line,sizeof(line),&pos,&s[i]);
    }
    if(a->num>limit)append_ascii(line,sizeof(line),&pos," | ...");
    append_ascii(line,sizeof(line),&pos,"]");
    log_ascii(line);
}

static int scan_object_for_title(void*self,const char*reason){
    /* V0.45: legacy CommonTop2 generic memory discovery is permanently disabled.
       Title recognition is handled only by the fixed BrgUIMenu_Title +0x3D0 probe. */
    (void)self;
    (void)reason;
    return 0;
}



/* ---------------- BrgUIMenu_Title direct injection ---------------- */

/*
BrgGame.upk proves the displayed title menu is BrgUIMenu_Title, derived from
BrgUIMenu_Base.

Important UE3 layout detail:
UStruct::LinkOffsets walks the UField Children chain. In this cooked package
that chain runs the script fields in reverse declaration order.

The declared source order is:

    mVoiceLanguageID
    mMenuEntries
    mStateMenuDestinationEntries
    mReturnIndex
    mMaxTitleMenuHelpNum
    mSelectIndex

but the runtime storage order around these members is therefore:

    mSelectIndex / mMaxTitleMenuHelpNum / mReturnIndex
    mStateMenuDestinationEntries
    mMenuEntries
    mVoiceLanguageID

The V0.22 log proves mMenuEntries is live at object + 0x3D0 on the supplied
build. Consequently mStateMenuDestinationEntries is the TArray immediately
BEFORE it, not after it.

The title's UnrealScript Tick calls BrgUIMenu_Base.Tick, and
IsConfirmButtonPressed calls BrgUIMenu_Base.IsConfirmButtonPressed. V0.22
hooks those two native wrappers directly.
*/

static int plausible_byte_array(RawArray*a,int expected_num){
    int i;
    BYTE*values;

    if(!a||a->num!=expected_num||a->max<a->num||a->max>128||!a->data)return 0;
    if(!readable_range(a->data,(size_t)a->num))return 0;

    values=(BYTE*)a->data;

    /* EBrgUIMenu_TitleState has values 0..31 in this package. */
    for(i=0;i<a->num;i++){
        if(values[i]>31)return 0;
    }

    return 1;
}

/*
Find mSelectIndex from the three IntProperty fields immediately before the
state array. Depending on 8-byte alignment, mSelectIndex can be 12 or 16 bytes
before mStateMenuDestinationEntries.

Memory order:
    mSelectIndex
    mMaxTitleMenuHelpNum
    mReturnIndex
    [possible 4-byte alignment padding]
    mStateMenuDestinationEntries
*/
static int* find_title_select_index(RawArray*states,int menu_count){
    BYTE*state_ptr=(BYTE*)states;
    int*best=0;
    int best_score=-999;
    int back_values[2]={12,16};
    int c;

    for(c=0;c<2;c++){
        int back=back_values[c];
        int*select_index=(int*)(state_ptr-back);
        int select_value,max_help,return_index,score=0;

        if(!readable_range(select_index,12))continue;

        select_value=select_index[0];
        max_help=select_index[1];
        return_index=select_index[2];

        if(select_value<-1||select_value>=menu_count)continue;
        if(max_help<0||max_help>64)continue;
        if(return_index<-1||return_index>64)continue;

        /* The screenshot/log starts with Game Start selected at index 0. */
        if(select_value==0)score+=8;
        else if(select_value>=0)score+=3;

        if(max_help>=menu_count)score+=3;
        if(return_index==-1||return_index==0)score+=2;

        if(score>best_score){
            best=select_index;
            best_score=score;
        }
    }

    return best;
}

static int pointer_value_readable(void*p){
    MEMORY_BASIC_INFORMATION mbi;
    if(!p)return 1;
    lid_memset(&mbi,0,sizeof(mbi));
    if(!g_win.VirtualQuery||g_win.VirtualQuery(p,&mbi,sizeof(mbi))==0)return 0;
    if(mbi.State!=MEM_COMMIT)return 0;
    if((mbi.Protect&PAGE_GUARD)||mbi.Protect==PAGE_NOACCESS)return 0;
    return 1;
}

static int plausible_pointer_array(RawArray*a,int expected_num){
    int i;
    void**items;
    if(!a||a->num<expected_num||a->num>64||a->max<a->num||a->max>128||!a->data)return 0;
    if(!readable_range(a->data,(size_t)a->num*sizeof(void*)))return 0;
    items=(void**)a->data;
    for(i=0;i<a->num;i++){
        if(items[i]&&!pointer_value_readable(items[i]))return 0;
    }
    return 1;
}

static RawArray* make_title_menu_array(RawArray*src){
    static const wchar_t label[]=L"Connect to Archipelago";
    static RawArray built;
    RawFString*in;
    RawFString*out;
    wchar_t*allocated_strings[64];
    int allocated_count=0;
    int i,n;

    if(!g_tbb_malloc||!g_tbb_free||!plausible_string_array(src))return 0;

    n=src->num+1;
    if(n<1||n>64)return 0;
    in=(RawFString*)src->data;

    out=(RawFString*)g_tbb_malloc((size_t)n*sizeof(RawFString));
    if(!out)return 0;
    lid_memset(out,0,(size_t)n*sizeof(RawFString));

    /* Copy the four vanilla strings to the SAME indices. Nothing is shifted. */
    for(i=0;i<src->num;i++){
        size_t bytes=(size_t)in[i].num*sizeof(wchar_t);
        wchar_t*text=0;
        if(bytes){
            text=(wchar_t*)g_tbb_malloc(bytes);
            if(!text){
                int j;
                for(j=0;j<allocated_count;j++)g_tbb_free(allocated_strings[j]);
                g_tbb_free(out);
                return 0;
            }
            lid_memcpy(text,in[i].data,bytes);
            allocated_strings[allocated_count++]=text;
        }
        out[i].data=text;
        out[i].num=in[i].num;
        out[i].max=in[i].num;
    }

    /* AP is the fifth entry: index 4. */
    {
        size_t bytes=sizeof(label);
        wchar_t*text=(wchar_t*)g_tbb_malloc(bytes);
        if(!text){
            int j;
            for(j=0;j<allocated_count;j++)g_tbb_free(allocated_strings[j]);
            g_tbb_free(out);
            return 0;
        }
        lid_memcpy(text,label,bytes);
        allocated_strings[allocated_count++]=text;
        out[src->num].data=text;
        out[src->num].num=(int)(sizeof(label)/sizeof(label[0]));
        out[src->num].max=out[src->num].num;
    }

    built.data=out;
    built.num=n;
    built.max=n;

    g_direct_title_menu_block=out;
    log_ascii("V0.27 ALLOC: vanilla menu indices preserved; AP appended at index 4 with game tbbmalloc.");
    return &built;
}

static int patch_title_state_array(RawArray*states,int old_count){
    BYTE*old_data;
    BYTE*new_data;
    int i;

    if(!plausible_byte_array(states,old_count))return 0;

    old_data=(BYTE*)states->data;
    if(!g_tbb_malloc)return 0;
    new_data=(BYTE*)g_tbb_malloc((size_t)old_count+1);
    if(!new_data)return 0;

    /* Keep vanilla states 0..3 exactly aligned with their original buttons. */
    for(i=0;i<old_count;i++)new_data[i]=old_data[i];

    /* Fifth state belongs only to Connect to Archipelago.
       UE3 EBrgUIMenu_TitleState value 5 is MainMenuChoiceWait itself.
       The real Title.Tick bytecode stores mReturnIndex=mSelectIndex, then calls
       StartWait(0.0, mStateMenuDestinationEntries[mSelectIndex]).  Keeping AP
       on state 5 therefore records the confirmed index without leaving the
       title menu.  The next Base.Tick hook consumes mReturnIndex==4 and opens
       our connection window. */
    new_data[old_count]=5;

    states->data=new_data;
    states->num=old_count+1;
    states->max=old_count+1;

    g_direct_title_state_block=new_data;
    log_ascii("V0.27: state array extended 4->5 by appending AP state; vanilla indices unchanged.");
    return 1;
}

static int plausible_raw_array_descriptor(RawArray*a){
    if(!a)return 0;
    if(a->num<0||a->num>64)return 0;
    if(a->max<a->num||a->max>128)return 0;
    if(a->num==0)return 1;
    if(!a->data)return 0;
    return readable_range(a->data,(size_t)a->num);
}

static int plausible_pointer_list(RawArray*a,int minimum){
    int i;
    void**items;

    if(!a||a->num<minimum||a->num>32||a->max<a->num||a->max>64||!a->data)
        return 0;

    if(!readable_range(a->data,(size_t)a->num*sizeof(void*)))
        return 0;

    items=(void**)a->data;

    for(i=0;i<a->num;i++){
        if(!items[i])continue;
        if(!pointer_value_readable(items[i]))return 0;
    }

    return 1;
}

/*
V0.22 reste volontairement proche de V0.22.

On sait grâce au package BrgGame.upk que mMenuEntryAllGuides est un
TArray<BrgUIImage*> et que, dans le layout runtime inversé, il est suivi de:

    float mMenuEntryAllGuidesDiff
    TArray<BrgUIImage*> mNoticeTextGuides
    plusieurs pointeurs UObject de la zone Notice

On utilise cette structure uniquement pour éviter une lecture hors limites
pendant RenderMenu. Si le tableau possède déjà 5 slots, aucune modification.
*/
static int objectish_pointer_array(RawArray*a,int*nonnull_out){
    int i,nonnull=0,objectish=0;
    void**items;

    if(!a||a->num<1||a->num>32||a->max<a->num||a->max>64||!a->data)
        return 0;
    if(!readable_range(a->data,(size_t)a->num*sizeof(void*)))
        return 0;

    items=(void**)a->data;
    for(i=0;i<a->num;i++){
        void*p=items[i];
        void*vt;
        if(!p)continue;
        nonnull++;
        if(!readable_range(p,sizeof(void*)))continue;
        vt=*(void**)p;
        if(vt&&readable_range(vt,sizeof(void*)))objectish++;
    }

    if(nonnull_out)*nonnull_out=nonnull;
    if(nonnull<2)return 0;
    return objectish==nonnull;
}

static void log_v22_pointer_candidate(size_t off,RawArray*a,int score){
    char line[256];size_t pos=0;
    line[0]=0;
    append_ascii(line,sizeof(line),&pos,"V0.22: pointer-array candidate @+");
    append_dec(line,sizeof(line),&pos,(unsigned)off);
    append_ascii(line,sizeof(line),&pos," num=");
    append_dec(line,sizeof(line),&pos,(unsigned)a->num);
    append_ascii(line,sizeof(line),&pos," max=");
    append_dec(line,sizeof(line),&pos,(unsigned)a->max);
    append_ascii(line,sizeof(line),&pos," score=");
    append_dec(line,sizeof(line),&pos,(unsigned)(score<0?0:score));
    log_ascii(line);
}

/*
V0.22 conserve l'injection visible V0.20/V0.17 mais remplace la signature
structurelle trop stricte des guides. Le log V0.20 a prouve que le vrai menu
est bien a +0x3D0, alors que la signature guides/float/notice ne matche pas la
mise en page runtime de cette build.

On recherche donc dans LE MEME objet BrgUIMenu_Title le meilleur
TArray<UObject*> coherent avec le nombre d'entrees du menu. Aucun fichier du
jeu n'est modifie: seul le descripteur runtime retenu est etendu.
*/
static RawArray* locate_title_guides_v22(
    void*self,
    size_t menu_offset,
    int old_count,
    int max_help,
    size_t*out_offset){

    BYTE*base=(BYTE*)self;
    MEMORY_BASIC_INFORMATION mbi;
    size_t available,limit,off;
    RawArray*best=0;
    size_t best_off=0;
    int best_score=-999999;
    int logged=0;

    lid_memset(&mbi,0,sizeof(mbi));
    if(!g_win.VirtualQuery||g_win.VirtualQuery(self,&mbi,sizeof(mbi))==0||mbi.State!=MEM_COMMIT)
        return 0;

    available=(size_t)(((BYTE*)mbi.BaseAddress+(size_t)mbi.RegionSize)-base);
    limit=available<0x1800?available:0x1800;

    for(off=0x80;off+sizeof(RawArray)<=limit;off+=8){
        RawArray*a=(RawArray*)(base+off);
        int nonnull=0,score=0;
        size_t distance;

        if(off==menu_offset||a==g_direct_title_menu||a==g_direct_title_states)
            continue;
        if(!objectish_pointer_array(a,&nonnull))
            continue;

        if(a->num==old_count)score+=500;
        else if(a->num==old_count+1)score+=420;
        else if(a->num>old_count&&a->num<=old_count+4)score+=260;
        else if(max_help>0&&a->num==max_help)score+=220;
        else continue;

        if(a->max==a->num)score+=35;
        if(nonnull==a->num)score+=80;
        else score+=nonnull*5;

        distance=off>menu_offset?off-menu_offset:menu_offset-off;
        if(distance<=0x200)score+=140;
        else if(distance<=0x400)score+=100;
        else if(distance<=0x800)score+=60;
        else if(distance<=0x1000)score+=20;

        /* Bonus seulement: accepte aussi bien le layout direct que l'inverse. */
        if(off+0x28+sizeof(RawArray)<=limit){
            float diff=*(float*)(base+off+0x10);
            RawArray*near=(RawArray*)(base+off+0x18);
            if(diff==diff&&diff>-100000.0f&&diff<100000.0f)score+=15;
            if(plausible_raw_array_descriptor(near))score+=20;
        }
        if(off>=0x28){
            RawArray*near=(RawArray*)(base+off-0x28);
            if(plausible_raw_array_descriptor(near))score+=10;
        }

        if(logged<8){
            log_v22_pointer_candidate(off,a,score);
            logged++;
        }

        if(score>best_score){
            best=a;
            best_off=off;
            best_score=score;
        }
    }

    if(best&&out_offset)*out_offset=best_off;
    return best;
}

static int ensure_title_guide_capacity(
    void*self,
    size_t menu_offset,
    int old_count,
    int*max_help){

    RawArray*guides;
    void**old_items;
    void**new_items;
    size_t guide_offset=0;
    int i;
    int help=max_help?*max_help:0;

    guides=locate_title_guides_v22(self,menu_offset,old_count,help,&guide_offset);

    if(!guides){
        log_ascii("V0.22: no parallel UObject array located; keeping visible injection for diagnostics.");
        return 1;
    }

    g_direct_title_guides=guides;
    g_vanilla_guide_desc=*guides;

    {
        char line[180];size_t pos=0;
        line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.22: selected parallel guide candidate @+");
        append_dec(line,sizeof(line),&pos,(unsigned)guide_offset);
        append_ascii(line,sizeof(line),&pos," num=");
        append_dec(line,sizeof(line),&pos,(unsigned)guides->num);
        append_ascii(line,sizeof(line),&pos," max=");
        append_dec(line,sizeof(line),&pos,(unsigned)guides->max);
        log_ascii(line);
    }

    if(guides->num>=old_count+1){
        log_ascii("V0.22: selected guide candidate already has at least five entries.");
        if(max_help&&*max_help==old_count)(*max_help)++;
        return 1;
    }

    if(guides->num!=old_count)
        return 1;

    old_items=(void**)guides->data;
    if(!g_tbb_malloc)
        return 0;

    new_items=(void**)g_tbb_malloc((size_t)(old_count+1)*sizeof(void*));

    if(!new_items)
        return 0;

    /* Preserve the four vanilla visual slots and add one safe fifth slot. */
    for(i=0;i<old_count;i++)
        new_items[i]=old_items[i];
    new_items[old_count]=old_items[old_count-1];

    guides->data=new_items;
    guides->num=old_count+1;
    guides->max=old_count+1;

    if(max_help&&*max_help==old_count)(*max_help)++;

    g_direct_title_guide_block=new_items;
    g_guide_desc_patched=1;

    log_ascii("V0.22: extended selected parallel guide candidate to five entries.");
    return 1;
}

/* Restaure strictement les trois tableaux vanilla avant qu'un choix normal
   soit traité par BrgUIMenu_Title. */
static void restore_vanilla_title_arrays(int visual_index){
    if(!g_vanilla_desc_valid)return;

    if(g_direct_title_menu)
        *g_direct_title_menu=g_vanilla_menu_desc;

    if(g_direct_title_states)
        *g_direct_title_states=g_vanilla_state_desc;

    if(g_guide_desc_patched&&g_direct_title_guides)
        *g_direct_title_guides=g_vanilla_guide_desc;

    if(g_direct_title_select){
        int original_index=visual_index-1;
        if(original_index<0)original_index=0;
        *g_direct_title_select=original_index;
    }

    /*
    V0.24: once published into UE3 descriptors, these allocations are owned by
    the game's allocator/lifetime. We deliberately do not free them here: UE3
    may already have consumed or released them. The total one-shot footprint is
    tiny and process-lifetime leakage is safer than a double free.
    */
    g_direct_title_menu_block=0;
    g_direct_title_state_block=0;
    g_direct_title_guide_block=0;

    g_direct_title_injected=0;
    g_guide_desc_patched=0;
    g_suspend_reinjection=1;

    log_ascii("V0.22: restored vanilla title arrays before vanilla selection.");
}

/* V0.32: consumes only the completed selection of appended index 4.\n   Vanilla indices 0..3 return the original CheckEnd result unchanged. */
static int WINAPI hk_direct_title_check_end(void*self){
    int r;
    int selected;

    if(!g_direct_check_end_orig)return 0;
    r=g_direct_check_end_orig(self);
    if(!r)return r;

    if(self!=g_direct_title_self||!g_direct_title_injected||!g_direct_title_select)
        return r;
    if(!readable_range(g_direct_title_select,sizeof(int)))return r;

    selected=*g_direct_title_select;
    if(selected!=4)return r;

    log_ascii("V0.32 ACTION: CheckEnd completed for AP index 4; vanilla destination consumed.");

    if(!g_dialog_open){
        g_dialog_open=1;
        log_ascii("V0.32 ACTION: opening Archipelago connection window.");
        if(show_dialog())
            log_ascii("V0.32 ACTION: connection form accepted (network handshake not implemented yet).");
        else
            log_ascii("V0.32 ACTION: connection form cancelled.");
        g_dialog_open=0;
    }

    /* Clear the completed-choice latch so closing the dialog leaves the title
       menu active instead of immediately re-firing the same selection. */
    if(g_direct_reset_choice)g_direct_reset_choice(self);
    return 0;
}

/* Installs only CheckEnd on the already-identified title class vtable.
   Failure here NEVER rolls back or blocks the visible AP button. */
static int install_direct_title_action_hook(void*self){
    void**vt;
    void**slot;
    DWORD old=0,ignored=0;

    if(g_direct_check_end_hooked)return 1;
    if(!self||!g_win.VirtualProtect)return 0;

    vt=*(void***)self;
    if(!vt)return 0;

    slot=&vt[0x370/sizeof(void*)];
    if(!slot||!*slot)return 0;

    g_direct_reset_choice=(NativeResetChoice)vt[0x348/sizeof(void*)];
    if(!g_direct_reset_choice)return 0;

    g_direct_check_end_orig=(NativeCheckEnd)*slot;
    g_direct_check_end_slot=slot;
    g_direct_action_vtable=vt;

    if(!g_win.VirtualProtect(slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&old)){
        g_direct_check_end_orig=0;
        g_direct_check_end_slot=0;
        g_direct_action_vtable=0;
        return 0;
    }

    *slot=(void*)hk_direct_title_check_end;
    g_win.VirtualProtect(slot,sizeof(void*),old,&ignored);
    g_direct_check_end_hooked=1;
    log_ascii("V0.32 ACTION: lazy BrgUIMenu_Title CheckEnd hook installed after button injection.");
    return 1;
}

static void restore_direct_title_action_hook(void){
    DWORD old=0,ignored=0;
    if(!g_direct_check_end_hooked||!g_direct_check_end_slot||!g_direct_check_end_orig||!g_win.VirtualProtect)
        return;
    if(g_win.VirtualProtect(g_direct_check_end_slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&old)){
        if(*g_direct_check_end_slot==(void*)hk_direct_title_check_end)
            *g_direct_check_end_slot=(void*)g_direct_check_end_orig;
        g_win.VirtualProtect(g_direct_check_end_slot,sizeof(void*),old,&ignored);
    }
    g_direct_check_end_hooked=0;
    g_direct_check_end_slot=0;
    g_direct_action_vtable=0;
    g_direct_check_end_orig=0;
    g_direct_reset_choice=0;
}

static int inject_direct_title(void*self,RawArray*menu,size_t menu_offset){
    RawArray*new_menu;
    RawArray*states;
    int*select_index;
    BYTE*state_values;
    int state_was_four;

    if(!self||!menu||!is_v26_vanilla_title_array(menu))return 0;

    /* Runtime layout: state array immediately precedes mMenuEntries. The game
       may rebuild only the labels, so accept a state array already extended to 5. */
    states=(RawArray*)((BYTE*)menu-sizeof(RawArray));
    state_was_four=plausible_byte_array(states,4);
    if(!state_was_four && !plausible_byte_array(states,5)){
        log_ascii("V0.27: title state array validation failed; no write performed.");
        return 0;
    }

    select_index=find_title_select_index(states,5);
    if(!select_index){
        log_ascii("V0.27: title mSelectIndex validation failed; no write performed.");
        return 0;
    }

    /* The uploaded fresh-save screenshot proves this exact 4-entry menu is the
       target. Defer tbbmalloc initialization until this point, when UE3 has
       fully constructed the live title object and its aligned state array. */
    if(!ensure_title_allocator()){
        log_ascii("V0.40 ALLOC: allocator unavailable after exact title detection; menu left untouched.");
        return 0;
    }

    new_menu=make_title_menu_array(menu);
    if(!new_menu)return 0;

    g_direct_title_self=self;
    g_direct_title_menu=menu;
    g_direct_title_states=states;
    g_direct_title_select=select_index;
    g_vanilla_menu_desc=*menu;
    g_vanilla_state_desc=*states;
    g_vanilla_desc_valid=1;
    g_direct_title_guides=0;
    g_guide_desc_patched=0;

    g_v26_menu_offset=menu_offset;
    g_v26_offset_known=1;
    g_v26_title_generation++;

    /* Publish the fifth label, preserving positions 0..3. */
    menu->data=new_menu->data;
    menu->num=5;
    menu->max=5;

    /* Navigation/state lookup also needs five aligned slots. Append only. */
    if(state_was_four){
        if(!patch_title_state_array(states,4)){
            *menu=g_vanilla_menu_desc;
            log_ascii("V0.27: could not append fifth state; restored vanilla menu.");
            return 0;
        }
    }

    /* Never rewrite mSelectIndex here. Vanilla starts at 0 and navigates 0..4. */
    g_direct_title_injected=1;

    /* V0.37: no guessed action vtable hook is installed.  Static UE3 analysis
       proved that Title.Tick itself latches mReturnIndex before StartWait.
       The existing Base.Tick hook handles that latch on the next frame. */

    {
        char line[300];size_t pos=0;
        state_values=(BYTE*)states->data;
        line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.27: title constructed [Game=0 Voice=1 Credit=2 Quit=3 AP=4], select=");
        append_dec(line,sizeof(line),&pos,(unsigned)(*select_index<0?0:*select_index));
        append_ascii(line,sizeof(line),&pos," states=[");
        append_dec(line,sizeof(line),&pos,(unsigned)state_values[0]);append_ascii(line,sizeof(line),&pos,",");
        append_dec(line,sizeof(line),&pos,(unsigned)state_values[1]);append_ascii(line,sizeof(line),&pos,",");
        append_dec(line,sizeof(line),&pos,(unsigned)state_values[2]);append_ascii(line,sizeof(line),&pos,",");
        append_dec(line,sizeof(line),&pos,(unsigned)state_values[3]);append_ascii(line,sizeof(line),&pos,",");
        append_dec(line,sizeof(line),&pos,(unsigned)state_values[4]);append_ascii(line,sizeof(line),&pos,"]");
        log_ascii(line);
    }

    return 1;
}

static void v26_clear_live_title(void){
    g_direct_title_self=0;
    g_direct_title_menu=0;
    g_direct_title_states=0;
    g_direct_title_select=0;
    g_direct_title_injected=0;
    g_vanilla_desc_valid=0;
    g_direct_title_guides=0;
    g_guide_desc_patched=0;
}

/* Finds the exact four-entry title menu. After the first discovery we use the
   proven member offset and only perform one read-only descriptor check on other
   BrgUIMenu_Base instances. No unrelated menu is ever written. */
static int scan_direct_title_object(void*self){
    static int discovery_logged=0;
    RawArray*a;

    if(!self)return 0;

    /* V0.45 deterministic title recognition.
       The V0.40 runtime proved mMenuEntries == self + 0x3D0 on this build.
       Probe exactly that one descriptor and validate the exact four vanilla
       title strings. There is no UObject scan and no offset discovery path. */
    a=(RawArray*)((BYTE*)self+0x3D0);
    if(!readable_range(a,sizeof(RawArray)))return 0;
    if(!is_v26_vanilla_title_array(a))return 0;

    if(!discovery_logged){
        log_ascii("V0.45: exact BrgUIMenu_Title recognized at fixed mMenuEntries offset +0x3D0.");
        log_candidate_array(0x3D0,a);
        discovery_logged=1;
    }

    g_v26_menu_offset=0x3D0;
    g_v26_offset_known=1;
    return inject_direct_title(self,a,0x3D0);
}


/* Ensures only the currently ticking exact title instance is active. If UE3
   reconstructs its four vanilla entries, publication happens again immediately
   before Base.Tick processes that newly constructed menu. */
static int v26_prepare_title_for_base_tick(void*self){
    RawArray*menu=0;

    /* V0.38 performance fix: while an exact live title object is locked, never
       scan unrelated BrgUIMenu_Base instances.  V0.37 performed the cached
       title-signature probe for every menu object on every Tick. */
    if(g_direct_title_self&&self!=g_direct_title_self)return 0;

    if(self==g_direct_title_self&&g_direct_title_menu){
        menu=g_direct_title_menu;

        /* Hot path: our published five-entry arrays are still exactly the same
           descriptors/data pointers.  Avoid the repeated VirtualQuery/string
           validation performed by is_v26_injected_title_array every frame. */
        if(menu->num==5&&menu->max>=5&&menu->data==g_direct_title_menu_block){
            g_direct_title_states=(RawArray*)((BYTE*)menu-sizeof(RawArray));

            if(g_direct_title_states->num==5&&g_direct_title_states->max>=5&&
               g_direct_title_states->data==g_direct_title_state_block){
                g_direct_title_injected=1;
                return 1;
            }

            /* UE3 can rebuild only the state descriptor.  Validate only that
               small byte array on this exceptional path, then cache it again. */
            if(plausible_byte_array(g_direct_title_states,4)){
                if(!patch_title_state_array(g_direct_title_states,4)){
                    log_ascii("V0.38 PERF: title states rebuilt to 4 and could not be extended.");
                    return 0;
                }
                log_ascii("V0.38 PERF: UE3 rebuilt only the state array; AP state restored once.");
                g_direct_title_injected=1;
                return 1;
            }
            if(plausible_byte_array(g_direct_title_states,5)){
                g_direct_title_state_block=g_direct_title_states->data;
                g_direct_title_injected=1;
                return 1;
            }

            log_ascii("V0.38 PERF: injected title lost its aligned state array; live target released.");
            v26_clear_live_title();
            return 0;
        }

        /* Exceptional path only: UE3 changed/rebuilt the title descriptor.
           Preserve V0.37's exact signature checks before writing anything. */
        if(readable_range(menu,sizeof(RawArray))&&is_v26_injected_title_array(menu)){
            g_direct_title_menu_block=menu->data;
            g_direct_title_states=(RawArray*)((BYTE*)menu-sizeof(RawArray));
            if(plausible_byte_array(g_direct_title_states,4)){
                if(!patch_title_state_array(g_direct_title_states,4)){
                    log_ascii("V0.38 PERF: title labels are 5 but states rebuilt to 4 and could not be extended.");
                    return 0;
                }
                log_ascii("V0.38 PERF: UE3 rebuilt only the state array; AP state restored once.");
            }else if(plausible_byte_array(g_direct_title_states,5)){
                g_direct_title_state_block=g_direct_title_states->data;
            }else{
                log_ascii("V0.38 PERF: injected title lost its aligned state array; live target released.");
                v26_clear_live_title();
                return 0;
            }
            g_direct_title_injected=1;
            return 1;
        }

        if(readable_range(menu,sizeof(RawArray))&&is_v26_vanilla_title_array(menu)){
            g_direct_title_injected=0;
            log_ascii("V0.38 PERF: UE3 rebuilt the four-entry title; AP re-appended once.");
            return inject_direct_title(self,menu,g_v26_menu_offset);
        }

        /* This UObject/memory is no longer the title menu. Never write to it. */
        log_ascii("V0.38 PERF: previous title instance no longer has the title signature; live target released.");
        v26_clear_live_title();
    }

    /* Only while no live title is locked do we search for a new/reused exact
       title instance.  After the first discovery this is one cached offset
       fixed +0x3D0 signature probe with no discovery scan. */
    if(scan_direct_title_object(self))return 1;
    return 0;
}

/*
V0.25: the title menu is rebuilt asynchronously by the game after our first
injection.  The runtime log proved that the SAME locked title object can move
from menu=5/5 back to the vanilla menu=4/4 while the parallel state array is
still 5/5.  That is not a new menu and must not unlock global scanning.

If that exact transition happens, rebuild ONLY mMenuEntries with the same TBB
allocator-safe representation.  The state array and select index are already
in their injected form, so do not extend/shift them a second time.
*/
static int v025_reinject_if_title_rebuilt(void*self){(void)self;return 0;}

/*
V0.37: handle the exact UE3 selection latch proved from BrgGame.upk.
BrgUIMenu_Title.Tick/MainMenuChoiceWait executes:
    mReturnIndex = mSelectIndex;
    StartWait(0.0, int(mStateMenuDestinationEntries[mSelectIndex]));
For AP, state[4] is MainMenuChoiceWait (5), so the title remains active and
returnIndex==4 is a reliable one-shot confirmation latch on the next frame.
*/

/* V0.43 bridge path helpers. These run only from the worker/game thread, never from DllMain. */
static int direct_file_exists(const wchar_t*p){HANDLE h;if(!g_win.CreateFileW)return 0;h=g_win.CreateFileW(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return 0;g_win.CloseHandle(h);return 1;}
static void trim_parent_dir(wchar_t*p){size_t n=lid_wcslen(p);while(n>0){if(p[n-1]==L'\\'||p[n-1]==L'/'){p[n-1]=0;return;}n--;}}

/* ---------------- V0.43 Archipelago launcher bridge ---------------- */
static wchar_t g_bridge_request[1000],g_bridge_result[1000],g_bridge_launcher[1000],g_bridge_cmd[4096],g_bridge_component_marker[1000],g_bridge_client_marker[1000];
static char g_bridge_result_buf[4096];
/* V0.52: bridge state is shared only between the UE3 hook thread and one worker.
   While non-zero, a second AP form/bridge launch is rejected. */
static volatile LONG g_bridge_running=0;
/* V0.52: hot AP activation state is published by the bridge worker and consumed on the UE3 game thread. */
static volatile LONG g_ap_mode_active=0;
static volatile LONG g_pending_title_reset=0;
static volatile LONG g_pending_ap_release=0;
static char g_release_slot_key[64];
static void* g_release_userdata=0;
static int g_release_stage=0;
static int g_release_wait_ticks=0;
static wchar_t g_release_save_path[1000];
static wchar_t g_release_backup_path[1000];
static BYTE g_release_copy_buffer[65536];

/* V0.70: runtime character watcher.  Unlike V0.60-V0.62, this remains active after
   Game Start so a Body/Chara instance recreated by the native loading flow is
   detected and synchronized instead of being changed only at the title menu. */
static void* g_v070_last_userdata=0;
static void* g_v070_last_body=0;
static int g_v070_last_index=-999;
static int g_v070_last_chara_grade=-999;
static int g_v070_last_body_grade=-999;
static unsigned g_v070_watch_calls=0;
static unsigned g_v070_corrections=0;

/* V0.74: dynamic runtime fighter configuration.
   The AP client writes the actual slot_data values into connection_result.json.
   These values are parsed after authentication and used to build the in-memory
   BAL progression table. Nothing in this block hard-codes the player's YAML
   starting stats or progressive increases. */
typedef struct V074RuntimeConfig {
    int valid;
    int fighter_grade;
    int starting_hp;
    int starting_stm;
    int starting_str;
    int starting_dex;
    int starting_vit;
    int starting_luk;
    int starting_skill_slots;
    int starting_death_bag;
    int starting_rage_gauge;
    int hp_increase;
    int stm_increase;
    int str_increase;
    int dex_increase;
    int vit_increase;
    int luk_increase;
    int death_bag_increase;
    int skill_slots_increase;
    int rage_gauge_increase;
    int hp_quantity;
    int stm_quantity;
    int str_quantity;
    int dex_quantity;
    int vit_quantity;
    int luk_quantity;
    int death_bag_quantity;
    int skill_slots_quantity;
    int rage_gauge_quantity;
    int max_levels;
} V074RuntimeConfig;

static V074RuntimeConfig g_v074_cfg;
static int g_v074_status_rows_ready=0;
static void* g_v074_last_scanned_body=0;
static unsigned g_v074_patch_passes=0;
static unsigned g_v074_total_matches=0;
static unsigned long long g_v074_patched_level_mask=0;
static void* g_v074_last_level_chara=0;
static void* g_v074_last_level_body=0;

static const int g_v074_vanilla_grade6[45][10]={
    {2800,117,117,90,252,360,14,5,24,5},
    {3110,120,120,91,254,365,15,5,24,5},
    {3420,123,123,92,256,370,16,5,24,5},
    {3730,126,126,93,258,375,17,5,24,5},
    {4040,129,129,94,260,380,18,5,24,5},
    {4350,132,132,95,262,385,19,5,24,5},
    {4660,135,135,96,264,390,20,5,24,5},
    {4970,138,138,97,266,395,21,5,24,5},
    {5280,141,141,98,268,400,22,5,24,5},
    {5590,144,144,99,270,405,23,5,24,5},
    {5900,147,147,100,272,410,24,5,24,5},
    {6210,150,150,101,274,415,25,5,24,5},
    {6520,153,153,102,276,420,26,5,24,5},
    {6830,156,156,103,278,425,27,5,24,5},
    {7140,159,159,104,280,430,28,5,24,5},
    {7450,162,162,105,282,435,29,5,24,5},
    {7760,165,165,106,284,440,30,5,24,5},
    {8070,168,168,107,286,445,31,5,24,5},
    {8380,171,171,108,288,450,32,5,24,5},
    {8690,174,174,109,290,455,33,5,24,5},
    {9000,177,177,110,292,460,34,5,24,5},
    {9310,180,180,111,294,465,35,5,24,5},
    {9620,183,183,112,296,470,36,5,24,5},
    {9930,186,186,113,298,475,37,5,24,5},
    {10240,189,189,114,300,480,38,5,24,5},
    {10550,192,192,115,302,485,39,6,25,5},
    {10860,195,195,116,304,490,40,0,26,0},
    {11170,198,198,117,306,495,41,0,27,0},
    {11480,201,201,118,308,500,42,0,0,0},
    {11790,204,204,119,310,505,43,0,0,0},
    {12720,222,222,127,312,505,44,7,28,5},
    {13650,240,240,135,314,505,45,0,29,0},
    {14580,258,258,143,316,505,46,0,30,0},
    {15510,276,276,151,318,505,47,0,0,0},
    {16440,294,294,159,320,505,48,0,0,0},
    {17370,312,312,167,322,505,49,8,31,5},
    {18300,330,330,175,324,505,50,0,32,0},
    {19230,348,348,183,326,505,51,0,33,0},
    {20160,366,366,191,328,505,52,0,0,0},
    {21090,384,384,199,330,505,53,0,0,0},
    {22950,420,420,215,334,505,55,9,34,5},
    {23880,438,438,223,336,505,56,0,35,0},
    {24810,456,456,231,338,505,57,0,36,0},
    {25740,474,474,239,340,505,58,0,0,0},
    {26670,492,492,247,342,505,59,0,0,0}
};

static const int g_v074_vanilla_grade1[5][10]={
    {200,10,10,10,216,360,10,1,20,3},
    {230,14,14,13,218,365,11,1,20,3},
    {260,18,18,16,220,370,12,1,20,3},
    {290,22,22,19,222,375,13,1,20,3},
    {320,26,26,22,224,380,14,1,20,3}
};

static int v074_writable_region(DWORD protect){
    DWORD p=protect&0xffu;
    if(protect&PAGE_GUARD)return 0;
    if(p==PAGE_NOACCESS)return 0;
    return p==0x04u||p==0x08u||p==PAGE_EXECUTE_READWRITE||p==0x80u;
}

/* Maps the first field of a vanilla BAL Grade-6 status row to its level.
   A switch keeps the process-wide scan cheap even when the YAML requests
   levels beyond the old V0.71 hard limit of 25. */
static int v074_grade6_row_from_hp(int hp){
    switch(hp){
        case 2800: return 0;
        case 3110: return 1;
        case 3420: return 2;
        case 3730: return 3;
        case 4040: return 4;
        case 4350: return 5;
        case 4660: return 6;
        case 4970: return 7;
        case 5280: return 8;
        case 5590: return 9;
        case 5900: return 10;
        case 6210: return 11;
        case 6520: return 12;
        case 6830: return 13;
        case 7140: return 14;
        case 7450: return 15;
        case 7760: return 16;
        case 8070: return 17;
        case 8380: return 18;
        case 8690: return 19;
        case 9000: return 20;
        case 9310: return 21;
        case 9620: return 22;
        case 9930: return 23;
        case 10240: return 24;
        case 10550: return 25;
        case 10860: return 26;
        case 11170: return 27;
        case 11480: return 28;
        case 11790: return 29;
        case 12720: return 30;
        case 13650: return 31;
        case 14580: return 32;
        case 15510: return 33;
        case 16440: return 34;
        case 17370: return 35;
        case 18300: return 36;
        case 19230: return 37;
        case 20160: return 38;
        case 21090: return 39;
        case 22950: return 40;
        case 23880: return 41;
        case 24810: return 42;
        case 25740: return 43;
        case 26670: return 44;
        default: return -1;
    }
}

static int v074_grade1_row_from_hp(int hp){
    switch(hp){
        case 200:return 0;case 230:return 1;case 260:return 2;case 290:return 3;case 320:return 4;
        default:return -1;
    }
}

static int v074_match32_table(const int* p,const int table[][10],int row){
    int j;
    for(j=0;j<10;j++)if(p[j]!=table[row][j])return 0;
    return 1;
}

static int v074_match64_table(const long long* p,const int table[][10],int row){
    int j;
    for(j=0;j<10;j++)if(p[j]!=(long long)table[row][j])return 0;
    return 1;
}

/* Builds one status row from the actual Archipelago slot_data.
   stmrecov is intentionally preserved from LET IT DIE's own vanilla row because
   it is not an AP/YAML option. */
static void v074_build_target_row(int level,const int vanilla[10],int out[10]){
    int i=level-1;
    out[0]=g_v074_cfg.starting_hp+i*g_v074_cfg.hp_increase;
    out[1]=g_v074_cfg.starting_str+i*g_v074_cfg.str_increase;
    out[2]=g_v074_cfg.starting_dex+i*g_v074_cfg.dex_increase;
    out[3]=g_v074_cfg.starting_vit+i*g_v074_cfg.vit_increase;
    out[4]=g_v074_cfg.starting_stm+i*g_v074_cfg.stm_increase;
    out[5]=vanilla[5];
    out[6]=g_v074_cfg.starting_luk+i*g_v074_cfg.luk_increase;
    /* Skill/Bag/Rage start at progression level 0. Row N therefore stores
       the value after N progressive items, unlike the six main stats whose
       minimum level is 1. */
    out[7]=g_v074_cfg.starting_skill_slots+level*g_v074_cfg.skill_slots_increase;
    out[8]=g_v074_cfg.starting_death_bag+level*g_v074_cfg.death_bag_increase;
    out[9]=g_v074_cfg.starting_rage_gauge+level*g_v074_cfg.rage_gauge_increase;
}

static void v074_log_match(const char*kind,int level,void*address){
    char line[360];size_t pos=0;line[0]=0;
    append_ascii(line,sizeof(line),&pos,"V0.74 DYNAMIC STATS: patched ");
    append_ascii(line,sizeof(line),&pos,kind);
    append_ascii(line,sizeof(line),&pos," BAL level ");
    append_dec(line,sizeof(line),&pos,(unsigned)level);
    append_ascii(line,sizeof(line),&pos," row @");
    append_hex64(line,sizeof(line),&pos,(ULONG_PTR)address);
    line[pos]=0;log_ascii(line);
}

/* Validates a UE3 FString against an ASCII literal. */
static int v074_skill_fstring_matches(BYTE*field,const char*ascii){
    RawFString*s=(RawFString*)field;
    size_t len=0,i;
    if(!field||!ascii||!readable_range(field,sizeof(RawFString)))return 0;
    while(ascii[len])len++;
    if(!s->data||s->num<0||s->max<s->num||s->max>128)return 0;
    if(!(s->num==(int)len||s->num==(int)len+1))return 0;
    if(!readable_range(s->data,(len+1)*sizeof(wchar_t)))return 0;
    for(i=0;i<len;i++)if(s->data[i]!=(wchar_t)(unsigned char)ascii[i])return 0;
    return s->data[len]==0||s->num==(int)len;
}

/* V0.76 hooks UBrgDatabase::GetBodyDetail directly instead of scanning for
   cached master_body_detail rows.  The native output is a 0x68-byte
   BrgDbBodyDetail structure whose relevant fields are:
     +0x2C param_lv_max, +0x30 bag_capacity,
     +0x34 TArray<int> skill_slots, +0x44 rage_capacity. */
typedef struct V076IntArray {
    int* data;
    int num;
    int max;
} V076IntArray;

typedef int (WINAPI *NativeGetBodyDetailFn)(void*,RawFString*,int,int,void*);
typedef int (WINAPI *NativeIntArrayAddFn)(void*,int);

static NativeGetBodyDetailFn g_v076_get_body_detail_original=0;
static void** g_v076_get_body_detail_slot=0;
static void** g_v076_database_vtable=0;
static int g_v076_body_detail_hook_ready=0;
static BYTE* g_v076_active_chara=0;
#include "reload_capacity_values.h"
static unsigned g_v076_body_detail_log_budget=18;

#include "skill_capacity_policy.h"
static LidSkillCapacityPolicy g_v076_skill_capacity_policy;

/* Returns the AP capacity represented by a zero-based progression counter. */
static int v076_capacity_from_level(int start,int increase,int quantity,int level){
    long long value;
    if(level<0)level=0;
    if(quantity>=0&&level>quantity)level=quantity;
    value=(long long)start+(long long)level*(long long)increase;
    if(value>2147483647LL)value=2147483647LL;
    if(value<0)value=0;
    return (int)value;
}

/* Reads the three zero-based progression counters from the exact active
   BrgDbChara record already tracked by the validated V0.70 watcher. */
static void v076_get_active_capacity_levels(int*skill_level,int*bag_level,int*rage_level){
    BYTE*r=g_v076_active_chara;
    int skill=0,bag=0,rage=0;
    if(r&&readable_range(r,0x174)){
        skill=*(int*)(r+0x168);
        bag=*(int*)(r+0x16c);
        rage=*(int*)(r+0x170);
    }
    if(skill<0)skill=0;if(bag<0)bag=0;if(rage<0)rage=0;
    if(g_v074_cfg.skill_slots_quantity>=0&&skill>g_v074_cfg.skill_slots_quantity)skill=g_v074_cfg.skill_slots_quantity;
    if(g_v074_cfg.death_bag_quantity>=0&&bag>g_v074_cfg.death_bag_quantity)bag=g_v074_cfg.death_bag_quantity;
    if(g_v074_cfg.rage_gauge_quantity>=0&&rage>g_v074_cfg.rage_gauge_quantity)rage=g_v074_cfg.rage_gauge_quantity;
    if(skill_level)*skill_level=skill;
    if(bag_level)*bag_level=bag;
    if(rage_level)*rage_level=rage;
}

/* Uses LET IT DIE's own native TArray<int> grow helper (RVA 0x5BC890) to
   resize the skill-slot list exactly as GetBodyDetail itself does.  Shrinking
   only changes Num; growing is delegated to the game's allocator/reallocator. */
static int v076_set_skill_slot_array(void*out_detail,int desired){
    V076IntArray*a;
    void*exe;
    NativeIntArrayAddFn add_items;
    int i,delta;
    if(!out_detail||desired<0||desired>64||!readable_range((BYTE*)out_detail+0x34,sizeof(V076IntArray)))return 0;
    a=(V076IntArray*)((BYTE*)out_detail+0x34);
    if(a->num<0||a->max<0||a->num>a->max||a->max>256)return 0;
    if(desired>a->num){
        exe=lid_find_module_ascii("BrgGame-Steam.exe");
        if(!exe)return 0;
        add_items=(NativeIntArrayAddFn)((BYTE*)exe+0x5BC890);
        if(!executable_ptr(exe,(void*)add_items))return 0;
        delta=desired-a->num;
        add_items(a,delta);
    }else{
        a->num=desired;
    }
    if(desired>0){
        if(!a->data||a->num<desired||a->max<desired||!readable_range(a->data,(size_t)desired*sizeof(int)))return 0;
        for(i=0;i<desired;i++)a->data[i]=i+1;
        a->num=desired;
    }
    return 1;
}

static void v076_log_body_detail(int grade,int limit_break,int skill_level,int bag_level,int rage_level,
                                 int native_bag,int native_skill,int native_rage,
                                 int ap_bag,int ap_skill,int ap_rage,int skill_decision){
    char line[640];size_t pos=0;
    if(!g_v076_body_detail_log_budget)return;
    g_v076_body_detail_log_budget--;
    line[0]=0;
    append_ascii(line,sizeof(line),&pos,"V0.76 BODY DETAIL: grade=");append_dec(line,sizeof(line),&pos,(unsigned)(grade<0?0:grade));
    append_ascii(line,sizeof(line),&pos," lb=");append_dec(line,sizeof(line),&pos,(unsigned)(limit_break<0?0:limit_break));
    append_ascii(line,sizeof(line),&pos," levels S/B/R=");append_dec(line,sizeof(line),&pos,(unsigned)skill_level);append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)bag_level);append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)rage_level);
    append_ascii(line,sizeof(line),&pos," native=");append_dec(line,sizeof(line),&pos,(unsigned)(native_skill<0?0:native_skill));append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)(native_bag<0?0:native_bag));append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)(native_rage<0?0:native_rage));
    append_ascii(line,sizeof(line),&pos," -> AP=");append_dec(line,sizeof(line),&pos,(unsigned)ap_skill);append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)ap_bag);append_ascii(line,sizeof(line),&pos,"/");append_dec(line,sizeof(line),&pos,(unsigned)ap_rage);
    append_ascii(line,sizeof(line),&pos,skill_decision==1?"; skill capacity protected/applied in RAM":skill_decision<0?"; skill capacity update refused; no confirmation":"; skill capacity retained pending reliable AP application");
    line[pos]=0;log_ascii(line);
}

/* Hooks the exact database detail getter used by LET IT DIE.  The original is
   called first, then only the AP-controlled fields are replaced.  Grade 1 is
   also covered because the native Game Start path briefly constructs the
   selected fighter as Grade 1 before the validated watcher changes it to the
   configured AP grade. */
static int WINAPI v076_get_body_detail(void*self,RawFString*type,int grade,int limit_break,void*out_detail){
    int result=g_v076_get_body_detail_original?g_v076_get_body_detail_original(self,type,grade,limit_break,out_detail):0;
    int skill_level=0,bag_level=0,rage_level=0;
    int ap_skill,ap_bag,ap_rage;
    int native_bag,native_skill,native_rage;
    int equipped_floor=0,protected_skill=0,skill_decision=0;
#if LID_NATIVE_RELOAD_ENABLED
    LidReloadCapacityValues restored;int restoring;
#endif
    if(!g_ap_mode_active||!g_v074_cfg.valid||!out_detail)return result;
    if(!(grade==1||grade==g_v074_cfg.fighter_grade))return result;
    if(!v074_skill_fstring_matches((BYTE*)type,"BAL"))return result;
    if(!readable_range(out_detail,0x68))return result;
    /* A successful native lookup copies grade/limit_break into the returned
       BrgDbBodyDetail.  Validate those fields instead of assuming a specific
       success return convention for the native method. */
    if(*(int*)((BYTE*)out_detail+0x10)!=grade||*(int*)((BYTE*)out_detail+0x14)!=limit_break)return result;

    native_bag=*(int*)((BYTE*)out_detail+0x30);
    native_skill=((V076IntArray*)((BYTE*)out_detail+0x34))->num;
    native_rage=*(int*)((BYTE*)out_detail+0x44);
#if LID_NATIVE_RELOAD_ENABLED
    restoring=progress_reload_capacity(&restored);
    if(restoring<0)return result;
    if(restoring){skill_level=restored.skill_level;bag_level=restored.bag_level;rage_level=restored.rage_level;}
    else
#endif
    v076_get_active_capacity_levels(&skill_level,&bag_level,&rage_level);
    ap_skill=v076_capacity_from_level(g_v074_cfg.starting_skill_slots,g_v074_cfg.skill_slots_increase,g_v074_cfg.skill_slots_quantity,skill_level);
    ap_bag=v076_capacity_from_level(g_v074_cfg.starting_death_bag,g_v074_cfg.death_bag_increase,g_v074_cfg.death_bag_quantity,bag_level);
    ap_rage=v076_capacity_from_level(g_v074_cfg.starting_rage_gauge,g_v074_cfg.rage_gauge_increase,g_v074_cfg.rage_gauge_quantity,rage_level);
#if LID_NATIVE_RELOAD_ENABLED
    if(restoring){
        if(ap_bag<restored.bag_floor)ap_bag=restored.bag_floor;
        protected_skill=ap_skill>restored.skill_floor?ap_skill:restored.skill_floor;
        skill_decision=1;
    }
#endif

    if(grade==g_v074_cfg.fighter_grade)*(int*)((BYTE*)out_detail+0x2c)=g_v074_cfg.max_levels;
    *(int*)((BYTE*)out_detail+0x30)=ap_bag;
#if LID_NATIVE_RELOAD_ENABLED
    if(!restoring)
#endif
    if(lid_skill_equipped_floor(g_v076_active_chara,readable_range,&equipped_floor))
        skill_decision=lid_skill_detail_target(&g_v076_skill_capacity_policy,g_release_slot_key,
            ap_skill,native_skill,equipped_floor,&protected_skill);
    if(skill_decision==1)ap_skill=protected_skill;
    else ap_skill=native_skill; /* Unknown/startup state never truncates slots. */
    if(skill_decision==1&&!v076_set_skill_slot_array(out_detail,ap_skill)){
        skill_decision=-1;
#if LID_NATIVE_RELOAD_ENABLED
        if(restoring)progress_reload_fail("AP RELOAD R26AF: restored skill metadata allocation failed; save/delivery refused.");
#endif
        log_ascii("V0.76 BODY DETAIL: skill_slots TArray resize failed; leaving native skill slot array unchanged.");
    }
    *(int*)((BYTE*)out_detail+0x44)=ap_rage;
    v076_log_body_detail(grade,limit_break,skill_level,bag_level,rage_level,native_bag,native_skill,native_rage,ap_bag,ap_skill,ap_rage,skill_decision);
    return result;
}

static int v076_write_vtable_slot(void**slot,void*replacement,void**original_out){
    DWORD old=0,ignore=0;
    if(!slot||!replacement||!original_out||!g_win.VirtualProtect)return 0;
    if(!readable_range(slot,sizeof(void*)))return 0;
    *original_out=*slot;
    if(!g_win.VirtualProtect(slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&old))return 0;
    *slot=replacement;
    if(g_win.FlushInstructionCache)g_win.FlushInstructionCache(g_win.GetCurrentProcess(),slot,sizeof(void*));
    g_win.VirtualProtect(slot,sizeof(void*),old,&ignore);
    return 1;
}

/* Finds any loaded BrgDatabase object whose vtable exposes the proven
   GetBodyDetail slot +0x348. */
static void* v076_find_database_object(void){
    static const wchar_t*names[]={
        L"Default__BrgDatabase",
        L"BrgDatabase_0",L"BrgDatabase_1",L"BrgDatabase_2",L"BrgDatabase_3",
        L"BrgDatabase_4",L"BrgDatabase_5",L"BrgDatabase_6",L"BrgDatabase_7",
        L"BrgDatabase_8",L"BrgDatabase_9",L"BrgDatabase_10",L"BrgDatabase_11",
        L"BrgDatabase_12",L"BrgDatabase_13",L"BrgDatabase_14",L"BrgDatabase_15"
    };
    void*exe=lid_find_module_ascii("BrgGame-Steam.exe");
    StaticFindObjectFn find_object;
    unsigned i;
    if(!exe)return 0;
    find_object=(StaticFindObjectFn)((BYTE*)exe+0x16D230);
    if(!executable_ptr(exe,(void*)find_object))return 0;
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++){
        void*object=find_object(0,(void*)(LONG_PTR)-1,names[i],0);
        void**vt;void*fn;
        if(!object||!readable_range(object,sizeof(void*)))continue;
        vt=*(void***)object;
        if(!vt||!readable_range(vt,0x350))continue;
        fn=vt[0x348/sizeof(void*)];
        if(fn&&executable_ptr(exe,fn))return object;
    }
    return 0;
}

/* Installs only the proven UBrgDatabase::GetBodyDetail hook. */
static int v076_install_body_detail_hook(void){
    void*object;void**vt;void*exe=lid_find_module_ascii("BrgGame-Steam.exe");void*original=0;
    if(g_v076_body_detail_hook_ready)return 1;
    if(!g_ap_mode_active||!g_v074_cfg.valid||!exe)return 0;
    object=v076_find_database_object();
    if(!object)return 0;
    vt=*(void***)object;
    if(!vt||!readable_range(vt,0x350))return 0;
    g_v076_get_body_detail_slot=&vt[0x348/sizeof(void*)];
    if(!executable_ptr(exe,*g_v076_get_body_detail_slot))return 0;
    if(!v076_write_vtable_slot(g_v076_get_body_detail_slot,(void*)v076_get_body_detail,&original))return 0;
    g_v076_get_body_detail_original=(NativeGetBodyDetailFn)original;
    g_v076_database_vtable=vt;
    g_v076_body_detail_hook_ready=1;
    log_ascii("V0.76 BODY DETAIL: UBrgDatabase::GetBodyDetail vtable +0x348 hook installed.");
    return 1;
}

#include "level_up_xp_policy.h"
typedef int (WINAPI *R61GetBodyLvlExpFn)(void*,int,int);
static R61GetBodyLvlExpFn g_r61_get_level_xp_original=0;
static int g_r61_level_xp_ready=0;
static unsigned g_r61_level_xp_log_budget=3;

static int WINAPI r61_get_level_xp(void*self,int level,int grade){
    int native_cost,result;
    if(!g_r61_get_level_xp_original)return -1;
    native_cost=g_r61_get_level_xp_original(self,level,grade);
    result=lid_level_up_xp_cost(g_ap_mode_active,g_v074_cfg.valid,native_cost);
    if(result!=native_cost&&g_r61_level_xp_log_budget){
        g_r61_level_xp_log_budget--;
        log_ascii("AP LEVEL XP R61: positive native NPC level price replaced with 9999999 XP.");
    }
    return result;
}

static unsigned g_r62_level_xp_reports=0;
static void r62_level_xp_report(unsigned bit,const char*message){
    if(g_r62_level_xp_reports&bit)return;
    g_r62_level_xp_reports|=bit;
    log_ascii(message);
}

static int r61_install_level_xp_hook(void){
    BYTE*exe;void**vt;void*original=0;
    if(g_r61_level_xp_ready)return 1;
    if(!g_ap_mode_active||!g_v074_cfg.valid)return 0;
    exe=(BYTE*)lid_find_module_ascii("BrgGame-Steam.exe");
    if(!exe)return 0;
    /* The detail hook has already redirected slot348 into this DLL. Its
       original object finder therefore rejects that slot by design. Reuse
       the table captured by its successful installation, never rescan. */
    if(!g_v076_body_detail_hook_ready||!g_v076_database_vtable){
        r62_level_xp_report(1,"AP LEVEL XP R62: waiting for verified BodyDetail database table.");
        return 0;
    }
    vt=g_v076_database_vtable;
    if(vt!=(void**)(exe+0x20AFFC0)||!readable_range(vt,0x368)){
        r62_level_xp_report(2,"AP LEVEL XP R62: refused; database table identity/readability mismatch.");
        return 0;
    }
    if(vt[0x360/sizeof(void*)]!=(void*)(exe+0xFD2A80)||!executable_ptr(exe,vt[0x360/sizeof(void*)])){
        r62_level_xp_report(4,"AP LEVEL XP R62: refused; original GetBodyLvlExp target mismatch.");
        return 0;
    }
    g_r61_get_level_xp_original=(R61GetBodyLvlExpFn)vt[0x360/sizeof(void*)];
    if(!v076_write_vtable_slot(&vt[0x360/sizeof(void*)],(void*)r61_get_level_xp,&original)){
        g_r61_get_level_xp_original=0;
        r62_level_xp_report(8,"AP LEVEL XP R62: vtable write refused; installation will retry.");
        return 0;
    }
    g_r61_level_xp_ready=1;
    log_ascii("AP LEVEL XP R62: verified cached database table; GetBodyLvlExp +0x360 hook installed; AP price=9999999 XP.");
    return 1;
}

#include "level_up_bloodnium_policy.h"
typedef int (WINAPI *R75GetBodyLvlBloodniumFn)(void*,int,int);
static R75GetBodyLvlBloodniumFn g_r75_level_bloodnium_original=0;
static int g_r75_level_bloodnium_ready=0,g_r75_level_bloodnium_armed=0;
static unsigned g_r75_level_bloodnium_reports=0,g_r75_level_bloodnium_log_budget=3;
static void r75_level_bloodnium_report(unsigned bit,const char*message){
    if(g_r75_level_bloodnium_reports&bit)return;
    g_r75_level_bloodnium_reports|=bit;log_ascii(message);
}
static int WINAPI r75_get_level_bloodnium(void*self,int level,int grade){
    R75GetBodyLvlBloodniumFn original=__atomic_load_n(&g_r75_level_bloodnium_original,__ATOMIC_ACQUIRE);
    if(!original)return -1;
    int native_cost=original(self,level,grade);
    int result=lid_level_up_bloodnium_cost(g_ap_mode_active&&__atomic_load_n(&g_r75_level_bloodnium_armed,__ATOMIC_ACQUIRE),g_v074_cfg.valid,native_cost);
    unsigned budget=__atomic_load_n(&g_r75_level_bloodnium_log_budget,__ATOMIC_RELAXED);
    if(result!=native_cost&&budget&&__atomic_compare_exchange_n(&g_r75_level_bloodnium_log_budget,&budget,budget-1,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED))
        log_ascii("AP LEVEL BLOODNIUM R75: native NPC cost, including zero, replaced with 999999999 bloodpoints.");
    return result;
}
static int r75_install_level_bloodnium_hook(void){
    static const BYTE signature[16]={0x40,0x57,0x48,0x83,0xec,0x40,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48};
    if(g_r75_level_bloodnium_ready)return __atomic_load_n(&g_r75_level_bloodnium_armed,__ATOMIC_ACQUIRE);
    if(!g_ap_mode_active||!g_v074_cfg.valid)return 0;
    BYTE*exe=(BYTE*)lid_find_module_ascii("BrgGame-Steam.exe");
    if(!exe)return 0;
    if(!g_v076_body_detail_hook_ready||!g_v076_database_vtable){
        r75_level_bloodnium_report(1,"AP LEVEL BLOODNIUM R75: waiting for verified BodyDetail database table.");return 0;
    }
    void**vt=g_v076_database_vtable;void*expected=exe+0xfd2990;
    if(vt!=(void**)(exe+0x20affc0)||!readable_range(vt,0x370)||
       __atomic_load_n(&vt[0x368/sizeof(void*)],__ATOMIC_ACQUIRE)!=expected||!executable_ptr(exe,expected)||
       !readable_range(expected,16)||lid_memcmp(expected,signature,16)||!g_win.VirtualProtect){
        r75_level_bloodnium_report(2,"AP LEVEL BLOODNIUM R75: refused; verified table/getter signature mismatch.");return 0;
    }
    DWORD old=0,ignored=0;
    __atomic_store_n(&g_r75_level_bloodnium_original,(R75GetBodyLvlBloodniumFn)expected,__ATOMIC_RELEASE);
    if(!g_win.VirtualProtect(&vt[0x368/sizeof(void*)],8,0x04 /* PAGE_READWRITE */,&old)){
        r75_level_bloodnium_report(4,"AP LEVEL BLOODNIUM R75: vtable protection refused; will retry.");return 0;
    }
    int swapped=__atomic_compare_exchange_n(&vt[0x368/sizeof(void*)],&expected,(void*)r75_get_level_bloodnium,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    int restored=g_win.VirtualProtect(&vt[0x368/sizeof(void*)],8,old,&ignored)!=0;
    if(swapped)g_r75_level_bloodnium_ready=1;
    if(!swapped||!restored){
        r75_level_bloodnium_report(8,"AP LEVEL BLOODNIUM R75: slot exchange/restoration refused; price override disabled.");return 0;
    }
    __atomic_store_n(&g_r75_level_bloodnium_armed,1,__ATOMIC_RELEASE);
    log_ascii("AP LEVEL BLOODNIUM R75: GetBodyLvlBloodnium +0x368 hook installed; AP price=999999999 at every supported level.");
    return 1;
}

/* Initializes every progression level in the active BrgDbChara to the YAML
   starting state.  Main stats use native minimum level 1; Skill/Bag/Rage use
   native minimum level 0.  This runs once per newly observed Chara/Body pointer,
   so future AP items can increment these fields without the watcher undoing it. */
static void v074_initialize_progression_levels(void*record,const char*which){
    int old_lvl,old_hp,old_str,old_dex,old_vit,old_stm,old_luk,old_skill,old_bag,old_rage;
    char line[640];size_t pos=0;
    BYTE*r=(BYTE*)record;
    if(!r||!readable_range(r,0x174))return;
    old_lvl=*(int*)(r+0x14c);
    old_hp=*(int*)(r+0x150);
    old_str=*(int*)(r+0x154);
    old_dex=*(int*)(r+0x158);
    old_vit=*(int*)(r+0x15c);
    old_stm=*(int*)(r+0x160);
    old_luk=*(int*)(r+0x164);
    old_skill=*(int*)(r+0x168);
    old_bag=*(int*)(r+0x16c);
    old_rage=*(int*)(r+0x170);

    *(int*)(r+0x14c)=1;
    *(int*)(r+0x150)=1;
    *(int*)(r+0x154)=1;
    *(int*)(r+0x158)=1;
    *(int*)(r+0x15c)=1;
    *(int*)(r+0x160)=1;
    *(int*)(r+0x164)=1;
    *(int*)(r+0x168)=0;
    *(int*)(r+0x16c)=0;
    *(int*)(r+0x170)=0;

    line[0]=0;
    append_ascii(line,sizeof(line),&pos,"V0.74 LEVELS: ");
    append_ascii(line,sizeof(line),&pos,which);
    append_ascii(line,sizeof(line),&pos," old=");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_lvl<0?0:old_lvl));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_hp<0?0:old_hp));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_stm<0?0:old_stm));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_str<0?0:old_str));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_dex<0?0:old_dex));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_vit<0?0:old_vit));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_luk<0?0:old_luk));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_skill<0?0:old_skill));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_bag<0?0:old_bag));append_ascii(line,sizeof(line),&pos,"/");
    append_dec(line,sizeof(line),&pos,(unsigned)(old_rage<0?0:old_rage));
    append_ascii(line,sizeof(line),&pos," -> 1/1/1/1/1/1/1/0/0/0 record=");
    append_hex64(line,sizeof(line),&pos,(ULONG_PTR)record);
    line[pos]=0;log_ascii(line);
}

/* Replaces exact vanilla BAL status rows in the already-loaded master cache.
   The scan stops permanently once all YAML-required levels and the Grade-6
   body-detail capacity row have been found. */
#include "status_scan_fast.h"
#include "status_scan_guard.h"
#if LID_FAST_MASTER_SCAN_ENABLED
static void v074_fast_scan_log(void*context,int width,int grade,int row,void*address){
    unsigned*logged=context;if(*logged>=48)return;
    v074_log_match(width==32?(grade==6?"int32 G6":"int32 G1"):(grade==6?"int64 G6":"int64 G1"),row+1,address);(*logged)++;
}
#endif
static unsigned v074_patch_runtime_master_rows(void){
    BYTE*cursor=(BYTE*)(ULONG_PTR)0x10000;
    MEMORY_BASIC_INFORMATION mbi;
    unsigned matches=0;
    unsigned long long distinct_mask=0;
    unsigned logged=0;
    int required=g_v074_cfg.max_levels;
#if LID_FAST_MASTER_SCAN_ENABLED && LID_MASTER_SCAN_GUARD_ENABLED
    unsigned read_faults=0;
#endif
    LID_PERF_DETAIL_BEGIN();
    if(!g_v074_cfg.valid||required<1||required>45||!g_win.VirtualQuery)return 0;
#if LID_FAST_MASTER_SCAN_ENABLED && LID_MASTER_SCAN_GUARD_ENABLED
    if(!lid_master_scan_guard_init(lid_get_export(lid_find_module_ascii("ntdll.dll"),"__C_specific_handler"))){
        log_ascii("AP STATS R38: local scan guard unavailable; unsafe scan refused.");
        return 0;
    }
#endif
#if LID_FAST_MASTER_SCAN_ENABLED
    int target6[45][10],target1[5][10],level;
    LidMasterScanTables fast={g_v074_vanilla_grade6,g_v074_vanilla_grade1,target6,target1,required,&g_v074_patched_level_mask,&logged,v074_fast_scan_log};
    for(level=0;level<required;level++)v074_build_target_row(level+1,g_v074_vanilla_grade6[level],target6[level]);
    for(level=0;level<5;level++)v074_build_target_row(level+1,g_v074_vanilla_grade1[level],target1[level]);
#endif

    while((ULONG_PTR)cursor<0x00007fffffff0000ULL){
        LID_PERF_DETAIL(3,1);
        SIZE_T q=g_win.VirtualQuery(cursor,&mbi,sizeof(mbi));
        BYTE*base;
        BYTE*end;
        ULONG_PTR next;
        if(!q)break;
        base=(BYTE*)mbi.BaseAddress;
        next=(ULONG_PTR)base+(ULONG_PTR)mbi.RegionSize;
        if(next<=(ULONG_PTR)cursor)break;

        if(mbi.State==MEM_COMMIT&&v074_writable_region(mbi.Protect)&&mbi.RegionSize>=40){
#if LID_FAST_MASTER_SCAN_ENABLED
            LID_PERF_DETAIL(4,1);LID_PERF_DETAIL(5,(unsigned long long)mbi.RegionSize);
            LID_PERF_BEGIN(region_time);
#if LID_MASTER_SCAN_GUARD_ENABLED
            LidMasterScanGuardResult guarded=lid_master_scan_guarded(base,(size_t)mbi.RegionSize,&fast);
            matches+=guarded.matches;read_faults+=guarded.read_faults;
#else
            matches+=lid_master_scan_region(base,(size_t)mbi.RegionSize,&fast);
#endif
            LID_PERF_END(region_time,38,"master_scan_regions",1);
#else
            BYTE*p=(BYTE*)(((ULONG_PTR)base+3u)&~(ULONG_PTR)3u);
            end=base+(size_t)mbi.RegionSize;

            while(p+40<=end){
                int hp=*(int*)p;
                int row=v074_grade6_row_from_hp(hp);
                if(row>=0&&row<required&&v074_match32_table((const int*)p,g_v074_vanilla_grade6,row)){
                    int j,target[10];v074_build_target_row(row+1,g_v074_vanilla_grade6[row],target);
                    for(j=0;j<10;j++)((int*)p)[j]=target[j];
                    matches++;distinct_mask|=(1ULL<<row);g_v074_patched_level_mask|=(1ULL<<row);
                    if(logged<48){v074_log_match("int32 G6",row+1,p);logged++;}
                    p+=40;continue;
                }
                row=v074_grade1_row_from_hp(hp);
                if(row>=0&&v074_match32_table((const int*)p,g_v074_vanilla_grade1,row)){
                    int j,target[10];v074_build_target_row(row+1,g_v074_vanilla_grade1[row],target);
                    for(j=0;j<10;j++)((int*)p)[j]=target[j];
                    matches++;
                    if(logged<48){v074_log_match("int32 G1",row+1,p);logged++;}
                    p+=40;continue;
                }
                p+=4;
            }

            p=(BYTE*)(((ULONG_PTR)base+7u)&~(ULONG_PTR)7u);
            while(p+80<=end){
                long long hp=*(long long*)p;
                int row=(hp>=(-2147483647LL-1)&&hp<=2147483647LL)?v074_grade6_row_from_hp((int)hp):-1;
                if(row>=0&&row<required&&v074_match64_table((const long long*)p,g_v074_vanilla_grade6,row)){
                    int j,target[10];v074_build_target_row(row+1,g_v074_vanilla_grade6[row],target);
                    for(j=0;j<10;j++)((long long*)p)[j]=(long long)target[j];
                    matches++;distinct_mask|=(1ULL<<row);g_v074_patched_level_mask|=(1ULL<<row);
                    if(logged<48){v074_log_match("int64 G6",row+1,p);logged++;}
                    p+=80;continue;
                }
                row=(hp>=(-2147483647LL-1)&&hp<=2147483647LL)?v074_grade1_row_from_hp((int)hp):-1;
                if(row>=0&&v074_match64_table((const long long*)p,g_v074_vanilla_grade1,row)){
                    int j,target[10];v074_build_target_row(row+1,g_v074_vanilla_grade1[row],target);
                    for(j=0;j<10;j++)((long long*)p)[j]=(long long)target[j];
                    matches++;
                    if(logged<48){v074_log_match("int64 G1",row+1,p);logged++;}
                    p+=80;continue;
                }

                p+=8;
            }
#endif
        }
        cursor=(BYTE*)next;
    }

    {
        char line[420];size_t pos=0;unsigned i,levels=0;line[0]=0;
        for(i=0;i<(unsigned)required;i++)if(g_v074_patched_level_mask&(1ULL<<i))levels++;
        if(levels==(unsigned)required)g_v074_status_rows_ready=1;
        append_ascii(line,sizeof(line),&pos,"V0.74 DYNAMIC STATS: pass ");
        append_dec(line,sizeof(line),&pos,++g_v074_patch_passes);
        append_ascii(line,sizeof(line),&pos," complete; matched copies=");
        append_dec(line,sizeof(line),&pos,matches);
        append_ascii(line,sizeof(line),&pos," distinct_levels=");
        append_dec(line,sizeof(line),&pos,levels);
        append_ascii(line,sizeof(line),&pos,"/");
        append_dec(line,sizeof(line),&pos,(unsigned)required);
        line[pos]=0;log_ascii(line);
    }
    g_v074_total_matches+=matches;
#if LID_FAST_MASTER_SCAN_ENABLED && LID_MASTER_SCAN_GUARD_ENABLED
    if(read_faults){
        char line[180];size_t pos=0;line[0]=0;
        append_ascii(line,sizeof(line),&pos,"AP STATS R38: inaccessible scan regions skipped=");
        append_dec(line,sizeof(line),&pos,read_faults);line[pos]=0;log_ascii(line);
    }
#endif
    return matches;
}

static int append_ascii_wide(wchar_t*out,size_t cap,const char*ascii){size_t i=0,n=lid_wcslen(out);if(!ascii)return 0;while(ascii[i]){if(n+1>=cap)return 0;out[n++]=(wchar_t)(unsigned char)ascii[i++];}out[n]=0;return 1;}
static void append_wchar(wchar_t*out,size_t cap,wchar_t c){size_t n=lid_wcslen(out);if(n+1<cap){out[n]=c;out[n+1]=0;}}
static void append_dec_w(wchar_t*out,size_t cap,DWORD v){wchar_t tmp[24];size_t n=0,i;if(v==0){append_wchar(out,cap,L'0');return;}while(v&&n<23){tmp[n++]=(wchar_t)(L'0'+(v%10));v/=10;}for(i=0;i<n;i++)append_wchar(out,cap,tmp[n-1-i]);}
static void append_quoted_arg(wchar_t*out,size_t cap,const wchar_t*arg){size_t i=0,slashes=0,j;append_wchar(out,cap,L'"');while(arg&&arg[i]){if(arg[i]==L'\\'){slashes++;i++;continue;}if(arg[i]==L'"'){for(j=0;j<slashes*2+1;j++)append_wchar(out,cap,L'\\');append_wchar(out,cap,L'"');slashes=0;i++;continue;}for(j=0;j<slashes;j++)append_wchar(out,cap,L'\\');slashes=0;append_wchar(out,cap,arg[i++]);}for(j=0;j<slashes*2;j++)append_wchar(out,cap,L'\\');append_wchar(out,cap,L'"');}
static int write_utf16_piece(HANDLE h,const wchar_t*s){DWORD wrote=0;DWORD bytes=(DWORD)(lid_wcslen(s)*sizeof(wchar_t));return g_win.WriteFile&&g_win.WriteFile(h,s,bytes,&wrote,0)&&wrote==bytes;}
static int write_connection_request(wchar_t*request_path,size_t request_cap,wchar_t*result_path,size_t result_cap){wchar_t win64[900],root[900],pidline[64];HANDLE h;WORD bom=0xFEFF;DWORD wrote=0;game_dir(win64,900);lid_memcpy(root,win64,(lid_wcslen(win64)+1)*sizeof(wchar_t));trim_parent_dir(root);trim_parent_dir(root);lid_memcpy(request_path,win64,(lid_wcslen(win64)+1)*sizeof(wchar_t));lid_wappend(request_path,request_cap,L"\\Archipelago\\connection_request.lidapconnect");lid_memcpy(result_path,win64,(lid_wcslen(win64)+1)*sizeof(wchar_t));lid_wappend(result_path,result_cap,L"\\Archipelago\\connection_result.json");if(g_win.DeleteFileW)g_win.DeleteFileW(result_path);h=g_win.CreateFileW(request_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return 0;if(!g_win.WriteFile(h,&bom,sizeof(bom),&wrote,0)){g_win.CloseHandle(h);return 0;}if(!write_utf16_piece(h,L"server=")||!write_utf16_piece(h,g_server)||!write_utf16_piece(h,L"\r\nplayer=")||!write_utf16_piece(h,g_player)||!write_utf16_piece(h,L"\r\ngame_root=")||!write_utf16_piece(h,root)||!write_utf16_piece(h,L"\r\npid=")){g_win.CloseHandle(h);return 0;}pidline[0]=0;append_dec_w(pidline,64,g_win.GetCurrentProcessId?g_win.GetCurrentProcessId():0);if(!write_utf16_piece(h,pidline)||!write_utf16_piece(h,L"\r\n")){g_win.CloseHandle(h);return 0;}g_win.CloseHandle(h);return 1;}
static int find_archipelago_launcher(wchar_t*out,size_t cap){wchar_t p[900];char drive;HANDLE h;/* User project historically uses D:\\Archipelago; scan all drive roots first. */for(drive='C';drive<='Z';drive++){p[0]=(wchar_t)drive;p[1]=L':';p[2]=L'\\';p[3]=0;lid_wappend(p,900,L"Archipelago\\ArchipelagoLauncher.exe");h=g_win.CreateFileW(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(h!=INVALID_HANDLE_VALUE){g_win.CloseHandle(h);lid_memcpy(out,p,(lid_wcslen(p)+1)*sizeof(wchar_t));return 1;}}/* Official installer default. */lid_memcpy(p,L"C:\\ProgramData\\Archipelago\\ArchipelagoLauncher.exe",(lid_wcslen(L"C:\\ProgramData\\Archipelago\\ArchipelagoLauncher.exe")+1)*sizeof(wchar_t));h=g_win.CreateFileW(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(h!=INVALID_HANDLE_VALUE){g_win.CloseHandle(h);lid_memcpy(out,p,(lid_wcslen(p)+1)*sizeof(wchar_t));return 1;}return 0;}
/* Extracts one quoted JSON string value from the small bridge result document. */
static int extract_json_string(const char*buf,size_t got,const char*key,char*out,size_t cap){size_t i,klen=lid_strlen(key);if(!buf||!key||!out||cap<2)return 0;out[0]=0;for(i=0;i+klen<got;i++){size_t j,pos,n=0;if(lid_memcmp(buf+i,key,klen)!=0)continue;pos=i+klen;while(pos<got&&(buf[pos]==' '||buf[pos]=='\t'||buf[pos]=='\r'||buf[pos]=='\n'))pos++;if(pos>=got||buf[pos]!=':')continue;pos++;while(pos<got&&(buf[pos]==' '||buf[pos]=='\t'||buf[pos]=='\r'||buf[pos]=='\n'))pos++;if(pos>=got||buf[pos]!='"')continue;pos++;for(j=pos;j<got&&buf[j]!='"'&&n+1<cap;j++){char c=buf[j];if(c=='\\')return 0;out[n++]=c;}if(j>=got||buf[j]!='"')return 0;out[n]=0;return n>0;}return 0;}

/* Extracts one integer JSON value from the small bridge result document. */
static int extract_json_int(const char*buf,size_t got,const char*key,int*out){
    size_t i,klen=lid_strlen(key);
    if(!buf||!key||!out)return 0;
    for(i=0;i+klen<got;i++){
        size_t pos;int sign=1;long long value=0;int digits=0;
        if(lid_memcmp(buf+i,key,klen)!=0)continue;
        pos=i+klen;
        while(pos<got&&(buf[pos]==' '||buf[pos]=='\t'||buf[pos]=='\r'||buf[pos]=='\n'))pos++;
        if(pos>=got||buf[pos]!=':')continue;
        pos++;
        while(pos<got&&(buf[pos]==' '||buf[pos]=='\t'||buf[pos]=='\r'||buf[pos]=='\n'))pos++;
        if(pos<got&&buf[pos]=='-'){sign=-1;pos++;}
        while(pos<got&&buf[pos]>='0'&&buf[pos]<='9'){
            value=value*10+(buf[pos]-'0');digits++;pos++;
            if(value>2147483647LL+(sign<0))return 0;
        }
        if(!digits)return 0;
        *out=(int)(sign*value);
        return 1;
    }
    return 0;
}

/* Parses the exact generated slot_data copied by Client.py into
   connection_result.json. Missing values are fatal: V0.74 never silently falls
   back to compiled starter stats. */
static int v074_parse_runtime_config(const char*buf,size_t got){
    V074RuntimeConfig c;
    int maxq=0;
    lid_memset(&c,0,sizeof(c));
#define NEED_INT(JSON_KEY,FIELD) do{if(!extract_json_int(buf,got,JSON_KEY,&c.FIELD))return 0;}while(0)
    NEED_INT("\"starting_fighter_grade\"",fighter_grade);
    NEED_INT("\"starting_hp\"",starting_hp);
    NEED_INT("\"starting_stm\"",starting_stm);
    NEED_INT("\"starting_str\"",starting_str);
    NEED_INT("\"starting_dex\"",starting_dex);
    NEED_INT("\"starting_vit\"",starting_vit);
    NEED_INT("\"starting_luk\"",starting_luk);
    NEED_INT("\"starting_skill_slots\"",starting_skill_slots);
    NEED_INT("\"starting_death_bag\"",starting_death_bag);
    NEED_INT("\"starting_rage_gauge\"",starting_rage_gauge);

    NEED_INT("\"progressive_hp_increase\"",hp_increase);
    NEED_INT("\"progressive_stm_increase\"",stm_increase);
    NEED_INT("\"progressive_str_increase\"",str_increase);
    NEED_INT("\"progressive_dex_increase\"",dex_increase);
    NEED_INT("\"progressive_vit_increase\"",vit_increase);
    NEED_INT("\"progressive_luck_increase\"",luk_increase);
    NEED_INT("\"progressive_death_bag_increase\"",death_bag_increase);
    NEED_INT("\"progressive_skill_slots_increase\"",skill_slots_increase);
    NEED_INT("\"progressive_rage_gauge_increase\"",rage_gauge_increase);

    NEED_INT("\"progressive_hp_quantity\"",hp_quantity);
    NEED_INT("\"progressive_stm_quantity\"",stm_quantity);
    NEED_INT("\"progressive_str_quantity\"",str_quantity);
    NEED_INT("\"progressive_dex_quantity\"",dex_quantity);
    NEED_INT("\"progressive_vit_quantity\"",vit_quantity);
    NEED_INT("\"progressive_luck_quantity\"",luk_quantity);
    NEED_INT("\"progressive_death_bag_quantity\"",death_bag_quantity);
    NEED_INT("\"progressive_skill_slots_quantity\"",skill_slots_quantity);
    NEED_INT("\"progressive_rage_gauge_quantity\"",rage_gauge_quantity);
#undef NEED_INT

    if(c.fighter_grade!=6)return 0;
    if(c.starting_hp<1||c.starting_stm<1||c.starting_str<0||c.starting_dex<0||
       c.starting_vit<0||c.starting_luk<0||c.starting_skill_slots<0||
       c.starting_skill_slots>20||c.starting_death_bag<1||c.starting_rage_gauge<0)return 0;
    if(c.hp_quantity<0||c.stm_quantity<0||c.str_quantity<0||c.dex_quantity<0||
       c.vit_quantity<0||c.luk_quantity<0||c.death_bag_quantity<0||
       c.skill_slots_quantity<0||c.rage_gauge_quantity<0)return 0;

    if(c.hp_quantity>maxq)maxq=c.hp_quantity;
    if(c.stm_quantity>maxq)maxq=c.stm_quantity;
    if(c.str_quantity>maxq)maxq=c.str_quantity;
    if(c.dex_quantity>maxq)maxq=c.dex_quantity;
    if(c.vit_quantity>maxq)maxq=c.vit_quantity;
    if(c.luk_quantity>maxq)maxq=c.luk_quantity;
    if(c.death_bag_quantity>maxq)maxq=c.death_bag_quantity;
    if(c.skill_slots_quantity>maxq)maxq=c.skill_slots_quantity;
    if(c.rage_gauge_quantity>maxq)maxq=c.rage_gauge_quantity;
    if(maxq>44)return 0;
    c.max_levels=1+maxq;
    c.valid=1;
    g_v074_cfg=c;
    return 1;
}

/* Reads the bridge result status and, on success, its dedicated AP slot key and
   exact runtime configuration. */
static int result_status_and_slot(const wchar_t*path,char*slot_key,size_t slot_cap){
    HANDLE h;char*buf=g_bridge_result_buf;DWORD got=0;size_t i;int status=-1;
    if(slot_key&&slot_cap)slot_key[0]=0;
    lid_memset(&g_v074_cfg,0,sizeof(g_v074_cfg));
    h=g_win.CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE)return 0;
    lid_memset(buf,0,4096);
    if(!g_win.ReadFile(h,buf,4095,&got,0)){g_win.CloseHandle(h);return -1;}
    g_win.CloseHandle(h);
    for(i=0;i+14<got;i++){
        if(lid_memcmp(buf+i,"\"status\": \"ok\"",14)==0||lid_memcmp(buf+i,"\"status\":\"ok\"",13)==0){status=1;break;}
        if(lid_memcmp(buf+i,"\"status\": \"error\"",17)==0||lid_memcmp(buf+i,"\"status\":\"error\"",16)==0){status=-1;break;}
    }
    if(status==1){
        if(!slot_key||!slot_cap||!extract_json_string(buf,got,"\"slot_key\"",slot_key,slot_cap))return -2;
        if(!v074_parse_runtime_config(buf,got))return -3;
    }
    return status;
}

/* V0.59: preserve the freshly generated AP save while the already-loaded vanilla userdata is released. */
static int copy_file_direct(const wchar_t*src,const wchar_t*dst){HANDLE in_h,out_h;DWORD got=0,wrote=0,off;if(!g_win.CreateFileW||!g_win.ReadFile||!g_win.WriteFile||!g_win.CloseHandle)return 0;in_h=g_win.CreateFileW(src,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(in_h==INVALID_HANDLE_VALUE)return 0;out_h=g_win.CreateFileW(dst,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(out_h==INVALID_HANDLE_VALUE){g_win.CloseHandle(in_h);return 0;}for(;;){got=0;if(!g_win.ReadFile(in_h,g_release_copy_buffer,sizeof(g_release_copy_buffer),&got,0)){g_win.CloseHandle(out_h);g_win.CloseHandle(in_h);return 0;}if(!got)break;off=0;while(off<got){wrote=0;if(!g_win.WriteFile(out_h,g_release_copy_buffer+off,got-off,&wrote,0)||!wrote){g_win.CloseHandle(out_h);g_win.CloseHandle(in_h);return 0;}off+=wrote;}}g_win.CloseHandle(out_h);g_win.CloseHandle(in_h);return 1;}
static int build_release_paths(void){game_dir(g_release_save_path,1000);lid_wappend(g_release_save_path,1000,L"\\Archipelago\\slots\\");if(!append_ascii_wide(g_release_save_path,1000,g_release_slot_key))return 0;lid_wappend(g_release_save_path,1000,L"\\brggame.sav");lid_memcpy(g_release_backup_path,g_release_save_path,(lid_wcslen(g_release_save_path)+1)*sizeof(wchar_t));lid_wappend(g_release_backup_path,1000,L".releasebak");return 1;}
/* Finds the live BrgUserData object in all UE3 packages; used only after AP authentication. */
static void* find_live_brg_userdata(void){void*exe=lid_find_module_ascii("BrgGame-Steam.exe");StaticFindObjectFn find_object;DWORD i;if(!exe)return 0;find_object=(StaticFindObjectFn)((BYTE*)exe+0x16D230);if(!executable_ptr(exe,(void*)find_object))return 0;for(i=0;i<16;i++){wchar_t name[64];void*object;void**vt;NativeUserDataBool is_loaded;lid_memcpy(name,L"BrgUserData_",(lid_wcslen(L"BrgUserData_")+1)*sizeof(wchar_t));append_dec_w(name,64,i);object=find_object(0,(void*)(LONG_PTR)-1,name,0);if(!object)continue;if(!readable_range(object,sizeof(void*)))continue;vt=*(void***)object;if(!vt||!readable_range(vt,0x290))continue;is_loaded=(NativeUserDataBool)vt[0x270/sizeof(void*)];if(!is_loaded||!executable_ptr(exe,(void*)is_loaded))continue;if(is_loaded(object)>0)return object;}return 0;}



/* V0.82 fixes the Death Bag list at the exact native source used by the live
   Death Bag menu.  Static analysis of BrgGame-Steam.exe proves this chain:
     BrgUIMenu_DeathBag::TickASync
       -> BrgUIMenuPart_ItemSelect::AddUnitForDeathBag (RVA 0x14BA450)
       -> AddUnitFromDeathBag
       -> ABrgCommonPawn_CustomCharaNative::GetPossessionItem (vtable +0x8C0)
   The GetPossessionItem call returns a TArray whose elements are 0x728 bytes;
   AddUnitFromDeathBag creates one visible UI cell for every element in that
   array.  V0.82 therefore trims only that returned UI list to the YAML/AP
   Death Bag capacity, using LET IT DIE's own TArray remove/destructor helper. */
typedef int (WINAPI *V082AddUnitForDeathBagFn)(void*,int,int,int,int,int);
typedef void (WINAPI *V082PawnGetPossessionItemFn)(void*,RawArray*,int,int,int,int,int);
typedef void (WINAPI *V082RemovePossessionEntriesFn)(RawArray*,int,int);

static Hook h_v082_add_unit_for_deathbag;
static V082AddUnitForDeathBagFn g_v082_add_unit_for_deathbag_original=0;
static V082PawnGetPossessionItemFn g_v082_pawn_get_possession_original=0;
static void** g_v082_pawn_get_possession_slot=0;
static void** g_v082_pawn_vtable=0;
static int g_v082_add_unit_hook_ready=0;
static int g_v082_pawn_hook_ready=0;
static unsigned g_v082_list_log_budget=12;

/* Computes only the current AP Death Bag capacity from the validated
   zero-based Bag progression level already tracked by V0.76. */
static int v082_current_bag_capacity(void){
    int bag_level=0;
#if LID_NATIVE_RELOAD_ENABLED
    LidReloadCapacityValues restored;int restoring=progress_reload_capacity(&restored),target;
    if(restoring<0)return 99; /* Never trim an unproved startup UI list. */
    if(restoring){
        target=v076_capacity_from_level(g_v074_cfg.starting_death_bag,g_v074_cfg.death_bag_increase,
            g_v074_cfg.death_bag_quantity,restored.bag_level);
        return target>restored.bag_floor?target:restored.bag_floor;
    }
#endif
    v076_get_active_capacity_levels(0,&bag_level,0);
    return v076_capacity_from_level(g_v074_cfg.starting_death_bag,
                                    g_v074_cfg.death_bag_increase,
                                    g_v074_cfg.death_bag_quantity,
                                    bag_level);
}

#include "bag_capacity_policy.h"
static int rage_player_objects(void*exe,BYTE**out_pawn,BYTE**out_common);
typedef int (WINAPI *R40BagSpaceFn)(void*,int);
static R40BagSpaceFn g_r40_bag_space_original;
static R40BagSpaceFn g_r45_bag_size_original;
#if LID_NATIVE_RELOAD_ENABLED
static int progress_reload_delivery_ready(void);
#endif
/* Derive ownership from the object receiving the call, not the currently
   displayed world's global GameInfo or a fighter-type string. The exact
   player class and reciprocal CommonPawn/Pawn link reject AI/preview pawns. */
static int r45_bag_player_scope(void*self){
    BYTE*exe=lid_find_module_ascii("BrgGame-Steam.exe");BYTE*pawn;
    if(!exe||!self||!g_ap_mode_active||!g_v074_cfg.valid||
       !readable_range(self,8)||*(void**)self!=exe+0x20ac950||
       !readable_range((BYTE*)self+0x284,8))return 0;
    pawn=*(BYTE**)((BYTE*)self+0x284);
    return pawn&&readable_range(pawn,8)&&*(void**)pawn==exe+0x21b9730&&
        readable_range(pawn+0x17f8,8)&&*(void**)(pawn+0x17f8)==self;
}
/* A preserved overflow is not extra capacity: only expose existing objects
   so the player can remove them. No writes or borrowed retained entries. */
static int r40_bag_occupied_floor(void*self){
    RawArray a;int floor=0;
    if(!self||!readable_range((BYTE*)self+0x3e58,sizeof(a)))return -1;
    lid_memcpy(&a,(BYTE*)self+0x3e58,sizeof(a));
    if(a.num<0||a.num>99||a.max<a.num||a.max>4096||(!a.data)!=(a.max==0)||
       (a.num&&!readable_range(a.data,(size_t)a.num*0x718)))return -1;
    for(int i=0;i<a.num;i++)if(*((BYTE*)a.data+(size_t)i*0x718)&1)floor=i+1;
    return floor;
}
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
static int checks_material_bag_request(void*,int,uint64_t,int,int*);
#endif
static int WINAPI r40_get_bag_space(void*self,int index){
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
    uint64_t material_caller=(uint64_t)(ULONG_PTR)__builtin_return_address(0);
#endif
    R40BagSpaceFn original=__atomic_load_n(&g_r40_bag_space_original,__ATOMIC_ACQUIRE);
    LID_PERF_BEGIN(pickup_native_space_time);
    int result=original?original(self,index):-1;
    LID_PERF_END(pickup_native_space_time,45,"pickup_native_bag_space",result);
    if(index>=0||!original||!r45_bag_player_scope(self))return result;
#if LID_NATIVE_RELOAD_ENABLED
    if(!progress_reload_delivery_ready())return result;
#endif
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
    int material_result;
    int material_native_space=r40_bag_space(result,v082_current_bag_capacity(),r40_bag_occupied_floor(self));
    if(checks_material_bag_request(self,index,material_caller,material_native_space,&material_result))return material_result;
    return material_native_space;
#endif
    return r40_bag_space(result,v082_current_bag_capacity(),r40_bag_occupied_floor(self));
}
static int WINAPI r45_get_bag_size(void*self,int index){
    R40BagSpaceFn original=__atomic_load_n(&g_r45_bag_size_original,__ATOMIC_ACQUIRE);
    LID_PERF_BEGIN(pickup_native_size_time);
    int result=original?original(self,index):0;
    LID_PERF_END(pickup_native_size_time,46,"pickup_native_bag_size",result);
    if(index>=0||!original||!r45_bag_player_scope(self))return result;
#if LID_NATIVE_RELOAD_ENABLED
    if(!progress_reload_delivery_ready())return result;
#endif
    return r45_bag_size(result,v082_current_bag_capacity(),r40_bag_occupied_floor(self));
}
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
static int checks_material_early_install(void*);
#endif
static int r40_install_bag_space_guard(void*common){
    BYTE*exe=lid_find_module_ascii("BrgGame-Steam.exe");void**vt;void*previous=0;int size_changed=0;
    if(!exe||!common||!readable_range(common,8))return 0;vt=*(void***)common;
    if(vt!=(void**)(exe+0x20ac950)||!readable_range(vt,0x8e0))return 0;
    if((vt[0x8c8/8]!=exe+0xfa3600&&vt[0x8c8/8]!=(void*)r45_get_bag_size)||
       (vt[0x8d8/8]!=exe+0xfa3640&&vt[0x8d8/8]!=(void*)r40_get_bag_space))return 0;
    if(vt[0x8c8/8]==(void*)r45_get_bag_size&&vt[0x8d8/8]==(void*)r40_get_bag_space)
        return g_r45_bag_size_original&&g_r40_bag_space_original;
    if(!executable_ptr(exe,exe+0xfa3600)||!executable_ptr(exe,exe+0xfa3640))return 0;
    __atomic_store_n(&g_r45_bag_size_original,(R40BagSpaceFn)(exe+0xfa3600),__ATOMIC_RELEASE);
    __atomic_store_n(&g_r40_bag_space_original,(R40BagSpaceFn)(exe+0xfa3640),__ATOMIC_RELEASE);
    if(vt[0x8c8/8]!=(void*)r45_get_bag_size){
        if(!v076_write_vtable_slot(&vt[0x8c8/8],(void*)r45_get_bag_size,&previous))return 0;
        size_changed=1;
    }
    if(vt[0x8d8/8]!=(void*)r40_get_bag_space&&
       !v076_write_vtable_slot(&vt[0x8d8/8],(void*)r40_get_bag_space,&previous)){
        if(size_changed)v076_write_vtable_slot(&vt[0x8c8/8],exe+0xfa3600,&previous);
        return 0;
    }
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
    (void)checks_material_early_install(exe);
#endif
    log_ascii("AP DEATH BAG R45: player-owned size and free-slot guards ready for hub and tower; existing objects preserved.");
    return 1;
}
/* Trims the exact 0x728-byte possession/UI entry array produced for the live
   Death Bag menu.  The native remove helper at RVA 0xFB8610 invokes the proper
   destructors for every discarded entry and updates TArray::Num itself. */
static void WINAPI v082_pawn_get_possession_item(void*self,RawArray*out_items,
                                                  int a,int b,int c,int d,int e){
    int before=0,target=0;
    void*exe;
    V082RemovePossessionEntriesFn remove_entries;
    if(g_v082_pawn_get_possession_original)
        g_v082_pawn_get_possession_original(self,out_items,a,b,c,d,e);

    if(!g_ap_mode_active||!g_v074_cfg.valid||!out_items)return;
    /* Exact argument tuple used by ItemSelect::AddUnitFromDeathBag when
       AddUnitForDeathBag is called by BrgUIMenu_DeathBag::TickASync. */
    if(a!=0||b!=0||c!=1||d!=0||e!=-1)return;
    if(!readable_range(out_items,sizeof(RawArray)))return;
    before=out_items->num;
    if(before<=0||out_items->max<before||out_items->max>4096)return;
    target=v082_current_bag_capacity();
    {
        int occupied=r40_bag_occupied_floor(self);
        if(occupied<0)target=before;
        else if(occupied>target)target=occupied;
    }
    if(target<0)target=0;
    if(target>=before)return;

    exe=lid_find_module_ascii("BrgGame-Steam.exe");
    if(!exe)return;
    remove_entries=(V082RemovePossessionEntriesFn)((BYTE*)exe+0xFB8610);
    if(!executable_ptr(exe,(void*)remove_entries))return;
    remove_entries(out_items,target,before-target);

    if(g_v082_list_log_budget){
        char line[320];size_t pos=0;g_v082_list_log_budget--;line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.82 DEATH BAG UI LIST: GetPossessionItem entries ");
        append_dec(line,sizeof(line),&pos,(unsigned)before);
        append_ascii(line,sizeof(line),&pos,"->");
        append_dec(line,sizeof(line),&pos,(unsigned)(out_items->num<0?0:out_items->num));
        append_ascii(line,sizeof(line),&pos," target=");
        append_dec(line,sizeof(line),&pos,(unsigned)target);
        line[pos]=0;log_ascii(line);
    }
}

/* Installs the +0x8C0 GetPossessionItem hook on the exact CustomChara pawn
   reached by AddUnitFromDeathBag: ItemSelect+0x68 -> owner+0x2D4C -> pawn. */
static int v082_install_pawn_hook_from_itemselect(void*item_select){
    BYTE*owner;
    void*pawn;
    void**vt;
    void**slot;
    void*original=0;
    void*exe=lid_find_module_ascii("BrgGame-Steam.exe");
    if(g_v082_pawn_hook_ready)return 1;
    if(!item_select||!exe||!readable_range((BYTE*)item_select+0x68,sizeof(void*)))return 0;
    owner=*(BYTE**)((BYTE*)item_select+0x68);
    if(!owner||!readable_range(owner+0x2D4C,sizeof(void*)))return 0;
    pawn=*(void**)(owner+0x2D4C);
    if(!pawn||!readable_range(pawn,sizeof(void*)))return 0;
    vt=*(void***)pawn;
    if(!vt||!readable_range(vt,0x8C8))return 0;
    slot=&vt[0x8C0/sizeof(void*)];
    if(!*slot||!executable_ptr(exe,*slot))return 0;
    if(!v076_write_vtable_slot(slot,(void*)v082_pawn_get_possession_item,&original))return 0;
    g_v082_pawn_get_possession_original=(V082PawnGetPossessionItemFn)original;
    g_v082_pawn_get_possession_slot=slot;
    g_v082_pawn_vtable=vt;
    g_v082_pawn_hook_ready=1;
    log_ascii("V0.82 DEATH BAG: CustomCharaNative::GetPossessionItem vtable +0x8C0 hook installed from proven ItemSelect path.");
    return 1;
}

/* Runs immediately before the game's proven AddUnitForDeathBag thunk.  Its
   only job is to expose the real ItemSelect instance early enough to install
   the pawn GetPossessionItem hook before AddUnitFromDeathBag requests the list. */
static int WINAPI v082_add_unit_for_deathbag(void*self,int p1,int p2,int p3,int p4,int p5){
    if(g_ap_mode_active&&!g_v082_pawn_hook_ready)
        v082_install_pawn_hook_from_itemselect(self);
    return g_v082_add_unit_for_deathbag_original?
        g_v082_add_unit_for_deathbag_original(self,p1,p2,p3,p4,p5):0;
}

/* Installs a single exact native hook on AddUnitForDeathBag.  The 15-byte
   prefix is composed only of complete non-RIP-relative instructions in the
   validated Steam executable, so it is safe for the existing trampoline. */
static int v082_install_add_unit_for_deathbag_hook(void){
    void*exe=lid_find_module_ascii("BrgGame-Steam.exe");
    BYTE*target;
    static const BYTE prefix[]={
        0x48,0x8B,0x01,
        0x44,0x8B,0x4C,0x24,0x30,
        0x44,0x8B,0x44,0x24,0x28,
        0x33,0xD2
    };
    if(g_v082_add_unit_hook_ready)return 1;
    if(!g_ap_mode_active||!g_v074_cfg.valid||!exe)return 0;
    target=(BYTE*)exe+0x14BA450;
    if(!executable_ptr(exe,target))return 0;
    if(!hook_install(&h_v082_add_unit_for_deathbag,target,(void*)v082_add_unit_for_deathbag,prefix,sizeof(prefix)))return 0;
    g_v082_add_unit_for_deathbag_original=(V082AddUnitForDeathBagFn)h_v082_add_unit_for_deathbag.trampoline;
    g_v082_add_unit_hook_ready=1;
    log_ascii("V0.82 DEATH BAG: exact ItemSelect::AddUnitForDeathBag RVA 0x14BA450 hook installed.");
    return 1;
}

/* Keeps the current AP fighter synchronized in both memory representations that
   were reverse-engineered in V0.60-V0.62.  The important difference is timing:
   this function is called from every observed BrgUIMenu_Base Tick after AP mode
   is active, including after Game Start.  Therefore if LET IT DIE replaces the
   title-time Body/Chara with a newly loaded Grade-1 instance, that new instance
   is detected by pointer/value change and corrected in place. */
/* Rage capacity is a gameplay field, separate from GetBodyDetail and SklGauge.
   Verified only for executable SHA f33e91e6...17bddfd. Resolve the sole current
   player with the same native path used by the read-only runtime capture. */
typedef void (WINAPI *NativeRageBodyStatusFn)(void*,void*,RawFString*,int);
static Hook h_rage_body_status;
static NativeRageBodyStatusFn g_rage_body_status_original;
static int g_rage_body_status_ready;
static int g_rage_install_failure_logged;
static unsigned g_rage_log_budget=16;

#include "character_read_scope.h"
static int R157_READ_NAME(rage_player_objects)(void*exe,BYTE**out_pawn,BYTE**out_common){
    BYTE*engine;BYTE*world;BYTE*actors;BYTE*first;BYTE*game;BYTE*pawn;BYTE*common;
    void**vt;
    if(!exe||!out_pawn||!out_common)return 0;
    engine=*(BYTE**)((BYTE*)exe+0x29709F0);
    if(!engine||!readable_range(engine+0x80,sizeof(void*)))return 0;
    world=*(BYTE**)(engine+0x80);
    if(!world||!readable_range(world+0x60,sizeof(void*)))return 0;
    actors=*(BYTE**)(world+0x60);
    if(!actors||!readable_range(actors,sizeof(void*)))return 0;
    first=*(BYTE**)actors;
    if(!first||!readable_range(first+0x714,sizeof(void*)))return 0;
    game=*(BYTE**)(first+0x714);
    if(!game||!readable_range(game,0x8c4))return 0;
    vt=*(void***)game;
    if(!vt||!readable_range(vt,0x888)||vt[0x880/sizeof(void*)]!=(BYTE*)exe+0x10B36A0)return 0;
    pawn=*(BYTE**)(game+0x8bc);
    if(!pawn||!readable_range(pawn+0x17f8,sizeof(void*)))return 0;
    common=*(BYTE**)(pawn+0x17f8);
    if(!common||!readable_range(common,0x29c))return 0;
    if(*(void**)common!=(BYTE*)exe+0x20AC950||*(BYTE**)(common+0x284)!=pawn)return 0;
    if(!v074_skill_fstring_matches(common+0x294,"BAL"))return 0;
    *out_pawn=pawn;*out_common=common;
    return 1;
}

#if R157_READ_SCOPED
static int rage_player_objects(void*exe,BYTE**out_pawn,BYTE**out_common){
    int acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
    int result=rage_player_objects_read_core(exe,out_pawn,out_common);
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
    return result;
}
#endif
static int rage_ap_capacity(void){
    int level=0;
    v076_get_active_capacity_levels(0,0,&level);
    return v076_capacity_from_level(g_v074_cfg.starting_rage_gauge,
                                  g_v074_cfg.rage_gauge_increase,
                                  g_v074_cfg.rage_gauge_quantity,level);
}

/* Existing game load owns restoration of persisted BodyLvl. The progressive
   mode must never turn a newly loaded record back into1/0. Invalid/inconsistent
   records defer cache synchronization instead of erasing positive charge. */
static int WINAPI progress_loaded_readable(const void* p,size_t n){return readable_range(p,n);}
static int R157_READ_NAME(progress_loaded_levels_ready)(BYTE* chara,BYTE* body){
    LidGameBagContext context={0};int i;
    context.readable=progress_loaded_readable;
    if(!chara||!body||!readable_range(chara,0x174)||!readable_range(body,0x174)||
       !lid_character_levels_valid((int*)(chara+0x14c))||!lid_character_levels_valid((int*)(body+0x14c))||
       !lid_game_bag_string_valid((LidBagString*)chara,&context)||
       !lid_game_bag_string_valid((LidBagString*)body,&context)||((LidBagString*)chara)->num<=1||
       !lid_game_bag_string_same((LidBagString*)chara,(LidBagString*)body))return 0;
    for(i=0;i<10;i++)if(*(int*)(chara+0x14c+i*4)!=*(int*)(body+0x14c+i*4))return 0;
    return 1;
}

#if R157_READ_SCOPED
static int progress_loaded_levels_ready(BYTE*chara,BYTE*body){
    int acquired=lid_read_scope_begin(&g_reflection_read_scope,g_get_tid);
    int result=progress_loaded_levels_ready_read_core(chara,body);
    lid_read_scope_end(&g_reflection_read_scope,g_get_tid,acquired);
    return result;
}
#endif
static void WINAPI rage_get_body_status(void*out,void*levels,RawFString*type,int grade){
    BYTE*pawn=0;BYTE*common=0;void*exe;
    if(g_rage_body_status_original)g_rage_body_status_original(out,levels,type,grade);
    if(!g_ap_mode_active||!g_v074_cfg.valid||!out)return;
    exe=lid_find_module_ascii("BrgGame-Steam.exe");
    if(!rage_player_objects(exe,&pawn,&common)||out!=pawn+0xa70)return;
    if(!readable_range(out,0x34))return;
#if LID_CHARACTER_DELIVERY_ENABLED
    /* Native GetBodyStatus itself reads levels+24 (Rage counter). This input
       also exists during ReadUserData, before our UI watcher sees a new pointer. */
    if(!levels||!readable_range(levels,40)||!lid_character_levels_valid((int*)levels))return;
    *(int*)((BYTE*)out+0x30)=v076_capacity_from_level(g_v074_cfg.starting_rage_gauge,
        g_v074_cfg.rage_gauge_increase,g_v074_cfg.rage_gauge_quantity,((int*)levels)[9]);
#else
    *(int*)((BYTE*)out+0x30)=rage_ap_capacity();
#endif
}

static int rage_install_body_status_hook(void*exe){
    static const BYTE prefix[]={0x48,0x8b,0xc4,0x57,0x41,0x56,0x41,0x57,
                               0x48,0x81,0xec,0x30,0x01,0x00,0x00};
    BYTE*target=(BYTE*)exe+0xFA29E0;
    if(g_rage_body_status_ready)return 1;
    if(!exe||!executable_ptr(exe,target))return 0;
    if(!hook_install(&h_rage_body_status,target,(void*)rage_get_body_status,prefix,sizeof(prefix))){
        if(!g_rage_install_failure_logged){
            log_ascii("AP RAGE: GetBodyStatus hook rejected; gameplay synchronization disabled.");
            g_rage_install_failure_logged=1;
        }
        return 0;
    }
    g_rage_body_status_original=(NativeRageBodyStatusFn)h_rage_body_status.trampoline;
    g_rage_body_status_ready=1;
    log_ascii("AP RAGE: player GetBodyStatus capacity hook installed at RVA 0xFA29E0.");
    return 1;
}

/* Called on the existing game-thread character watcher. Reconcile only Rage
   caches (including a pawn already loaded before the hook became active). */
static void rage_sync_player(void*exe){
    BYTE*pawn=0;BYTE*common=0;int target;
    int*base;int*total;int*maximum;int*stock;float*fraction;
    if(!g_rage_body_status_ready||!g_ap_mode_active||!g_v074_cfg.valid)return;
    if(!rage_player_objects(exe,&pawn,&common))return;
    if(!readable_range(pawn+0xa70,0x34)||!readable_range(common+0x448c,0x34)||
       !readable_range(common+0x4cdc,12))return;
    target=rage_ap_capacity();
    base=(int*)(pawn+0xaa0);total=(int*)(common+0x44bc);
    maximum=(int*)(common+0x4ce4);stock=(int*)(common+0x4ce0);fraction=(float*)(common+0x4cdc);
    if(lid_rage_apply(target,base,total,maximum,stock,fraction)&&g_rage_log_budget){
        char line[220];size_t pos=0;g_rage_log_budget--;line[0]=0;
        append_ascii(line,sizeof(line),&pos,"AP RAGE: player base/total/runtime capacity=");
        append_dec(line,sizeof(line),&pos,(unsigned)target);
        append_ascii(line,sizeof(line),&pos," stock=");append_dec(line,sizeof(line),&pos,(unsigned)*stock);
        append_ascii(line,sizeof(line),&pos,"; charge bounded on game thread.");
        line[pos]=0;log_ascii(line);
    }
}

/* Character progressives: runtime readiness probe only. No transport offer,
   no ownership transfer and no mutation until the transaction host is ready. */
static BYTE* g_progress_event_exe;
static LidRuntimeMutex g_progress_runtime_mutex;
static unsigned long long g_progress_creation_time;
static int g_progress_prime_state;
typedef BOOL (WINAPI *LidGetProcessTimes)(HANDLE,void*,void*,void*,void*);
static int progress_prime_runtime(void){
    void* kernel;LidGetProcessTimes times;LidRuntimeMutexOps ops;
    unsigned long long creation=0,exit_time=0,kernel_time=0,user_time=0;
    DWORD pid;
    if(g_progress_prime_state)return g_progress_prime_state>0;
    g_progress_prime_state=-1;
    kernel=lid_find_module_ascii("kernel32.dll");
    if(!kernel||!g_win.GetCurrentProcess||!g_win.GetCurrentProcessId)return 0;
    times=(LidGetProcessTimes)lid_get_export(kernel,"GetProcessTimes");
    ops.create=(HANDLE (WINAPI *)(void*,BOOL,const wchar_t*))lid_get_export(kernel,"CreateMutexW");
    ops.wait=(DWORD (WINAPI *)(HANDLE,DWORD))lid_get_export(kernel,"WaitForSingleObject");
    ops.thread_id=(DWORD (WINAPI *)(void))lid_get_export(kernel,"GetCurrentThreadId");
    if(!times||!times(g_win.GetCurrentProcess(),&creation,&exit_time,&kernel_time,&user_time)||!creation)return 0;
    pid=g_win.GetCurrentProcessId();
    if(!lid_runtime_mutex_prime(&g_progress_runtime_mutex,&ops,(unsigned)pid,creation))return 0;
    g_progress_creation_time=creation;g_progress_prime_state=1;return 1;
}
static unsigned g_progress_probe_ticks;
static int g_progress_probe_last=-99;
static int progress_event_name_is(void* object,const char* wanted){
    BYTE* entry;BYTE** names;int index,count;size_t i;int wide;
    LID_PERF_DETAIL(1,1);
    if(!g_progress_event_exe||!object||!wanted||!readable_range(object,0x50)||
       !readable_range(g_progress_event_exe+0x295ad18,12))return 0;
    index=*(int*)((BYTE*)object+0x48);
    if(*(unsigned*)((BYTE*)object+0x4c))return 0;
    names=*(BYTE***)(g_progress_event_exe+0x295ad18);
    count=*(int*)(g_progress_event_exe+0x295ad20);
    if(index<0||count<=0||count>2000000||index>=count||!names||
       !readable_range(names+index,sizeof(void*)))return 0;
    entry=names[index];
    if(!entry||!readable_range(entry+8,1))return 0;
    wide=entry[8]&1;
    for(i=0;i<128;i++){
        unsigned actual;
        if(!readable_range(entry+0x14+i*(wide?2:1),wide?2:1))return 0;
        actual=wide?*(wchar_t*)(entry+0x14+i*2):entry[0x14+i];
        if(actual!=(unsigned char)wanted[i])return 0;
        if(!actual)return 1;
    }
    return 0;
}
static int progress_event_executable(const void* address){
    return g_progress_event_exe&&executable_ptr(g_progress_event_exe,(void*)address);
}
static const LidEventOps g_progress_event_ops={readable_range,progress_event_name_is,progress_event_executable};
static int build_ap_slot_dir(wchar_t*,size_t);
static LidCharacterHost g_progress_host;
static LidBridgeIo g_progress_bridge_io;
#if LID_BLUEPRINT_DELIVERY_ENABLED
static LidBlueprintPairGate g_blueprint_pair;
static LidBlueprintBridgeReader g_blueprint_reader;
#if LID_DECAL_DELIVERY_ENABLED
static int progress_decal_pair_read(void*,int,BYTE*,size_t,size_t*);
#endif
#endif
static LidCharacterConfig g_progress_config;
static wchar_t g_progress_slot_directory[1000];
static int g_progress_prepare_state;
typedef LONG (WINAPI *LidCngOpen)(void**,const wchar_t*,const wchar_t*,DWORD);
typedef LONG (WINAPI *LidCngHash)(void*,BYTE*,DWORD,BYTE*,DWORD,BYTE*,DWORD);
typedef LONG (WINAPI *LidCngClose)(void*,DWORD);
typedef LONG (WINAPI *LidCngRandom)(void*,BYTE*,DWORD,DWORD);
static int progress_host_own_mutex(void* context){
    (void)context;return lid_runtime_mutex_try_own(&g_progress_runtime_mutex);
}
static void progress_config_from_runtime(LidCharacterConfig* out){
    const int start[9]={g_v074_cfg.starting_hp,g_v074_cfg.starting_stm,g_v074_cfg.starting_str,
        g_v074_cfg.starting_dex,g_v074_cfg.starting_vit,g_v074_cfg.starting_luk,
        g_v074_cfg.starting_death_bag,g_v074_cfg.starting_skill_slots,g_v074_cfg.starting_rage_gauge};
    const int step[9]={g_v074_cfg.hp_increase,g_v074_cfg.stm_increase,g_v074_cfg.str_increase,
        g_v074_cfg.dex_increase,g_v074_cfg.vit_increase,g_v074_cfg.luk_increase,
        g_v074_cfg.death_bag_increase,g_v074_cfg.skill_slots_increase,g_v074_cfg.rage_gauge_increase};
    const int cap[9]={g_v074_cfg.hp_quantity,g_v074_cfg.stm_quantity,g_v074_cfg.str_quantity,
        g_v074_cfg.dex_quantity,g_v074_cfg.vit_quantity,g_v074_cfg.luk_quantity,
        g_v074_cfg.death_bag_quantity,g_v074_cfg.skill_slots_quantity,g_v074_cfg.rage_gauge_quantity};
    int i;for(i=0;i<9;i++){out->tracks[i][0]=start[i];out->tracks[i][1]=step[i];out->tracks[i][2]=cap[i];}
}
/* Initializes codecs, crypto and fixed-file adapters only. No file is opened,
   no offer is published and the legacy watcher is NOT stopped here. */
static int progress_prepare_host(void){
    char json[384];BYTE digest[32],nonce[16];size_t length,i;void* bcrypt;void* kernel;void* algorithm=0;
    LidCngOpen open;LidCngHash hash;LidCngClose close;LidCngRandom random;
    LidBridgeIoOps io_ops;LidCharacterHostOps host_ops;LONG status;unsigned pid;
    if(g_progress_prepare_state)return g_progress_prepare_state>0;
    if(!g_v074_cfg.valid||!g_savedata_route_active||g_progress_prime_state!=1)return 0;
    g_progress_prepare_state=-1;
    for(i=0;g_release_slot_key[i];i++){
        char c=g_release_slot_key[i];if(i>=20||!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;
    }
    if(i!=20||!build_ap_slot_dir(g_progress_slot_directory,1000))return 0;
    progress_config_from_runtime(&g_progress_config);
    length=lid_config_json(&g_progress_config,json,sizeof(json));if(!length)return 0;
    if(!g_win.LoadLibraryW||!g_win.GetCurrentProcessId)return 0;
    bcrypt=lid_find_module_ascii("bcrypt.dll");
    if(!bcrypt)bcrypt=g_win.LoadLibraryW(L"bcrypt.dll");
    kernel=lid_find_module_ascii("kernel32.dll");if(!bcrypt||!kernel)return 0;
    open=(LidCngOpen)lid_get_export(bcrypt,"BCryptOpenAlgorithmProvider");
    hash=(LidCngHash)lid_get_export(bcrypt,"BCryptHash");
    close=(LidCngClose)lid_get_export(bcrypt,"BCryptCloseAlgorithmProvider");
    random=(LidCngRandom)lid_get_export(bcrypt,"BCryptGenRandom");
    if(!open||!hash||!close||!random||open(&algorithm,L"SHA256",0,0)<0)return 0;
    status=hash(algorithm,0,0,(BYTE*)json,(DWORD)length,digest,32);
    if(close(algorithm,0)<0||status<0||random(0,nonce,16,2)<0||!lid_wire_nonzero(nonce,16))return 0;
    io_ops.open=g_win.CreateFileW;io_ops.read=g_win.ReadFile;io_ops.write=g_win.WriteFile;
    io_ops.close=g_win.CloseHandle;io_ops.remove=g_win.DeleteFileW;io_ops.error=g_win.GetLastError;
    io_ops.flush=(BOOL (WINAPI *)(HANDLE))lid_get_export(kernel,"FlushFileBuffers");
    io_ops.replace=(BOOL (WINAPI *)(LPCWSTR,LPCWSTR,DWORD))lid_get_export(kernel,"MoveFileExW");
    pid=(unsigned)g_win.GetCurrentProcessId();
    if(!lid_bridge_io_init(&g_progress_bridge_io,&io_ops,g_progress_slot_directory,pid,g_progress_creation_time))return 0;
    host_ops.context=&g_progress_bridge_io;host_ops.read=lid_bridge_io_read;
    host_ops.publish=lid_bridge_io_publish;host_ops.own_mutex=progress_host_own_mutex;
#if LID_BLUEPRINT_DELIVERY_ENABLED
    {LidCharacterHostOps original=host_ops;
     g_blueprint_reader.bridge=&g_progress_bridge_io;
     if(!lid_blueprint_pair_init(&g_blueprint_pair,&host_ops,&original,&g_blueprint_reader,lid_blueprint_bridge_read))return 0;
#if LID_DECAL_DELIVERY_ENABLED
     host_ops.read=progress_decal_pair_read;
#endif
    }
#endif
    if(!lid_host_init(&g_progress_host,&host_ops,pid,g_progress_creation_time,nonce,digest))return 0;
    g_progress_prepare_state=1;return 1;
}
static int progress_probe_hub_ready(void* module){
    BYTE* exe=module;BYTE* engine;BYTE* world;BYTE* actors;BYTE* first;BYTE* game;BYTE* stage;void** vt;
    BYTE (WINAPI *is_hub)(void*);
    if(!exe||!readable_range(exe+0x29709f0,8))return -1;
    engine=*(BYTE**)(exe+0x29709f0);
    if(!engine||!readable_range(engine+0x80,8))return -1;
    world=*(BYTE**)(engine+0x80);
    if(!world||!readable_range(world+0x60,8))return -1;
    actors=*(BYTE**)(world+0x60);
    if(!actors||!readable_range(actors,8))return -1;
    first=*(BYTE**)actors;
    if(!first||!readable_range(first+0x714,8))return -1;
    game=*(BYTE**)(first+0x714);
    if(!game||!readable_range(game,0x13f0))return -1;
    vt=*(void***)game;
    if(!vt||!readable_range(vt,0xaa0)||vt[0xa98/8]!=exe+0x10b5d80)return -1;
    if(!(*(unsigned*)(game+0x844)&0x08000000u))return 0;
    stage=*(BYTE**)(game+0x13e8);
    if(!stage||!readable_range(stage,0x58)||*(void**)stage!=exe+0x21c3350)return -1;
    vt=*(void***)stage;
    if(!readable_range(vt,0x3d8)||vt[0x3d0/8]!=exe+0x1433900)return -1;
    is_hub=(BYTE (WINAPI *)(void*))vt[0x3d0/8];
    if(!is_hub(stage))return 0;
    g_progress_event_exe=exe;
    return lid_event_call(stage,LID_EVENT_BOOL,&g_progress_event_ops,0);
}
static int progress_entry_body_ready(void);
#if LID_BLUEPRINT_DELIVERY_ENABLED
static int progress_blueprint_attach(void);
static void progress_blueprint_log_status(int);
#endif
#include "progression_native_adapter.h"
/* R21: a network NPC reload copies GetChara into ReadUserData before the
   legacy watcher can repair grade. Repair that SAME current AP record before
   the copy; native status/decals/equipment calculation remains untouched. */
typedef int (WINAPI *R21GetCharaFn)(void*,void*);
static R21GetCharaFn g_r21_get_chara_original;
static NativeUserDataPtr g_r21_get_current_body;
static void** g_r21_get_chara_slot;
static void* g_r21_userdata;
static void* g_r21_attempted_userdata;
static unsigned g_r21_grade_log_budget=8;

static int r21_normalize_loaded_grade(BYTE*body){
    int grade;
    if(!g_ap_mode_active||!g_savedata_route_active||!g_v074_cfg.valid||
       g_v074_cfg.fighter_grade!=6||!body||!readable_range(body,0x174)||
       !v074_skill_fstring_matches(body+0x28,"BAL")||
       !lid_character_levels_valid((int*)(body+0x14c)))return 0;
    grade=*(int*)(body+0x38);
    if(grade<1||grade>9||grade==6||!progress_native_writable(body+0x38,4))return 0;
    *(int*)(body+0x38)=6;
    return 1;
}
static int WINAPI r21_get_chara(void*self,void*out){
    if(self==g_r21_userdata&&g_r21_get_current_body&&g_ap_mode_active&&
       g_savedata_route_active&&g_v074_cfg.valid&&g_v074_cfg.fighter_grade==6){
        BYTE*body=(BYTE*)g_r21_get_current_body(self);
        if(r21_normalize_loaded_grade(body)&&g_r21_grade_log_budget){
            g_r21_grade_log_budget--;
            log_ascii("AP CHARACTER R21: current BAL grade repaired before GetChara copy; saved levels preserved.");
        }
    }
    return g_r21_get_chara_original?g_r21_get_chara_original(self,out):0;
}
static int r21_install_current_chara_hook(BYTE*exe,void*userdata){
    void**vt;
    if(!exe||!userdata||!readable_range(userdata,sizeof(void*)))return 0;
    vt=*(void***)userdata;
    if(vt!=(void**)(exe+0x21f7ac0)||!readable_range(vt,0x1138)||
       vt[0x1130/8]!=exe+0x1568a70)return 0;
    if(g_r21_get_chara_slot){
        if(vt+0x510/8!=g_r21_get_chara_slot||*g_r21_get_chara_slot!=(void*)r21_get_chara)return 0;
        g_r21_userdata=userdata;return 1;
    }
    if(vt[0x510/8]!=exe+0x1567890)return 0;
    g_r21_get_current_body=(NativeUserDataPtr)vt[0x1130/8];
    g_r21_userdata=userdata;
    /* v076 publishes the original before changing the slot protection/write. */
    if(!v076_write_vtable_slot(vt+0x510/8,(void*)r21_get_chara,(void**)&g_r21_get_chara_original))return 0;
    g_r21_get_chara_slot=vt+0x510/8;
    log_ascii("AP CHARACTER R21: current GetChara grade guard installed; no new grant or polling.");
    return 1;
}
#if LID_BLUEPRINT_DELIVERY_ENABLED
#include "blueprint_runtime_attach.h"
#endif
#include "progression_hub_observer.h"
#if defined(LID_CHECKS_ROTATION_ENABLED) && LID_CHECKS_ROTATION_ENABLED
static int checks_rotation_prepare_at_idle(void);
#endif
#if defined(LID_RELOAD_DECODE_TRACE_ENABLED) && LID_RELOAD_DECODE_TRACE_ENABLED
static int checks_load_decode_prepare_at_idle(void);
static void checks_load_decode_begin(void);
static void checks_load_decode_end(void);
#endif
#define LID_R36_OS_ERROR() (g_win.GetLastError?g_win.GetLastError():0)
#include "reload_runtime_adapter.h"
#ifndef LID_SHOP_FEATURE_ENABLED
#define LID_SHOP_FEATURE_ENABLED 0
#endif
#if LID_SHOP_FEATURE_ENABLED
#include "shop_runtime_driver.h"
#endif
#if LID_NATIVE_RELOAD_ENABLED
static int progress_reload_attach_override(BYTE*exe,LidReloadAttachOverride*out){
#if LID_SHOP_FEATURE_ENABLED
    Hook*h=&shop_hooks[0];unsigned state=(unsigned)__atomic_load_n(&shop_install_state,__ATOMIC_ACQUIRE);
    if(state){
        if(state!=1||!out||shop_exe!=exe||h->target!=exe+0x26ad90||h->size!=16||
           !h->trampoline||(void*)shop_original_set!=h->trampoline||
           !readable_range(h->trampoline,30)||lid_memcmp(h->original,h->trampoline,16))return -1;
        out->detour=(void*)shop_set_object;out->trampoline=h->trampoline;return 1;
    }
#else
    (void)exe;(void)out;
#endif
    return 0;
}
#endif
static void progress_probe_tick(void* exe,int is_title){
    int ready;
    if(is_title)return;
    if((g_progress_probe_ticks++%60)!=0)return;
    ready=progress_probe_hub_ready(exe);
    if(ready==1){
        if(progress_prepare_host())progress_init_native_adapter();
        /* Intentionally UNARMED until the mutation/save adapter is complete. */
        lid_host_step(&g_progress_host,0);
    }
    if(ready==g_progress_probe_last)return;
    g_progress_probe_last=ready;
    log_ascii(ready==1?"AP CHARACTER: hub readiness verified; probe only, no delivery.":
        ready==0?"AP CHARACTER: waiting for game-started, hub and stage-ready.":
        "AP CHARACTER: readiness metadata unavailable/incompatible; delivery disabled.");
}

#include "progression_profile.h"
#ifndef LID_CHARACTER_PROFILE_ENABLED
#define LID_CHARACTER_PROFILE_ENABLED 0
#endif
static LidCharacterProfile g_character_profile;
static int g_character_profile_state;
static int g_character_profile_game_started;
static int g_character_profile_rearmed;
static int progress_profile_prepare(void){
#if LID_CHARACTER_PROFILE_ENABLED
    void* kernel;LidProfileOps ops;long long frequency;
    BOOL (WINAPI *get_frequency)(long long*);int expected=0,state;
    state=__atomic_load_n(&g_character_profile_state,__ATOMIC_ACQUIRE);
    if(state)return state==1;
    if(!__atomic_compare_exchange_n(&g_character_profile_state,&expected,-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return expected==1;
    kernel=lid_find_module_ascii("kernel32.dll");
    if(kernel){
        ops.now=(BOOL (WINAPI *)(long long*))lid_get_export(kernel,"QueryPerformanceCounter");
        ops.thread=(DWORD (WINAPI *)(void))lid_get_export(kernel,"GetCurrentThreadId");
        ops.emit=log_ascii;
        get_frequency=(BOOL (WINAPI *)(long long*))lid_get_export(kernel,"QueryPerformanceFrequency");
        if(get_frequency&&get_frequency(&frequency)&&lid_profile_init(&g_character_profile,&ops,frequency)){
            __atomic_store_n(&g_character_profile_state,1,__ATOMIC_RELEASE);
            log_ascii("AP PERF R16: timings start after GameStarted, may rearm once at body change; maximum24 windows; elapsed time, not FPS.");
            return 1;
        }
    }
    log_ascii("AP PERF R16: timer unavailable; profiling disabled, no fallback timing invented.");
#endif
    return 0;
}
static unsigned long long progress_profile_mark(void){
#if LID_CHARACTER_PROFILE_ENABLED
    return lid_profile_now(&g_character_profile);
#else
    return 0;
#endif
}
static void progress_profile_record(int section,unsigned long long mark){
#if LID_CHARACTER_PROFILE_ENABLED
    lid_profile_record(&g_character_profile,section,mark);
#else
    (void)section;(void)mark;
#endif
}
#include "checks_rotation.h"
#include "checks_rotation_proof.h"
#include "checks_rotation_patch.h"
#include "checks_rotation_call_patch.h"
#include "checks_rotation_runtime.h"
#include "reload_decode_runtime.h"
#include "checks_rotation_stage.h"
#include "checks_rotation_observer.h"
#include "checks_delivery.h"
#if LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
static void checks_chest_boundary(DWORD owner);
#endif
#include "checks_diagnostic.h"
#include "checks_chest_runtime.h"
#include "material_owner_runtime.h"
#include "material_host_runtime.h"
#include "material_hook_runtime.h"
#include "material_early_runtime.h"
#include "progression_entry_driver.h"
#include "kill_coin_owner_runtime.h"
#include "notification_native.h"
static void v070_sync_runtime_character_impl(int is_title){
    void*exe;
    void*userdata;
    void**vt;
    void*body=0;
    void*payload=0;
    void*array_base=0;
    BYTE*chara_record=0;
    NativeUserDataBool is_valid;
    NativeUserDataPtr get_current_body;
    NativeUserDataIntArg get_body_grade;
    NativeUserDataNoArg get_current_index;
    int current_index=-1;
    int count=0;
    int chara_grade=-1;
    int body_grade=-1;
    int changed=0;
    int should_log=0;
    unsigned long long profile_mark;

    if(!g_ap_mode_active)return;
    g_v070_watch_calls++;

    exe=lid_find_module_ascii("BrgGame-Steam.exe");
    if(!exe)return;
    /* Create only: the legacy watcher remains free to acquire its mutex.
       Acquiring ownership / stopping it is deferred until the full host. */
    progress_prime_runtime();
#if LID_SHOP_FEATURE_ENABLED
    if(!shop_install_state)shop_driver_install(exe);
#endif
    if(!g_rage_body_status_ready)rage_install_body_status_hook(exe);
    if(!g_v076_body_detail_hook_ready)v076_install_body_detail_hook();
    if(!g_r61_level_xp_ready)r61_install_level_xp_hook();
    if(!g_r75_level_bloodnium_ready)r75_install_level_bloodnium_hook();
    if(!g_v082_add_unit_hook_ready)v082_install_add_unit_for_deathbag_hook();
    if(LID_CHARACTER_DELIVERY_ENABLED)progress_install_hub_observer(exe);
#if LID_NATIVE_RELOAD_ENABLED
    if(progress_reload_tick((BYTE*)exe))return;
#endif
    profile_mark=progress_profile_mark();
    userdata=find_live_brg_userdata();
    progress_profile_record(LID_PROFILE_LOOKUP,profile_mark);
    if(!userdata)return;
    if(!readable_range(userdata,0x7c))return;
    vt=*(void***)userdata;
    if(!vt||!readable_range(vt,0x1138))return;

    is_valid=(NativeUserDataBool)vt[0x280/sizeof(void*)];
    get_current_index=(NativeUserDataNoArg)vt[0x530/sizeof(void*)];
    get_body_grade=(NativeUserDataIntArg)vt[0x890/sizeof(void*)];
    get_current_body=(NativeUserDataPtr)vt[0x1130/sizeof(void*)];
    if(!is_valid||!get_current_index||!get_body_grade||!get_current_body)return;
    if(!executable_ptr(exe,(void*)is_valid)||!executable_ptr(exe,(void*)get_current_index)||
       !executable_ptr(exe,(void*)get_body_grade)||!executable_ptr(exe,(void*)get_current_body))return;
    if(is_valid(userdata)<=0)return;

    if(!is_title&&userdata!=g_r21_attempted_userdata){
        g_r21_attempted_userdata=userdata;
        if(!r21_install_current_chara_hook((BYTE*)exe,userdata))
            log_ascii("AP CHARACTER R21: GetChara guard rejected; existing grade correction retained.");
    }

    current_index=get_current_index(userdata);
    body=get_current_body(userdata);
    if(body&&readable_range(body,0x3c))body_grade=get_body_grade(userdata,-1);

    if(body&&body!=g_v074_last_scanned_body&&!g_v074_status_rows_ready){
        profile_mark=progress_profile_mark();
        LID_PERF_BEGIN(master_time);
        unsigned patched=v074_patch_runtime_master_rows();
        LID_PERF_END(master_time,0,"master_scan",(int)patched);
        LID_PERF_SUMMARY("scan_complete");
        progress_profile_record(LID_PROFILE_MASTER,profile_mark);
        g_v074_last_scanned_body=body;
        if(!patched)log_ascii("V0.74 DYNAMIC STATS: no remaining vanilla runtime rows matched on this body generation.");
    }

    payload=*(void**)((BYTE*)userdata+0x74);
    if(payload&&readable_range(payload,0x2f8)&&current_index>=0){
        count=*(int*)((BYTE*)payload+0x2f4);
        array_base=*(void**)((BYTE*)payload+0x2ec);
        if(array_base&&current_index<count&&current_index>=0){
            chara_record=(BYTE*)array_base+(size_t)current_index*0x220;
            if(readable_range(chara_record,0x3c))chara_grade=*(int*)(chara_record+0x38);
            else chara_record=0;
        }
    }

    if(chara_record)g_v076_active_chara=chara_record;
#if !LID_CHARACTER_DELIVERY_ENABLED
    if(!is_title&&chara_record)rage_sync_player(exe);

    if(chara_record&&chara_record!=g_v074_last_level_chara){
        v074_initialize_progression_levels(chara_record,"persistent Chara");
        g_v074_last_level_chara=chara_record;
        if(body==chara_record)g_v074_last_level_body=body;
    }
    if(body&&body!=chara_record&&body!=g_v074_last_level_body){
        v074_initialize_progression_levels(body,"runtime Body");
        g_v074_last_level_body=body;
    }
#else
    /* Preserve the game's loaded levels from the FIRST instance, including
       before OFFER/GRANT. Pending receipts are applied only by the hub lease. */
    profile_mark=progress_profile_mark();
    int loaded_ready=progress_loaded_levels_ready(chara_record,(BYTE*)body);
    progress_profile_record(LID_PROFILE_LOADED,profile_mark);
    if(loaded_ready){
        g_v074_last_level_chara=chara_record;g_v074_last_level_body=body;
        if(!is_title){
            profile_mark=progress_profile_mark();rage_sync_player(exe);
            progress_profile_record(LID_PROFILE_RAGE,profile_mark);
        }
    }
#endif

    if(userdata!=g_v070_last_userdata||body!=g_v070_last_body||current_index!=g_v070_last_index||
       chara_grade!=g_v070_last_chara_grade||body_grade!=g_v070_last_body_grade)should_log=1;

    if(should_log){
        char line[520];size_t pos=0;
        line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.70 RAM: phase=");
        append_ascii(line,sizeof(line),&pos,is_title?"TITLE":"NON_TITLE");
        append_ascii(line,sizeof(line),&pos," userdata=");append_hex64(line,sizeof(line),&pos,(ULONG_PTR)userdata);
        append_ascii(line,sizeof(line),&pos," body=");append_hex64(line,sizeof(line),&pos,(ULONG_PTR)body);
        append_ascii(line,sizeof(line),&pos," index=");
        if(current_index<0)append_ascii(line,sizeof(line),&pos,"NEG");else append_dec(line,sizeof(line),&pos,(unsigned)current_index);
        append_ascii(line,sizeof(line),&pos," chara_grade=");
        if(chara_grade<0)append_ascii(line,sizeof(line),&pos,"NA");else append_dec(line,sizeof(line),&pos,(unsigned)chara_grade);
        append_ascii(line,sizeof(line),&pos," body_grade=");
        if(body_grade<0)append_ascii(line,sizeof(line),&pos,"NA");else append_dec(line,sizeof(line),&pos,(unsigned)body_grade);
        line[pos]=0;log_ascii(line);
    }

    /* Persistent Chara record used by the native save system. */
    if(chara_record&&chara_grade>=0&&chara_grade!=6){
        int before=chara_grade;
        *(int*)(chara_record+0x38)=6;
        chara_grade=*(int*)(chara_record+0x38);
        if(chara_grade==6){
            char line[320];size_t pos=0;line[0]=0;
            append_ascii(line,sizeof(line),&pos,"V0.70 RAM: persistent Chara corrected ");
            append_dec(line,sizeof(line),&pos,(unsigned)before);
            append_ascii(line,sizeof(line),&pos,"->6 record=");append_hex64(line,sizeof(line),&pos,(ULONG_PTR)chara_record);
            line[pos]=0;log_ascii(line);changed=1;
        }
    }

    /* Runtime Body currently used by the game/UI.  Verify through the game's own
       GetBodyGrade virtual method, not only by reading the field back. */
    if(body&&readable_range(body,0x3c)&&body_grade>=0&&body_grade!=6){
        int before=body_grade;
        *(int*)((BYTE*)body+0x38)=6;
        body_grade=get_body_grade(userdata,-1);
        if(body_grade==6){
            char line[320];size_t pos=0;line[0]=0;
            append_ascii(line,sizeof(line),&pos,"V0.70 RAM: runtime Body corrected ");
            append_dec(line,sizeof(line),&pos,(unsigned)before);
            append_ascii(line,sizeof(line),&pos,"->6 body=");append_hex64(line,sizeof(line),&pos,(ULONG_PTR)body);
            line[pos]=0;log_ascii(line);changed=1;
        }else{
            log_ascii("V0.70 RAM: runtime Body write did not verify through GetBodyGrade.");
        }
    }

    if(changed)g_v070_corrections++;
    g_v070_last_userdata=userdata;
    g_v070_last_body=body;
    g_v070_last_index=current_index;
    g_v070_last_chara_grade=chara_grade;
    g_v070_last_body_grade=body_grade;
    /* Continuous readiness probing retired. Hub candidates come from actual
       native area-set notifications; receipt arrival is NOT an entry event.
       The entry-only transaction adapter must be completed before grants. */
    if(!is_title){
#if LID_NOTIFICATIONS_ENABLED
        notifications_native_tick((BYTE*)exe,loaded_ready);
#endif
#if LID_KILL_COIN_DELIVERY_ENABLED
        coins_native_tick((BYTE*)exe,(BYTE*)userdata,(BYTE*)body,current_index,loaded_ready);
#endif
        profile_mark=progress_profile_mark();progress_native_entry_tick();
        progress_profile_record(LID_PROFILE_TRANSACTION,profile_mark);
    }
}
static void v070_sync_runtime_character(int is_title){
    unsigned long long profile_start=0;void* before_body=g_v070_last_body;
#if LID_CHARACTER_PROFILE_ENABLED
    BYTE* game;
    if(!is_title&&g_ap_mode_active){
        if(!g_character_profile_game_started&&g_hub_event_exe){
            game=progress_native_game(g_hub_event_exe);
            if(game&&lid_progress_game_started(game,progress_native_readable)==1)g_character_profile_game_started=1;
        }
        if(g_character_profile_game_started&&progress_profile_prepare())profile_start=lid_profile_enter(&g_character_profile);
    }
#endif
    v070_sync_runtime_character_impl(is_title);
    if(profile_start)lid_profile_leave(&g_character_profile,profile_start);
#if LID_CHARACTER_PROFILE_ENABLED
    if(!is_title&&g_character_profile_game_started&&!g_character_profile_rearmed&&before_body&&
       g_v070_last_body&&before_body!=g_v070_last_body&&lid_profile_rearm(&g_character_profile)){
        g_character_profile_rearmed=1;
        log_ascii("AP PERF R16: body generation changed; rearmed once for late load/hub measurement.");
    }
#else
    (void)before_body;
#endif
}
static void fail_ap_release(const wchar_t*message,const char*logline){if(logline)log_ascii(logline);if(g_release_backup_path[0]&&direct_file_exists(g_release_backup_path)){copy_file_direct(g_release_backup_path,g_release_save_path);if(g_win.DeleteFileW)g_win.DeleteFileW(g_release_backup_path);}g_pending_ap_release=0;g_release_stage=0;g_release_wait_ticks=0;if(g_win.MessageBoxW&&message)g_win.MessageBoxW(0,message,L"LET IT DIE Archipelago",0x10u);}
static int activate_hot_overlay(const char*slot_key);
/* V0.67: redirect the game's own Savedata directory only after AP authentication.
   The vanilla loader then constructs brggame.sav/.tmp/.bak itself inside the AP slot. */
static int build_ap_slot_dir(wchar_t*out,size_t cap){
    if(!out||cap<8||!g_release_slot_key[0])return 0;
    game_dir(out,cap);
    lid_wappend(out,cap,L"\\Archipelago\\slots\\");
    if(!append_ascii_wide(out,cap,g_release_slot_key))return 0;
    lid_wappend(out,cap,L"\\");
    return 1;
}

/* Returns the AP slot directory as a UE3 FString after the runtime route is active. */
static RawFString* WINAPI hk_savedata_dir(RawFString*out){
    wchar_t path[1000];
    wchar_t*copy;
    size_t chars,bytes;
    if(!out)return out;
    if(!g_savedata_route_active||!g_tbb_malloc||!build_ap_slot_dir(path,1000))
        return g_savedata_dir_original?g_savedata_dir_original(out):out;
    if(g_v068_savedata_trace_calls<128){
        char line[320];size_t pos=0;void*base=lid_find_module_ascii("BrgGame-Steam.exe");ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);
        LONG callno=++g_v068_savedata_trace_calls;
        append_ascii(line,sizeof(line),&pos,"V0.68 TRACE: SavedataDir #");append_dec(line,sizeof(line),&pos,(unsigned)callno);
        append_ascii(line,sizeof(line),&pos," caller=");append_hex64(line,sizeof(line),&pos,caller);
        append_ascii(line,sizeof(line),&pos," main_rva=");
        if(base&&caller>=(ULONG_PTR)base)append_hex64(line,sizeof(line),&pos,caller-(ULONG_PTR)base);else append_ascii(line,sizeof(line),&pos,"OUTSIDE_MAIN");
        append_ascii(line,sizeof(line),&pos," -> AP slot");line[pos]=0;log_ascii(line);
    }
    chars=lid_wcslen(path)+1;
    bytes=chars*sizeof(wchar_t);
    copy=(wchar_t*)g_tbb_malloc(bytes);
    if(!copy)
        return g_savedata_dir_original?g_savedata_dir_original(out):out;
    lid_memcpy(copy,path,bytes);
    out->data=copy;
    out->num=(int)chars;
    out->max=(int)chars;
    return out;
}

/* Installs the Savedata-directory hook only after a successful AP connection. */
static int install_savedata_dir_hook(void){
    void*exe=lid_find_module_ascii("BrgGame-Steam.exe");
    BYTE*target;
    static const BYTE prefix[]={
        0x48,0x8B,0xC4,
        0x48,0x89,0x48,0x08,
        0x55,
        0x48,0x8B,0xEC,
        0x48,0x81,0xEC,0x80,0x00,0x00,0x00
    };
    if(h_savedata_dir.target)return 1;
    if(!exe)return 0;
    target=(BYTE*)exe+0x1102730;
    if(!hook_install(&h_savedata_dir,target,(void*)hk_savedata_dir,prefix,sizeof(prefix)))return 0;
    g_savedata_dir_original=(NativeSavedataDirFn)h_savedata_dir.trampoline;
    return 1;
}

/* After authentication, enable the existing V0.60 overlay and redirect the true vanilla Savedata directory. */
static void process_ap_savedata_route(void){
    if(!g_pending_ap_release)return;
    if(!g_tbb_malloc){
        fail_ap_release(L"Connexion Archipelago validee, mais l'allocateur du jeu n'est pas disponible. Le jeu reste au menu titre.","V0.67 SAVEDATA: game allocator unavailable.");
        return;
    }
    if(!install_savedata_dir_hook()){
        fail_ap_release(L"Connexion Archipelago validee, mais le chemin natif de sauvegarde n'a pas pu etre redirige. Le jeu reste au menu titre.","V0.67 SAVEDATA: native Savedata directory hook installation failed.");
        return;
    }
    if(!activate_hot_overlay(g_release_slot_key)){
        fail_ap_release(L"Connexion Archipelago validee, mais l'activation du slot AP a echoue. Le jeu reste au menu titre.","V0.67 SAVEDATA: V0.60 hot overlay activation failed.");
        return;
    }
    g_savedata_route_active=1;
    g_pending_ap_release=0;
    g_release_stage=0;
    g_release_wait_ticks=0;
    log_ascii("V0.70 RAM: AP route active; runtime Chara/Body watcher armed across Game Start.");
    if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Connexion Archipelago validee. Le suivi RAM du personnage est actif. Valide Debut du jeu.",L"LET IT DIE Archipelago",0x40u);
}

/* Resolves the dinput8 proxy API and installs/activates the AP file overlay only now, after the title menu is already stable. */
static int activate_hot_overlay(const char*slot_key){
    void*proxy; ProxyActivateOverlayA activate; ProxyGetInt get_hooks; int hooks=-1;
    proxy=lid_find_module_ascii("dinput8.dll");
    if(!proxy){log_ascii("V0.52 HOT: dinput8 proxy module not found; current-process activation unavailable.");return 0;}
    activate=(ProxyActivateOverlayA)lid_get_export(proxy,"LID_ActivateOverlayA");
    get_hooks=(ProxyGetInt)lid_get_export(proxy,"LID_GetOverlayHookStatus");
    if(!activate){log_ascii("V0.52 HOT: proxy does not export LID_ActivateOverlayA; install the V0.52 dinput8.dll.");return 0;}
    if(get_hooks)hooks=get_hooks();
    if(hooks>0){log_ascii("V0.52 HOT: unexpected file hooks were already active before connection; refusing activation.");return 0;}
    log_ascii("V0.52 HOT: boot remained vanilla with NO file hooks; installing AP redirection now after successful authentication.");
    if(!activate(slot_key)){log_ascii("V0.52 HOT: runtime overlay hook installation/activation failed.");return 0;}
    g_ap_mode_active=1;
    g_pending_title_reset=1;
    log_ascii("V0.52 HOT: AP overlay installed and activated at runtime; no restart requested.");
    return 1;
}

static int launch_archipelago_client_and_wait(void){
    wchar_t*request=g_bridge_request,*result=g_bridge_result,*launcher=g_bridge_launcher,*cmd=g_bridge_cmd;
    wchar_t*component_marker=g_bridge_component_marker,*client_marker=g_bridge_client_marker;
    STARTUPINFOW si;PROCESS_INFORMATION pi;
    int i,status=0,component_seen=0,client_seen=0;
    char slot_key[64];

    if(!write_connection_request(request,1000,result,1000)){
        log_ascii("V0.52 AP: could not write connection_request.lidapconnect.");
        return 0;
    }

    game_dir(component_marker,1000);
    lid_wappend(component_marker,1000,L"\\Archipelago\\component_entry.log");
    game_dir(client_marker,1000);
    lid_wappend(client_marker,1000,L"\\Archipelago\\client_bootstrap.log");
    if(g_win.DeleteFileW){g_win.DeleteFileW(component_marker);g_win.DeleteFileW(client_marker);}

    if(!find_archipelago_launcher(launcher,1000)){
        log_ascii("V0.52 AP: ArchipelagoLauncher.exe not found on drive-root Archipelago folders or ProgramData.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"ArchipelagoLauncher.exe introuvable. Verifie ton installation Archipelago.",L"LET IT DIE Archipelago",0x10u);
        return 0;
    }

    /* V0.52: give Launcher the request file itself. SuffixIdentifier(.lidapconnect)
       resolves the exact LET IT DIE V0.52 component, avoiding component-name dispatch. */
    cmd[0]=0;
    append_quoted_arg(cmd,4096,launcher);
    lid_wappend(cmd,4096,L" ");
    append_quoted_arg(cmd,4096,request);
    lid_wappend(cmd,4096,L" ");
    append_quoted_arg(cmd,4096,g_password);

    lid_memset(&si,0,sizeof(si));lid_memset(&pi,0,sizeof(pi));si.cb=sizeof(si);
    log_ascii("V0.52 AP: dispatching .lidapconnect request through Archipelago Launcher file association.");
    if(!g_win.CreateProcessW||!g_win.CreateProcessW(launcher,cmd,0,0,FALSE,0,0,0,&si,&pi)){
        log_ascii("V0.52 AP: CreateProcessW failed for Archipelago Launcher.");
        return 0;
    }
    if(pi.hThread)g_win.CloseHandle(pi.hThread);
    if(pi.hProcess)g_win.CloseHandle(pi.hProcess);

    /* Password must not persist in our process after child creation. */
    lid_memset(g_password,0,sizeof(g_password));
    log_ascii("V0.52 AP: Launcher process created; waiting for exact component/bootstrap markers and final result.");

    /* The game thread is free: this worker may wait up to five minutes for first-time
       save/masters preparation. Missing component/bootstrap is detected much earlier. */
    for(i=0;i<3000;i++){
        if(g_win.Sleep)g_win.Sleep(100);

        if(!component_seen&&direct_file_exists(component_marker)){
            component_seen=1;
            log_ascii("V0.52 AP: exact V0.52 Archipelago component invoked (.lidapconnect association confirmed).");
        }
        if(!client_seen&&direct_file_exists(client_marker)){
            client_seen=1;
            log_ascii("V0.52 AP: Client.py bootstrap reached; waiting for authentication/slot preparation.");
        }
        if(direct_file_exists(result)){
            status=result_status_and_slot(result,slot_key,sizeof(slot_key));
            break;
        }

        if(i==299&&!component_seen){
            log_ascii("V0.52 AP: no component_entry.log after 30s; V0.52 APWorld/component was not loaded by this Archipelago Launcher.");
            if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Archipelago n'a pas charge le composant LET IT DIE V0.52. Verifie que let_it_die.apworld V0.52 est installe dans CETTE installation Archipelago et qu'aucune ancienne copie prioritaire n'est utilisee.",L"LET IT DIE Archipelago",0x10u);
            return 0;
        }
        if(i==899&&component_seen&&!client_seen){
            log_ascii("V0.52 AP: component entered but Client.py bootstrap marker is still missing after 90s.");
            if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Le composant LET IT DIE V0.52 a ete trouve, mais Client.py n'a pas demarre. Consulte component_entry.log et les logs Archipelago.",L"LET IT DIE Archipelago",0x10u);
            return 0;
        }
    }

    if(status==1){
        char cfgline[720];size_t cp=0;cfgline[0]=0;
        log_ascii("V0.52 AP: server authentication succeeded; dedicated AP save/master slot prepared.");
        append_ascii(cfgline,sizeof(cfgline),&cp,"V0.74 SLOT DATA: Grade=");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.fighter_grade);
        append_ascii(cfgline,sizeof(cfgline),&cp," HP/STM/STR/DEX/VIT/LUK=");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_hp);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_stm);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_str);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_dex);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_vit);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_luk);
        append_ascii(cfgline,sizeof(cfgline),&cp," Skill/Bag/Rage=");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_skill_slots);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_death_bag);append_ascii(cfgline,sizeof(cfgline),&cp,"/");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.starting_rage_gauge);
        append_ascii(cfgline,sizeof(cfgline),&cp," MaxLevels=");
        append_dec(cfgline,sizeof(cfgline),&cp,(unsigned)g_v074_cfg.max_levels);
        cfgline[cp]=0;log_ascii(cfgline);
        {size_t k=0;while(slot_key[k]&&k+1<sizeof(g_release_slot_key)){g_release_slot_key[k]=slot_key[k];k++;}g_release_slot_key[k]=0;}
        g_release_stage=0;g_release_wait_ticks=0;g_pending_ap_release=1;
        log_ascii("V0.67 AP: slot prepared; AP Savedata routing queued on the existing title thread. No launch-manager probe is performed at the title menu.");
        return 1;
    }
    if(status==-2){
        log_ascii("V0.52 AP: result was OK but slot_key was missing; hot activation aborted.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Connexion Archipelago validee, mais connection_result.json ne contient pas slot_key. Le jeu reste en mode vanilla.",L"LET IT DIE Archipelago",0x10u);
        return 0;
    }
    if(status==-3){
        log_ascii("V0.74 AP: slot_data runtime configuration missing/invalid or requested more than 44 progressive increases; AP activation aborted.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Le slot Archipelago ne fournit pas une configuration runtime V0.74 valide, ou demande plus de 44 ameliorations progressives. Le jeu reste en mode vanilla.",L"LET IT DIE Archipelago",0x10u);
        return 0;
    }
    if(status<0){
        log_ascii("V0.52 AP: Archipelago client reported an error; game remains in vanilla mode.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"La connexion Archipelago a echoue. La sauvegarde vanilla n'a pas ete modifiee. Consulte connection_result.json et client_bootstrap.log.",L"LET IT DIE Archipelago",0x10u);
        return 0;
    }

    if(client_seen){
        log_ascii("V0.52 AP: Client.py started but no final result after five minutes; game remains in vanilla mode.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Client.py a bien demarre, mais aucune reponse finale n'a ete produite apres cinq minutes. Consulte client_bootstrap.log. La sauvegarde vanilla n'a pas ete modifiee.",L"LET IT DIE Archipelago",0x10u);
    }else{
        log_ascii("V0.52 AP: timed out before Client.py bootstrap; game remains in vanilla mode.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Le client LET IT DIE Archipelago n'a pas atteint Client.py. La sauvegarde vanilla n'a pas ete modifiee.",L"LET IT DIE Archipelago",0x10u);
    }
    return 0;
}

/* V0.52: never wait for the external Archipelago client from a UE3 Tick hook.
   The blocking result-file wait runs here instead, then clears the retry guard. */
static DWORD WINAPI archipelago_bridge_worker(LPVOID unused){
    (void)unused;
    log_ascii("V0.52 AP: asynchronous bridge worker entered; UE3 game thread remains free.");
    launch_archipelago_client_and_wait();
    /* Scrub even when request creation/launcher lookup failed before child creation. */
    lid_memset(g_password,0,sizeof(g_password));
    g_bridge_running=0;
    log_ascii("V0.52 AP: bridge worker finished; Connect to Archipelago may be retried.");
    return 0;
}

/* Starts exactly one asynchronous AP bridge attempt. */
static int start_archipelago_bridge_async(void){
    DWORD thread_id=0;
    HANDLE thread_handle;

    if(g_bridge_running){
        log_ascii("V0.52 AP: bridge already running; duplicate launch ignored.");
        return 0;
    }

    if(!g_win.CreateThread){
        log_ascii("V0.52 AP: CreateThread is unavailable; bridge not started.");
        lid_memset(g_password,0,sizeof(g_password));
        return 0;
    }

    g_bridge_running=1;
    thread_handle=g_win.CreateThread(0,0,archipelago_bridge_worker,0,0,&thread_id);
    if(!thread_handle){
        g_bridge_running=0;
        lid_memset(g_password,0,sizeof(g_password));
        log_ascii("V0.52 AP: CreateThread failed; bridge not started.");
        if(g_win.MessageBoxW)g_win.MessageBoxW(0,L"Impossible de demarrer le worker Archipelago. Le jeu reste en mode vanilla.",L"LET IT DIE Archipelago",0x10u);
        return 0;
    }

    g_win.CloseHandle(thread_handle);
    log_ascii("V0.52 AP: bridge worker started; returning immediately to UE3 Base.Tick.");
    return 1;
}

static void v037_handle_ap_selection_latch(void){
    int selected;
    int*return_index;
    BYTE*states;

    if(g_dialog_open||g_bridge_running||g_ap_mode_active||g_pending_ap_release)return;
    if(!g_direct_title_injected||!g_direct_title_select||!g_direct_title_states)return;

    /* V0.38 performance fix: v26_prepare_title_for_base_tick has already
       validated/cached the exact live descriptors for this frame.  Do not run
       another chain of VirtualQuery calls on the same data every Tick. */
    if(g_direct_title_states->num!=5||!g_direct_title_states->data)return;

    selected=g_direct_title_select[0];
    return_index=&g_direct_title_select[2];
    states=(BYTE*)g_direct_title_states->data;

    if(selected!=4||*return_index!=4||states[4]!=5)return;

    /* Clear before opening the modal window so the same confirmed selection
       cannot fire again when the game thread resumes. */
    *return_index=-1;

    log_ascii("V0.52 ACTION: UE3 title selection latch confirmed AP index 4 (destination MainMenuChoiceWait=5).");
    g_dialog_open=1;
    log_ascii("V0.52 ACTION: opening Archipelago connection window.");
    if(show_dialog()){
        log_ascii("V0.52 ACTION: connection form accepted; starting asynchronous Archipelago bridge worker.");
        start_archipelago_bridge_async();
    }
    else
        log_ascii("V0.52 ACTION: connection form cancelled.");
    g_dialog_open=0;
}

static void WINAPI hk_title_base_tick(void*self,void*frame,void*result){
    int is_title;

    /* V0.52: recognize the title using exactly one fixed +0x3D0 probe. */
    is_title=v26_prepare_title_for_base_tick(self);

    /* V0.70: route the AP slot, then keep watching Chara/Body across native Game Start. */
    if(is_title&&g_pending_ap_release)process_ap_savedata_route();

    /* V0.52: after a successful hot activation, reset the live title selection to vanilla Game Start on the UE3 thread. */
    if(is_title&&g_pending_title_reset&&g_direct_title_select){
        g_direct_title_select[0]=0;
        g_direct_title_select[2]=-1;
        g_pending_title_reset=0;
        log_ascii("V0.52 HOT: title selection reset to vanilla Game Start index 0; waiting for normal user confirmation.");
    }

    /* Every unrelated BrgUIMenu_Base object goes straight back to Unreal. */
    if(!is_title){
        g_exec_title_base_tick(self,frame,result);
        if(g_ap_mode_active)v070_sync_runtime_character(0);
        if(!g_direct_tick_logged){
            log_ascii("V0.27: BrgUIMenu_Base.Tick observed.");
            g_direct_tick_logged=1;
        }
        return;
    }

    /* Preserve the proven V0.40/V0.43 title selection-latch behavior. */
    if(self==g_direct_title_self)v037_handle_ap_selection_latch();

    g_exec_title_base_tick(self,frame,result);
    if(g_ap_mode_active)v070_sync_runtime_character(1);

    if(!g_direct_tick_logged){
        log_ascii("V0.27: BrgUIMenu_Base.Tick observed.");
        g_direct_tick_logged=1;
    }

    if(self!=g_direct_title_self||!g_direct_title_injected||!g_direct_title_select)
        return;
    if(!readable_range(g_direct_title_select,sizeof(int)))return;

    /* Diagnostic only. Never rewrite mSelectIndex. */
    g_v23_title_tick_count++;
    if(g_v23_title_tick_count<=3){
        char line[300];size_t pos=0;
        int selected=*g_direct_title_select;
        line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.27: title Tick #");
        append_dec(line,sizeof(line),&pos,g_v23_title_tick_count);
        append_ascii(line,sizeof(line),&pos," select=");append_dec(line,sizeof(line),&pos,(unsigned)(selected<0?0:selected));
        if(g_direct_title_menu&&readable_range(g_direct_title_menu,sizeof(RawArray))){
            append_ascii(line,sizeof(line),&pos," menu=");append_dec(line,sizeof(line),&pos,(unsigned)g_direct_title_menu->num);
        }
        if(g_direct_title_states&&readable_range(g_direct_title_states,sizeof(RawArray))){
            append_ascii(line,sizeof(line),&pos," states=");append_dec(line,sizeof(line),&pos,(unsigned)g_direct_title_states->num);
        }
        log_ascii(line);
    }
}


static void WINAPI hk_title_base_confirm(void*self,void*frame,void*result){
    int confirmed;
    int selected;

    g_exec_title_base_confirm(self,frame,result);

    if(!g_direct_confirm_logged){
        log_ascii("V0.27: BrgUIMenu_Base.IsConfirmButtonPressed observed.");
        g_direct_confirm_logged=1;
    }

    if(self!=g_direct_title_self||!g_direct_title_injected||!result||!g_direct_title_select)
        return;
    if(!readable_range(g_direct_title_select,sizeof(int)))return;
    if(!g_direct_title_menu||!is_v26_injected_title_array(g_direct_title_menu))return;

    confirmed=(*(BYTE*)result)!=0;
    if(!confirmed)return;

    selected=*g_direct_title_select;
    {
        char line[160];size_t pos=0;
        line[0]=0;
        append_ascii(line,sizeof(line),&pos,"V0.27: confirm select=");
        append_dec(line,sizeof(line),&pos,(unsigned)(selected<0?0:selected));
        log_ascii(line);
    }

    /* Vanilla indices 0..3 are completely untouched: return TRUE as-is. */
    if(selected>=0&&selected<=3)return;

    /* Only the appended fifth entry is ours. */
    if(selected!=4)return;

    *(BYTE*)result=0;
    if(!g_dialog_open){
        g_dialog_open=1;
        log_ascii("V0.27: Connect to Archipelago index 4 selected; opening connection form.");
        if(show_dialog())
            log_ascii("V0.27: Connect form accepted (network handshake not implemented yet).");
        else
            log_ascii("V0.27: Connect form cancelled.");
        g_dialog_open=0;
    }
}

/* ---------------- Connection dialog ---------------- */
static HWND d_server,d_player,d_pass,d_status; static int d_done,d_ok; static HMODULE g_hinst;
static HWND g_owner;
static LRESULT WINAPI dialog_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){(void)lp;if(msg==WM_CREATE){d_server=g_win.CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",g_server,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,170,20,315,26,w,(HMENU)(ULONG_PTR)1001,g_hinst,0);d_player=g_win.CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",g_player,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,170,64,315,26,w,(HMENU)(ULONG_PTR)1002,g_hinst,0);d_pass=g_win.CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_PASSWORD,170,108,315,26,w,(HMENU)(ULONG_PTR)1003,g_hinst,0);d_status=g_win.CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,20,154,465,38,w,(HMENU)(ULONG_PTR)1006,g_hinst,0);g_win.CreateWindowExW(0,L"STATIC",L"Server Address",WS_CHILD|WS_VISIBLE,20,24,145,24,w,0,g_hinst,0);g_win.CreateWindowExW(0,L"STATIC",L"Player Name",WS_CHILD|WS_VISIBLE,20,68,145,24,w,0,g_hinst,0);g_win.CreateWindowExW(0,L"STATIC",L"Password (optional)",WS_CHILD|WS_VISIBLE,20,112,145,24,w,0,g_hinst,0);g_win.CreateWindowExW(0,L"BUTTON",L"Connect",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,280,204,95,30,w,(HMENU)(ULONG_PTR)1004,g_hinst,0);g_win.CreateWindowExW(0,L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,390,204,95,30,w,(HMENU)(ULONG_PTR)1005,g_hinst,0);g_win.SetFocus(d_server);return 0;}if(msg==WM_COMMAND){int id=(int)LOWORD(wp);if(id==1004){if(g_win.GetWindowTextLengthW(d_server)<=0){g_win.SetWindowTextW(d_status,L"Enter the Archipelago server address.");return 0;}if(g_win.GetWindowTextLengthW(d_player)<=0){g_win.SetWindowTextW(d_status,L"Enter the Archipelago player name.");return 0;}g_win.GetWindowTextW(d_server,g_server,512);g_win.GetWindowTextW(d_player,g_player,256);g_win.GetWindowTextW(d_pass,g_password,256);d_ok=1;d_done=1;g_win.DestroyWindow(w);return 0;}if(id==1005){d_ok=0;d_done=1;g_win.DestroyWindow(w);return 0;}}if(msg==WM_CLOSE){d_ok=0;d_done=1;g_win.DestroyWindow(w);return 0;}return g_win.DefWindowProcW(w,msg,wp,lp);}
static BOOL WINAPI enum_proc(HWND w,LPARAM p){DWORD pid=0;HWND*res=(HWND*)(ULONG_PTR)p;g_win.GetWindowThreadProcessId(w,&pid);if(pid==g_win.GetCurrentProcessId()&&g_win.IsWindowVisible(w)&&g_win.GetWindow(w,GW_OWNER)==0){*res=w;return FALSE;}return TRUE;}
static HWND find_owner(void){HWND w=0;g_win.EnumWindows(enum_proc,(LPARAM)(ULONG_PTR)&w);return w;}
static int register_dialog(void){static int done;WNDCLASSEXW wc;if(done)return 1;lid_memset(&wc,0,sizeof(wc));wc.cbSize=sizeof(wc);wc.lpfnWndProc=dialog_proc;wc.hInstance=g_hinst;wc.hbrBackground=(HBRUSH)(ULONG_PTR)(COLOR_WINDOW+1);wc.lpszClassName=L"LetItDieArchipelagoConnectionDialog";if(!g_win.RegisterClassExW(&wc)&&g_win.GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;done=1;return 1;}
static int show_dialog(void){HWND w;MSG m;RECT pr,cr;int x,y;if(!lid_init_user_api()||!register_dialog())return 0;g_owner=find_owner();d_done=d_ok=0;g_password[0]=0;w=g_win.CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_TOPMOST,L"LetItDieArchipelagoConnectionDialog",L"Connect to Archipelago",WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,525,285,g_owner,0,g_hinst,0);if(!w)return 0;if(g_owner&&g_win.GetWindowRect(g_owner,&pr)&&g_win.GetWindowRect(w,&cr)){x=pr.left+((pr.right-pr.left)-(cr.right-cr.left))/2;y=pr.top+((pr.bottom-pr.top)-(cr.bottom-cr.top))/2;g_win.SetWindowPos(w,HWND_TOPMOST,x,y,0,0,SWP_NOSIZE|SWP_SHOWWINDOW);}if(g_owner)g_win.EnableWindow(g_owner,FALSE);g_win.ShowWindow(w,SW_SHOW);g_win.UpdateWindow(w);while(!d_done&&g_win.GetMessageW(&m,0,0,0)>0){if(!g_win.IsDialogMessageW(w,&m)){g_win.TranslateMessage(&m);g_win.DispatchMessageW(&m);}}if(g_owner){g_win.EnableWindow(g_owner,TRUE);g_win.SetForegroundWindow(g_owner);}return d_ok;}

/* ---------------- vtable hooks ---------------- */
static int injected_for(void*self){return self==g_title_self&&g_title_injected;}

static void inject(void*self,RawArray*src){
    RawArray*a;int prev=0,shift;
    if(!g_set_units||!src)return;
    if(g_get_num&&g_get_index&&g_get_num(self)>0)prev=g_get_index(self);
    a=build_injected(self,src);
    if(!a)return;
    g_set_units(self,a);
    g_title_self=self;
    g_title_injected=1;
    if(g_set_index){
        shift=prev+1;
        if(shift>=a->num)shift=a->num-1;
        if(shift<0)shift=0;
        g_set_index(self,shift);
    }
    log_ascii("Injected Connect to Archipelago into a main title menu variant.");
}

static void WINAPI hk_native_initialize(void*self){
    g_native_initialize(self);
}

static void WINAPI hk_native_set_top(void*self,BYTE mode){
    g_native_set_top(self,mode);
}

static void WINAPI hk_start_common(void*self,BYTE mode,RawArray*units,int selected,int arg5,int arg6){
    RawArray*a=0;
    if(is_main_title_array(units)){
        a=build_injected(self,units);
        if(a){
            g_title_self=self;
            g_title_injected=0;
            log_ascii("Recognized main title menu through StartCommonTop2.");
            g_start_common(self,mode,a,selected+1,arg5,arg6);
            g_title_injected=1;
            log_ascii("Injected Connect to Archipelago into a main title menu variant.");
            return;
        }
    }
    if(self==g_title_self){g_title_self=0;g_title_injected=0;}
    g_start_common(self,mode,units,selected,arg5,arg6);
}

static void WINAPI hk_set_units(void*self,RawArray*units){
    if(plausible_string_array(units))log_candidate_array(0,units);
    copy_array(self,units);
    if(is_main_title_array(units)){
        g_title_self=self;
        g_title_injected=0;
        inject(self,&g_pending_array);
        return;
    }
    g_set_units(self,units);
}

static void WINAPI hk_change_name(void*self,RawFString*name,BYTE update){
    int title=fstring_eq(name,L"Brg Title Menu");
    if(title){g_title_self=self;g_title_injected=0;}
    g_change_name(self,name,update);
    if(title&&g_pending_self==self&&g_pending_array.data)inject(self,&g_pending_array);
}
static void WINAPI hk_set_index(void*self,int idx){if(injected_for(self)&&idx>=0)idx++;g_set_index(self,idx);}
static int WINAPI hk_get_index(void*self){int a=g_get_index(self);if(!injected_for(self))return a;return a<=0?0:a-1;}
static int WINAPI hk_get_num(void*self){int a=g_get_num(self);if(injected_for(self)&&a>0)a--;return a;}
static int WINAPI hk_check_end(void*self){int r=g_check_end(self),actual;if(!r||!injected_for(self))return r;actual=g_get_index?g_get_index(self):-1;if(actual!=0)return r;if(!g_dialog_open){g_dialog_open=1;if(show_dialog())log_ascii("Connect form accepted (network handshake not implemented yet).");g_dialog_open=0;}if(g_reset_choice)g_reset_choice(self);return 0;}
static int patch_slot(void**vt,size_t off,void*det,void**orig){DWORD old=0,ig=0;void**slot=&vt[off/8];*orig=*slot;if(!*orig)return 0;if(!g_win.VirtualProtect(slot,8,PAGE_EXECUTE_READWRITE,&old))return 0;*slot=det;g_win.VirtualProtect(slot,8,old,&ig);return 1;}
static int patch_vtable(void*self){
    void**vt;
    if(!self)return 0;
    vt=*(void***)self;
    if(!vt)return 0;
    if(g_vtable==vt&&g_set_units&&g_start_common&&g_native_initialize&&g_native_set_top)return 1;
    if(g_vtable&&g_vtable!=vt)return 0;
    g_vtable=vt;
    g_reset_choice=(NativeResetChoice)vt[0x348/8];
    if(!g_reset_choice)return 0;
    if(!patch_slot(vt,0x258,(void*)hk_native_initialize,(void**)&g_native_initialize))return 0;
    if(!patch_slot(vt,0x310,(void*)hk_set_units,(void**)&g_set_units))return 0;
    if(!patch_slot(vt,0x328,(void*)hk_start_common,(void**)&g_start_common))return 0;
    if(!patch_slot(vt,0x330,(void*)hk_change_name,(void**)&g_change_name))return 0;
    if(!patch_slot(vt,0x360,(void*)hk_native_set_top,(void**)&g_native_set_top))return 0;
    if(!patch_slot(vt,0x370,(void*)hk_check_end,(void**)&g_check_end))return 0;
    if(!patch_slot(vt,0x398,(void*)hk_set_index,(void**)&g_set_index))return 0;
    if(!patch_slot(vt,0x3A0,(void*)hk_get_index,(void**)&g_get_index))return 0;
    if(!patch_slot(vt,0x3B0,(void*)hk_get_num,(void**)&g_get_num))return 0;
    log_ascii("CommonTop2 vtable hooked: native Initialize/SetUnitArray/StartCommonTop2/SetTopMenu.");
    return 1;
}
static void WINAPI hk_exec_set(void*self,void*frame,void*result){log_ascii("Observed execSetUnitArray.");patch_vtable(self);g_exec_set_units(self,frame,result);}
static void WINAPI hk_exec_name(void*self,void*frame,void*result){log_ascii("Observed execChangeMenuName.");patch_vtable(self);g_exec_change_name(self,frame,result);}
static void WINAPI hk_exec_start(void*self,void*frame,void*result){log_ascii("Observed execStartCommonTop2.");patch_vtable(self);g_exec_start(self,frame,result);}
static void WINAPI hk_exec_initialize(void*self,void*frame,void*result){
    static int logged;
    if(!logged){log_ascii("Observed execInitialize.");logged=1;}
    patch_vtable(self);
    g_exec_initialize(self,frame,result);
}
static void WINAPI hk_exec_set_top(void*self,void*frame,void*result){log_ascii("Observed execSetTopMenu.");patch_vtable(self);g_exec_set_top(self,frame,result);}

static int install_hooks(void){
    void*exe=lid_main_module();
    void*tick=resolve_exec(exe,"UBrgUIMenu_BaseexecTick");
    void*confirm=resolve_exec(exe,"UBrgUIMenu_BaseexecIsConfirmButtonPressed");

    /*
    Uploaded Steam build 5.0.4.2.0.
    Both 14-byte prefixes contain no RIP-relative instruction and are safe for
    the small trampoline used by this test mod.
    */
    static const BYTE tick_prefix[]={
        0x48,0x89,0x5C,0x24,0x10,
        0x57,
        0x48,0x83,0xEC,0x20,
        0x48,0x8B,0x42,0x24
    };

    static const BYTE confirm_prefix[]={
        0x48,0x89,0x5C,0x24,0x08,
        0x57,
        0x48,0x83,0xEC,0x20,
        0x48,0xFF,0x42,0x24
    };

    if(tick)log_ascii("Resolved BrgUIMenu_Base Tick wrapper.");
    if(confirm)log_ascii("Resolved BrgUIMenu_Base IsConfirmButtonPressed wrapper.");

    if(!tick||!confirm){
        log_ascii("Could not resolve BrgUIMenu_Base title hooks.");
        return 0;
    }

    if(!hook_install(
            &h_title_base_tick,
            tick,
            (void*)hk_title_base_tick,
            tick_prefix,
            sizeof(tick_prefix))){
        log_ascii("BrgUIMenu_Base Tick wrapper mismatch.");
        return 0;
    }

    g_exec_title_base_tick=(ExecFunction)h_title_base_tick.trampoline;

    if(!hook_install(
            &h_title_base_confirm,
            confirm,
            (void*)hk_title_base_confirm,
            confirm_prefix,
            sizeof(confirm_prefix))){
        log_ascii("BrgUIMenu_Base IsConfirmButtonPressed wrapper mismatch.");
        return 0;
    }

    g_exec_title_base_confirm=(ExecFunction)h_title_base_confirm.trampoline;

    log_ascii("V0.52 title hooks installed (fixed +0x3D0 title probe; AP bridge wait is off the UE3 game thread).");
    return 1;
}
static DWORD WINAPI worker(LPVOID x){
    (void)x;
    if(!lid_init_kernel_api())return 1;
    lid_init_user_api();

    log_ascii("LET IT DIE Archipelago V0.82 DEATHBAG POSSESSION LIST DLL loaded.");
    log_ascii("Base: validated V0.73 boot/Grade watcher; stat targets now come from current Archipelago slot_data instead of compiled constants.");
    log_ascii("V0.82 FIX: exact V0.76 base; trim the proven CustomCharaNative::GetPossessionItem UI list used by BrgUIMenu_DeathBag to the YAML/AP Death Bag capacity; no restart or userdata reload.");
    log_ascii("V0.52 BOOT: exact stable boot path; dinput8 installs NO game file hooks before the title menu.");

    install_crash_diagnostics();

    if(!install_hooks())return 2;
    return 0;
}

BOOL WINAPI DllMain(HMODULE m,DWORD reason,LPVOID reserved){
    (void)reserved;

    if(reason==DLL_PROCESS_ATTACH){
        DWORD id=0;
        HANDLE t;

        g_self=g_hinst=m;

        if(!lid_init_kernel_api())return TRUE;

        t=g_win.CreateThread(0,0,worker,0,0,&id);
        if(t)g_win.CloseHandle(t);
    }
    else if(reason==DLL_PROCESS_DETACH){
        if(g_remove_veh&&g_veh_handle){g_remove_veh(g_veh_handle);g_veh_handle=0;}
        if(g_dbghelp&&g_win.FreeLibrary){g_win.FreeLibrary(g_dbghelp);g_dbghelp=0;}
        if(g_win.VirtualProtect){
            restore_direct_title_action_hook();
            g_savedata_route_active=0;
            hook_remove(&h_savedata_dir);
            g_savedata_dir_original=0;
            hook_remove(&h_title_base_confirm);
            hook_remove(&h_title_base_tick);
            hook_remove(&h_rage_body_status);
            g_rage_body_status_original=0;
            g_rage_body_status_ready=0;
            if(g_r21_get_chara_slot&&*g_r21_get_chara_slot==(void*)r21_get_chara){
                void*previous=0;
                v076_write_vtable_slot(g_r21_get_chara_slot,(void*)g_r21_get_chara_original,&previous);
            }
            g_r21_get_chara_slot=0;g_r21_userdata=0;
        }

        /* V0.24: published title allocations belong to UE3/tbbmalloc. */
        g_direct_title_menu_block=0;
        g_direct_title_state_block=0;
        g_direct_title_guide_block=0;

        free_pending();
    }

    return TRUE;
}
