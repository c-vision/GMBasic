/* native/repl/orchestrator.c — speculare a Orchestrator.cs: compilazione .bas -> chunk.
 * Percorso 100% C: lexer -> macro expand -> parser -> linker -> compilatore P-Code C
 * (gmb_compile) -> chunk A1/GMBC -> gmb_vm_host. Nessun ponte C#.
 */
#include "repl.h"
#include "executor.h"
#include "gmb_preprocess.h"

int rpl_compile_csharp(const char* bas_file, const char* out_path, rpl_backend_t be, const rpl_options_t* o)
{
    if (o && o->defines_count > 0) {
        for (int i = 0; i < o->defines_count; i++) {
            const char* d = o->defines[i];
            const char* eq = strchr(d, '=');
            if (eq) {
                char name[128]; size_t nl = (size_t)(eq - d);
                if (nl >= sizeof(name)) nl = sizeof(name)-1;
                memcpy(name, d, nl); name[nl]=0;
                gmb_preprocess_define(name, eq+1);
            } else {
                gmb_preprocess_define(d, "1");
            }
        }
    }
    int rc = gmb_native_compile(bas_file, out_path, be == RPL_VMC ? GMB_EXEC_VMC : GMB_EXEC_C99);
    if (o && o->defines_count > 0) gmb_preprocess_reset();
    return rc;
}
