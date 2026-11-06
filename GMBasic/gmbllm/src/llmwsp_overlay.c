/* llmwsp_overlay.c — overlay copy-on-write, edit, journal, recipe/replay, patch (W6). C99. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_overlay.h"
#include "llmwsp_analyze.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------ conversioni dtype */
static float bf16_to_f(uint16_t v) { uint32_t b = (uint32_t)v << 16; float f; memcpy(&f, &b, 4); return f; }
static uint16_t f_to_bf16(float f) {
    uint32_t b; memcpy(&b, &f, 4);
    if ((b & 0x7fffffffu) > 0x7f800000u) return 0x7fc0;
    b += 0x7fffu + ((b >> 16) & 1);
    return (uint16_t)(b >> 16);
}
static float h_to_f(uint16_t h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023, o;
    if (e == 0) { if (m == 0) o = s << 31; else { e = 1; while (!(m & 1024)) { m <<= 1; e--; } m &= 1023; o = (s << 31) | ((e + 112) << 23) | (m << 13); } }
    else if (e == 31) o = (s << 31) | 0x7f800000u | (m << 13);
    else o = (s << 31) | ((e + 112) << 23) | (m << 13);
    float f; memcpy(&f, &o, 4); return f;
}
static uint16_t f_to_h(float f) {
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t sign = (x >> 16) & 0x8000u;
    if ((x & 0x7fffffffu) > 0x7f800000u) return (uint16_t)(sign | 0x7e00);
    int e = (int)((x >> 23) & 0xff) - 127 + 15;
    uint32_t m = x & 0x7fffffu;
    if (e >= 31) return (uint16_t)(sign | 0x7c00);
    if (e <= 0) {
        if (e < -10) return (uint16_t)sign;
        m |= 0x800000u;
        int sh = 14 - e;
        uint32_t half = m >> sh, rem = m & ((1u << sh) - 1), mid = 1u << (sh - 1);
        if (rem > mid || (rem == mid && (half & 1))) half++;
        return (uint16_t)(sign | half);
    }
    uint32_t half = sign | ((uint32_t)e << 10) | (m >> 13), rem = m & 0x1fffu;
    if (rem > 0x1000u || (rem == 0x1000u && (half & 1))) half++;
    return (uint16_t)half;
}
static int dkind(const char *d) { return !strcmp(d, "F32") ? 0 : !strcmp(d, "BF16") ? 1 : !strcmp(d, "F16") ? 2 : -1; }

int llmwsp_decode_f64(const char *dtype, const void *raw, size_t n, double *out) {
    int k = dkind(dtype);
    if (k < 0) return -1;
    for (size_t i = 0; i < n; i++) {
        if (k == 0) { float f; memcpy(&f, (const char *)raw + 4 * i, 4); out[i] = f; }
        else { uint16_t v; memcpy(&v, (const char *)raw + 2 * i, 2); out[i] = k == 1 ? bf16_to_f(v) : h_to_f(v); }
    }
    return 0;
}
int llmwsp_encode_f64(const char *dtype, const double *in, size_t n, void *raw) {
    int k = dkind(dtype);
    if (k < 0) return -1;
    for (size_t i = 0; i < n; i++) {
        if (k == 0) { float f = (float)in[i]; memcpy((char *)raw + 4 * i, &f, 4); }
        else { uint16_t v = k == 1 ? f_to_bf16((float)in[i]) : f_to_h((float)in[i]); memcpy((char *)raw + 2 * i, &v, 2); }
    }
    return 0;
}

/* ------------------------------------------------------------ strutture */
typedef enum { K_SCALE, K_ZERO, K_SET, K_ADD, K_COPY, K_PROJ } kind_t;

typedef struct {
    kind_t   kind;
    char    *target;          /* indirizzo del bersaglio */
    char    *src;             /* K_COPY */
    double   alpha;           /* K_SCALE */
    double  *blob; size_t blobn;   /* K_SET/K_ADD/K_PROJ (direzioni) */
    size_t   ndirs, len; int vec_axis, flags;   /* K_PROJ */
    size_t   ti;              /* indice del tensore */
    int      axis; int64_t a, b;
    unsigned char *before, *after; size_t nbytes;   /* immagini della slice (raw) */
} entry;

struct llmwsp_overlay {
    const llmwsp_store *s; const llmwsp_doc *d; char *srcdir;
    size_t nt;
    unsigned char **buf;      /* per tensore: copia modificabile o NULL */
    entry *j; size_t n, cap, pos;
    char **cpn; size_t *cpp; size_t ncp;       /* checkpoint nominati */
};

static void E(char *err, size_t ec, const char *m, const char *a) { if (err && ec) snprintf(err, ec, "%s%s%s", m, a ? ": " : "", a ? a : ""); }

llmwsp_overlay *llmwsp_ov_new(const llmwsp_store *s, const llmwsp_doc *d, const char *srcdir) {
    llmwsp_overlay *o = (llmwsp_overlay *)calloc(1, sizeof *o);
    if (!o) return NULL;
    o->s = s; o->d = d; o->srcdir = strdup(srcdir); o->nt = llmwsp_store_count(s);
    o->buf = (unsigned char **)calloc(o->nt ? o->nt : 1, sizeof(unsigned char *));
    if (!o->buf || !o->srcdir) { llmwsp_ov_free(o); return NULL; }
    return o;
}
static void entry_free(entry *e) { free(e->target); free(e->src); free(e->blob); free(e->before); free(e->after); }
void llmwsp_ov_free(llmwsp_overlay *o) {
    if (!o) return;
    for (size_t i = 0; i < o->n; i++) entry_free(&o->j[i]);
    free(o->j);
    for (size_t i = 0; i < o->nt; i++) free(o->buf ? o->buf[i] : NULL);
    for (size_t i = 0; i < o->ncp; i++) free(o->cpn[i]);
    free(o->cpn); free(o->cpp);
    free(o->buf); free(o->srcdir); free(o);
}

/* ------------------------------------------------------------ slice raw */
static void dims(const llmwsp_tensor *t, int64_t *R, int64_t *C) { *R = t->shape[0]; *C = t->ndim == 2 ? t->shape[1] : 1; }

static size_t rect_bytes(const llmwsp_tensor *t, int axis, int64_t a, int64_t b) {
    int64_t R, C; dims(t, &R, &C);
    size_t es = llmwsp_dtype_size(t->dtype);
    return (size_t)(axis == 0 ? (b - a) * C : R * (b - a)) * es;
}
static void rect_get(const unsigned char *T, const llmwsp_tensor *t, int axis, int64_t a, int64_t b, unsigned char *out) {
    int64_t R, C; dims(t, &R, &C); size_t es = llmwsp_dtype_size(t->dtype);
    if (axis == 0) memcpy(out, T + (size_t)a * (size_t)C * es, (size_t)(b - a) * (size_t)C * es);
    else for (int64_t r = 0; r < R; r++) memcpy(out + (size_t)r * (size_t)(b - a) * es, T + ((size_t)r * (size_t)C + (size_t)a) * es, (size_t)(b - a) * es);
}
static void rect_put(unsigned char *T, const llmwsp_tensor *t, int axis, int64_t a, int64_t b, const unsigned char *in) {
    int64_t R, C; dims(t, &R, &C); size_t es = llmwsp_dtype_size(t->dtype);
    if (axis == 0) memcpy(T + (size_t)a * (size_t)C * es, in, (size_t)(b - a) * (size_t)C * es);
    else for (int64_t r = 0; r < R; r++) memcpy(T + ((size_t)r * (size_t)C + (size_t)a) * es, in + (size_t)r * (size_t)(b - a) * es, (size_t)(b - a) * es);
}

static int resolve(const llmwsp_overlay *o, const char *addr, const llmwsp_tensor **t, int *axis, int64_t *a, int64_t *b) {
    if (addr[0] == '@') {
        int ax; long long x, y; int off = 0;
        if (sscanf(addr, "@%d:%lld:%lld:%n", &ax, &x, &y, &off) < 3 || off == 0) return -1;
        const llmwsp_tensor *tt = llmwsp_store_find(o->s, addr + off);
        if (!tt || tt->ndim < 1 || ax < 0 || ax > 1 || (ax == 1 && tt->ndim != 2) || x < 0 || y < x || y > tt->shape[ax]) return -1;
        *t = tt; *axis = ax; *a = x; *b = y;
        return 0;
    }
    return llmwsp_doc_resolve(o->d, addr, t, axis, a, b);
}

static int ensure(llmwsp_overlay *o, const llmwsp_tensor *t, size_t *ti) {
    *ti = (size_t)(t - llmwsp_store_at(o->s, 0));
    if (o->buf[*ti]) return 0;
    unsigned char *b = (unsigned char *)malloc(t->nbytes ? (size_t)t->nbytes : 1);
    if (!b || llmwsp_read_bytes(o->s, t, 0, t->nbytes, b) < 0) { free(b); return -1; }
    o->buf[*ti] = b;
    return 0;
}

/* ------------------------------------------------------------ commit di una slice */
static int commit(llmwsp_overlay *o, entry *proto, const llmwsp_tensor *t, int axis, int64_t a, int64_t b,
                  const double *X, const double *Y, size_t n, llmwsp_ov_report *rep, char *err, size_t ec) {
    size_t es = llmwsp_dtype_size(t->dtype), nb = n * es, ti;
    for (size_t i = 0; i < n; i++) if (!isfinite(Y[i])) { E(err, ec, "valore non finito nel risultato", t->name); entry_free(proto); return -1; }
    if (ensure(o, t, &ti) < 0) { E(err, ec, "lettura del tensore fallita", t->name); entry_free(proto); return -1; }
    unsigned char *before = (unsigned char *)malloc(nb ? nb : 1), *after = (unsigned char *)malloc(nb ? nb : 1);
    double *Yd = (double *)malloc((n ? n : 1) * sizeof(double));
    if (!before || !after || !Yd) { free(before); free(after); free(Yd); entry_free(proto); return -1; }
    rect_get(o->buf[ti], t, axis, a, b, before);
    llmwsp_encode_f64(t->dtype, Y, n, after);
    llmwsp_decode_f64(t->dtype, after, n, Yd);                  /* valori effettivamente rappresentati */
    int64_t touched = 0; double dn = 0, xn = 0, mx = 0;
    for (size_t i = 0; i < n; i++) {
        if (memcmp(before + i * es, after + i * es, es) != 0) touched++;
        double dd = Yd[i] - X[i]; dn += dd * dd; xn += X[i] * X[i]; if (fabs(dd) > mx) mx = fabs(dd);
    }
    free(Yd);
    if (rep) { rep->touched = touched; rep->total = (int64_t)n; rep->rel_norm = xn > 0 ? sqrt(dn / xn) : 0; rep->max_abs = mx; }
    rect_put(o->buf[ti], t, axis, a, b, after);
    /* tronca la coda di redo */
    for (size_t i = o->pos; i < o->n; i++) entry_free(&o->j[i]);
    o->n = o->pos;
    if (o->n == o->cap) { size_t nc = o->cap ? o->cap * 2 : 16; entry *nj = (entry *)realloc(o->j, nc * sizeof *nj); if (!nj) { free(before); free(after); entry_free(proto); return -1; } o->j = nj; o->cap = nc; }
    proto->ti = ti; proto->axis = axis; proto->a = a; proto->b = b; proto->before = before; proto->after = after; proto->nbytes = nb;
    o->j[o->n++] = *proto; o->pos = o->n;
    return 0;
}

/* carica la slice come double; restituisce buffer allocato */
static double *load(llmwsp_overlay *o, const llmwsp_tensor *t, int axis, int64_t a, int64_t b, size_t *n, char *err, size_t ec) {
    int64_t R, C; dims(t, &R, &C);
    size_t cnt = (size_t)(axis == 0 ? (b - a) * C : R * (b - a));
    if (dkind(t->dtype) < 0) { E(err, ec, "dtype non modificabile (quantizzato?)", t->dtype); return NULL; }
    size_t ti; double *X = (double *)malloc((cnt ? cnt : 1) * sizeof(double));
    unsigned char *raw = (unsigned char *)malloc(rect_bytes(t, axis, a, b) + 1);
    if (!X || !raw || ensure(o, t, &ti) < 0) { free(X); free(raw); E(err, ec, "memoria/lettura", t->name); return NULL; }
    rect_get(o->buf[ti], t, axis, a, b, raw);
    llmwsp_decode_f64(t->dtype, raw, cnt, X);
    free(raw); *n = cnt;
    return X;
}

static entry mk(kind_t k, const char *target) { entry e; memset(&e, 0, sizeof e); e.kind = k; e.target = strdup(target); return e; }

#define PREP(addr) const llmwsp_tensor *t; int axis; int64_t a, b; \
    if (resolve(o, addr, &t, &axis, &a, &b) < 0) { E(err, ec, "indirizzo non risolvibile", addr); return -1; } \
    size_t n; double *X = load(o, t, axis, a, b, &n, err, ec); if (!X) return -1; \
    double *Y = (double *)malloc((n ? n : 1) * sizeof(double)); if (!Y) { free(X); return -1; }

int llmwsp_ov_scale(llmwsp_overlay *o, const char *addr, double alpha, llmwsp_ov_report *r, char *err, size_t ec) {
    PREP(addr);
    for (size_t i = 0; i < n; i++) Y[i] = X[i] * alpha;
    entry e = mk(K_SCALE, addr); e.alpha = alpha;
    int rc = commit(o, &e, t, axis, a, b, X, Y, n, r, err, ec); free(X); free(Y); return rc;
}
int llmwsp_ov_zero(llmwsp_overlay *o, const char *addr, llmwsp_ov_report *r, char *err, size_t ec) {
    PREP(addr);
    for (size_t i = 0; i < n; i++) Y[i] = 0;
    entry e = mk(K_ZERO, addr);
    int rc = commit(o, &e, t, axis, a, b, X, Y, n, r, err, ec); free(X); free(Y); return rc;
}
static int setadd(llmwsp_overlay *o, kind_t k, const char *addr, const double *v, size_t cnt, llmwsp_ov_report *r, char *err, size_t ec) {
    PREP(addr);
    if (cnt != n) { E(err, ec, "dimensione dei valori diversa dalla slice", addr); free(X); free(Y); return -1; }
    for (size_t i = 0; i < n; i++) Y[i] = k == K_SET ? v[i] : X[i] + v[i];
    entry e = mk(k, addr); e.blob = (double *)malloc((n ? n : 1) * sizeof(double)); e.blobn = n;
    if (e.blob) memcpy(e.blob, v, n * sizeof(double));
    int rc = commit(o, &e, t, axis, a, b, X, Y, n, r, err, ec); free(X); free(Y); return rc;
}
int llmwsp_ov_set(llmwsp_overlay *o, const char *addr, const double *v, size_t n, llmwsp_ov_report *r, char *err, size_t ec) { return setadd(o, K_SET, addr, v, n, r, err, ec); }
int llmwsp_ov_adddelta(llmwsp_overlay *o, const char *addr, const double *v, size_t n, llmwsp_ov_report *r, char *err, size_t ec) { return setadd(o, K_ADD, addr, v, n, r, err, ec); }

int llmwsp_ov_copy(llmwsp_overlay *o, const char *dst, const char *src, llmwsp_ov_report *r, char *err, size_t ec) {
    const llmwsp_tensor *ts; int ax; int64_t sa, sb;
    if (resolve(o, src, &ts, &ax, &sa, &sb) < 0) { E(err, ec, "sorgente non risolvibile", src); return -1; }
    size_t m; double *S = load(o, ts, ax, sa, sb, &m, err, ec);
    if (!S) return -1;
    PREP(dst);
    if (m != n) { E(err, ec, "slice di forma diversa", dst); free(S); free(X); free(Y); return -1; }
    memcpy(Y, S, n * sizeof(double)); free(S);
    entry e = mk(K_COPY, dst); e.src = strdup(src);
    int rc = commit(o, &e, t, axis, a, b, X, Y, n, r, err, ec); free(X); free(Y); return rc;
}

int llmwsp_ov_projectout(llmwsp_overlay *o, const char *addr, const double *dirs, size_t ndirs, size_t len, int vec_axis, int flags,
                         llmwsp_ov_report *r, char *err, size_t ec) {
    PREP(addr);
    int64_t R, C; dims(t, &R, &C);
    int64_t rr = axis == 0 ? b - a : R, cc = t->ndim == 2 ? (axis == 1 ? b - a : C) : 1;
    int ok = (vec_axis == 0 || vec_axis == 1) && ndirs > 0 && len == (size_t)(vec_axis == 0 ? rr : cc) && !(vec_axis == 1 && t->ndim != 2);
    if (!ok) { E(err, ec, "direzioni incompatibili con la slice (lunghezza/asse)", addr); free(X); free(Y); return -1; }
    /* base ortonormale (Gram-Schmidt modificato, due passate) */
    size_t k = 0; double *U = (double *)calloc(ndirs * len, sizeof(double));
    double *src = (double *)malloc(ndirs * len * sizeof(double));
    if (!U || !src) { free(U); free(src); free(X); free(Y); return -1; }
    memcpy(src, dirs, ndirs * len * sizeof(double));
    size_t nd = ndirs;
    if (flags & LLMWSP_PROJ_HARMLESS) {
        if (ndirs != 2) { E(err, ec, "HARMLESS richiede esattamente 2 direzioni", addr); free(U); free(src); free(X); free(Y); return -1; }
        double hh = 0, dh = 0; for (size_t i = 0; i < len; i++) { hh += src[len + i] * src[len + i]; dh += src[i] * src[len + i]; }
        if (hh <= 0) { E(err, ec, "direzione innocua nulla", addr); free(U); free(src); free(X); free(Y); return -1; }
        for (size_t i = 0; i < len; i++) src[i] -= dh / hh * src[len + i];
        nd = 1;
    }
    for (size_t q = 0; q < nd; q++) {
        double *u = U + k * len; memcpy(u, src + q * len, len * sizeof(double));
        for (int rep = 0; rep < 2; rep++)
            for (size_t p = 0; p < k; p++) { double dp = 0; for (size_t i = 0; i < len; i++) dp += u[i] * U[p * len + i]; for (size_t i = 0; i < len; i++) u[i] -= dp * U[p * len + i]; }
        double nn = 0; for (size_t i = 0; i < len; i++) nn += u[i] * u[i];
        if (nn < 1e-24) { E(err, ec, "direzione nulla o linearmente dipendente", addr); free(U); free(src); free(X); free(Y); return -1; }
        nn = sqrt(nn); for (size_t i = 0; i < len; i++) u[i] /= nn;
        k++;
    }
    free(src);
    memcpy(Y, X, n * sizeof(double));
    size_t nv = vec_axis == 0 ? (size_t)cc : (size_t)rr;            /* numero di vettori */
    for (size_t v = 0; v < nv; v++) {
        #define EL(m, i) ((m)[vec_axis == 0 ? (i) * (size_t)cc + v : v * (size_t)cc + (i)])
        double n0 = 0; for (size_t i = 0; i < len; i++) n0 += EL(Y, i) * EL(Y, i);
        for (size_t p = 0; p < k; p++) { double dp = 0; for (size_t i = 0; i < len; i++) dp += EL(Y, i) * U[p * len + i]; for (size_t i = 0; i < len; i++) EL(Y, i) -= dp * U[p * len + i]; }
        if (flags & LLMWSP_PROJ_NORMPRESERVING) {
            double n1 = 0; for (size_t i = 0; i < len; i++) n1 += EL(Y, i) * EL(Y, i);
            if (n1 > 1e-300 && n0 > 0) { double sc = sqrt(n0 / n1); for (size_t i = 0; i < len; i++) EL(Y, i) *= sc; }
        }
        #undef EL
    }
    entry e = mk(K_PROJ, addr); e.blob = (double *)malloc(ndirs * len * sizeof(double)); e.blobn = ndirs * len;
    if (e.blob) memcpy(e.blob, dirs, ndirs * len * sizeof(double));
    e.ndirs = ndirs; e.len = len; e.vec_axis = vec_axis; e.flags = flags;
    free(U);
    int rc = commit(o, &e, t, axis, a, b, X, Y, n, r, err, ec); free(X); free(Y); return rc;
}

/* ------------------------------------------------------------ journal */
size_t llmwsp_ov_journal_len(const llmwsp_overlay *o) { return o->n; }
size_t llmwsp_ov_journal_pos(const llmwsp_overlay *o) { return o->pos; }
int llmwsp_ov_undo(llmwsp_overlay *o) {
    if (o->pos == 0) return -1;
    entry *e = &o->j[--o->pos];
    rect_put(o->buf[e->ti], llmwsp_store_at(o->s, e->ti), e->axis, e->a, e->b, e->before);
    return 0;
}
int llmwsp_ov_redo(llmwsp_overlay *o) {
    if (o->pos >= o->n) return -1;
    entry *e = &o->j[o->pos++];
    rect_put(o->buf[e->ti], llmwsp_store_at(o->s, e->ti), e->axis, e->a, e->b, e->after);
    return 0;
}

int llmwsp_ov_tensor_sha256(const llmwsp_overlay *o, const char *name, char hex[65]) {
    const llmwsp_tensor *t = llmwsp_store_find(o->s, name);
    if (!t) return -1;
    size_t ti = (size_t)(t - llmwsp_store_at(o->s, 0));
    if (!o->buf[ti]) return llmwsp_tensor_sha256(o->s, t, hex);
    llmwsp_sha256_buf(o->buf[ti], (size_t)t->nbytes, hex);
    return 0;
}

/* confronto con la base: riempie changed/bbox/norma */
static int cmp_base(const llmwsp_overlay *o, size_t ti, llmwsp_ov_diffent *d) {
    const llmwsp_tensor *t = llmwsp_store_at(o->s, ti);
    size_t es = llmwsp_dtype_size(t->dtype);
    unsigned char *orig = (unsigned char *)malloc(t->nbytes ? (size_t)t->nbytes : 1);
    if (!orig || llmwsp_read_bytes(o->s, t, 0, t->nbytes, orig) < 0) { free(orig); return -1; }
    int64_t R, C; dims(t, &R, &C);
    d->tensor = t->name; d->changed = 0; d->total = R * C; d->r0 = R; d->r1 = 0; d->c0 = C; d->c1 = 0; d->rel_norm = 0;
    double dn = 0, xn = 0;
    int dec = dkind(t->dtype) >= 0;
    for (int64_t r = 0; r < R; r++) for (int64_t c = 0; c < C; c++) {
        size_t i = (size_t)(r * C + c);
        if (memcmp(orig + i * es, o->buf[ti] + i * es, es) != 0) {
            d->changed++;
            if (r < d->r0) d->r0 = r;
            if (r + 1 > d->r1) d->r1 = r + 1;
            if (c < d->c0) d->c0 = c;
            if (c + 1 > d->c1) d->c1 = c + 1;
        }
        if (dec) { double x, y; llmwsp_decode_f64(t->dtype, orig + i * es, 1, &x); llmwsp_decode_f64(t->dtype, o->buf[ti] + i * es, 1, &y); dn += (y - x) * (y - x); xn += x * x; }
    }
    d->rel_norm = xn > 0 ? sqrt(dn / xn) : 0;
    free(orig);
    return 0;
}

int llmwsp_ov_diff(const llmwsp_overlay *o, llmwsp_ov_diffent *out, size_t cap, size_t *n) {
    size_t c = 0;
    for (size_t ti = 0; ti < o->nt; ti++) {
        if (!o->buf[ti]) continue;
        llmwsp_ov_diffent d;
        if (cmp_base(o, ti, &d) < 0) return -1;
        if (d.changed == 0) continue;
        if (c < cap) out[c] = d;
        c++;
    }
    *n = c;
    return 0;
}

int llmwsp_ov_checkpoint(llmwsp_overlay *o, const char *name) {
    for (size_t i = 0; i < o->ncp; i++) if (!strcmp(o->cpn[i], name)) { o->cpp[i] = o->pos; return 0; }
    char **nn = (char **)realloc(o->cpn, (o->ncp + 1) * sizeof(char *)); if (!nn) return -1; o->cpn = nn;
    size_t *np = (size_t *)realloc(o->cpp, (o->ncp + 1) * sizeof(size_t)); if (!np) return -1; o->cpp = np;
    o->cpn[o->ncp] = strdup(name); o->cpp[o->ncp] = o->pos; o->ncp++;
    return 0;
}
int llmwsp_ov_rollback(llmwsp_overlay *o, const char *name) {
    for (size_t i = 0; i < o->ncp; i++) if (!strcmp(o->cpn[i], name)) {
        if (o->cpp[i] > o->pos) return -1;
        while (o->pos > o->cpp[i]) llmwsp_ov_undo(o);
        return 0;
    }
    return -1;
}

char *llmwsp_ov_summary(const llmwsp_overlay *o) {
    size_t cap = 4096, n = 0; char *b = (char *)malloc(cap);
    if (!b) return NULL;
    n += (size_t)snprintf(b + n, cap - n, "WSP: riepilogo delle differenze rispetto alla base (edit applicate: %zu)\n", o->pos);
    size_t cnt = 0; double tot_changed = 0, tot_elems = 0;
    for (size_t ti = 0; ti < o->nt; ti++) {
        if (!o->buf[ti]) continue;
        llmwsp_ov_diffent d;
        if (cmp_base(o, ti, &d) < 0 || d.changed == 0) continue;
        if (n + 600 > cap) { cap *= 2; b = (char *)realloc(b, cap); if (!b) return NULL; }
        n += (size_t)snprintf(b + n, cap - n, "  %s: %lld/%lld pesi cambiati (%.4f%%), variazione relativa di norma %.5f\n", d.tensor, (long long)d.changed, (long long)d.total, 100.0 * (double)d.changed / (double)d.total, d.rel_norm);
        cnt++; tot_changed += (double)d.changed; tot_elems += (double)d.total;
    }
    if (n + 300 > cap) { cap += 512; b = (char *)realloc(b, cap); if (!b) return NULL; }
    snprintf(b + n, cap - n, "TOTALE: %zu tensori modificati, %.0f pesi cambiati (%.5f%% dei pesi dei soli tensori toccati)\n", cnt, tot_changed, tot_elems > 0 ? 100.0 * tot_changed / tot_elems : 0.0);
    return b;
}

int llmwsp_ov_materialize(const llmwsp_overlay *o, const char *outdir, llmwsp_mat_stats *st, char *err, size_t ec) {
    llmwsp_edit *ed = (llmwsp_edit *)calloc(o->nt ? o->nt : 1, sizeof *ed);
    size_t ne = 0;
    if (!ed) return -1;
    for (size_t ti = 0; ti < o->nt; ti++) {
        if (!o->buf[ti]) continue;
        llmwsp_ov_diffent d;
        if (cmp_base(o, ti, &d) < 0) { free(ed); return -1; }
        if (d.changed == 0) continue;
        const llmwsp_tensor *t = llmwsp_store_at(o->s, ti);
        ed[ne].name = t->name; ed[ne].data = o->buf[ti]; ed[ne].nbytes = t->nbytes; ne++;
    }
    int rc = llmwsp_materialize(o->s, o->srcdir, ed, ne, outdir, st, err, ec);
    free(ed);
    if (rc == 0) {                                   /* nessuna scrittura silenziosa: il riepilogo accompagna il checkpoint */
        char *sm = llmwsp_ov_summary(o); char p[1100]; snprintf(p, sizeof p, "%s/wsp_summary.txt", outdir);
        FILE *f = sm ? fopen(p, "wb") : NULL;
        if (f) { fputs(sm, f); fclose(f); }
        free(sm);
    }
    return rc;
}

/* ------------------------------------------------------------ recipe / replay */
static const char *KN[] = {"SCALE", "ZERO", "SET", "ADDDELTA", "COPY", "PROJECTOUT"};

int llmwsp_ov_recipe(const llmwsp_overlay *o, const char *dir, char *err, size_t ec) {
    char p[1100];
    mkdir(dir, 0755);
    snprintf(p, sizeof p, "%s/recipe.gmbderive", dir);
    FILE *f = fopen(p, "wb");
    if (!f) { E(err, ec, "recipe non scrivibile", p); return -1; }
    fprintf(f, "# gmbderive esteso — overlay WSP (%zu edit)\nVERSION 1\n", o->pos);
    int nb = 0;
    for (size_t i = 0; i < o->pos; i++) {
        const entry *e = &o->j[i];
        if (e->kind == K_SCALE) fprintf(f, "SCALE %s %a\n", e->target, e->alpha);
        else if (e->kind == K_ZERO) fprintf(f, "ZERO %s\n", e->target);
        else if (e->kind == K_COPY) fprintf(f, "COPY %s %s\n", e->target, e->src);
        else {
            snprintf(p, sizeof p, "%s/blob_%d.f64", dir, nb);
            FILE *b = fopen(p, "wb");
            if (!b || fwrite(e->blob, sizeof(double), e->blobn, b) != e->blobn) { if (b) fclose(b); fclose(f); E(err, ec, "blob non scrivibile", p); return -1; }
            fclose(b);
            if (e->kind == K_PROJ) fprintf(f, "PROJECTOUT %s %d %d %zu %zu blob_%d.f64\n", e->target, e->vec_axis, e->flags, e->ndirs, e->len, nb);
            else fprintf(f, "%s %s %zu blob_%d.f64\n", KN[e->kind], e->target, e->blobn, nb);
            nb++;
        }
    }
    fclose(f);
    return 0;
}

static double *read_blob(const char *dir, const char *name, size_t n) {
    char p[1100]; snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    double *v = (double *)malloc((n ? n : 1) * sizeof(double));
    if (!v || fread(v, sizeof(double), n, f) != n) { free(v); fclose(f); return NULL; }
    fclose(f); return v;
}

int llmwsp_ov_replay(llmwsp_overlay *o, const char *dir, char *err, size_t ec) {
    char p[1100], line[2048];
    snprintf(p, sizeof p, "%s/recipe.gmbderive", dir);
    FILE *f = fopen(p, "rb");
    if (!f) { E(err, ec, "recipe non leggibile", p); return -1; }
    int ln = 0, rc = 0;
    while (rc == 0 && fgets(line, sizeof line, f)) {
        ln++;
        char verb[32], a1[900], a2[900], bn[128]; double al; size_t cnt, nd, len; int va, fl;
        if (line[0] == '#' || line[0] == '\n' || !strncmp(line, "VERSION", 7)) continue;
        if (sscanf(line, "SCALE %899s %la", a1, &al) == 2) rc = llmwsp_ov_scale(o, a1, al, NULL, err, ec);
        else if (sscanf(line, "ZERO %899s", a1) == 1 && !strncmp(line, "ZERO", 4)) rc = llmwsp_ov_zero(o, a1, NULL, err, ec);
        else if (sscanf(line, "COPY %899s %899s", a1, a2) == 2 && !strncmp(line, "COPY", 4)) rc = llmwsp_ov_copy(o, a1, a2, NULL, err, ec);
        else if (sscanf(line, "PROJECTOUT %899s %d %d %zu %zu %127s", a1, &va, &fl, &nd, &len, bn) == 6) {
            double *v = read_blob(dir, bn, nd * len);
            if (!v) { E(err, ec, "blob mancante", bn); rc = -1; }
            else { rc = llmwsp_ov_projectout(o, a1, v, nd, len, va, fl, NULL, err, ec); free(v); }
        } else if (sscanf(line, "%31s %899s %zu %127s", verb, a1, &cnt, bn) == 4 && (!strcmp(verb, "SET") || !strcmp(verb, "ADDDELTA"))) {
            double *v = read_blob(dir, bn, cnt);
            if (!v) { E(err, ec, "blob mancante", bn); rc = -1; }
            else { rc = !strcmp(verb, "SET") ? llmwsp_ov_set(o, a1, v, cnt, NULL, err, ec) : llmwsp_ov_adddelta(o, a1, v, cnt, NULL, err, ec); free(v); }
        } else { E(err, ec, "riga di recipe non riconosciuta", line); rc = -1; }
    }
    (void)ln;
    fclose(f);
    return rc;
}

/* ------------------------------------------------------------ patch sparsa a slice */
static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void w64(FILE *f, int64_t v) { fwrite(&v, 8, 1, f); }
static int r32(FILE *f, uint32_t *v) { return fread(v, 4, 1, f) == 1 ? 0 : -1; }
static int r64(FILE *f, int64_t *v) { return fread(v, 8, 1, f) == 1 ? 0 : -1; }

int llmwsp_ov_export_patch(const llmwsp_overlay *o, const char *path, size_t *bytes, char *err, size_t ec) {
    FILE *f = fopen(path, "wb");
    if (!f) { E(err, ec, "patch non scrivibile", path); return -1; }
    fwrite("WSPPTCH1", 1, 8, f);
    size_t cntpos = (size_t)ftell(f); w32(f, 0);
    uint32_t cnt = 0;
    for (size_t ti = 0; ti < o->nt; ti++) {
        if (!o->buf[ti]) continue;
        llmwsp_ov_diffent d;
        if (cmp_base(o, ti, &d) < 0) { fclose(f); return -1; }
        if (d.changed == 0) continue;
        const llmwsp_tensor *t = llmwsp_store_at(o->s, ti);
        int axis = (d.r1 - d.r0) <= (d.c1 - d.c0) || t->ndim == 1 ? 0 : 1;
        int64_t a = axis == 0 ? d.r0 : d.c0, b = axis == 0 ? d.r1 : d.c1;
        size_t nb = rect_bytes(t, axis, a, b);
        unsigned char *raw = (unsigned char *)malloc(nb + 1);
        if (!raw) { fclose(f); return -1; }
        rect_get(o->buf[ti], t, axis, a, b, raw);
        w32(f, (uint32_t)strlen(t->name)); fwrite(t->name, 1, strlen(t->name), f);
        w32(f, (uint32_t)axis); w64(f, a); w64(f, b); w64(f, (int64_t)nb);
        fwrite(raw, 1, nb, f); free(raw); cnt++;
    }
    long end = ftell(f);
    fseek(f, (long)cntpos, SEEK_SET); w32(f, cnt);
    fclose(f);
    if (bytes) *bytes = (size_t)end;
    return 0;
}

int llmwsp_ov_apply_patch(llmwsp_overlay *o, const char *path, char *err, size_t ec) {
    FILE *f = fopen(path, "rb");
    if (!f) { E(err, ec, "patch non leggibile", path); return -1; }
    char mg[8]; uint32_t cnt;
    if (fread(mg, 1, 8, f) != 8 || memcmp(mg, "WSPPTCH1", 8) || r32(f, &cnt) < 0) { fclose(f); E(err, ec, "patch non valida", path); return -1; }
    int rc = 0;
    for (uint32_t i = 0; i < cnt && rc == 0; i++) {
        uint32_t nl, ax; int64_t a, b, nb; char name[600];
        if (r32(f, &nl) < 0 || nl >= 500 || fread(name, 1, nl, f) != nl) { rc = -1; break; }
        name[nl] = 0;
        if (r32(f, &ax) < 0 || r64(f, &a) < 0 || r64(f, &b) < 0 || r64(f, &nb) < 0) { rc = -1; break; }
        const llmwsp_tensor *t = llmwsp_store_find(o->s, name);
        if (!t || nb < 0 || (size_t)nb != rect_bytes(t, (int)ax, a, b)) { E(err, ec, "voce di patch incompatibile con la base", name); rc = -1; break; }
        unsigned char *raw = (unsigned char *)malloc((size_t)nb + 1);
        size_t cntel = (size_t)nb / llmwsp_dtype_size(t->dtype);
        double *v = (double *)malloc((cntel ? cntel : 1) * sizeof(double));
        if (!raw || !v || fread(raw, 1, (size_t)nb, f) != (size_t)nb || llmwsp_decode_f64(t->dtype, raw, cntel, v) < 0) { free(raw); free(v); rc = -1; break; }
        char tg[800]; snprintf(tg, sizeof tg, "@%u:%lld:%lld:%s", ax, (long long)a, (long long)b, name);
        rc = llmwsp_ov_set(o, tg, v, cntel, NULL, err, ec);
        free(raw); free(v);
    }
    fclose(f);
    if (rc < 0 && err && ec && !err[0]) E(err, ec, "patch corrotta", path);
    return rc;
}
