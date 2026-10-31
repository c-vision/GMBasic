/* tool_registry.h — ToolRegistry per Universal Action Runtime (CUA §13, §14)
 * Registro dinamico dei tool (browser.*, computer.*, files.*, db.*, pdf.*)
 * con SEARCH per evitare prompt da 500 tool. Un file/dominio, <1200 righe. */
#ifndef GMB_TOOL_REGISTRY_H
#define GMB_TOOL_REGISTRY_H
#include <stddef.h>

#define GMB_TOOL_NAME_MAX 64
#define GMB_TOOL_DESC_MAX 256
#define GMB_TOOL_MAX 256

typedef struct {
    char name[GMB_TOOL_NAME_MAX];        // es. "browser.navigate"
    char description[GMB_TOOL_DESC_MAX]; // es. "Apre una pagina"
    char params_json[512];               // JSON schema dei parametri
    char provider[32];                   // es. "BrowserProvider"
    int risk;                            // 0=LOW ... 3=CRITICAL
} gmb_tool_manifest_t;

int gmb_tool_registry_register(const gmb_tool_manifest_t *manifest);
int gmb_tool_registry_unregister(const char *name);
const gmb_tool_manifest_t *gmb_tool_registry_get(const char *name);
int gmb_tool_registry_list(gmb_tool_manifest_t *out, int max);
int gmb_tool_registry_search(const char *query, gmb_tool_manifest_t *out, int max); // case-insensitive substring su name/description
int gmb_tool_registry_count(void);
void gmb_tool_registry_clear(void);

/* wrapper JSON-friendly (per VM/C builtin 1369/1370) */
int gmb_tool_registry_register_simple(const char *name, const char *desc, const char *provider);
int gmb_tool_registry_register_simple_risk(const char *name, const char *desc, const char *provider, int risk);
char *gmb_tool_registry_search_json(const char *query); /* heap, free() */

/* Executor del registry (seam C puro, parità IMcpToolDispatcher.DispatchAsync C#): l'host
 * (VM/C, embedding) registra la funzione che ESEGUE un tool del registry. Senza executor un
 * tool non-builtin produce un ERRORE ESPLICITO (mai no-op/stub). La firma ritorna 0 su
 * successo e riempie `out` (anche con un testo di errore del tool). */
typedef int (*gmb_tool_exec_fn)(const char *name, const char *args_json, char *out, size_t cap, void *ctx);
void gmb_tool_registry_set_executor(gmb_tool_exec_fn fn, void *ctx);
int gmb_tool_registry_has_executor(void);
/* Esegue un tool del registry via executor: 1 = eseguito (out riempito), 0 = tool non
 * registrato, -1 = nessun executor registrato. */
int gmb_tool_registry_execute(const char *name, const char *args_json, char *out, size_t cap);

/* semina ToolRegistry con i tool builtin noti (chiamato al boot) */
void gmb_tool_registry_seed_builtins(void);

#endif
