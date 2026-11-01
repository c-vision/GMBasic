/* native/repl/vmc_runner.c — speculare a GmVmHostRunner.cs/VmCDebugDriver.cs.
 * Esecuzione VMC: Native Executor V2 -> gmb_vm_host (chunk A1/GMBC, processo esterno).
 */
#include "repl.h"
#include "executor.h"
#include "gmb_preprocess.h"
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int rpl_run_vmc(const char* bas_file, const rpl_options_t* o)
{
    if (o && o->defines_count > 0) {
        for (int i = 0; i < o->defines_count; i++) {
            const char* d = o->defines[i];
            const char* eq = strchr(d, '=');
            if (eq) { char name[128]; size_t nl = (size_t)(eq - d); if (nl >= sizeof(name)) nl = sizeof(name)-1; memcpy(name, d, nl); name[nl]=0; gmb_preprocess_define(name, eq+1); }
            else gmb_preprocess_define(d, "1");
        }
    }
    extern gmb_debug_session_t g_dbg;
    int use_debug = (g_dbg.mode != 0 || g_dbg.break_count > 0);
    int rc;
    if (use_debug) {
        // Debug: passa i breakpoint e single_step al VM host via --debug --break <line> --single-step
        char dbg_args[2048] = "";
        for (int i = 0; i < g_dbg.break_count; i++) {
            char tmp[64]; snprintf(tmp, sizeof(tmp), " --break %d", g_dbg.breaks[i].line);
            strncat(dbg_args, tmp, sizeof(dbg_args)-strlen(dbg_args)-1);
        }
        if (g_dbg.mode == 2) strncat(dbg_args, " --single-step", sizeof(dbg_args)-strlen(dbg_args)-1); // GMB_DBG_STEPPING = 2
        // WATCH: per ora stub con g_dbg.current_line, il VM stamperà i watch alla pausa
        char chunk[2048];
        snprintf(chunk, sizeof(chunk), "/tmp/gmb_exec_%ld.chunk", (long)getpid());
        { if (rpl_gmb_dump("chunk", bas_file, chunk) != 0) { if (o && o->defines_count > 0) gmb_preprocess_reset(); return 2; } }
        char host[2048]; 
        /* Unica risoluzione del binario gmb_vm_host (executor.c): env GMBASIC_VM_HOST ->
         * GMBASIC_REPO_ROOT -> repo root. Prima esisteva solo come `extern` mai definito. */
        const char* h = gmb_resolve_vm_host();
        snprintf(host, sizeof(host), "%s", h ? h : "native/gmb_vm_c/build/gmb_vm_host");
        char cmd[8192];
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" --debug%s", host, chunk, dbg_args);
        rc = system(cmd);
        rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
        unlink(chunk);
        // Dopo il debug, mostra i watch (lettura variabili via g_dbg.watches — per ora stub con g_dbg.current_line)
        for (int i = 0; i < g_dbg.watch_count; i++) {
            printf("[WATCH] %s @ line %d\n", g_dbg.watches[i].name, g_dbg.breaks[0].line);
        }
    } else {
        { char chunk2[2048];
          snprintf(chunk2, sizeof(chunk2), "/tmp/gmb_run_%ld.chunk", (long)getpid());
          if (rpl_gmb_dump("chunk", bas_file, chunk2) != 0) rc = 2;
          else {
              const char* h = gmb_resolve_vm_host();
              char host2[2048];
              snprintf(host2, sizeof(host2), "%s", h ? h : "native/gmb_vm_c/build/gmb_vm_host");
              char cmd2[4096];
              snprintf(cmd2, sizeof(cmd2), "\"%s\" \"%s\"", host2, chunk2);
              int r2 = system(cmd2);
              rc = WIFEXITED(r2) ? WEXITSTATUS(r2) : 1;
              unlink(chunk2);
          } }
    }
    if (o && o->defines_count > 0) gmb_preprocess_reset();
    if (rc != 0) printf("[VMC ERROR] exit %d\n", rc);
    return rc;
}
