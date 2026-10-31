/* gmb_sysinfo.c — SYSTEM.SYS(proprietà) per i backend C (C99, C99Native, VM/C) + memoria di
 * sistema disponibile/reclamabile. Parità con IlRuntime.Sys (backend managed IL/PCode).
 *
 * Memoria (attendibile, API native del SO):
 *   macOS  : host_statistics64(HOST_VM_INFO64) → (free+inactive+purgeable+speculative)×pageSize
 *   Linux  : /proc/meminfo → MemAvailable
 *   Windows: GlobalMemoryStatusEx → ullAvailPhys
 */
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <pwd.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

unsigned long long gmb_mem_total_bytes(void) {
#if defined(__APPLE__)
    unsigned long long mem = 0; size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) != 0) return 0;
    return mem;
#elif defined(_WIN32)
    MEMORYSTATUSEX m; m.dwLength = sizeof(m);
    return GlobalMemoryStatusEx(&m) ? (unsigned long long)m.ullTotalPhys : 0;
#else
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256]; unsigned long long kb = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %llu kB", &kb) == 1) break;
    }
    fclose(f);
    return kb * 1024ULL;
#endif
}

unsigned long long gmb_mem_available_bytes(void) {
#if defined(__APPLE__)
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm; mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS) return 0;
    vm_size_t page = 0;
    if (host_page_size(host, &page) != KERN_SUCCESS) page = 16384;
    unsigned long long pages = (unsigned long long)vm.free_count + vm.inactive_count
                             + vm.purgeable_count + vm.speculative_count;
    return pages * (unsigned long long)page;
#elif defined(_WIN32)
    MEMORYSTATUSEX m; m.dwLength = sizeof(m);
    return GlobalMemoryStatusEx(&m) ? (unsigned long long)m.ullAvailPhys : 0;
#else
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256]; unsigned long long kb = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemAvailable: %llu kB", &kb) == 1) break;
    }
    fclose(f);
    return kb * 1024ULL;
#endif
}

/* Buffer per-proprietà (statico: il valore va copiato in una stringa BASIC subito). */
static char g_sysbuf[256];

const char* gmb_sys_info(const char* property) {
    if (property == NULL) property = "";
    char p[32];
    size_t n = strlen(property);
    if (n >= sizeof(p)) n = sizeof(p) - 1;
    for (size_t i = 0; i < n; i++) {
        char c = property[i];
        p[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    p[n] = '\0';

#if defined(__APPLE__)
    const char* osname = "MACOS";
    const char* arch = "ARM64";
#elif defined(_WIN32)
    const char* osname = "WINDOWS";
    const char* arch = "X64";
#else
    const char* osname = "LINUX";
#if defined(__aarch64__)
    const char* arch = "ARM64";
#else
    const char* arch = "X64";
#endif
#endif

    if (strcmp(p, "OS") == 0) { snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", osname); return g_sysbuf; }
    if (strcmp(p, "ARCH") == 0) { snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", arch); return g_sysbuf; }
    if (strcmp(p, "UI") == 0) { snprintf(g_sysbuf, sizeof(g_sysbuf), "STANDARD"); return g_sysbuf; }
    if (strcmp(p, "VERSION") == 0) { snprintf(g_sysbuf, sizeof(g_sysbuf), "GMBASICAI"); return g_sysbuf; }
    if (strcmp(p, "RUNTIME") == 0) { snprintf(g_sysbuf, sizeof(g_sysbuf), "NATIVE-C"); return g_sysbuf; }
    if (strcmp(p, "CPUCOUNT") == 0) {
#if defined(_WIN32)
        SYSTEM_INFO si; GetSystemInfo(&si);
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%lu", (unsigned long)si.dwNumberOfProcessors);
#else
        long c = sysconf(_SC_NPROCESSORS_ONLN);
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%ld", c > 0 ? c : 0);
#endif
        return g_sysbuf;
    }
    if (strcmp(p, "MACHINE") == 0) {
        char host[128] = {0};
#if defined(_WIN32)
        DWORD sz = sizeof(host); GetComputerNameA(host, &sz);
#else
        gethostname(host, sizeof(host) - 1);
#endif
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", host); return g_sysbuf;
    }
    if (strcmp(p, "USER") == 0) {
#if defined(_WIN32)
        char un[128] = {0}; DWORD sz = sizeof(un); GetUserNameA(un, &sz);
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", un);
#else
        struct passwd* pw = getpwuid(getuid());
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", pw && pw->pw_name ? pw->pw_name : "");
#endif
        return g_sysbuf;
    }
    if (strcmp(p, "CWD") == 0) {
        char cwd[1024] = {0};
#if defined(_WIN32)
        GetCurrentDirectoryA(sizeof(cwd), cwd);
#else
        if (getcwd(cwd, sizeof(cwd) - 1) == NULL) cwd[0] = '\0';
#endif
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", cwd); return g_sysbuf;
    }
    if (strcmp(p, "MEMAVAILABLE") == 0 || strcmp(p, "MEMFREE") == 0 ||
        strcmp(p, "MEMRECLAIMABLE") == 0) {
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%llu", gmb_mem_available_bytes()); return g_sysbuf;
    }
    if (strcmp(p, "MEMTOTAL") == 0) {
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%llu", gmb_mem_total_bytes()); return g_sysbuf;
    }
    if (strcmp(p, "MEMUSED") == 0) {
        unsigned long long t = gmb_mem_total_bytes(), a = gmb_mem_available_bytes();
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%llu", t > a ? t - a : 0ULL); return g_sysbuf;
    }
    if (strcmp(p, "MEMAVAILABLEMB") == 0 || strcmp(p, "MEMFREEMB") == 0) {
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%llu", gmb_mem_available_bytes() / (1024ULL * 1024ULL)); return g_sysbuf;
    }
    if (strcmp(p, "MEMTOTALMB") == 0) {
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%llu", gmb_mem_total_bytes() / (1024ULL * 1024ULL)); return g_sysbuf;
    }
    if (strcmp(p, "SETTINGSPATH") == 0) {
        char sp[1024] = {0};
        gmb_paths_settings_file(sp, sizeof(sp));
        snprintf(g_sysbuf, sizeof(g_sysbuf), "%s", sp);
        return g_sysbuf;
    }
    g_sysbuf[0] = '\0';
    return g_sysbuf;
}
