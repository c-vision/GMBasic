/*
 * conic_presolve.c — shim FFI del presolve LP (handle = stato, non configurazione).
 *
 * PRESOLVENEW riduce  min c'x, A x = b, x >= 0  con A in CSC e restituisce un
 * handle (>0). Il problema ridotto si legge con PRESOLVEDIMS + PRESOLVEREDUCED,
 * si risolve con un solver qualsiasi e si riporta nella spazio originale con
 * PRESOLVEPOSTSOLVE (o PRESOLVEPOSTSOLVEDIR per una direzione di Farkas).
 * PRESOLVEFREE libera l'handle.
 */
#include "presolve.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef long long i64;

#define NN_PRE_BASE 7400
#define NN_PRE_MAX  16

typedef struct { int used; Presolve *p; } pre_slot;
static pre_slot g_pre[NN_PRE_MAX];

static pre_slot *pre_get(i64 h)
{
    if (h <= NN_PRE_BASE || h > NN_PRE_BASE + NN_PRE_MAX) return NULL;
    if (!g_pre[h - NN_PRE_BASE - 1].used) return NULL;
    return &g_pre[h - NN_PRE_BASE - 1];
}

static int *cvt_i(const i64 *src, i64 n)
{
    int *d;
    i64 i;
    if (!src || n <= 0) return NULL;
    d = (int *)malloc((size_t)n * sizeof(int));
    if (!d) return NULL;
    for (i = 0; i < n; i++) d[i] = (int)src[i];
    return d;
}

i64 ofx_presolve_new(const i64 *Aptr, int64_t Aptr_n, const i64 *Arow, int64_t Arow_n, const double *Aval, int64_t Aval_n,
                     const double *b, int64_t b_n, const double *c, int64_t c_n, i64 m, i64 n, double tol)
{
    int *ap, *ar, i, rc;
    Presolve *p = NULL;
    if (!Aptr || !Arow || !Aval || !b || !c || m < 1 || n < 1 || Aptr[n] < 0) return -1;
    ap = cvt_i(Aptr, n + 1);
    ar = cvt_i(Arow, Aptr[n]);
    if (!ap || (Aptr[n] > 0 && !ar)) { free(ap); free(ar); return -1; }
    rc = lp_presolve(ap, ar, Aval, b, c, (int)m, (int)n, tol, &p);
    free(ap); free(ar);
    if (rc != 0 || !p) return -1;
    for (i = 0; i < NN_PRE_MAX; i++) {
        if (g_pre[i].used) continue;
        g_pre[i].used = 1;
        g_pre[i].p = p;
        return (i64)(NN_PRE_BASE + i + 1);
    }
    presolve_free(p);
    return -1;
}

i64 ofx_presolve_changed(i64 h)
{
    pre_slot *s = pre_get(h);
    if (!s) return -1;
    return (i64)presolve_changed(s->p);
}

/* out(0)=mr, out(1)=nr, out(2)=nnz(Ar) */
i64 ofx_presolve_dims(i64 h, i64 *out, int64_t out_n)
{
    const int *ap, *ar;
    const double *av, *br, *cr;
    int mr = 0, nr = 0;
    pre_slot *s = pre_get(h);
    if (!s || !out) return -1;
    presolve_reduced(s->p, &ap, &ar, &av, &br, &cr, &mr, &nr);
    out[0] = (i64)mr;
    out[1] = (i64)nr;
    out[2] = (i64)(nr > 0 && ap ? ap[nr] : 0);
    return 0;
}

i64 ofx_presolve_reduced(i64 h, i64 *Arptr, int64_t Arptr_n, i64 *Arrow, int64_t Arrow_n, double *Arval, int64_t Arval_n,
                         double *br, int64_t br_n, double *cr, int64_t cr_n)
{
    const int *ap, *ar;
    const double *av, *bp, *cp;
    int mr = 0, nr = 0, i, nnz;
    pre_slot *s = pre_get(h);
    if (!s || !Arptr || !Arrow || !Arval || !br || !cr) return -1;
    presolve_reduced(s->p, &ap, &ar, &av, &bp, &cp, &mr, &nr);
    nnz = (nr > 0 && ap) ? ap[nr] : 0;
    for (i = 0; i <= nr; i++) Arptr[i] = (i64)ap[i];
    for (i = 0; i < nnz; i++) { Arrow[i] = (i64)ar[i]; Arval[i] = av[i]; }
    for (i = 0; i < mr; i++) br[i] = bp[i];
    for (i = 0; i < nr; i++) cr[i] = cp[i];
    return (i64)nr;
}

i64 ofx_presolve_postsolve(i64 h, const double *xred, int64_t xred_n, const double *yred, int64_t yred_n,
                           double *xfull, int64_t xfull_n, double *yfull, int64_t yfull_n)
{
    pre_slot *s = pre_get(h);
    if (!s || !xfull || !yfull) return -1;
    presolve_postsolve(s->p, xred, yred, xfull, yfull);
    return 0;
}

i64 ofx_presolve_postsolve_dir(i64 h, const double *rred, int64_t rred_n, const double *yred, int64_t yred_n,
                               double *rfull, int64_t rfull_n, double *yfull, int64_t yfull_n)
{
    pre_slot *s = pre_get(h);
    if (!s || !rfull || !yfull) return -1;
    presolve_postsolve_dir(s->p, rred, yred, rfull, yfull);
    return 0;
}

i64 ofx_presolve_free(i64 h)
{
    pre_slot *s = pre_get(h);
    if (!s) return -1;
    presolve_free(s->p);
    s->p = NULL;
    s->used = 0;
    return 0;
}
