/* Registro di opzioni GENERICO condiviso (§0bis — gmb_runtime_c).
 *
 * Superficie BASIC unica: `AI SET OPTION <nome>, <valore>` / `v = AI.Option("<nome>")`.
 *
 * Le voci del CORE vivono qui (una RIGA in gmb_opt_core[]): così TUTTI i backend C
 * (VM/C, C99, C99Native) le vedono senza dipendere da host esterni. Un dominio che
 * possiede il comportamento di una voce AGGANCIA il setter con `gmb_opt_attach`
 * (es. l'host MLX aggancia `moe_swap_dir` -> `gmb_mlx_set_moe_swap_dir`).
 */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char   name[64];
    char   desc[192];
    char   val[1024];
    int    set;
    void (*on_set)(const char* value);
} GmbOpt;

#define GMB_OPT_MAX 64
static GmbOpt g_opts[GMB_OPT_MAX];
static int     g_nopts = 0;
static int     g_init  = 0;

/* Voci disponibili in ogni backend C (host-agnostiche). */
static const struct { const char* name; const char* desc; } g_opt_core[] = {
    { "moe_swap_dir", "dir modello per lo streaming esperti MoE (vuoto = off)" },
};
#define GMB_OPT_CORE_N ((int)(sizeof g_opt_core / sizeof g_opt_core[0]))

static GmbOpt* gmb_opt_find(const char* n) {
    if (!n) return NULL;
    for (int i = 0; i < g_nopts; i++) if (!strcmp(n, g_opts[i].name)) return &g_opts[i];
    return NULL;
}

static void ensure_init(void) {
    if (g_init) return;
    g_init = 1;
    for (int i = 0; i < GMB_OPT_CORE_N; i++) {
        if (g_nopts >= GMB_OPT_MAX) break;
        GmbOpt* o = &g_opts[g_nopts++];
        snprintf(o->name, sizeof o->name, "%s", g_opt_core[i].name);
        snprintf(o->desc, sizeof o->desc, "%s", g_opt_core[i].desc);
        o->val[0] = '\0'; o->set = 0; o->on_set = NULL;
    }
    gmb_opts_load_hierarchy();   /* cache dai settings: globali (exe) → app (override) */
}

int gmb_opt_register(const char* name, const char* desc, const char* def, void (*on_set)(const char*)) {
    ensure_init();
    if (!name || gmb_opt_find(name) || g_nopts >= GMB_OPT_MAX) return -1;
    GmbOpt* o = &g_opts[g_nopts++];
    snprintf(o->name, sizeof o->name, "%s", name);
    snprintf(o->desc, sizeof o->desc, "%s", desc ? desc : "");
    snprintf(o->val,  sizeof o->val,  "%s", def  ? def  : "");
    o->on_set = on_set;
    o->set    = (def && def[0]) ? 1 : 0;
    return 0;
}

int gmb_opt_attach(const char* name, void (*on_set)(const char*)) {
    ensure_init();
    GmbOpt* o = gmb_opt_find(name);
    if (!o) return -1;
    o->on_set = on_set;
    return 0;
}

int gmb_opt_set(const char* name, const char* value) {
    ensure_init();
    if (!name || !value) return -1;
    GmbOpt* o = gmb_opt_find(name);
    if (!o) {                                   /* registro avanzato: qualunque nome */
        if (g_nopts >= GMB_OPT_MAX) return -1;
        o = &g_opts[g_nopts++];
        snprintf(o->name, sizeof o->name, "%s", name);
        o->desc[0] = '\0'; o->val[0] = '\0'; o->set = 0; o->on_set = NULL;
    }
    snprintf(o->val, sizeof o->val, "%s", value);
    o->set = 1;
    if (o->on_set) o->on_set(o->val);
    return 0;
}

int gmb_opt_get(const char* name, char* out, int cap) {
    ensure_init();
    GmbOpt* o = gmb_opt_find(name);
    if (!o || !out) return -1;
    snprintf(out, (size_t)cap, "%s", o->val);
    return 0;
}

int gmb_opt_count(void) { ensure_init(); return g_nopts; }

int gmb_opt_at(int i, const char** name, const char** desc, char* val, int cap) {
    ensure_init();
    if (i < 0 || i >= g_nopts) return -1;
    if (name) *name = g_opts[i].name;
    if (desc) *desc = g_opts[i].desc;
    if (val)  snprintf(val, (size_t)cap, "%s", g_opts[i].val);
    return 0;
}

/* Variante comoda per l'emissione C: valore come stringa heap ("" se ignoto). */
char* gmb_opt_get_str(const char* name) {
    char buf[1024];
    if (gmb_opt_get(name, buf, sizeof buf) != 0) buf[0] = '\0';
    size_t n = strlen(buf);
    char* p = (char*)malloc(n + 1);
    if (p) memcpy(p, buf, n + 1);
    return p;
}

/* PONTE DI TRANSIZIONE (parità LLMEnv `llm_ai_mlx_opt_env`): i siti di lettura dei
 * parametri di funzionamento passano da qui. Ordine: registro (se impostato) →
 * `getenv` (fallback). Quando tutte le voci sono impostate via `AI SET OPTION`,
 * l'env non e' piu' il meccanismo. I debug/dump restano `getenv` diretto. */
const char* gmb_opt_env(const char* name) {
    ensure_init();
    if (!name) return NULL;
    GmbOpt* o = gmb_opt_find(name);
    if (o && o->set) return o->val;
    return getenv(name);
}

/* ── SETTINGS condivisi (§ consolidamento A, 2026-09-29) ─────────────────────────────────────
 * Le opzioni motore vivono nella sezione `"engine": { … }` degli STESSI file di settings
 * dell'app, con la stessa gerarchia di `SettingsManager.cs`:
 *   system.settings.json (dir ESeguibile) = default globali
 *   app.settings.json    (dir applicazione) = override, HA PRECEDENZA
 * Letti indipendentemente dai mondi C e C# (nessun push). `gmb_opt_set` è in-memory;
 * `gmb_opt_set_persist` scrive l'override nel livello APP. */

#include <ctype.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static void exe_dir(char* out, size_t cap) {
    out[0] = '.'; out[1] = '\0';
#ifdef __APPLE__
    uint32_t sz = (uint32_t)cap;
    if (_NSGetExecutablePath(out, &sz) != 0) return;
    char* s = strrchr(out, '/');
    if (s) *s = '\0';                       /* dalla dir del binario */
    /* allinea a FileSystemUtils.CoreAppPath(): se il binario sta sotto .../bin/...,
     * risale di 3 livelli (es. Executor/bin/Debug/net10.0 → Executor). */
    if (strstr(out, "/bin/")) {
        for (int i = 0; i < 3; i++) {
            char* p = strrchr(out, '/');
            if (!p) break;
            *p = '\0';
        }
    }
#endif
}

/* Carica la sezione "engine" (flat) da un JSON; sovrascrive la cache. */
static int load_engine_section(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    static char buf[131072];
    size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = '\0';
    const char* p = strstr(buf, "\"engine\"");
    if (!p) return 0;
    p = strchr(p, '{');
    if (!p) return 0;
    p++;
    const char* end = strchr(p, '}');
    while (p && *p && (!end || p < end)) {
        const char* ks = strchr(p, '"');           if (!ks || (end && ks > end)) break;
        const char* ke = strchr(ks + 1, '"');      if (!ke) break;
        const char* c  = strchr(ke, ':');          if (!c)  break;
        char k[64], v[1024];
        size_t kl = (size_t)(ke - ks - 1);
        if (kl >= sizeof k) kl = sizeof k - 1;
        memcpy(k, ks + 1, kl); k[kl] = '\0';
        const char* vs = strchr(c, '"');
        if (vs && (!end || vs < end)) {
            const char* ve = strchr(vs + 1, '"');  if (!ve) break;
            size_t vl = (size_t)(ve - vs - 1);
            if (vl >= sizeof v) vl = sizeof v - 1;
            memcpy(v, vs + 1, vl); v[vl] = '\0';
            p = ve + 1;
        } else {
            const char* ve = c + 1;
            while (*ve && *ve != '}' && *ve != ',' && !isspace((unsigned char)*ve)) ve++;
            size_t vl = (size_t)(ve - (c + 1));
            if (vl >= sizeof v) vl = sizeof v - 1;
            memcpy(v, c + 1, vl); v[vl] = '\0';
            p = ve;
        }
        gmb_opt_set(k, v);
    }
    return 0;
}

/* Compat: carica la sezione engine da un singolo file. */
int gmb_opts_load(const char* path) { ensure_init(); return load_engine_section(path); }

/* Gerarchia a DUE livelli di dir (nessun ~/.gmbasic):
 *   1. dir COMPILATORE: system.settings.json (default globali)
 *   2. dir APP:         system.settings.json (copia locale, se presente, sostituisce 1)
 *   3. dir APP:         app.settings.json    (override applicazione, in cima) */
void gmb_opts_load_hierarchy(void) {
    ensure_init();
    char exe[1024], p[1200];
    exe_dir(exe, sizeof exe);
    snprintf(p, sizeof p, "%s/system.settings.json", exe);
    load_engine_section(p);                          /* 1. globali (dir compilatore) */
    load_engine_section("system.settings.json");     /* 2. copia locale (dir app) */
    {
        /* 3. override app: nella CWD, altrimenti ACCANTO ALL'ESEGUIBILE (compilato distribuito) */
        char ap[2600];
        load_engine_section(gmb_paths_app_settings(ap, sizeof ap) ? ap : "app.settings.json");
    }
}

/* Salva la sezione engine in app.settings.json, preservando il resto (splice). */
int gmb_opts_save(const char* path) {
    ensure_init();
    const char* file = path ? path : "app.settings.json";
    static char buf[131072]; buf[0] = '\0';
    FILE* f = fopen(file, "r");
    if (f) { size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = '\0'; }

    char eng[16384]; int e = 0;
    e += snprintf(eng + e, sizeof eng - e, "{");
    for (int i = 0; i < g_nopts; i++)
        e += snprintf(eng + e, sizeof eng - e, "%s\"%s\": \"%s\"",
                      (i == 0 ? "" : ", "), g_opts[i].name, g_opts[i].val);
    e += snprintf(eng + e, sizeof eng - e, "}");

    static char out[147456]; size_t o = 0;
    char* pe = strstr(buf, "\"engine\"");
    if (pe) {
        char* ob = strchr(pe, '{');
        char* oe = ob ? strchr(ob, '}') : NULL;
        if (!ob || !oe) { snprintf(out, sizeof out, "%s", buf); o = strlen(out); }
        else {
            size_t pre = (size_t)(ob - buf);
            memcpy(out, buf, pre); o = pre;
            memcpy(out + o, eng, strlen(eng)); o += strlen(eng);
            snprintf(out + o, sizeof out - o, "%s", oe + 1); o += strlen(oe + 1);
        }
    } else if (buf[0]) {
        char* cl = strrchr(buf, '}');
        if (!cl) { snprintf(out, sizeof out, "%s", buf); o = strlen(out); }
        else {
            size_t pre = (size_t)(cl - buf);
            memcpy(out, buf, pre); o = pre;
            int has = (pre > 0 && buf[pre - 1] != '{' && buf[pre - 1] != '{') ? 1 : 0;
            o += (size_t)snprintf(out + o, sizeof out - o, "%s\"engine\": %s\n", has ? ", " : "", eng);
            snprintf(out + o, sizeof out - o, "%s", cl); o = strlen(out);
        }
    } else {
        snprintf(out, sizeof out, "{\n  \"engine\": %s\n}\n", eng);
    }
    f = fopen(file, "w");
    if (!f) return -1;
    fwrite(out, 1, strlen(out), f);
    fclose(f);
    return 0;
}

int gmb_opt_set_persist(const char* name, const char* value) {
    int rc = gmb_opt_set(name, value);
    if (rc == 0) gmb_opts_save(NULL);
    return rc;
}
