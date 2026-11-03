/* decide_train.c — T4 CLI (PUL): training LoRA con la loss DECISIONALE (JEV).
 *
 * Uso:
 *   gmbllm_decide_train <model_dir> <dataset.jsonl> <ckpt_prefix>
 *                       [steps] [rank] [layer] [lr] [criterion] [verbalize] [seed]
 *
 * dataset.jsonl: una riga per esempio
 *   {"context":"…","options":["…","…"],"label":N}
 * (schema Open-Jev). Il prompt è costruito con il chat_template REALE del modello
 * (`llmmodelio_chat_render`) e le opzioni sono tokenizzate come nel provider locale
 * (`gmb_ai_decide_local.c`): con `verbalize=1` il prompt elenca `A) testo…` e il readout
 * usa il token dello slot (necessario per opzioni multi-parola); senza, ogni opzione deve
 * essere un token singolo (vincolo HARD, errore esplicito).
 *
 * Il passo di training è `llm_ai_mlx_train_decide_step` (grafo MLX: forward differenziabile
 * → ultima posizione → gather opzioni → CE/Brier/RPS) e l'ottimizzazione
 * `llm_ai_mlx_decide_train_lora` (AdamW su A/B). Salva `<ckpt>_best_{A,B}.raw` + `.meta`. */
#include "train/llm_train.h"
#include "llm_decide_loss.h"
#include "llmmodelio_model.h"
#include "llmengine.h"
#include "llmtoken.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define DT_MAX_OPTIONS 16
#define DT_CTX_CAP 16384
#define DT_OPT_CAP 512
#define DT_PROMPT_CAP (1u << 18)
#define DT_TOKEN_CAP 8192
#define DT_MAX_ITEMS 200000

typedef struct {
    char* context;
    char options[DT_MAX_OPTIONS][DT_OPT_CAP];
    int n_options;
    int label;
} DtItem;

typedef struct {
    int32_t* tokens;
    int n_tokens;
    int32_t option_ids[DT_MAX_OPTIONS];
    int n_options;
    int label;
} DtSample;

static void mkdirs_for(const char* path) {
    char buf[1200];
    snprintf(buf, sizeof buf, "%s", path);
    for (char* p = buf + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
    }
}

/* ---------- mini parser JSON per le righe del dataset ---------- */
static const char* jfind(const char* js, const char* key) {
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* q = strstr(js, pat);
    if (!q) return NULL;
    q = strchr(q + strlen(pat), ':');
    return q ? q + 1 : NULL;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parsa una stringa JSON da *pp (aggiorna il cursore). Copia in `out` (NUL-terminata);
 * supporta \" \\ \/ \b \f \n \r \t e \uXXXX (UTF-8). 0 ok / -1. */
static int jparse_str(const char** pp, char* out, size_t cap) {
    const char* p = *pp;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return -1;
    p++;
    size_t o = 0;
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            char e = *p++;
            switch (e) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++) {
                        int h = hexval(*p++);
                        if (h < 0) return -1;
                        cp = (cp << 4) | (unsigned)h;
                    }
                    if (cp == 0) cp = '?';
                    if (cp < 0x80) { if (o + 1 < cap) out[o++] = (char)cp; }
                    else if (cp < 0x800) {
                        if (o + 2 < cap) {
                            out[o++] = (char)(0xC0 | (cp >> 6));
                            out[o++] = (char)(0x80 | (cp & 0x3F));
                        }
                    } else {
                        if (o + 3 < cap) {
                            out[o++] = (char)(0xE0 | (cp >> 12));
                            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            out[o++] = (char)(0x80 | (cp & 0x3F));
                        }
                    }
                    continue;
                }
                default: c = (unsigned char)e; break;
            }
        }
        if (o + 1 < cap) out[o++] = (char)c;
        else return -1;  /* niente troncamento silenzioso */
    }
    if (*p != '"') return -1;
    *pp = p + 1;
    out[o] = 0;
    return 0;
}

static int parse_item(const char* line, DtItem* it) {
    memset(it, 0, sizeof *it);
    const char* pc = jfind(line, "context");
    if (!pc) return -1;
    char ctx[DT_CTX_CAP];
    if (jparse_str(&pc, ctx, sizeof ctx) != 0) return -1;
    it->context = strdup(ctx);
    if (!it->context) return -1;

    const char* po = jfind(line, "options");
    if (!po) return -1;
    while (*po == ' ' || *po == '\t') po++;
    if (*po != '[') return -1;
    po++;
    for (;;) {
        while (*po == ' ' || *po == '\t' || *po == '\n' || *po == '\r') po++;
        if (*po == ']') { po++; break; }
        if (it->n_options >= DT_MAX_OPTIONS) return -1;
        if (jparse_str(&po, it->options[it->n_options], DT_OPT_CAP) != 0) return -1;
        it->n_options++;
        while (*po == ' ' || *po == '\t') po++;
        if (*po == ',') { po++; continue; }
        if (*po == ']') { po++; break; }
        return -1;
    }
    const char* pl = jfind(line, "label");
    if (!pl) return -1;
    it->label = (int)strtol(pl, NULL, 10);
    if (it->n_options < 1 || it->label < 0 || it->label >= it->n_options) return -1;
    return 0;
}

/* ---------- config.json: hidden_size / num_kv_heads / head_dim ---------- */
static long jnum(const char* js, const char* key, long dflt) {
    const char* p = jfind(js, key);
    return p ? strtol(p, NULL, 10) : dflt;
}

static int model_dims(const char* dir, int* hidden, int* out_dim) {
    char p[1200];
    snprintf(p, sizeof p, "%s/config.json", dir);
    FILE* f = fopen(p, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)n + 1);
    if (!js) { fclose(f); return -1; }
    if (fread(js, 1, (size_t)n, f) != (size_t)n) { free(js); fclose(f); return -1; }
    js[n] = 0;
    fclose(f);
    long h = jnum(js, "hidden_size", 0);
    long heads = jnum(js, "num_attention_heads", 0);
    long kv = jnum(js, "num_key_value_heads", heads);
    long hd = jnum(js, "head_dim", (heads > 0 && h > 0) ? h / heads : 0);
    free(js);
    if (h <= 0 || kv <= 0 || hd <= 0) return -1;
    *hidden = (int)h;
    *out_dim = (int)(kv * hd);
    return 0;
}

static int criterion_of(const char* s) {
    if (!s || !*s) return LLM_DECIDE_CRIT_CE;
    if (!strcasecmp(s, "brier")) return LLM_DECIDE_CRIT_BRIER;
    if (!strcasecmp(s, "rps")) return LLM_DECIDE_CRIT_RPS;
    if (!strcasecmp(s, "ce_rps") || !strcasecmp(s, "cerps")) return LLM_DECIDE_CRIT_CE_RPS;
    if (!strcasecmp(s, "ce")) return LLM_DECIDE_CRIT_CE;
    return -1;
}

/* Prompt = stesso default del provider locale/managed (Stato + opzioni + risposta). */
static int build_prompt(const char* model, int verbalize, const DtItem* it,
                        char* out, size_t cap, char* err, size_t errcap) {
    char labels[8192];
    labels[0] = 0;
    size_t used = 0;
    for (int i = 0; i < it->n_options; i++) {
        const char* text = it->options[i];
        int w = verbalize
            ? snprintf(labels + used, sizeof labels - used, "%s%c) %s", i ? ", " : "", 'A' + i, text)
            : snprintf(labels + used, sizeof labels - used, "%s%s", i ? ", " : "", text);
        if (w < 0 || (size_t)w >= sizeof labels - used) return -1;
        used += (size_t)w;
    }
    const char* answer = verbalize
        ? "Answer with exactly one option letter (A..P) and nothing else."
        : "Answer with exactly one option and nothing else.";
    size_t need = strlen(it->context) + used + 512;
    char* user = (char*)malloc(need);
    if (!user) return -1;
    snprintf(user, need, "State:\n%s\nAllowed options: %s\n%s", it->context, labels, answer);
    int rc = llmmodelio_chat_render(model,
        "You are a decision engine. Choose exactly one allowed option.",
        user, out, cap, err, errcap);
    free(user);
    return rc;
}

static int encode_option(llmtoken_handle tk, int verbalize, int index, const char* text) {
    char slot[8];
    const char* target;
    if (verbalize) { slot[0] = (char)('A' + index); slot[1] = 0; target = slot; }
    else target = text;
    int32_t ids[8];
    int n = llmtoken_encode(tk, target, ids, 8);
    if (n != 1) {
        char with_space[DT_OPT_CAP + 2];
        snprintf(with_space, sizeof with_space, " %s", target);
        n = llmtoken_encode(tk, with_space, ids, 8);
    }
    return n == 1 ? ids[0] : -1;
}

static double eval_samples(void* mh, const DtSample* samples, int n, int criterion,
                           float label_smoothing, float rps_weight,
                           int layer, const char* proj,
                           const float* A, int Ar, int Ac, const float* B, int Br, int Bc,
                           int max_items) {
    int count = 0;
    double acc = 0.0;
    for (int i = 0; i < n && (max_items <= 0 || count < max_items); i++) {
        llm_decide_train_sample_t s = {
            samples[i].tokens, samples[i].n_tokens, samples[i].option_ids,
            samples[i].n_options, samples[i].label
        };
        float L = 0.0f;
        if (llm_ai_mlx_train_decide_step(mh, s.tokens, s.n_tokens, s.option_ids, s.n_options,
                                         s.label, criterion, NULL, label_smoothing, rps_weight,
                                         layer, proj, A, Ar, Ac, B, Br, Bc,
                                         &L, NULL, NULL) == 0 && !isnan(L)) {
            acc += (double)L;
            count++;
        }
    }
    return count ? acc / (double)count : -1.0;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr,
            "uso: %s <model_dir> <dataset.jsonl> <ckpt_prefix>"
            " [steps] [rank] [layer] [lr] [criterion] [verbalize] [seed]\n"
            "  criterion: ce|brier|rps|ce_rps (default ce)\n"
            "  verbalize: 1 = opzioni A..P (multi-parola), 0 = token singolo (default 0)\n",
            argv[0]);
        return 2;
    }
    const char* model_dir = argv[1];
    const char* dataset = argv[2];
    const char* ckpt = argv[3];
    int steps = argc > 4 ? atoi(argv[4]) : 100;
    int rank = argc > 5 ? atoi(argv[5]) : 8;
    int layer = argc > 6 ? atoi(argv[6]) : 15;
    float lr = argc > 7 ? (float)atof(argv[7]) : 5e-3f;
    int criterion = argc > 8 ? criterion_of(argv[8]) : LLM_DECIDE_CRIT_CE;
    int verbalize = argc > 9 ? atoi(argv[9]) : 0;
    unsigned seed = argc > 10 ? (unsigned)strtoul(argv[10], NULL, 10) : 20261102u;
    if (steps < 1 || rank < 1 || rank > 64 || criterion < 0) {
        fprintf(stderr, "parametri non validi (steps>=1, 1<=rank<=64, criterion ce|brier|rps|ce_rps)\n");
        return 2;
    }

    /* 1) dataset */
    FILE* f = fopen(dataset, "rb");
    if (!f) { fprintf(stderr, "dataset non apribile: %s\n", dataset); return 2; }
    DtItem* items = (DtItem*)calloc(DT_MAX_ITEMS, sizeof(DtItem));
    if (!items) { fclose(f); return 2; }
    int n_items = 0, line_no = 0, bad = 0;
    char line[DT_CTX_CAP + DT_MAX_OPTIONS * DT_OPT_CAP + 1024];
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char* s = line;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        if (!*s || *s == '#') continue;
        if (n_items >= DT_MAX_ITEMS) { fprintf(stderr, "dataset oltre %d righe\n", DT_MAX_ITEMS); bad = 1; break; }
        if (parse_item(s, &items[n_items]) != 0) {
            fprintf(stderr, "riga %d non valida (context/options/label)\n", line_no);
            bad = 1;
            break;
        }
        n_items++;
    }
    fclose(f);
    if (bad || n_items < 1) { fprintf(stderr, "dataset vuoto o invalido\n"); return 1; }

    /* 2) motore + tokenizer + dimensioni */
    const LlmEngineLlmEngine* E = llmengine_llm_engine_mlx();
    if (!E || !E->load_model || !E->free_model) {
        fprintf(stderr, "motore MLX non disponibile (llmengine_llm_engine_mlx)\n");
        return 1;
    }
    int hidden = 0, out_dim = 0;
    if (model_dims(model_dir, &hidden, &out_dim) != 0) {
        fprintf(stderr, "config.json del modello non leggibile (hidden_size/heads/head_dim)\n");
        return 1;
    }
    llmtoken_handle tk = llmmodelio_tokenizer_open(model_dir);
    if (!tk) { fprintf(stderr, "tokenizer non caricabile: %s\n", model_dir); return 1; }
    LlmEngineLlmModel model = E->load_model(model_dir);
    if (!model) {
        fprintf(stderr, "modello non caricabile: %s\n", model_dir);
        llmtoken_free(tk);
        return 1;
    }

    /* 3) prompt/token per ogni esempio (fallisce esplicitamente: nessun troncamento) */
    DtSample* samples = (DtSample*)calloc((size_t)n_items, sizeof(DtSample));
    char* prompt = (char*)malloc(DT_PROMPT_CAP);
    int32_t* ids = (int32_t*)malloc((size_t)DT_TOKEN_CAP * sizeof(int32_t));
    char errbuf[512] = "";
    int ok = samples && prompt && ids;
    for (int i = 0; i < n_items && ok; i++) {
        if (build_prompt(model_dir, verbalize, &items[i], prompt, DT_PROMPT_CAP,
                         errbuf, sizeof errbuf) != 0) {
            fprintf(stderr, "prompt non costruibile per l'esempio %d: %s\n", i, errbuf);
            ok = 0;
            break;
        }
        int n = llmtoken_encode(tk, prompt, ids, DT_TOKEN_CAP);
        if (n <= 0 || n >= DT_TOKEN_CAP) {
            fprintf(stderr, "tokenizzazione fallita o troppo lunga per l'esempio %d\n", i);
            ok = 0;
            break;
        }
        samples[i].tokens = (int32_t*)malloc((size_t)n * sizeof(int32_t));
        if (!samples[i].tokens) { ok = 0; break; }
        memcpy(samples[i].tokens, ids, (size_t)n * sizeof(int32_t));
        samples[i].n_tokens = n;
        samples[i].n_options = items[i].n_options;
        samples[i].label = items[i].label;
        for (int k = 0; k < items[i].n_options; k++) {
            int tok = encode_option(tk, verbalize, k, items[i].options[k]);
            if (tok < 0) {
                fprintf(stderr,
                    "l'opzione '%s' (esempio %d) non è un token singolo"
                    " — usa verbalize=1 per opzioni multi-parola\n",
                    items[i].options[k], i);
                ok = 0;
                break;
            }
            samples[i].option_ids[k] = tok;
        }
    }
    free(prompt);
    free(ids);
    llmtoken_free(tk);
    if (!ok) { E->free_model(model); return 1; }

    /* 4) LoRA (A ~ gauss, B=0 → il modello parte dai pesi base) e training */
    const char* proj = "v_proj";
    /* A = [in,rank]; B = [rank,out] (stessa convenzione di llm_mlx_lora_apply: x@A@B). */
    int Ar = hidden, Ac = rank, Br = rank, Bc = out_dim;
    float* A = (float*)malloc((size_t)Ar * (size_t)Ac * sizeof(float));
    float* B = (float*)calloc((size_t)Br * (size_t)Bc, sizeof(float));
    if (!A || !B) {
        free(A); free(B);
        E->free_model(model);
        return 1;
    }
    unsigned rs = seed ? seed : 1u;
    for (size_t k = 0; k < (size_t)Ar * (size_t)Ac; k++) {
        float u = 0.0f;
        for (int j = 0; j < 12; j++) {
            rs ^= rs >> 12; rs ^= rs << 25; rs ^= rs >> 27;
            u += (float)((rs * 2685821657736338717ULL) >> 40) / 16777216.0f;
        }
        A[k] = (u - 6.0f) * 0.005f;
    }

    double base = eval_samples(model, samples, n_items, criterion, 0.0f, 0.5f,
                               layer, proj, A, Ar, Ac, B, Br, Bc, 8);
    float train_loss = 0.0f;
    llm_decide_train_sample_t* view = (llm_decide_train_sample_t*)calloc((size_t)n_items, sizeof(*view));
    if (!view) { free(A); free(B); E->free_model(model); return 1; }
    for (int i = 0; i < n_items; i++) {
        view[i].tokens = samples[i].tokens;
        view[i].n_tokens = samples[i].n_tokens;
        view[i].option_ids = samples[i].option_ids;
        view[i].n_options = samples[i].n_options;
        view[i].label = samples[i].label;
    }
    int trc = llm_ai_mlx_decide_train_lora(model, view, n_items, criterion, 0.0f, 0.5f,
                                           steps, lr, (int)seed, layer, proj,
                                           Ar, Ac, Br, Bc, &train_loss, A, B);
    free(view);
    if (trc != 0) {
        fprintf(stderr, "training fallito (passo decisionale)\n");
        free(A); free(B);
        E->free_model(model);
        return 1;
    }
    double final_loss = eval_samples(model, samples, n_items, criterion, 0.0f, 0.5f,
                                     layer, proj, A, Ar, Ac, B, Br, Bc, 0);

    /* 5) checkpoint (stessa convenzione degli altri trainer) */
    mkdirs_for(ckpt);
    char p[1200];
    snprintf(p, sizeof p, "%s_best_A.raw", ckpt);
    FILE* fa = fopen(p, "wb");
    if (fa) { fwrite(A, sizeof(float), (size_t)Ar * (size_t)Ac, fa); fclose(fa); }
    snprintf(p, sizeof p, "%s_best_B.raw", ckpt);
    FILE* fb = fopen(p, "wb");
    if (fb) { fwrite(B, sizeof(float), (size_t)Br * (size_t)Bc, fb); fclose(fb); }
    snprintf(p, sizeof p, "%s_best.meta", ckpt);
    FILE* fm = fopen(p, "w");
    if (fm) {
        fprintf(fm, "layer=%d\nrank=%d\nmod=%s\ncriterion=%d\nverbalize=%d\n"
                    "steps=%d\nlr=%.6g\nitems=%d\ntrain=%.6f\nbase=%.6f\n",
                layer, rank, proj, criterion, verbalize, steps, (double)lr, n_items,
                (double)train_loss, base);
        fclose(fm);
    }
    printf("DECIDE-TRAIN items=%d steps=%d rank=%d layer=%d criterion=%d verbalize=%d "
           "base=%.6f train=%.6f final=%.6f ckpt=%s_best\n",
           n_items, steps, rank, layer, criterion, verbalize, base,
           (double)train_loss, final_loss, ckpt);

    for (int i = 0; i < n_items; i++) {
        free(samples[i].tokens);
        free(items[i].context);
    }
    free(samples); free(items);
    free(A); free(B);
    E->free_model(model);
    return 0;
}
