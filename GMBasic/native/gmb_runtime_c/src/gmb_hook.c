/* gmb_hook.c — Registro degli hook di lifecycle (mondo C).
 *
 * Orchestrazione agenti (A3): registro evento → handler (nomi di SUB/FUNCTION). La FIRE
 * risolve gli handler registrati per l'evento; l'invocazione automatica ai lifecycle point
 * (con risoluzione del symbol) è il follow-on. Parita' con Basic.Agents.HookRegistry (C#):
 * stessa tassonomia, stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis), thread-safe.
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_HOOK_NEVENTS 13
#define GMB_HOOK_MAX_HANDLERS 16
#define GMB_HOOK_EVENT_MAX 32
#define GMB_HOOK_HANDLER_MAX 64

static const char* g_hook_events[GMB_HOOK_NEVENTS] = {
    "before_task", "after_task",
    "before_agent", "after_agent",
    "before_tool", "after_tool",
    "before_model", "after_model",
    "on_error", "on_retry", "on_complete", "on_verify", "on_handoff"
};

typedef struct {
    char handlers[GMB_HOOK_MAX_HANDLERS][GMB_HOOK_HANDLER_MAX];
    int n;
} gmb_hook_slot_t;

static gmb_hook_slot_t g_hooks[GMB_HOOK_NEVENTS];
static pthread_mutex_t g_hook_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_hook_event_index(const char* ev)
{
    if (!ev || !*ev) return -1;
    for (int i = 0; i < GMB_HOOK_NEVENTS; i++)
        if (strcasecmp(g_hook_events[i], ev) == 0) return i;
    return -1;
}

int gmb_hook_register(const char* event, const char* handler)
{
    if (!handler || !*handler) return 0;
    int ei = gmb_hook_event_index(event);
    if (ei < 0) return 0;
    pthread_mutex_lock(&g_hook_lock);
    gmb_hook_slot_t* s = &g_hooks[ei];
    for (int i = 0; i < s->n; i++)
        if (strcasecmp(s->handlers[i], handler) == 0) { pthread_mutex_unlock(&g_hook_lock); return 1; }
    if (s->n >= GMB_HOOK_MAX_HANDLERS) { pthread_mutex_unlock(&g_hook_lock); return 0; }
    snprintf(s->handlers[s->n++], GMB_HOOK_HANDLER_MAX, "%s", handler);
    pthread_mutex_unlock(&g_hook_lock);
    return 1;
}

int gmb_hook_unregister(const char* event, const char* handler)
{
    if (!handler || !*handler) return 0;
    int ei = gmb_hook_event_index(event);
    if (ei < 0) return 0;
    pthread_mutex_lock(&g_hook_lock);
    gmb_hook_slot_t* s = &g_hooks[ei];
    for (int i = 0; i < s->n; i++)
        if (strcasecmp(s->handlers[i], handler) == 0)
        {
            for (int j = i; j < s->n - 1; j++)
                snprintf(s->handlers[j], GMB_HOOK_HANDLER_MAX, "%s", s->handlers[j + 1]);
            s->n--;
            pthread_mutex_unlock(&g_hook_lock);
            return 1;
        }
    pthread_mutex_unlock(&g_hook_lock);
    return 0;
}

static void gmb_hook_escape(const char* s, char* out, size_t cap)
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

/* Ordina gli handler di uno slot (copia in out, per output deterministico). */
static void gmb_hook_sorted(gmb_hook_slot_t* s, char out[][GMB_HOOK_HANDLER_MAX], int* n)
{
    *n = s->n;
    for (int i = 0; i < s->n; i++) snprintf(out[i], GMB_HOOK_HANDLER_MAX, "%s", s->handlers[i]);
    for (int i = 0; i < *n - 1; i++)
        for (int j = i + 1; j < *n; j++)
            if (strcasecmp(out[i], out[j]) > 0)
            { char t[GMB_HOOK_HANDLER_MAX]; snprintf(t, sizeof(t), "%s", out[i]); snprintf(out[i], GMB_HOOK_HANDLER_MAX, "%s", out[j]); snprintf(out[j], GMB_HOOK_HANDLER_MAX, "%s", t); }
}

char* gmb_hook_list(void)
{
    size_t cap = 64 + (size_t)GMB_HOOK_NEVENTS * (GMB_HOOK_EVENT_MAX + GMB_HOOK_MAX_HANDLERS * (GMB_HOOK_HANDLER_MAX + 4));
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_hook_lock);
    size_t off = (size_t)snprintf(buf, cap, "{\"hooks\":{");
    int first = 1;
    for (int i = 0; i < GMB_HOOK_NEVENTS; i++)
    {
        if (g_hooks[i].n == 0) continue;
        char sorted[GMB_HOOK_MAX_HANDLERS][GMB_HOOK_HANDLER_MAX]; int n;
        gmb_hook_sorted(&g_hooks[i], sorted, &n);
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":[", first ? "" : ",", g_hook_events[i]);
        first = 0;
        for (int k = 0; k < n; k++)
        {
            char eh[GMB_HOOK_HANDLER_MAX * 2]; gmb_hook_escape(sorted[k], eh, sizeof(eh));
            off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\"", k > 0 ? "," : "", eh);
        }
        off += (size_t)snprintf(buf + off, cap - off, "]");
    }
    snprintf(buf + off, cap - off, "}}");
    pthread_mutex_unlock(&g_hook_lock);
    return buf;
}

char* gmb_hook_events(void)
{
    size_t cap = 32 + (size_t)GMB_HOOK_NEVENTS * (GMB_HOOK_EVENT_MAX + 4);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    size_t off = (size_t)snprintf(buf, cap, "{\"events\":[");
    for (int i = 0; i < GMB_HOOK_NEVENTS; i++)
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\"", i > 0 ? "," : "", g_hook_events[i]);
    snprintf(buf + off, cap - off, "]}");
    return buf;
}

char* gmb_hook_fire(const char* event, const char* payload)
{
    size_t cap = GMB_HOOK_EVENT_MAX + (size_t)GMB_HOOK_MAX_HANDLERS * (GMB_HOOK_HANDLER_MAX * 2 + 4) + 1024;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    int ei = gmb_hook_event_index(event);
    if (ei < 0) { snprintf(buf, 40, "{\"error\":\"evento hook sconosciuto\"}"); return buf; }
    pthread_mutex_lock(&g_hook_lock);
    char sorted[GMB_HOOK_MAX_HANDLERS][GMB_HOOK_HANDLER_MAX]; int n;
    gmb_hook_sorted(&g_hooks[ei], sorted, &n);
    char ep[512]; gmb_hook_escape(payload, ep, sizeof(ep));
    size_t off = (size_t)snprintf(buf, cap, "{\"event\":\"%s\",\"handlers\":[", g_hook_events[ei]);
    for (int k = 0; k < n; k++)
    {
        char eh[GMB_HOOK_HANDLER_MAX * 2]; gmb_hook_escape(sorted[k], eh, sizeof(eh));
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\"", k > 0 ? "," : "", eh);
    }
    off += (size_t)snprintf(buf + off, cap - off, "],\"agents\":{");
    int first = 1;
    for (int k = 0; k < n; k++)
    {
        if (!gmb_agent_exists(sorted[k])) continue;
        char* reply = gmb_agent_ask(sorted[k], payload ? payload : "");
        char ea[GMB_HOOK_HANDLER_MAX * 2]; gmb_hook_escape(sorted[k], ea, sizeof(ea));
        char er[2048]; gmb_hook_escape(reply ? reply : "", er, sizeof(er));
        free(reply);
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":\"%s\"", first ? "" : ",", ea, er);
        first = 0;
    }
    snprintf(buf + off, cap - off, "},\"payload\":\"%s\"}", ep);
    /* RILASCIO OBBLIGATORIO: senza questo la lock restava presa per sempre e la
     * successiva operazione hook (REGISTER/UNREGISTER/LIST) si bloccava → gmb_vm_host
     * appeso (misc/hook_domain.bas). */
    pthread_mutex_unlock(&g_hook_lock);
    return buf;
}

void gmb_hook_clear(void)
{
    pthread_mutex_lock(&g_hook_lock);
    for (int i = 0; i < GMB_HOOK_NEVENTS; i++) g_hooks[i].n = 0;
    pthread_mutex_unlock(&g_hook_lock);
}

/* Persistenza: `event\x1fhandler` separati da \x1e. */
char* gmb_hook_export(void)
{
    pthread_mutex_lock(&g_hook_lock);
    size_t cap = 64;
    for (int i = 0; i < GMB_HOOK_NEVENTS; i++)
        for (int j = 0; j < g_hooks[i].n; j++) cap += strlen(g_hook_events[i]) + strlen(g_hooks[i].handlers[j]) + 4;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_hook_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    int first = 1;
    for (int i = 0; i < GMB_HOOK_NEVENTS; i++)
        for (int j = 0; j < g_hooks[i].n; j++)
        {
            if (!first) out[off++] = '\x1e';
            first = 0;
            off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s", g_hook_events[i], g_hooks[i].handlers[j]);
        }
    pthread_mutex_unlock(&g_hook_lock);
    return out;
}

void gmb_hook_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        gmb_hook_register(rec, f2);
    }
    free(copy);
}
