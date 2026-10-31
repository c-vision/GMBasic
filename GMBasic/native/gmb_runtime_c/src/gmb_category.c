/* gmb_category.c — Registro delle categorie di task → provider/modello (mondo C).
 *
 * Orchestrazione agenti (A1): tassonomia di categorie NOMINATE (deep/quick/ultrabrain/
 * research/coding/testing/visual) con routing esplicito categoria→provider e fallback.
 * Parita' di comportamento con Basic.AI.Providers.CategoryRegistry (C#): stesse operazioni,
 * stesso formato JSON di gmb_category_list.
 *
 * Regola: questo registro e' solo POLICY (categoria → provider/modello). La conoscenza delle
 * categorie e' separata dall'esecuzione; il dispatch effettivo resta a gmb_brain_*. Thread-safe.
 *
 * Runtime CONDIVISO: VM/C e i backend C99 chiamano le stesse gmb_category_* (mai duplicazioni
 * inline nei backend — AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_CATEGORY_MAX 128
#define GMB_CATEGORY_NAME_MAX 64
#define GMB_CATEGORY_PROV_MAX 128

typedef struct {
    char category[GMB_CATEGORY_NAME_MAX];
    char provider[GMB_CATEGORY_PROV_MAX];
    char fallback[GMB_CATEGORY_PROV_MAX];
} gmb_category_entry_t;

static gmb_category_entry_t g_cat_entries[GMB_CATEGORY_MAX];
static int g_cat_count = 0;
static int g_cat_auto = 0;
static pthread_mutex_t g_cat_lock = PTHREAD_MUTEX_INITIALIZER;

static int gmb_category_find_locked(const char* category)
{
    for (int i = 0; i < g_cat_count; i++)
        if (strcasecmp(g_cat_entries[i].category, category) == 0) return i;
    return -1;
}

/* Associa una categoria a un provider/modello. 1 = ok, 0 = errore (categoria/provider vuoti
 * o registro pieno). Parita' CategoryRegistry.Set (C#). */
int gmb_category_set(const char* category, const char* provider)
{
    if (!category || !*category || !provider || !*provider) return 0;
    pthread_mutex_lock(&g_cat_lock);
    int idx = gmb_category_find_locked(category);
    if (idx < 0)
    {
        if (g_cat_count >= GMB_CATEGORY_MAX) { pthread_mutex_unlock(&g_cat_lock); return 0; }
        idx = g_cat_count++;
        snprintf(g_cat_entries[idx].category, sizeof(g_cat_entries[idx].category), "%s", category);
    }
    snprintf(g_cat_entries[idx].provider, sizeof(g_cat_entries[idx].provider), "%s", provider);
    pthread_mutex_unlock(&g_cat_lock);
    return 1;
}

/* Imposta il provider di fallback per una categoria. 1 = ok, 0 = errore. */
int gmb_category_fallback(const char* category, const char* provider)
{
    if (!category || !*category) return 0;
    pthread_mutex_lock(&g_cat_lock);
    int idx = gmb_category_find_locked(category);
    if (idx < 0)
    {
        if (g_cat_count >= GMB_CATEGORY_MAX) { pthread_mutex_unlock(&g_cat_lock); return 0; }
        idx = g_cat_count++;
        snprintf(g_cat_entries[idx].category, sizeof(g_cat_entries[idx].category), "%s", category);
    }
    snprintf(g_cat_entries[idx].fallback, sizeof(g_cat_entries[idx].fallback), "%s", provider ? provider : "");
    pthread_mutex_unlock(&g_cat_lock);
    return 1;
}

/* Abilita/disabilita il routing automatico. Ritorna il nuovo stato (1/0). */
int gmb_category_auto(int flag)
{
    pthread_mutex_lock(&g_cat_lock);
    g_cat_auto = flag != 0;
    int v = g_cat_auto;
    pthread_mutex_unlock(&g_cat_lock);
    return v;
}

/* Provider associato alla categoria (malloc; "" se assente o routing disabilitato). */
char* gmb_category_pick(const char* category)
{
    char* out = (char*)malloc(1);
    if (!out) return NULL;
    out[0] = '\0';
    if (!category || !*category) return out;
    pthread_mutex_lock(&g_cat_lock);
    if (g_cat_auto)
    {
        int idx = gmb_category_find_locked(category);
        if (idx >= 0)
        {
            size_t len = strlen(g_cat_entries[idx].provider) + 1;
            char* p = (char*)realloc(out, len);
            if (p) { out = p; snprintf(out, len, "%s", g_cat_entries[idx].provider); }
        }
    }
    pthread_mutex_unlock(&g_cat_lock);
    return out;
}

/* Fallback associato alla categoria (malloc; "" se assente). */
char* gmb_category_pick_fallback(const char* category)
{
    char* out = (char*)malloc(1);
    if (!out) return NULL;
    out[0] = '\0';
    if (!category || !*category) return out;
    pthread_mutex_lock(&g_cat_lock);
    int idx = gmb_category_find_locked(category);
    if (idx >= 0)
    {
        size_t len = strlen(g_cat_entries[idx].fallback) + 1;
        char* p = (char*)realloc(out, len);
        if (p) { out = p; snprintf(out, len, "%s", g_cat_entries[idx].fallback); }
    }
    pthread_mutex_unlock(&g_cat_lock);
    return out;
}

/* True se il routing automatico e' abilitato. */
int gmb_category_auto_enabled(void)
{
    pthread_mutex_lock(&g_cat_lock);
    int v = g_cat_auto;
    pthread_mutex_unlock(&g_cat_lock);
    return v;
}

/* JSON deterministico delle categorie registrate, ordinate per nome (parita' ListJson C#).
 * Formato: {"auto":bool,"categories":{"<cat>":{"provider":"...","fallback":"..."},...}} */
char* gmb_category_list(void)
{
    pthread_mutex_lock(&g_cat_lock);
    /* Copia indice per ordinamento (non tocca l'ordine di registrazione). */
    int order[GMB_CATEGORY_MAX];
    int n = g_cat_count;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(g_cat_entries[order[i]].category, g_cat_entries[order[j]].category) > 0)
            { int t = order[i]; order[i] = order[j]; order[j] = t; }

    size_t cap = 256 + (size_t)n * (GMB_CATEGORY_NAME_MAX + 2 * GMB_CATEGORY_PROV_MAX + 64);
    char* buf = (char*)malloc(cap);
    if (!buf) { pthread_mutex_unlock(&g_cat_lock); return NULL; }
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "{\"auto\":%s,\"categories\":{", g_cat_auto ? "true" : "false");
    for (int k = 0; k < n; k++)
    {
        gmb_category_entry_t* e = &g_cat_entries[order[k]];
        off += (size_t)snprintf(buf + off, cap - off, "%s\"%s\":{\"provider\":\"%s\",\"fallback\":\"%s\"}",
            k > 0 ? "," : "", e->category, e->provider, e->fallback);
    }
    snprintf(buf + off, cap - off, "}}");
    pthread_mutex_unlock(&g_cat_lock);
    return buf;
}

/* Svuota il registro (test/harness). */
void gmb_category_clear(void)
{
    pthread_mutex_lock(&g_cat_lock);
    g_cat_count = 0;
    g_cat_auto = 0;
    pthread_mutex_unlock(&g_cat_lock);
}

/* Persistenza: `auto\x1ecat\x1fprovider\x1ffallback...`. */
char* gmb_category_export(void)
{
    pthread_mutex_lock(&g_cat_lock);
    size_t cap = 64;
    for (int i = 0; i < g_cat_count; i++)
        cap += strlen(g_cat_entries[i].category) + strlen(g_cat_entries[i].provider) + strlen(g_cat_entries[i].fallback) + 4;
    char* out = (char*)malloc(cap);
    if (!out) { pthread_mutex_unlock(&g_cat_lock); return NULL; }
    size_t off = (size_t)snprintf(out, cap, "%d", g_cat_auto);
    for (int i = 0; i < g_cat_count; i++)
        off += (size_t)snprintf(out + off, cap - off, "\x1e%s\x1f%s\x1f%s",
            g_cat_entries[i].category, g_cat_entries[i].provider, g_cat_entries[i].fallback);
    pthread_mutex_unlock(&g_cat_lock);
    return out;
}

void gmb_category_import(const char* state)
{
    if (!state || !*state) return;
    char* copy = strdup(state);
    if (!copy) return;
    char* save = NULL;
    char* first = strtok_r(copy, "\x1e", &save);
    if (first) gmb_category_auto(atoi(first));
    for (char* rec = strtok_r(NULL, "\x1e", &save); rec; rec = strtok_r(NULL, "\x1e", &save))
    {
        char* f2 = strchr(rec, '\x1f'); if (!f2) continue; *f2++ = '\0';
        char* f3 = strchr(f2, '\x1f'); if (!f3) continue; *f3++ = '\0';
        gmb_category_set(rec, f2); gmb_category_fallback(rec, f3);
    }
    free(copy);
}
