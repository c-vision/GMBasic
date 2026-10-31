/* native/executor/executor.h — Native Executor V2 (C): vmc|c99, mai IL.
 * Usato da ReplNative (C). Esecuzione e compilazione 100% C: vmc -> compilatore P-Code C
 * (gmb_compile) -> chunk A1/GMBC -> gmb_vm_host; nessun ponte C#.
 */
#ifndef GMB_EXECUTOR_NATIVE_H
#define GMB_EXECUTOR_NATIVE_H

typedef enum { GMB_EXEC_VMC, GMB_EXEC_C99 } gmb_exec_backend_t;

/* Esegue un file .bas sul backend nativo. Ritorna exit code (0 ok). */
int gmb_native_exec(const char* bas_file, gmb_exec_backend_t backend);

/* Compila .bas -> chunk via pipeline C (vmc). c99: generazione sorgente ancora C# (divergenza). */
int gmb_native_compile(const char* bas_file, const char* out_path, gmb_exec_backend_t backend);

/* Risolve il percorso del binario gmb_vm_host (host della VM/C): env GMBASIC_VM_HOST ->
 * GMBASIC_REPO_ROOT -> repo root. NULL se non trovato. Unica sorgente (executor.c). */
const char* gmb_resolve_vm_host(void);

#endif
