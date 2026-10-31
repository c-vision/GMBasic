/* gmb_artifact.c — Artifact Store (mondo C).
 *
 * Orchestrazione agenti (A6): registro di artefatti REFERENZIABILI senza duplicarne il
 * contenuto. Ogni artefatto è (nome, tipo, riferimento). gmb_artifact_put registra un file;
 * gmb_artifact_link registra una URI; get risolve; list elenca; remove elimina. Parita' con
 * Basic.Agents.ArtifactStore (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_ART_MAX 128
#define GMB_ART_NAME_MAX 64
#define GMB_ART_REF_MAX 512

typedef struct {
    char name[GMB_ART_NAME_MAX];
    char kind[8];   /* "file" | "link" */
    char ref[GMB_ART_REF_MAX];
} gmb_artifact_t;

static gmb_artifact_t g_arts[GMB_ART_MAX];
static int g_art_count = 0;
static pthread_mutex_t g_art_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_artifact_find_locked(const char* name)
{
    for (int i = 0; i < g_art_count; i++)
        if (strcasecmp(g_arts[i].name, name) == 0) return i;
    return -1;
}

static int gmb_artifact_store(const char* name, const char* kind, const char* ref)
{
    if (!name || !*name || !ref || !*ref) return 0;
    pthread_mutex_lock(&g_art_lock);
    int idx = gmb_artifact_find_locked(name);
    if (idx < 0)
    {
        if (g_art_count >= GMB_ART_MAX) { pthread_mutex_unlock(&g_art_lock); return 0; }
        idx = g_art_count++;
    }
    snprintf(g_arts[idx].name, sizeof(g_arts[idx].name), "%s", name);
    snprintf(g_arts[idx].kind, sizeof(g_arts[idx].kind), "%s", kind);
    snprintf(g_arts[idx].ref, sizeof(g_arts[idx].ref), "%s", ref);
    pthread_mutex_unlock(&g_art_lock);
    return 1;
}

int gmb_artifact_put(const char* name, const char* path) { return gmb_artifact_store(name, "file", path); }
int gmb_artifact_link(const char* name, const char* uri) { return gmb_artifact_store(name, "link", uri); }

/* Riferimento dell'artefatto (malloc; "" se assente). */
char* gmb_artifact_get(const char* name)
{
    char* out = (char*)malloc(GMB_ART_REF_MAX);
    if (!out) return NULL;
    out[0] = '\0';
    if (name && *name)
    {
        pthread_mutex_lock(&g_art_lock);
        int idx = gmb_artifact_find_locked(name);
        if (idx >= 0) snprintf(out, GMB_ART_REF_MAX, "%s", g_arts[idx].ref);
        pthread_mutex_unlock(&g_art_lock);
    }
    return out;
}

/* Tipo dell'artefatto (malloc; "" se assente). */
char* gmb_artifact_kind(const char* name)
{
    char* out = (char*)malloc(8);
    if (!out) return NULL;
    out[0] = '\0';
    if (name && *name)
    {
        pthread_mutex_lock(&g_art_lock);
        int idx = gmb_artifact_find_locked(name);
        if (idx >= 0) snprintf(out, 8, "%s", g_arts[idx].kind);
        pthread_mutex_unlock(&g_art_lock);
    }
    return out;
}

int gmb_artifact_remove(const char* name)
{
    if (!name || !*name) return 0;
    pthread_mutex_lock(&g_art_lock);
    int idx = gmb_artifact_find_locked(name);
    if (idx < 0) { pthread_mutex_unlock(&g_art_lock); return 0; }
    for (int j = idx; j < g_art_count - 1; j++) g_arts[j] = g_arts[j + 1];
    g_art_count--;
    pthread_mutex_unlock(&g_art_lock);
    return 1;
}

static void gmb_artifact_escape(const char* s, char* out, size_t cap)
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

/* JSON di tutti gli artefatti, ordinati per nome. malloc. */
char* gmb_artifact_list(void)
{
    size_t cap = 64 + (size_t)GMB_ART_MAX * (GMB_ART_NAME_MAX * 2 + GMB_ART_REF_MAX * 2 + 64);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_art_lock);
    int order[GMB_ART_MAX];
    int n = g_art_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_arts[order[i]].name, g_arts[order[j]].name) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }
    size_t off = (size_t)snprintf(buf, cap, "{\"artifacts\":[");
    for (int k = 0; k < n; k++)
    {
        char en[GMB_ART_NAME_MAX * 2], ek[16], er[GMB_ART_REF_MAX * 2];
        gmb_artifact_escape(g_arts[order[k]].name, en, sizeof(en));
        gmb_artifact_escape(g_arts[order[k]].kind, ek, sizeof(ek));
        gmb_artifact_escape(g_arts[order[k]].ref, er, sizeof(er));
        off += (size_t)snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"kind\":\"%s\",\"ref\":\"%s\"}", k > 0 ? "," : "", en, ek, er);
    }
    snprintf(buf + off, cap - off, "]}");
    pthread_mutex_unlock(&g_art_lock);
    return buf;
}

void gmb_artifact_clear(void)
{
    pthread_mutex_lock(&g_art_lock);
    g_art_count = 0;
    pthread_mutex_unlock(&g_art_lock);
}

/* Persistenza: record `name\x1fkind\x1fref` separati da \x1e. */
char* gmb_artifact_export(void)
{
    pthread_mutex_lock(&g_art_lock);
    size_t cap = 64;
    for (int i = 0; i < g_art_count; i++)
        cap += strlen(g_arts[i].name) + strlen(g_arts[i].kind) + strlen(g_arts[i].ref) + 4;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_art_lock); return NULL; }
    size_t off = 0; out[0] = '\0';
    for (int i = 0; i < g_art_count; i++)
    {
        if (i > 0) out[off++] = '\x1e';
        off += (size_t)snprintf(out + off, cap - off, "%s\x1f%s\x1f%s", g_arts[i].name, g_arts[i].kind, g_arts[i].ref);
    }
    pthread_mutex_unlock(&g_art_lock);
    return out;
}

void gmb_artifact_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    for (char* rec = strtok_r(copy, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        char* f3 = strchr(f2, '\x1f'); if (!f3) continue; *f3++ = '\0';
        gmb_artifact_store(rec, f2, f3);
    }
    free(copy);
}
