/* gmb_ai_decide_local.c — provider locale MLX del decision layer (JEV/SystemOne) per le gambe C.
 *
 * Architettura (AGENTS §0bis): il core di inferenza è LLMEnv (ABI v8 CAP_DECIDE) e il runtime C
 * non ne contiene una copia. Questo file vive nella dylib `gmb_runtime_ai` (non-Windows, che linka
 * LLMEnv) e in Windows direttamente nel core; il dispatcher `gmb_ai_decide.c` lo chiama tramite
 * il forwarder del gateway.
 *
 * Flusso di una decisione (parità con MlxLlmCompleter.Decide C#):
 *   1. modello attivo del ruolo completion (`gmb_provider_active_model_path`);
 *   2. prompt col chat_template REALE del modello (`llmmodelio_chat_render`, fonte unica C);
 *   3. tokenizzazione (llmtoken) + prefill in una sessione nuova;
 *   4. un token per opzione (vincolo HARD: errore esplicito se multi-token);
 *   5. `decide_plan_create/run`: readout batch + softmax mascherata;
 *   6. `scoring_head.json` (bias/T_kind/head_T/verbalizer_ids): se presente, probabilità
 *      calibrata + confidence = p_top − media(resto); altrimenti confidence/calibrated restano
 *      null (mai copiati da probability).
 *
 * Il modello (e il tokenizer) sono caricati UNA volta e riusati; la sessione KV è per-chiamata.
 * Nessun fallback silenzioso: ogni fallimento riporta un motivo esplicito. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE   /* dladdr */
#endif

#include "gmb_ai_decide.h"
#include "gmb_decision_head.h"
#include <stdio.h>

#if defined(_WIN32)
/* Windows non ha `dlopen`/`dladdr` e il motore MLX è Apple-only: errore esplicito, non uno stub. */
int gmb_ai_decide_local_run(const char* state, const char* kind,
                            const gmb_decision_option_t* options, int n_options,
                            gmb_decision_result_t* out, char* err, size_t errcap)
{
    (void)state; (void)kind; (void)options; (void)n_options; (void)out;
    if (err && errcap)
        snprintf(err, errcap, "local provider non disponibile: motore MLX non supportato su Windows");
    return -1;
}

int gmb_ai_decide_local_run_group(const char* state, const gmb_decision_domain_t* domains,
                                  int n_domains, gmb_decision_result_t* out,
                                  char* err, size_t errcap)
{
    (void)state; (void)domains; (void)n_domains; (void)out;
    if (err && errcap)
        snprintf(err, errcap, "local provider non disponibile: motore MLX non supportato su Windows");
    return -1;
}
#else

#include "gmb_runtime.h"
#include "llmengine.h"
#include "llmtoken.h"
#include "llmmodelio_model.h"

#include <ctype.h>
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define LOCAL_MODEL_CAP   1200
#define LOCAL_ERR_CAP     512
#define LOCAL_PROMPT_CAP  131072
#define LOCAL_TOKEN_CAP   32768
typedef struct {
    int state;                       /* 0 = non init, 1 = pronto, -1 = errore */
    char err[LOCAL_ERR_CAP];
    char model[LOCAL_MODEL_CAP];
    void* lib;
    const LlmEngineLlmEngine* eng;
    LlmEngineLlmModel model_h;
    void* tok;                       /* llmtoken_handle */
    /* scoring_head.json (opzionale): -1 = non letto, 0 = assente, 1 = caricato. */
    int head_state;
    gmb_decision_head_t head;
    /* Config decide per il modello attivo (dalla config del provider completion, la stessa
     * che porta modelDir/engine): decide=off|on|auto, decideTemplate, decideCalibration. */
    char decide_mode[16];
    char decide_template[4096];
    char decide_calibration[LOCAL_MODEL_CAP];
    /* Prefix-cache MLX (opzionale): simboli risolti dalla dylib del motore. */
    int (*save_prefix)(void* mh, const char* name, void* sh);
    int (*append_auto_prefix)(void* sh, void* mh, const int32_t* ids, int n_ids, int min_match);
} local_ctx_t;

static local_ctx_t g_local;
static pthread_mutex_t g_local_lock = PTHREAD_MUTEX_INITIALIZER;

static void set_err(char* err, size_t cap, const char* msg, const char* detail)
{
    if (!err || cap == 0) return;
    if (detail && *detail) snprintf(err, cap, "%s: %s", msg, detail);
    else snprintf(err, cap, "%s", msg);
}

/* `~/...` -> $HOME/... (il C non espande la tilde del config BASIC, il C# sì). */
static void expand_home(const char* in, char* out, size_t cap)
{
    if (!in) { out[0] = 0; return; }
    if (in[0] == '~' && (in[1] == '/' || in[1] == 0)) {
        const char* home = getenv("HOME");
        if (home && *home) { snprintf(out, cap, "%s%s", home, in + 1); return; }
    }
    snprintf(out, cap, "%s", in);
}

static int slot_index(const char* key)
{
    if (!key || !*key) return -1;
    gmb_decision_option_t option = { key, key, -1, 0.0f, 1.0f };
    return gmb_decision_head_slot(&option);
}

static void *open_engine_lib(void)
{
    char path[4096];
    const char* home = getenv("HOME");
    const char* rid =
#if defined(__APPLE__) && defined(__aarch64__)
        "osx-arm64";
#elif defined(__APPLE__)
        "osx-x64";
#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm64__))
        "linux-arm64";
#else
        "linux-x64";
#endif
    static const char* const names[] = { "libgmb_ai_mlx.dylib", "libllm_ai_mlx.dylib",
                                         "libgmb_ai_mlx.so", "libllm_ai_mlx.so" };
    /* 1) cache canonica del runtime (dove il gateway trova le AI libs). */
    if (home && *home) {
        for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
            snprintf(path, sizeof path, "%s/.gmbasic/libs/%s/%s", home, rid, names[i]);
            struct stat st;
            if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (h) return h;
            }
        }
    }
    /* 2) accanto alla dylib AI (bundle che spedisce insieme runtime_ai + engine). */
    Dl_info info;
    if (dladdr((void*)&llmmodelio_chat_render, &info) && info.dli_fname && *info.dli_fname) {
        char dir[2048];
        snprintf(dir, sizeof dir, "%s", info.dli_fname);
        char* sl = strrchr(dir, '/');
        if (sl) {
            *sl = 0;
            for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
                snprintf(path, sizeof path, "%s/%s", dir, names[i]);
                void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (h) return h;
            }
        }
    }
    /* 3) nome nudo (DYLD_LIBRARY_PATH / rpath dell'host). */
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        void* h = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (h) return h;
    }
    return NULL;
}

static void close_locked(local_ctx_t* c)
{
    if (c->eng && c->model_h) c->eng->free_model(c->model_h);
    c->model_h = NULL;
    if (c->tok) llmtoken_free((llmtoken_handle)c->tok);
    c->tok = NULL;
    if (c->lib) dlclose(c->lib);
    c->lib = NULL;
    c->eng = NULL;
    c->state = 0;
    c->head_state = -1;
    gmb_decision_head_init(&c->head);
    c->model[0] = 0;
    c->err[0] = 0;
    snprintf(c->decide_mode, sizeof c->decide_mode, "auto");
    c->decide_template[0] = 0;
    c->decide_calibration[0] = 0;
    c->save_prefix = NULL;
    c->append_auto_prefix = NULL;
}

/* Legge le chiavi decide* dalla config del provider completion attivo (JSON di AI REGISTER).
 * `gmb_json_extract` è un simbolo del core (ancorato dal gateway). Assente => default auto. */
static void load_decide_config(local_ctx_t* c) {
    snprintf(c->decide_mode, sizeof c->decide_mode, "auto");
    c->decide_template[0] = 0;
    c->decide_calibration[0] = 0;
    const char* cfg = gmb_provider_active_config("completion");
    if (!cfg || !*cfg) return;
    char* v = gmb_json_extract(cfg, "decide");
    if (v) { if (*v && strcmp(v, "null") != 0) snprintf(c->decide_mode, sizeof c->decide_mode, "%s", v); free(v); }
    v = gmb_json_extract(cfg, "decideTemplate");
    if (v) { if (*v && strcmp(v, "null") != 0) snprintf(c->decide_template, sizeof c->decide_template, "%s", v); free(v); }
    v = gmb_json_extract(cfg, "decideCalibration");
    if (v) { if (*v && strcmp(v, "null") != 0) { expand_home(v, c->decide_calibration, sizeof c->decide_calibration); } free(v); }
}

/* Apre/riusa il modello attivo. Ritorna 0 ok / -1 con motivo in err. */
static int ensure_model(char* err, size_t errcap)
{
    load_decide_config(&g_local);
    if (strcasecmp(g_local.decide_mode, "off") == 0) {
        set_err(err, errcap,
                "local provider: decisione disabilitata per questo modello (decide=off)", NULL);
        return -1;
    }
    const char* active = gmb_ai_decide_active_model_path();
    if (!active || !*active) {
        set_err(err, errcap, "local provider non disponibile",
                "nessun modello MLX attivo (AI REGISTER con path o \"mlx\"+modelDir + AI SET ACTIVE)");
        return -1;
    }
    char model[LOCAL_MODEL_CAP];
    expand_home(active, model, sizeof model);

    if (g_local.state == 1 && strcmp(g_local.model, model) == 0) return 0;
    if (g_local.state == 1) close_locked(&g_local);

    void* lib = open_engine_lib();
    if (!lib) {
        set_err(err, errcap, "local provider non disponibile",
                "libgmb_ai_mlx/libllm_ai_mlx non caricabile");
        return -1;
    }
    typedef const LlmEngineLlmEngine* (*engine_get_fn)(void);
    engine_get_fn get = (engine_get_fn)dlsym(lib, "llmengine_llm_engine_mlx");
    const LlmEngineLlmEngine* eng = get ? get() : NULL;
    if (!eng || !eng->load_model || !eng->create_session || !eng->destroy_session ||
        !eng->append_ids || !eng->decide_plan_create || !eng->decide_plan_run ||
        !eng->decide_plan_free) {
        dlclose(lib);
        set_err(err, errcap, "local provider non disponibile",
                "motore MLX senza CAP_DECIDE (dylib non allineata)");
        return -1;
    }

    LlmEngineLlmModel m = eng->load_model(model);
    if (!m) {
        dlclose(lib);
        set_err(err, errcap, "local provider: modello non caricabile", model);
        return -1;
    }
    void* tok = llmmodelio_tokenizer_open(model);
    if (!tok) {
        eng->free_model(m);
        dlclose(lib);
        set_err(err, errcap, "local provider: tokenizer non caricabile", model);
        return -1;
    }

    snprintf(g_local.model, sizeof g_local.model, "%s", model);
    g_local.lib = lib;
    g_local.eng = eng;
    g_local.model_h = m;
    g_local.tok = tok;
    g_local.save_prefix = (int (*)(void*, const char*, void*))
        dlsym(lib, "llm_ai_mlx_model_save_prefix");
    g_local.append_auto_prefix = (int (*)(void*, void*, const int32_t*, int, int))
        dlsym(lib, "llm_ai_mlx_session_append_auto_prefix");
    gmb_decision_head_init(&g_local.head);
    g_local.head_state = g_local.decide_calibration[0]
        ? gmb_decision_head_load_file(g_local.decide_calibration, &g_local.head)
        : gmb_decision_head_load(model, &g_local.head);
    if (g_local.head_state < 0) {
        /* Artefatto presente ma illeggibile: errore esplicito (parità col provider C#),
         * non una decisione "non calibrata" silenziosa. */
        llmtoken_free((llmtoken_handle)g_local.tok);
        g_local.tok = NULL;
        eng->free_model(m);
        dlclose(lib);
        g_local.model_h = NULL;
        g_local.lib = NULL;
        g_local.eng = NULL;
        set_err(err, errcap, "local provider: scoring_head.json illeggibile", model);
        return -1;
    }
    g_local.state = 1;
    return 0;
}

static int encode_one(llmtoken_handle tok, const char* label, const char* slot_key, int slot)
{
    if (slot >= 0 && g_local.head.token_id[slot] >= 0) return g_local.head.token_id[slot];
    const char* text = (label && *label) ? label : slot_key;
    int32_t ids[8];
    int n = llmtoken_encode(tok, text, ids, 8);
    if (n == 1) return ids[0];
    char buf[256];
    snprintf(buf, sizeof buf, " %s", text);
    n = llmtoken_encode(tok, buf, ids, 8);
    return n == 1 ? ids[0] : -1;
}

static gmb_decision_kind_t kind_enum_of(const char* kind)
{
    if (kind && strcasecmp(kind, "NOUL") == 0) return GMB_DECISION_NOUL;
    /* SCORE e SCORE:lo:hi usano la stessa temperatura per-kind. */
    if (kind && (strcasecmp(kind, "SCORE") == 0 || strncasecmp(kind, "SCORE:", 6) == 0))
        return GMB_DECISION_SCORE;
    return GMB_DECISION_CHOICE;
}

/* Applica bias/T della testa e risolve il token singolo di ogni opzione. `c->err` =
 * motivo se un'opzione non è un token (vincolo HARD, parità col provider C#). */
static int local_prepare_options(local_ctx_t* c, gmb_decision_kind_t kind_enum,
                                 const gmb_decision_option_t* options, int n_options,
                                 gmb_decision_option_t* eff, LlmEngineDecideOption* eopt)
{
    llmtoken_handle tok = (llmtoken_handle)c->tok;
    for (int i = 0; i < n_options; i++) {
        eff[i] = options[i];
        int slot = slot_index(options[i].id);
        if (slot < 0) slot = slot_index(options[i].label);
        float bias = options[i].bias;
        if (c->head_state == 1 && slot >= 0) bias += c->head.bias[slot];
        float temp = options[i].temperature;
        if (c->head_state == 1 && temp > 0.999f && temp < 1.001f)
            temp = c->head.T_kind[(int)kind_enum];
        if (!(temp > 0.0f)) temp = 1.0f;
        int token = encode_one(tok, options[i].label, options[i].id, slot);
        if (token < 0) {
            snprintf(c->err, sizeof c->err,
                     "local provider: l'opzione '%s' non è un token singolo",
                     (options[i].label && *options[i].label) ? options[i].label : options[i].id);
            return -1;
        }
        eff[i].bias = bias;
        eff[i].temperature = temp;
        eff[i].token_id = token;
        eopt[i].option_token_id = token;
        eopt[i].bias = bias;
        eopt[i].temperature = temp;
    }
    return 0;
}

/* Prompt (parità col default C#: istruzioni fisse + Stato/opzioni). */
static int local_build_prompt(local_ctx_t* c, const char* state,
                              const gmb_decision_option_t* eff, int n_options,
                              char* prompt, size_t cap, char* err, size_t errcap)
{
    const char* st = state ? state : "";
    char labels[4096]; labels[0] = 0;
    for (int i = 0; i < n_options; i++) {
        const char* lab = (eff[i].label && *eff[i].label) ? eff[i].label : eff[i].id;
        size_t used = strlen(labels);
        if (used >= sizeof labels) return -1;
        snprintf(labels + used, sizeof labels - used, "%s%s", i ? ", " : "", lab);
    }
    /* decideTemplate: se contiene {state}/{options} è un TEMPLATE del messaggio utente; se non
     * contiene placeholder è il testo di SISTEMA (override completo). Default invariato. */
    const char* sys = "You are a decision engine. Choose exactly one allowed option.";
    size_t base = strlen(st) + strlen(labels) + 512;
    size_t tplcap = strlen(c->decide_template);
    /* Capacità nel caso peggiore: ogni occorrenza dei placeholder può espandere lo stato/le
     * opzioni, quindi si dimensiona sul numero massimo di occorrenze (poi si taglia al buffer). */
    size_t cap2 = base;
    if (tplcap) {
        size_t worst = tplcap + (tplcap / 7 + 1) * strlen(st)
                     + (tplcap / 9 + 1) * strlen(labels) + 2;
        if (worst > cap2) cap2 = worst;
    }
    char* user = (char*)malloc(cap2);
    if (!user) { set_err(err, errcap, "local provider: memoria esaurita", NULL); return -1; }
    if (c->decide_template[0] &&
        (strstr(c->decide_template, "{state}") || strstr(c->decide_template, "{options}"))) {
        const char* t = c->decide_template;
        size_t o = 0;
        while (*t && o + 1 < cap2) {
            if (strncmp(t, "{state}", 7) == 0) {
                size_t l = strlen(st); memcpy(user + o, st, l); o += l; t += 7;
            } else if (strncmp(t, "{options}", 9) == 0) {
                size_t l = strlen(labels); memcpy(user + o, labels, l); o += l; t += 9;
            } else {
                user[o++] = *t++;
            }
        }
        user[o] = 0;
    } else {
        if (c->decide_template[0]) sys = c->decide_template;
        snprintf(user, cap2,
                 "State:\n%s\nAllowed options: %s\nAnswer with exactly one option and nothing else.",
                 st, labels);
    }
    char rerr[LOCAL_ERR_CAP] = "";
    int rc = llmmodelio_chat_render(c->model, sys, user, prompt, cap, rerr, sizeof rerr);
    free(user);
    if (rc != 0) set_err(err, errcap, "local provider: prompt non costruibile", rerr);
    return rc;
}

static void local_fill_result(local_ctx_t* c, gmb_decision_kind_t kind_enum,
                              const gmb_decision_option_t* eff, int n_options,
                              const float* prob, gmb_decision_result_t* out)
{
    int best = 0;
    for (int i = 1; i < n_options; i++) if (prob[i] > prob[best]) best = i;
    memset(out, 0, sizeof(*out));
    out->kind = kind_enum;
    out->option_id = eff[best].id;
    out->value = (eff[best].label && *eff[best].label) ? eff[best].label : eff[best].id;
    out->probability = prob[best];
    if (c->head_state == 1) {
        double sum = 0.0;
        for (int i = 0; i < n_options; i++) if (i != best) sum += prob[i];
        double mean_rest = n_options > 1 ? sum / (n_options - 1) : 0.0;
        double conf = (double)prob[best] - mean_rest;
        if (conf < 0.0) conf = 0.0;
        if (conf > 1.0) conf = 1.0;
        out->confidence = conf;
        out->has_confidence = 1;
        if (c->head.calibrated) {
            out->calibrated = prob[best];
            out->has_calibrated = 1;
        }
    }
}

int gmb_ai_decide_local_run(const char* state, const char* kind,
                            const gmb_decision_option_t* options, int n_options,
                            gmb_decision_result_t* out, char* err, size_t errcap)
{
    if (err && errcap) err[0] = 0;
    if (!out || !options || n_options < 1 || n_options > GMB_DECISION_MAX_OPTIONS) {
        set_err(err, errcap, "local provider: dominio non valido", NULL);
        return -1;
    }

    pthread_mutex_lock(&g_local_lock);
    int rc = ensure_model(err, errcap);
    pthread_mutex_unlock(&g_local_lock);
    if (rc != 0) return -1;

    local_ctx_t* c = &g_local;
    gmb_decision_kind_t kind_enum = kind_enum_of(kind);
    gmb_decision_option_t eff[GMB_DECISION_MAX_OPTIONS];
    LlmEngineDecideOption eopt[GMB_DECISION_MAX_OPTIONS];
    if (local_prepare_options(c, kind_enum, options, n_options, eff, eopt) != 0) {
        set_err(err, errcap, c->err, NULL);
        return -1;
    }

    char* prompt = (char*)malloc(LOCAL_PROMPT_CAP);
    int32_t* ids = (int32_t*)malloc((size_t)LOCAL_TOKEN_CAP * sizeof(int32_t));
    if (!prompt || !ids) {
        free(prompt); free(ids);
        set_err(err, errcap, "local provider: memoria esaurita", NULL);
        return -1;
    }
    if (local_build_prompt(c, state, eff, n_options, prompt, LOCAL_PROMPT_CAP, err, errcap) != 0) {
        free(prompt); free(ids);
        return -1;
    }
    int n_ids = llmtoken_encode((llmtoken_handle)c->tok, prompt, ids, LOCAL_TOKEN_CAP);
    free(prompt);
    if (n_ids <= 0 || n_ids >= LOCAL_TOKEN_CAP) {
        free(ids);
        set_err(err, errcap, "local provider: tokenizzazione del prompt fallita", NULL);
        return -1;
    }

    LlmEngineLlmSession sess = c->eng->create_session(c->model_h);
    if (!sess) {
        free(ids);
        set_err(err, errcap, "local provider: sessione non creata", NULL);
        return -1;
    }
    int ok = 0;
    void* plan = NULL;
    float* lp = NULL;
    float* prob = NULL;
    if (c->eng->append_ids(sess, ids, n_ids) == 0) {
        plan = c->eng->decide_plan_create(sess, eopt, n_options,
                                          c->head_state == 1 ? c->head.head_T : 1.0f);
        if (plan) {
            lp = (float*)malloc((size_t)n_options * sizeof(float));
            prob = (float*)malloc((size_t)n_options * sizeof(float));
            int argmax = 0;
            if (lp && prob && c->eng->decide_plan_run(plan, lp, prob, &argmax) == 0) {
                if (argmax < 0 || argmax >= n_options) argmax = 0;
                local_fill_result(c, kind_enum, eff, n_options, prob, out);
                ok = 1;
            }
        }
    }
    if (plan) c->eng->decide_plan_free(plan);
    c->eng->destroy_session(sess);
    free(lp);
    free(prob);
    free(ids);
    if (!ok) {
        set_err(err, errcap, "local provider: readout della decisione fallito", NULL);
        return -1;
    }
    return 1;
}

/* Batch su uno STESSO stato: UNA entry point per l'intero gruppo (parità di API col managed).
 *
 * Percorso di riferimento: un prefill lineare per dominio (numeri identici al dispatcher
 * singolo). La FUSIONE KV (prefill comune + `append_auto_prefix`) è disponibile dietro
 * `GMB_DECIDE_GROUP_PREFIX=1`: è la stessa strategia del provider managed e, con una
 * prefix-cache PULITA, è numericamente equivalente (0.982151/0.887789 vs 0.982288/0.888928;
 * differenza FP, argmax identico). Resta opt-in perché la radix del motore sceglie il match
 * PIÙ LUNGO: un prefisso "full prompt" rimasto in cache da build precedenti (tail=0) può
 * cambiare il readout. Default = prefill lineare: nessun numero sbagliato in silenzio. */
int gmb_ai_decide_local_run_group(const char* state, const gmb_decision_domain_t* domains,
                                  int n_domains, gmb_decision_result_t* out,
                                  char* err, size_t errcap)
{
    if (err && errcap) err[0] = 0;
    if (!domains || n_domains < 1 || n_domains > 32 || !out) {
        set_err(err, errcap, "local provider: batch non valido", NULL);
        return -1;
    }
    pthread_mutex_lock(&g_local_lock);
    int rc = ensure_model(err, errcap);
    pthread_mutex_unlock(&g_local_lock);
    if (rc != 0) return -1;

    local_ctx_t* c = &g_local;
    llmtoken_handle tok = (llmtoken_handle)c->tok;
    int rc2 = -1;
    char* prompt = (char*)malloc(LOCAL_PROMPT_CAP);
    int32_t* tmp = (int32_t*)malloc((size_t)LOCAL_TOKEN_CAP * sizeof(int32_t));
    gmb_decision_option_t* eff = (gmb_decision_option_t*)calloc(
        (size_t)n_domains * GMB_DECISION_MAX_OPTIONS, sizeof(gmb_decision_option_t));
    LlmEngineDecideOption* eopt = (LlmEngineDecideOption*)calloc(
        (size_t)n_domains * GMB_DECISION_MAX_OPTIONS, sizeof(LlmEngineDecideOption));
    int32_t** all_ids = (int32_t**)calloc((size_t)n_domains, sizeof(int32_t*));
    int* all_lens = (int*)calloc((size_t)n_domains, sizeof(int));
    if (!prompt || !tmp || !eff || !eopt || !all_ids || !all_lens) {
        set_err(err, errcap, "local provider: memoria esaurita", NULL);
        goto done;
    }

    /* Fase 1: opzioni, prompt e token di OGNI dominio (servono tutti per il prefisso comune). */
    for (int i = 0; i < n_domains; i++) {
        gmb_decision_option_t* ei = eff + (size_t)i * GMB_DECISION_MAX_OPTIONS;
        LlmEngineDecideOption* oi = eopt + (size_t)i * GMB_DECISION_MAX_OPTIONS;
        if (local_prepare_options(c, domains[i].kind, domains[i].options,
                                  domains[i].n_options, ei, oi) != 0) {
            set_err(err, errcap, c->err, NULL);
            goto done;
        }
        if (local_build_prompt(c, state, ei, domains[i].n_options,
                               prompt, LOCAL_PROMPT_CAP, err, errcap) != 0)
            goto done;
        int n = llmtoken_encode(tok, prompt, tmp, LOCAL_TOKEN_CAP);
        if (n <= 0 || n >= LOCAL_TOKEN_CAP) {
            set_err(err, errcap, "local provider: tokenizzazione del prompt fallita", NULL);
            goto done;
        }
        all_ids[i] = (int32_t*)malloc((size_t)n * sizeof(int32_t));
        if (!all_ids[i]) {
            set_err(err, errcap, "local provider: memoria esaurita", NULL);
            goto done;
        }
        memcpy(all_ids[i], tmp, (size_t)n * sizeof(int32_t));
        all_lens[i] = n;
    }
    free(prompt); prompt = NULL;
    free(tmp); tmp = NULL;

    /* Fase 2: fusione KV opzionale. Prefisso = match comune, con almeno 1 token di coda. */
    const char* pfenv = getenv("GMB_DECIDE_GROUP_PREFIX");
    int use_prefix = pfenv && *pfenv && strcmp(pfenv, "0") != 0
                  && c->save_prefix && c->append_auto_prefix;
    int common = 0;
    if (use_prefix) {
        int min_len = all_lens[0];
        for (int i = 1; i < n_domains; i++) if (all_lens[i] < min_len) min_len = all_lens[i];
        common = all_lens[0];
        for (int i = 1; i < n_domains; i++) {
            int m = common < all_lens[i] ? common : all_lens[i];
            int k = 0;
            while (k < m && all_ids[0][k] == all_ids[i][k]) k++;
            if (k < common) common = k;
        }
        if (common >= min_len) common = min_len - 1;
        if (common < 8) use_prefix = 0;
        if (use_prefix) {
            LlmEngineLlmSession ps = c->eng->create_session(c->model_h);
            if (!ps) use_prefix = 0;
            else {
                if (c->eng->append_ids(ps, all_ids[0], common) != 0) use_prefix = 0;
                else if (c->save_prefix((void*)c->model_h, "gmb_decide_group_c", (void*)ps) != 0)
                    use_prefix = 0;
                c->eng->destroy_session(ps);
            }
        }
    }
    {   /* Diagnostica opt-in. */
        const char* dbg = getenv("GMB_DECIDE_GROUP_DEBUG");
        if (dbg && *dbg)
            fprintf(stderr, "[decide-group-c] n=%d common=%d usePrefix=%d\n",
                    n_domains, common, use_prefix);
    }

    /* Fase 3: readout per dominio (lineare o KV riusata). */
    for (int i = 0; i < n_domains; i++) {
        gmb_decision_option_t* ei = eff + (size_t)i * GMB_DECISION_MAX_OPTIONS;
        LlmEngineDecideOption* oi = eopt + (size_t)i * GMB_DECISION_MAX_OPTIONS;
        LlmEngineLlmSession sess = c->eng->create_session(c->model_h);
        if (!sess) {
            set_err(err, errcap, "local provider: sessione non creata", NULL);
            goto done;
        }
        int appended = use_prefix
            ? c->append_auto_prefix((void*)sess, (void*)c->model_h, all_ids[i], all_lens[i], 8)
            : -1;
        if (appended < 0 && c->eng->append_ids(sess, all_ids[i], all_lens[i]) != 0) {
            c->eng->destroy_session(sess);
            set_err(err, errcap, "local provider: prefill della sessione fallito", NULL);
            goto done;
        }
        void* plan = c->eng->decide_plan_create(
            sess, oi, domains[i].n_options, c->head_state == 1 ? c->head.head_T : 1.0f);
        if (!plan) {
            c->eng->destroy_session(sess);
            set_err(err, errcap, "local provider: decide_plan_create fallito", NULL);
            goto done;
        }
        float* lp = (float*)malloc((size_t)domains[i].n_options * sizeof(float));
        float* prob = (float*)malloc((size_t)domains[i].n_options * sizeof(float));
        int argmax = 0;
        int ok = lp && prob && c->eng->decide_plan_run(plan, lp, prob, &argmax) == 0;
        if (ok) {
            if (argmax < 0 || argmax >= domains[i].n_options) argmax = 0;
            local_fill_result(c, domains[i].kind, ei, domains[i].n_options, prob, &out[i]);
        }
        free(lp); free(prob);
        c->eng->decide_plan_free(plan);
        c->eng->destroy_session(sess);
        if (!ok) {
            set_err(err, errcap, "local provider: readout della decisione fallito", NULL);
            goto done;
        }
    }
    rc2 = 1;

done:
    free(prompt);
    free(tmp);
    if (all_ids) for (int i = 0; i < n_domains; i++) free(all_ids[i]);
    free(all_ids); free(all_lens); free(eff); free(eopt);
    return rc2;
}

#endif /* !_WIN32 */
