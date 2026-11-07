#ifndef LID_WINLITE_H
#define LID_WINLITE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef __SIZE_TYPE__ size_t;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef long LONG;
typedef long long LONG_PTR;
typedef unsigned long long ULONG_PTR;
typedef unsigned long long UINT_PTR;
typedef unsigned long long WPARAM;
typedef long long LPARAM;
typedef long long LRESULT;
typedef int BOOL;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HMENU;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HBRUSH;
typedef void* HGDIOBJ;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef unsigned short wchar_t;
typedef wchar_t WCHAR;
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef unsigned long long SIZE_T;
typedef long HRESULT;
typedef void* LPUNKNOWN;

typedef LRESULT (__attribute__((ms_abi)) *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef DWORD (__attribute__((ms_abi)) *LPTHREAD_START_ROUTINE)(LPVOID);
typedef BOOL (__attribute__((ms_abi)) *WNDENUMPROC)(HWND, LPARAM);

#define WINAPI __attribute__((ms_abi))
#define CALLBACK __attribute__((ms_abi))
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define MAX_PATH 260

#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1

#define PAGE_NOACCESS 0x01u
#define PAGE_GUARD 0x100u
#define PAGE_EXECUTE_READWRITE 0x40u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define MEM_RELEASE 0x8000u

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_APPEND_DATA 0x00000004u
#define FILE_SHARE_READ 0x00000001u
#define FILE_SHARE_WRITE 0x00000002u
#define CREATE_ALWAYS 2u
#define OPEN_EXISTING 3u
#define OPEN_ALWAYS 4u
#define FILE_ATTRIBUTE_NORMAL 0x00000080u
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define INFINITE 0xffffffffu

#define WS_OVERLAPPED 0x00000000L
#define WS_CAPTION 0x00C00000L
#define WS_SYSMENU 0x00080000L
#define WS_CHILD 0x40000000L
#define WS_VISIBLE 0x10000000L
#define WS_TABSTOP 0x00010000L
#define ES_AUTOHSCROLL 0x0080L
#define ES_PASSWORD 0x0020L
#define BS_DEFPUSHBUTTON 0x00000001L
#define WS_EX_CLIENTEDGE 0x00000200L
#define WS_EX_DLGMODALFRAME 0x00000001L
#define WS_EX_TOPMOST 0x00000008L

#define SW_SHOW 5
#define SWP_NOSIZE 0x0001u
#define SWP_SHOWWINDOW 0x0040u
#define HWND_TOPMOST ((HWND)(LONG_PTR)-1)
#define CW_USEDEFAULT ((int)0x80000000u)
#define COLOR_WINDOW 5

#define WM_NCCREATE 0x0081u
#define WM_CREATE 0x0001u
#define WM_COMMAND 0x0111u
#define WM_CLOSE 0x0010u
#define WM_DESTROY 0x0002u

#define GW_OWNER 4u
#define SPI_GETWORKAREA 0x0030u
#define ERROR_CLASS_ALREADY_EXISTS 1410u

#define CP_UTF8 65001u
#define E_FAIL ((HRESULT)0x80004005L)

#define LOWORD(l) ((WORD)((ULONG_PTR)(l) & 0xffffu))

#pragma pack(push, 8)
typedef struct _RECT { LONG left, top, right, bottom; } RECT;
typedef struct _POINT { LONG x, y; } POINT;
typedef struct _MSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
    DWORD lPrivate;
} MSG;

typedef struct _MEMORY_BASIC_INFORMATION {
    LPVOID BaseAddress;
    LPVOID AllocationBase;
    DWORD AllocationProtect;
    WORD PartitionId;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
} MEMORY_BASIC_INFORMATION;

typedef struct _WNDCLASSEXW {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
    HICON hIconSm;
} WNDCLASSEXW;


typedef struct _STARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    BYTE* lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION;

typedef struct _CREATESTRUCTW {
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy;
    int cx;
    int y;
    int x;
    LONG style;
    LPCWSTR lpszName;
    LPCWSTR lpszClass;
    DWORD dwExStyle;
} CREATESTRUCTW;
#pragma pack(pop)

/* Dynamically resolved Win32 API set. */
typedef struct WinApi {
    HMODULE (WINAPI *LoadLibraryW)(LPCWSTR);
    BOOL (WINAPI *FreeLibrary)(HMODULE);
    DWORD (WINAPI *GetModuleFileNameW)(HMODULE, LPWSTR, DWORD);
    LPWSTR (WINAPI *GetCommandLineW)(void);
    UINT (WINAPI *GetSystemDirectoryW)(LPWSTR, UINT);
    HANDLE (WINAPI *CreateThread)(LPVOID, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*);
    BOOL (WINAPI *CloseHandle)(HANDLE);
    LPVOID (WINAPI *VirtualAlloc)(LPVOID, SIZE_T, DWORD, DWORD);
    BOOL (WINAPI *VirtualFree)(LPVOID, SIZE_T, DWORD);
    BOOL (WINAPI *VirtualProtect)(LPVOID, SIZE_T, DWORD, DWORD*);
    SIZE_T (WINAPI *VirtualQuery)(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T);
    BOOL (WINAPI *FlushInstructionCache)(HANDLE, LPCVOID, SIZE_T);
    HANDLE (WINAPI *GetCurrentProcess)(void);
    DWORD (WINAPI *GetCurrentProcessId)(void);
    BOOL (WINAPI *CreateDirectoryW)(LPCWSTR, LPVOID);
    HANDLE (WINAPI *CreateFileW)(LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
    BOOL (WINAPI *ReadFile)(HANDLE, LPVOID, DWORD, DWORD*, LPVOID);
    BOOL (WINAPI *WriteFile)(HANDLE, LPCVOID, DWORD, DWORD*, LPVOID);
    BOOL (WINAPI *DeleteFileW)(LPCWSTR);
    BOOL (WINAPI *CreateProcessW)(LPCWSTR,LPWSTR,LPVOID,LPVOID,BOOL,DWORD,LPVOID,LPCWSTR,STARTUPINFOW*,PROCESS_INFORMATION*);
    void (WINAPI *Sleep)(DWORD);
    void (WINAPI *ExitProcess)(UINT);
    DWORD (WINAPI *GetLastError)(void);

    /* user32 */
    HWND (WINAPI *CreateWindowExW)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
    LRESULT (WINAPI *DefWindowProcW)(HWND,UINT,WPARAM,LPARAM);
    unsigned short (WINAPI *RegisterClassExW)(const WNDCLASSEXW*);
    BOOL (WINAPI *DestroyWindow)(HWND);
    BOOL (WINAPI *ShowWindow)(HWND,int);
    BOOL (WINAPI *UpdateWindow)(HWND);
    BOOL (WINAPI *EnableWindow)(HWND,BOOL);
    BOOL (WINAPI *SetForegroundWindow)(HWND);
    BOOL (WINAPI *SetWindowTextW)(HWND,LPCWSTR);
    int (WINAPI *MessageBoxW)(HWND,LPCWSTR,LPCWSTR,UINT);
    int (WINAPI *GetWindowTextLengthW)(HWND);
    int (WINAPI *GetWindowTextW)(HWND,LPWSTR,int);
    HWND (WINAPI *SetFocus)(HWND);
    BOOL (WINAPI *GetWindowRect)(HWND,RECT*);
    BOOL (WINAPI *SetWindowPos)(HWND,HWND,int,int,int,int,UINT);
    BOOL (WINAPI *SystemParametersInfoW)(UINT,UINT,LPVOID,UINT);
    int (WINAPI *GetMessageW)(MSG*,HWND,UINT,UINT);
    BOOL (WINAPI *TranslateMessage)(const MSG*);
    LRESULT (WINAPI *DispatchMessageW)(const MSG*);
    BOOL (WINAPI *IsDialogMessageW)(HWND,MSG*);
    BOOL (WINAPI *EnumWindows)(WNDENUMPROC,LPARAM);
    DWORD (WINAPI *GetWindowThreadProcessId)(HWND,DWORD*);
    BOOL (WINAPI *IsWindowVisible)(HWND);
    HWND (WINAPI *GetWindow)(HWND,UINT);
} WinApi;

extern WinApi g_win;

void* lid_main_module(void);
void* lid_find_module_ascii(const char* module_name);
void* lid_get_export(void* module, const char* name);
int lid_init_kernel_api(void);
int lid_init_user_api(void);

void* lid_memcpy(void* d, const void* s, size_t n);
void* lid_memset(void* d, int c, size_t n);
int lid_memcmp(const void* a, const void* b, size_t n);
size_t lid_strlen(const char* s);
size_t lid_wcslen(const wchar_t* s);
int lid_wstrcmp(const wchar_t* a, const wchar_t* b);
void lid_wcopy(wchar_t* d, size_t cap, const wchar_t* s);
void lid_wappend(wchar_t* d, size_t cap, const wchar_t* s);

/* Compiler may emit these symbols even with -ffreestanding. */
void* memcpy(void* d, const void* s, size_t n);
void* memset(void* d, int c, size_t n);
int memcmp(const void* a, const void* b, size_t n);

#ifdef __cplusplus
}
#endif
#endif
