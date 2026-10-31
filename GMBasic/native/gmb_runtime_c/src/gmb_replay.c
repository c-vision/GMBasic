/* gmb_replay.c — Session Replay (mondo C).
 *
 * Orchestrazione agenti (A8): registrazione di una sessione agentica (eventi append-only per
 * sessione, con stato di registrazione). gmb_replay_get restituisce la sequenza registrata (il
 * "nastro" riproducibile); gmb_replay_list elenca le sessioni. Parita' con
 * Basic.Agents.ReplayRegistry (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include "gmb_expr.h"   /* valutatore runtime condiviso (E1 replay automatico) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_REPLAY_MAX 32
#define GMB_REPLAY_EVENTS_MAX 64
#define GMB_REPLAY_ID_MAX 64
#define GMB_REPLAY_EVENT_MAX 256

typedef struct {
    char id[GMB_REPLAY_ID_MAX];
    int recording;
    char events[GMB_REPLAY_EVENTS_MAX][GMB_REPLAY_EVENT_MAX];
    int count;
} gmb_replay_t;

static gmb_replay_t g_replay[GMB_REPLAY_MAX];
static int g_replay_count = 0;
static int g_replay_current = -1;
static pthread_mutex_t g_replay_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_replay_find_locked(const char* id)
{
    for (int i = 0; i < g_replay_count; i++)
        if (strcasecmp(g_replay[i].id, id) == 0) return i;
    return -1;
}

int gmb_replay_start(const char* session_id)
{
    if (!session_id || !*session_id) return 0;
    pthread_mutex_lock(&g_replay_lock);
    int idx = gmb_replay_find_locked(session_id);
    if (idx < 0)
    {
        if (g_replay_count >= GMB_REPLAY_MAX) { pthread_mutex_unlock(&g_replay_lock); return 0; }
        idx = g_replay_count++;
    }
    memset(&g_replay[idx], 0, sizeof(g_replay[idx]));
    snprintf(g_replay[idx].id, GMB_REPLAY_ID_MAX, "%s", session_id);
    g_replay[idx].recording = 1;
    g_replay_current = idx;
    pthread_mutex_unlock(&g_replay_lock);
    return 1;
}

int gmb_replay_record(const char* event)
{
    pthread_mutex_lock(&g_replay_lock);
    int ok = 0;
    if (g_replay_current >= 0 && g_replay[g_replay_current].recording)
    {
        gmb_replay_t* s = &g_replay[g_replay_current];
        if (s->count < GMB_REPLAY_EVENTS_MAX)
        {
            snprintf(s->events[s->count], GMB_REPLAY_EVENT_MAX, "%s", event ? event : "");
            s->count++;
            ok = 1;
        }
    }
    pthread_mutex_unlock(&g_replay_lock);
    return ok;
}

int gmb_replay_stop(void)
{
    pthread_mutex_lock(&g_replay_lock);
    int ok = 0;
    if (g_replay_current >= 0 && g_replay[g_replay_current].recording)
    {
        g_replay[g_replay_current].recording = 0;
        g_replay_current = -1;
        ok = 1;
    }
    pthread_mutex_unlock(&g_replay_lock);
    return ok;
}

static void gmb_replay_escape(const char* s, char* out, size_t cap)
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

char* gmb_replay_get(const char* session_id)
{
    size_t cap = 64 + (size_t)GMB_REPLAY_EVENTS_MAX * (GMB_REPLAY_EVENT_MAX * 2 + 4);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_replay_lock);
    int idx = (session_id && *session_id) ? gmb_replay_find_locked(session_id) : -1;
    if (idx < 0) { snprintf(buf, 3, "{}"); }
    else
    {
        gmb_replay_t* s = &g_replay[idx];
        char eid[GMB_REPLAY_ID_MAX * 2];
        gmb_replay_escape(s->id, eid, sizeof(eid));
        size_t off = (size_t)snprintf(buf, cap, "{\"session\":\"%s\",\"recording\":%s,\"events\":[",
            eid, s->recording ? "true" : "false");
        for (int i = 0; i < s->count; i++)
        {
            char ee[GMB_REPLAY_EVENT_MAX * 2];
            gmb_replay_escape(s->events[i], ee, sizeof(ee));
            off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\"", i > 0 ? "," : "", ee);
        }
        snprintf(buf + off, cap - off, "]}");
    }
    pthread_mutex_unlock(&g_replay_lock);
    return buf;
}

char* gmb_replay_list(void)
{
    size_t cap = 64 + (size_t)GMB_REPLAY_MAX * (GMB_REPLAY_ID_MAX * 2 + 64);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_replay_lock);
    int order[GMB_REPLAY_MAX];
    int n = g_replay_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_replay[order[i]].id, g_replay[order[j]].id) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = (size_t)snprintf(buf, cap, "{\"sessions\":[");
    for (int k = 0; k < n; k++)
    {
        gmb_replay_t* s = &g_replay[order[k]];
        char eid[GMB_REPLAY_ID_MAX * 2];
        gmb_replay_escape(s->id, eid, sizeof(eid));
        off += (size_t)snprintf(buf + off, cap - off, "%s{\"session\":\"%s\",\"recording\":%s,\"count\":%d}",
            k > 0 ? "," : "", eid, s->recording ? "true" : "false", s->count);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_replay_lock);
    return buf;
}

/* Replay: riproduce gli eventi di session_id in una NUOVA sessione target_id. Ritorna il numero
 * di eventi riprodotti (0 se la sorgente non esiste). */
int gmb_replay_run(const char* session_id, const char* target_id)
{
    if (!session_id || !*session_id || !target_id || !*target_id) return 0;
    char events[GMB_REPLAY_EVENTS_MAX][GMB_REPLAY_EVENT_MAX];
    int n = 0;
    pthread_mutex_lock(&g_replay_lock);
    int idx = gmb_replay_find_locked(session_id);
    if (idx >= 0)
    {
        n = g_replay[idx].count;
        for (int i = 0; i < n; i++) snprintf(events[i], GMB_REPLAY_EVENT_MAX, "%s", g_replay[idx].events[i]);
    }
    pthread_mutex_unlock(&g_replay_lock);
    if (n == 0) return 0;
    gmb_replay_start(target_id);
    for (int i = 0; i < n; i++) gmb_replay_record(events[i]);
    gmb_replay_stop();
    return n;
}

/* E1 — RIESECUZIONE AUTOMATICA degli eventi di una sessione: ogni evento è valutato dal valutatore
 * runtime condiviso (gmb_expr_eval_ex) e l'esito è registrato come evento "ok: …" / "error: …" nella
 * sessione "<sessione>#auto" (leggibile con AI.REPLAYGET). Mai silenzioso: i fallimenti restano
 * tracciati. Ritorna il numero di eventi rieseguiti con successo. Speculare a ReplayRegistry.Auto. */
double gmb_replay_auto(const char* session_id)
{
    if (!session_id || !*session_id) return 0.0;
    char events[GMB_REPLAY_EVENTS_MAX][GMB_REPLAY_EVENT_MAX];
    int n = 0;
    pthread_mutex_lock(&g_replay_lock);
    int idx = gmb_replay_find_locked(session_id);
    if (idx >= 0)
    {
        n = g_replay[idx].count;
        for (int i = 0; i < n; i++) snprintf(events[i], GMB_REPLAY_EVENT_MAX, "%s", g_replay[idx].events[i]);
    }
    pthread_mutex_unlock(&g_replay_lock);
    if (n == 0) return 0.0;
    char target[GMB_REPLAY_ID_MAX + 8];
    snprintf(target, sizeof(target), "%s#auto", session_id);
    gmb_replay_start(target);
    int ok = 0;
    for (int i = 0; i < n; i++)
    {
        gmb_val_t v = gmb_val_nil();
        int rc = gmb_expr_eval_ex(events[i], NULL, NULL, NULL, NULL, &v);
        char line[GMB_REPLAY_EVENT_MAX + 64];
        /* Contratto CONDIVISO col C#: "eseguito" = valutazione OK **e** valore non-NIL; un evento
         * non eseguibile è registrato come ERRORE (mai "ok" silenzioso). */
        if (rc == GMB_EXPR_OK && v.kind != GMB_TYPE_NIL) { ok++; snprintf(line, sizeof(line), "ok: %s", events[i]); }
        else snprintf(line, sizeof(line), "error: %s → non eseguibile", events[i]);
        gmb_replay_record(line);
    }
    gmb_replay_stop();
    return (double)ok;
}

int gmb_replay_clear(void)
{
    pthread_mutex_lock(&g_replay_lock);
    int n = g_replay_count;
    g_replay_count = 0;
    g_replay_current = -1;
    pthread_mutex_unlock(&g_replay_lock);
    return n;
}

/* Persistenza: record `session\x1frecording\x1fevent1\x1devent2...` separati da \x1e. */
char* gmb_replay_export(void)
{
    pthread_mutex_lock(&g_replay_lock);
    size_t cap = 64;
    for (int i = 0; i < g_replay_count; i++)
    {
        cap += strlen(g_replay[i].id) + 4;
        for (int j = 0; j < g_replay[i].count; j++) cap += strlen(g_replay[i].events[j]) + 1;
    }
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_replay_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_replay_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s\x1f", g_replay[i].id, g_replay[i].recording ? "1" : "0");
        for (int j = 0; j < g_replay[i].count; j++)
        {
            if (j > 0) out[off++] = '\x1d';
            off += (size_t)snprintf(out + off, cap - off, "%s", g_replay[i].events[j]);
        }
    }
    pthread_mutex_unlock(&g_replay_lock);
    return out;
}

void gmb_replay_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        char* f3 = strchr(f2, '\x1f'); if (!f3) continue; *f3++ = '\0';
        gmb_replay_start(rec);
        if (*f3)
        {
            char* save2 = NULL;
            for (char* ev = strtok_r(f3, "\x1d", &save2); ev; ev = strtok_r(NULL, "\x1d", &save2))
                gmb_replay_record(ev);
        }
        if (strcmp(f2, "1") != 0) gmb_replay_stop();
    }
    free(copy);
}
