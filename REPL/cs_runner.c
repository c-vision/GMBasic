/* native/repl/cs_runner.c — esecuzione sull'engine C# opzionale (`gmbcs`).
 *
 * RUN con `--engine=csharp`:
 *   - senza debug: `gmbcs run <file> --engine pcode|il` (vmc|c99, come il ramo C);
 *   - con debug (BREAK/STEP): `gmbcs debug <file> --break N… [--step]` (debugger P-Code,
 *     comandi su stdin, stdio ereditato: identico schema di `gmb_vm_host --debug`).
 *
 * Il mondo C# è un PROCESSO: la REPL non linka nulla di C# (F5).
 */
#include "repl.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int rpl_run_cs(const char* bas_file, const rpl_options_t* o)
{
    extern gmb_debug_session_t g_dbg;
    int use_debug = (g_dbg.mode != GMB_DBG_OFF || g_dbg.break_count > 0);

    if (!use_debug) {
        /* -n/-m scelgono l'engine (native AOT / IL managed); default: vmc→P-Code, c99→IL. */
        const char* extra = o->native_backend ? " --engine native"
                          : o->managed_backend ? " --engine il"
                          : (o->backend == RPL_VMC) ? " --engine pcode" : " --engine il";
        /* -full: prima materializza la distribuzione managed completa (runtime copiato). */
        if (o->full) {
            int brc = rpl_cs_cmd("build", bas_file, " --target il --full", o);
            if (brc != 0) printf("[CS ERROR] build -full exit %d\n", brc);
        }
        int rc = rpl_cs_cmd("run", bas_file, extra, o);
        if (rc != 0) printf("[CS ERROR] exit %d\n", rc);
        /* --dump-il: oltre a eseguire, produce il dump IL (comportamento del vecchio REPL) */
        if (o->dump_il) rpl_cs_cmd("dump il", bas_file, NULL, o);
        return rc;
    }

    /* Debug: il debugger dell'engine C# è P-Code (il backend IL non espone lo step). */
    char extra[2048] = "";
    for (int i = 0; i < g_dbg.break_count; i++) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), " --break %d", g_dbg.breaks[i].line);
        strncat(extra, tmp, sizeof(extra) - strlen(extra) - 1);
    }
    if (g_dbg.mode == GMB_DBG_STEPPING)
        strncat(extra, " --step", sizeof(extra) - strlen(extra) - 1);
    int rc = rpl_cs_cmd("debug", bas_file, extra, o);
    if (rc != 0) printf("[CS ERROR] exit %d\n", rc);
    return rc;
}
