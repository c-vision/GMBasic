/* dlfcn.h (Windows) — dlopen/dlsym/dlclose/dlerror su LoadLibrary/GetProcAddress. */
#ifndef GMB_WIN32_DLFCN_H
#define GMB_WIN32_DLFCN_H
#if defined(_WIN32) || defined(_WIN64)
#include "gmb_win32.h"

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif
#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void*)0)
#endif

/* dladdr: su Windows si risolve il modulo che contiene l'indirizzo (per il nome si usa
 * GetModuleFileName; qui basta la base/il percorso, che è ciò che serve al discovery). */
#ifndef GMB_DL_INFO_DEFINED
#define GMB_DL_INFO_DEFINED
typedef struct {
    const char* dli_fname;
    void*       dli_fbase;
    const char* dli_sname;
    void*       dli_saddr;
} Dl_info;
#endif
static inline int dladdr(const void* addr, Dl_info* info)
{
    HMODULE m = NULL;
    if (!info) return 0;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)addr, &m) || !m)
        return 0;
    info->dli_fbase = (void*)m;
    static char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(m, path, (DWORD)sizeof path);
    info->dli_fname = (n > 0) ? path : NULL;
    return 1;
}

static inline void* dlopen(const char* file, int mode)
{
    (void)mode;
    if (!file) return (void*)GetModuleHandleA(NULL);   /* RTLD_DEFAULT */
    return (void*)LoadLibraryA(file);
}
static inline void* dlsym(void* handle, const char* name)
{
    return (void*)GetProcAddress((HMODULE)handle, name);
}
static inline int dlclose(void* handle)
{
    return (handle == NULL) ? 0 : (FreeLibrary((HMODULE)handle) ? 0 : -1);
}
static inline const char* dlerror(void)
{
    static char buf[256];
    DWORD e = GetLastError();
    if (!e) return NULL;
    snprintf(buf, sizeof buf, "win32 error %lu", (unsigned long)e);
    return buf;
}
#endif
#endif
