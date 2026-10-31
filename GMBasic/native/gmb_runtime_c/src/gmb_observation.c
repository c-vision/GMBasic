/* gmb_observation.c — Registry ContextObject (todo AI-Runtime consolidato, P1).
 * Semantica dei testi JSON fissata in gmb_observation.h; il lato C# osserva la stessa
 * forma. Hash FNV-1a 64 bit = 16 hex minuscole (il C# deve batterlo bit per bit).
 * Capacità 256, drop-oldest (gli id restano monotoni: un id non viene mai riusato). */
#include "gmb_observation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define GMB_OBS_MAX 256
#define GMB_OBS_PATH_CAP 512
#define GMB_OBS_TYPE_CAP 8

typedef struct {
    int id;
    char type[GMB_OBS_TYPE_CAP];
    char source[GMB_OBS_PATH_CAP];
    char artifact[GMB_OBS_PATH_CAP];
    char hash[17];
    long size;
} gmb_obs_entry_t;

static gmb_obs_entry_t g_obs[GMB_OBS_MAX];
static int g_obs_n = 0;          /* entries used (<= max) */
static int g_obs_head = 0;       /* index of oldest entry when full (ring) */
static int g_obs_next_id = 1;

static void g_obs_fnv1a64(const unsigned char* data, size_t n, char out[17]) {
    unsigned long long h = 14695981039346656037ULL; /* FNV-1a64 offset basis (64 bit) */
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned long long)data[i];
        h *= 1099511628211ULL;
    }
    for (int i = 15; i >= 0; i--) { out[i] = "0123456789abcdef"[(h >> ((15 - i) * 4)) & 0xF]; }
    out[16] = 0;
}

static gmb_obs_entry_t* g_obs_slot(void) {
    if (g_obs_n < GMB_OBS_MAX) return &g_obs[g_obs_n++];
    gmb_obs_entry_t* s = &g_obs[g_obs_head];
    g_obs_head = (g_obs_head + 1) % GMB_OBS_MAX;   /* drop-oldest */
    return s;
}

/* -1 se il file non esiste/è illeggibile (nessuna osservazione registrata). */
int gmb_obs_record_file(const char* path) {
    if (!path || !*path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    struct stat st;
    long size = -1;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) size = (long)st.st_size;
    if (size < 0) { fclose(f); return -1; }
    char* buf = (char*)malloc((size_t)size + 1u);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    char hash[17];
    g_obs_fnv1a64((const unsigned char*)buf, got, hash);
    free(buf);
    gmb_obs_entry_t* e = g_obs_slot();
    memset(e, 0, sizeof(*e));
    e->id = g_obs_next_id++;
    strncpy(e->type, "file", GMB_OBS_TYPE_CAP - 1);
    strncpy(e->source, path, GMB_OBS_PATH_CAP - 1);
    strncpy(e->artifact, path, GMB_OBS_PATH_CAP - 1);
    memcpy(e->hash, hash, 17);
    e->size = (long)got;
    return e->id;
}

/* Tiene le ultime tail_lines righe del file come contenuto (tail per lettura, non byte). */
/* Registra un ContextObject con TYPE esplicito (nuove fonti OBSERVE: test/database/process/browser)
 * hashandо l'artefatto. -1 se il file non è leggibile (errore esplicito a monte). Speculare a
 * ObservationRegistry.RecordTyped (C#). */
int gmb_obs_record_typed(const char* type, const char* source, const char* artifact_path)
{
    if (!artifact_path || !*artifact_path) return -1;
    FILE* f = fopen(artifact_path, "rb");
    if (!f) return -1;
    struct stat st;
    long size = -1;
    if (stat(artifact_path, &st) == 0 && S_ISREG(st.st_mode)) size = (long)st.st_size;
    if (size < 0) { fclose(f); return -1; }
    char* buf = (char*)malloc((size_t)size + 1u);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    char hash[17];
    g_obs_fnv1a64((const unsigned char*)buf, got, hash);
    free(buf);
    gmb_obs_entry_t* e = g_obs_slot();
    memset(e, 0, sizeof(*e));
    e->id = g_obs_next_id++;
    strncpy(e->type, (type && *type) ? type : "file", GMB_OBS_TYPE_CAP - 1);
    strncpy(e->source, source ? source : "", GMB_OBS_PATH_CAP - 1);
    strncpy(e->artifact, artifact_path, GMB_OBS_PATH_CAP - 1);
    memcpy(e->hash, hash, 17);
    e->size = (long)got;
    return e->id;
}

int gmb_obs_record_log(const char* path, int tail_lines) {
    if (!path || !*path) return -1;
    if (tail_lines <= 0) tail_lines = 200;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    char* all = NULL; size_t all_n = 0, cap = 0;
    char chunk[4096];
    for (;;) {
        size_t r = fread(chunk, 1, sizeof(chunk), f);
        if (!r) break;
        if (all_n + r + 1 > cap) {
            size_t nc = cap ? cap * 2 : 8192;
            while (nc < all_n + r + 1) nc *= 2;
            char* nb = (char*)realloc(all, nc);
            if (!nb) { free(all); fclose(f); return -1; }
            all = nb; cap = nc;
        }
        memcpy(all + all_n, chunk, r);
        all_n += r;
    }
    fclose(f);
    if (!all) { all_n = 0; all = (char*)malloc(1); }
    all[all_n] = 0;
    /* contenuto = ultime tail_lines righe, newline finali esclusi */
    size_t len = all_n;
    while (len > 0 && (all[len - 1] == '\n' || all[len - 1] == '\r')) len--;
    size_t start = 0;
    int nl = 0;
    for (size_t i = len; i-- > 0; ) {
        if (all[i] == '\n') { nl++; if (nl >= tail_lines) { start = i + 1; break; } }
    }
    const char* content = all + start;
    size_t clen = len - start;
    char hash[17];
    g_obs_fnv1a64((const unsigned char*)content, clen, hash);
    gmb_obs_entry_t* e = g_obs_slot();
    memset(e, 0, sizeof(*e));
    e->id = g_obs_next_id++;
    strncpy(e->type, "log", GMB_OBS_TYPE_CAP - 1);
    strncpy(e->source, path, GMB_OBS_PATH_CAP - 1);
    strncpy(e->artifact, path, GMB_OBS_PATH_CAP - 1);
    memcpy(e->hash, hash, 17);
    e->size = (long)clen;
    free(all);
    return e->id;
}

static const gmb_obs_entry_t* g_obs_find(int id) {
    for (int i = 0; i < g_obs_n; i++) if (g_obs[i].id == id) return &g_obs[i];
    return NULL;
}

/* Iterazione in ordine di inserimento (drop-oldest: gli slot riciclati mostrano l'entry
 * corrente; gli id monotoni rendono la lista stabile in pratica). */
static void g_obs_order(int* out) {
    int k = 0;
    if (g_obs_n < GMB_OBS_MAX) { for (int i = 0; i < g_obs_n; i++) out[k++] = i; return; }
    for (int i = g_obs_head; i < GMB_OBS_MAX; i++) out[k++] = i;
    for (int i = 0; i < g_obs_head; i++) out[k++] = i;
}

gmb_string_t gmb_obs_latest_id(void) {
    if (g_obs_next_id <= 1) return gmb_string_alloc("");
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g_obs_next_id - 1);
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_obs_json(int id) {
    const gmb_obs_entry_t* e = g_obs_find(id);
    char buf[1200];
    if (!e) return gmb_string_alloc("{\"error\":\"unknown observation\"}");
    snprintf(buf, sizeof(buf),
        "{\"id\":%d,\"type\":\"%s\",\"source\":\"%s\",\"artifact\":\"%s\",\"hash\":\"%s\",\"size\":%ld}",
        e->id, e->type, e->source, e->artifact, e->hash, e->size);
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_obs_hash(int id) {
    const gmb_obs_entry_t* e = g_obs_find(id);
    return gmb_string_alloc(e ? e->hash : "");
}

int gmb_obs_count(void) { return g_obs_n; }

gmb_string_t gmb_obs_list(void) {
    int order[GMB_OBS_MAX];
    g_obs_order(order);
    size_t cap = 64 + (size_t)g_obs_n * (GMB_OBS_PATH_CAP + 32);
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("");
    size_t p = 0;
    for (int i = 0; i < g_obs_n; i++) {
        const gmb_obs_entry_t* e = &g_obs[order[i]];
        int w = snprintf(buf + p, cap - p, "%s%d|%s|%s", i ? "\n" : "", e->id, e->type, e->hash);
        if (w > 0 && (size_t)w < cap - p) p += (size_t)w;
    }
    buf[p] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

gmb_string_t gmb_obs_file(int id) {
    const gmb_obs_entry_t* e = g_obs_find(id);
    return gmb_string_alloc(e ? e->artifact : "");
}
