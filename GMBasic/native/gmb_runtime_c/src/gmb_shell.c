/* ============================================================================
 * gmb_shell.c — SHELL / SHELLOUT / SHELLCODE (SYSTEM.SHELL*)
 * Esecuzione di comandi di sistema per BASIC transpilato (VM/C, C99Native).
 * REGOLA §0bis: questa è l'UNICA implementazione nei backend C — i backend la CHIAMANO,
 * mai inline. Opt-in: gmb_shell_set_enabled(1) abilita (default disabilitato).
 * ============================================================================ */

#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define GMB_POPEN _popen
#define GMB_PCLOSE _pclose
#else
#define GMB_POPEN popen
#define GMB_PCLOSE pclose
#endif

static int g_gmb_shell_enabled = 0;

void gmb_shell_set_enabled(int enabled) { g_gmb_shell_enabled = enabled ? 1 : 0; }
int  gmb_shell_is_enabled(void) { return g_gmb_shell_enabled; }

/* Esegue `command` via popen, cattura stdout+stderr in un buffer allocato.
 * Ritorna la stringa (strdup) — da liberare col chiamante. */
static char* gmb_shell_capture(const char* command, int* exit_code_out) {
    if (exit_code_out) *exit_code_out = -1;
    if (!command || !*command) return strdup("");
    if (!g_gmb_shell_enabled) return NULL;

    FILE* fp = GMB_POPEN(command, "r");
    if (!fp) return NULL;

    size_t cap = 4096, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { GMB_PCLOSE(fp); return NULL; }
    char chunk[1024];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (len + n + 1 > cap) {
            cap = (len + n + 1) * 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); GMB_PCLOSE(fp); return NULL; }
            buf = nb;
        }
        memcpy(buf + len, chunk, n);
        len += n;
    }
    buf[len] = '\0';
    /* stderr su popen va sullo stesso pipe (2>&1 non usato: popen cattura solo stdout su alcuni
     * sistemi; aggiungiamo 2>&1 in modo esplicito nel chiamante se serve). */
    int rc = GMB_PCLOSE(fp);
    if (exit_code_out) *exit_code_out = rc;
    return buf;
}

/* SHELL: esegue e scarta l'output. Ritorna l'exit code (0 se ok, -1 se non abilitato/errore). */
int gmb_shell_exec(const char* command) {
    int rc = -1;
    char* out = gmb_shell_capture(command, &rc);
    if (out) free(out);               /* scarta l'output */
    else if (!g_gmb_shell_enabled) return -1;
    return rc == -1 ? 0 : rc;
}

/* SHELLOUT: esegue e ritorna stdout+stderr come stringa allocata (strdup). */
char* gmb_shell_out(const char* command) {
    if (!g_gmb_shell_enabled) return NULL;
    int rc = 0;
    char* out = gmb_shell_capture(command, &rc);
    return out ? out : strdup("");
}

/* SHELLCODE: esegue e ritorna l'exit code. */
int gmb_shell_code(const char* command) {
    int rc = -1;
    char* out = gmb_shell_capture(command, &rc);
    if (out) free(out);
    return g_gmb_shell_enabled ? rc : -1;
}

/* RUN "comando" — statement del LINGUAGGIO (non la funzione SYSTEM.SHELL): esegue il comando
 * con stdout/stderr EREDITATI dal processo, esattamente come IlRuntime.RunCommand sulla gamba
 * IL, quindi NON richiede l'opt-in SET SHELL ON. Ritorna l'exit code (0 se comando vuoto).
 * Implementazione unica per i backend C (§0bis): VM/C la chiama, mai inline. */
int gmb_shell_run(const char* command) {
    if (!command || !*command) return 0;
    /* Ordine dell'output: il figlio scrive DIRETTAMENTE sullo stdout del processo. Senza
     * questo flush il buffer stdio del padre (i PRINT precedenti) verrebbe emesso DOPO
     * l'output del comando, mentre IL (Console .NET, auto-flush) lo emette prima:
     * divergenza visibile in samples/Commands/RunChainLoad.bas (IL/C99 "Eseguo..." poi
     * "Ciao dal chain-load", PCode/VM-C invertiti). */
    fflush(stdout);
    fflush(stderr);
    return system(command);
}
