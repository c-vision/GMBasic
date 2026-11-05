/* test_wsp_doc.c — selfcheck del WSP Document (W2). Sintetico + opzionale modello reale.
 * Uso: test_wsp_doc [dir_modello [n_tensori_attesi [byte_attesi [out.json]]]] */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_doc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { const char *name; int nd; int64_t sh[2]; } T;

static void write_model(const char *dir, const T *t, int n, const char *cfg) {
    char p[300]; snprintf(p, sizeof p, "%s/model.safetensors", dir);
    llmwsp_wspec sp[64]; uint64_t tot = 0;
    for (int i = 0; i < n; i++) {
        sp[i].name = t[i].name; sp[i].dtype = "F32"; sp[i].ndim = t[i].nd;
        uint64_t e = 1; for (int d = 0; d < t[i].nd; d++) { sp[i].shape[d] = t[i].sh[d]; e *= (uint64_t)t[i].sh[d]; }
        tot += e * 4;
    }
    llmwsp_writer *w; char err[200];
    if (llmwsp_writer_open(p, sp, (size_t)n, &w, err, sizeof err) < 0) { fails++; return; }
    float *z = (float *)calloc(tot / 4 + 1, 4);
    llmwsp_writer_append(w, z, tot); free(z);
    llmwsp_writer_close(w, err, sizeof err);
    snprintf(p, sizeof p, "%s/config.json", dir);
    FILE *f = fopen(p, "wb"); fputs(cfg, f); fclose(f);
}

static void tiny(void) {
    char dir[] = "/tmp/wsp_doc_XXXXXX";
    if (!mkdtemp(dir)) { fails++; return; }
    /* H=8, 4 teste, 2 kv, hd=4 (head_dim esplicito), I=6, V=10, 2 layer, tied */
    const char *cfg = "{\"model_type\":\"qwen3\",\"hidden_size\":8,\"num_attention_heads\":4,\"num_key_value_heads\":2,"
                      "\"head_dim\":4,\"intermediate_size\":6,\"num_hidden_layers\":2,\"vocab_size\":10,\"tie_word_embeddings\":true,"
                      "\"rope_scaling\":{\"a\":[1,2,{\"b\":\"}\"}]}}";
    T t[64]; int n = 0;
    t[n++] = (T){"model.embed_tokens.weight", 2, {10, 8}};
    t[n++] = (T){"model.norm.weight", 1, {8}};
    static char nm[2][9][80]; static const char *sfx[9] = {"input_layernorm.weight", "post_attention_layernorm.weight",
        "self_attn.q_proj.weight", "self_attn.k_proj.weight", "self_attn.v_proj.weight", "self_attn.o_proj.weight",
        "mlp.gate_proj.weight", "mlp.up_proj.weight", "mlp.down_proj.weight"};
    static const T shp[9] = {{0,1,{8}},{0,1,{8}},{0,2,{16,8}},{0,2,{8,8}},{0,2,{8,8}},{0,2,{8,16}},{0,2,{6,8}},{0,2,{6,8}},{0,2,{8,6}}};
    for (int L = 0; L < 2; L++) for (int k = 0; k < 9; k++) {
        snprintf(nm[L][k], 80, "model.layers.%d.%s", L, sfx[k]);
        t[n] = shp[k]; t[n].name = nm[L][k]; n++;
    }
    t[n++] = (T){"model.layers.1.self_attn.q_norm.weight", 1, {4}};
    write_model(dir, t, n, cfg);

    char p[300], err[300];
    snprintf(p, sizeof p, "%s/model.safetensors", dir);
    llmwsp_store *s; llmwsp_doc *d;
    CHECK(llmwsp_store_open(p, &s, err, sizeof err) == 0, "open: %s", err);
    snprintf(p, sizeof p, "%s/config.json", dir);
    CHECK(llmwsp_doc_build(s, p, &d, err, sizeof err) == 0, "build: %s", err);
    if (!d) return;
    CHECK(llmwsp_doc_leaf_count(d) == (size_t)n, "leaves %zu", llmwsp_doc_leaf_count(d));
    CHECK(llmwsp_doc_check(d, err, sizeof err) == 0, "check: %s", err);
    CHECK(llmwsp_doc_unclassified(d) == 0 && llmwsp_doc_layers(d) == 2, "meta");
    const llmwsp_tensor *tt; int ax; int64_t a, b;
    CHECK(llmwsp_doc_resolve(d, "layers.1.attn.q.heads.3", &tt, &ax, &a, &b) == 0 && ax == 0 && a == 12 && b == 16, "q head 3");
    CHECK(llmwsp_doc_resolve(d, "layers.0.attn.o.heads.1", &tt, &ax, &a, &b) == 0 && ax == 1 && a == 4 && b == 8, "o head 1 (asse 1)");
    CHECK(llmwsp_doc_resolve(d, "layers.0.attn.k.kv_heads.1", &tt, &ax, &a, &b) == 0 && a == 4 && b == 8, "kv head");
    CHECK(llmwsp_doc_resolve(d, "layers.0.mlp.down.neurons.5", &tt, &ax, &a, &b) == 0 && ax == 1 && a == 5 && b == 6, "neurone");
    CHECK(llmwsp_doc_resolve(d, "layers.0.attn.q.heads.4", &tt, &ax, &a, &b) < 0, "indice oltre");
    CHECK(llmwsp_doc_resolve(d, "layers.0.attn.q.rows.0", &tt, &ax, &a, &b) < 0, "partizione ignota");
    CHECK(llmwsp_doc_resolve(d, "layers.9.attn.q", &tt, &ax, &a, &b) < 0, "foglia ignota");
    CHECK(llmwsp_doc_resolve(d, "embed", &tt, &ax, &a, &b) == 0 && b == 10, "foglia intera");
    char *js = llmwsp_doc_to_json(d);
    CHECK(js && strstr(js, "\"format\":\"gmb.wsp@1\"") && strstr(js, "{\"alias\":\"lm_head\",\"of\":\"embed\"}"), "json/tie");
    free(js);
    llmwsp_doc_free(d); llmwsp_store_close(s);

    /* config incoerente: hidden_size sbagliato -> il check deve segnalare */
    const char *bad = "{\"model_type\":\"qwen3\",\"hidden_size\":16,\"num_attention_heads\":4,\"num_key_value_heads\":2,"
                      "\"head_dim\":4,\"intermediate_size\":6,\"num_hidden_layers\":2,\"vocab_size\":10,\"tie_word_embeddings\":true}";
    snprintf(p, sizeof p, "%s/config.json", dir); FILE *f = fopen(p, "wb"); fputs(bad, f); fclose(f);
    snprintf(p, sizeof p, "%s/model.safetensors", dir); llmwsp_store_open(p, &s, err, sizeof err);
    snprintf(p, sizeof p, "%s/config.json", dir); llmwsp_doc_build(s, p, &d, err, sizeof err);
    CHECK(llmwsp_doc_check(d, err, sizeof err) > 0, "config incoerente deve violare");
    llmwsp_doc_free(d); llmwsp_store_close(s);
    /* layer mancante: config dichiara 3 layer */
    const char *bad2 = "{\"model_type\":\"qwen3\",\"hidden_size\":8,\"num_attention_heads\":4,\"num_key_value_heads\":2,"
                       "\"head_dim\":4,\"intermediate_size\":6,\"num_hidden_layers\":3,\"vocab_size\":10,\"tie_word_embeddings\":true}";
    snprintf(p, sizeof p, "%s/config.json", dir); f = fopen(p, "wb"); fputs(bad2, f); fclose(f);
    snprintf(p, sizeof p, "%s/model.safetensors", dir); llmwsp_store_open(p, &s, err, sizeof err);
    snprintf(p, sizeof p, "%s/config.json", dir); llmwsp_doc_build(s, p, &d, err, sizeof err);
    CHECK(llmwsp_doc_check(d, err, sizeof err) > 0 && strstr(err, "layers.2"), "layer mancante (%s)", err);
    llmwsp_doc_free(d); llmwsp_store_close(s);
}

static void real(const char *dir, long wn, long long wb, const char *outjson) {
    char p[1024], err[300];
    llmwsp_store *s; llmwsp_doc *d;
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open: %s", err);
    if (!s) return;
    snprintf(p, sizeof p, "%s/config.json", dir);
    CHECK(llmwsp_doc_build(s, p, &d, err, sizeof err) == 0, "build: %s", err);
    if (!d) return;
    int v = llmwsp_doc_check(d, err, sizeof err);
    printf("foglie=%zu layer=%d non-classificate=%zu violazioni=%d %s\n", llmwsp_doc_leaf_count(d), llmwsp_doc_layers(d),
           llmwsp_doc_unclassified(d), v, err);
    CHECK(v == 0, "violazioni");
    CHECK(llmwsp_doc_unclassified(d) == 0, "non classificati");
    if (wn) CHECK((long)llmwsp_store_count(s) == wn, "tensori");
    if (wb) CHECK((long long)llmwsp_store_total_bytes(s) == wb, "byte");
    if (outjson) {
        char *js = llmwsp_doc_to_json(d);
        FILE *f = fopen(outjson, "wb"); if (f) { fputs(js, f); fclose(f); }
        free(js);
    }
    llmwsp_doc_free(d); llmwsp_store_close(s);
}

int main(int argc, char **argv) {
    tiny();
    if (argc > 1) real(argv[1], argc > 2 ? atol(argv[2]) : 0, argc > 3 ? atoll(argv[3]) : 0, argc > 4 ? argv[4] : NULL);
    printf(fails ? "WSP DOC: %d FALLITI\n" : "WSP DOC: OK\n", fails);
    return fails != 0;
}
