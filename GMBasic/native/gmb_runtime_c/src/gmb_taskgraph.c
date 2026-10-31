/* gmb_taskgraph.c — Registro di task con dipendenze (DAG) e esecuzione topologica (mondo C).
 *
 * Orchestrazione agenti (B1): espone al BASIC il Task Graph riusando il runtime agent esistente
 * (gmb_agent_ask). Ogni nodo con agente chiama l'agente (prompt = desc + risultati delle
 * dipendenze); senza agente il risultato è la descrizione (passthrough, testabile senza LLM).
 * Esecuzione SEQUENZIALE in ordine topologico (GPM-safe; il parallelismo bounded via gmb_dag_run
 * è un follow-on). Parita' di comportamento con Basic.Agents.TaskGraphRegistry (C#): stesse
 * operazioni, stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis), thread-safe (mutex).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_TASK_MAX 64
#define GMB_TASK_ID_MAX 64
#define GMB_TASK_DESC_MAX 512
#define GMB_TASK_AGENT_MAX 64
#define GMB_TASK_DEPS_MAX 16
#define GMB_TASK_RESULT_MAX 2048

typedef struct {
    char id[GMB_TASK_ID_MAX];
    char desc[GMB_TASK_DESC_MAX];
    char agent[GMB_TASK_AGENT_MAX];
    char deps[GMB_TASK_DEPS_MAX][GMB_TASK_ID_MAX];
    int ndeps;
    char status[16];
    char result[GMB_TASK_RESULT_MAX];
} gmb_tgnode_t;

static gmb_tgnode_t g_tg_nodes[GMB_TASK_MAX];
static int g_tg_count = 0;
static pthread_mutex_t g_tg_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_task_find_locked(const char* id)
{
    for (int i = 0; i < g_tg_count; i++)
        if (strcasecmp(g_tg_nodes[i].id, id) == 0) return i;
    return -1;
}

/* Definisce un task (id unico). Ridefinire resetta desc/stato. 1 = ok, 0 = id vuoto/pieno. */
int gmb_task_define(const char* id, const char* desc)
{
    if (!id || !*id) return 0;
    pthread_mutex_lock(&g_tg_lock);
    int idx = gmb_task_find_locked(id);
    if (idx < 0)
    {
        if (g_tg_count >= GMB_TASK_MAX) { pthread_mutex_unlock(&g_tg_lock); return 0; }
        idx = g_tg_count++;
    }
    memset(&g_tg_nodes[idx], 0, sizeof(g_tg_nodes[idx]));
    snprintf(g_tg_nodes[idx].id, sizeof(g_tg_nodes[idx].id), "%s", id);
    snprintf(g_tg_nodes[idx].desc, sizeof(g_tg_nodes[idx].desc), "%s", desc ? desc : "");
    snprintf(g_tg_nodes[idx].status, sizeof(g_tg_nodes[idx].status), "pending");
    pthread_mutex_unlock(&g_tg_lock);
    return 1;
}

/* Aggiunge una dipendenza al task id. 1 = ok, 0 = task/dep inesistente o già presente. */
int gmb_task_depends(const char* id, const char* dep)
{
    if (!id || !*id || !dep || !*dep) return 0;
    pthread_mutex_lock(&g_tg_lock);
    int idx = gmb_task_find_locked(id);
    if (idx < 0 || gmb_task_find_locked(dep) < 0) { pthread_mutex_unlock(&g_tg_lock); return 0; }
    for (int i = 0; i < g_tg_nodes[idx].ndeps; i++)
        if (strcasecmp(g_tg_nodes[idx].deps[i], dep) == 0) { pthread_mutex_unlock(&g_tg_lock); return 0; }
    if (g_tg_nodes[idx].ndeps >= GMB_TASK_DEPS_MAX) { pthread_mutex_unlock(&g_tg_lock); return 0; }
    snprintf(g_tg_nodes[idx].deps[g_tg_nodes[idx].ndeps++], GMB_TASK_ID_MAX, "%s", dep);
    pthread_mutex_unlock(&g_tg_lock);
    return 1;
}

/* Assegna un agente al task. 1 = ok, 0 = task inesistente. */
int gmb_task_assign(const char* id, const char* agent)
{
    if (!id || !*id) return 0;
    pthread_mutex_lock(&g_tg_lock);
    int idx = gmb_task_find_locked(id);
    if (idx < 0) { pthread_mutex_unlock(&g_tg_lock); return 0; }
    snprintf(g_tg_nodes[idx].agent, sizeof(g_tg_nodes[idx].agent), "%s", agent ? agent : "");
    pthread_mutex_unlock(&g_tg_lock);
    return 1;
}

static void gmb_task_escape(const char* s, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = s ? s : ""; *p && o + 2 < cap; p++)
    {
        char c = *p;
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else out[o++] = c;
    }
    out[o] = '\0';
}

/* JSON di un nodo. Richiede il lock tenuto dal chiamante. */
static void gmb_task_node_json_locked(const gmb_tgnode_t* n, char* out, size_t cap)
{
    char eid[GMB_TASK_ID_MAX * 2], edesc[GMB_TASK_DESC_MAX * 2];
    char eagent[GMB_TASK_AGENT_MAX * 2], estatus[64], eres[GMB_TASK_RESULT_MAX * 2];
    gmb_task_escape(n->id, eid, sizeof(eid));
    gmb_task_escape(n->desc, edesc, sizeof(edesc));
    gmb_task_escape(n->agent, eagent, sizeof(eagent));
    gmb_task_escape(n->status, estatus, sizeof(estatus));
    gmb_task_escape(n->result, eres, sizeof(eres));
    size_t off = 0;
    off += (size_t)snprintf(out + off, cap - off,
        "{\"id\":\"%s\",\"desc\":\"%s\",\"agent\":\"%s\",\"status\":\"%s\",\"result\":\"%s\",\"deps\":[",
        eid, edesc, eagent, estatus, eres);
    for (int i = 0; i < n->ndeps && off + 8 < cap; i++)
    {
        char ed[GMB_TASK_ID_MAX * 2];
        gmb_task_escape(n->deps[i], ed, sizeof(ed));
        off += (size_t)snprintf(out + off, cap - off, "%s\"%s\"", i > 0 ? "," : "", ed);
    }
    snprintf(out + off, cap - off, "]}");
}

/* JSON dello stato del task ({} se assente). malloc. */
/* Prossimo task lavorabile dal MANAGER loop: primo `pending` in ordine alfabetico di id con TUTTE
 * le dipendenze `done`. Scrive id/desc nei buffer dati. Ritorna 1 se c'è un task lavorabile,
 * 0 se non c'è (con *deadlock=0: nessun task pendente), 0 con *deadlock=1 (pendenti ma bloccati).
 * Speculare a TaskGraphRegistry.NextPending (C#). */
int gmb_task_next_pending(char* id_out, size_t id_cap, char* desc_out, size_t desc_cap, int* deadlock)
{
    return gmb_task_next_pending_filtered(NULL, id_out, id_cap, desc_out, desc_cap, deadlock);
}

/* Come gmb_task_next_pending ma limitato ai soli id presenti in `only_csv` (NULL/vuoto = tutti):
 * è lo SCOPING del manager loop su un piano. Speculare al filtro di TaskGraphRegistry.NextPending. */
static int gmb_task_id_in_csv(const char* csv, const char* id)
{
    if (!csv || !*csv) return 1;
    size_t idlen = strlen(id);
    const char* p = csv;
    while (*p) {
        const char* comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        while (len && (*p == ' ' || *p == '\t')) { p++; len--; }
        while (len && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
        if (len == idlen && strncasecmp(p, id, len) == 0) return 1;
        if (!comma) break;
        p = comma + 1;
    }
    return 0;
}

int gmb_task_next_pending_filtered(const char* only_csv, char* id_out, size_t id_cap, char* desc_out, size_t desc_cap, int* deadlock)
{
    if (deadlock) *deadlock = 0;
    if (id_out && id_cap) id_out[0] = 0;
    if (desc_out && desc_cap) desc_out[0] = 0;
    pthread_mutex_lock(&g_tg_lock);
    int pending = 0;
    /* ordine alfabetico deterministico: scansione con selezione del minimo id valido */
    int best = -1;
    for (int i = 0; i < g_tg_count; i++) {
        if (strcasecmp(g_tg_nodes[i].status, "pending") != 0) continue;
        if (!gmb_task_id_in_csv(only_csv, g_tg_nodes[i].id)) continue;
        pending++;
        int ready = 1;
        for (int d = 0; d < g_tg_nodes[i].ndeps; d++) {
            int di = gmb_task_find_locked(g_tg_nodes[i].deps[d]);
            if (di < 0 || strcasecmp(g_tg_nodes[di].status, "done") != 0) { ready = 0; break; }
        }
        if (!ready) continue;
        if (best < 0 || strcasecmp(g_tg_nodes[i].id, g_tg_nodes[best].id) < 0) best = i;
    }
    if (best >= 0) {
        if (id_out && id_cap) snprintf(id_out, id_cap, "%s", g_tg_nodes[best].id);
        if (desc_out && desc_cap) snprintf(desc_out, desc_cap, "%s", g_tg_nodes[best].desc);
        pthread_mutex_unlock(&g_tg_lock);
        return 1;
    }
    if (deadlock) *deadlock = pending > 0 ? 1 : 0;
    pthread_mutex_unlock(&g_tg_lock);
    return 0;
}

/* Stato/risultato di un task: 1 = ok. `status` = "done"|"pending"|… (speculare a SetDone/SetDissented). */
int gmb_task_set_status(const char* id, const char* status, const char* result)
{
    if (!id || !*id) return 0;
    pthread_mutex_lock(&g_tg_lock);
    int idx = gmb_task_find_locked(id);
    if (idx < 0) { pthread_mutex_unlock(&g_tg_lock); return 0; }
    snprintf(g_tg_nodes[idx].status, sizeof(g_tg_nodes[idx].status), "%s", status ? status : "pending");
    snprintf(g_tg_nodes[idx].result, sizeof(g_tg_nodes[idx].result), "%s", result ? result : "");
    pthread_mutex_unlock(&g_tg_lock);
    return 1;
}

char* gmb_task_status(const char* id)
{
    char* buf = (char*)malloc(GMB_TASK_DESC_MAX * 2 + GMB_TASK_RESULT_MAX * 2 + 512);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_tg_lock);
    int idx = (id && *id) ? gmb_task_find_locked(id) : -1;
    if (idx < 0) snprintf(buf, 3, "{}");
    else gmb_task_node_json_locked(&g_tg_nodes[idx], buf, GMB_TASK_DESC_MAX * 2 + GMB_TASK_RESULT_MAX * 2 + 512);
    pthread_mutex_unlock(&g_tg_lock);
    return buf;
}

/* JSON di tutti i task, ordinati per id. malloc. */
char* gmb_task_list(void)
{
    size_t cap = 64 + (size_t)GMB_TASK_MAX * (GMB_TASK_DESC_MAX * 2 + GMB_TASK_RESULT_MAX * 2 + 512);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_tg_lock);
    int order[GMB_TASK_MAX];
    int n = g_tg_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_tg_nodes[order[i]].id, g_tg_nodes[order[j]].id) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "{\"tasks\":[");
    for (int k = 0; k < n; k++)
    {
        if (k > 0) off += (size_t)snprintf(buf + off, cap - off, ",");
        gmb_task_node_json_locked(&g_tg_nodes[order[k]], buf + off, cap - off);
        off = strlen(buf);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_tg_lock);
    return buf;
}

/* Esegue il DAG in ordine topologico (sequenziale, GPM-safe). malloc JSON finale. */
char* gmb_task_run(void)
{
    pthread_mutex_lock(&g_tg_lock);
    for (int i = 0; i < g_tg_count; i++)
    { snprintf(g_tg_nodes[i].status, sizeof(g_tg_nodes[i].status), "pending"); g_tg_nodes[i].result[0] = '\0'; }

    /* Validazione dipendenze. */
    for (int i = 0; i < g_tg_count; i++)
        for (int d = 0; d < g_tg_nodes[i].ndeps; d++)
            if (gmb_task_find_locked(g_tg_nodes[i].deps[d]) < 0)
            {
                char* buf = (char*)malloc(256);
                if (buf) snprintf(buf, 256, "{\"error\":\"dipendenza inesistente\"}");
                pthread_mutex_unlock(&g_tg_lock);
                return buf;
            }

    /* Kahn (indici). */
    int indeg[GMB_TASK_MAX];
    for (int i = 0; i < g_tg_count; i++) indeg[i] = g_tg_nodes[i].ndeps;
    int order[GMB_TASK_MAX]; int norder = 0;
    for (int i = 0; i < g_tg_count; i++) if (indeg[i] == 0) order[norder++] = i;
    for (int q = 0; q < norder; q++)
        for (int i = 0; i < g_tg_count; i++)
            for (int d = 0; d < g_tg_nodes[i].ndeps; d++)
                if (strcasecmp(g_tg_nodes[i].deps[d], g_tg_nodes[order[q]].id) == 0)
                    if (--indeg[i] == 0) order[norder++] = i;
    if (norder != g_tg_count)
    {
        char* buf = (char*)malloc(64);
        if (buf) snprintf(buf, 64, "{\"error\":\"ciclo nel DAG\"}");
        pthread_mutex_unlock(&g_tg_lock);
        return buf;
    }

    /* Esecuzione sequenziale. */
    for (int k = 0; k < norder; k++)
    {
        gmb_tgnode_t* n = &g_tg_nodes[order[k]];
        char prompt[GMB_TASK_DESC_MAX + GMB_TASK_DEPS_MAX * GMB_TASK_RESULT_MAX];
        size_t off = (size_t)snprintf(prompt, sizeof(prompt), "%s", n->desc);
        for (int d = 0; d < n->ndeps; d++)
        {
            int di = gmb_task_find_locked(n->deps[d]);
            if (di >= 0 && off + 2 < sizeof(prompt))
                off += (size_t)snprintf(prompt + off, sizeof(prompt) - off, "\n%s", g_tg_nodes[di].result);
        }
        if (!n->agent[0])
        {
            snprintf(n->result, sizeof(n->result), "%s", n->desc);
            snprintf(n->status, sizeof(n->status), "done");
        }
        else if (!gmb_agent_exists(n->agent))
        {
            /* Parità C# TaskGraphRegistry (2026-09-23, Parita-5-Backend-Residui §2 —
             * AiTaskGraph): agente non registrato = failed con "Error: Agent 'x' is not
             * registered." (non esecuzione mock). Prima eseguiva comunque (mock fixture)
             * → done con "[] (insufficient context)". */
            snprintf(n->result, sizeof(n->result), "Error: Agent '%s' is not registered.", n->agent);
            snprintf(n->status, sizeof(n->status), "failed");
        }
        else
        {
            char* res = gmb_agent_ask(n->agent, prompt);
            if (res) { snprintf(n->result, sizeof(n->result), "%s", res); free(res); snprintf(n->status, sizeof(n->status), "done"); }
            else { snprintf(n->result, sizeof(n->result), "Error: agent ask failed"); snprintf(n->status, sizeof(n->status), "failed"); }
        }
    }
    pthread_mutex_unlock(&g_tg_lock);
    return gmb_task_list();
}

/* Svuota il registro (test/harness). */
void gmb_task_clear(void)
{
    pthread_mutex_lock(&g_tg_lock);
    g_tg_count = 0;
    pthread_mutex_unlock(&g_tg_lock);
}

/* Persistenza: `id\x1fdesc\x1fagent\x1fstatus\x1fresult\x1fdeps(\x1d)` per nodo, \x1e tra nodi. */
char* gmb_task_export(void)
{
    pthread_mutex_lock(&g_tg_lock);
    size_t cap = 64;
    for (int i = 0; i < g_tg_count; i++)
    {
        cap += strlen(g_tg_nodes[i].id) + strlen(g_tg_nodes[i].desc) + strlen(g_tg_nodes[i].agent)
             + strlen(g_tg_nodes[i].status) + strlen(g_tg_nodes[i].result) + 8;
        for (int d = 0; d < g_tg_nodes[i].ndeps; d++) cap += strlen(g_tg_nodes[i].deps[d]) + 1;
    }
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_tg_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_tg_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f",
            g_tg_nodes[i].id, g_tg_nodes[i].desc, g_tg_nodes[i].agent, g_tg_nodes[i].status, g_tg_nodes[i].result);
        for (int d = 0; d < g_tg_nodes[i].ndeps; d++)
        {
            if (d > 0) out[off++] = '\x1d';
            off += (size_t)snprintf(out + off, cap - off, "%s", g_tg_nodes[i].deps[d]);
        }
    }
    pthread_mutex_unlock(&g_tg_lock);
    return out;
}

void gmb_task_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f[6]; char* p = rec; int k = 0;
        while (k < 5) { f[k++] = p; char* s = strchr(p, '\x1f'); if (!s) break; *s = '\0'; p = s + 1; }
        f[k++] = p;
        if (k < 6) continue;
        gmb_task_define(f[0], f[1]);
        if (*f[2]) gmb_task_assign(f[0], f[2]);
        if (*f[5]) { char* sv = NULL; for (char* d = strtok_r(f[5], "\x1d", &sv); d; d = strtok_r(NULL, "\x1d", &sv)) gmb_task_depends(f[0], d); }
        pthread_mutex_lock(&g_tg_lock);
        int idx = gmb_task_find_locked(f[0]);
        if (idx >= 0) { snprintf(g_tg_nodes[idx].status, sizeof(g_tg_nodes[idx].status), "%s", f[3]); snprintf(g_tg_nodes[idx].result, sizeof(g_tg_nodes[idx].result), "%s", f[4]); }
        pthread_mutex_unlock(&g_tg_lock);
    }
    free(copy);
}
