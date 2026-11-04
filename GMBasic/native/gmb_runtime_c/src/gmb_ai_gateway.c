/* gmb_ai_gateway.c — entry point AI del runtime C, inoltrate alla dylib `gmb_runtime_ai`.
 *
 * PERCHÉ: il runtime è la sola implementazione del linguaggio e include le famiglie AI/LLM;
 * ma solo 3 file (`gmb_asr_cmd.c`, `gmb_llm_tok.c`, `gmb_model_io.c`) dipendono realmente da
 * LLMEnv (ASR, `llmmodelio_*`, `llmdesc`/`llmcodec`). Tenerli nel core significava che OGNI
 * consumer (REPL compresa) dovesse compilare/linkare LLMEnv.
 *
 * ORA: le implementazioni vivono nella dylib `libgmb_runtime_ai` (linkata a LLMEnv); qui ci
 * sono i **forwarder deboli** (`__attribute__((weak))`): se la dylib c'è, l'AI funziona; se
 * manca, l'operazione fallisce con un errore ESPLICITO (mai un no-op silenzioso).
 *
 * La dylib non linka il runtime (eviterebbe una seconda copia degli stati globali del core):
 * risolve i simboli del core a load-time (`-undefined dynamic_lookup` su macOS, `-rdynamic`
 * sui consumer ELF).
 */
#include "gmb_runtime.h"
#include "gmb_llm_tok.h"
#include "gmb_model_io.h"
#include "gmb_distill.h"
#include "gmb_ai_decide.h"
#include "gmb_decision_head.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define GMB_AI_WEAK __attribute__((weak))

static void* g_ai_lib;
static int   g_ai_tried;
static char  g_ai_err[512];

static char g_ai_last_dlerr[512];

static int ai_try(const char* path)
{
    if (!path || !*path) return 0;
    g_ai_lib = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (g_ai_lib) return 1;
    const char* e = dlerror();
    if (e && *e) { snprintf(g_ai_last_dlerr, sizeof g_ai_last_dlerr, "%s", e); }
    return 0;
}

/* Apre (una volta) la dylib AI. Cerca: path di build compilato dentro, accanto all'eseguibile,
 * nella cache librerie del RID, poi il nome nudo (DYLD_LIBRARY_PATH). */
static void* ai_lib(void)
{
    if (g_ai_tried) return g_ai_lib;
    g_ai_tried = 1;

    char cand[16][4096];
    int n = 0;
    char exe[4096] = "";
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0;
    }
#else
    {
        ssize_t r = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (r > 0) { exe[r] = 0; }
    }
#endif
    /* NOME della dylib per piattaforma: il bundle la mette in <exe>/libs/<rid>/. */
#if defined(_WIN32)
    const char* aiFile = "gmb_runtime_ai.dll";
#elif defined(__APPLE__)
    const char* aiFile = "libgmb_runtime_ai.dylib";
#else
    const char* aiFile = "libgmb_runtime_ai.so";
#endif
    if (exe[0]) {
        char dir[4096];
        snprintf(dir, sizeof dir, "%s", exe);
        char* sl = strrchr(dir, '/');
#if defined(_WIN32)
        if (!sl) sl = strrchr(dir, '\\');
#endif
        if (sl) {
            *sl = 0;
            /* LOCALE PRIMA: bundle `<exe>/libs/<rid>/`, poi `<exe>/libs`, poi accanto. */
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/libs/%s/%s", dir, gmb_libs_rid(), aiFile); n++; }
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/libs/%s", dir, aiFile); n++; }
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/%s", dir, aiFile); n++; }
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/../libs/%s/%s", dir, gmb_libs_rid(), aiFile); n++; }
        }
    }
    /* path di build compilato dentro (checkout/REPL in sviluppo). DOPO il locale: un bundle
     * accanto all'eseguibile deve vincere sul build tree. */
#ifdef GMB_RUNTIME_AI_PATH
    if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s", GMB_RUNTIME_AI_PATH); n++; }
#endif
    /* checkout: la dylib è prodotta in GMBasic/build/libs/<rid>/ (e, per il build standalone
     * del runtime, nella radice del suo build dir). */
    {
        char root[2048];
        if (gmb_paths_repo_root(root, sizeof root) && root[0]) {
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/build/libs/%s/%s", root, gmb_libs_rid(), aiFile); n++; }
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/native/gmb_runtime_c/build/%s", root, aiFile); n++; }
            if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s/../GMBasic/build/libs/%s/%s", root, gmb_libs_rid(), aiFile); n++; }
        }
    }
    {
        char cache[2048];
        gmb_paths_libs_cache_dir(cache, sizeof cache);
        if (cache[0] && n < 16) {
            snprintf(cand[n], sizeof cand[0], "%s/%s/%s", cache, gmb_libs_rid(), aiFile);
            n++;
        }
    }
    if (n < 16) { snprintf(cand[n], sizeof cand[0], "%s", aiFile); n++; }

    for (int i = 0; i < n; i++) if (ai_try(cand[i])) return g_ai_lib;

    snprintf(g_ai_err, sizeof g_ai_err,
             "runtime AI non disponibile: libgmb_runtime_ai non caricabile "
             "(cercata accanto all'eseguibile, nel build del checkout e in ~/.gmbasic/libs/%s)%s%s",
             gmb_libs_rid(),
             g_ai_last_dlerr[0] ? " — " : "", g_ai_last_dlerr);
    fprintf(stderr, "?%s\n", g_ai_err);
    return NULL;
}

static void* ai_sym(const char* name)
{
    void* h = ai_lib();
    if (!h) return NULL;
    void* f = dlsym(h, name);
    if (!f) {
        snprintf(g_ai_err, sizeof g_ai_err, "runtime AI: simbolo '%s' assente nella dylib", name);
        fprintf(stderr, "?%s\n", g_ai_err);
    }
    return f;
}

/* Errore esplicito: usato dai chiamanti per distinguere "AI assente" da un risultato vuoto. */
const char* gmb_ai_last_error(void) { return g_ai_err[0] ? g_ai_err : NULL; }

/* ── ASR ─────────────────────────────────────────────────────────────────── */

GMB_AI_WEAK char* gmb_asr_cmd(const char* model_dir, const char* wav_path, const char* out_path)
{
    char* (*fn)(const char*, const char*, const char*) = (char* (*)(const char*, const char*, const char*))ai_sym("gmb_asr_cmd");
    return fn ? fn(model_dir, wav_path, out_path) : NULL;
}

GMB_AI_WEAK void gmb_asr_free(char* p)
{
    void (*fn)(char*) = (void (*)(char*))ai_sym("gmb_asr_free");
    /* senza dylib non c'è nulla da liberare: il buffer non può essere stato allocato da noi */
    if (fn) fn(p);
}

/* ── LLM (tokenizer / verbi con output catturato) ────────────────────────── */

GMB_AI_WEAK char* gmb_llm_tokens(const char* model, const char* text)
{
    char* (*fn)(const char*, const char*) = (char* (*)(const char*, const char*))ai_sym("gmb_llm_tokens");
    return fn ? fn(model, text) : NULL;
}

GMB_AI_WEAK char* gmb_llm_detokenize(const char* model, const char* ids_csv)
{
    char* (*fn)(const char*, const char*) = (char* (*)(const char*, const char*))ai_sym("gmb_llm_detokenize");
    return fn ? fn(model, ids_csv) : NULL;
}

GMB_AI_WEAK char* gmb_llm_tensor_out(const char* sub, const char* a, const char* b, const char* c)
{
    char* (*fn)(const char*, const char*, const char*, const char*) =
        (char* (*)(const char*, const char*, const char*, const char*))ai_sym("gmb_llm_tensor_out");
    return fn ? fn(sub, a, b, c) : NULL;
}

GMB_AI_WEAK char* gmb_llm_cmd_out(int family, const char* sub, const char* a, const char* b, const char* c)
{
    char* (*fn)(int, const char*, const char*, const char*, const char*) =
        (char* (*)(int, const char*, const char*, const char*, const char*))ai_sym("gmb_llm_cmd_out");
    return fn ? fn(family, sub, a, b, c) : NULL;
}

/* Helper NON delegato: alza l'errore BASIC (sta nel core) e ritorna `r`.
 * Tenerlo qui evita un round-trip dlsym su una funzione di 3 righe. */
GMB_AI_WEAK char* gmb_llm_out_or_raise(char* r, const char* what, long line)
{
    if (!r) gmb_raise_error(9, what && *what ? what : "LLM error", line);
    return r;
}

static void set_err_local(char* err, size_t cap)
{
    if (!err || cap == 0) return;
    if (g_ai_err[0]) snprintf(err, cap, "%s", g_ai_err);
    else snprintf(err, cap, "runtime AI non disponibile");
}

/* ── DECIDE locale (JEV/SystemOne): implementazione nella dylib AI ────────── */

GMB_AI_WEAK int gmb_ai_decide_local_run(const char* state, const char* kind,
                                        const gmb_decision_option_t* options, int n_options,
                                        gmb_decision_result_t* out, char* err, size_t errcap)
{
    /* KEEP-ALIVE per gli host linkati con `-Wl,-dead_strip` (gmb_vm_host): la dylib AI
     * risolve a load-time i simboli del core `gmb_ai_decide_active_model_path` e
     * `gmb_json_extract`, ma il linker li vedrebbe "non usati" dall'eseguibile e li
     * rimuoverebbe → salto a simbolo nullo. I riferimenti volatile qui sotto li ancorano. */
    static void* volatile g_decide_core_anchor;
    g_decide_core_anchor = (void*)&gmb_ai_decide_active_model_path;
    g_decide_core_anchor = (void*)&gmb_ai_decide_profile_active;
    g_decide_core_anchor = (void*)&gmb_json_extract;
    /* Import dell'intera dylib AI (non solo di DECIDE): sono simboli del core usati dai suoi
     * moduli (ABI/llmdesc, rid, paths) e senza ancora dead_strip li rimuove dall'eseguibile
     * → dlopen della dylib fallisce/aborta al primo uso. */
    g_decide_core_anchor = (void*)&gmb_abi_set_llmdesc;
    g_decide_core_anchor = (void*)&gmb_rid;
    g_decide_core_anchor = (void*)&gmb_paths_libs_cache_dir;
    g_decide_core_anchor = (void*)&gmb_paths_resolve_model;
    g_decide_core_anchor = (void*)&gmb_raise_error;
    /* Testa di scoring condivisa (T3): importata dalla dylib AI per leggere l'artefatto. */
    g_decide_core_anchor = (void*)&gmb_decision_head_init;
    g_decide_core_anchor = (void*)&gmb_decision_head_load;
    g_decide_core_anchor = (void*)&gmb_decision_head_slot;
    g_decide_core_anchor = (void*)&gmb_decision_head_apply;
    int (*fn)(const char*, const char*, const gmb_decision_option_t*, int,
              gmb_decision_result_t*, char*, size_t) =
        (int (*)(const char*, const char*, const gmb_decision_option_t*, int,
                 gmb_decision_result_t*, char*, size_t))ai_sym("gmb_ai_decide_local_run");
    if (!fn) {
        set_err_local(err, errcap);
        return -1;
    }
    return fn(state, kind, options, n_options, out, err, errcap);
}

/* P6 — adapter generativo (generazione sul modello locale). */
GMB_AI_WEAK int gmb_ai_decide_local_adapter_run(const char* state, const char* kind,
                                                const gmb_decision_option_t* options, int n_options,
                                                gmb_decision_result_t* out, char* err, size_t errcap)
{
    int (*fn)(const char*, const char*, const gmb_decision_option_t*, int,
              gmb_decision_result_t*, char*, size_t) =
        (int (*)(const char*, const char*, const gmb_decision_option_t*, int,
                 gmb_decision_result_t*, char*, size_t))ai_sym("gmb_ai_decide_local_adapter_run");
    if (!fn) {
        set_err_local(err, errcap);
        return -1;
    }
    return fn(state, kind, options, n_options, out, err, errcap);
}

/* Batch locale: N domini sullo stesso stato con prefill/KV condiviso (fusione C). */
GMB_AI_WEAK int gmb_ai_decide_local_run_group(const char* state,
                                              const gmb_decision_domain_t* domains,
                                              int n_domains, gmb_decision_result_t* out,
                                              char* err, size_t errcap)
{
    int (*fn)(const char*, const gmb_decision_domain_t*, int, gmb_decision_result_t*,
              char*, size_t) =
        (int (*)(const char*, const gmb_decision_domain_t*, int, gmb_decision_result_t*,
                 char*, size_t))ai_sym("gmb_ai_decide_local_run_group");
    if (!fn) {
        set_err_local(err, errcap);
        return -1;
    }
    return fn(state, domains, n_domains, out, err, errcap);
}

/* ── Model I/O (LLM MODEL CONVERT/QUANTIZE/INSPECT) ──────────────────────── */

GMB_AI_WEAK int gmb_model_io_convert(const char* model_dir, const char* out_path)
{
    int (*fn)(const char*, const char*) = (int (*)(const char*, const char*))ai_sym("gmb_model_io_convert");
    return fn ? fn(model_dir, out_path) : -1;
}

GMB_AI_WEAK int gmb_model_io_quantize(const char* src_dir, const char* out_dir, int bits, int group,
                                      const char* imatrix_path, int expert_bits)
{
    int (*fn)(const char*, const char*, int, int, const char*, int) =
        (int (*)(const char*, const char*, int, int, const char*, int))ai_sym("gmb_model_io_quantize");
    return fn ? fn(src_dir, out_dir, bits, group, imatrix_path, expert_bits) : -1;
}

GMB_AI_WEAK int gmb_model_io_inspect(const char* dir, char* out, int cap)
{
    int (*fn)(const char*, char*, int) = (int (*)(const char*, char*, int))ai_sym("gmb_model_io_inspect");
    return fn ? fn(dir, out, cap) : -1;
}

GMB_AI_WEAK int gmb_model_io_inspect_print(const char* dir)
{
    int (*fn)(const char*) = (int (*)(const char*))ai_sym("gmb_model_io_inspect_print");
    return fn ? fn(dir) : -1;
}

/* ── TRAIN DISTILL (PUL): 19 entry point, tutti inoltrati ────────────────── */

GMB_AI_WEAK int gmb_distill_build(const char* jsonl_path, const char* tokenizer_json,
                                  const char* out_prefix, int window, double hold_ratio, int max_pairs)
{
    int (*fn)(const char*, const char*, const char*, int, double, int) =
        (int (*)(const char*, const char*, const char*, int, double, int))ai_sym("gmb_distill_build");
    return fn ? fn(jsonl_path, tokenizer_json, out_prefix, window, hold_ratio, max_pairs) : -1;
}

GMB_AI_WEAK int gmb_distill_report(char* out, int cap)
{
    int (*fn)(char*, int) = (int (*)(char*, int))ai_sym("gmb_distill_report");
    return fn ? fn(out, cap) : -1;
}

GMB_AI_WEAK int gmb_distill_build_opts(const char* jsonl_path, const char* opts_json, char* resp, int cap)
{
    int (*fn)(const char*, const char*, char*, int) =
        (int (*)(const char*, const char*, char*, int))ai_sym("gmb_distill_build_opts");
    return fn ? fn(jsonl_path, opts_json, resp, cap) : -1;
}

GMB_AI_WEAK int gmb_distill_pairs(void)
{ int (*fn)(void) = (int (*)(void))ai_sym("gmb_distill_pairs"); return fn ? fn() : -1; }

GMB_AI_WEAK int gmb_distill_trainsamples(void)
{ int (*fn)(void) = (int (*)(void))ai_sym("gmb_distill_trainsamples"); return fn ? fn() : -1; }

GMB_AI_WEAK int gmb_distill_holdsamples(void)
{ int (*fn)(void) = (int (*)(void))ai_sym("gmb_distill_holdsamples"); return fn ? fn() : -1; }

GMB_AI_WEAK int gmb_distill_sample_opts(const char* prompts, const char* opts_json, char* resp, int cap)
{
    int (*fn)(const char*, const char*, char*, int) =
        (int (*)(const char*, const char*, char*, int))ai_sym("gmb_distill_sample_opts");
    return fn ? fn(prompts, opts_json, resp, cap) : -1;
}

GMB_AI_WEAK int gmb_distill_samplen(void)
{ int (*fn)(void) = (int (*)(void))ai_sym("gmb_distill_samplen"); return fn ? fn() : -1; }

GMB_AI_WEAK int gmb_distill_samplefail(void)
{ int (*fn)(void) = (int (*)(void))ai_sym("gmb_distill_samplefail"); return fn ? fn() : -1; }

GMB_AI_WEAK double gmb_distill_sampledone(void)
{ double (*fn)(void) = (double (*)(void))ai_sym("gmb_distill_sampledone"); return fn ? fn() : -1.0; }

GMB_AI_WEAK int gmb_distill_eval_opts(const char* prompts, const char* opts_json, char* resp, int cap)
{
    int (*fn)(const char*, const char*, char*, int) =
        (int (*)(const char*, const char*, char*, int))ai_sym("gmb_distill_eval_opts");
    return fn ? fn(prompts, opts_json, resp, cap) : -1;
}

GMB_AI_WEAK double gmb_distill_evaltok(int idx)
{ double (*fn)(int) = (double (*)(int))ai_sym("gmb_distill_evaltok"); return fn ? fn(idx) : -1.0; }

GMB_AI_WEAK double gmb_distill_evalpass(int idx)
{ double (*fn)(int) = (double (*)(int))ai_sym("gmb_distill_evalpass"); return fn ? fn(idx) : -1.0; }

GMB_AI_WEAK double gmb_distill_evalacc(int idx)
{ double (*fn)(int) = (double (*)(int))ai_sym("gmb_distill_evalacc"); return fn ? fn(idx) : -1.0; }

GMB_AI_WEAK int gmb_distill_apply_opts(const char* ckpt, const char* opts_json, char* resp, int cap)
{
    int (*fn)(const char*, const char*, char*, int) =
        (int (*)(const char*, const char*, char*, int))ai_sym("gmb_distill_apply_opts");
    return fn ? fn(ckpt, opts_json, resp, cap) : -1;
}

GMB_AI_WEAK int gmb_distill_restore_opts(const char* backup, char* resp, int cap)
{
    int (*fn)(const char*, char*, int) = (int (*)(const char*, char*, int))ai_sym("gmb_distill_restore_opts");
    return fn ? fn(backup, resp, cap) : -1;
}

GMB_AI_WEAK int gmb_distill_train_opts(const char* dataset, const char* opts_json, char* resp, int cap)
{
    int (*fn)(const char*, const char*, char*, int) =
        (int (*)(const char*, const char*, char*, int))ai_sym("gmb_distill_train_opts");
    return fn ? fn(dataset, opts_json, resp, cap) : -1;
}

GMB_AI_WEAK double gmb_distill_loss(void)
{ double (*fn)(void) = (double (*)(void))ai_sym("gmb_distill_loss"); return fn ? fn() : -1.0; }

GMB_AI_WEAK double gmb_distill_hold(void)
{ double (*fn)(void) = (double (*)(void))ai_sym("gmb_distill_hold"); return fn ? fn() : -1.0; }

GMB_AI_WEAK const char* gmb_distill_ckpt(void)
{ const char* (*fn)(void) = (const char* (*)(void))ai_sym("gmb_distill_ckpt"); return fn ? fn() : NULL; }

GMB_AI_WEAK double gmb_distill_done(void)
{ double (*fn)(void) = (double (*)(void))ai_sym("gmb_distill_done"); return fn ? fn() : -1.0; }

GMB_AI_WEAK const char* gmb_distill_last_error(void)
{ const char* (*fn)(void) = (const char* (*)(void))ai_sym("gmb_distill_last_error"); return fn ? fn() : gmb_ai_last_error(); }

GMB_AI_WEAK const char* gmb_distill_status(void)
{
    const char* (*fn)(void) = (const char* (*)(void))ai_sym("gmb_distill_status");
    if (fn) return fn();
    static char msg[512];
    snprintf(msg, sizeof msg, "TRAIN DISTILL non disponibile: %s", g_ai_err[0] ? g_ai_err : "runtime AI assente");
    return msg;
}

GMB_AI_WEAK int gmb_distill_selfcheck(void)
{ int (*fn)(void) = (int (*)(void))ai_sym("gmb_distill_selfcheck"); return fn ? fn() : -1; }
