/* wsp_eval.c — valutazione di modelli WSP sul motore MLX (C99, dlopen): PPL con teacher forcing e GATE KL contro un
 * modello di riferimento. Usa solo l'ABI esistente: llm_ai_mlx_session_{append,logprob_of,topk}.
 *
 *   MODO RIFERIMENTO:  wsp_eval <mlx.dylib> <token.dylib> <dir_modello> <testo.txt> <max_token> --ref <file_ref>
 *       tokenizza il testo, calcola NLL per posizione e salva i top-K (id, logprob) di OGNI posizione.
 *   MODO CONFRONTO:    wsp_eval <mlx.dylib> <token.dylib> <dir_modello> <testo.txt> <max_token> --vs <file_ref> [soglia_KL]
 *       riusa la STESSA sequenza di id del riferimento; calcola PPL, KL(P_ref || Q) e accordo sul top-1.
 *
 * KL TRONCATO: per ogni posizione KL = Σ_{i∈topK(P)} p_i (log p_i − log q_i) / Σ_{i∈topK(P)} p_i. È una stima per
 * difetto della KL piena (la coda oltre i top-K non è vista); K=32. Se soglia_KL è data e la KL media la supera, esce
 * con codice 3 (edit RIFIUTATA dal gate). Il testo di valutazione è una scelta del chiamante: riportarlo sempre. */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { K = 32 };
typedef void *(*load_fn)(const char *);
typedef void (*free_fn)(void *);
typedef void *(*cs_fn)(void *);
typedef void (*ds_fn)(void *);
typedef int (*app_fn)(void *, const int32_t *, int);
typedef int (*lp_fn)(void *, int32_t, float *);
typedef int (*tk_fn)(void *, int, int32_t *, float *);
typedef void *(*tload_fn)(const char *);
typedef int (*tenc_fn)(void *, const char *, int32_t *, int);

static void *sym(void *l, const char *n) { void *p = dlsym(l, n); if (!p) { fprintf(stderr, "simbolo mancante: %s\n", n); exit(1); } return p; }

int main(int argc, char **argv) {
    if (argc < 8) { fprintf(stderr, "uso: %s <mlx.dylib> <token.dylib> <dir> <testo> <max_token> --ref|--vs <file> [soglia_KL]\n", argv[0]); return 2; }
    int ref_mode = !strcmp(argv[6], "--ref");
    void *lm = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL), *lt = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!lm || !lt) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    load_fn load = (load_fn)sym(lm, "llm_ai_mlx_load_model"); free_fn fr = (free_fn)sym(lm, "llm_ai_mlx_free_model");
    cs_fn cs = (cs_fn)sym(lm, "llm_ai_mlx_create_session"); ds_fn ds = (ds_fn)sym(lm, "llm_ai_mlx_destroy_session");
    app_fn app = (app_fn)sym(lm, "llm_ai_mlx_session_append"); lp_fn lpo = (lp_fn)sym(lm, "llm_ai_mlx_session_logprob_of");
    tk_fn topk = (tk_fn)sym(lm, "llm_ai_mlx_session_topk");
    int maxn = atoi(argv[5]);
    int32_t *ids = (int32_t *)malloc((size_t)maxn * sizeof(int32_t)); int n = 0;
    int32_t *rids = NULL; int32_t *rk_id = NULL; float *rk_lp = NULL;
    if (ref_mode) {
        tload_fn tl = (tload_fn)sym(lt, "llmtoken_load"); tenc_fn te = (tenc_fn)sym(lt, "llmtoken_encode");
        char tp[1100]; snprintf(tp, sizeof tp, "%s/tokenizer.json", argv[3]);
        void *tk = tl(tp);
        FILE *f = fopen(argv[4], "rb");
        if (!tk || !f) { fprintf(stderr, "tokenizer o testo non apribili\n"); return 1; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        char *txt = (char *)malloc((size_t)sz + 1); if (fread(txt, 1, (size_t)sz, f) != (size_t)sz) return 1; txt[sz] = 0; fclose(f);
        n = te(tk, txt, ids, maxn);
        if (n > maxn) n = maxn;          /* llmtoken_encode può restituire il conteggio totale */
        free(txt);
        if (n < 8) { fprintf(stderr, "troppo pochi token (%d)\n", n); return 1; }
        rk_id = (int32_t *)calloc((size_t)n * K, sizeof(int32_t)); rk_lp = (float *)calloc((size_t)n * K, sizeof(float));
    } else {
        FILE *f = fopen(argv[7], "rb");
        int hdr[2];
        if (!f || fread(hdr, sizeof(int), 2, f) != 2 || hdr[1] != K) { fprintf(stderr, "file di riferimento non valido\n"); return 1; }
        n = hdr[0]; rids = (int32_t *)malloc((size_t)n * sizeof(int32_t));
        rk_id = (int32_t *)malloc((size_t)n * K * sizeof(int32_t)); rk_lp = (float *)malloc((size_t)n * K * sizeof(float));
        if (fread(rids, sizeof(int32_t), (size_t)n, f) != (size_t)n || fread(rk_id, sizeof(int32_t), (size_t)n * K, f) != (size_t)n * K ||
            fread(rk_lp, sizeof(float), (size_t)n * K, f) != (size_t)n * K) { fprintf(stderr, "riferimento troncato\n"); return 1; }
        fclose(f); memcpy(ids, rids, (size_t)n * sizeof(int32_t));
    }
    void *m = load(argv[3]);
    if (!m) { fprintf(stderr, "load fallita: %s\n", argv[3]); return 1; }
    void *s = cs(m);
    if (app(s, ids, 1) != 0) { fprintf(stderr, "append fallita\n"); return 1; }
    double nll = 0, klsum = 0, klmax = 0; int agree = 0, cnt = 0;
    for (int i = 1; i < n; i++) {                                  /* il pending predice ids[i] */
        float lp;
        if (lpo(s, ids[i], &lp) != 0) { fprintf(stderr, "logprob_of fallita alla posizione %d\n", i); return 1; }
        nll -= (double)lp;
        if (ref_mode) {
            if (topk(s, K, rk_id + (size_t)i * K, rk_lp + (size_t)i * K) != K) { fprintf(stderr, "topk fallita\n"); return 1; }
        } else {
            double mass = 0, kl = 0;
            for (int j = 0; j < K; j++) {
                float q; double p = exp((double)rk_lp[(size_t)i * K + j]);
                if (lpo(s, rk_id[(size_t)i * K + j], &q) != 0) { fprintf(stderr, "logprob_of(ref) fallita\n"); return 1; }
                mass += p; kl += p * ((double)rk_lp[(size_t)i * K + j] - (double)q);
            }
            kl = mass > 0 ? kl / mass : 0;
            klsum += kl; if (kl > klmax) klmax = kl;
            int32_t t1; float l1;
            if (topk(s, 1, &t1, &l1) == 1 && t1 == rk_id[(size_t)i * K]) agree++;
            cnt++;
        }
        if (app(s, ids + i, 1) != 0) { fprintf(stderr, "append fallita alla posizione %d\n", i); return 1; }
    }
    ds(s); fr(m);
    double ppl = exp(nll / (double)(n - 1));
    if (ref_mode) {
        FILE *f = fopen(argv[7], "wb"); int hdr[2] = {n, K};
        fwrite(hdr, sizeof(int), 2, f); fwrite(ids, sizeof(int32_t), (size_t)n, f);
        fwrite(rk_id, sizeof(int32_t), (size_t)n * K, f); fwrite(rk_lp, sizeof(float), (size_t)n * K, f); fclose(f);
        printf("RIFERIMENTO %s: %d token, PPL=%.4f, NLL medio=%.4f -> %s\n", argv[3], n, ppl, nll / (double)(n - 1), argv[7]);
        return 0;
    }
    double klm = klsum / (double)cnt;
    printf("%s: PPL=%.4f  KL_top%d(ref||q) media=%.5f max=%.5f  accordo top-1=%.1f%% (%d/%d)\n", argv[3], ppl, K, klm, klmax, 100.0 * agree / cnt, agree, cnt);
    if (argc > 8) { double th = atof(argv[8]); if (klm > th) { printf("GATE KL: RIFIUTATO (media %.5f > soglia %.5f)\n", klm, th); return 3; } printf("GATE KL: ok (soglia %.5f)\n", th); }
    return 0;
}
