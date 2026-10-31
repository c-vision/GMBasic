/* gmb_rules.c — Rules injection gerarchica (mondo C).
 *
 * Orchestrazione agenti (A5): registro di regole per SCOPE (directory). La risoluzione per un
 * path concatena le regole di tutti gli scope ANCESTRALI, dal più generico (root) al più
 * specifico (leaf) — come la risoluzione gerarchica di AGENTS.md. Parita' con
 * Basic.Agents.RuleRegistry (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 * v1 in-memory (caricamento da disco = follow-on).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_RULES_MAX 64
#define GMB_RULES_SCOPE_MAX 256
#define GMB_RULES_CONTENT_MAX 4096

typedef struct {
    char scope[GMB_RULES_SCOPE_MAX];
    char content[GMB_RULES_CONTENT_MAX];
} gmb_rule_t;

static gmb_rule_t g_rules[GMB_RULES_MAX];
static int g_rules_count = 0;
static pthread_mutex_t g_rules_lock = PTHREAD_MUTEX_INITIALIZER;

/* Normalizza: rimuove slash finali. */
static void gmb_rules_norm(const char* in, char* out, size_t cap)
{
    size_t n = strlen(in ? in : "");
    while (n > 0 && (in[n - 1] == '/' || in[n - 1] == '\\')) n--;
    if (n >= cap) n = cap - 1;
    memcpy(out, in ? in : "", n);
    out[n] = '\0';
}

static int gmb_rules_find_locked(const char* scope)
{
    for (int i = 0; i < g_rules_count; i++)
        if (strcasecmp(g_rules[i].scope, scope) == 0) return i;
    return -1;
}

int gmb_rules_add(const char* scope, const char* content)
{
    if (!scope || !*scope) return 0;
    char norm[GMB_RULES_SCOPE_MAX];
    gmb_rules_norm(scope, norm, sizeof(norm));
    pthread_mutex_lock(&g_rules_lock);
    int idx = gmb_rules_find_locked(norm);
    if (idx < 0)
    {
        if (g_rules_count >= GMB_RULES_MAX) { pthread_mutex_unlock(&g_rules_lock); return 0; }
        idx = g_rules_count++;
    }
    snprintf(g_rules[idx].scope, GMB_RULES_SCOPE_MAX, "%s", norm);
    snprintf(g_rules[idx].content, GMB_RULES_CONTENT_MAX, "%s", content ? content : "");
    pthread_mutex_unlock(&g_rules_lock);
    return 1;
}

int gmb_rules_remove(const char* scope)
{
    if (!scope || !*scope) return 0;
    char norm[GMB_RULES_SCOPE_MAX];
    gmb_rules_norm(scope, norm, sizeof(norm));
    pthread_mutex_lock(&g_rules_lock);
    int idx = gmb_rules_find_locked(norm);
    if (idx < 0) { pthread_mutex_unlock(&g_rules_lock); return 0; }
    for (int j = idx; j < g_rules_count - 1; j++) g_rules[j] = g_rules[j + 1];
    g_rules_count--;
    pthread_mutex_unlock(&g_rules_lock);
    return 1;
}

static void gmb_rules_escape(const char* s, char* out, size_t cap)
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

char* gmb_rules_list(void)
{
    size_t cap = 64 + (size_t)GMB_RULES_MAX * (GMB_RULES_SCOPE_MAX * 2 + GMB_RULES_CONTENT_MAX * 2 + 32);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_rules_lock);
    int order[GMB_RULES_MAX];
    int n = g_rules_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_rules[order[i]].scope, g_rules[order[j]].scope) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = (size_t)snprintf(buf, cap, "{\"rules\":[");
    for (int k = 0; k < n; k++)
    {
        char es[GMB_RULES_SCOPE_MAX * 2], ec[GMB_RULES_CONTENT_MAX * 2];
        gmb_rules_escape(g_rules[order[k]].scope, es, sizeof(es));
        gmb_rules_escape(g_rules[order[k]].content, ec, sizeof(ec));
        off += (size_t)snprintf(buf + off, cap - off, "%s{\"scope\":\"%s\",\"content\":\"%s\"}", k > 0 ? "," : "", es, ec);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_rules_lock);
    return buf;
}

int gmb_rules_clear(void)
{
    pthread_mutex_lock(&g_rules_lock);
    int n = g_rules_count;
    g_rules_count = 0;
    pthread_mutex_unlock(&g_rules_lock);
    return n;
}

/* Risolve le regole per un path: scope ancestrali, dal più corto (root) al più lungo, '\n'-join. */
char* gmb_rules_for(const char* path)
{
    char p[GMB_RULES_SCOPE_MAX];
    gmb_rules_norm(path, p, sizeof(p));
    size_t pn = strlen(p);
    pthread_mutex_lock(&g_rules_lock);
    int order[GMB_RULES_MAX]; int n = 0;
    for (int i = 0; i < g_rules_count; i++)
    {
        const char* s = g_rules[i].scope;
        int match = 0;
        if (s[0] == '\0' || strcmp(s, "/") == 0) match = 1;
        else if (strcasecmp(p, s) == 0) match = 1;
        else {
            size_t sn = strlen(s);
            if (pn > sn && p[sn] == '/' && strncasecmp(p, s, sn) == 0) match = 1;
        }
        if (match) order[n++] = i;
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strlen(g_rules[order[i]].scope) > strlen(g_rules[order[j]].scope))
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t cap = 64 + (size_t)n * (GMB_RULES_CONTENT_MAX + 2);
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_rules_lock); return NULL; }
    out[0] = '\0';
    size_t off = 0;
    for (int k = 0; k < n; k++)
    {
        if (off > 0) { out[off++] = '\n'; out[off] = '\0'; }
        int w = snprintf(out + off, cap - off, "%s", g_rules[order[k]].content);
        if (w > 0) off += (size_t)w;
    }
    pthread_mutex_unlock(&g_rules_lock);
    return out;
}

/* Persistenza: `scope\x1fcontent` separati da \x1e. */
char* gmb_rules_export(void)
{
    pthread_mutex_lock(&g_rules_lock);
    size_t cap = 64;
    for (int i = 0; i < g_rules_count; i++) cap += strlen(g_rules[i].scope) + strlen(g_rules[i].content) + 4;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_rules_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_rules_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s", g_rules[i].scope, g_rules[i].content);
    }
    pthread_mutex_unlock(&g_rules_lock);
    return out;
}

void gmb_rules_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        gmb_rules_add(rec, f2);
    }
    free(copy);
}
