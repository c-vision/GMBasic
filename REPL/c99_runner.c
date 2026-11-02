/* native/repl/c99_runner.c — speculare a C99DebugDriver.cs.
 * Esecuzione C99: Native Executor V2 -> cc + libgmb_runtime.a.
 */
#include "repl.h"

int rpl_run_c99(const char* bas_file, const rpl_options_t* o)
{
    /* Esecuzione C99: la fa il CLI del compilatore (`gmb run` = emette C, compila, esegue).
     * La REPL non linka più l'executor: nessuna compilazione in-process (F2b). */
    char cli[2048];
    if (!rpl_gmb_cli(cli, sizeof(cli))) { fprintf(stderr, "?gmb (CLI compilatore C) non trovato\n"); return 1; }
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s", bas_file);
    char* slash = strrchr(dir, '/');
    if (slash) *slash = 0; else snprintf(dir, sizeof(dir), ".");
    char defs[1024] = "";
    if (o) for (int i = 0; i < o->defines_count; i++) {
        size_t used = strlen(defs); snprintf(defs + used, sizeof(defs) - used, " -D \"%s\"", o->defines[i]);
    }
    char gpm[64] = "";
    if (o && o->gpm_mode && *o->gpm_mode) snprintf(gpm, sizeof(gpm), " --gpm %s", o->gpm_mode);
    /* -full con l'engine C = distribuzione: `--alllibs` (tutte le lib usate accanto all'exe). */
    const char* alllibs = (o && o->full) ? " --alllibs" : "";
    char cmd[6144];
    snprintf(cmd, sizeof(cmd), "\"%s\" run --project \"%s\" --entry \"%s\" --quiet%s%s%s", cli, dir, bas_file, gpm, defs, alllibs);
    int rc = system(cmd);
    rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
    if (rc != 0) printf("[C99 ERROR] exit %d\n", rc);
    return rc;
}
