/* gmb_checkpoint.c — Checkpoint/recovery (mondo C).
 *
 * Orchestrazione agenti (B7): salvataggio di uno stato di sessione (checkpoint) e ripresa
 * (resume) per riprendere un lavoro agentico interrotto. Parita' con
 * Basic.Agents.CheckpointRegistry (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_CP_MAX 64
#define GMB_CP_ID_MAX 64
#define GMB_CP_STATE_MAX 4096

typedef struct {
    char session[GMB_CP_ID_MAX];
    char state[GMB_CP_STATE_MAX];
} gmb_cp_t;

static gmb_cp_t g_cp[GMB_CP_MAX];
static int g_cp_count = 0;
static pthread_mutex_t g_cp_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_cp_find_locked(const char* session)
{
    for (int i = 0; i < g_cp_count; i++)
        if (strcasecmp(g_cp[i].session, session) == 0) return i;
    return -1;
}

int gmb_checkpoint_save(const char* session_id, const char* state)
{
    if (!session_id || !*session_id) return 0;
    pthread_mutex_lock(&g_cp_lock);
    int idx = gmb_cp_find_locked(session_id);
    if (idx < 0)
    {
        if (g_cp_count >= GMB_CP_MAX) { pthread_mutex_unlock(&g_cp_lock); return 0; }
        idx = g_cp_count++;
    }
    snprintf(g_cp[idx].session, GMB_CP_ID_MAX, "%s", session_id);
    snprintf(g_cp[idx].state, GMB_CP_STATE_MAX, "%s", state ? state : "");
    pthread_mutex_unlock(&g_cp_lock);
    return 1;
}

char* gmb_checkpoint_resume(const char* session_id)
{
    char* out = (char*)malloc(GMB_CP_STATE_MAX);
    if (!out) return NULL;
    out[0] = '\0';
    if (session_id && *session_id)
    {
        pthread_mutex_lock(&g_cp_lock);
        int idx = gmb_cp_find_locked(session_id);
        if (idx >= 0) snprintf(out, GMB_CP_STATE_MAX, "%s", g_cp[idx].state);
        pthread_mutex_unlock(&g_cp_lock);
    }
    return out;
}

static void gmb_cp_escape(const char* s, char* out, size_t cap)
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

char* gmb_checkpoint_list(void)
{
    size_t cap = 64 + (size_t)GMB_CP_MAX * (GMB_CP_ID_MAX * 2 + GMB_CP_STATE_MAX * 2 + 32);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_cp_lock);
    int order[GMB_CP_MAX];
    int n = g_cp_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_cp[order[i]].session, g_cp[order[j]].session) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = (size_t)snprintf(buf, cap, "{\"checkpoints\":[");
    for (int k = 0; k < n; k++)
    {
        char es[GMB_CP_ID_MAX * 2], et[GMB_CP_STATE_MAX * 2];
        gmb_cp_escape(g_cp[order[k]].session, es, sizeof(es));
        gmb_cp_escape(g_cp[order[k]].state, et, sizeof(et));
        off += (size_t)snprintf(buf + off, cap - off, "%s{\"session\":\"%s\",\"state\":\"%s\"}", k > 0 ? "," : "", es, et);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_cp_lock);
    return buf;
}

int gmb_checkpoint_clear(void)
{
    pthread_mutex_lock(&g_cp_lock);
    int n = g_cp_count;
    g_cp_count = 0;
    pthread_mutex_unlock(&g_cp_lock);
    return n;
}

/* Persistenza: record `session\x1fstate` separati da \x1e. */
char* gmb_checkpoint_export(void)
{
    pthread_mutex_lock(&g_cp_lock);
    size_t cap = 64;
    for (int i = 0; i < g_cp_count; i++) cap += strlen(g_cp[i].session) + strlen(g_cp[i].state) + 4;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_cp_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_cp_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s", g_cp[i].session, g_cp[i].state);
    }
    pthread_mutex_unlock(&g_cp_lock);
    return out;
}

void gmb_checkpoint_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        gmb_checkpoint_save(rec, f2);
    }
    free(copy);
}
