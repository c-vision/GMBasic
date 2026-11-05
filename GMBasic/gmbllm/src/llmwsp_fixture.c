/* llmwsp_fixture.c — fixture compilate a verità nota (W4). C99. */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_fixture.h"
#include "llmwsp_store.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { H = 16, HD = 4, NH = 4, I = 4, VOC = 4 };

static unsigned long long rs;
static double urand(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return ((double)(rs >> 11) + 1.0) / 9007199254740993.0; }
static float gauss(double sd) { return (float)(sd * sqrt(-2 * log(urand())) * cos(6.283185307179586 * urand())); }

typedef struct { char name[96]; int nd; int64_t sh[2]; float *buf; size_t e; } Tn;

static void set_role(llmwsp_fx_truth *t, int l, int h, const char *r) {
    for (int i = 0; i < t->nheads; i++) if (t->hd[i].layer == l && t->hd[i].head == h) { snprintf(t->hd[i].role, sizeof t->hd[i].role, "%s", r); return; }
}

int llmwsp_fixture_compile(const char *kind, const char *outdir, unsigned seed, llmwsp_fx_truth *truth, char *err, size_t errcap) {
    int L = !strcmp(kind, "copy") ? 1 : !strcmp(kind, "induction") ? 2 : 0;
    if (!L) { if (err && errcap) snprintf(err, errcap, "kind sconosciuto: %s", kind); return -1; }
    rs = 0x9E3779B97F4A7C15ULL ^ ((unsigned long long)seed * 0xBF58476D1CE4E5B9ULL); if (!rs) rs = 1;
    for (int i = 0; i < 8; i++) urand();
    llmwsp_fx_truth tr; memset(&tr, 0, sizeof tr);
    tr.layers = L; tr.heads = NH;
    for (int l = 0; l < L; l++) for (int h = 0; h < NH; h++) { llmwsp_fx_head *x = &tr.hd[tr.nheads++]; x->layer = l; x->head = h; snprintf(x->role, sizeof x->role, "noise"); }
    int n = 0, cap = 2 + L * 11;
    Tn *t = (Tn *)calloc((size_t)cap, sizeof *t);
    #define ADD(nm, nd_, a, b) do { Tn *x = &t[n++]; snprintf(x->name, sizeof x->name, "%s", nm); x->nd = nd_; x->sh[0] = a; x->sh[1] = b; \
        x->e = (size_t)(a) * (size_t)((nd_) == 2 ? (b) : 1); x->buf = (float *)calloc(x->e, 4); } while (0)
    ADD("model.embed_tokens.weight", 2, VOC, H);
    ADD("model.norm.weight", 1, H, 0);
    for (int l = 0; l < L; l++) {
        char nm[96];
        #define LN(sfx, nd_, a, b) do { snprintf(nm, sizeof nm, "model.layers.%d.%s", l, sfx); ADD(nm, nd_, a, b); } while (0)
        LN("input_layernorm.weight", 1, H, 0); LN("post_attention_layernorm.weight", 1, H, 0);
        LN("self_attn.q_proj.weight", 2, NH * HD, H); LN("self_attn.k_proj.weight", 2, NH * HD, H);
        LN("self_attn.v_proj.weight", 2, NH * HD, H); LN("self_attn.o_proj.weight", 2, H, NH * HD);
        LN("mlp.gate_proj.weight", 2, I, H); LN("mlp.up_proj.weight", 2, I, H); LN("mlp.down_proj.weight", 2, H, I);
    }
    /* indici: embed 0, norm 1, layer l: base 2 + 9*l: ln1,ln2,q,k,v,o,gate,up,down */
    for (int i = 0; i < VOC; i++) t[0].buf[i * H + i] = 1;                       /* token one-hot in T */
    for (int i = 0; i < H; i++) t[1].buf[i] = 1;
    for (int l = 0; l < L; l++) {
        int b = 2 + 9 * l;
        for (int i = 0; i < H; i++) { t[b].buf[i] = 1; t[b + 1].buf[i] = 1; }
        float *q = t[b + 2].buf, *k = t[b + 3].buf, *v = t[b + 4].buf, *o = t[b + 5].buf;
        for (int h = 0; h < NH; h++) {                                           /* rumore su tutte le teste */
            for (int r = 0; r < HD; r++) for (int c = 0; c < H; c++) {
                q[(h * HD + r) * H + c] = gauss(0.3); k[(h * HD + r) * H + c] = gauss(0.3); v[(h * HD + r) * H + c] = gauss(0.3);
            }
            for (int r = 0; r < H; r++) for (int c = 0; c < HD; c++) o[r * (NH * HD) + h * HD + c] = gauss(0.3);
        }
        #define CLEAR_Q(h) for (int r = 0; r < HD; r++) for (int c = 0; c < H; c++) q[((h) * HD + r) * H + c] = 0
        #define CLEAR_K(h) for (int r = 0; r < HD; r++) for (int c = 0; c < H; c++) k[((h) * HD + r) * H + c] = 0
        #define CLEAR_V(h) for (int r = 0; r < HD; r++) for (int c = 0; c < H; c++) v[((h) * HD + r) * H + c] = 0
        #define CLEAR_O(h) for (int r = 0; r < H; r++) for (int c = 0; c < HD; c++) o[r * (NH * HD) + (h) * HD + c] = 0
        if (l == 0 && !strcmp(kind, "copy")) {          /* L0H0: legge T, riscrive T (copia) */
            CLEAR_Q(0); CLEAR_K(0); CLEAR_V(0); CLEAR_O(0);
            for (int i = 0; i < HD; i++) { v[i * H + i] = 1; o[i * (NH * HD) + i] = 1; q[i * H + i] = 1; k[i * H + i] = 1; }
            set_role(&tr, 0, 0, "copy");
        }
        if (l == 0 && !strcmp(kind, "induction")) {     /* L0H0 previous-token: legge T, scrive P (il QK posizionale non è nei pesi) */
            CLEAR_Q(0); CLEAR_K(0); CLEAR_V(0); CLEAR_O(0);
            for (int i = 0; i < HD; i++) { v[i * H + i] = 1; o[(4 + i) * (NH * HD) + i] = 1; }
            set_role(&tr, 0, 0, "prevtoken");
        }
        if (l == 1) {                                   /* L1H0 induzione: Q legge T, K legge P, V legge T, O scrive T */
            CLEAR_Q(0); CLEAR_K(0); CLEAR_V(0); CLEAR_O(0);
            for (int i = 0; i < HD; i++) { q[i * H + i] = 1; k[i * H + 4 + i] = 1; v[i * H + i] = 1; o[i * (NH * HD) + i] = 1; }
            set_role(&tr, 1, 0, "induction");
            tr.edges[tr.nedges++] = (llmwsp_fx_edge){0, 0, 1, 0, 'k'};
        }
    }
    llmwsp_wspec *sp = (llmwsp_wspec *)calloc((size_t)n, sizeof *sp);
    for (int i = 0; i < n; i++) { sp[i].name = t[i].name; sp[i].dtype = "F32"; sp[i].ndim = t[i].nd; sp[i].shape[0] = t[i].sh[0]; if (t[i].nd == 2) sp[i].shape[1] = t[i].sh[1]; }
    char p[1100];
    snprintf(p, sizeof p, "%s/model.safetensors", outdir);
    llmwsp_writer *w = NULL; int rc = llmwsp_writer_open(p, sp, (size_t)n, &w, err, errcap);
    for (int i = 0; i < n && rc == 0; i++) rc = llmwsp_writer_append(w, t[i].buf, t[i].e * 4);
    if (w) { char e2[200]; if (llmwsp_writer_close(w, e2, sizeof e2) < 0 && rc == 0) rc = -1; }
    for (int i = 0; i < n; i++) free(t[i].buf);
    free(t); free(sp);
    if (rc < 0) return -1;
    snprintf(p, sizeof p, "%s/config.json", outdir);
    FILE *f = fopen(p, "wb");
    if (!f) return -1;
    fprintf(f, "{\"model_type\":\"qwen3\",\"hidden_size\":%d,\"num_attention_heads\":%d,\"num_key_value_heads\":%d,\"head_dim\":%d,"
               "\"intermediate_size\":%d,\"num_hidden_layers\":%d,\"vocab_size\":%d,\"tie_word_embeddings\":true}", H, NH, NH, HD, I, L, VOC);
    fclose(f);
    if (truth) *truth = tr;
    return 0;
}
