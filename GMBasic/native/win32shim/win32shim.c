/* ============================================================================
 * win32shim.c — shim portabile (macOS/Linux) delle API Win32 piu' usate dagli
 * Expert Advisor MQL4/MQL5 importati via `#import "*.dll"`.
 *
 * Scopo: una dylib (.dylib/.so) che esporta gli STESSI NOMI-SIMBOLO dei DLL
 * Windows, cosi' che `DECLARE FUNCTION Fn LIB "wininet"` (emesso dal transpiler
 * MQL, o scritto a mano in GMBasic) si risolva senza i DLL di Windows.
 *
 * Convenzione ABI FFI GMBasic: interi/string -> int64_t o const char* (classe
 * intera); DOUBLE -> double; array -> puntatore. Le firme qui sotto usano
 * int64_t, const char*, void* per restare compatibili col marshalling FFI.
 *
 * Build: vedi build.sh (produce libwin32shim.dylib + symlink per libreria).
 * ============================================================================ */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

#define EXPORT __attribute__((visibility("default")))

/* ultimo errore per-thread (GetLastError/SetLastError) */
static pthread_key_t g_tls_key;
static pthread_once_t g_tls_once = PTHREAD_ONCE_INIT;
static void tls_make(void){ pthread_key_create(&g_tls_key, NULL); }
static int64_t* tls_slot(void){ pthread_once(&g_tls_once, tls_make); int64_t* p = pthread_getspecific(g_tls_key); if(!p){ p = calloc(1,sizeof(int64_t)); pthread_setspecific(g_tls_key, p);} return p; }

static uint64_t monotime_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000ull + (uint64_t)ts.tv_nsec/1000000ull;
}

/* ============================== wininet ============================== */
EXPORT int64_t InternetOpenA(const char* agent, int64_t type, const char* proxy, const char* bypass, int64_t flags){ (void)agent;(void)type;(void)proxy;(void)bypass;(void)flags; return 1; }
EXPORT int64_t InternetOpenW(const char* agent, int64_t type, const char* proxy, const char* bypass, int64_t flags){ return InternetOpenA(agent,type,proxy,bypass,flags); }
EXPORT int64_t InternetOpenUrlA(int64_t h, const char* url, const char* headers, int64_t hlen, int64_t flags, int64_t ctx){ (void)h;(void)url;(void)headers;(void)hlen;(void)flags;(void)ctx; return 1; }
EXPORT int64_t InternetOpenUrlW(int64_t h, const char* url, const char* headers, int64_t hlen, int64_t flags, int64_t ctx){ return InternetOpenUrlA(h,url,headers,hlen,flags,ctx); }
EXPORT int64_t InternetConnectA(int64_t h, const char* server, int64_t port, const char* user, const char* pass, int64_t service, int64_t flags, int64_t ctx){ (void)h;(void)server;(void)port;(void)user;(void)pass;(void)service;(void)flags;(void)ctx; return 1; }
EXPORT int64_t InternetConnectW(int64_t h, const char* server, int64_t port, const char* user, const char* pass, int64_t service, int64_t flags, int64_t ctx){ return InternetConnectA(h,server,port,user,pass,service,flags,ctx); }
EXPORT int64_t InternetCloseHandle(int64_t h){ (void)h; return 1; }
EXPORT int64_t InternetAttemptConnect(int64_t reserved){ (void)reserved; return 0; }
EXPORT int64_t InternetCanonicalizeUrlA(const char* url, char* buf, int64_t* len, int64_t flags){ (void)flags; if(buf && len && *len>0){ size_t n=strlen(url?url:""); size_t c=(size_t)*len; if(n>=c)n=c-1; memcpy(buf,url?url:"",n); buf[n]=0; *len=(int64_t)n; } return 1; }
EXPORT int64_t HttpOpenRequestA(int64_t h, const char* verb, const char* obj, const char* ver, const char* referer, const char* accept, int64_t flags, int64_t ctx){ (void)h;(void)verb;(void)obj;(void)ver;(void)referer;(void)accept;(void)flags;(void)ctx; return 1; }
EXPORT int64_t HttpAddRequestHeadersA(int64_t h, const char* headers, int64_t len, int64_t flags){ (void)h;(void)headers;(void)len;(void)flags; return 1; }
EXPORT int64_t HttpSendRequestA(int64_t h, const char* headers, int64_t hlen, void* opt, int64_t optlen){ (void)h;(void)headers;(void)hlen;(void)opt;(void)optlen; return 1; }
EXPORT int64_t HttpQueryInfoA(int64_t h, int64_t level, void* buf, int64_t* len, int64_t* idx){ (void)h;(void)level;(void)buf;(void)idx; if(len)*len=0; return 1; }
EXPORT int64_t InternetReadFile(int64_t h, char* buf, int64_t toread, int64_t* read){ (void)h;(void)buf;(void)toread; if(read)*read=0; return 1; }
EXPORT int64_t InternetSetOptionA(int64_t h, int64_t opt, void* buf, int64_t len){ (void)h;(void)opt;(void)buf;(void)len; return 1; }
EXPORT int64_t InternetQueryOptionA(int64_t h, int64_t opt, void* buf, int64_t* len){ (void)h;(void)opt;(void)buf; if(len)*len=0; return 1; }

/* ============================== kernel32 ============================== */
EXPORT int64_t GetTickCount(void){ return (int64_t)monotime_ms(); }
EXPORT int64_t GetTickCount64(void){ return (int64_t)monotime_ms(); }
EXPORT void Sleep(int64_t ms){ if(ms>0) usleep((useconds_t)(ms*1000)); }
EXPORT void OutputDebugStringA(const char* s){ if(s){ fputs(s, stderr); fflush(stderr); } }
EXPORT void OutputDebugStringW(const char* s){ OutputDebugStringA(s); }
EXPORT int64_t WinExec(const char* cmd, int64_t show){ (void)cmd;(void)show; return 0; }
EXPORT int64_t GetLastError(void){ return *tls_slot(); }
EXPORT void SetLastError(int64_t e){ *tls_slot() = e; }
EXPORT int64_t lstrlenA(const char* s){ return s ? (int64_t)strlen(s) : 0; }
EXPORT int64_t GetCurrentProcessId(void){ return (int64_t)getpid(); }
EXPORT int64_t GetCurrentThreadId(void){ return (int64_t)(uintptr_t)pthread_self(); }
EXPORT int64_t GetModuleHandleA(const char* name){ (void)name; return 0; }
EXPORT int64_t GetModuleHandleW(const char* name){ (void)name; return 0; }

/* SYSTEMTIME = 8 WORD (16 byte) */
EXPORT void GetSystemTime(void* st){ if(!st)return; memset(st,0,16); time_t t=time(NULL); struct tm u; gmtime_r(&t,&u);
    int16_t* w=(int16_t*)st; w[0]=(int16_t)(u.tm_year+1900); w[1]=(int16_t)(u.tm_mon+1); w[2]=(int16_t)u.tm_wday; w[3]=(int16_t)u.tm_mday; w[4]=(int16_t)u.tm_hour; w[5]=(int16_t)u.tm_min; w[6]=(int16_t)u.tm_sec; w[7]=0; }
EXPORT void GetLocalTime(void* st){ if(!st)return; memset(st,0,16); time_t t=time(NULL); struct tm u; localtime_r(&t,&u);
    int16_t* w=(int16_t*)st; w[0]=(int16_t)(u.tm_year+1900); w[1]=(int16_t)(u.tm_mon+1); w[2]=(int16_t)u.tm_wday; w[3]=(int16_t)u.tm_mday; w[4]=(int16_t)u.tm_hour; w[5]=(int16_t)u.tm_min; w[6]=(int16_t)u.tm_sec; w[7]=0; }
EXPORT void GetSystemTimeAsFileTime(void* ft){ if(!ft)return; /* FILETIME = 100ns da 1601 */ struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    uint64_t t=(uint64_t)ts.tv_sec*10000000ull + (uint64_t)ts.tv_nsec/100ull + 116444736000000000ull; ((uint32_t*)ft)[0]=(uint32_t)t; ((uint32_t*)ft)[1]=(uint32_t)(t>>32); }
/* TIME_ZONE_INFORMATION: BIAS(long) + 2*(StandardName[32],StandardDate[16],StandardBias(long)) + 2*(DaylightName[32],DaylightDate[16],DaylightBias(long)) */
EXPORT int64_t GetTimeZoneInformation(void* tzi){ if(tzi) memset(tzi,0,172); return 0; } /* TIME_ZONE_ID_UNKNOWN */
EXPORT int64_t SystemTimeToTzSpecificLocalTime(void* tz, const void* utc, void* loc){ (void)tz; if(utc&&loc) memcpy(loc,utc,16); return 1; }
EXPORT int64_t TzSpecificLocalTimeToSystemTime(void* tz, const void* loc, void* utc){ (void)tz; if(utc&&loc) memcpy(utc,loc,16); return 1; }
EXPORT int64_t FileTimeToSystemTime(const void* ft, void* st){ (void)ft; if(st) memset(st,0,16); return 1; }
EXPORT int64_t SystemTimeToFileTime(const void* st, void* ft){ (void)st; if(ft) memset(ft,0,8); return 1; }
EXPORT int64_t MultiByteToWideChar(int64_t cp, int64_t flags, const char* in, int64_t inlen, void* out, int64_t outlen){ (void)cp;(void)flags;(void)in;(void)inlen;(void)out;(void)outlen; return 0; }
EXPORT int64_t WideCharToMultiByte(int64_t cp, int64_t flags, const void* in, int64_t inlen, char* out, int64_t outlen, const char* def, int64_t* used){ (void)cp;(void)flags;(void)in;(void)inlen;(void)def;(void)used; if(out&&outlen>0) out[0]=0; return 0; }
EXPORT int64_t CreateDirectoryA(const char* p, void* sa){ (void)sa; return p ? (mkdir(p,0755)==0 || access(p,0)==0) : 0; }

/* ============================== user32 ============================== */
EXPORT int64_t MessageBoxA(int64_t hwnd, const char* text, const char* caption, int64_t type){ (void)hwnd;(void)type; fprintf(stderr,"[MessageBox] %s: %s\n", caption?caption:"", text?text:""); return 1; }
EXPORT int64_t MessageBoxW(int64_t hwnd, const char* text, const char* caption, int64_t type){ return MessageBoxA(hwnd,text,caption,type); }
EXPORT int64_t MessageBoxExA(int64_t hwnd, const char* text, const char* caption, int64_t type, int64_t lang){ (void)lang; return MessageBoxA(hwnd,text,caption,type); }
EXPORT int64_t MessageBeep(int64_t type){ (void)type; return 1; }
EXPORT int64_t PostMessageA(int64_t hwnd, int64_t msg, int64_t w, int64_t l){ (void)hwnd;(void)msg;(void)w;(void)l; return 1; }
EXPORT int64_t PostMessageW(int64_t hwnd, int64_t msg, int64_t w, int64_t l){ (void)hwnd;(void)msg;(void)w;(void)l; return 1; }
EXPORT int64_t SendMessageA(int64_t hwnd, int64_t msg, int64_t w, int64_t l){ (void)hwnd;(void)msg;(void)w;(void)l; return 0; }
EXPORT int64_t SendNotifyMessageA(int64_t hwnd, int64_t msg, int64_t w, int64_t l){ (void)hwnd;(void)msg;(void)w;(void)l; return 1; }
EXPORT int64_t RegisterWindowMessageA(const char* s){ uint64_t h=1469598103934665603ull; while(s&&*s){ h^=(unsigned char)*s++; h*=1099511628211ull; } return (int64_t)(h & 0xffff); }
EXPORT int64_t GetClientRect(int64_t hwnd, void* rect){ (void)hwnd; if(rect) memset(rect,0,16); return 1; }
EXPORT int64_t GetWindowRect(int64_t hwnd, void* rect){ (void)hwnd; if(rect) memset(rect,0,16); return 1; }
EXPORT int64_t GetForegroundWindow(void){ return 0; }
EXPORT int64_t GetDesktopWindow(void){ return 0; }
EXPORT int64_t FindWindowA(const char* cls, const char* win){ (void)cls;(void)win; return 0; }
EXPORT int64_t ShowWindow(int64_t hwnd, int64_t cmd){ (void)hwnd;(void)cmd; return 0; }
EXPORT int64_t UpdateWindow(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t SetWindowTextA(int64_t hwnd, const char* s){ (void)hwnd;(void)s; return 1; }
EXPORT int64_t GetWindowTextA(int64_t hwnd, char* s, int64_t max){ (void)hwnd; if(s&&max>0) s[0]=0; return 0; }
EXPORT int64_t SetWindowPos(int64_t hwnd, int64_t after, int64_t x, int64_t y, int64_t cx, int64_t cy, int64_t flags){ (void)hwnd;(void)after;(void)x;(void)y;(void)cx;(void)cy;(void)flags; return 1; }
EXPORT int64_t MoveWindow(int64_t hwnd, int64_t x, int64_t y, int64_t w, int64_t h, int64_t repaint){ (void)hwnd;(void)x;(void)y;(void)w;(void)h;(void)repaint; return 1; }
EXPORT int64_t SetFocus(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t SetActiveWindow(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t IsWindowVisible(int64_t hwnd){ (void)hwnd; return 1; }
EXPORT int64_t IsZoomed(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t IsIconic(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t SwapMouseButton(int64_t f){ (void)f; return 0; }
EXPORT int64_t GetSystemMetrics(int64_t idx){ (void)idx; return 0; }
EXPORT void mouse_event(int64_t flags, int64_t dx, int64_t dy, int64_t data, int64_t extra){ (void)flags;(void)dx;(void)dy;(void)data;(void)extra; }
EXPORT void keybd_event(int64_t vk, int64_t scan, int64_t flags, int64_t extra){ (void)vk;(void)scan;(void)flags;(void)extra; }
EXPORT int64_t GetKeyState(int64_t vk){ (void)vk; return 0; }
EXPORT int64_t GetAsyncKeyState(int64_t vk){ (void)vk; return 0; }
EXPORT int64_t GetCursorPos(void* pt){ if(pt) memset(pt,0,8); return 1; }
EXPORT int64_t SetCursorPos(int64_t x, int64_t y){ (void)x;(void)y; return 1; }

/* ============================== shell32 ============================== */
EXPORT int64_t ShellExecuteA(int64_t hwnd, const char* op, const char* file, const char* params, const char* dir, int64_t show){ (void)hwnd;(void)op;(void)file;(void)params;(void)dir;(void)show; return 0; }
EXPORT int64_t ShellExecuteW(int64_t hwnd, const char* op, const char* file, const char* params, const char* dir, int64_t show){ return ShellExecuteA(hwnd,op,file,params,dir,show); }
EXPORT int64_t ShellExecuteExA(void* sei){ (void)sei; return 0; }

/* ============================== advapi32 ============================== */
EXPORT int64_t RegOpenKeyExA(int64_t hkey, const char* sub, int64_t opt, int64_t access, int64_t* out){ (void)hkey;(void)sub;(void)opt;(void)access; if(out)*out=0; return 2; }
EXPORT int64_t RegQueryValueExA(int64_t hkey, const char* name, int64_t* res, int64_t* type, void* data, int64_t* len){ (void)hkey;(void)name;(void)res;(void)type;(void)data; if(len)*len=0; return 2; }
EXPORT int64_t RegSetValueExA(int64_t hkey, const char* name, int64_t res, int64_t type, const void* data, int64_t len){ (void)hkey;(void)name;(void)res;(void)type;(void)data;(void)len; return 0; }
EXPORT int64_t RegCloseKey(int64_t hkey){ (void)hkey; return 0; }

/* ============================== gdi32 ============================== */
EXPORT int64_t GetStockObject(int64_t i){ (void)i; return 0; }
EXPORT int64_t GetDeviceCaps(int64_t dc, int64_t idx){ (void)dc;(void)idx; return 0; }
EXPORT int64_t DeleteObject(int64_t obj){ (void)obj; return 1; }

/* ============================================================================
 * Estensione: file I/O, memoria, stringhe, modulo/env, winmm, ws2_32, gdi32/user32.
 * ============================================================================ */

/* --- tabella HANDLE file (indice+1) --- */
#define W32_MAXFILES 256
static FILE* g_files[W32_MAXFILES];
static int64_t file_put(FILE* f){ for(int i=0;i<W32_MAXFILES;i++){ if(!g_files[i]){ g_files[i]=f; return i+1; } } fclose(f); return -1; }
static FILE*  file_get(int64_t h){ return (h>=1 && h<=W32_MAXFILES) ? g_files[h-1] : NULL; }

#define GENERIC_READ_  0x80000000ll
#define GENERIC_WRITE_ 0x40000000ll

EXPORT int64_t CreateFileA(const char* name, int64_t access, int64_t share, void* sa, int64_t disp, int64_t attrs, int64_t tpl){
    (void)share;(void)sa;(void)attrs;(void)tpl; FILE* f=NULL;
    int w = (access & GENERIC_WRITE_)!=0;
    if(disp==2 || disp==1 || disp==5) f = fopen(name, w?"wb":"rb");
    else if(disp==4) f = fopen(name, w?"ab+":"rb");
    else f = fopen(name, w?"r+b":"rb");
    if(!f && w) f = fopen(name,"wb+");
    if(!f) return -1; /* INVALID_HANDLE_VALUE */
    return file_put(f);
}
EXPORT int64_t ReadFile(int64_t h, void* buf, int64_t n, void* readp, void* ov){ (void)ov; FILE* f=file_get(h); if(!f) return 0;
    size_t got = fread(buf,1,(size_t)(n>0?n:0),f); if(readp) *(int64_t*)readp=(int64_t)got; return 1; }
EXPORT int64_t WriteFile(int64_t h, const void* buf, int64_t n, void* writep, void* ov){ (void)ov; FILE* f=file_get(h); if(!f) return 0;
    size_t put = fwrite(buf,1,(size_t)(n>0?n:0),f); if(writep) *(int64_t*)writep=(int64_t)put; return 1; }
EXPORT int64_t CloseHandle(int64_t h){ FILE* f=file_get(h); if(f){ fclose(f); g_files[h-1]=NULL; return 1; } return 1; }
EXPORT int64_t GetFileSize(int64_t h, void* highp){ FILE* f=file_get(h); if(!f) return 0; long cur=ftell(f); fseek(f,0,SEEK_END); long e=ftell(f); fseek(f,cur,SEEK_SET); if(highp) *(int64_t*)highp=0; return e<0?0:e; }
EXPORT int64_t SetFilePointer(int64_t h, int64_t lo, void* highp, int64_t method){ (void)highp; FILE* f=file_get(h); if(!f) return -1;
    int whence = method==0?SEEK_SET:method==1?SEEK_CUR:SEEK_END; if(fseek(f,(long)lo,whence)!=0) return -1; return ftell(f); }
EXPORT int64_t DeleteFileA(const char* name){ return remove(name)==0 ? 1 : 0; }
EXPORT int64_t MoveFileA(const char* a, const char* b){ return rename(a,b)==0 ? 1 : 0; }
EXPORT int64_t RemoveDirectoryA(const char* p){ return rmdir(p)==0 ? 1 : 0; }
EXPORT int64_t CopyFileA(const char* a, const char* b, int64_t failifexists){ FILE* fi=fopen(a,"rb"); if(!fi) return 0;
    if(failifexists && access(b,0)==0){ fclose(fi); return 0; } FILE* fo=fopen(b,"wb"); if(!fo){ fclose(fi); return 0; }
    char tmp[8192]; size_t r; while((r=fread(tmp,1,sizeof tmp,fi))>0) fwrite(tmp,1,r,fo); fclose(fi); fclose(fo); return 1; }
EXPORT int64_t GetFileAttributesA(const char* name){ struct stat st; if(stat(name,&st)!=0) return -1; if(S_ISDIR(st.st_mode)) return 0x10; return 0x80; }
EXPORT int64_t SetFileAttributesA(const char* name, int64_t a){ (void)name;(void)a; return 1; }
EXPORT int64_t GetCurrentDirectoryA(int64_t n, char* buf){ if(!buf) return 0; if(!getcwd(buf,(size_t)n)) buf[0]=0; return (int64_t)strlen(buf); }
EXPORT int64_t SetCurrentDirectoryA(const char* p){ return chdir(p)==0 ? 1 : 0; }
EXPORT int64_t GetFullPathNameA(const char* name, int64_t n, char* buf, char** filepart){ (void)filepart; if(buf&&n>0){ strncpy(buf,name?name:"",(size_t)n-1); buf[n-1]=0; } return (int64_t)strlen(name?name:""); }
EXPORT int64_t FindFirstFileA(const char* pat, void* data){ (void)pat;(void)data; return -1; }
EXPORT int64_t FindNextFileA(int64_t h, void* data){ (void)h;(void)data; return 0; }
EXPORT int64_t FindClose(int64_t h){ (void)h; return 1; }

/* --- memoria --- */
EXPORT int64_t GlobalAlloc(int64_t flags, int64_t size){ (void)flags; return (int64_t)(intptr_t)malloc((size_t)size); }
EXPORT int64_t GlobalFree(int64_t p){ free((void*)(intptr_t)p); return 0; }
EXPORT int64_t LocalAlloc(int64_t flags, int64_t size){ (void)flags; return (int64_t)(intptr_t)malloc((size_t)size); }
EXPORT int64_t LocalFree(int64_t p){ free((void*)(intptr_t)p); return 0; }
EXPORT int64_t HeapAlloc(int64_t heap, int64_t flags, int64_t size){ (void)heap;(void)flags; return (int64_t)(intptr_t)malloc((size_t)size); }
EXPORT int64_t HeapFree(int64_t heap, int64_t flags, int64_t p){ (void)heap;(void)flags; free((void*)(intptr_t)p); return 1; }

/* --- stringhe --- */
EXPORT int64_t lstrcpyA(char* dst, const char* src){ if(dst&&src){ strcpy(dst,src); return (int64_t)strlen(dst); } return 0; }
EXPORT int64_t lstrcpynA(char* dst, const char* src, int64_t n){ if(dst&&src&&n>0){ strncpy(dst,src,(size_t)n-1); dst[n-1]=0; } return (int64_t)strlen(dst?dst:""); }
EXPORT int64_t lstrcatA(char* dst, const char* src){ if(dst&&src) strcat(dst,src); return (int64_t)strlen(dst?dst:""); }
EXPORT int64_t lstrcmpA(const char* a, const char* b){ int r=strcmp(a?a:"",b?b:""); return r<0?-1:r>0?1:0; }
EXPORT void wsprintfA(char* dst, const char* fmt, ...){ (void)fmt; if(dst) dst[0]=0; }
EXPORT void FormatMessageA(int64_t flags, void* src, int64_t id, int64_t lang, char* buf, int64_t size, void* args){ (void)flags;(void)src;(void)id;(void)lang;(void)args; if(buf&&size>0) buf[0]=0; }

/* --- modulo / env / process --- */
EXPORT int64_t GetModuleFileNameA(int64_t hmod, char* buf, int64_t n){ (void)hmod; if(buf&&n>0) buf[0]=0; return 0; }
EXPORT int64_t GetCommandLineA(void){ return (int64_t)(intptr_t)""; }
EXPORT int64_t GetEnvironmentVariableA(const char* name, char* buf, int64_t n){ if(!buf||n<=0) return 0; const char* e=getenv(name); if(!e){ buf[0]=0; return 0; } strncpy(buf,e,(size_t)n-1); buf[n-1]=0; return (int64_t)strlen(buf); }
EXPORT int64_t SetEnvironmentVariableA(const char* name, const char* val){ if(val) setenv(name,val,1); else unsetenv(name); return 1; }
EXPORT void ExitProcess(int64_t code){ (void)code; /* no-op: non terminare il backtest */ }
EXPORT int64_t TerminateProcess(int64_t h, int64_t code){ (void)h;(void)code; return 1; }
EXPORT int64_t GetComputerNameA(char* buf, int64_t* n){ if(buf&&n&&*n>0){ strcpy(buf,"gmbasic"); *(int64_t*)n=7; return 1; } return 0; }
EXPORT int64_t GetUserNameA(char* buf, int64_t* n){ if(buf&&n&&*n>0){ strcpy(buf,"gmbasic"); *(int64_t*)n=7; return 1; } return 0; }
EXPORT int64_t Beep(int64_t freq, int64_t dur){ (void)freq;(void)dur; return 1; }
EXPORT int64_t GetVersion(void){ return 0x0A00; }
EXPORT int64_t GetVersionExA(void* vi){ if(vi) memset(vi,0,20); return 1; }
EXPORT int64_t GetSystemInfo(void* si){ if(si) memset(si,0,64); return 0; }
EXPORT int64_t QueryPerformanceCounter(void* out){ if(out) *(int64_t*)out = (int64_t)(monotime_ms()*1000ull); return 1; }
EXPORT int64_t QueryPerformanceFrequency(void* out){ if(out) *(int64_t*)out = 1000000; return 1; }
EXPORT int64_t InterlockedIncrement(volatile int32_t* p){ return (int64_t)(++(*p)); }
EXPORT int64_t InterlockedDecrement(volatile int32_t* p){ return (int64_t)(--(*p)); }

/* --- winmm --- */
EXPORT int64_t timeGetTime(void){ return (int64_t)monotime_ms(); }
EXPORT int64_t PlaySoundA(const char* name, int64_t mod, int64_t flags){ (void)name;(void)mod;(void)flags; return 0; }
EXPORT int64_t mciSendStringA(const char* cmd, char* ret, int64_t retlen, int64_t hwnd){ (void)cmd;(void)hwnd; if(ret&&retlen>0) ret[0]=0; return 0; }

/* --- ws2_32 (stub: nessuna rete) --- */
EXPORT int64_t WSAStartup(int64_t ver, void* data){ (void)ver; if(data) memset(data,0,400); return 0; }
EXPORT int64_t WSACleanup(void){ return 0; }
EXPORT int64_t WSAGetLastError(void){ return 0; }
EXPORT int64_t socket(int64_t af, int64_t type, int64_t proto){ (void)af;(void)type;(void)proto; return -1; }
EXPORT int64_t connect(int64_t s, const void* name, int64_t namelen){ (void)s;(void)name;(void)namelen; return -1; }
EXPORT int64_t send(int64_t s, const char* buf, int64_t len, int64_t flags){ (void)s;(void)buf;(void)len;(void)flags; return -1; }
EXPORT int64_t recv(int64_t s, char* buf, int64_t len, int64_t flags){ (void)s;(void)buf;(void)len;(void)flags; return -1; }
EXPORT int64_t closesocket(int64_t s){ (void)s; return 0; }
EXPORT int64_t gethostbyname(const char* name){ (void)name; return 0; }
EXPORT int64_t inet_addr(const char* s){ (void)s; return -1; }
#undef htons
#undef ntohs
EXPORT int64_t htons(int64_t v){ return (int64_t)(((v & 0xff)<<8) | ((v>>8) & 0xff)); }
EXPORT int64_t ntohs(int64_t v){ return htons(v); }
EXPORT int64_t inet_ntoa(int64_t addr){ (void)addr; return (int64_t)(intptr_t)"0.0.0.0"; }

/* --- urlmon --- */
EXPORT int64_t URLDownloadToFileA(int64_t caller, const char* url, const char* file, int64_t res, int64_t cb){ (void)caller;(void)url;(void)file;(void)res;(void)cb; return 0; }

/* --- gdi32 (base) --- */
EXPORT int64_t CreateCompatibleDC(int64_t hdc){ (void)hdc; return 0; }
EXPORT int64_t CreateCompatibleBitmap(int64_t hdc, int64_t w, int64_t h){ (void)hdc;(void)w;(void)h; return 0; }
EXPORT int64_t DeleteDC(int64_t hdc){ (void)hdc; return 1; }
EXPORT int64_t BitBlt(int64_t d, int64_t x, int64_t y, int64_t w, int64_t h, int64_t s, int64_t sx, int64_t sy, int64_t rop){ (void)d;(void)x;(void)y;(void)w;(void)h;(void)s;(void)sx;(void)sy;(void)rop; return 1; }
EXPORT int64_t SelectObject(int64_t hdc, int64_t obj){ (void)hdc;(void)obj; return 0; }
EXPORT int64_t TextOutA(int64_t hdc, int64_t x, int64_t y, const char* s, int64_t n){ (void)hdc;(void)x;(void)y;(void)s;(void)n; return 1; }
EXPORT int64_t SetPixel(int64_t hdc, int64_t x, int64_t y, int64_t c){ (void)hdc;(void)x;(void)y;(void)c; return 0; }
EXPORT int64_t GetPixel(int64_t hdc, int64_t x, int64_t y){ (void)hdc;(void)x;(void)y; return 0; }
EXPORT int64_t CreateFontA(int64_t h, int64_t w, int64_t e, int64_t o, int64_t wt, int64_t it, int64_t u, int64_t st, int64_t cs, int64_t op, int64_t cl, int64_t q, int64_t p, const char* face){ (void)h;(void)w;(void)e;(void)o;(void)wt;(void)it;(void)u;(void)st;(void)cs;(void)op;(void)cl;(void)q;(void)p;(void)face; return 0; }
EXPORT int64_t CreatePen(int64_t style, int64_t w, int64_t c){ (void)style;(void)w;(void)c; return 0; }
EXPORT int64_t CreateSolidBrush(int64_t c){ (void)c; return 0; }
EXPORT int64_t Rectangle(int64_t hdc, int64_t l, int64_t t, int64_t r, int64_t b){ (void)hdc;(void)l;(void)t;(void)r;(void)b; return 1; }
EXPORT int64_t Ellipse(int64_t hdc, int64_t l, int64_t t, int64_t r, int64_t b){ (void)hdc;(void)l;(void)t;(void)r;(void)b; return 1; }
EXPORT int64_t MoveToEx(int64_t hdc, int64_t x, int64_t y, void* old){ (void)hdc;(void)x;(void)y;(void)old; return 1; }
EXPORT int64_t LineTo(int64_t hdc, int64_t x, int64_t y){ (void)hdc;(void)x;(void)y; return 1; }

/* --- user32 (base) --- */
EXPORT int64_t GetDC(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t ReleaseDC(int64_t hwnd, int64_t hdc){ (void)hwnd;(void)hdc; return 1; }
EXPORT int64_t GetWindowLongA(int64_t hwnd, int64_t idx){ (void)hwnd;(void)idx; return 0; }
EXPORT int64_t SetWindowLongA(int64_t hwnd, int64_t idx, int64_t v){ (void)hwnd;(void)idx;(void)v; return 0; }
EXPORT int64_t CreateWindowExA(int64_t ex, const char* cls, const char* title, int64_t style, int64_t x, int64_t y, int64_t w, int64_t h, int64_t parent, int64_t menu, int64_t inst, void* param){ (void)ex;(void)cls;(void)title;(void)style;(void)x;(void)y;(void)w;(void)h;(void)parent;(void)menu;(void)inst;(void)param; return 0; }
EXPORT int64_t DestroyWindow(int64_t hwnd){ (void)hwnd; return 1; }
EXPORT int64_t LoadCursorA(int64_t inst, const char* name){ (void)inst;(void)name; return 0; }
EXPORT int64_t SetCursor(int64_t c){ (void)c; return 0; }
EXPORT int64_t GetKeyboardState(void* ks){ if(ks) memset(ks,0,256); return 1; }
EXPORT int64_t ScreenToClient(int64_t hwnd, void* pt){ (void)hwnd;(void)pt; return 1; }
EXPORT int64_t ClientToScreen(int64_t hwnd, void* pt){ (void)hwnd;(void)pt; return 1; }
EXPORT int64_t InvalidateRect(int64_t hwnd, void* r, int64_t erase){ (void)hwnd;(void)r;(void)erase; return 1; }
EXPORT int64_t BeginPaint(int64_t hwnd, void* ps){ (void)hwnd; if(ps) memset(ps,0,64); return 0; }
EXPORT int64_t EndPaint(int64_t hwnd, void* ps){ (void)hwnd;(void)ps; return 1; }
EXPORT int64_t DefWindowProcA(int64_t hwnd, int64_t msg, int64_t w, int64_t l){ (void)hwnd;(void)msg;(void)w;(void)l; return 0; }
EXPORT int64_t GetParent(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t GetWindowTextLengthA(int64_t hwnd){ (void)hwnd; return 0; }
EXPORT int64_t WinMain(int64_t inst, int64_t prev, const char* cmd, int64_t show){ (void)inst;(void)prev;(void)cmd;(void)show; return 0; }
