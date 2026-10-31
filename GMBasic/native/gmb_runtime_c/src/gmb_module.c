/* gmb_module.c — REGISTRY di moduli esterni (model compiler, §4.3 "librerie").
 *
 * Un MODULO è una dylib nativa che si registra su uno SLOT (una capacità: es. "kernel.qmv",
 * "quant.emitter", "pass.prune"). Più dylib possono registrarsi sullo stesso slot a priorità
 * diverse: la PRIORITÀ più alta vince (replace), a parità vince l'ultima registrata (enrich).
 * Il registry risolve per slot -> dylib + simbolo via dlopen/dlsym (gmb_ffi_dl, sorgente unica).
 *
 * Vive nel runtime C condiviso (§0bis): VM/C, C99Native lo usano allo stesso modo.
 * NIENTE env (§regola AGENTS): gli slot/priorità si passano dai comandi (`GMB MODULE ...`).
 */
#include "gmb_module.h"
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* gmb_ffi_dl_open/sym/preload: sorgente unica del caricamento dinamico (gmb_runtime.h). */

#define GMB_MODULE_MAX 64
typedef struct {
    char slot[96];
    char dylib[1024];
    int  priority;
    int  seq;          /* ordine di registrazione (a parità di priorità vince l'ultima) */
} gmb_module_t;

static gmb_module_t s_mods[GMB_MODULE_MAX];
static int s_mods_n = 0;
static int s_mods_seq = 0;

/* Registra/aggiorna uno slot. Ritorna 0/-1. */
int gmb_module_register(const char* slot, const char* dylib, int priority) {
    if (!slot || !*slot || !dylib || !*dylib) return -1;
    gmb_ffi_dl_preload(dylib);   /* dlopen subito: gli errori emergono qui, non al primo uso */
    for (int i = 0; i < s_mods_n; i++)
        if (strcmp(s_mods[i].slot, slot) == 0 && strcmp(s_mods[i].dylib, dylib) == 0) {
            s_mods[i].priority = priority; s_mods[i].seq = s_mods_seq++;
            return 0;
        }
    if (s_mods_n >= GMB_MODULE_MAX) return -1;
    snprintf(s_mods[s_mods_n].slot, sizeof s_mods[s_mods_n].slot, "%s", slot);
    snprintf(s_mods[s_mods_n].dylib, sizeof s_mods[s_mods_n].dylib, "%s", dylib);
    s_mods[s_mods_n].priority = priority;
    s_mods[s_mods_n].seq = s_mods_seq++;
    s_mods_n++;
    return 0;
}

/* Cambia la priorità della dylib già registrata su uno slot. Ritorna 0/-1. */
int gmb_module_priority(const char* slot, const char* dylib, int priority) {
    if (!slot || !*slot) return -1;
    for (int i = 0; i < s_mods_n; i++)
        if (strcmp(s_mods[i].slot, slot) == 0 && (!dylib || !*dylib || strcmp(s_mods[i].dylib, dylib) == 0)) {
            s_mods[i].priority = priority; return 0;
        }
    return -1;
}

/* La dylib VINCENTE per lo slot (priorità più alta, a parità l'ultima registrata). NULL se nessuna. */
const char* gmb_module_resolve(const char* slot) {
    if (!slot || !*slot) return NULL;
    int best = -1;
    for (int i = 0; i < s_mods_n; i++)
        if (strcmp(s_mods[i].slot, slot) == 0)
            if (best < 0 || s_mods[i].priority > s_mods[best].priority ||
                (s_mods[i].priority == s_mods[best].priority && s_mods[i].seq > s_mods[best].seq))
                best = i;
    return best < 0 ? NULL : s_mods[best].dylib;
}

/* Simbolo dalla dylib VINCENTE dello slot. NULL se lo slot/simbolo non risolve. */
void* gmb_module_sym(const char* slot, const char* symbol) {
    const char* d = gmb_module_resolve(slot);
    if (!d || !symbol || !*symbol) return NULL;
    void* h = gmb_ffi_dl_open(d);
    return h ? gmb_ffi_dl_sym(h, symbol) : NULL;
}

/* Elenca i moduli registrati, uno per riga: "slot\tdylib\tpriority". Ritorna il numero. */
int gmb_module_list(char* out, size_t cap) {
    if (!out || !cap) return -1;
    size_t o = 0;
    for (int i = 0; i < s_mods_n && o < cap; i++)
        o += (size_t)snprintf(out + o, cap - o, "%s\t%s\t%d\n", s_mods[i].slot, s_mods[i].dylib, s_mods[i].priority);
    return s_mods_n;
}

/* Svuota il registry (test/riproduzione). */
void gmb_module_reset(void) { s_mods_n = 0; s_mods_seq = 0; }


/* Chiama un simbolo esportato dalla dylib vincente dello slot. `packed` = "simbolo|sig|argsCsv":
 *   sig = prima lettera = tipo di ritorno, le altre = tipi dei parametri ('d'=double, 'i'=int64).
 *   argsCsv = argomenti separati da virgola (numerici). Risultato formattato in `out`.
 * Il managed world (IL/PCode) usa la stessa forma via P/Invoke (C#->C, §REGOLA PRIMARIA). */
int gmb_module_call_to(const char* slot, const char* packed, char* out, size_t cap) {
    if (!out || !cap) return -1;
    out[0] = 0;
    if (!packed) packed = "";
    char sym[128] = "", sig[16] = "", args[256] = "";
    const char* p1 = strchr(packed, '|');
    if (!p1) { snprintf(out, cap, "GMB MODULE CALL: atteso \"simbolo|sig|args\"\n"); return -1; }
    size_t ls = (size_t)(p1 - packed); if (ls > sizeof(sym) - 1) ls = sizeof(sym) - 1;
    memcpy(sym, packed, ls); sym[ls] = 0;
    const char* p2 = strchr(p1 + 1, '|');
    if (!p2) { snprintf(out, cap, "GMB MODULE CALL: manca la firma\n"); return -1; }
    size_t lg = (size_t)(p2 - (p1 + 1)); if (lg > sizeof(sig) - 1) lg = sizeof(sig) - 1;
    memcpy(sig, p1 + 1, lg); sig[lg] = 0;
    snprintf(args, sizeof args, "%s", p2 + 1);
    void* fp = gmb_module_sym(slot, sym);
    if (!fp) { snprintf(out, cap, "GMB MODULE CALL: simbolo '%s' non risolto\n", sym); return -1; }
    double d[8] = {0}; long long i[8] = {0}; int n = 0;
    for (char* tok = strtok(args, ","); tok && n < 8; tok = strtok(NULL, ",")) { d[n] = atof(tok); i[n] = atoll(tok); n++; }
    (void)i[0]; (void)d[0];
    if (strcmp(sig, "dd") == 0)  { double (*f)(double) = (double(*)(double))fp; snprintf(out, cap, "%.10g\n", f(d[0])); }
    else if (strcmp(sig, "ddd") == 0) { double (*f)(double,double) = (double(*)(double,double))fp; snprintf(out, cap, "%.10g\n", f(d[0], d[1])); }
    else if (strcmp(sig, "dddd") == 0) { double (*f)(double,double,double) = (double(*)(double,double,double))fp; snprintf(out, cap, "%.10g\n", f(d[0], d[1], d[2])); }
    else if (strcmp(sig, "ii") == 0)  { long long (*f)(long long) = (long long(*)(long long))fp; snprintf(out, cap, "%lld\n", f(i[0])); }
    else if (strcmp(sig, "iii") == 0) { long long (*f)(long long,long long) = (long long(*)(long long,long long))fp; snprintf(out, cap, "%lld\n", f(i[0], i[1])); }
    else if (strcmp(sig, "iiii") == 0) { long long (*f)(long long,long long,long long) = (long long(*)(long long,long long,long long))fp; snprintf(out, cap, "%lld\n", f(i[0], i[1], i[2])); }
    else { snprintf(out, cap, "GMB MODULE CALL: firma '%s' non supportata (dd|ddd|dddd|ii|iii|iiii)\n", sig); return -1; }
    return 0;
}

/* Dispatcher unico (sub, a, b) -> report in `out`: REGISTER|PRIORITY|LIST|SYM|CALL. Sorgente condivisa
 * da C99Native/VM-C. Ritorna 0/-1 (l'output va SEMPRE stampato dal chiamante). */
int gmb_module_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap) {
    if (!out || !cap) return -1;
    out[0] = 0;
    if (!sub) sub = "";
    if (strcasecmp(sub, "REGISTER") == 0) { if (gmb_module_register(a, b, 0) != 0) { snprintf(out, cap, "GMB MODULE REGISTER: fallito\n"); return -1; } return 0; }
    if (strcasecmp(sub, "PRIORITY") == 0) { if (gmb_module_priority(a, "", b ? atoi(b) : 0) != 0) { snprintf(out, cap, "GMB MODULE PRIORITY: fallito\n"); return -1; } return 0; }
    if (strcasecmp(sub, "LIST") == 0) { gmb_module_list(out, cap); return 0; }
    if (strcasecmp(sub, "SYM") == 0) { if (!gmb_module_sym(a, b)) { snprintf(out, cap, "GMB MODULE SYM: non risolto\n"); return -1; } return 0; }
    if (strcasecmp(sub, "CALL") == 0) return gmb_module_call_to(a, b, out, cap);
    snprintf(out, cap, "GMB MODULE: atteso REGISTER|PRIORITY|LIST|SYM|CALL\n"); return -1;
}
