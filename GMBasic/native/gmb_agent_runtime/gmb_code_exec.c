#include "gmb_code_exec.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Resolve del runner: env GMB_EXECUTOR_NATIVE, altrimenti repo accanto. */
static void find_executor(char* buf, size_t cap) {
    const char* e = gmb_opt_env("GMB_EXECUTOR_NATIVE");
    if (e && *e) { snprintf(buf, cap, "%s", e); return; }
    {
        /* Nessun path personale: il runner vive sotto la root del repo C (GMBasic/). */
        char root[PATH_MAX];
        if (gmb_paths_repo_root(root, sizeof root) && root[0])
            snprintf(buf, cap, "%s/native/executor/build/gmb_executor_native_host", root);
        else
            snprintf(buf, cap, "native/executor/build/gmb_executor_native_host");
    }
}

int gmb_code_exec(const char* code, char* out, size_t cap) {
    if (!code || !out || cap == 0) return -1;
    out[0] = 0;
    const char* wd = gmb_opt_env("GMB_WORK");
    const char* dir = (wd && *wd) ? wd : "work";
    char bas[PATH_MAX], capt[PATH_MAX], exe[PATH_MAX];
    find_executor(exe, sizeof(exe));
    if (access(exe, X_OK) != 0) {
        snprintf(out, cap, "error: executor nativo non trovato: %s (GMB_EXECUTOR_NATIVE)", exe);
        return -1;
    }
    /* dir di lavoro (mkdir -p) */
    { char cmd[PATH_MAX + 32]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir); system(cmd); }
    snprintf(bas, sizeof(bas), "%s/agent_code_%ld.bas", dir, (long)getpid());
    snprintf(capt, sizeof(capt), "%s/agent_code_%ld.out", dir, (long)getpid());

    FILE* f = fopen(bas, "wb");
    if (!f) { snprintf(out, cap, "error: apertura %s", bas); return -1; }
    fwrite(code, 1, strlen(code), f);
    fclose(f);

    /* comando: executor <file> > capture 2>&1 ; printf cod. exit separato */
    char cmd[PATH_MAX * 2 + 64];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' > '%s' 2>&1; echo __RC__=$? >> '%s'", exe, bas, capt, capt);
    int rc = system(cmd);
    /* leggi l'output */
    FILE* r = fopen(capt, "rb");
    if (!r) { snprintf(out, cap, "error: output mancante (rc=%d)", rc); unlink(bas); return -1; }
    size_t n = fread(out, 1, cap - 1, r);
    out[n] = 0;
    fclose(r);
    /* togli la riga __RC__ finale */
    char* mark = strstr(out, "__RC__=");
    if (mark) *mark = 0;
    unlink(bas);
    unlink(capt);
    return 0;
}
