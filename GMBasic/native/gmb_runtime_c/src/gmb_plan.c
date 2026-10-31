/* gmb_plan.c — Registro di piani (Planner) con approvazione opzionale (mondo C).
 *
 * Orchestrazione agenti (A2): un piano = obiettivo + insieme di task + approvazione.
 * Separa la pianificazione dall'esecuzione: gmb_plan_run rifiuta un piano non approvato e,
 * se approvato, esegue il Task Graph riusando gmb_task_run (gmb_taskgraph.c). Parita' con
 * Basic.Agents.PlanRegistry (C#). La decomposizione LLM dell'obiettivo è un follow-on.
 * Runtime CONDIVISO (AGENTS §0bis), thread-safe (mutex).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_PLAN_MAX 64
#define GMB_PLAN_NAME_MAX 64
#define GMB_PLAN_GOAL_MAX 512
#define GMB_PLAN_TASKS_MAX 32
#define GMB_PLAN_RESULT_MAX 4096

typedef struct {
    char name[GMB_PLAN_NAME_MAX];
    char goal[GMB_PLAN_GOAL_MAX];
    int approved;
    char status[16];
    char result[GMB_PLAN_RESULT_MAX];
    char tasks[GMB_PLAN_TASKS_MAX][GMB_PLAN_NAME_MAX];
    int ntasks;
} gmb_plan_t;

static gmb_plan_t g_plans[GMB_PLAN_MAX];
static int g_plan_count = 0;
static pthread_mutex_t g_plan_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_plan_find_locked(const char* name)
{
    for (int i = 0; i < g_plan_count; i++)
        if (strcasecmp(g_plans[i].name, name) == 0) return i;
    return -1;
}

int gmb_plan_define(const char* name, const char* goal)
{
    if (!name || !*name) return 0;
    pthread_mutex_lock(&g_plan_lock);
    int idx = gmb_plan_find_locked(name);
    if (idx < 0)
    {
        if (g_plan_count >= GMB_PLAN_MAX) { pthread_mutex_unlock(&g_plan_lock); return 0; }
        idx = g_plan_count++;
    }
    memset(&g_plans[idx], 0, sizeof(g_plans[idx]));
    snprintf(g_plans[idx].name, sizeof(g_plans[idx].name), "%s", name);
    snprintf(g_plans[idx].goal, sizeof(g_plans[idx].goal), "%s", goal ? goal : "");
    snprintf(g_plans[idx].status, sizeof(g_plans[idx].status), "draft");
    pthread_mutex_unlock(&g_plan_lock);
    return 1;
}

int gmb_plan_add(const char* name, const char* task_id)
{
    if (!name || !*name || !task_id || !*task_id) return 0;
    pthread_mutex_lock(&g_plan_lock);
    int idx = gmb_plan_find_locked(name);
    if (idx < 0) { pthread_mutex_unlock(&g_plan_lock); return 0; }
    for (int i = 0; i < g_plans[idx].ntasks; i++)
        if (strcasecmp(g_plans[idx].tasks[i], task_id) == 0) { pthread_mutex_unlock(&g_plan_lock); return 0; }
    if (g_plans[idx].ntasks >= GMB_PLAN_TASKS_MAX) { pthread_mutex_unlock(&g_plan_lock); return 0; }
    snprintf(g_plans[idx].tasks[g_plans[idx].ntasks++], GMB_PLAN_NAME_MAX, "%s", task_id);
    pthread_mutex_unlock(&g_plan_lock);
    return 1;
}

int gmb_plan_approve(const char* name)
{
    if (!name || !*name) return 0;
    pthread_mutex_lock(&g_plan_lock);
    int idx = gmb_plan_find_locked(name);
    if (idx < 0) { pthread_mutex_unlock(&g_plan_lock); return 0; }
    g_plans[idx].approved = 1;
    snprintf(g_plans[idx].status, sizeof(g_plans[idx].status), "approved");
    pthread_mutex_unlock(&g_plan_lock);
    return 1;
}

static void gmb_plan_escape(const char* s, char* out, size_t cap)
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

static void gmb_plan_json_locked(const gmb_plan_t* p, char* out, size_t cap)
{
    char en[GMB_PLAN_NAME_MAX * 2], eg[GMB_PLAN_GOAL_MAX * 2];
    char est[64], er[GMB_PLAN_RESULT_MAX * 2];
    gmb_plan_escape(p->name, en, sizeof(en));
    gmb_plan_escape(p->goal, eg, sizeof(eg));
    gmb_plan_escape(p->status, est, sizeof(est));
    gmb_plan_escape(p->result, er, sizeof(er));
    size_t off = 0;
    off += (size_t)snprintf(out + off, cap - off,
        "{\"name\":\"%s\",\"goal\":\"%s\",\"approved\":%s,\"status\":\"%s\",\"result\":\"%s\",\"tasks\":[",
        en, eg, p->approved ? "true" : "false", est, er);
    for (int i = 0; i < p->ntasks && off + 8 < cap; i++)
    {
        char et[GMB_PLAN_NAME_MAX * 2];
        gmb_plan_escape(p->tasks[i], et, sizeof(et));
        off += (size_t)snprintf(out + off, cap - off, "%s\"%s\"", i > 0 ? "," : "", et);
    }
    snprintf(out + off, cap - off, "]}");
}

/* Elenco dei task di un piano in CSV (ordine di inserimento) — per lo SCOPING del manager loop
 * (`AI WORKER MANAGE "<plan>"`). Ritorna 1 se il piano esiste (csv scritto, anche vuoto), 0 se no.
 * Speculare a PlanRegistry.TasksCsv (C#). */
int gmb_plan_tasks(const char* name, char* csv_out, size_t cap)
{
    if (csv_out && cap) csv_out[0] = 0;
    if (!name || !*name || !csv_out || cap == 0) return 0;
    pthread_mutex_lock(&g_plan_lock);
    int idx = gmb_plan_find_locked(name);
    if (idx < 0) { pthread_mutex_unlock(&g_plan_lock); return 0; }
    size_t off = 0;
    for (int i = 0; i < g_plans[idx].ntasks && off + 2 < cap; i++)
        off += (size_t)snprintf(csv_out + off, cap - off, "%s%s", i ? "," : "", g_plans[idx].tasks[i]);
    pthread_mutex_unlock(&g_plan_lock);
    return 1;
}

/* 1 se il piano esiste ed è approvato (`AI PLAN APPROVE`) — il manager loop non lavora su un
 * piano in bozza. Speculare a PlanRegistry.IsApproved (C#). */
int gmb_plan_is_approved(const char* name)
{
    if (!name || !*name) return 0;
    pthread_mutex_lock(&g_plan_lock);
    int idx = gmb_plan_find_locked(name);
    int ok = (idx >= 0 && g_plans[idx].approved) ? 1 : 0;
    pthread_mutex_unlock(&g_plan_lock);
    return ok;
}

char* gmb_plan_status(const char* name)
{
    size_t cap = GMB_PLAN_GOAL_MAX * 2 + GMB_PLAN_RESULT_MAX * 2 + 512;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_plan_lock);
    int idx = (name && *name) ? gmb_plan_find_locked(name) : -1;
    if (idx < 0) snprintf(buf, 3, "{}");
    else gmb_plan_json_locked(&g_plans[idx], buf, cap);
    pthread_mutex_unlock(&g_plan_lock);
    return buf;
}

char* gmb_plan_list(void)
{
    size_t cap = 64 + (size_t)GMB_PLAN_MAX * (GMB_PLAN_GOAL_MAX * 2 + GMB_PLAN_RESULT_MAX * 2 + 512);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_plan_lock);
    int order[GMB_PLAN_MAX];
    int n = g_plan_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_plans[order[i]].name, g_plans[order[j]].name) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "{\"plans\":[");
    for (int k = 0; k < n; k++)
    {
        if (k > 0) off += (size_t)snprintf(buf + off, cap - off, ",");
        gmb_plan_json_locked(&g_plans[order[k]], buf + off, cap - off);
        off = strlen(buf);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_plan_lock);
    return buf;
}

/* Esegue il piano: richiede approvazione; esegue il Task Graph (gmb_task_run). malloc JSON. */
char* gmb_plan_run(const char* name)
{
    pthread_mutex_lock(&g_plan_lock);
    int idx = (name && *name) ? gmb_plan_find_locked(name) : -1;
    if (idx < 0)
    {
        char* e = (char*)malloc(128);
        if (e) snprintf(e, 128, "{\"error\":\"piano inesistente\"}");
        pthread_mutex_unlock(&g_plan_lock);
        return e;
    }
    if (!g_plans[idx].approved)
    {
        char* e = (char*)malloc(160);
        if (e) snprintf(e, 160, "{\"error\":\"piano non approvato (usare PLANAPPROVE)\"}");
        pthread_mutex_unlock(&g_plan_lock);
        return e;
    }
    snprintf(g_plans[idx].status, sizeof(g_plans[idx].status), "running");
    pthread_mutex_unlock(&g_plan_lock);

    char* result = gmb_task_run();

    pthread_mutex_lock(&g_plan_lock);
    idx = gmb_plan_find_locked(name);
    if (idx >= 0)
    {
        snprintf(g_plans[idx].result, sizeof(g_plans[idx].result), "%s", result ? result : "");
        snprintf(g_plans[idx].status, sizeof(g_plans[idx].status),
            (result && strstr(result, "\"error\"")) ? "failed" : "done");
    }
    pthread_mutex_unlock(&g_plan_lock);
    return result;
}

void gmb_plan_clear(void)
{
    pthread_mutex_lock(&g_plan_lock);
    g_plan_count = 0;
    pthread_mutex_unlock(&g_plan_lock);
}

/* Persistenza: `name\x1fgoal\x1fapproved\x1fstatus\x1fresult\x1ftasks(\x1d)` per piano. */
char* gmb_plan_export(void)
{
    pthread_mutex_lock(&g_plan_lock);
    size_t cap = 64;
    for (int i = 0; i < g_plan_count; i++)
    {
        cap += strlen(g_plans[i].name) + strlen(g_plans[i].goal) + strlen(g_plans[i].status) + strlen(g_plans[i].result) + 8;
        for (int t = 0; t < g_plans[i].ntasks; t++) cap += strlen(g_plans[i].tasks[t]) + 1;
    }
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_plan_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_plan_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s\x1f%d\x1f%s\x1f%s\x1f",
            g_plans[i].name, g_plans[i].goal, g_plans[i].approved, g_plans[i].status, g_plans[i].result);
        for (int t = 0; t < g_plans[i].ntasks; t++)
        {
            if (t > 0) out[off++] = '\x1d';
            off += (size_t)snprintf(out + off, cap - off, "%s", g_plans[i].tasks[t]);
        }
    }
    pthread_mutex_unlock(&g_plan_lock);
    return out;
}

void gmb_plan_import(const char* state)
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
        gmb_plan_define(f[0], f[1]);
        if (atoi(f[2])) gmb_plan_approve(f[0]);
        if (*f[5]) { char* sv = NULL; for (char* t = strtok_r(f[5], "\x1d", &sv); t; t = strtok_r(NULL, "\x1d", &sv)) gmb_plan_add(f[0], t); }
        pthread_mutex_lock(&g_plan_lock);
        int idx = gmb_plan_find_locked(f[0]);
        if (idx >= 0) { snprintf(g_plans[idx].status, sizeof(g_plans[idx].status), "%s", f[3]); snprintf(g_plans[idx].result, sizeof(g_plans[idx].result), "%s", f[4]); }
        pthread_mutex_unlock(&g_plan_lock);
    }
    free(copy);
}
