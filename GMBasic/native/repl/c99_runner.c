/* native/repl/c99_runner.c — speculare a C99DebugDriver.cs.
 * Esecuzione C99: Native Executor V2 -> cc + libgmb_runtime.a.
 */
#include "repl.h"
#include "executor.h"
#include "gmb_preprocess.h"

int rpl_run_c99(const char* bas_file, const rpl_options_t* o)
{
    if (o && o->defines_count > 0) {
        for (int i = 0; i < o->defines_count; i++) {
            const char* d = o->defines[i];
            const char* eq = strchr(d, '=');
            if (eq) { char name[128]; size_t nl = (size_t)(eq - d); if (nl >= sizeof(name)) nl = sizeof(name)-1; memcpy(name, d, nl); name[nl]=0; gmb_preprocess_define(name, eq+1); }
            else gmb_preprocess_define(d, "1");
        }
    }
    int rc = gmb_native_exec(bas_file, GMB_EXEC_C99);
    if (o && o->defines_count > 0) gmb_preprocess_reset();
    printf(rc == 0 ? "[C99 OK]\n" : "[C99 ERROR] exit %d\n", rc);
    return rc;
}
