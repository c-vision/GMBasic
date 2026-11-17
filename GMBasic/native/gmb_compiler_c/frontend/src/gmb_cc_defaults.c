/* gmb_cc_defaults.c — vedi gmb_cc_defaults.h: la ricetta UNICA degli `-I` e delle statiche
 * con cui si compila e si linka il C emesso. Nata perché il driver `gmbnative`, invocato da
 * `<root>/bin`, non trovava né gli header (`'gmb_runtime.h' file not found`) né il runtime
 * (Undefined symbols): la ricetta esisteva solo dentro il CLI `gmb build`.
 *
 * REGOLE RISPETTATE:
 *  • nessun path hardcoded: la root dell'installazione viene da `gmb_resolve_install_root()`
 *    (che sa già riconoscere `<monorepo>/bin`) e quella di LLMEnv da `gmb_resolve_llm_root()`;
 *  • solo cartelle/file ESISTENTI entrano nella ricetta (una `-I`/`-l` che punta al nulla fa
 *    accusare il programma invece dell'ambiente);
 *  • niente env nuove: gli override esistenti (`GMBASIC_ROOT`, `LLM_ROOT`) sono quelli del
 *    resolver, non parametri di questa ricetta.
 */
#include "gmb_cc_defaults.h"
#include "gmb_inc_resolve.h"   /* gmb_resolve_install_root / gmb_resolve_llm_root */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* RID di build: lo passa CMake (frontend/CMakeLists.txt da native/cmake/GmbRid.cmake). Il
 * fallback copre un build che non lo passasse: la cache native non si trova e le voci
 * OPZIONALI si saltano — la ricetta resta valida. */
#ifndef GMB_RID_STR
#define GMB_RID_STR "osx-arm64"
#endif

#define CC_MAX_INC 32

static int cc_is_dir(const char* p)
{
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int cc_is_file(const char* p)
{
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* Cache delle librerie, **settings-aware**: `paths.libsCacheDir` di system.settings.json
 * (default `~/.gmbasic/libs`). Prima era `$HOME/.gmbasic/libs` hardcoded: con la chiave
 * impostata il primo uso scaricava nella cache dei settings, ma il link cercava in `$HOME` →
 * su una macchina pulita «library 'crypto' not found» (2026-11-16).
 * I settings si cercano accanto all'eseguibile (`<exe>/../shared/settings/…` in una
 * distribuzione, `<exe>/../../shared/settings/…` in un checkout), poi nella CWD. Nessuna
 * dipendenza dal runtime: qui si legge solo la stringa `"libsCacheDir": "…"`. */
static int cc_settings_libscache(char* out, size_t cap)
{
    char exe[4096] = "";
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0;
    }
#elif defined(__linux__)
    {
        ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0) exe[n] = 0;
    }
#endif
    char* sl = exe[0] ? strrchr(exe, '/') : NULL;
    if (sl) *sl = 0;
    const char* cand[3] = { NULL, NULL, NULL };
    char p0[4096], p1[4096];
    if (exe[0]) {
        snprintf(p0, sizeof p0, "%s/../shared/settings/system.settings.json", exe);
        snprintf(p1, sizeof p1, "%s/../../shared/settings/system.settings.json", exe);
        cand[0] = p0; cand[1] = p1;
    }
    cand[2] = "system.settings.json";
    for (int i = 0; i < 3; i++) {
        if (!cand[i] || !cc_is_file(cand[i])) continue;
        FILE* f = fopen(cand[i], "rb");
        if (!f) continue;
        char txt[262144];
        size_t n = fread(txt, 1, sizeof txt - 1, f);
        fclose(f);
        txt[n] = 0;
        const char* k = strstr(txt, "\"libsCacheDir\"");
        if (!k) continue;
        const char* q = strchr(k, ':');
        if (!q) continue;
        q = strchr(q, '"');
        if (!q) continue;                       /* null = default */
        q++;
        const char* e = strchr(q, '"');
        if (!e || e <= q) continue;
        char v[2048];
        snprintf(v, sizeof v, "%.*s", (int)(e - q), q);
        if (v[0] == '~') snprintf(out, cap, "%s%s", getenv("HOME") ? getenv("HOME") : "", v + 1);
        else snprintf(out, cap, "%s", v);
        return out[0] != 0;
    }
    return 0;
}

int gmb_cc_cache_libdir(char* out, size_t cap)
{
    char base[2048];
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (cc_settings_libscache(base, sizeof base)) {
        /* la chiave contiene la RADICE della cache: il RID lo aggiunge il chiamante */
        snprintf(out, cap, "%s/%s", base, GMB_RID_STR);
        if (cc_is_dir(out)) return 1;
        out[0] = 0;
    }
    {
        const char* home = getenv("HOME");
        if (!home || !*home) return 0;
        snprintf(out, cap, "%s/.gmbasic/libs/%s", home, GMB_RID_STR);
    }
    return cc_is_dir(out);
}

int gmb_cc_inc_dirs(const char* install, char (*out)[GMB_CC_PATH], int max)
{
    int n = 0;
    static const char* rel[] = {
        "native/gmb_runtime_c/include", "native/gmb_runtime_c/src",
        "native/gmb_chart/include", "native/gmb_compiler_c/include",
        "native/gmb_db_c/include", "native/gmb_torch/include",
        "native/gmb_doc_c/include", "native/gmb_agent_runtime",
        "gmbllm/include", "gmbllm/include/quant",
        "gmbllm/include/train", "gmbllm/include/lora",
    };
    static const char* llmrel[] = {
        "LLMCodec/include", "LLMJinja", "LLMEngine/moe/include", "LLMEngine/asr/include",
        "LLMEngine/mlx", "LLMEngine/include", "LLMEngine/Options", "LLMEngine/plugin",
        "LLMDesc", "LLMToken/include",
    };
    if (!install || !*install || !out || max <= 0) return 0;
    for (size_t i = 0; i < sizeof rel / sizeof rel[0] && n < max; i++) {
        char p[GMB_CC_PATH];
        snprintf(p, sizeof p, "%s/%s", install, rel[i]);
        if (cc_is_dir(p)) snprintf(out[n++], GMB_CC_PATH, "%s", p);
    }
    /* LLMEnv è un REPO SEPARATO: due `-I` hardcoded (`../LLMEnv`, `../../LLMEnv`) non
     * bastavano su un client che la installa altrove. `LLM_ROOT` (env) vince; altrimenti
     * si scopre. */
    {
        char llm[GMB_CC_PATH];
        if (gmb_resolve_llm_root(llm, sizeof llm) && llm[0]) {
            for (size_t i = 0; i < sizeof llmrel / sizeof llmrel[0] && n < max; i++) {
                char p[GMB_CC_PATH];
                snprintf(p, sizeof p, "%s/%s", llm, llmrel[i]);
                if (cc_is_dir(p)) snprintf(out[n++], GMB_CC_PATH, "%s", p);
            }
        }
    }
    {
        /* header delle terze parti: gruppo `dev` della cache (`gmb install --dev`). */
        char cache[GMB_CC_PATH];
        if (gmb_cc_cache_libdir(cache, sizeof cache)) {
            char inc2[GMB_CC_PATH];
            snprintf(inc2, sizeof inc2, "%s/include", cache);
            if (cc_is_dir(inc2) && n < max) snprintf(out[n++], GMB_CC_PATH, "%s", inc2);
        }
    }
    return n;
}

/* Sottostringa case-insensitive: `strcasestr` non è portabile (manca su Windows). */
static int cc_ci_contains(const char* hay, const char* needle)
{
    size_t n = needle ? strlen(needle) : 0;
    if (!hay || n == 0) return 0;
    for (const char* p = hay; *p; p++)
        if (strncasecmp(p, needle, n) == 0) return 1;
    return 0;
}

/* Parola a inizio token: la keyword non deve essere il prefisso di un identificatore
 * (`FORM` sì, `FORMATX` no). */
static int cc_word_at(const char* p, const char* w)
{
    size_t n = strlen(w);
    char c;
    if (strncasecmp(p, w, n) != 0) return 0;
    c = p[n];
    return (c == ' ' || c == '\t' || c == '(' || c == '\r' || c == '\n' || c == '\0' || c == ',');
}

static char* cc_read_all(const char* path)
{
    FILE* f = fopen(path, "rb");
    long n;
    char* buf;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    buf[n] = 0;
    return buf;
}

int gmb_cc_source_needs_full_runtime(const char* entry)
{
    static const char* kw[] = {
        "FORM","READ","TORCH","TRAIN","JINJA","SCREEN","GRAPHICS","SPRITE","SPRITESET","PSET",
        "PRESET","CIRCLE","PAINT","DRAW","POLYGON","PCOPY","SCREENSET","PALETTE","MOUSE","WINDOW",
        "IMAGE","BITMAP","TILE","FONT","PLOT","CHART","VIDEO","DBF","ISAM","SPEED","RDD","SQL",
        "NET","HTTP","AI","LLM","ASR","MEMORY","VECTORDB","DOC","FETCH","TUI"
    };
    char* src;
    int need = 0;
    const char* p;
    if (!entry || !*entry) return 1;
    src = cc_read_all(entry);
    /* Illeggibile: prudenza (meglio il runtime completo che un link senza simboli). */
    if (!src) return 1;
    p = src;
    while (*p && !need) {
        while (*p == ' ' || *p == '\t') p++;
        if (strncasecmp(p, "IMPORT", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
            const char* q = p + 6;
            while (*q == ' ' || *q == '\t') q++;
            for (size_t k = 0; k < sizeof kw / sizeof kw[0] && !need; k++)
                if (cc_word_at(q, kw[k])) need = 1;
        }
        for (size_t k = 0; k < sizeof kw / sizeof kw[0] && !need; k++)
            if (cc_word_at(p, kw[k])) need = 1;
        /* `PRINT AI.PLUGINSLIST()`: la parola non è a inizio riga. Il punto dopo il namespace
         * è il segnale: senza, un programma che usa funzioni AI linkava il runtime MINIMO e
         * falliva con simboli agent/sqlite non definiti. */
        for (size_t k = 0; k < sizeof kw / sizeof kw[0] && !need; k++) {
            char nsdot[48];
            snprintf(nsdot, sizeof nsdot, "%s.", kw[k]);
            if (cc_ci_contains(src, nsdot)) need = 1;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    free(src);
    return need;
}

int gmb_cc_runtime_lib(const char* install, int need_full, char* out, size_t cap)
{
    static const char* mincand[] = {
        "native/gmb_runtime_c/build/libgmb_runtime_min.a",
        "native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime_min.a",
    };
    /* Il terzo candidato non esiste più nel build attuale: costa un `is_file` e copre i
     * checkout più datati. */
    static const char* rtcand[] = {
        "native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime.a",
        "native/gmb_runtime_c/build/gmb_runtime_c_build/libgmb_runtime.a",
        "native/gmb_runtime_c/build/libgmb_runtime.a",
    };
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (!install || !*install) return 0;
    if (!need_full) {
        for (size_t i = 0; i < sizeof mincand / sizeof mincand[0]; i++) {
            char p[GMB_CC_PATH];
            struct stat st;
            snprintf(p, sizeof p, "%s/%s", install, mincand[i]);
            /* un archivio VUOTO/parziale (pochi byte: build interrotta) non è una libreria:
             * sceglierlo produce un link senza simboli (`_gmb_print_newline` non definito).
             * Si accetta solo un archivio con dimensione plausibile. */
            if (!cc_is_file(p) || stat(p, &st) != 0 || st.st_size < 65536) continue;
            snprintf(out, cap, "%s", p);
            return 1;
        }
    }
    for (size_t i = 0; i < sizeof rtcand / sizeof rtcand[0]; i++) {
        char p[GMB_CC_PATH];
        snprintf(p, sizeof p, "%s/%s", install, rtcand[i]);
        if (cc_is_file(p)) { snprintf(out, cap, "%s", p); return 1; }
    }
    return 0;
}

int gmb_cc_chart_lib(const char* install, char* out, size_t cap)
{
    static const char* cand[] = {
        "native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a",
        "native/gmb_vm_c/build/gmb_chart_build/libgmb_chart.a",
        "native/gmb_runtime_c/build/libgmb_chart.a",
    };
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (!install || !*install) return 0;
    for (size_t i = 0; i < sizeof cand / sizeof cand[0]; i++) {
        char p[GMB_CC_PATH];
        snprintf(p, sizeof p, "%s/%s", install, cand[i]);
        if (cc_is_file(p)) { snprintf(out, cap, "%s", p); return 1; }
    }
    return 0;
}

int gmb_cc_llmdesc_lib(const char* install, char* out, size_t cap)
{
    static const char* cand[] = {
        "native/gmb_runtime_c/build/llmdesc_build/libllmdesc.a",
        "native/gmb_vm_c/build/gmb_runtime_c_build/llmdesc_build/libllmdesc.a",
    };
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (!install || !*install) return 0;
    for (size_t i = 0; i < sizeof cand / sizeof cand[0]; i++) {
        char p[GMB_CC_PATH];
        snprintf(p, sizeof p, "%s/%s", install, cand[i]);
        if (cc_is_file(p)) { snprintf(out, cap, "%s", p); return 1; }
    }
    return 0;
}
