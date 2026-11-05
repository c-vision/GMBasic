/*
 * gor_pgm.c — indice PGM e Swing Filter (da IvanIsCoding/AppendPGM).
 *
 * PLA ottimale (numero minimo di segmenti) di una sequenza monotona con errore
 * massimo eps tramite lo Swing Filter; ricerca per rank con intervallo garantito.
 */
#include "gor_pgm.h"
#include <stdlib.h>
#include <math.h>

#define GOR_PGM_INF 1e300

static int pgm_push(gor_pgm *pgm, int *cap, gor_real a, gor_real b, gor_real pos)
{
    if (pgm->nseg >= *cap) {
        int ncap = *cap ? *cap * 2 : 8;
        gor_pgm_seg *ns = (gor_pgm_seg *)realloc(pgm->seg, (size_t)ncap * sizeof(gor_pgm_seg));
        if (!ns) return -1;
        pgm->seg = ns;
        *cap = ncap;
    }
    pgm->seg[pgm->nseg].a = a;
    pgm->seg[pgm->nseg].b = b;
    pgm->seg[pgm->nseg].pos = pos;
    pgm->nseg++;
    return 0;
}

int gor_pgm_build(gor_pgm *pgm, int n, const gor_real *x, int eps)
{
    int cap = 0, i;
    gor_real ax, ay, a_lo, a_hi;
    if (!pgm) return -1;
    pgm->seg = NULL;
    pgm->nseg = 0;
    pgm->count = n;
    pgm->eps = eps < 0 ? 0 : eps;
    if (n <= 0 || !x) return 0;
    pgm->xmin = x[0];
    pgm->xmax = x[n - 1];
    if (n == 1) return 0;   /* un solo punto: rank 0 noto */
    ax = x[0]; ay = 0.0;
    a_lo = -GOR_PGM_INF; a_hi = GOR_PGM_INF;
    for (i = 1; i < n; i++) {
        gor_real dx = x[i] - ax;
        gor_real slo, shi, nlo, nhi;
        if (dx <= 0.0) continue;   /* chiavi duplicate: ignora */
        slo = ((gor_real)i - ay - (gor_real)pgm->eps) / dx;
        shi = ((gor_real)i - ay + (gor_real)pgm->eps) / dx;
        nlo = a_lo > slo ? a_lo : slo;
        nhi = a_hi < shi ? a_hi : shi;
        if (nlo <= nhi) {
            a_lo = nlo; a_hi = nhi;
        } else {
            gor_real a = 0.5 * (a_lo + a_hi);
            if (pgm_push(pgm, &cap, a, ay - a * ax, ax) != 0) { gor_pgm_free(pgm); return -1; }
            ax = x[i - 1]; ay = (gor_real)(i - 1);
            dx = x[i] - ax;
            if (dx <= 0.0) { a_lo = -GOR_PGM_INF; a_hi = GOR_PGM_INF; continue; }
            a_lo = ((gor_real)i - ay - (gor_real)pgm->eps) / dx;
            a_hi = ((gor_real)i - ay + (gor_real)pgm->eps) / dx;
        }
    }
    {
        gor_real a = 0.5 * (a_lo + a_hi);
        if (a_lo <= -GOR_PGM_INF / 2 || a_hi >= GOR_PGM_INF / 2) {
            gor_real dx = x[n - 1] - ax;
            a = dx > 0.0 ? ((gor_real)(n - 1) - ay) / dx : 0.0;
        }
        if (pgm_push(pgm, &cap, a, ay - a * ax, ax) != 0) { gor_pgm_free(pgm); return -1; }
    }
    return 0;
}

void gor_pgm_free(gor_pgm *pgm)
{
    if (!pgm) return;
    free(pgm->seg);
    pgm->seg = NULL;
    pgm->nseg = 0;
}

long gor_pgm_search(const gor_pgm *pgm, gor_real key, long *lo, long *hi)
{
    int a = 0, b, m;
    gor_real pred;
    long rlo, rhi;
    if (!pgm || pgm->nseg <= 0 || pgm->count <= 0) {
        if (lo) *lo = 0;
        if (hi) *hi = -1;
        return -1;
    }
    /* ultimo segmento con pos <= key */
    b = pgm->nseg - 1;
    while (a <= b) {
        m = (a + b) / 2;
        if (pgm->seg[m].pos <= key) a = m + 1;
        else b = m - 1;
    }
    if (b < 0) b = 0;
    pred = pgm->seg[b].a * key + pgm->seg[b].b;
    rlo = (long)floor(pred - (gor_real)pgm->eps);
    rhi = (long)ceil(pred + (gor_real)pgm->eps);
    if (rlo < 0) rlo = 0;
    if (rhi > pgm->count - 1) rhi = pgm->count - 1;
    if (lo) *lo = rlo;
    if (hi) *hi = rhi;
    return (long)llround(pred);
}
