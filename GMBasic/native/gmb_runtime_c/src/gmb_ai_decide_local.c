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
}

/* Apre/riusa il modello attivo. Ritorna 0 ok / -1 con motivo in err. */
static int ensure_model(char* err, size_t errcap)
{
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
    gmb_decision_head_init(&g_local.head);
    g_local.head_state = gmb_decision_head_load(model, &g_local.head);
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
    llmtoken_handle tok = (llmtoken_handle)c->tok;

    gmb_decision_kind_t kind_enum;
    if (kind && strcasecmp(kind, "NOUL") == 0) kind_enum = GMB_DECISION_NOUL;
    else if (kind && strcasecmp(kind, "SCORE") == 0) kind_enum = GMB_DECISION_SCORE;
    else kind_enum = GMB_DECISION_CHOICE;

    /* Opzioni effettive: bias/T della testa + token singolo. */
    gmb_decision_option_t eff[GMB_DECISION_MAX_OPTIONS];
    LlmEngineDecideOption eopt[GMB_DECISION_MAX_OPTIONS];
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
            set_err(err, errcap, c->err, NULL);
            return -1;
        }
        eff[i].bias = bias;
        eff[i].temperature = temp;
        eff[i].token_id = token;
        eopt[i].option_token_id = token;
        eopt[i].bias = bias;
        eopt[i].temperature = temp;
    }

    /* Prompt (parità col default C#: istruzioni fisse + Stato/opzioni). */
    const char* st = state ? state : "";
    char labels[4096]; labels[0] = 0;
    for (int i = 0; i < n_options; i++) {
        const char* lab = (eff[i].label && *eff[i].label) ? eff[i].label : eff[i].id;
        size_t used = strlen(labels);
        snprintf(labels + used, sizeof labels - used, "%s%s", i ? ", " : "", lab);
    }
    size_t need = strlen(st) + strlen(labels) + 512;
    char* user = (char*)malloc(need);
    char* prompt = (char*)malloc(LOCAL_PROMPT_CAP);
    int32_t* ids = (int32_t*)malloc((size_t)LOCAL_TOKEN_CAP * sizeof(int32_t));
    if (!user || !prompt || !ids) {
        free(user); free(prompt); free(ids);
        set_err(err, errcap, "local provider: memoria esaurita", NULL);
        return -1;
    }
    snprintf(user, need,
             "State:\n%s\nAllowed options: %s\nAnswer with exactly one option and nothing else.",
             st, labels);
    char rerr[LOCAL_ERR_CAP] = "";
    if (llmmodelio_chat_render(c->model, "You are a decision engine. Choose exactly one allowed option.",
                               user, prompt, LOCAL_PROMPT_CAP, rerr, sizeof rerr) != 0) {
        free(user); free(prompt); free(ids);
        set_err(err, errcap, "local provider: prompt non costruibile", rerr);
        return -1;
    }
    free(user);
    int n_ids = llmtoken_encode(tok, prompt, ids, LOCAL_TOKEN_CAP);
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

#endif /* !_WIN32 */
