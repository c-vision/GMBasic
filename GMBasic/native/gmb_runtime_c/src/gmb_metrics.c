/* gmb_metrics.c — Metriche fleet-level (mondo C).
 *
 * Orchestrazione agenti (B6): aggregazione di metriche per-agente (token, costo, latenza,
 * successi/fallimenti) in un registro unico; gmb_fleet_stats somma per agente e per chiave su
 * tutta la flotta. Parita' con Basic.Agents.MetricsRegistry (C#): stesso formato JSON.
 * Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_MET_MAX 256
#define GMB_MET_AGENT_MAX 64
#define GMB_MET_KEY_MAX 64

typedef struct {
    char agent[GMB_MET_AGENT_MAX];
    char key[GMB_MET_KEY_MAX];
    double value;
} gmb_met_t;

static gmb_met_t g_met[GMB_MET_MAX];
static int g_met_count = 0;
static pthread_mutex_t g_met_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_met_find_locked(const char* agent, const char* key)
{
    for (int i = 0; i < g_met_count; i++)
        if (strcasecmp(g_met[i].agent, agent) == 0 && strcasecmp(g_met[i].key, key) == 0) return i;
    return -1;
}

int gmb_metric_record(const char* agent, const char* key, double value)
{
    if (!agent || !*agent || !key || !*key) return 0;
    pthread_mutex_lock(&g_met_lock);
    int idx = gmb_met_find_locked(agent, key);
    if (idx < 0)
    {
        if (g_met_count >= GMB_MET_MAX) { pthread_mutex_unlock(&g_met_lock); return 0; }
        idx = g_met_count++;
        snprintf(g_met[idx].agent, GMB_MET_AGENT_MAX, "%s", agent);
        snprintf(g_met[idx].key, GMB_MET_KEY_MAX, "%s", key);
        g_met[idx].value = 0;
    }
    g_met[idx].value += value;
    pthread_mutex_unlock(&g_met_lock);
    return 1;
}

double gmb_metric_get(const char* agent, const char* key)
{
    if (!agent || !*agent || !key || !*key) return 0;
    pthread_mutex_lock(&g_met_lock);
    int idx = gmb_met_find_locked(agent, key);
    double v = idx >= 0 ? g_met[idx].value : 0;
    pthread_mutex_unlock(&g_met_lock);
    return v;
}

static void gmb_met_escape(const char* s, char* out, size_t cap)
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

/* Ordina gli agenti distinti (indici in order). Ritorna il numero. */
static int gmb_met_agents_locked(char out[][GMB_MET_AGENT_MAX], int max)
{
    int n = 0;
    for (int i = 0; i < g_met_count && n < max; i++)
    {
        int seen = 0;
        for (int j = 0; j < n; j++) if (strcasecmp(out[j], g_met[i].agent) == 0) { seen = 1; break; }
        if (!seen) snprintf(out[n++], GMB_MET_AGENT_MAX, "%s", g_met[i].agent);
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(out[i], out[j]) > 0) { char t[GMB_MET_AGENT_MAX]; snprintf(t, sizeof(t), "%s", out[i]); snprintf(out[i], GMB_MET_AGENT_MAX, "%s", out[j]); snprintf(out[j], GMB_MET_AGENT_MAX, "%s", t); }
    return n;
}

static void gmb_met_agent_json_locked(const char* agent, char* out, size_t cap)
{
    size_t off = (size_t)snprintf(out, cap, "{");
    int first = 1;
    /* chiavi distinte dell'agente, ordinate */
    char keys[GMB_MET_MAX][GMB_MET_KEY_MAX]; int nk = 0;
    for (int i = 0; i < g_met_count; i++)
        if (strcasecmp(g_met[i].agent, agent) == 0)
        {
            int seen = 0;
            for (int j = 0; j < nk; j++) if (strcasecmp(keys[j], g_met[i].key) == 0) { seen = 1; break; }
            if (!seen) snprintf(keys[nk++], GMB_MET_KEY_MAX, "%s", g_met[i].key);
        }
    for (int i = 0; i < nk - 1; i++)
        for (int j = i + 1; j < nk; j++)
            if (strcasecmp(keys[i], keys[j]) > 0) { char t[GMB_MET_KEY_MAX]; snprintf(t, sizeof(t), "%s", keys[i]); snprintf(keys[i], GMB_MET_KEY_MAX, "%s", keys[j]); snprintf(keys[j], GMB_MET_KEY_MAX, "%s", t); }
    for (int k = 0; k < nk; k++)
    {
        int idx = gmb_met_find_locked(agent, keys[k]);
        if (idx < 0) continue;
        char ek[GMB_MET_KEY_MAX * 2]; gmb_met_escape(keys[k], ek, sizeof(ek));
        off += (size_t)snprintf(out + off, cap - off, "%s\"%s\":%g", first ? "" : ",", ek, g_met[idx].value);
        first = 0;
    }
    snprintf(out + off, cap - off, "}");
}

char* gmb_metric_list(void)
{
    size_t cap = 64 + (size_t)GMB_MET_MAX * (GMB_MET_AGENT_MAX * 2 + GMB_MET_KEY_MAX * 2 + 32);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_met_lock);
    char agents[GMB_MET_MAX][GMB_MET_AGENT_MAX];
    int na = gmb_met_agents_locked(agents, GMB_MET_MAX);
    size_t off = (size_t)snprintf(buf, cap, "{\"metrics\":{");
    for (int i = 0; i < na; i++)
    {
        char ea[GMB_MET_AGENT_MAX * 2]; gmb_met_escape(agents[i], ea, sizeof(ea));
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":", i > 0 ? "," : "", ea);
        gmb_met_agent_json_locked(agents[i], buf + off, cap - off);
        off = strlen(buf);
    }
    snprintf(buf + off, cap - off, "}}");
    pthread_mutex_unlock(&g_met_lock);
    return buf;
}

char* gmb_fleet_stats(void)
{
    size_t cap = 128 + (size_t)GMB_MET_MAX * (GMB_MET_AGENT_MAX * 2 + GMB_MET_KEY_MAX * 2 + 32);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_met_lock);
    char agents[GMB_MET_MAX][GMB_MET_AGENT_MAX];
    int na = gmb_met_agents_locked(agents, GMB_MET_MAX);
    size_t off = (size_t)snprintf(buf, cap, "{\"agents\":{");
    for (int i = 0; i < na; i++)
    {
        char ea[GMB_MET_AGENT_MAX * 2]; gmb_met_escape(agents[i], ea, sizeof(ea));
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":", i > 0 ? "," : "", ea);
        gmb_met_agent_json_locked(agents[i], buf + off, cap - off);
        off = strlen(buf);
    }
    /* totals: somma per chiave su tutti gli agenti */
    off += (size_t)snprintf(buf + off, cap - off, "},\"totals\":{");
    char keys[GMB_MET_MAX][GMB_MET_KEY_MAX]; int nk = 0;
    for (int i = 0; i < g_met_count; i++)
    {
        int seen = 0;
        for (int j = 0; j < nk; j++) if (strcasecmp(keys[j], g_met[i].key) == 0) { seen = 1; break; }
        if (!seen) snprintf(keys[nk++], GMB_MET_KEY_MAX, "%s", g_met[i].key);
    }
    for (int i = 0; i < nk - 1; i++)
        for (int j = i + 1; j < nk; j++)
            if (strcasecmp(keys[i], keys[j]) > 0) { char t[GMB_MET_KEY_MAX]; snprintf(t, sizeof(t), "%s", keys[i]); snprintf(keys[i], GMB_MET_KEY_MAX, "%s", keys[j]); snprintf(keys[j], GMB_MET_KEY_MAX, "%s", t); }
    for (int k = 0; k < nk; k++)
    {
        double sum = 0;
        for (int i = 0; i < g_met_count; i++) if (strcasecmp(g_met[i].key, keys[k]) == 0) sum += g_met[i].value;
        char ek[GMB_MET_KEY_MAX * 2]; gmb_met_escape(keys[k], ek, sizeof(ek));
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":%g", k > 0 ? "," : "", ek, sum);
    }
    snprintf(buf + off, cap - off, "},\"count\":%d}", na);
    pthread_mutex_unlock(&g_met_lock);
    return buf;
}

/* Persistenza: record `agent\x1fkey\x1fvalue` separati da \x1e. */
char* gmb_metric_export(void)
{
    pthread_mutex_lock(&g_met_lock);
    size_t cap = 64;
    for (int i = 0; i < g_met_count; i++) cap += strlen(g_met[i].agent) + strlen(g_met[i].key) + 40;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_met_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_met_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s\x1f%g", g_met[i].agent, g_met[i].key, g_met[i].value);
    }
    pthread_mutex_unlock(&g_met_lock);
    return out;
}

void gmb_metric_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        char* f3 = strchr(f2, '\x1f'); if (!f3) continue; *f3++ = '\0';
        gmb_metric_record(rec, f2, atof(f3));
    }
    free(copy);
}

int gmb_metric_reset(void)
{
    pthread_mutex_lock(&g_met_lock);
    /* Parità C# MetricsRegistry.Reset: numero di AGENTI distinti (non di voci). */
    char agents[GMB_MET_MAX][GMB_MET_AGENT_MAX];
    int na = gmb_met_agents_locked(agents, GMB_MET_MAX);
    g_met_count = 0;
    pthread_mutex_unlock(&g_met_lock);
    return na;
}

/* §3.6 PARSEC — metriche TASK standard (TOKENS/TTFT/TOOL CALLS/LLM CALLS/LATENCY) come JSON.
 * Stesse chiavi e formato (%.0f) del C# MetricsRegistry.TaskMetrics (parità 5/5). */
char* gmb_task_metrics(const char* task)
{
    static const char* k[5] = { "tokens", "ttft", "tool_calls", "llm_calls", "latency" };
    char* o = (char*)malloc(512);
    if (!o) return NULL;
    size_t p = 0;
    p += (size_t)snprintf(o + p, 512 - p, "{");
    for (int i = 0; i < 5; i++)
        p += (size_t)snprintf(o + p, 512 - p, "%s\"%s\":%.0f", i ? "," : "", k[i], gmb_metric_get(task, k[i]));
    p += (size_t)snprintf(o + p, 512 - p, "}");
    return o;
}
