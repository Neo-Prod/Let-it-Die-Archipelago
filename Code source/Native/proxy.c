#include "winlite.h"

typedef HRESULT (WINAPI *DirectInput8CreateFn)(HINSTANCE,DWORD,const void*,void**,LPUNKNOWN);
typedef HANDLE (WINAPI *ImpCreateFileW)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
typedef HANDLE (WINAPI *ImpCreateFileA)(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
typedef BOOL (WINAPI *ImpDeleteFileW)(LPCWSTR);
typedef BOOL (WINAPI *ImpDeleteFileA)(LPCSTR);
typedef BOOL (WINAPI *ImpCopyFileW)(LPCWSTR,LPCWSTR,BOOL);
typedef BOOL (WINAPI *ImpMoveFileW)(LPCWSTR,LPCWSTR);
typedef void* (WINAPI *ImpWFOpen)(LPCWSTR,LPCWSTR);
typedef void* (WINAPI *ImpFOpen)(LPCSTR,LPCSTR);
typedef int (WINAPI *ImpFOpenS)(void**,LPCSTR,LPCSTR);
typedef BOOL (WINAPI *ImpWriteFile)(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
typedef BOOL (WINAPI *ImpCloseHandle)(HANDLE);
typedef int (WINAPI *ImpFClose)(void*);
typedef BOOL (WINAPI *ImpMoveFileExW)(LPCWSTR,LPCWSTR,DWORD);
typedef BOOL (WINAPI *ImpReplaceFileW)(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPVOID,LPVOID);
typedef int (WINAPI *ImpRenameA)(LPCSTR,LPCSTR);
typedef int (WINAPI *ImpRemoveA)(LPCSTR);

typedef struct ImportPatch { void** slot; void* original; } ImportPatch;

static HMODULE g_real;
static DirectInput8CreateFn g_create;
static ImportPatch g_import_patches[32];
static int g_import_patch_count;
static volatile LONG g_hooks_installed;
static volatile LONG g_overlay_active;
static volatile LONG g_overlay_status;
static wchar_t g_ap_save_w[900],g_ap_tmp_w[900],g_ap_bak_w[900],g_ap_masters_w[900];
static char g_ap_save_a[900],g_ap_tmp_a[900],g_ap_bak_a[900],g_ap_masters_a[900];
static volatile LONG g_startup_save_guard;
static ImpCreateFileW g_orig_CreateFileW;
static ImpCreateFileA g_orig_CreateFileA;
static ImpDeleteFileW g_orig_DeleteFileW;
static ImpDeleteFileA g_orig_DeleteFileA;
static ImpCopyFileW g_orig_CopyFileW;
static ImpMoveFileW g_orig_MoveFileW;
static ImpWFOpen g_orig_wfopen;
static ImpFOpen g_orig_fopen;
static ImpFOpenS g_orig_fopen_s;
static ImpWriteFile g_orig_WriteFile;
static ImpCloseHandle g_orig_CloseHandle;
static ImpFClose g_orig_fclose;
static ImpMoveFileExW g_orig_MoveFileExW;
static ImpReplaceFileW g_orig_ReplaceFileW;
static ImpRenameA g_orig_rename;
static ImpRemoveA g_orig_remove;

/* Returns 0 while vanilla, 1 after runtime AP activation, and -1 on activation failure. */
__declspec(dllexport) int WINAPI LID_GetOverlayStatus(void){return (int)g_overlay_status;}

/* Returns whether the runtime file hooks are currently installed. */
__declspec(dllexport) int WINAPI LID_GetOverlayHookStatus(void){return g_hooks_installed?1:0;}

/* V0.52 deliberately performs no pre-activation file interception. */
__declspec(dllexport) int WINAPI LID_GetSaveTouchesBeforeActivation(void){return -1;}
__declspec(dllexport) int WINAPI LID_GetMastersTouchesBeforeActivation(void){return -1;}

/* Returns ASCII lowercase for a single byte. */
static int ascii_lower(int c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}

/* Returns lowercase for a single UTF-16 code unit in the ASCII range. */
static wchar_t wide_lower(wchar_t c){return c>=L'A'&&c<=L'Z'?(wchar_t)(c+32):c;}

/* Checks an ANSI suffix without case sensitivity. */
static int ends_ci_a(const char*s,const char*suf){size_t n,m,i;if(!s||!suf)return 0;n=lid_strlen(s);m=lid_strlen(suf);if(n<m)return 0;for(i=0;i<m;i++)if(ascii_lower((unsigned char)s[n-m+i])!=ascii_lower((unsigned char)suf[i]))return 0;return 1;}

/* Checks a UTF-16 suffix without case sensitivity. */
static int ends_ci_w(const wchar_t*s,const wchar_t*suf){size_t n,m,i;if(!s||!suf)return 0;n=lid_wcslen(s);m=lid_wcslen(suf);if(n<m)return 0;for(i=0;i<m;i++)if(wide_lower(s[n-m+i])!=wide_lower(suf[i]))return 0;return 1;}

/* Redirects the complete AP save family after runtime activation. */
static const wchar_t* redirect_path_w(const wchar_t*p){if(!p||!g_overlay_active)return p;if(ends_ci_w(p,L"masters.db"))return g_ap_masters_w;if(ends_ci_w(p,L"brggame.sav"))return g_ap_save_w;if(ends_ci_w(p,L"brggame.tmp"))return g_ap_tmp_w;if(ends_ci_w(p,L"brggame.bak"))return g_ap_bak_w;return p;}

/* Redirects the complete AP save family for ANSI file APIs. */
static const char* redirect_path_a(const char*p){if(!p||!g_overlay_active)return p;if(ends_ci_a(p,"masters.db"))return g_ap_masters_a;if(ends_ci_a(p,"brggame.sav"))return g_ap_save_a;if(ends_ci_a(p,"brggame.tmp"))return g_ap_tmp_a;if(ends_ci_a(p,"brggame.bak"))return g_ap_bak_a;return p;}

/* V0.68 diagnostic: forward declaration used by the trace writer. */
static void get_game_dir(wchar_t* out,size_t cap);

typedef struct TraceHandle {
    HANDLE h;
    int used;
    int writable;
    int wrote;
    wchar_t path[900];
} TraceHandle;

typedef struct TraceStream {
    void* stream;
    int used;
    int writable;
    wchar_t path[900];
} TraceStream;

static TraceHandle g_trace_handles[48];
static TraceStream g_trace_streams[24];
static volatile DWORD g_trace_seq;
static BYTE g_trace_copy_buffer[65536];
static int g_trace_writefile_hook;
static int g_trace_closehandle_hook;
static int g_trace_fclose_hook;

static int contains_ci_w(const wchar_t*s,const wchar_t*needle){size_t n,m,i,j;if(!s||!needle)return 0;n=lid_wcslen(s);m=lid_wcslen(needle);if(!m||n<m)return 0;for(i=0;i+m<=n;i++){for(j=0;j<m;j++)if(wide_lower(s[i+j])!=wide_lower(needle[j]))break;if(j==m)return 1;}return 0;}
static int contains_ci_a(const char*s,const char*needle){size_t n,m,i,j;if(!s||!needle)return 0;n=lid_strlen(s);m=lid_strlen(needle);if(!m||n<m)return 0;for(i=0;i+m<=n;i++){for(j=0;j<m;j++)if(ascii_lower((unsigned char)s[i+j])!=ascii_lower((unsigned char)needle[j]))break;if(j==m)return 1;}return 0;}
static int trace_relevant_w(const wchar_t*p){return p&&contains_ci_w(p,L"brggame");}
static int trace_relevant_a(const char*p){return p&&contains_ci_a(p,"brggame");}
static int trace_access_writable(DWORD a){return (a&0x40000000u)||(a&0x10000000u)||(a&0x00000004u);}
static int trace_mode_writable_a(const char*m){size_t i;if(!m)return 0;for(i=0;m[i];i++)if(m[i]=='w'||m[i]=='a'||m[i]=='+')return 1;return 0;}
static int trace_mode_writable_w(const wchar_t*m){size_t i;if(!m)return 0;for(i=0;m[i];i++)if(m[i]==L'w'||m[i]==L'a'||m[i]==L'+')return 1;return 0;}
static void trace_a_to_w(const char*a,wchar_t*w,size_t cap){size_t i=0;if(!cap)return;while(a&&a[i]&&i+1<cap){w[i]=(wchar_t)(unsigned char)a[i];i++;}w[i]=0;}
static void trace_append(char*out,size_t cap,size_t*pos,const char*s){size_t i=0;while(s&&s[i]&&*pos+1<cap)out[(*pos)++]=s[i++];out[*pos]=0;}
static void trace_append_dec(char*out,size_t cap,size_t*pos,unsigned long long v){char t[32];size_t n=0,i;if(v==0){if(*pos+1<cap)out[(*pos)++]='0';out[*pos]=0;return;}while(v&&n<31){t[n++]=(char)('0'+(v%10));v/=10;}for(i=0;i<n&&*pos+1<cap;i++)out[(*pos)++]=t[n-1-i];out[*pos]=0;}
static void trace_append_hex(char*out,size_t cap,size_t*pos,ULONG_PTR v){static const char h[]="0123456789ABCDEF";int shift;trace_append(out,cap,pos,"0x");for(shift=60;shift>=0;shift-=4){if(*pos+1<cap)out[(*pos)++]=h[(v>>shift)&15u];}out[*pos]=0;}
static void trace_append_path_w(char*out,size_t cap,size_t*pos,const wchar_t*w){size_t i=0;while(w&&w[i]&&*pos+1<cap){unsigned c=w[i++];out[(*pos)++]=(char)(c<128?c:'?');}out[*pos]=0;}
static void trace_append_path_a(char*out,size_t cap,size_t*pos,const char*a){trace_append(out,cap,pos,a?a:"(null)");}
static ULONG_PTR trace_main_rva(ULONG_PTR caller){BYTE*b=(BYTE*)lid_main_module();DWORD pe,size;if(!b||*(WORD*)b!=0x5A4D)return 0;pe=*(DWORD*)(b+0x3c);size=*(DWORD*)(b+pe+24+56);if(caller<(ULONG_PTR)b||caller>=(ULONG_PTR)b+size)return 0;return caller-(ULONG_PTR)b;}
static __attribute__((noinline)) void trace_log_line(const char*line){wchar_t dir[900],path[900];HANDLE h;DWORD wrote=0;get_game_dir(dir,900);lid_wappend(dir,900,L"\\Archipelago\\diagnostic");g_win.CreateDirectoryW(dir,0);lid_wcopy(path,900,dir);lid_wappend(path,900,L"\\save_write_trace.log");h=g_win.CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return;g_win.WriteFile(h,line,(DWORD)lid_strlen(line),&wrote,0);g_win.WriteFile(h,"\r\n",2,&wrote,0);g_win.CloseHandle(h);}
static __attribute__((noinline)) DWORD trace_log_w(const char*api,const wchar_t*p1,const wchar_t*p2,DWORD v1,DWORD v2,ULONG_PTR caller){char line[2600];size_t pos=0;DWORD seq=++g_trace_seq;ULONG_PTR rva=trace_main_rva(caller);line[0]=0;trace_append(line,sizeof(line),&pos,"TRACE #");trace_append_dec(line,sizeof(line),&pos,seq);trace_append(line,sizeof(line),&pos," api=");trace_append(line,sizeof(line),&pos,api);trace_append(line,sizeof(line),&pos," caller=");trace_append_hex(line,sizeof(line),&pos,caller);trace_append(line,sizeof(line),&pos," main_rva=");if(rva)trace_append_hex(line,sizeof(line),&pos,rva);else trace_append(line,sizeof(line),&pos,"OUTSIDE_MAIN");trace_append(line,sizeof(line),&pos," v1=");trace_append_hex(line,sizeof(line),&pos,(ULONG_PTR)v1);trace_append(line,sizeof(line),&pos," v2=");trace_append_hex(line,sizeof(line),&pos,(ULONG_PTR)v2);trace_append(line,sizeof(line),&pos," p1=");trace_append_path_w(line,sizeof(line),&pos,p1);if(p2){trace_append(line,sizeof(line),&pos," p2=");trace_append_path_w(line,sizeof(line),&pos,p2);}trace_log_line(line);return seq;}
static __attribute__((noinline)) DWORD trace_log_a(const char*api,const char*p1,const char*p2,DWORD v1,DWORD v2,ULONG_PTR caller){char line[2600];size_t pos=0;DWORD seq=++g_trace_seq;ULONG_PTR rva=trace_main_rva(caller);line[0]=0;trace_append(line,sizeof(line),&pos,"TRACE #");trace_append_dec(line,sizeof(line),&pos,seq);trace_append(line,sizeof(line),&pos," api=");trace_append(line,sizeof(line),&pos,api);trace_append(line,sizeof(line),&pos," caller=");trace_append_hex(line,sizeof(line),&pos,caller);trace_append(line,sizeof(line),&pos," main_rva=");if(rva)trace_append_hex(line,sizeof(line),&pos,rva);else trace_append(line,sizeof(line),&pos,"OUTSIDE_MAIN");trace_append(line,sizeof(line),&pos," v1=");trace_append_hex(line,sizeof(line),&pos,(ULONG_PTR)v1);trace_append(line,sizeof(line),&pos," v2=");trace_append_hex(line,sizeof(line),&pos,(ULONG_PTR)v2);trace_append(line,sizeof(line),&pos," p1=");trace_append_path_a(line,sizeof(line),&pos,p1);if(p2){trace_append(line,sizeof(line),&pos," p2=");trace_append_path_a(line,sizeof(line),&pos,p2);}trace_log_line(line);return seq;}
static void trace_append_wchar_ascii(wchar_t*out,size_t cap,const char*s){size_t n=lid_wcslen(out),i=0;while(s&&s[i]&&n+1<cap)out[n++]=(wchar_t)(unsigned char)s[i++];out[n]=0;}
static void trace_append_dec_w(wchar_t*out,size_t cap,DWORD v){wchar_t t[24];size_t n=0,i,pos=lid_wcslen(out);if(v==0){if(pos+1<cap){out[pos++]=L'0';out[pos]=0;}return;}while(v&&n<23){t[n++]=(wchar_t)(L'0'+(v%10));v/=10;}for(i=0;i<n&&pos+1<cap;i++)out[pos++]=t[n-1-i];out[pos]=0;}
static __attribute__((noinline)) int trace_copy_file(const wchar_t*src,const wchar_t*dst){HANDLE in_h,out_h;DWORD got,wrote,off;if(!src||!dst)return 0;in_h=g_win.CreateFileW(src,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(in_h==INVALID_HANDLE_VALUE)return 0;out_h=g_win.CreateFileW(dst,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(out_h==INVALID_HANDLE_VALUE){g_win.CloseHandle(in_h);return 0;}for(;;){got=0;if(!g_win.ReadFile(in_h,g_trace_copy_buffer,sizeof(g_trace_copy_buffer),&got,0))break;if(!got)break;off=0;while(off<got){wrote=0;if(!g_win.WriteFile(out_h,g_trace_copy_buffer+off,got-off,&wrote,0)||!wrote){off=got;break;}off+=wrote;}}g_win.CloseHandle(out_h);g_win.CloseHandle(in_h);return 1;}
static __attribute__((noinline)) void trace_snapshot(const wchar_t*src,const char*tag,DWORD seq){wchar_t dir[900],dst[900];if(!src||!trace_relevant_w(src))return;get_game_dir(dir,900);lid_wappend(dir,900,L"\\Archipelago\\diagnostic");g_win.CreateDirectoryW(dir,0);lid_wcopy(dst,900,dir);lid_wappend(dst,900,L"\\snap_");trace_append_dec_w(dst,900,seq);lid_wappend(dst,900,L"_");trace_append_wchar_ascii(dst,900,tag);lid_wappend(dst,900,L".savdump");trace_copy_file(src,dst);}
static void trace_track_handle(HANDLE h,const wchar_t*p,int writable){int i;if(h==INVALID_HANDLE_VALUE||!p||!trace_relevant_w(p))return;for(i=0;i<48;i++)if(!g_trace_handles[i].used){g_trace_handles[i].used=1;g_trace_handles[i].h=h;g_trace_handles[i].writable=writable;g_trace_handles[i].wrote=0;lid_wcopy(g_trace_handles[i].path,900,p);return;}}
static TraceHandle* trace_find_handle(HANDLE h){int i;for(i=0;i<48;i++)if(g_trace_handles[i].used&&g_trace_handles[i].h==h)return &g_trace_handles[i];return 0;}
static void trace_track_stream(void*stream,const wchar_t*p,int writable){int i;if(!stream||!p||!trace_relevant_w(p))return;for(i=0;i<24;i++)if(!g_trace_streams[i].used){g_trace_streams[i].used=1;g_trace_streams[i].stream=stream;g_trace_streams[i].writable=writable;lid_wcopy(g_trace_streams[i].path,900,p);return;}}
static TraceStream* trace_find_stream(void*stream){int i;for(i=0;i<24;i++)if(g_trace_streams[i].used&&g_trace_streams[i].stream==stream)return &g_trace_streams[i];return 0;}
static __attribute__((noinline)) void trace_start(void){wchar_t dir[900],log[900];static char line[512];size_t pos=0;get_game_dir(dir,900);lid_wappend(dir,900,L"\\Archipelago\\diagnostic");g_win.CreateDirectoryW(dir,0);lid_wcopy(log,900,dir);lid_wappend(log,900,L"\\save_write_trace.log");g_win.DeleteFileW(log);g_trace_seq=0;line[0]=0;trace_append(line,sizeof(line),&pos,"V0.68 SAVE WRITE TRACE START; initial AP save snapshot follows. WriteFileHook=");trace_append_dec(line,sizeof(line),&pos,(unsigned)g_trace_writefile_hook);trace_append(line,sizeof(line),&pos," CloseHandleHook=");trace_append_dec(line,sizeof(line),&pos,(unsigned)g_trace_closehandle_hook);trace_append(line,sizeof(line),&pos," fcloseHook=");trace_append_dec(line,sizeof(line),&pos,(unsigned)g_trace_fclose_hook);trace_log_line(line);trace_snapshot(g_ap_save_w,"initial_ap_save",0);}

/* CreateFileW detour: behavior unchanged, but every brggame-family open is traced. */
static HANDLE WINAPI hk_CreateFileW(LPCWSTR p,DWORD a,DWORD sh,LPVOID sa,DWORD cd,DWORD fl,HANDLE t){const wchar_t*q=redirect_path_w(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);HANDLE h;DWORD seq=0;if(trace_relevant_w(p)||trace_relevant_w(q)){seq=trace_log_w("CreateFileW",q,p!=q?p:0,a,cd,caller);if(trace_access_writable(a)&&(cd==1u||cd==2u||cd==5u))trace_snapshot(g_ap_save_w,"before_destructive_open",seq);}h=g_orig_CreateFileW(q,a,sh,sa,cd,fl,t);if(trace_relevant_w(q))trace_track_handle(h,q,trace_access_writable(a));return h;}

/* CreateFileA detour: behavior unchanged, but every brggame-family open is traced. */
static HANDLE WINAPI hk_CreateFileA(LPCSTR p,DWORD a,DWORD sh,LPVOID sa,DWORD cd,DWORD fl,HANDLE t){const char*q=redirect_path_a(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);HANDLE h;wchar_t qw[900];DWORD seq=0;if(trace_relevant_a(p)||trace_relevant_a(q)){seq=trace_log_a("CreateFileA",q,p!=q?p:0,a,cd,caller);if(trace_access_writable(a)&&(cd==1u||cd==2u||cd==5u))trace_snapshot(g_ap_save_w,"before_destructive_open",seq);}h=g_orig_CreateFileA(q,a,sh,sa,cd,fl,t);if(trace_relevant_a(q)){trace_a_to_w(q,qw,900);trace_track_handle(h,qw,trace_access_writable(a));}return h;}

/* WriteFile detour: records the exact caller RVA writing a tracked save handle. */
static BOOL WINAPI hk_WriteFile(HANDLE h,LPCVOID b,DWORD n,DWORD*w,LPVOID o){TraceHandle*t=trace_find_handle(h);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);BOOL r=g_orig_WriteFile(h,b,n,w,o);if(t){t->wrote=1;trace_log_w("WriteFile",t->path,0,n,w?*w:0,caller);}return r;}

/* CloseHandle detour: snapshots each completed write, including temp/bak files. */
static BOOL WINAPI hk_CloseHandle(HANDLE h){TraceHandle*t=trace_find_handle(h);wchar_t path[900];int snap=0;ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);BOOL r;if(t){lid_wcopy(path,900,t->path);snap=t->writable||t->wrote;t->used=0;}r=g_orig_CloseHandle(h);if(snap){DWORD seq=trace_log_w("CloseHandle(write)",path,0,0,0,caller);trace_snapshot(path,"closed_written_target",seq);trace_snapshot(g_ap_save_w,"main_after_close",seq);}return r;}

/* DeleteFileW detour. During the one pre-load save transaction, preserve the verified AP save. */
static BOOL WINAPI hk_DeleteFileW(LPCWSTR p){const wchar_t*q=redirect_path_w(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);if(trace_relevant_w(p)||trace_relevant_w(q))trace_log_w("DeleteFileW",q,p!=q?p:0,0,0,caller);if(g_startup_save_guard==2&&p&&ends_ci_w(p,L"brggame.sav")){trace_log_line("V0.69 GUARD: suppressed pre-load DeleteFileW(brggame.sav); verified Grade-6 AP save preserved.");return TRUE;}return g_orig_DeleteFileW(q);}

/* DeleteFileA detour. Same protection for ANSI callers. */
static BOOL WINAPI hk_DeleteFileA(LPCSTR p){const char*q=redirect_path_a(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);if(trace_relevant_a(p)||trace_relevant_a(q))trace_log_a("DeleteFileA",q,p!=q?p:0,0,0,caller);if(g_startup_save_guard==2&&p&&ends_ci_a(p,"brggame.sav")){trace_log_line("V0.69 GUARD: suppressed pre-load DeleteFileA(brggame.sav); verified Grade-6 AP save preserved.");return TRUE;}return g_orig_DeleteFileA(q);}

/* CopyFileW detour. */
static BOOL WINAPI hk_CopyFileW(LPCWSTR a,LPCWSTR b,BOOL fail){const wchar_t*qa=redirect_path_w(a),*qb=redirect_path_w(b);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);BOOL r;if(trace_relevant_w(qa)||trace_relevant_w(qb))trace_log_w("CopyFileW",qa,qb,(DWORD)fail,0,caller);r=g_orig_CopyFileW(qa,qb,fail);if(r&&trace_relevant_w(qb)){DWORD seq=trace_log_w("CopyFileW(done)",qb,0,0,0,caller);trace_snapshot(qb,"after_copy",seq);trace_snapshot(g_ap_save_w,"main_after_copy",seq);}return r;}

/* MoveFileW detour. The first Game Start save serializes stale Grade-1 userdata before loading the AP slot.
   Preserve the verified Grade-6 AP save across exactly that one atomic sav->bak / tmp->sav transaction. */
static BOOL WINAPI hk_MoveFileW(LPCWSTR a,LPCWSTR b){const wchar_t*qa=redirect_path_w(a),*qb=redirect_path_w(b);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);BOOL r;if(trace_relevant_w(qa)||trace_relevant_w(qb))trace_log_w("MoveFileW",qa,qb,0,0,caller);
    if(g_startup_save_guard==1&&a&&b&&ends_ci_w(a,L"brggame.sav")&&ends_ci_w(b,L"brggame.bak")){
        r=g_orig_CopyFileW(qa,qb,FALSE);
        if(r){g_startup_save_guard=2;trace_log_line("V0.69 GUARD: intercepted first sav->bak move; copied Grade-6 AP save to AP backup without removing main save.");}
        else trace_log_line("V0.69 GUARD ERROR: could not preserve AP save while intercepting first sav->bak move.");
        return r;
    }
    if(g_startup_save_guard==2&&a&&b&&ends_ci_w(a,L"brggame.tmp")&&ends_ci_w(b,L"brggame.sav")){
        if(g_orig_DeleteFileW)g_orig_DeleteFileW(qa);
        g_startup_save_guard=0;
        trace_log_line("V0.69 GUARD: discarded stale Grade-1 pre-load brggame.tmp; Grade-6 AP save remains authoritative. Guard released.");
        return TRUE;
    }
    r=g_orig_MoveFileW(qa,qb);if(r&&trace_relevant_w(qb)){DWORD seq=trace_log_w("MoveFileW(done)",qb,0,0,0,caller);trace_snapshot(qb,"after_move",seq);trace_snapshot(g_ap_save_w,"main_after_move",seq);}return r;}

/* Optional file APIs use the same AP save-family routing. */
static BOOL WINAPI hk_MoveFileExW(LPCWSTR a,LPCWSTR b,DWORD flags){const wchar_t*qa=redirect_path_w(a),*qb=redirect_path_w(b);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);BOOL r;if(trace_relevant_w(qa)||trace_relevant_w(qb))trace_log_w("MoveFileExW",qa,qb,flags,0,caller);r=g_orig_MoveFileExW(qa,qb,flags);if(r&&trace_relevant_w(qb)){DWORD seq=trace_log_w("MoveFileExW(done)",qb,0,flags,0,caller);trace_snapshot(qb,"after_moveex",seq);trace_snapshot(g_ap_save_w,"main_after_moveex",seq);}return r;}
static BOOL WINAPI hk_ReplaceFileW(LPCWSTR a,LPCWSTR b,LPCWSTR backup,DWORD flags,LPVOID x,LPVOID y){const wchar_t*qa=redirect_path_w(a),*qb=redirect_path_w(b),*qc=redirect_path_w(backup);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);BOOL r;if(trace_relevant_w(qa)||trace_relevant_w(qb)||trace_relevant_w(qc))trace_log_w("ReplaceFileW",qa,qb,flags,0,caller);r=g_orig_ReplaceFileW(qa,qb,qc,flags,x,y);if(r&&trace_relevant_w(qa)){DWORD seq=trace_log_w("ReplaceFileW(done)",qa,qc,flags,0,caller);trace_snapshot(qa,"after_replace",seq);trace_snapshot(g_ap_save_w,"main_after_replace",seq);}return r;}
static int WINAPI hk_rename(LPCSTR a,LPCSTR b){const char*qa=redirect_path_a(a),*qb=redirect_path_a(b);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);int r;if(trace_relevant_a(qa)||trace_relevant_a(qb))trace_log_a("rename",qa,qb,0,0,caller);r=g_orig_rename(qa,qb);return r;}
static int WINAPI hk_remove(LPCSTR a){const char*qa=redirect_path_a(a);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);if(trace_relevant_a(qa))trace_log_a("remove",qa,0,0,0,caller);return g_orig_remove(qa);}

/* _wfopen detour. */
static void* WINAPI hk_wfopen(LPCWSTR p,LPCWSTR m){const wchar_t*q=redirect_path_w(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);void*f=g_orig_wfopen(q,m);if(trace_relevant_w(q)){trace_log_w("_wfopen",q,m,0,0,caller);trace_track_stream(f,q,trace_mode_writable_w(m));}return f;}

/* fopen detour. */
static void* WINAPI hk_fopen(LPCSTR p,LPCSTR m){const char*q=redirect_path_a(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);void*f=g_orig_fopen(q,m);wchar_t qw[900];if(trace_relevant_a(q)){trace_log_a("fopen",q,m,0,0,caller);trace_a_to_w(q,qw,900);trace_track_stream(f,qw,trace_mode_writable_a(m));}return f;}

/* fopen_s detour. */
static int WINAPI hk_fopen_s(void**f,LPCSTR p,LPCSTR m){const char*q=redirect_path_a(p);ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);int r=g_orig_fopen_s(f,q,m);wchar_t qw[900];if(trace_relevant_a(q)){trace_log_a("fopen_s",q,m,(DWORD)r,0,caller);trace_a_to_w(q,qw,900);if(f)trace_track_stream(*f,qw,trace_mode_writable_a(m));}return r;}

/* fclose detour: snapshots stdio writes after the stream is committed. */
static int WINAPI hk_fclose(void*f){TraceStream*t=trace_find_stream(f);wchar_t path[900];int snap=0;ULONG_PTR caller=(ULONG_PTR)__builtin_return_address(0);int r;if(t){lid_wcopy(path,900,t->path);snap=t->writable;t->used=0;}r=g_orig_fclose(f);if(snap){DWORD seq=trace_log_w("fclose(write)",path,0,(DWORD)r,0,caller);trace_snapshot(path,"stdio_closed_target",seq);trace_snapshot(g_ap_save_w,"main_after_fclose",seq);}return r;}

/* Compares two null-terminated import names. */
static int import_name_eq(const char*a,const char*b){return a&&b&&lid_memcmp(a,b,lid_strlen(b)+1)==0;}

/* Patches one named import of the main executable without calling the loader. */
static int patch_import_by_name(void*module,const char*wanted,void*detour,void**original){BYTE*b=(BYTE*)module,*nt,*opt,*desc;DWORD pe,imp_rva;WORD magic;size_t guard=0;if(!b||!wanted||!detour||!original||*(WORD*)b!=0x5A4D)return 0;pe=*(DWORD*)(b+0x3c);nt=b+pe;if(*(DWORD*)nt!=0x4550)return 0;opt=nt+24;magic=*(WORD*)opt;if(magic!=0x20B)return 0;imp_rva=*(DWORD*)(opt+120);if(!imp_rva)return 0;desc=b+imp_rva;while(guard++<512){DWORD oft=*(DWORD*)desc,name=*(DWORD*)(desc+12),ft=*(DWORD*)(desc+16);unsigned long long*names,*slots;size_t i;if(!oft&&!name&&!ft)break;if(oft&&ft){names=(unsigned long long*)(b+oft);slots=(unsigned long long*)(b+ft);for(i=0;i<4096&&names[i];i++){unsigned long long v=names[i];char*n;void**slot;DWORD old=0,ig=0;if(v&0x8000000000000000ULL)continue;n=(char*)(b+(DWORD)v+2);if(!import_name_eq(n,wanted))continue;slot=(void**)&slots[i];if(g_import_patch_count>=16)return 0;*original=*slot;if(!g_win.VirtualProtect(slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&old))return 0;*slot=detour;g_win.VirtualProtect(slot,sizeof(void*),old,&ig);g_import_patches[g_import_patch_count].slot=slot;g_import_patches[g_import_patch_count].original=*original;g_import_patch_count++;return 1;}}desc+=20;}return 0;}

/* Restores every import slot modified by the proxy. */
static void restore_import_patches(void){int i;DWORD old=0,ig=0;for(i=g_import_patch_count-1;i>=0;i--){if(g_import_patches[i].slot&&g_win.VirtualProtect(g_import_patches[i].slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&old)){*g_import_patches[i].slot=g_import_patches[i].original;g_win.VirtualProtect(g_import_patches[i].slot,sizeof(void*),old,&ig);}}g_import_patch_count=0;g_hooks_installed=0;}

/* Gets the Binaries\\Win64 directory without filesystem access. */
static void get_game_dir(wchar_t* out,size_t cap){size_t n,i;if(!g_win.GetModuleFileNameW){out[0]=0;return;}g_win.GetModuleFileNameW(0,out,(DWORD)cap);n=lid_wcslen(out);for(i=n;i>0;i--){if(out[i-1]==L'\\'||out[i-1]==L'/'){out[i-1]=0;return;}}out[0]=L'.';out[1]=0;}

/* Converts the computed ASCII-safe AP path for ANSI file APIs. */
static void wide_to_ascii_path(const wchar_t*w,char*a,size_t cap){size_t i=0;if(!cap)return;while(w&&w[i]&&i+1<cap){unsigned int c=w[i];a[i]=(char)(c<128?c:'?');i++;}a[i]=0;}

/* Extracts and validates -lidapslot=<hex key> from the launch command line. */
static int parse_slot_key_from_command_line(char*out,size_t cap){const wchar_t*cmd=g_win.GetCommandLineW?g_win.GetCommandLineW():0;const wchar_t tag[]=L"-lidapslot=";size_t i,j,k=0;if(!cmd||cap<2)return 0;for(i=0;cmd[i];i++){for(j=0;tag[j]&&cmd[i+j]&&wide_lower(cmd[i+j])==tag[j];j++);if(tag[j])continue;i+=j;while(cmd[i]&&cmd[i]!=L' '&&cmd[i]!=L'\t'&&cmd[i]!=L'"'&&k+1<cap){wchar_t c=cmd[i++];if(!((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f')||(c>=L'A'&&c<=L'F')))return -1;out[k++]=(char)c;}out[k]=0;if(k<8||k>40)return -1;return 1;}return 0;}

/* Validates the hexadecimal slot key used for AP slot-directory selection. */
static int valid_slot_key(const char*key){size_t i,n;if(!key)return 0;n=lid_strlen(key);if(n<8||n>40)return 0;for(i=0;i<n;i++){char c=key[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}return 1;}

/* Builds the dedicated AP save/temp/backup/master paths entirely in memory. */
static int build_overlay_paths(const char*key){wchar_t base[900],slot[900];size_t i,n;if(!valid_slot_key(key))return 0;get_game_dir(base,900);lid_wcopy(slot,900,base);lid_wappend(slot,900,L"\\Archipelago\\slots\\");n=lid_strlen(key);for(i=0;i<n&&lid_wcslen(slot)+2<900;i++){size_t z=lid_wcslen(slot);slot[z]=(wchar_t)key[i];slot[z+1]=0;}lid_wcopy(g_ap_save_w,900,slot);lid_wcopy(g_ap_tmp_w,900,slot);lid_wcopy(g_ap_bak_w,900,slot);lid_wcopy(g_ap_masters_w,900,slot);lid_wappend(g_ap_save_w,900,L"\\brggame.sav");lid_wappend(g_ap_tmp_w,900,L"\\brggame.tmp");lid_wappend(g_ap_bak_w,900,L"\\brggame.bak");lid_wappend(g_ap_masters_w,900,L"\\masters.db");wide_to_ascii_path(g_ap_save_w,g_ap_save_a,sizeof(g_ap_save_a));wide_to_ascii_path(g_ap_tmp_w,g_ap_tmp_a,sizeof(g_ap_tmp_a));wide_to_ascii_path(g_ap_bak_w,g_ap_bak_a,sizeof(g_ap_bak_a));wide_to_ascii_path(g_ap_masters_w,g_ap_masters_a,sizeof(g_ap_masters_a));return 1;}

/* Installs the file redirection hooks only after the game is already stable at the title menu. */
static int install_runtime_overlay_hooks(void){
    void*exe;
    if(g_hooks_installed)return 1;
    exe=lid_main_module();
    if(!exe)return 0;
    if(!patch_import_by_name(exe,"CreateFileW",(void*)hk_CreateFileW,(void**)&g_orig_CreateFileW))goto fail;
    if(!patch_import_by_name(exe,"CreateFileA",(void*)hk_CreateFileA,(void**)&g_orig_CreateFileA))goto fail;
    if(!patch_import_by_name(exe,"DeleteFileW",(void*)hk_DeleteFileW,(void**)&g_orig_DeleteFileW))goto fail;
    if(!patch_import_by_name(exe,"DeleteFileA",(void*)hk_DeleteFileA,(void**)&g_orig_DeleteFileA))goto fail;
    if(!patch_import_by_name(exe,"CopyFileW",(void*)hk_CopyFileW,(void**)&g_orig_CopyFileW))goto fail;
    if(!patch_import_by_name(exe,"MoveFileW",(void*)hk_MoveFileW,(void**)&g_orig_MoveFileW))goto fail;
    if(!patch_import_by_name(exe,"_wfopen",(void*)hk_wfopen,(void**)&g_orig_wfopen))goto fail;
    if(!patch_import_by_name(exe,"fopen",(void*)hk_fopen,(void**)&g_orig_fopen))goto fail;
    if(!patch_import_by_name(exe,"fopen_s",(void*)hk_fopen_s,(void**)&g_orig_fopen_s))goto fail;
    g_trace_writefile_hook=patch_import_by_name(exe,"WriteFile",(void*)hk_WriteFile,(void**)&g_orig_WriteFile);
    g_trace_closehandle_hook=patch_import_by_name(exe,"CloseHandle",(void*)hk_CloseHandle,(void**)&g_orig_CloseHandle);
    g_trace_fclose_hook=patch_import_by_name(exe,"fclose",(void*)hk_fclose,(void**)&g_orig_fclose);
    patch_import_by_name(exe,"MoveFileExW",(void*)hk_MoveFileExW,(void**)&g_orig_MoveFileExW);
    patch_import_by_name(exe,"ReplaceFileW",(void*)hk_ReplaceFileW,(void**)&g_orig_ReplaceFileW);
    patch_import_by_name(exe,"rename",(void*)hk_rename,(void**)&g_orig_rename);
    patch_import_by_name(exe,"remove",(void*)hk_remove,(void**)&g_orig_remove);
    g_hooks_installed=1;
    return 1;
fail:
    restore_import_patches();
    return 0;
}

/* Activates one prepared AP slot in the already-running process. No hook is installed before this call. */
__declspec(dllexport) int WINAPI LID_ActivateOverlayA(const char*key){
    if(g_overlay_active)return 1;
    if(!build_overlay_paths(key)||!install_runtime_overlay_hooks()){g_overlay_status=-1;return 0;}
    g_startup_save_guard=1;
    g_overlay_active=1;
    g_overlay_status=1;
    trace_start();
    trace_log_line("V0.69 GUARD: armed for the first pre-load atomic brggame save transaction.");
    return 1;
}

/* Loads the system dinput8 implementation. */
static int load_real(void){wchar_t p[512];UINT n;if(g_real)return g_create!=0;if(!lid_init_kernel_api())return 0;n=g_win.GetSystemDirectoryW(p,450);if(!n||n>=450)return 0;lid_wappend(p,512,L"\\dinput8.dll");g_real=g_win.LoadLibraryW(p);if(!g_real)return 0;g_create=(DirectInput8CreateFn)lid_get_export(g_real,"DirectInput8Create");return g_create!=0;}

/* Loads the main Archipelago mod outside the proxy DllMain loader lock. */
static DWORD WINAPI load_mod(LPVOID unused){wchar_t p[700];(void)unused;if(!g_win.LoadLibraryW)return 0;get_game_dir(p,700);lid_wappend(p,700,L"\\Archipelago\\LetItDieArchipelago.dll");g_win.LoadLibraryW(p);return 0;}

/* Forwards DirectInput8Create to the system DLL. */
__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE a,DWORD b,const void*c,void**d,LPUNKNOWN e){if(!load_real())return E_FAIL;return g_create(a,b,c,d,e);}

/* V0.52 boot path: exact stable V0.40/V0.48 behavior. Do not patch any game file API before entry point. */
BOOL WINAPI DllMain(HMODULE m,DWORD reason,LPVOID reserved){(void)m;(void)reserved;if(reason==DLL_PROCESS_ATTACH){DWORD tid=0;HANDLE t;if(!lid_init_kernel_api())return TRUE;g_overlay_status=0;g_overlay_active=0;g_hooks_installed=0;g_startup_save_guard=0;t=g_win.CreateThread(0,0,load_mod,0,0,&tid);if(t)g_win.CloseHandle(t);}else if(reason==DLL_PROCESS_DETACH){if(g_win.VirtualProtect)restore_import_patches();}return TRUE;}
