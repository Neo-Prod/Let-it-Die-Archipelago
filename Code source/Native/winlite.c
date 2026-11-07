#include "winlite.h"

WinApi g_win;

void* lid_memcpy(void* d, const void* s, size_t n) {
    BYTE* dd=(BYTE*)d; const BYTE* ss=(const BYTE*)s; size_t i;
    for(i=0;i<n;i++) dd[i]=ss[i]; return d;
}
void* lid_memset(void* d, int c, size_t n) {
    BYTE* dd=(BYTE*)d; size_t i; for(i=0;i<n;i++) dd[i]=(BYTE)c; return d;
}
int lid_memcmp(const void* a,const void* b,size_t n){const BYTE*aa=(const BYTE*)a;const BYTE*bb=(const BYTE*)b;size_t i;for(i=0;i<n;i++){if(aa[i]!=bb[i])return aa[i]<bb[i]?-1:1;}return 0;}
void* memcpy(void*d,const void*s,size_t n){return lid_memcpy(d,s,n);} void* memset(void*d,int c,size_t n){return lid_memset(d,c,n);} int memcmp(const void*a,const void*b,size_t n){return lid_memcmp(a,b,n);}
size_t lid_strlen(const char*s){size_t n=0;if(!s)return 0;while(s[n])n++;return n;}
size_t lid_wcslen(const wchar_t*s){size_t n=0;if(!s)return 0;while(s[n])n++;return n;}
int lid_wstrcmp(const wchar_t*a,const wchar_t*b){size_t i=0;if(!a||!b)return a==b?0:(a?1:-1);while(a[i]&&b[i]&&a[i]==b[i])i++;return (int)a[i]-(int)b[i];}
void lid_wcopy(wchar_t*d,size_t cap,const wchar_t*s){size_t i=0;if(!cap)return; if(s){while(i+1<cap&&s[i]){d[i]=s[i];i++;}}d[i]=0;}
void lid_wappend(wchar_t*d,size_t cap,const wchar_t*s){size_t n=lid_wcslen(d),i=0;if(n>=cap)return;while(s&&s[i]&&n+1<cap)d[n++]=s[i++];d[n]=0;}

static void* get_peb(void){void* p; __asm__("movq %%gs:0x60,%0":"=r"(p)); return p;}
void* lid_main_module(void){BYTE* peb=(BYTE*)get_peb(); return peb?*(void**)(peb+0x10):0;}

static char lowera(char c){if(c>='A'&&c<='Z')c=(char)(c+32);return c;}
static wchar_t lowerw(wchar_t c){if(c>=L'A'&&c<=L'Z')c=(wchar_t)(c+32);return c;}
static int module_name_eq(const wchar_t* w, unsigned short byte_len, const char* a){size_t wn=(size_t)byte_len/2, an=lid_strlen(a),i;if(wn!=an)return 0;for(i=0;i<wn;i++){if((char)lowerw(w[i])!=lowera(a[i]))return 0;}return 1;}

void* lid_find_module_ascii(const char* module_name){
    BYTE* peb=(BYTE*)get_peb(); BYTE* ldr; void** head; void** cur;
    if(!peb)return 0; ldr=*(BYTE**)(peb+0x18); if(!ldr)return 0;
    head=(void**)(ldr+0x10); cur=(void**)(*head);
    while(cur && cur!=head){
        BYTE* e=(BYTE*)cur; void* dll=*(void**)(e+0x30); USHORT len=*(USHORT*)(e+0x58); wchar_t* buf=*(wchar_t**)(e+0x60);
        if(buf && module_name_eq(buf,len,module_name))return dll;
        cur=(void**)(*cur);
    }
    return 0;
}

static DWORD rd32(const BYTE*p){return *(const DWORD*)p;} static WORD rd16(const BYTE*p){return *(const WORD*)p;}

void* lid_get_export(void* module,const char* name){
    BYTE* b=(BYTE*)module; DWORD peoff, exprva, expsz; BYTE* nt; BYTE* ex; DWORD nname, funcs, names, ords; DWORD i;
    if(!b||rd16(b)!=0x5A4D)return 0; peoff=rd32(b+0x3c); nt=b+peoff; if(rd32(nt)!=0x00004550)return 0;
    exprva=rd32(nt+0x88); expsz=rd32(nt+0x8c); if(!exprva)return 0; ex=b+exprva;
    nname=rd32(ex+0x18); funcs=rd32(ex+0x1c); names=rd32(ex+0x20); ords=rd32(ex+0x24);
    for(i=0;i<nname;i++){
        const char* en=(const char*)(b+rd32(b+names+i*4));
        size_t k=0; while(en[k]&&name[k]&&en[k]==name[k])k++; if(en[k]||name[k])continue;
        WORD ord=rd16(b+ords+i*2); DWORD rva=rd32(b+funcs+(DWORD)ord*4);
        if(rva>=exprva && rva<exprva+expsz){
            const char* fwd=(const char*)(b+rva); char mod[80]; char sym[128]; size_t m=0,s=0; void* fm;
            while(fwd[m]&&fwd[m]!='.'&&m+5<sizeof(mod)){mod[m]=fwd[m];m++;} if(fwd[m]!='.')return 0; mod[m]=0; lid_memcpy(mod+m,".dll",5); m++;
            while(fwd[m+s]&&s+1<sizeof(sym)){sym[s]=fwd[m+s];s++;} sym[s]=0;
            fm=lid_find_module_ascii(mod); if(!fm)return 0; if(sym[0]=='#')return 0; return lid_get_export(fm,sym);
        }
        return b+rva;
    }
    return 0;
}

#define RESOLVE(mod, field, type) do { g_win.field=(type)lid_get_export(mod,#field); if(!g_win.field)return 0; } while(0)
int lid_init_kernel_api(void){
    void* k=lid_find_module_ascii("kernel32.dll"); if(!k)return 0;
    RESOLVE(k,LoadLibraryW,HMODULE(WINAPI*)(LPCWSTR));
    RESOLVE(k,FreeLibrary,BOOL(WINAPI*)(HMODULE));
    RESOLVE(k,GetModuleFileNameW,DWORD(WINAPI*)(HMODULE,LPWSTR,DWORD));
    RESOLVE(k,GetCommandLineW,LPWSTR(WINAPI*)(void));
    RESOLVE(k,GetSystemDirectoryW,UINT(WINAPI*)(LPWSTR,UINT));
    RESOLVE(k,CreateThread,HANDLE(WINAPI*)(LPVOID,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,DWORD*));
    RESOLVE(k,CloseHandle,BOOL(WINAPI*)(HANDLE));
    RESOLVE(k,VirtualAlloc,LPVOID(WINAPI*)(LPVOID,SIZE_T,DWORD,DWORD));
    RESOLVE(k,VirtualFree,BOOL(WINAPI*)(LPVOID,SIZE_T,DWORD));
    RESOLVE(k,VirtualProtect,BOOL(WINAPI*)(LPVOID,SIZE_T,DWORD,DWORD*));
    RESOLVE(k,VirtualQuery,SIZE_T(WINAPI*)(LPCVOID,MEMORY_BASIC_INFORMATION*,SIZE_T));
    RESOLVE(k,FlushInstructionCache,BOOL(WINAPI*)(HANDLE,LPCVOID,SIZE_T));
    RESOLVE(k,GetCurrentProcess,HANDLE(WINAPI*)(void));
    RESOLVE(k,GetCurrentProcessId,DWORD(WINAPI*)(void));
    RESOLVE(k,CreateDirectoryW,BOOL(WINAPI*)(LPCWSTR,LPVOID));
    RESOLVE(k,CreateFileW,HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE));
    RESOLVE(k,ReadFile,BOOL(WINAPI*)(HANDLE,LPVOID,DWORD,DWORD*,LPVOID));
    RESOLVE(k,WriteFile,BOOL(WINAPI*)(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID));
    RESOLVE(k,DeleteFileW,BOOL(WINAPI*)(LPCWSTR));
    RESOLVE(k,CreateProcessW,BOOL(WINAPI*)(LPCWSTR,LPWSTR,LPVOID,LPVOID,BOOL,DWORD,LPVOID,LPCWSTR,STARTUPINFOW*,PROCESS_INFORMATION*));
    RESOLVE(k,Sleep,void(WINAPI*)(DWORD));
    RESOLVE(k,ExitProcess,void(WINAPI*)(UINT));
    RESOLVE(k,GetLastError,DWORD(WINAPI*)(void));
    return 1;
}
int lid_init_user_api(void){
    void* u=lid_find_module_ascii("user32.dll"); if(!u){ if(!g_win.LoadLibraryW)return 0; u=g_win.LoadLibraryW(L"user32.dll"); } if(!u)return 0;
    RESOLVE(u,CreateWindowExW,HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID));
    RESOLVE(u,DefWindowProcW,LRESULT(WINAPI*)(HWND,UINT,WPARAM,LPARAM));
    RESOLVE(u,RegisterClassExW,unsigned short(WINAPI*)(const WNDCLASSEXW*));
    RESOLVE(u,DestroyWindow,BOOL(WINAPI*)(HWND));
    RESOLVE(u,ShowWindow,BOOL(WINAPI*)(HWND,int));
    RESOLVE(u,UpdateWindow,BOOL(WINAPI*)(HWND));
    RESOLVE(u,EnableWindow,BOOL(WINAPI*)(HWND,BOOL));
    RESOLVE(u,SetForegroundWindow,BOOL(WINAPI*)(HWND));
    RESOLVE(u,SetWindowTextW,BOOL(WINAPI*)(HWND,LPCWSTR));
    RESOLVE(u,MessageBoxW,int(WINAPI*)(HWND,LPCWSTR,LPCWSTR,UINT));
    RESOLVE(u,GetWindowTextLengthW,int(WINAPI*)(HWND));
    RESOLVE(u,GetWindowTextW,int(WINAPI*)(HWND,LPWSTR,int));
    RESOLVE(u,SetFocus,HWND(WINAPI*)(HWND));
    RESOLVE(u,GetWindowRect,BOOL(WINAPI*)(HWND,RECT*));
    RESOLVE(u,SetWindowPos,BOOL(WINAPI*)(HWND,HWND,int,int,int,int,UINT));
    RESOLVE(u,SystemParametersInfoW,BOOL(WINAPI*)(UINT,UINT,LPVOID,UINT));
    RESOLVE(u,GetMessageW,int(WINAPI*)(MSG*,HWND,UINT,UINT));
    RESOLVE(u,TranslateMessage,BOOL(WINAPI*)(const MSG*));
    RESOLVE(u,DispatchMessageW,LRESULT(WINAPI*)(const MSG*));
    RESOLVE(u,IsDialogMessageW,BOOL(WINAPI*)(HWND,MSG*));
    RESOLVE(u,EnumWindows,BOOL(WINAPI*)(WNDENUMPROC,LPARAM));
    RESOLVE(u,GetWindowThreadProcessId,DWORD(WINAPI*)(HWND,DWORD*));
    RESOLVE(u,IsWindowVisible,BOOL(WINAPI*)(HWND));
    RESOLVE(u,GetWindow,HWND(WINAPI*)(HWND,UINT));
    return 1;
}

/* Material binding/source-plan freestanding string dependencies. */
int strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return (int)(unsigned char)*a-(int)(unsigned char)*b;}
char*strchr(const char*s,int c){unsigned char wanted=(unsigned char)c;for(;;s++){if((unsigned char)*s==wanted)return (char*)s;if(!*s)return 0;}}
char*strcpy(char*d,const char*s){char*out=d;while((*d++=*s++)!=0){}return out;}
