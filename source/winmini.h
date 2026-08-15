#ifndef WINMINI_H
#define WINMINI_H

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned short WCHAR;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef long LONG;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef unsigned long long UINT64;
typedef unsigned long long ULONG_PTR;
typedef long long LONG_PTR;
typedef ULONG_PTR UINT_PTR;
typedef LONG_PTR INT_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef void* HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HINSTANCE HMODULE;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HMENU;
typedef HANDLE HFONT;
typedef HANDLE HGDIOBJ;
typedef HANDLE HDC;
typedef const WCHAR* LPCWSTR;
typedef WCHAR* LPWSTR;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef int BOOL;
typedef unsigned short ATOM;
typedef DWORD* LPDWORD;
typedef LONG_PTR (*FARPROC)(void);

#ifndef NULL
#define NULL ((void*)0)
#endif
#define TRUE 1
#define FALSE 0
#define WINAPI __attribute__((ms_abi))
#define CALLBACK __attribute__((ms_abi))
#define WINAPI_PTR WINAPI
#define __declspec_dllimport __declspec(dllimport)

#define LOWORD(l) ((WORD)((ULONG_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((ULONG_PTR)(l) >> 16))

#define WS_OVERLAPPED       0x00000000L
#define WS_CAPTION          0x00C00000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_VISIBLE          0x10000000L
#define WS_CHILD            0x40000000L
#define WS_TABSTOP          0x00010000L
#define WS_BORDER           0x00800000L
#define WS_VSCROLL          0x00200000L
#define WS_GROUP            0x00020000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX)

#define WS_EX_CLIENTEDGE    0x00000200L
#define WS_EX_CONTROLPARENT 0x00010000L

#define BS_PUSHBUTTON       0x00000000L
#define BS_DEFPUSHBUTTON    0x00000001L
#define BS_AUTORADIOBUTTON  0x00000009L
#define BS_GROUPBOX         0x00000007L
#define BS_AUTOCHECKBOX     0x00000003L
#define ES_LEFT             0x0000L
#define ES_AUTOHSCROLL      0x0080L
#define ES_MULTILINE        0x0004L
#define ES_AUTOVSCROLL      0x0040L
#define ES_READONLY         0x0800L
#define CBS_DROPDOWNLIST    0x0003L
#define SS_LEFT             0x00000000L
#define SS_CENTER           0x00000001L

#define SW_SHOWDEFAULT      10
#define CW_USEDEFAULT       ((int)0x80000000)

#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_SIZE             0x0005
#define WM_COMMAND          0x0111
#define WM_SETFONT          0x0030
#define WM_CLOSE            0x0010
#define WM_APP              0x8000
#define BM_GETCHECK         0x00F0
#define BM_SETCHECK         0x00F1
#define BST_UNCHECKED       0x0000
#define BST_CHECKED         0x0001
#define CB_ADDSTRING        0x0143
#define CB_GETCURSEL        0x0147
#define CB_SETCURSEL        0x014E
#define EM_SETCUEBANNER     0x1501
#define EN_CHANGE           0x0300
#define BN_CLICKED          0

#define COLOR_WINDOW        5
#define COLOR_BTNFACE       15
#define DEFAULT_GUI_FONT    17
#define IDC_ARROW           ((LPCWSTR)32512)
#define IDI_APPLICATION     ((LPCWSTR)32512)

#define MB_OK               0x00000000L
#define MB_ICONERROR        0x00000010L
#define MB_ICONINFORMATION  0x00000040L

#define HEAP_ZERO_MEMORY    0x00000008
#define HANDLE_FLAG_INHERIT 0x00000001
#define STARTF_USESTDHANDLES 0x00000100
#define CREATE_NO_WINDOW    0x08000000
#define INFINITE            0xFFFFFFFF
#define WAIT_OBJECT_0       0x00000000L

#define GWLP_USERDATA       (-21)

#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_ERROR_HANDLE    ((DWORD)-12)

#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))

#define ID_INPUT_N      1001
#define ID_INPUT_P      1002
#define ID_VALUE        1003
#define ID_BOOTSTRAP    1004
#define ID_GENERATE     1005
#define ID_CANCEL       1006
#define ID_EXPLORE      1007
#define ID_RESULT_P     1010
#define ID_RESULT_GAP   1011
#define ID_RESULT_NEXT  1012
#define ID_RESULT_GATE  1013
#define ID_RESULT_EXACT 1014
#define ID_RESULT_META  1015
#define ID_LOG          1020

#define WM_APP_DONE     (WM_APP + 1)
#define WM_APP_PROGRESS (WM_APP + 2)

typedef struct tagPOINT { LONG x; LONG y; } POINT;
typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT;
typedef struct tagSIZE { LONG cx; LONG cy; } SIZE;
typedef struct tagMSG {
    HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; DWORD lPrivate;
} MSG;
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagWNDCLASSEXW {
    UINT cbSize; UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCWSTR lpszMenuName; LPCWSTR lpszClassName; HICON hIconSm;
} WNDCLASSEXW;
typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;
typedef struct _STARTUPINFOW {
    DWORD cb; LPWSTR lpReserved; LPWSTR lpDesktop; LPWSTR lpTitle;
    DWORD dwX; DWORD dwY; DWORD dwXSize; DWORD dwYSize;
    DWORD dwXCountChars; DWORD dwYCountChars; DWORD dwFillAttribute;
    DWORD dwFlags; WORD wShowWindow; WORD cbReserved2; BYTE* lpReserved2;
    HANDLE hStdInput; HANDLE hStdOutput; HANDLE hStdError;
} STARTUPINFOW;
typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess; HANDLE hThread; DWORD dwProcessId; DWORD dwThreadId;
} PROCESS_INFORMATION;

typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);

__declspec_dllimport HINSTANCE WINAPI GetModuleHandleW(LPCWSTR);
__declspec_dllimport HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec_dllimport FARPROC WINAPI GetProcAddress(HMODULE,LPCSTR);
__declspec_dllimport BOOL WINAPI FreeLibrary(HMODULE);
__declspec_dllimport void WINAPI ExitProcess(UINT);
__declspec_dllimport HANDLE WINAPI GetProcessHeap(void);
__declspec_dllimport LPVOID WINAPI HeapAlloc(HANDLE,DWORD,UINT_PTR);
__declspec_dllimport BOOL WINAPI HeapFree(HANDLE,DWORD,LPVOID);
__declspec_dllimport HANDLE WINAPI CreateThread(LPVOID,UINT_PTR,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
__declspec_dllimport BOOL WINAPI CloseHandle(HANDLE);
__declspec_dllimport ULONGLONG WINAPI GetTickCount64(void);
__declspec_dllimport BOOL WINAPI CreatePipe(HANDLE*,HANDLE*,SECURITY_ATTRIBUTES*,DWORD);
__declspec_dllimport BOOL WINAPI SetHandleInformation(HANDLE,DWORD,DWORD);
__declspec_dllimport BOOL WINAPI CreateProcessW(LPCWSTR,LPWSTR,LPVOID,LPVOID,BOOL,DWORD,LPVOID,LPCWSTR,STARTUPINFOW*,PROCESS_INFORMATION*);
__declspec_dllimport BOOL WINAPI ReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPVOID);
__declspec_dllimport DWORD WINAPI WaitForSingleObject(HANDLE,DWORD);
__declspec_dllimport HANDLE WINAPI GetStdHandle(DWORD);
__declspec_dllimport DWORD WINAPI GetLastError(void);

__declspec_dllimport ATOM WINAPI RegisterClassExW(const WNDCLASSEXW*);
__declspec_dllimport HWND WINAPI CreateWindowExW(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
__declspec_dllimport LRESULT WINAPI DefWindowProcW(HWND,UINT,WPARAM,LPARAM);
__declspec_dllimport BOOL WINAPI ShowWindow(HWND,int);
__declspec_dllimport BOOL WINAPI UpdateWindow(HWND);
__declspec_dllimport BOOL WINAPI GetMessageW(MSG*,HWND,UINT,UINT);
__declspec_dllimport BOOL WINAPI TranslateMessage(const MSG*);
__declspec_dllimport LRESULT WINAPI DispatchMessageW(const MSG*);
__declspec_dllimport void WINAPI PostQuitMessage(int);
__declspec_dllimport LRESULT WINAPI SendMessageW(HWND,UINT,WPARAM,LPARAM);
__declspec_dllimport BOOL WINAPI PostMessageW(HWND,UINT,WPARAM,LPARAM);
__declspec_dllimport BOOL WINAPI SetWindowTextW(HWND,LPCWSTR);
__declspec_dllimport int WINAPI GetWindowTextW(HWND,LPWSTR,int);
__declspec_dllimport int WINAPI GetWindowTextLengthW(HWND);
__declspec_dllimport BOOL WINAPI EnableWindow(HWND,BOOL);
__declspec_dllimport int WINAPI MessageBoxW(HWND,LPCWSTR,LPCWSTR,UINT);
__declspec_dllimport BOOL WINAPI MoveWindow(HWND,int,int,int,int,BOOL);
__declspec_dllimport BOOL WINAPI GetClientRect(HWND,RECT*);
__declspec_dllimport HWND WINAPI SetFocus(HWND);
__declspec_dllimport HCURSOR WINAPI LoadCursorW(HINSTANCE,LPCWSTR);
__declspec_dllimport HICON WINAPI LoadIconW(HINSTANCE,LPCWSTR);
__declspec_dllimport HDC WINAPI GetDC(HWND);
__declspec_dllimport int WINAPI ReleaseDC(HWND,HDC);

__declspec_dllimport HGDIOBJ WINAPI GetStockObject(int);
__declspec_dllimport HGDIOBJ WINAPI SelectObject(HDC,HGDIOBJ);
__declspec_dllimport BOOL WINAPI GetTextExtentPoint32W(HDC,LPCWSTR,int,SIZE*);

#endif
