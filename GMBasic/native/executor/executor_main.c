/* native/executor/executor_main.c — driver del Native Executor V2 (vmc|c99). */
#include "executor.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    const char* backend = "vmc";
    const char* file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) { backend = argv[++i]; continue; }
        if (strncmp(argv[i], "--backend=", 10) == 0) { backend = argv[i] + 10; continue; }
        file = argv[i];
    }
    if (!file) { fprintf(stderr, "uso: gmb_executor_native_host [--backend vmc|c99] <file.bas>\n"); return 2; }
    gmb_exec_backend_t be;
    if (strncmp(backend, "vm", 2) == 0) be = GMB_EXEC_VMC;
    else if (strcmp(backend, "c99") == 0) be = GMB_EXEC_C99;
    else { fprintf(stderr, "?backend sconosciuto: %s (vmc|c99)\n", backend); return 2; }
    return gmb_native_exec(file, be);
}
