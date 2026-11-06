/* gmb_moestream.c — dlopen di libllmengine_moe_stream + API comandi/funzioni per i backend C. */
/* Dl_info/dladdr su Linux sono estensioni GNU di glibc: senza _GNU_SOURCE la struct non esiste. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "gmb_moestream.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include "gmb_model_io.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Memoria disponibile (gmb_runtime_c/gmb_sysinfo.c): iniettata nel provider per lo scheduler M21.
 * Tipo IDENTICO al prototipo in gmb_runtime.h (`unsigned long long`): su Linux aarch64 uint64_t
 * è `unsigned long`, e le due dichiarazioni andavano in conflitto. */
unsigned long long gmb_mem_available_bytes(void);

/* ── ABI nativa (deve combaciare con llmengine_ai_moestream.c) ───────────────────── */
typedef struct {
    int32_t  mode;
    int32_t  device;
    uint64_t cache_mb;
    uint32_t workers;
    uint32_t topn;
    uint32_t cap;
} MsConfig;

typedef struct {
    int32_t  loaded;
    int32_t  mode;
    int32_t  device;
    uint32_t layers;
    uint32_t experts;
    uint64_t bytes_read;
    uint64_t expert_bytes;
    uint64_t resident_bytes;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cache_evictions;
    uint64_t cache_resident_hits;   /* K2: resident hit rate (append: layout ABI) */
    uint64_t pp;
    uint64_t ph;
    uint64_t pw;
    uint64_t unresolvable_experts;  /* K5: esperti assenti/corrotti (output non affidabile) */
} MsStats;

typedef struct {
    void* (*create)(void);
    void  (*destroy)(void*);
    int   (*load)(void*, const char*, const char*, MsConfig*);
    int   (*unload)(void*, const char*);
    int   (*set_cache)(void*, const char*, uint64_t);
    int   (*set_prefetch)(void*, const char*, uint32_t, uint32_t);
    int   (*set_device)(void*, const char*, int32_t);
    int   (*promote)(void*, const char*);
    int   (*demote)(void*, const char*);
    void  (*set_mem_available)(void*, uint64_t);
    void  (*set_priority)(void*, const char*, int32_t);
    int   (*stats)(void*, const char*, MsStats*);
    const char* (*status_str)(int32_t);
    /* K1: trace `.moetrace` (opzionali: dylib più vecchie senza trace restano caricabili). */
    int   (*trace_start)(void*, const char*, const char*);
    int   (*trace_stop)(void*, const char*);
} MsApi;

static MsApi g_api;
static int g_api_loaded = 0;
static int g_api_ok = 0;
static void* g_lib = NULL;
static void* g_prov = NULL;

static void* ms_try(const char* path) {
    if (path == NULL) return NULL;
    return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
}

static void* ms_open_lib(void) {
    const char* env = gmb_opt_env("GMB_MOESTREAM_LIB");
    void* h = ms_try(env);
#if defined(_WIN32)
    const char* fname = "llmengine_moe_stream.dll";
    const char* sub[] = { "win-x64", NULL };
#elif defined(__APPLE__)
    const char* fname = "libllmengine_moe_stream.dylib";
    const char* sub[] = { "osx-arm64", "osx-x64", NULL };
#else
    const char* fname = "libllmengine_moe_stream.so";
    const char* sub[] = { "linux-x64", "linux-arm64", NULL };
#endif
    if (h) return h;
    h = ms_try(fname);
    if (h) return h;
    /* 1) accanto all'eseguibile, risalendo: `build/libs/<rid>/` (checkout) e
     * `~/.gmbasic/libs/<rid>/` (cache, dove il bundle pubblicato mette il motore). */
    {
        Dl_info info;
        if (dladdr((void*)&ms_open_lib, &info) && info.dli_fname) {
            char dir[1024];
            snprintf(dir, sizeof(dir), "%s", info.dli_fname);
            char* slash = strrchr(dir, '/');
            if (slash) *slash = 0;
            char up[1200]; snprintf(up, sizeof(up), "%s", dir);
            for (int i = 0; i < 6 && !h; ++i) {
                for (const char** s = sub; *s != NULL && !h; ++s) {
                    char cand[1400];
                    snprintf(cand, sizeof(cand), "%s/build/libs/%s/%s", up, *s, fname);
                    h = ms_try(cand);
                    if (!h)
                    {
                        snprintf(cand, sizeof(cand), "%s/.gmbasic/libs/%s/%s", up, *s, fname);
                        h = ms_try(cand);
                    }
                }
                char* sl = strrchr(up, '/');
                if (!sl) break;
                *sl = 0;
            }
        }
    }
    /* 2) relativo alla CWD, risalendo */
    for (const char** s = sub; *s != NULL && !h; ++s) {
        char rel[512];
        snprintf(rel, sizeof(rel), "build/libs/%s/%s", *s, fname);
        h = ms_try(rel);
        if (h) break;
        char buf[1024];
        snprintf(buf, sizeof(buf), "../../../../%s", rel); h = ms_try(buf);
        if (!h) { snprintf(buf, sizeof(buf), "../../../%s", rel); h = ms_try(buf); }
        if (!h) { snprintf(buf, sizeof(buf), "../../%s", rel); h = ms_try(buf); }
        if (!h) { snprintf(buf, sizeof(buf), "../%s", rel); h = ms_try(buf); }
    }
    return h;
}

static int ms_ensure(void) {
    if (g_api_loaded) return g_api_ok;
    g_api_loaded = 1;
    g_lib = ms_open_lib();
    if (g_lib == NULL) { g_api_ok = 0; return 0; }
    g_api.create      = (void* (*)(void))dlsym(g_lib, "llmengine_ai_moestream_create");
    g_api.destroy     = (void  (*)(void*))dlsym(g_lib, "llmengine_ai_moestream_destroy");
    g_api.load        = (int (*)(void*, const char*, const char*, MsConfig*))dlsym(g_lib, "llmengine_ai_moestream_load");
    g_api.unload      = (int (*)(void*, const char*))dlsym(g_lib, "llmengine_ai_moestream_unload");
    g_api.set_cache   = (int (*)(void*, const char*, uint64_t))dlsym(g_lib, "llmengine_ai_moestream_set_cache");
    g_api.set_prefetch= (int (*)(void*, const char*, uint32_t, uint32_t))dlsym(g_lib, "llmengine_ai_moestream_set_prefetch");
    g_api.set_device  = (int (*)(void*, const char*, int32_t))dlsym(g_lib, "llmengine_ai_moestream_set_device");
    g_api.promote     = (int (*)(void*, const char*))dlsym(g_lib, "llmengine_ai_moestream_promote");
    g_api.demote      = (int (*)(void*, const char*))dlsym(g_lib, "llmengine_ai_moestream_demote");
    g_api.set_mem_available = (void (*)(void*, uint64_t))dlsym(g_lib, "llmengine_ai_moestream_set_mem_available");
    g_api.set_priority = (void (*)(void*, const char*, int32_t))dlsym(g_lib, "llmengine_ai_moestream_set_priority");
    g_api.stats       = (int (*)(void*, const char*, MsStats*))dlsym(g_lib, "llmengine_ai_moestream_stats");
    g_api.status_str  = (const char* (*)(int32_t))dlsym(g_lib, "llmengine_ai_moestream_status_str");
    g_api.trace_start = (int (*)(void*, const char*, const char*))dlsym(g_lib, "llmengine_ai_moestream_trace_start");
    g_api.trace_stop  = (int (*)(void*, const char*))dlsym(g_lib, "llmengine_ai_moestream_trace_stop");
    g_api_ok = (g_api.create && g_api.destroy && g_api.load && g_api.unload &&
                g_api.set_cache && g_api.set_prefetch && g_api.set_device && g_api.promote && g_api.demote &&
                g_api.stats && g_api.status_str) ? 1 : 0;
    return g_api_ok;
}

int gmb_moestream_available(void) { return ms_ensure(); }

static void* ms_provider(void) {
    if (!ms_ensure()) return NULL;
    if (g_prov == NULL) g_prov = g_api.create();
    return g_prov;
}

static int ms_err(int rc) {
    (void)rc;
    return -1;
}

/* gmb_provider_register è nel runtime (gmb_ai.c): dichiarato qui per non trascinare gmb_runtime.h. */
int gmb_provider_register(const char* role, const char* name, const char* kind, const char* config);

/* Converte un checkpoint HF/MLX in un container `.gmoe` — IN-PROCESS (gmb_model_io_convert).
 * Ritorna 0 su successo, -1 su errore. Nessun subprocess (AGENTS: C#→C via P/Invoke; la logica C
 * si chiama, non si spawna). */
int gmb_moestream_convert(const char* src, const char* dst) {
    return gmb_model_io_convert(src, dst);
}

/* Alias "statement" per il codegen C99 (convenzione gmb_moestream_stmt_*). */
int gmb_moestream_stmt_convert(const char* src, const char* dst) { return gmb_moestream_convert(src, dst); }

/* Quantizza un modello HF/MLX (F32/F16/BF16) in MLX affine — IN-PROCESS (gmb_model_io_quantize).
 * bits 1..8; group 0 => default 64. Ritorna 0 su successo, -1 su errore. Nessun subprocess. */
int gmb_moestream_quantize(const char* src, const char* dst, int bits, int group) {
    return gmb_model_io_quantize(src, dst, bits, group, NULL, 0);
}

int gmb_moestream_stmt_quantize(const char* src, const char* dst, int bits, int group) {
    return gmb_moestream_quantize(src, dst, bits, group);
}

/* Percorso del tool di discovery `gmoe-discover-arch` (runtime MoE): env GMB_MODEL_DISCOVERER, poi
 * in-repo (LLMEnv/LLMEngine/moe/build/gmoe-discover-arch), infine PATH. */
/* Ispeziona (read-only) un modello su disco — IN-PROCESS (gmb_model_io_inspect_print): il report
 * di discovery viene STAMPATO su stdout direttamente, senza `gmoe-discover-arch` come subprocess.
 * Ritorna 0/-1. */
int gmb_moestream_stmt_inspect(const char* dir) {
    if (dir == NULL) return -1;
    /* '~' e percorsi relativi (paths.modelRoot) PRIMA dello stat: altrimenti un percorso
     * portabile come "~/ai/Models/X" non esiste per stat() e fallisce senza arrivare al loader. */
    char rdir[2048];
    gmb_paths_resolve_model(dir, rdir, sizeof rdir);
    if (rdir[0]) dir = rdir;
    struct stat st;
    if (stat(dir, &st) != 0) return -1;
    return gmb_model_io_inspect_print(dir);
}

int gmb_moestream_stmt_load(const char* name, const char* path, int resident) {
    void* p = ms_provider();
    if (p == NULL) { return -1; }
    /* Percorso omesso (`AI MODEL LOAD "m"`): si usa la voce del registro `moe_swap_dir`, che
     * arriva da `AI SET OPTION` o dalla sezione `engine` dei settings. Il percorso esplicito
     * vince sempre; se anche il registro e' vuoto il load resta senza directory (come prima). */
    char optdir[2048];
    if (path == NULL || *path == '\0') {
        if (gmb_opt_moe_swap_dir(optdir, (int)sizeof optdir) == 0 && optdir[0]) path = optdir;
    }
    /* Se `path` è una cartella HF/MLX, convertila una volta in <path>/<nome>.gmoe (artefatto
     * automatico): l'utente passa il modello, non il container. */
    char resolved[1200], rmodel[2048];
    if (path && *path) { gmb_paths_resolve_model(path, rmodel, sizeof rmodel); if (rmodel[0]) path = rmodel; }
    const char* use = path;
    struct stat st;
    if (path != NULL && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(resolved, sizeof(resolved), "%s/%s.gmoe", path, (name && *name) ? name : "model");
        if (stat(resolved, &st) != 0 && gmb_moestream_convert(path, resolved) != 0) return -1;
        use = resolved;
    }
    MsConfig c; memset(&c, 0, sizeof(c));
    c.mode = resident ? 1 : 0;
    if (g_api.set_mem_available) g_api.set_mem_available(p, gmb_mem_available_bytes());  /* M21 scheduler */
    int rc = g_api.load(p, name ? name : "", use ? use : "", &c);
    if (rc != 0) return ms_err(rc);
    /* Il modello caricato è un provider completion "moestream": registralo QUI (AI MODEL LOAD),
     * non via AI REGISTER (che è per i provider HTTP/API). */
    if (name && *name) (void)gmb_provider_register("completion", name, "moestream", use ? use : "");
    return 0;
}

int gmb_moestream_stmt_unload(const char* name) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    int rc = g_api.unload(p, name ? name : "");
    return rc == 0 ? 0 : ms_err(rc);
}

int gmb_moestream_stmt_cache(const char* name, int cache_mb) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    int rc = g_api.set_cache(p, name ? name : "", cache_mb < 0 ? 0 : (uint64_t)cache_mb);
    return rc == 0 ? 0 : ms_err(rc);
}

int gmb_moestream_stmt_prefetch(const char* name, int topn) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    uint32_t t = topn < 0 ? 0u : (uint32_t)topn;
    int rc = g_api.set_prefetch(p, name ? name : "", t, t > 0 ? 64u : 0u);
    return rc == 0 ? 0 : ms_err(rc);
}

int gmb_moestream_stmt_device(const char* name, const char* mode) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    int dev = 0;
    if (mode != NULL) {
        if (strcasecmp(mode, "METAL") == 0) dev = 2;
        else if (strcasecmp(mode, "CPU") == 0) dev = 1;
    }
    int rc = g_api.set_device(p, name ? name : "", dev);
    return rc == 0 ? 0 : ms_err(rc);
}

/* K1: LLM MODEL TRACE nome, path | OFF — registra il trace degli accessi agli esperti.
 * Il trace e' il carburante del replay offline `moe_sim` (sizing cache senza rifare il run). */
int gmb_moestream_stmt_trace(const char* name, const char* path, int on) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    int stop = (!on) || path == NULL || path[0] == '\0' || strcasecmp(path, "OFF") == 0;
    if (stop) {
        if (g_api.trace_stop == NULL) return -1;
        int rc = g_api.trace_stop(p, name ? name : "");
        return rc == 0 ? 0 : ms_err(rc);
    }
    if (g_api.trace_start == NULL) return -1;
    int rc = g_api.trace_start(p, name ? name : "", path);
    return rc == 0 ? 0 : ms_err(rc);
}

/* M21: residenza — PROMOTE (ACTIVE: tutti gli esperti in RAM) / DEMOTE (WARM: on-demand). */
int gmb_moestream_stmt_promote(const char* name) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    if (g_api.set_mem_available) g_api.set_mem_available(p, gmb_mem_available_bytes());  /* M21 scheduler */
    int rc = g_api.promote(p, name ? name : "");
    return rc == 0 ? 0 : ms_err(rc);
}
int gmb_moestream_stmt_demote(const char* name) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    int rc = g_api.demote(p, name ? name : "");
    return rc == 0 ? 0 : ms_err(rc);
}
/* M21 orchestrazione: priorità del modello (più alto = tenuto di più; evict dal più basso). */
int gmb_moestream_stmt_priority(const char* name, int pri) {
    void* p = ms_provider();
    if (p == NULL) return -1;
    if (g_api.set_priority) g_api.set_priority(p, name ? name : "", pri);
    return 0;
}

int gmb_moestream_streamstats_json(const char* name, char* out, size_t cap) {
    if (out == NULL || cap == 0) return -1;
    void* p = ms_provider();
    if (p == NULL) { snprintf(out, cap, "{\"ok\":false,\"error\":\"libreria non disponibile\"}"); return -1; }
    MsStats s; memset(&s, 0, sizeof(s));
    int rc = g_api.stats(p, name ? name : "", &s);
    if (rc != 0) {
        const char* e = g_api.status_str ? g_api.status_str(rc) : "errore";
        snprintf(out, cap, "{\"ok\":false,\"error\":\"%s\"}", e ? e : "errore");
        return rc;
    }
    const char* mode = (s.mode == 1) ? "resident" : "swap";
    const char* dev = (s.device == 2) ? "metal" : (s.device == 1) ? "cpu" : "auto";
    double hit = (s.cache_hits + s.cache_misses) > 0
        ? 100.0 * (double)s.cache_hits / (double)(s.cache_hits + s.cache_misses) : 0.0;
    double rec = s.pp > 0 ? 100.0 * (double)s.ph / (double)s.pp : 0.0;
    double resrate = s.cache_hits > 0
        ? 100.0 * (double)s.cache_resident_hits / (double)s.cache_hits : 0.0;
    snprintf(out, cap,
        "{\"ok\":true,\"model\":\"%s\",\"mode\":\"%s\",\"device\":\"%s\",\"layers\":%u,\"experts\":%u,"
        "\"bytesRead\":%llu,\"expertBytes\":%llu,\"residentBytes\":%llu,"
        "\"cacheHits\":%llu,\"cacheMisses\":%llu,\"cacheEvictions\":%llu,\"cacheHitPct\":%.1f,"
        "\"residentHits\":%llu,\"residentHitPct\":%.1f,"
        "\"prefetchPredictions\":%llu,\"prefetchHits\":%llu,\"prefetchWasted\":%llu,\"prefetchRecallPct\":%.1f,"
        "\"unresolvableExperts\":%llu,"
        "\"residency\":\"%s\"}",
        name ? name : "", mode, dev, s.layers, s.experts,
        (unsigned long long)s.bytes_read, (unsigned long long)s.expert_bytes, (unsigned long long)s.resident_bytes,
        (unsigned long long)s.cache_hits, (unsigned long long)s.cache_misses, (unsigned long long)s.cache_evictions, hit,
        (unsigned long long)s.cache_resident_hits, resrate,
        (unsigned long long)s.pp, (unsigned long long)s.ph, (unsigned long long)s.pw, rec,
        (unsigned long long)s.unresolvable_experts,
        (s.mode == 1) ? "active" : "warm");
    return 0;
}

int gmb_moestream_register(const char* alias, const char* path) {
    return gmb_moestream_stmt_load(alias, path, 0);
}
