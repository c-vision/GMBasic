/* gmb_handoff.c — Handoff (mondo C).
 *
 * Orchestrazione agenti (A9): trasferimento di un task da un agente a un altro — registro
 * (task → agente destinatario) con stato (pending/done) e risultato. Parita' con
 * Basic.Agents.HandoffRegistry (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_HANDOFF_MAX 128
#define GMB_HANDOFF_TASK_MAX 64
#define GMB_HANDOFF_TO_MAX 64
#define GMB_HANDOFF_RESULT_MAX 2048

typedef struct {
    char task[GMB_HANDOFF_TASK_MAX];
    char to[GMB_HANDOFF_TO_MAX];
    char status[16];
    char result[GMB_HANDOFF_RESULT_MAX];
} gmb_handoff_t;

static gmb_handoff_t g_hoff[GMB_HANDOFF_MAX];
static int g_hoff_count = 0;
static pthread_mutex_t g_hoff_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_handoff_find_locked(const char* task)
{
    for (int i = 0; i < g_hoff_count; i++)
        if (strcasecmp(g_hoff[i].task, task) == 0) return i;
    return -1;
}

/* Registra/aggiorna un handoff task → agente (stato pending). 1 = ok, 0 = err. */
int gmb_handoff_set(const char* task_id, const char* to_agent)
{
    if (!task_id || !*task_id || !to_agent || !*to_agent) return 0;
    pthread_mutex_lock(&g_hoff_lock);
    int idx = gmb_handoff_find_locked(task_id);
    if (idx < 0)
    {
        if (g_hoff_count >= GMB_HANDOFF_MAX) { pthread_mutex_unlock(&g_hoff_lock); return 0; }
        idx = g_hoff_count++;
    }
    snprintf(g_hoff[idx].task, GMB_HANDOFF_TASK_MAX, "%s", task_id);
    snprintf(g_hoff[idx].to, GMB_HANDOFF_TO_MAX, "%s", to_agent);
    snprintf(g_hoff[idx].status, sizeof(g_hoff[idx].status), "pending");
    g_hoff[idx].result[0] = '\0';
    pthread_mutex_unlock(&g_hoff_lock);
    return 1;
}

/* Marca l'handoff come completato col risultato. 1 = ok, 0 = task inesistente. */
int gmb_handoff_complete(const char* task_id, const char* result)
{
    if (!task_id || !*task_id) return 0;
    pthread_mutex_lock(&g_hoff_lock);
    int idx = gmb_handoff_find_locked(task_id);
    if (idx < 0) { pthread_mutex_unlock(&g_hoff_lock); return 0; }
    snprintf(g_hoff[idx].status, sizeof(g_hoff[idx].status), "done");
    snprintf(g_hoff[idx].result, GMB_HANDOFF_RESULT_MAX, "%s", result ? result : "");
    pthread_mutex_unlock(&g_hoff_lock);
    return 1;
}

static void gmb_hoff_escape(const char* s, char* out, size_t cap)
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

static void gmb_hoff_json_locked(const gmb_handoff_t* h, char* out, size_t cap)
{
    char et[GMB_HANDOFF_TASK_MAX * 2], eo[GMB_HANDOFF_TO_MAX * 2], es[64], er[GMB_HANDOFF_RESULT_MAX * 2];
    gmb_hoff_escape(h->task, et, sizeof(et));
    gmb_hoff_escape(h->to, eo, sizeof(eo));
    gmb_hoff_escape(h->status, es, sizeof(es));
    gmb_hoff_escape(h->result, er, sizeof(er));
    snprintf(out, cap, "{\"task\":\"%s\",\"to\":\"%s\",\"status\":\"%s\",\"result\":\"%s\"}", et, eo, es, er);
}

char* gmb_handoff_status(const char* task_id)
{
    char* buf = (char*)malloc(GMB_HANDOFF_TASK_MAX * 2 + GMB_HANDOFF_TO_MAX * 2 + GMB_HANDOFF_RESULT_MAX * 2 + 128);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_hoff_lock);
    int idx = (task_id && *task_id) ? gmb_handoff_find_locked(task_id) : -1;
    if (idx < 0) snprintf(buf, 3, "{}");
    else gmb_hoff_json_locked(&g_hoff[idx], buf, GMB_HANDOFF_TASK_MAX * 2 + GMB_HANDOFF_TO_MAX * 2 + GMB_HANDOFF_RESULT_MAX * 2 + 128);
    pthread_mutex_unlock(&g_hoff_lock);
    return buf;
}

char* gmb_handoff_list(void)
{
    size_t cap = 64 + (size_t)GMB_HANDOFF_MAX * (GMB_HANDOFF_TASK_MAX * 2 + GMB_HANDOFF_TO_MAX * 2 + GMB_HANDOFF_RESULT_MAX * 2 + 128);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_hoff_lock);
    int order[GMB_HANDOFF_MAX];
    int n = g_hoff_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_hoff[order[i]].task, g_hoff[order[j]].task) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = (size_t)snprintf(buf, cap, "{\"handoffs\":[");
    for (int k = 0; k < n; k++)
    {
        if (k > 0) off += (size_t)snprintf(buf + off, cap - off, ",");
        gmb_hoff_json_locked(&g_hoff[order[k]], buf + off, cap - off);
        off = strlen(buf);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_hoff_lock);
    return buf;
}

/* Svuota il registro. Ritorna il numero di handoff rimossi. */
int gmb_handoff_clear(void)
{
    pthread_mutex_lock(&g_hoff_lock);
    int n = g_hoff_count;
    g_hoff_count = 0;
    pthread_mutex_unlock(&g_hoff_lock);
    return n;
}

/* Persistenza: record `task\x1fto\x1fstatus\x1fresult` separati da \x1e. */
char* gmb_handoff_export(void)
{
    pthread_mutex_lock(&g_hoff_lock);
    size_t cap = 64;
    for (int i = 0; i < g_hoff_count; i++)
        cap += strlen(g_hoff[i].task) + strlen(g_hoff[i].to) + strlen(g_hoff[i].status) + strlen(g_hoff[i].result) + 5;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_hoff_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_hoff_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s\x1f%s\x1f%s",
            g_hoff[i].task, g_hoff[i].to, g_hoff[i].status, g_hoff[i].result);
    }
    pthread_mutex_unlock(&g_hoff_lock);
    return out;
}

void gmb_handoff_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        char* f3 = strchr(f2, '\x1f'); if (!f3) continue; *f3++ = '\0';
        char* f4 = strchr(f3, '\x1f'); if (!f4) continue; *f4++ = '\0';
        gmb_handoff_set(rec, f2);
        if (strcmp(f3, "done") == 0) gmb_handoff_complete(rec, f4);
    }
    free(copy);
}
