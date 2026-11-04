/* gmb_cli_bsp.c — precompilazione delle pagine BSP del progetto.
 *
 * Il motore BSP COMPLETO in C esegue la pagina con la VM/C a partire da un chunk A1
 * (`<pagina>.bsp.chunk`) prodotto dal compilatore C (`gmbchunk`, che accetta
 * direttamente un `.bsp`). Questo passo avviene in `gmb build`/`run`: se il chunk
 * manca o è più vecchio della pagina, viene rigenerato. Se il driver non c'è o la
 * compilazione fallisce, si avvisa e si prosegue: il runtime usa il valutatore di
 * espressioni (fallback dichiarato).
 */
#include "gmb_cli_internal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <strings.h>
#include <unistd.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static int self_dir(char* out, size_t cap)
{
    char exe[CLI_PATH] = "";
#if defined(__APPLE__)
    uint32_t sz = (uint32_t)sizeof exe;
    if (_NSGetExecutablePath(exe, &sz) != 0) return 0;
#elif defined(_WIN32)
    if (!GetModuleFileNameA(NULL, exe, (DWORD)sizeof exe)) return 0;
#else
    {
        ssize_t r = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (r <= 0) return 0;
        exe[r] = 0;
    }
#endif
    char* sl = strrchr(exe, '/');
#if defined(_WIN32)
    if (!sl) sl = strrchr(exe, '\\');
#endif
    if (sl) *sl = 0;
    snprintf(out, cap, "%s", exe);
    return out[0] != 0;
}

static int ends_with_bsp(const char* name)
{
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".bsp") == 0;
}

/* 0 = fatto/senza pagine; il chiamante non deve fallire la build per questo passo. */
int gmb_cli_precompile_bsp(const char* project)
{
    if (!project || !*project) return 0;
    char pages[CLI_PATH];
    snprintf(pages, sizeof pages, "%s/pages", project);
    DIR* d = opendir(pages);
    if (!d) return 0;   /* progetto senza pagine BSP */

    char tdir[CLI_PATH], tool[CLI_PATH + 40];
    int have_tool = self_dir(tdir, sizeof tdir);
    snprintf(tool, sizeof tool, "%s/gmbchunk", tdir);
    if (have_tool && access(tool, X_OK) != 0) have_tool = 0;

    int warned = 0, compiled = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (!ends_with_bsp(e->d_name)) continue;
        char page[CLI_PATH], chunk[CLI_PATH + 8];
        snprintf(page, sizeof page, "%s/%s", pages, e->d_name);
        snprintf(chunk, sizeof chunk, "%s.chunk", page);
        struct stat sp, sc;
        if (stat(page, &sp) != 0) continue;
        if (stat(chunk, &sc) == 0 && sc.st_mtime >= sp.st_mtime) continue;  /* fresco */
        if (!have_tool) {
            if (!warned)
                fprintf(stderr, "gmb: gmbchunk non trovato accanto a gmb: le pagine BSP "
                                "useranno il valutatore (non il compilatore completo)\n");
            warned = 1;
            continue;
        }
        char cmd[CLI_PATH * 3];
        snprintf(cmd, sizeof cmd, "'%s' '%s' '%s'", tool, page, chunk);
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "gmb: BSP %s non precompilata (resta il fallback al valutatore)\n",
                    e->d_name);
        } else {
            compiled++;
        }
    }
    closedir(d);
    (void)compiled;
    return 0;
}
