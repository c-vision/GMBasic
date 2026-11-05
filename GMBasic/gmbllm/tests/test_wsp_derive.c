/* test_wsp_derive.c — selfcheck della chirurgia di architettura (W7). Modello GQA minuscolo con valori unici
 * (ogni elemento = indice tensore*10000 + posizione) => oracolo esatto per ogni elemento dell'uscita.
 * Uso: test_wsp_derive [dir_modello_reale out_base [dir_modello_quantizzato]] */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_derive.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

enum { H = 8, NH = 4, NKV = 2, HD = 4, GR = 2, I = 6, V = 8, L = 3 };

typedef struct { char name[96]; int nd; int64_t sh[2]; float *v; } Src;
static Src src[2 + 11 * L]; static int nsrc;

static Src *S(const char *name) { for (int i = 0; i < nsrc; i++) if (!strcmp(src[i].name, name)) return &src[i]; return NULL; }

static void add(const char *name, int nd, int64_t a, int64_t b) {
    Src *x = &src[nsrc]; snprintf(x->name, sizeof x->name, "%s", name); x->nd = nd; x->sh[0] = a; x->sh[1] = b;
    size_t e = (size_t)a * (size_t)(nd == 2 ? b : 1); x->v = (float *)malloc(e * 4);
    for (size_t i = 0; i < e; i++) x->v[i] = (float)(nsrc * 10000 + (int)i);
    nsrc++;
}

static void build(const char *dir) {
    nsrc = 0;
    add("model.embed_tokens.weight", 2, V, H); add("model.norm.weight", 1, H, 0);
    for (int l = 0; l < L; l++) {
        char n[96];
        #define A(sfx, nd, a, b) do { snprintf(n, sizeof n, "model.layers.%d.%s", l, sfx); add(n, nd, a, b); } while (0)
        A("input_layernorm.weight", 1, H, 0); A("post_attention_layernorm.weight", 1, H, 0);
        A("self_attn.q_proj.weight", 2, NH * HD, H); A("self_attn.k_proj.weight", 2, NKV * HD, H);
        A("self_attn.v_proj.weight", 2, NKV * HD, H); A("self_attn.o_proj.weight", 2, H, NH * HD);
        A("self_attn.q_norm.weight", 1, HD, 0); A("self_attn.k_norm.weight", 1, HD, 0);
        A("mlp.gate_proj.weight", 2, I, H); A("mlp.up_proj.weight", 2, I, H); A("mlp.down_proj.weight", 2, H, I);
    }
    llmwsp_wspec sp[64]; for (int i = 0; i < nsrc; i++) { sp[i].name = src[i].name; sp[i].dtype = "F32"; sp[i].ndim = src[i].nd; sp[i].shape[0] = src[i].sh[0]; if (src[i].nd == 2) sp[i].shape[1] = src[i].sh[1]; }
    char p[400], err[200]; snprintf(p, sizeof p, "%s/model.safetensors", dir);
    llmwsp_writer *w; llmwsp_writer_open(p, sp, (size_t)nsrc, &w, err, sizeof err);
    for (int i = 0; i < nsrc; i++) llmwsp_writer_append(w, src[i].v, (size_t)(src[i].sh[0] * (src[i].nd == 2 ? src[i].sh[1] : 1)) * 4);
    llmwsp_writer_close(w, err, sizeof err);
    snprintf(p, sizeof p, "%s/config.json", dir);
    FILE *f = fopen(p, "wb");
    fprintf(f, "{\n  \"model_type\": \"qwen3\",\n  \"hidden_size\": %d,\n  \"num_attention_heads\": %d,\n  \"num_key_value_heads\": %d,\n  \"head_dim\": %d,\n"
               "  \"intermediate_size\": %d,\n  \"num_hidden_layers\": %d,\n  \"vocab_size\": %d,\n  \"layer_types\": [\"a\", \"b\", \"c\"],\n  \"tie_word_embeddings\": true\n}\n", H, NH, NKV, HD, I, L, V);
    fclose(f);
    snprintf(p, sizeof p, "%s/arch", dir); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/arch/tiny.gmbarch", dir); f = fopen(p, "wb");
    fprintf(f, "{\"arch\":\"tiny\",\"geometry\":{\"hidden\":%d,\"n_layers\":%d,\"n_heads\":%d,\"n_kv_heads\":%d,\"inter\":%d}}\n", H, L, NH, NKV, I); fclose(f);
    snprintf(p, sizeof p, "%s/tokenizer.json", dir); f = fopen(p, "wb"); fputs("{\"tok\":1}", f); fclose(f);
}

static long cfgint(const char *path, const char *key) {
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    char b[4096]; size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f);
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key); char *p = strstr(b, pat); if (!p) return -1;
    p = strchr(p, ':'); return p ? atol(p + 1) : -1;
}

typedef struct { llmwsp_store *s; llmwsp_doc *d; char dir[400]; } Out;
static int open_out(Out *o, const char *dir) {
    char e[300], p[500]; snprintf(o->dir, sizeof o->dir, "%s", dir);
    if (llmwsp_store_open(dir, &o->s, e, sizeof e) < 0) return -1;
    snprintf(p, sizeof p, "%s/config.json", dir);
    return llmwsp_doc_build(o->s, p, &o->d, e, sizeof e);
}
static void close_out(Out *o) { llmwsp_doc_free(o->d); llmwsp_store_close(o->s); }

static float *readall(const llmwsp_store *s, const char *name, size_t *n) {
    const llmwsp_tensor *t = llmwsp_store_find(s, name); if (!t) return NULL;
    float *b = (float *)malloc((size_t)t->nbytes); llmwsp_read_bytes(s, t, 0, t->nbytes, b); *n = (size_t)t->nbytes / 4; return b;
}

/* confronto esatto elemento per elemento: out(name_out) deve valere src(name_src)[rmap[r]][cmap[c]] */
static void expect_tensor(const Out *o, const char *name_out, const char *name_src, const int *rmap, size_t nr, const int *cmap, size_t nc) {
    size_t n; float *g = readall(o->s, name_out, &n); Src *x = S(name_src);
    if (!g || !x) { CHECK(0, "tensore mancante %s / %s", name_out, name_src); free(g); return; }
    int64_t C = x->nd == 2 ? x->sh[1] : 1;
    size_t cols = x->nd == 2 ? (cmap ? nc : (size_t)C) : 1, rows = rmap ? nr : (size_t)x->sh[0];
    CHECK(n == rows * cols, "%s: elementi %zu, attesi %zu", name_out, n, rows * cols);
    for (size_t r = 0; r < rows && n == rows * cols; r++)
        for (size_t c = 0; c < cols; c++) {
            int64_t sr = rmap ? rmap[r] : (int64_t)r, sc = cmap ? cmap[c] : (int64_t)c;
            float want = x->v[sr * C + sc];
            if (g[r * cols + c] != want) { CHECK(0, "%s[%zu,%zu] = %g, atteso %g", name_out, r, c, g[r * cols + c], want); r = rows; break; }
        }
    free(g);
}

static void names(char *a, char *b, int lo, int ls, const char *sfx) {
    snprintf(a, 96, "model.layers.%d.%s", lo, sfx); snprintf(b, 96, "model.layers.%d.%s", ls, sfx);
}
static const char *PL[] = {"input_layernorm.weight", "post_attention_layernorm.weight", "self_attn.q_proj.weight", "self_attn.k_proj.weight",
    "self_attn.v_proj.weight", "self_attn.o_proj.weight", "self_attn.q_norm.weight", "self_attn.k_norm.weight", "mlp.gate_proj.weight",
    "mlp.up_proj.weight", "mlp.down_proj.weight"};

static void layers_identical(const Out *o, const int *map, int nl) {
    for (int j = 0; j < nl; j++) for (int k = 0; k < 11; k++) { char a[96], b[96]; names(a, b, j, map[j], PL[k]); expect_tensor(o, a, b, NULL, 0, NULL, 0); }
}

static void synthetic(void) {
    char dir[] = "/tmp/wsp_dv_XXXXXX", p[500], err[400];
    if (!mkdtemp(dir)) { fails++; return; }
    build(dir);
    llmwsp_store *s; llmwsp_doc *d;
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open");
    snprintf(p, sizeof p, "%s/config.json", dir);
    CHECK(llmwsp_doc_build(s, p, &d, err, sizeof err) == 0 && llmwsp_doc_check(d, err, sizeof err) == 0, "doc: %s", err);
    llmwsp_derive_report rep; Out o;
    #define NEWOUT(var) char var[400]; snprintf(var, sizeof var, "%s_%s", dir, #var); mkdir(var, 0755)

    /* 1) identità */
    NEWOUT(o_id); llmwsp_derive_ops ops; memset(&ops, 0, sizeof ops);
    CHECK(llmwsp_derive(s, d, dir, &ops, o_id, &rep, err, sizeof err) == 0, "identità: %s", err);
    CHECK(open_out(&o, o_id) == 0, "apri id"); { int m[3] = {0, 1, 2}; layers_identical(&o, m, 3); expect_tensor(&o, "model.embed_tokens.weight", "model.embed_tokens.weight", NULL, 0, NULL, 0); } close_out(&o);

    /* 2) rimozione layer 1 + config/arch/layer_types/tokenizer */
    NEWOUT(o_rm); int map_rm[2] = {0, 2}; ops.layer_map = map_rm; ops.nl = 2;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_rm, &rep, err, sizeof err) == 0, "remove: %s", err);
    CHECK(rep.out_layers == 2 && rep.bytes_after < rep.bytes_before, "report remove");
    CHECK(open_out(&o, o_rm) == 0, "apri rm"); layers_identical(&o, map_rm, 2); close_out(&o);
    snprintf(p, sizeof p, "%s/config.json", o_rm); CHECK(cfgint(p, "num_hidden_layers") == 2, "num_hidden_layers");
    { FILE *f = fopen(p, "rb"); char b[2048]; size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f); CHECK(strstr(b, "[\"a\", \"c\"]") != NULL, "layer_types rimappato: %s", strstr(b, "layer_types")); }
    snprintf(p, sizeof p, "%s/arch/tiny.gmbarch", o_rm); CHECK(cfgint(p, "n_layers") == 2 && cfgint(p, "n_heads") == NH, "gmbarch");
    snprintf(p, sizeof p, "%s/tokenizer.json", o_rm); CHECK(access(p, F_OK) == 0, "tokenizer copiato");
    snprintf(p, sizeof p, "%s/provenance.json", o_rm); CHECK(access(p, F_OK) == 0, "provenance");

    /* 3) duplicazione e riordino */
    NEWOUT(o_dup); int map_dup[4] = {0, 1, 1, 2}; ops.layer_map = map_dup; ops.nl = 4;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_dup, &rep, err, sizeof err) == 0, "dup: %s", err);
    CHECK(open_out(&o, o_dup) == 0, "apri dup"); layers_identical(&o, map_dup, 4); close_out(&o);
    NEWOUT(o_ro); int map_ro[3] = {2, 1, 0}; ops.layer_map = map_ro; ops.nl = 3;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_ro, &rep, err, sizeof err) == 0, "reorder: %s", err);
    CHECK(open_out(&o, o_ro) == 0, "apri ro"); layers_identical(&o, map_ro, 3); close_out(&o);

    /* 4) potatura gruppi kv (GQA): tieni 1 gruppo per layer, diverso per layer */
    NEWOUT(o_kv); memset(&ops, 0, sizeof ops); int kv[3] = {0, 1, 0}; ops.kv_keep = kv; ops.kv_m = 1;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_kv, &rep, err, sizeof err) == 0, "kv prune: %s", err);
    CHECK(rep.heads == GR && rep.kv_heads == 1, "report heads %lld/%lld", (long long)rep.heads, (long long)rep.kv_heads);
    snprintf(p, sizeof p, "%s/config.json", o_kv);
    CHECK(cfgint(p, "num_attention_heads") == 2 && cfgint(p, "num_key_value_heads") == 1 && cfgint(p, "head_dim") == HD, "config kv");
    CHECK(open_out(&o, o_kv) == 0, "apri kv");
    for (int j = 0; j < L; j++) {
        int g = kv[j]; int qr[GR * HD], kr[HD], oc[GR * HD];
        for (int h = 0; h < GR; h++) for (int i = 0; i < HD; i++) { qr[h * HD + i] = (g * GR + h) * HD + i; oc[h * HD + i] = qr[h * HD + i]; }
        for (int i = 0; i < HD; i++) kr[i] = g * HD + i;
        char a[96], b[96];
        names(a, b, j, j, "self_attn.q_proj.weight"); expect_tensor(&o, a, b, qr, GR * HD, NULL, 0);
        names(a, b, j, j, "self_attn.k_proj.weight"); expect_tensor(&o, a, b, kr, HD, NULL, 0);
        names(a, b, j, j, "self_attn.v_proj.weight"); expect_tensor(&o, a, b, kr, HD, NULL, 0);
        names(a, b, j, j, "self_attn.o_proj.weight"); expect_tensor(&o, a, b, NULL, 0, oc, GR * HD);
        names(a, b, j, j, "self_attn.q_norm.weight"); expect_tensor(&o, a, b, NULL, 0, NULL, 0);
        names(a, b, j, j, "mlp.gate_proj.weight"); expect_tensor(&o, a, b, NULL, 0, NULL, 0);
    }
    close_out(&o);

    /* 5) potatura MLP con ordine non crescente */
    NEWOUT(o_mlp); memset(&ops, 0, sizeof ops); int nk[9] = {0, 2, 5, 1, 3, 4, 5, 0, 2}; ops.neuron_keep = nk; ops.neu_k = 3;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_mlp, &rep, err, sizeof err) == 0, "mlp prune: %s", err);
    snprintf(p, sizeof p, "%s/config.json", o_mlp); CHECK(cfgint(p, "intermediate_size") == 3, "intermediate_size");
    CHECK(open_out(&o, o_mlp) == 0, "apri mlp");
    for (int j = 0; j < L; j++) {
        char a[96], b[96]; const int *k = nk + 3 * j;
        names(a, b, j, j, "mlp.gate_proj.weight"); expect_tensor(&o, a, b, k, 3, NULL, 0);
        names(a, b, j, j, "mlp.up_proj.weight"); expect_tensor(&o, a, b, k, 3, NULL, 0);
        names(a, b, j, j, "mlp.down_proj.weight"); expect_tensor(&o, a, b, NULL, 0, k, 3);
        names(a, b, j, j, "self_attn.q_proj.weight"); expect_tensor(&o, a, b, NULL, 0, NULL, 0);
    }
    close_out(&o);

    /* 6) combinata: layer [2,0] + kv [1,0] per layer + neuroni */
    NEWOUT(o_all); int map2[2] = {2, 0}; int kv2[4] = {1, 0, 0, 1}, nk2[4] = {5, 4, 0, 1};
    ops.layer_map = map2; ops.nl = 2; ops.kv_keep = kv2; ops.kv_m = 2; ops.neuron_keep = nk2; ops.neu_k = 2;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_all, &rep, err, sizeof err) == 0, "combinata: %s", err);
    CHECK(open_out(&o, o_all) == 0, "apri all");
    for (int j = 0; j < 2; j++) {
        int sl = map2[j]; int qr[NH * HD], kr[NKV * HD], c = 0, c2 = 0;
        for (int a = 0; a < 2; a++) { int g = kv2[j * 2 + a]; for (int h = 0; h < GR; h++) for (int i = 0; i < HD; i++) qr[c++] = (g * GR + h) * HD + i; for (int i = 0; i < HD; i++) kr[c2++] = g * HD + i; }
        char a[96], b[96];
        names(a, b, j, sl, "self_attn.q_proj.weight"); expect_tensor(&o, a, b, qr, NH * HD, NULL, 0);
        names(a, b, j, sl, "self_attn.k_proj.weight"); expect_tensor(&o, a, b, kr, NKV * HD, NULL, 0);
        names(a, b, j, sl, "self_attn.o_proj.weight"); expect_tensor(&o, a, b, NULL, 0, qr, NH * HD);
        names(a, b, j, sl, "mlp.down_proj.weight"); expect_tensor(&o, a, b, NULL, 0, nk2 + 2 * j, 2);
        names(a, b, j, sl, "post_attention_layernorm.weight"); expect_tensor(&o, a, b, NULL, 0, NULL, 0);
    }
    close_out(&o);

    /* 7) il validatore RIFIUTA prima di scrivere */
    NEWOUT(o_bad); memset(&ops, 0, sizeof ops);
    int bm[2] = {0, 3}; ops.layer_map = bm; ops.nl = 2;
    CHECK(llmwsp_derive(s, d, dir, &ops, o_bad, &rep, err, sizeof err) < 0, "mappa fuori intervallo");
    snprintf(p, sizeof p, "%s/model.safetensors", o_bad); CHECK(access(p, F_OK) != 0, "nulla scritto dopo il rifiuto");
    memset(&ops, 0, sizeof ops); int dupkv[6] = {0, 0, 0, 0, 0, 0}; ops.kv_keep = dupkv; ops.kv_m = 2;
    CHECK(llmwsp_derive_validate(s, d, &ops, err, sizeof err) < 0, "gruppo kv duplicato");
    int okv[3] = {0, 0, 2}; ops.kv_keep = okv; ops.kv_m = 1;
    CHECK(llmwsp_derive_validate(s, d, &ops, err, sizeof err) < 0, "gruppo kv fuori intervallo");
    ops.kv_m = 0; CHECK(llmwsp_derive_validate(s, d, &ops, err, sizeof err) < 0, "kv_m = 0");
    ops.kv_m = 3; CHECK(llmwsp_derive_validate(s, d, &ops, err, sizeof err) < 0, "kv_m > kv_heads");
    memset(&ops, 0, sizeof ops); int onk[3] = {0, 1, 6}; ops.neuron_keep = onk; ops.neu_k = 1;
    CHECK(llmwsp_derive_validate(s, d, &ops, err, sizeof err) < 0, "neurone fuori intervallo");
    int dnk[6] = {1, 1, 1, 1, 1, 1}; ops.neuron_keep = dnk; ops.neu_k = 2;
    CHECK(llmwsp_derive_validate(s, d, &ops, err, sizeof err) < 0, "neuroni duplicati");
    memset(&ops, 0, sizeof ops);
    CHECK(llmwsp_derive(s, d, dir, &ops, o_id, &rep, err, sizeof err) < 0, "outdir non vuota rifiutata");

    /* 8) importanza e top-k */
    double sc[I], gs[NKV]; int top[3];
    CHECK(llmwsp_mlp_importance(s, d, 1, sc) == 0, "importanza mlp");
    int inc = 1; for (int i = 1; i < I; i++) if (!(sc[i] > sc[i - 1])) inc = 0;
    CHECK(inc, "importanza attesa crescente con l'indice (valori crescenti per costruzione)");
    CHECK(llmwsp_kvgroup_importance(s, d, 1, gs) == 0 && gs[1] > gs[0], "importanza gruppi kv");
    double tk[5] = {5, 1, 9, 3, 7}; llmwsp_topk_sorted(tk, 5, 2, top);
    CHECK(top[0] == 2 && top[1] == 4, "topk ordinato per indice: %d %d", top[0], top[1]);
    llmwsp_topk_sorted(tk, 5, 3, top); CHECK(top[0] == 0 && top[1] == 2 && top[2] == 4, "topk 3");
    llmwsp_doc_free(d); llmwsp_store_close(s);
}

static void real(const char *dir, const char *outbase, const char *qdir) {
    char p[1100], err[400]; llmwsp_store *s; llmwsp_doc *d; llmwsp_derive_report rep; clock_t t0 = clock();
    if (llmwsp_store_open(dir, &s, err, sizeof err) < 0) { CHECK(0, "open %s", err); return; }
    snprintf(p, sizeof p, "%s/config.json", dir);
    if (llmwsp_doc_build(s, p, &d, err, sizeof err) < 0) { CHECK(0, "doc"); return; }
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    /* (a) -1 layer: via "a ritroso dal penultimo" = rimuove il layer n-2 */
    int *map = malloc((size_t)g.layers * sizeof(int)); int c = 0;
    for (int l = 0; l < g.layers; l++) if (l != g.layers - 2) map[c++] = l;
    llmwsp_derive_ops ops; memset(&ops, 0, sizeof ops); ops.layer_map = map; ops.nl = (size_t)c;
    snprintf(p, sizeof p, "%s_a_layer", outbase);
    CHECK(llmwsp_derive(s, d, dir, &ops, p, &rep, err, sizeof err) == 0, "(a) %s", err);
    printf("(a) -1 layer: layer=%zu tensori=%zu byte %llu -> %llu\n", rep.out_layers, rep.tensors, (unsigned long long)rep.bytes_before, (unsigned long long)rep.bytes_after);
    /* (b)+(c): -25% neuroni MLP e -2 gruppi kv per layer, per importanza (baseline sui soli pesi) */
    size_t neuk = (size_t)g.inter * 3 / 4, kvm = (size_t)g.kv_heads - 2;
    int *nk = malloc((size_t)g.layers * neuk * sizeof(int)), *kk = malloc((size_t)g.layers * kvm * sizeof(int));
    double *sc = malloc((size_t)g.inter * sizeof(double)), *gs = malloc((size_t)g.kv_heads * sizeof(double));
    for (int l = 0; l < g.layers; l++) {
        CHECK(llmwsp_mlp_importance(s, d, l, sc) == 0 && llmwsp_kvgroup_importance(s, d, l, gs) == 0, "importanza layer %d", l);
        llmwsp_topk_sorted(sc, (size_t)g.inter, neuk, nk + (size_t)l * neuk);
        llmwsp_topk_sorted(gs, (size_t)g.kv_heads, kvm, kk + (size_t)l * kvm);
    }
    memset(&ops, 0, sizeof ops); ops.kv_keep = kk; ops.kv_m = kvm; ops.neuron_keep = nk; ops.neu_k = neuk;
    snprintf(p, sizeof p, "%s_bc_prune", outbase);
    CHECK(llmwsp_derive(s, d, dir, &ops, p, &rep, err, sizeof err) == 0, "(b,c) %s", err);
    printf("(b,c) neuroni %lld->%lld, gruppi kv %lld->%lld (teste %lld->%lld): byte %llu -> %llu (%.1f%%)\n", (long long)g.inter, (long long)rep.inter,
           (long long)g.kv_heads, (long long)rep.kv_heads, (long long)g.heads, (long long)rep.heads, (unsigned long long)rep.bytes_before,
           (unsigned long long)rep.bytes_after, 100.0 * (double)rep.bytes_after / (double)rep.bytes_before);
    /* sanity: la potatura su un modello quantizzato dev'essere rifiutata; la rimozione di layer no */
    if (qdir) {
        llmwsp_store *sq; llmwsp_doc *dq;
        if (llmwsp_store_open(qdir, &sq, err, sizeof err) == 0) {
            snprintf(p, sizeof p, "%s/config.json", qdir);
            if (llmwsp_doc_build(sq, p, &dq, err, sizeof err) == 0) {
                llmwsp_dims gq; llmwsp_doc_dims(dq, &gq);
                int *kq = calloc((size_t)gq.layers, sizeof(int)); llmwsp_derive_ops oq; memset(&oq, 0, sizeof oq); oq.kv_keep = kq; oq.kv_m = 1;
                CHECK(llmwsp_derive_validate(sq, dq, &oq, err, sizeof err) < 0, "potatura su quantizzato rifiutata");
                int *mq = malloc((size_t)gq.layers * sizeof(int)); for (int l = 0; l < gq.layers - 1; l++) mq[l] = l;
                memset(&oq, 0, sizeof oq); oq.layer_map = mq; oq.nl = (size_t)gq.layers - 1;
                snprintf(p, sizeof p, "%s_q_layer", outbase);
                CHECK(llmwsp_derive(sq, dq, qdir, &oq, p, &rep, err, sizeof err) == 0, "layer su quantizzato: %s", err);
                printf("quantizzato -1 layer: layer=%zu tensori=%zu byte %llu -> %llu\n", rep.out_layers, rep.tensors, (unsigned long long)rep.bytes_before, (unsigned long long)rep.bytes_after);
                free(kq); free(mq); llmwsp_doc_free(dq);
            }
            llmwsp_store_close(sq);
        }
    }
    printf("tempo: %.1f s\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    free(map); free(nk); free(kk); free(sc); free(gs); llmwsp_doc_free(d); llmwsp_store_close(s);
}

int main(int argc, char **argv) {
    synthetic();
    if (argc > 2) real(argv[1], argv[2], argc > 3 ? argv[3] : NULL);
    printf(fails ? "WSP DERIVE: %d FALLITI\n" : "WSP DERIVE: OK\n", fails);
    return fails != 0;
}
