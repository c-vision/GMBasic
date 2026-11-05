/* test_wsp_analyze.c — selfcheck degli analizzatori WSP (W4): modello con teste PIANTATE a risposta nota
 * (GQA 4 teste / 2 kv) + opzionale esecuzione su un modello reale.
 * Uso: test_wsp_analyze [dir_modello [layer]] */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_analyze.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 1e-9)

typedef struct { const char *name; int nd; int64_t sh[2]; } T;

static void build(const char *dir) {
    static const T t[] = {
        {"model.embed_tokens.weight", 2, {4, 8}}, {"model.norm.weight", 1, {8}},
        {"model.layers.0.input_layernorm.weight", 1, {8}}, {"model.layers.0.post_attention_layernorm.weight", 1, {8}},
        {"model.layers.0.self_attn.q_proj.weight", 2, {16, 8}}, {"model.layers.0.self_attn.k_proj.weight", 2, {8, 8}},
        {"model.layers.0.self_attn.v_proj.weight", 2, {8, 8}}, {"model.layers.0.self_attn.o_proj.weight", 2, {8, 16}},
        {"model.layers.0.mlp.gate_proj.weight", 2, {2, 8}}, {"model.layers.0.mlp.up_proj.weight", 2, {2, 8}},
        {"model.layers.0.mlp.down_proj.weight", 2, {8, 2}}};
    int n = (int)(sizeof t / sizeof t[0]);
    llmwsp_wspec sp[16]; float *buf[16];
    for (int i = 0; i < n; i++) {
        sp[i].name = t[i].name; sp[i].dtype = "F32"; sp[i].ndim = t[i].nd;
        size_t e = 1; for (int d = 0; d < t[i].nd; d++) { sp[i].shape[d] = t[i].sh[d]; e *= (size_t)t[i].sh[d]; }
        buf[i] = (float *)calloc(e, 4);
    }
    #define W(i) buf[i]
    float *q = W(4), *k = W(5), *v = W(6), *o = W(7);
    for (int i = 0; i < 4; i++) {
        v[i * 8 + i] = 1; v[(4 + i) * 8 + 4 + i] = 1;            /* V_0=[I|0], V_1=[0|I] */
        k[i * 8 + i] = 1;                                         /* K_0=[I|0] */
        q[i * 8 + i] = 1;                                         /* Q_0=[I|0] */
        q[(4 + i) * 8 + i] = -1;                                  /* Q_1=-[I|0] */
        o[i * 16 + 0 + i] = 1;                                    /* O_0=[I;0] */
        o[i * 16 + 4 + i] = -1;                                   /* O_1=-[I;0] */
        o[(4 + i) * 16 + 8 + i] = 2;                              /* O_2=2[0;I] */
        o[(4 + i) * 16 + 12 + i] = i == 0 ? 3.f : -1.f;           /* O_3=[0;diag(3,-1,-1,-1)] */
    }
    char p[300], err[200]; snprintf(p, sizeof p, "%s/model.safetensors", dir);
    llmwsp_writer *w;
    if (llmwsp_writer_open(p, sp, (size_t)n, &w, err, sizeof err) < 0) { fails++; return; }
    for (int i = 0; i < n; i++) {
        size_t e = 1; for (int d = 0; d < t[i].nd; d++) e *= (size_t)t[i].sh[d];
        llmwsp_writer_append(w, buf[i], e * 4); free(buf[i]);
    }
    llmwsp_writer_close(w, err, sizeof err);
    snprintf(p, sizeof p, "%s/config.json", dir);
    FILE *f = fopen(p, "wb");
    fputs("{\"model_type\":\"qwen3\",\"hidden_size\":8,\"num_attention_heads\":4,\"num_key_value_heads\":2,\"head_dim\":4,"
          "\"intermediate_size\":2,\"num_hidden_layers\":1,\"vocab_size\":4,\"tie_word_embeddings\":true}", f);
    fclose(f);
}

static void planted(void) {
    char dir[] = "/tmp/wsp_an_XXXXXX", p[300], err[300];
    if (!mkdtemp(dir)) { fails++; return; }
    build(dir);
    llmwsp_store *s; llmwsp_doc *d;
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open: %s", err);
    snprintf(p, sizeof p, "%s/config.json", dir);
    CHECK(llmwsp_doc_build(s, p, &d, err, sizeof err) == 0 && llmwsp_doc_check(d, err, sizeof err) == 0, "doc: %s", err);
    double wr[4], wi[4], sr, fp;
    /* OV: attese (sum_ratio, frac_pos) per testa 0..3 */
    const double ov[4][2] = {{1, 1}, {-1, 0}, {1, 1}, {0, 0.25}};
    for (int h = 0; h < 4; h++) {
        CHECK(llmwsp_an_circuit_spectrum(s, d, 0, h, LLMWSP_CIRCUIT_OV, wr, wi) == 0, "ov %d", h);
        llmwsp_an_copy_scores(wr, wi, 4, &sr, &fp);
        CHECK(NEAR(sr, ov[h][0]) && NEAR(fp, ov[h][1]), "OV testa %d: sum_ratio=%g frac_pos=%g (attesi %g %g)", h, sr, fp, ov[h][0], ov[h][1]);
    }
    /* GQA: le teste 0 e 1 condividono V_0 ma hanno O diversi -> segni opposti (verifica dell'indice kv) */
    const double qk[4] = {1, -1, 0, 0};
    for (int h = 0; h < 4; h++) {
        CHECK(llmwsp_an_circuit_spectrum(s, d, 0, h, LLMWSP_CIRCUIT_QK, wr, wi) == 0, "qk %d", h);
        llmwsp_an_copy_scores(wr, wi, 4, &sr, &fp);
        CHECK(NEAR(sr, qk[h]), "QK testa %d: %g (atteso %g)", h, sr, qk[h]);
    }
    /* composizione */
    double c[4], pk;
    CHECK(llmwsp_an_compose_cosines(s, d, 0, 0, 0, 0, 'v', c, &pk) == 0 && NEAR(pk, 4), "O_0 -> V_0: pk=%g", pk);
    CHECK(llmwsp_an_compose_cosines(s, d, 0, 0, 0, 2, 'v', c, &pk) == 0 && NEAR(pk, 0), "O_0 -> V_1: pk=%g", pk);
    CHECK(llmwsp_an_compose_cosines(s, d, 0, 2, 0, 2, 'v', c, &pk) == 0 && NEAR(pk, 4), "O_2 -> V_1: pk=%g", pk);
    CHECK(llmwsp_an_compose_cosines(s, d, 0, 0, 0, 0, 'z', c, &pk) < 0, "tipo ignoto");
    /* norme per testa (baseline) */
    double qn[4], kn[2], vn[2], on[4];
    CHECK(llmwsp_an_head_norms(s, d, 0, qn, kn, vn, on) == 0, "norme");
    CHECK(NEAR(on[0], 2) && NEAR(on[1], 2) && NEAR(on[2], 4) && NEAR(on[3], sqrt(12)), "norme o: %g %g %g %g", on[0], on[1], on[2], on[3]);
    CHECK(llmwsp_an_circuit_spectrum(s, d, 5, 0, LLMWSP_CIRCUIT_OV, wr, wi) < 0, "layer inesistente");
    llmwsp_doc_free(d); llmwsp_store_close(s);
}

static void real(const char *dir, int layer) {
    char p[1024], err[300]; llmwsp_store *s; llmwsp_doc *d;
    if (llmwsp_store_open(dir, &s, err, sizeof err) < 0) { CHECK(0, "open %s", err); return; }
    snprintf(p, sizeof p, "%s/config.json", dir);
    if (llmwsp_doc_build(s, p, &d, err, sizeof err) < 0) { CHECK(0, "doc %s", err); return; }
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    double *wr = malloc((size_t)g.head_dim * 8), *wi = malloc((size_t)g.head_dim * 8);
    double *on = malloc((size_t)g.heads * 8);
    clock_t t0 = clock();
    CHECK(llmwsp_an_head_norms(s, d, layer, NULL, NULL, NULL, on) == 0, "norme");
    printf("layer %d: testa | ||O||_F | OV sum_ratio | OV frac_pos | QK sum_ratio\n", layer);
    for (int h = 0; h < g.heads; h++) {
        double a, b, c2, dd;
        CHECK(llmwsp_an_circuit_spectrum(s, d, layer, h, LLMWSP_CIRCUIT_OV, wr, wi) == 0, "ov");
        llmwsp_an_copy_scores(wr, wi, (size_t)g.head_dim, &a, &b);
        CHECK(llmwsp_an_circuit_spectrum(s, d, layer, h, LLMWSP_CIRCUIT_QK, wr, wi) == 0, "qk");
        llmwsp_an_copy_scores(wr, wi, (size_t)g.head_dim, &c2, &dd);
        printf("  %2d | %8.3f | %+7.3f | %6.3f | %+7.3f\n", h, on[h], a, b, c2);
    }
    printf("tempo: %.1f s\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    free(wr); free(wi); free(on);
    llmwsp_doc_free(d); llmwsp_store_close(s);
}

int main(int argc, char **argv) {
    planted();
    if (argc > 1) real(argv[1], argc > 2 ? atoi(argv[2]) : 0);
    printf(fails ? "WSP ANALYZE: %d FALLITI\n" : "WSP ANALYZE: OK\n", fails);
    return fails != 0;
}
