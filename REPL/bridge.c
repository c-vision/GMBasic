/* native/repl/bridge.c — comandi TOOL ausiliari (bsp/convert/package/ml/providers/aipath).
 * Il ponte C# ReplNativeCompilerBridge è stato ELIMINATO: questi tool non sono ancora
 * portati in C e restituiscono un errore di divergenza esplicito (nessun lancio C#).
 * Il percorso di compilazione/esecuzione (COMPILE/RUN vmc) è 100% C: gmb_compile -> chunk -> gmb_vm_host.
 */
#include "repl.h"
#include <stdio.h>

int rpl_bridge_cmd(const char* cmd, const char* arg1)
{
    (void)arg1;
    fprintf(stderr, "?tool '%s' non ancora disponibile nel percorso C (divergenza: era via ponte C# ReplNativeCompilerBridge, rimosso).\n", cmd ? cmd : "?");
    return 1;
}
