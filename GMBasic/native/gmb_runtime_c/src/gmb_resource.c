/* gmb_resource.c — Resource Manager (mondo C).
 *
 * Orchestrazione agenti (A7): registro delle risorse/limiti (parallel agent limits, model slots,
 * context budget, connector limits, cpu/gpu/ram) + dynamic swarm sizing
 * (min(richiesti, max_agents)). Policy deterministica (nessuna lettura di sistema). Parita' con
 * Basic.Agents.ResourceRegistry (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_RES_MAX 64
#define GMB_RES_KEY_MAX 64
#define GMB_RES_VAL_MAX 256

typedef struct {
    char key[GMB_RES_KEY_MAX];
    char value[GMB_RES_VAL_MAX];
} gmb_res_t;

static gmb_res_t g_res[GMB_RES_MAX];
static int g_res_count = 0;
static pthread_mutex_t g_res_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_res_find_locked(const char* key)
{
    for (int i = 0; i < g_res_count; i++)
        if (strcasecmp(g_res[i].key, key) == 0) return i;
    return -1;
}

/* Imposta una risorsa/limite. 1 = ok, 0 = chiave vuota/pieno. */
int gmb_resource_set(const char* key, const char* value)
{
    if (!key || !*key) return 0;
    pthread_mutex_lock(&g_res_lock);
    int idx = gmb_res_find_locked(key);
    if (idx < 0)
    {
        if (g_res_count >= GMB_RES_MAX) { pthread_mutex_unlock(&g_res_lock); return 0; }
        idx = g_res_count++;
    }
    snprintf(g_res[idx].key, GMB_RES_KEY_MAX, "%s", key);
    snprintf(g_res[idx].value, GMB_RES_VAL_MAX, "%s", value ? value : "");
    pthread_mutex_unlock(&g_res_lock);
    return 1;
}

/* Valore della risorsa (malloc; "" se assente). */
char* gmb_resource_get(const char* key)
{
    char* out = (char*)malloc(GMB_RES_VAL_MAX);
    if (!out) return NULL;
    out[0] = '\0';
    if (key && *key)
    {
        pthread_mutex_lock(&g_res_lock);
        int idx = gmb_res_find_locked(key);
        if (idx >= 0) snprintf(out, GMB_RES_VAL_MAX, "%s", g_res[idx].value);
        pthread_mutex_unlock(&g_res_lock);
    }
    return out;
}

/* 1 se la risorsa è disponibile (numerica > 0, o stringa non vuota ≠ 0/false/off/no). */
int gmb_resource_available(const char* key)
{
    if (!key || !*key) return 0;
    pthread_mutex_lock(&g_res_lock);
    int idx = gmb_res_find_locked(key);
    int ok = 0;
    if (idx >= 0)
    {
        const char* v = g_res[idx].value;
        char* end = NULL;
        double d = strtod(v, &end);
        if (end && *end == '\0' && end != v) ok = d > 0 ? 1 : 0;
        else if (strcasecmp(v, "false") == 0 || strcasecmp(v, "off") == 0 || strcasecmp(v, "no") == 0) ok = 0;
        else ok = v[0] ? 1 : 0;
    }
    pthread_mutex_unlock(&g_res_lock);
    return ok;
}

static void gmb_res_escape(const char* s, char* out, size_t cap)
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

/* JSON di tutte le risorse, ordinate per chiave. malloc. */
char* gmb_resource_list(void)
{
    size_t cap = 64 + (size_t)GMB_RES_MAX * (GMB_RES_KEY_MAX * 2 + GMB_RES_VAL_MAX * 2 + 16);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_res_lock);
    int order[GMB_RES_MAX];
    int n = g_res_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_res[order[i]].key, g_res[order[j]].key) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = (size_t)snprintf(buf, cap, "{\"resources\":{");
    for (int k = 0; k < n; k++)
    {
        char ek[GMB_RES_KEY_MAX * 2], ev[GMB_RES_VAL_MAX * 2];
        gmb_res_escape(g_res[order[k]].key, ek, sizeof(ek));
        gmb_res_escape(g_res[order[k]].value, ev, sizeof(ev));
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":\"%s\"", k > 0 ? "," : "", ek, ev);
    }
    snprintf(buf + off, cap - off, "}}");
    pthread_mutex_unlock(&g_res_lock);
    return buf;
}

/* Dimensione effettiva dello swarm: min(n, max_agents) se max_agents è un numero positivo. */
double gmb_resource_swarm_size(double n)
{
    if (n < 0) n = 0;
    pthread_mutex_lock(&g_res_lock);
    int idx = gmb_res_find_locked("max_agents");
    double cap = 0;
    if (idx >= 0)
    {
        char* end = NULL;
        double d = strtod(g_res[idx].value, &end);
        if (end && *end == '\0' && end != g_res[idx].value && d > 0) cap = d;
    }
    pthread_mutex_unlock(&g_res_lock);
    if (cap > 0) { double f = (double)(long long)cap; return n < f ? n : f; }
    return n;
}

void gmb_resource_clear(void)
{
    pthread_mutex_lock(&g_res_lock);
    g_res_count = 0;
    pthread_mutex_unlock(&g_res_lock);
}

/* Persistenza: `key\x1fvalue` separati da \x1e. */
char* gmb_resource_export(void)
{
    pthread_mutex_lock(&g_res_lock);
    size_t cap = 64;
    for (int i = 0; i < g_res_count; i++) cap += strlen(g_res[i].key) + strlen(g_res[i].value) + 4;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_res_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_res_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s", g_res[i].key, g_res[i].value);
    }
    pthread_mutex_unlock(&g_res_lock);
    return out;
}

void gmb_resource_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        gmb_resource_set(rec, f2);
    }
    free(copy);
}
