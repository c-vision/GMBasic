/* gmb_cli_admin.c — `gmb <comando-admin>` inoltra a `gmbadmin` (tool C, stesso mondo).
 *
 * Front unico: l'utente usa sempre `gmb`. I comandi amministrativi sono implementati in C
 * dal tool `gmbadmin` (runtime C), non nel mondo C#.
 */
#include "gmb_cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

/* `gmb install …` = installazione delle LIBRERIE native (contratto storico del CLI). */
static int is_libs_install(const char* c) { return c && !strcmp(c, "install"); }

/* Alias TOP-LEVEL del package manager (contratto storico: `gmb add …` == `gmb pkg add …`). */
static int is_pkg_alias(const char* c)
{
    static const char* names[] = {
        "init", "add", "remove", "bump", "restore", "update", "trust", "rollback", "cache", NULL
    };
    for (int i = 0; names[i]; i++) if (!strcmp(c, names[i])) return 1;
    return 0;
}

static int is_admin_cmd(const char* c)
{
    static const char* names[] = {
        "pkg", "fixtures", "setup", "libs", "brain", "grep", "rg",
        "convert", "convfmt", "keygen",
        /* non ancora portati (gmbadmin risponde con un errore esplicito, F9c) */
        "publish", "login", "register", "logout", "admin", "pack", "daemon",
        "rag", "graph", "hf", "mlx", "web2md", "batch", NULL
    };
    for (int i = 0; names[i]; i++) if (!strcmp(c, names[i])) return 1;
    return 0;
}

int gmb_cli_find_admin(char* out, size_t cap)
{
    const char* pe = getenv("PATH");
    if (pe) {
        char tmp[4096];
        snprintf(tmp, sizeof tmp, "%s", pe);
        for (char* d = strtok(tmp, ":"); d; d = strtok(NULL, ":")) {
            snprintf(out, cap, "%s/gmbadmin", d);
            if (access(out, X_OK) == 0) return 1;
        }
    }
    static const char* rel[] = {
        "gmbadmin", "build/gmbadmin", "gmbadmin/build/gmbadmin",
        "GMBasic/native/gmbadmin/build/gmbadmin",
        "../gmbadmin/build/gmbadmin", "../../gmbadmin/build/gmbadmin",
        "../../../gmbadmin/build/gmbadmin", NULL
    };
    for (int i = 0; rel[i]; i++)
        if (access(rel[i], X_OK) == 0) { snprintf(out, cap, "%s", rel[i]); return 1; }
    char exe[4096];
    ssize_t n = -1;
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) == 0) n = (ssize_t)strlen(exe);
    }
#else
    n = readlink("/proc/self/exe", exe, sizeof exe - 1);
#endif
    if (n > 0) {
        exe[n] = 0;
        char* sl = strrchr(exe, '/');
        if (sl) {
            *sl = 0;
            char cand[4096];
            /* gmb in native/gmb_compiler_c/build ; gmbadmin in native/gmbadmin/build */
            snprintf(cand, sizeof cand, "%s/../../gmbadmin/build/gmbadmin", exe);
            if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
            snprintf(cand, sizeof cand, "%s/gmbadmin", exe);
            if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
        }
    }
    return 0;
}

/* -1 = non è un comando admin (prosegui); altrimenti exit code del tool. */
int gmb_cli_admin_maybe(int argc, char** argv)
{
    if (argc < 2) return -1;
    const int alias = is_pkg_alias(argv[1]);
    const int libs_install = is_libs_install(argv[1]);
    if (!alias && !libs_install && !is_admin_cmd(argv[1])) return -1;

    char path[4096];
    if (!gmb_cli_find_admin(path, sizeof path)) {
        fprintf(stderr, "?gmb %s: tool `gmbadmin` non trovato. Compila GMBasic/native/gmbadmin "
                        "oppure mettilo nel PATH.\n", argv[1]);
        return 2;
    }
    /* `gmb add …` → `gmbadmin pkg add …`; `gmb install …` → `gmbadmin libs install …`
     * (stesso argv, un solo backend) */
    char** nv = argv;
    if (alias || libs_install) {
        nv = (char**)calloc((size_t)argc + 2, sizeof(char*));
        if (!nv) { fprintf(stderr, "?gmb: memoria esaurita\n"); return 2; }
        nv[0] = argv[0];
        nv[1] = alias ? (char*)"pkg" : (char*)"libs";
        nv[2] = argv[1];
        for (int i = 2; i < argc; i++) nv[i + 1] = argv[i];
        nv[argc + 1] = NULL;
    }
    if (strchr(path, '/')) execv(path, nv);
    else execvp(path, nv);
    perror("?gmb: exec gmbadmin");
    return 2;
}
