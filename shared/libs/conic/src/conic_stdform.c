/*
 * conic_stdform.c — shim FFI della trasformazione in forma standard (handle).
 *
 * STDFORMBUILD prende l'LP/QP nell'utente (vincoli EQ/LO/UP/free, variabili con
 * bounds, Q come triplette) e restituisce un handle (>0) con il problema in
 * forma standard  min 1/2 xt' Qs xt + cs' xt  s.t. As xt = bs, xt >= 0.
 * La matrice A e' in CSC per colonne (colptr nvar+1, sub/val per gli nnz);
 * Q e' data come nq triplette (qi,qj,qv) simmetriche (nq=0 per un LP).
 *
 * Accesso: STDFORMINFO (dimensioni), STDFORMGETA/GETQ (materializzazione densa,
 * per i solver densi), STDFORMMAPX/MAPDIR/MAPY (ritorno nello spazio originale),
 * STDFORMFREE.
 */
#include "stdform.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef long long i64;

#define NN_SF_BASE 7500
#define NN_SF_MAX  16

typedef struct { int used; StdForm *s; } sf_slot;
static sf_slot g_sf[NN_SF_MAX];

static sf_slot *sf_get(i64 h)
{
    if (h <= NN_SF_BASE || h > NN_SF_BASE + NN_SF_MAX) return NULL;
    if (!g_sf[h - NN_SF_BASE - 1].used) return NULL;
    return &g_sf[h - NN_SF_BASE - 1];
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

i64 ofx_stdform_build(i64 nvar, i64 ncon,
                      const double *c_int, int64_t c_int_n,
                      const i64 *qi, int64_t qi_n, const i64 *qj, int64_t qj_n, const double *qv, int64_t qv_n, i64 nq,
                      const double *lx, int64_t lx_n, const double *ux, int64_t ux_n,
                      const double *lc, int64_t lc_n, const double *uc, int64_t uc_n,
                      const i64 *col_ptr, int64_t col_ptr_n, const i64 *sub, int64_t sub_n, const double *val, int64_t val_n)
{
    int *qii = NULL, *qjj = NULL, *cp = NULL, *sb = NULL;
    double *blx = NULL, *bux = NULL, *blc = NULL, *buc = NULL;
    StdForm *sf;
    i64 nnz, i;
    int rc = 0;

    if (nvar < 1 || ncon < 0 || !c_int || !lx || !ux || !col_ptr) return -1;
    if (ncon > 0 && (!lc || !uc)) return -1;
    if (nq < 0) return -1;
    nnz = col_ptr[nvar];
    if (nnz < 0) return -1;

    /* Sentinella per i lati liberi: |v| >= 1e307 -> +/-INFINITY.  In BASIC non
     * esiste un letterale infinito, quindi il binding accetta 1E308 / -1E308. */
    blx = (double *)malloc((size_t)nvar * sizeof(double));
    bux = (double *)malloc((size_t)nvar * sizeof(double));
    blc = (double *)malloc((size_t)(ncon > 0 ? ncon : 1) * sizeof(double));
    buc = (double *)malloc((size_t)(ncon > 0 ? ncon : 1) * sizeof(double));
    if (!blx || !bux || !blc || !buc) {
        free(blx); free(bux); free(blc); free(buc);
        return -1;
    }
    for (i = 0; i < nvar; i++) {
        blx[i] = (lx[i] <= -1e307) ? -INFINITY : lx[i];
        bux[i] = (ux[i] >= 1e307) ? INFINITY : ux[i];
    }
    for (i = 0; i < ncon; i++) {
        blc[i] = (lc[i] <= -1e307) ? -INFINITY : lc[i];
        buc[i] = (uc[i] >= 1e307) ? INFINITY : uc[i];
    }

    cp = cvt_i(col_ptr, nvar + 1);
    sb = cvt_i(sub, nnz);
    qii = cvt_i(qi, nq);
    qjj = cvt_i(qj, nq);
    if (!cp || (nnz > 0 && !sb) || (nq > 0 && (!qii || !qjj))) {
        free(cp); free(sb); free(qii); free(qjj);
        free(blx); free(bux); free(blc); free(buc);
        return -1;
    }
    sf = stdform_build((int)nvar, (int)ncon, c_int,
                       qii, qjj, qv, (int)nq,
                       blx, bux, blc, buc, cp, sb, val);
    free(cp); free(sb); free(qii); free(qjj);
    free(blx); free(bux); free(blc); free(buc);
    if (!sf) return -1;
    for (i = 0; i < NN_SF_MAX; i++) {
        if (g_sf[i].used) continue;
        g_sf[i].used = 1;
        g_sf[i].s = sf;
        return (i64)(NN_SF_BASE + i + 1);
    }
    stdform_free(sf);
    return -1;
}

/* out(0)=m, out(1)=n, out(2)=nvar, out(3)=ncon, out(4)=cfix */
i64 ofx_stdform_info(i64 h, double *out, int64_t out_n)
{
    sf_slot *s = sf_get(h);
    if (!s || !out) return -1;
    out[0] = (double)s->s->m;
    out[1] = (double)s->s->n;
    out[2] = (double)s->s->nvar;
    out[3] = (double)s->s->ncon;
    out[4] = s->s->cfix;
    return 0;
}

/* b del problema standard (m valori). */
i64 ofx_stdform_get_b(i64 h, double *b, int64_t b_n)
{
    sf_slot *s = sf_get(h);
    int i;
    if (!s || !b) return -1;
    for (i = 0; i < s->s->m; i++) b[i] = s->s->b[i];
    return 0;
}

/* c del problema standard (n valori; include il termine 1/2 xt' Qs xt). */
i64 ofx_stdform_get_c(i64 h, double *c, int64_t c_n)
{
    sf_slot *s = sf_get(h);
    int i;
    if (!s || !c) return -1;
    for (i = 0; i < s->s->n; i++) c[i] = s->s->c[i];
    return 0;
}

/* A densa m x n row-major; il chiamante dimensiona A(m*n). */
i64 ofx_stdform_get_a(i64 h, double *A, int64_t A_n)
{
    sf_slot *s = sf_get(h);
    double *d;
    long i, n;
    if (!s || !A) return -1;
    d = stdform_dense_A(s->s);
    if (!d) return -1;
    n = (long)s->s->m * (long)s->s->n;
    for (i = 0; i < n; i++) A[i] = d[i];
    free(d);
    return 0;
}

/* Q densa n x n row-major; -1 se il problema e' un LP (nessuna Q). */
i64 ofx_stdform_get_q(i64 h, double *Q, int64_t Q_n)
{
    sf_slot *s = sf_get(h);
    double *d;
    long i, n;
    if (!s || !Q) return -1;
    d = stdform_dense_Q(s->s);
    if (!d) return -1;
    n = (long)s->s->n * (long)s->s->n;
    for (i = 0; i < n; i++) Q[i] = d[i];
    free(d);
    return 0;
}

i64 ofx_stdform_map_x(i64 h, const double *xt, int64_t xt_n, double *x, int64_t x_n)
{
    sf_slot *s = sf_get(h);
    if (!s || !xt || !x) return -1;
    stdform_map_x(s->s, xt, x);
    return 0;
}

i64 ofx_stdform_map_dir(i64 h, const double *dxt, int64_t dxt_n, double *dx, int64_t dx_n)
{
    sf_slot *s = sf_get(h);
    if (!s || !dxt || !dx) return -1;
    stdform_map_dir(s->s, dxt, dx);
    return 0;
}

i64 ofx_stdform_map_y(i64 h, const double *ystd, int64_t ystd_n, double *ymin, int64_t ymin_n)
{
    sf_slot *s = sf_get(h);
    if (!s || !ystd || !ymin) return -1;
    stdform_map_y(s->s, ystd, ymin);
    return 0;
}

i64 ofx_stdform_free(i64 h)
{
    sf_slot *s = sf_get(h);
    if (!s) return -1;
    stdform_free(s->s);
    s->s = NULL;
    s->used = 0;
    return 0;
}
