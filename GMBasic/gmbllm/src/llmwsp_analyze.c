/* llmwsp_analyze.c — analizzatori senza forward (W4). C99. */
#include "llmwsp_analyze.h"
#include "llmwsp_linalg.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float bf16f(uint16_t v) { uint32_t b = (uint32_t)v << 16; float f; memcpy(&f, &b, 4); return f; }
static float f16f(uint16_t h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023, o;
    if (e == 0) { if (m == 0) o = s << 31; else { e = 1; while (!(m & 1024)) { m <<= 1; e--; } m &= 1023; o = (s << 31) | ((e + 112) << 23) | (m << 13); } }
    else if (e == 31) o = (s << 31) | 0x7f800000u | (m << 13);
    else o = (s << 31) | ((e + 112) << 23) | (m << 13);
    float f; memcpy(&f, &o, 4); return f;
}

int llmwsp_load_f64(const llmwsp_store *s, const llmwsp_tensor *t, int axis, int64_t start, int64_t end, double *out, size_t cap) {
    size_t es = llmwsp_dtype_size(t->dtype);
    int kind = !strcmp(t->dtype, "F32") ? 0 : !strcmp(t->dtype, "BF16") ? 1 : !strcmp(t->dtype, "F16") ? 2 : -1;
    if (kind < 0 || t->ndim < 1 || t->ndim > 2 || (axis == 1 && t->ndim != 2) || axis < 0 || axis > 1) return -1;
    if (start < 0 || end < start || end > t->shape[axis]) return -1;
    int64_t rows = axis == 0 ? end - start : t->shape[0];
    int64_t cols = t->ndim == 2 ? (axis == 1 ? end - start : t->shape[1]) : 1;
    size_t n = (size_t)(rows * cols);
    if (n > cap) return -1;
    unsigned char *raw = (unsigned char *)malloc(n * es);
    if (!raw) return -1;
    int64_t got = axis == 0 ? llmwsp_read_rows(s, t, start, end, raw, n * es) : llmwsp_read_cols(s, t, start, end, raw, n * es);
    if (got != (int64_t)(n * es)) { free(raw); return -1; }
    for (size_t i = 0; i < n; i++) {
        if (kind == 0) { float f; memcpy(&f, raw + 4 * i, 4); out[i] = f; }
        else { uint16_t v; memcpy(&v, raw + 2 * i, 2); out[i] = kind == 1 ? bf16f(v) : f16f(v); }
    }
    free(raw);
    return 0;
}

/* Carica la slice indicata da un indirizzo del documento. out: n righe attese dal chiamante. */
static double *load_addr(const llmwsp_store *s, const llmwsp_doc *d, const char *addr, size_t *rows, size_t *cols) {
    const llmwsp_tensor *t; int axis; int64_t a, b;
    if (llmwsp_doc_resolve(d, addr, &t, &axis, &a, &b) < 0 || t->ndim != 2) return NULL;
    size_t r = (size_t)(axis == 0 ? b - a : t->shape[0]), c = (size_t)(axis == 1 ? b - a : t->shape[1]);
    double *m = (double *)malloc(r * c * sizeof(double));
    if (!m || llmwsp_load_f64(s, t, axis, a, b, m, r * c) < 0) { free(m); return NULL; }
    *rows = r; *cols = c;
    return m;
}

static double fro(const double *m, size_t n) { double a = 0; for (size_t i = 0; i < n; i++) a += m[i] * m[i]; return sqrt(a); }

static int geom(const llmwsp_doc *d, int layer, llmwsp_dims *g) {
    llmwsp_doc_dims(d, g);
    return layer >= 0 && layer < g->layers && g->kv_heads > 0 && g->heads % g->kv_heads == 0 ? 0 : -1;
}

int llmwsp_an_head_norms(const llmwsp_store *s, const llmwsp_doc *d, int layer, double *q, double *k, double *v, double *o) {
    llmwsp_dims g;
    if (geom(d, layer, &g) < 0) return -1;
    struct { double *out; const char *role, *part; int64_t cnt; } J[4] = {
        {q, "q", "heads", g.heads}, {k, "k", "kv_heads", g.kv_heads}, {v, "v", "kv_heads", g.kv_heads}, {o, "o", "heads", g.heads}};
    for (int j = 0; j < 4; j++) {
        if (!J[j].out) continue;
        for (int64_t i = 0; i < J[j].cnt; i++) {
            char a[160]; size_t r, c;
            snprintf(a, sizeof a, "layers.%d.attn.%s.%s.%lld", layer, J[j].role, J[j].part, (long long)i);
            double *m = load_addr(s, d, a, &r, &c);
            if (!m) return -1;
            J[j].out[i] = fro(m, r * c); free(m);
        }
    }
    return 0;
}

int llmwsp_an_circuit_spectrum(const llmwsp_store *s, const llmwsp_doc *d, int layer, int head, llmwsp_circuit c, double *wr, double *wi) {
    llmwsp_dims g;
    if (geom(d, layer, &g) < 0 || head < 0 || head >= g.heads) return -1;
    int kv = head / (int)(g.heads / g.kv_heads);
    size_t hd = (size_t)g.head_dim, H = (size_t)g.hidden, r1, c1, r2, c2;
    char a1[160], a2[160];
    double *X = NULL, *Y = NULL, *M = NULL, *Yt = NULL;
    int rc = -1;
    if (c == LLMWSP_CIRCUIT_OV) {
        snprintf(a1, sizeof a1, "layers.%d.attn.v.kv_heads.%d", layer, kv);   /* V_g: [hd×H] */
        snprintf(a2, sizeof a2, "layers.%d.attn.o.heads.%d", layer, head);    /* O_h: [H×hd] */
        X = load_addr(s, d, a1, &r1, &c1); Y = load_addr(s, d, a2, &r2, &c2);
        if (!X || !Y || r1 != hd || c1 != H || r2 != H || c2 != hd) goto out;
        M = (double *)malloc(hd * hd * sizeof(double));
        if (!M) goto out;
        llmwsp_la_matmul(X, Y, M, hd, H, hd);                                 /* V_g · O_h */
    } else {
        snprintf(a1, sizeof a1, "layers.%d.attn.k.kv_heads.%d", layer, kv);   /* K_g: [hd×H] */
        snprintf(a2, sizeof a2, "layers.%d.attn.q.heads.%d", layer, head);    /* Q_h: [hd×H] */
        X = load_addr(s, d, a1, &r1, &c1); Y = load_addr(s, d, a2, &r2, &c2);
        if (!X || !Y || r1 != hd || c1 != H || r2 != hd || c2 != H) goto out;
        Yt = (double *)malloc(H * hd * sizeof(double)); M = (double *)malloc(hd * hd * sizeof(double));
        if (!Yt || !M) goto out;
        for (size_t i = 0; i < hd; i++) for (size_t j = 0; j < H; j++) Yt[j * hd + i] = Y[i * H + j];
        llmwsp_la_matmul(X, Yt, M, hd, H, hd);                                /* K_g · Q_hᵀ */
    }
    rc = llmwsp_la_eig(M, hd, wr, wi);
out:
    free(X); free(Y); free(M); free(Yt);
    return rc;
}

void llmwsp_an_copy_scores(const double *wr, const double *wi, size_t n, double *sum_ratio, double *frac_pos) {
    double sr = 0, sa = 0, mx = 0; size_t pos = 0;
    for (size_t i = 0; i < n; i++) { double a = hypot(wr[i], wi[i]); sr += wr[i]; sa += a; if (a > mx) mx = a; }
    double thr = mx * 1e-9;
    size_t nz = 0;
    for (size_t i = 0; i < n; i++) if (hypot(wr[i], wi[i]) > thr) { nz++; if (wr[i] > 0) pos++; }
    if (mx == 0 || sa == 0) { *sum_ratio = 0; *frac_pos = 0; return; }
    *sum_ratio = sr / sa;
    *frac_pos = nz ? (double)pos / (double)n : 0;   /* denominatore n: gli autovalori nulli contano come non positivi */
}

int llmwsp_an_compose_cosines(const llmwsp_store *s, const llmwsp_doc *d, int l1, int h1, int l2, int h2, char which, double *cosv, double *pk) {
    llmwsp_dims g;
    if (geom(d, l1, &g) < 0 || geom(d, l2, &g) < 0 || h1 < 0 || h2 < 0 || h1 >= g.heads || h2 >= g.heads) return -1;
    if (which != 'q' && which != 'k' && which != 'v') return -1;
    size_t hd = (size_t)g.head_dim, H = (size_t)g.hidden, r1, c1, r2, c2;
    char a1[160], a2[160];
    int grp = which == 'q' ? h2 : h2 / (int)(g.heads / g.kv_heads);
    snprintf(a1, sizeof a1, "layers.%d.attn.o.heads.%d", l1, h1);                                   /* O_h1: [H×hd] */
    snprintf(a2, sizeof a2, "layers.%d.attn.%c.%s.%d", l2, which, which == 'q' ? "heads" : "kv_heads", grp);   /* X: [hd×H] */
    double *A = load_addr(s, d, a1, &r1, &c1), *X = load_addr(s, d, a2, &r2, &c2), *Xt = NULL;
    int rc = -1;
    if (A && X && r1 == H && c1 == hd && r2 == hd && c2 == H && (Xt = (double *)malloc(H * hd * sizeof(double)))) {
        for (size_t i = 0; i < hd; i++) for (size_t j = 0; j < H; j++) Xt[j * hd + i] = X[i * H + j];
        rc = llmwsp_la_principal_cosines(A, hd, Xt, hd, H, cosv);
        if (rc == 0 && pk) { double a = 0; for (size_t i = 0; i < hd; i++) a += cosv[i] * cosv[i]; *pk = a; }
    }
    free(A); free(X); free(Xt);
    return rc;
}
