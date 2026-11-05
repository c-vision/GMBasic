/* gm_tensor.c — tensore N-dimensionale strided (da libtensr). */
#include "gm_tensor.h"

#include <stdlib.h>
#include <string.h>

static int shape_size(int ndim, const int *shape)
{
    int i;
    long n = 1;
    for (i = 0; i < ndim; i++) {
        if (shape[i] <= 0) return 0;
        n *= shape[i];
    }
    return (int)n;
}

static void rowmajor_strides(int ndim, const int *shape, ptrdiff_t *strides)
{
    int i;
    ptrdiff_t s = 1;
    for (i = ndim - 1; i >= 0; i--) {
        strides[i] = s;
        s *= shape[i];
    }
}

static gm_tensor *alloc_meta(int ndim, const int *shape)
{
    gm_tensor *t;
    if (ndim <= 0 || !shape) return NULL;
    t = (gm_tensor *)calloc(1, sizeof(gm_tensor));
    if (!t) return NULL;
    t->ndim = ndim;
    t->shape = (int *)malloc((size_t)ndim * sizeof(int));
    t->strides = (ptrdiff_t *)malloc((size_t)ndim * sizeof(ptrdiff_t));
    if (!t->shape || !t->strides) { gm_tensor_free(t); return NULL; }
    memcpy(t->shape, shape, (size_t)ndim * sizeof(int));
    rowmajor_strides(ndim, shape, t->strides);
    return t;
}

gm_tensor *gm_tensor_create(int ndim, const int *shape)
{
    gm_tensor *t = alloc_meta(ndim, shape);
    int n;
    if (!t) return NULL;
    n = shape_size(ndim, shape);
    if (n <= 0) { gm_tensor_free(t); return NULL; }
    t->data = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!t->data) { gm_tensor_free(t); return NULL; }
    t->owns = 1;
    return t;
}

gm_tensor *gm_tensor_zeros(int ndim, const int *shape)
{
    gm_tensor *t = gm_tensor_create(ndim, shape);
    if (t) memset(t->data, 0, gm_tensor_size(t) * sizeof(gm_real));
    return t;
}

gm_tensor *gm_tensor_full(int ndim, const int *shape, gm_real value)
{
    gm_tensor *t = gm_tensor_create(ndim, shape);
    size_t i, n;
    if (!t) return NULL;
    n = gm_tensor_size(t);
    for (i = 0; i < n; i++) t->data[i] = value;
    return t;
}

void gm_tensor_free(gm_tensor *t)
{
    if (!t) return;
    if (t->owns) free(t->data);
    free(t->shape);
    free(t->strides);
    free(t);
}

size_t gm_tensor_size(const gm_tensor *t)
{
    int i;
    size_t n = 1;
    if (!t) return 0;
    for (i = 0; i < t->ndim; i++) n *= (size_t)t->shape[i];
    return n;
}

ptrdiff_t gm_tensor_offset(const gm_tensor *t, const int *idx)
{
    int i;
    ptrdiff_t off = 0;
    for (i = 0; i < t->ndim; i++) off += (ptrdiff_t)idx[i] * t->strides[i];
    return off;
}

gm_real gm_tensor_get(const gm_tensor *t, const int *idx)
{
    return t->data[gm_tensor_offset(t, idx)];
}

void gm_tensor_set(gm_tensor *t, const int *idx, gm_real v)
{
    t->data[gm_tensor_offset(t, idx)] = v;
}

void gm_tensor_fill(gm_tensor *t, gm_real v)
{
    int idx[GM_TENSOR_MAX_DIM];
    size_t i, n = gm_tensor_size(t);
    int d;
    if (t->ndim <= GM_TENSOR_MAX_DIM && t->owns) {
        /* contiguo: riempimento lineare */
        int contiguous = 1;
        ptrdiff_t s = 1;
        for (d = t->ndim - 1; d >= 0; d--) { if (t->strides[d] != s) { contiguous = 0; break; } s *= t->shape[d]; }
        if (contiguous) { for (i = 0; i < n; i++) t->data[i] = v; return; }
    }
    memset(idx, 0, sizeof(idx));
    for (i = 0; i < n; i++) {
        t->data[gm_tensor_offset(t, idx)] = v;
        for (d = t->ndim - 1; d >= 0; d--) {
            if (++idx[d] < t->shape[d]) break;
            idx[d] = 0;
        }
    }
}

gm_tensor *gm_tensor_view(const gm_tensor *src, int ndim, const int *shape,
                          const ptrdiff_t *strides, ptrdiff_t offset)
{
    gm_tensor *t;
    if (!src || ndim <= 0 || !shape || !strides) return NULL;
    t = alloc_meta(ndim, shape);
    if (!t) return NULL;
    memcpy(t->strides, strides, (size_t)ndim * sizeof(ptrdiff_t));
    t->data = src->data + offset;
    t->owns = 0;
    return t;
}

gm_tensor *gm_tensor_reshape(const gm_tensor *src, int ndim, const int *shape)
{
    gm_tensor *t;
    ptrdiff_t *strides;
    if (!src || ndim <= 0 || !shape) return NULL;
    if ((size_t)shape_size(ndim, shape) != gm_tensor_size(src)) return NULL;
    t = alloc_meta(ndim, shape);
    if (!t) return NULL;
    (void)strides;
    t->data = src->data;
    t->owns = 0;
    return t;
}

gm_tensor *gm_tensor_permute(const gm_tensor *src, const int *perm)
{
    gm_tensor *t;
    int i;
    if (!src || !perm) return NULL;
    t = alloc_meta(src->ndim, src->shape);
    if (!t) return NULL;
    for (i = 0; i < src->ndim; i++) t->shape[i] = src->shape[perm[i]];
    for (i = 0; i < src->ndim; i++) t->strides[i] = src->strides[perm[i]];
    t->data = src->data;
    t->owns = 0;
    return t;
}

/* forma broadcast di a e b in `out`; ritorna ndim o -1. */
static int bcast(int na, const int *sa, int nb, const int *sb, int *out)
{
    int nd = (na > nb) ? na : nb, i;
    for (i = 0; i < nd; i++) {
        int da = (i < nd - na) ? 1 : sa[i - (nd - na)];
        int db = (i < nd - nb) ? 1 : sb[i - (nd - nb)];
        if (da != db && da != 1 && db != 1) return -1;
        out[i] = (da > db) ? da : db;
    }
    return nd;
}

/* indice lineare in un tensore per l'indice broadcast idx (dim 1 -> 0). */
static ptrdiff_t bcast_offset(const gm_tensor *t, const int *idx, int nd)
{
    int i;
    ptrdiff_t off = 0;
    for (i = 0; i < t->ndim; i++) {
        int j = nd - t->ndim + i;
        int k = (t->shape[i] == 1) ? 0 : idx[j];
        off += (ptrdiff_t)k * t->strides[i];
    }
    return off;
}

static gm_status tensor_binop(gm_tensor *out, const gm_tensor *a, const gm_tensor *b, int op)
{
    int shape[GM_TENSOR_MAX_DIM], idx[GM_TENSOR_MAX_DIM];
    int nd, i, d;
    size_t n;
    if (!out || !a || !b) return GM_ERR_ARG;
    nd = bcast(a->ndim, a->shape, b->ndim, b->shape, shape);
    if (nd < 0 || nd != out->ndim) return GM_ERR_ARG;
    for (i = 0; i < nd; i++) if (out->shape[i] != shape[i]) return GM_ERR_ARG;
    n = gm_tensor_size(out);
    memset(idx, 0, sizeof(idx));
    for (i = 0; i < (int)n; i++) {
        gm_real x = a->data[bcast_offset(a, idx, nd)];
        gm_real y = b->data[bcast_offset(b, idx, nd)];
        out->data[gm_tensor_offset(out, idx)] = (op == 0) ? x + y : ((op == 1) ? x - y : x * y);
        for (d = nd - 1; d >= 0; d--) { if (++idx[d] < shape[d]) break; idx[d] = 0; }
    }
    return GM_OK;
}

gm_status gm_tensor_add(gm_tensor *out, const gm_tensor *a, const gm_tensor *b) { return tensor_binop(out, a, b, 0); }
gm_status gm_tensor_sub(gm_tensor *out, const gm_tensor *a, const gm_tensor *b) { return tensor_binop(out, a, b, 1); }
gm_status gm_tensor_mul(gm_tensor *out, const gm_tensor *a, const gm_tensor *b) { return tensor_binop(out, a, b, 2); }

gm_real gm_tensor_sum(const gm_tensor *t)
{
    int idx[GM_TENSOR_MAX_DIM];
    int i, d;
    size_t n;
    gm_real s = 0.0;
    if (!t) return 0.0;
    n = gm_tensor_size(t);
    memset(idx, 0, sizeof(idx));
    for (i = 0; i < (int)n; i++) {
        s += t->data[gm_tensor_offset(t, idx)];
        for (d = t->ndim - 1; d >= 0; d--) { if (++idx[d] < t->shape[d]) break; idx[d] = 0; }
    }
    return s;
}

gm_real gm_tensor_max(const gm_tensor *t)
{
    int idx[GM_TENSOR_MAX_DIM];
    int i, d;
    size_t n;
    gm_real m;
    if (!t || gm_tensor_size(t) == 0) return 0.0;
    n = gm_tensor_size(t);
    memset(idx, 0, sizeof(idx));
    m = t->data[gm_tensor_offset(t, idx)];
    for (i = 0; i < (int)n; i++) {
        gm_real v = t->data[gm_tensor_offset(t, idx)];
        if (v > m) m = v;
        for (d = t->ndim - 1; d >= 0; d--) { if (++idx[d] < t->shape[d]) break; idx[d] = 0; }
    }
    return m;
}

gm_real gm_tensor_min(const gm_tensor *t)
{
    int idx[GM_TENSOR_MAX_DIM];
    int i, d;
    size_t n;
    gm_real m;
    if (!t || gm_tensor_size(t) == 0) return 0.0;
    n = gm_tensor_size(t);
    memset(idx, 0, sizeof(idx));
    m = t->data[gm_tensor_offset(t, idx)];
    for (i = 0; i < (int)n; i++) {
        gm_real v = t->data[gm_tensor_offset(t, idx)];
        if (v < m) m = v;
        for (d = t->ndim - 1; d >= 0; d--) { if (++idx[d] < t->shape[d]) break; idx[d] = 0; }
    }
    return m;
}

gm_real gm_tensor_mean(const gm_tensor *t)
{
    size_t n = gm_tensor_size(t);
    return (n > 0) ? gm_tensor_sum(t) / (gm_real)n : 0.0;
}
