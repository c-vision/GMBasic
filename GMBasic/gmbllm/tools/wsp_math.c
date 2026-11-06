/* wsp_math.c — valutazione MATH-500 ridotta sul motore MLX (C99, dlopen) per replicare, in forma RIDOTTA, l'esp. 8 del
 * paper WSP (Qwen3.5-0.8B, thinking on). Protocollo degli autori: chat template con thinking, utente = problema + "Please
 * reason step by step, and put your final answer within \boxed{}.", temperature 0.6, top_p 0.95, top_k 20, presence
 * penalty 1.5, fino a 32.768 token, ultima risposta \boxed{} verificata con math-verify. QUI: stesso prompt e stessi
 * parametri di campionamento, ma (a) sottoinsieme di problemi, (b) tetto di token molto più basso, (c) un campione per
 * problema, (d) confronto su stringhe NORMALIZZATE (non math-verify): i valori assoluti NON sono confrontabili con la
 * tabella del paper; contano il confronto base/modificato a parità di protocollo e la quota di risposte tagliate al tetto.
 * Uso: wsp_math <mlx.dylib> <token.dylib> <dir_modello> <test.jsonl> <inizio> <passo> <conteggio> <max_new> <seed> */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void *(*load_fn)(const char *);
typedef void (*free_fn)(void *);
typedef void *(*cs_fn)(void *);
typedef void (*ds_fn)(void *);
typedef int (*app_fn)(void *, const int32_t *, int);
typedef int (*sgen_fn)(void *, int, int32_t, int32_t *);
typedef int (*samp_fn)(void *, float, float, int, int64_t);
typedef int (*pen_fn)(void *, float);
typedef void *(*tload_fn)(const char *);
typedef int (*tenc_fn)(void *, const char *, int32_t *, int);
typedef int (*tdec_fn)(void *, const int32_t *, int, char *, int);

static void *sym(void *l, const char *n) { void *p = dlsym(l, n); if (!p) { fprintf(stderr, "simbolo mancante: %s\n", n); exit(1); } return p; }

/* estrae il valore stringa JSON di "key" (con unescape base) nella riga */
static char *jfield(const char *line, const char *key) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\": \"", key);
    const char *p = strstr(line, pat); if (!p) return NULL;
    p += strlen(pat);
    char *o = (char *)malloc(strlen(p) + 1); size_t k = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') o[k++] = '\n'; else if (*p == 't') o[k++] = '\t'; else if (*p == 'u') {
                unsigned cp = 0; sscanf(p + 1, "%4x", &cp); p += 4;
                if (cp < 0x80) o[k++] = (char)cp; else if (cp < 0x800) { o[k++] = (char)(0xC0 | (cp >> 6)); o[k++] = (char)(0x80 | (cp & 63)); }
                else { o[k++] = (char)(0xE0 | (cp >> 12)); o[k++] = (char)(0x80 | ((cp >> 6) & 63)); o[k++] = (char)(0x80 | (cp & 63)); }
            } else o[k++] = *p;
            p++;
        } else o[k++] = *p++;
    }
    o[k] = 0; return o;
}

/* contenuto dell'ULTIMO \boxed{...} (parentesi bilanciate); NULL se assente */
static char *last_boxed(const char *s) {
    const char *last = NULL, *p = s;
    while ((p = strstr(p, "\\boxed{")) != NULL) { last = p; p += 7; }
    if (!last) return NULL;
    const char *q = last + 7; int depth = 1; const char *st = q;
    while (*q && depth > 0) { if (*q == '{') depth++; else if (*q == '}') depth--; if (depth > 0) q++; }
    if (depth != 0) return NULL;
    char *o = (char *)malloc((size_t)(q - st) + 1); memcpy(o, st, (size_t)(q - st)); o[q - st] = 0; return o;
}

static void replace_all(char *s, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b); char *p;
    while ((p = strstr(s, a)) != NULL) { memmove(p + lb, p + la, strlen(p + la) + 1); memcpy(p, b, lb); }
}

/* normalizzazione grossolana per il confronto di risposte (NON math-verify) */
static void norm(char *s) {
    replace_all(s, "\\dfrac", "\\frac"); replace_all(s, "\\tfrac", "\\frac"); replace_all(s, "\\left", ""); replace_all(s, "\\right", "");
    replace_all(s, "\\!", ""); replace_all(s, "\\,", ""); replace_all(s, "\\;", ""); replace_all(s, "\\ ", ""); replace_all(s, "^\\circ", ""); replace_all(s, "^{\\circ}", "");
    replace_all(s, "\\$", ""); replace_all(s, "\\%", ""); replace_all(s, "%", ""); replace_all(s, "\\text{", "{"); replace_all(s, "\\mbox{", "{");
    size_t k = 0; for (size_t i = 0; s[i]; i++) if (!isspace((unsigned char)s[i]) && s[i] != '{' && s[i] != '}') s[k++] = s[i];
    s[k] = 0;
    while (k > 0 && s[k - 1] == '.') s[--k] = 0;
}

static int same_answer(const char *a, const char *b) {
    char *x = strdup(a), *y = strdup(b); norm(x); norm(y);
    int eq = strcmp(x, y) == 0;
    if (!eq) { char *e1, *e2; double u = strtod(x, &e1), v = strtod(y, &e2); if (*x && *y && !*e1 && !*e2 && fabs(u - v) < 1e-9 * (1 + fabs(v))) eq = 1; }
    free(x); free(y); return eq;
}

int main(int argc, char **argv) {
    if (argc < 10) { fprintf(stderr, "uso: %s <mlx.dylib> <tok.dylib> <modello> <test.jsonl> <inizio> <passo> <conteggio> <max_new> <seed>\n", argv[0]); return 2; }
    void *lm = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL), *lt = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!lm || !lt) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    load_fn load = (load_fn)sym(lm, "llm_ai_mlx_load_model"); free_fn fr = (free_fn)sym(lm, "llm_ai_mlx_free_model");
    cs_fn cs = (cs_fn)sym(lm, "llm_ai_mlx_create_session"); ds_fn ds = (ds_fn)sym(lm, "llm_ai_mlx_destroy_session");
    app_fn app = (app_fn)sym(lm, "llm_ai_mlx_session_append"); sgen_fn sgen = (sgen_fn)sym(lm, "llm_ai_mlx_session_generate");
    samp_fn samp = (samp_fn)sym(lm, "llm_ai_mlx_session_set_sampling"); pen_fn pres = (pen_fn)sym(lm, "llm_ai_mlx_session_set_presence_penalty");
    pen_fn minp = (pen_fn)sym(lm, "llm_ai_mlx_session_set_min_p");
    tload_fn tl = (tload_fn)sym(lt, "llmtoken_load"); tenc_fn te = (tenc_fn)sym(lt, "llmtoken_encode"); tdec_fn td = (tdec_fn)sym(lt, "llmtoken_decode");
    int start = atoi(argv[5]), step = atoi(argv[6]), count = atoi(argv[7]), maxnew = atoi(argv[8]); int64_t seed = atoll(argv[9]);
    char tp[1100]; snprintf(tp, sizeof tp, "%s/tokenizer.json", argv[3]);
    void *tk = tl(tp); void *m = load(argv[3]);
    if (!tk || !m) { fprintf(stderr, "tokenizer o modello non caricati\n"); return 1; }
    int32_t imend[4] = {0}; te(tk, "<|im_end|>", imend, 4);
    const int32_t EOS1 = imend[0], EOS2 = 248044;
    FILE *f = fopen(argv[4], "rb"); if (!f) return 1;
    char *line = (char *)malloc(1 << 20); int idx = 0, done = 0, correct = 0, cut = 0, noans = 0; long tot_tok = 0;
    int32_t *gen = (int32_t *)malloc((size_t)maxnew * sizeof(int32_t)), *pr = (int32_t *)malloc(8192 * sizeof(int32_t));
    char *text = (char *)malloc((size_t)maxnew * 8 + 4096);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    while (fgets(line, 1 << 20, f) && done < count) {
        int mine = idx >= start && (idx - start) % step == 0; idx++;
        if (!mine) continue;
        char *prob = jfield(line, "problem"), *ans = jfield(line, "answer");
        if (!prob || !ans) continue;
        static char prompt[1 << 16];
        snprintf(prompt, sizeof prompt, "<|im_start|>user\n%s\n\nPlease reason step by step, and put your final answer within \\boxed{}.<|im_end|>\n<|im_start|>assistant\n<think>\n", prob);
        int np = te(tk, prompt, pr, 8192); if (np > 8192) np = 8192;
        void *s = cs(m);
        samp(s, 0.6f, 0.95f, 20, seed + idx); pres(s, 1.5f); minp(s, 0.0f);
        app(s, pr, np);
        int ng = 0, finished = 0;
        while (ng < maxnew && !finished) {
            int want = maxnew - ng < 256 ? maxnew - ng : 256;
            int got = sgen(s, want, EOS1, gen + ng);
            if (got < 0) { fprintf(stderr, "generate fallita\n"); return 1; }
            if (got > 0 && (gen[ng + got - 1] == EOS1 || gen[ng + got - 1] == EOS2)) finished = 1;
            ng += got; if (got < want) finished = 1;
        }
        ds(s);
        td(tk, gen, ng, text, maxnew * 8 + 4000);
        char *b = last_boxed(text);
        int ok = b && same_answer(b, ans);
        done++; correct += ok; tot_tok += ng;
        int was_cut = !finished || ng >= maxnew; cut += was_cut; if (!b) noans++;
        printf("[%3d] idx=%d token=%d%s risposta=%s attesa=%s %s\n", done, idx - 1, ng, was_cut ? " (TAGLIATA)" : "", b ? b : "-", ans, ok ? "OK" : "no");
        fflush(stdout);
        free(b); free(prob); free(ans);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("RIEPILOGO %s: %d problemi, accuratezza %.1f%% (%d), tagliate al tetto %.1f%% (%d), senza \\boxed %.1f%%, token medi %.0f, tempo %.0fs\n",
           argv[3], done, 100.0 * correct / done, correct, 100.0 * cut / done, cut, 100.0 * noans / done, (double)tot_tok / done,
           (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9);
    fr(m);
    return 0;
}
