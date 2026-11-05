/* gs_alpha.c — Stochastic Dominance (FSD/SSD/TSD). */
#include "gs_alpha.h"

#include <math.h>
#include <stdlib.h>

/* D_k(x) = E[(x-A)_+^{k-1}] - E[(x-B)_+^{k-1}]; a domina b di ordine k se
 * D_k(x) <= 0 per ogni x (e < 0 in qualche punto). Formula esatta (senza
 * integrazione numerica) valutata sui punti campione, sui punti medi e in coda. */
static gc_real _mean_pospart(const gc_real *v, int n, gc_real x, int e)
{
    gc_real s = 0.0;
    int i;
    if (e == 0) {
        for (i = 0; i < n; i++) if (x >= v[i]) s += 1.0;   /* F(x)=P(X<=x) */
    } else {
        for (i = 0; i < n; i++) { gc_real d = x - v[i]; if (d > 0.0) s += pow(d, (gc_real)e); }
    }
    return s / (gc_real)n;
}

static int _cmp_real(const void *pa, const void *pb)
{
    gc_real x = *(const gc_real *)pa, y = *(const gc_real *)pb;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

int gs_stochastic_dominance(const gc_real *a, int na, const gc_real *b, int nb, int order)
{
    gc_real *t, x, lo, hi, scale, tol, dk;
    int i, m, k, dom_a, dom_b, res = 0;

    if (!a || !b || na < 1 || nb < 1 || order < 1 || order > 3) return 0;

    m = na + nb;
    t = (gc_real *)malloc((size_t)m * sizeof(gc_real));
    if (!t) return 0;
    for (i = 0; i < na; i++) t[i] = a[i];
    for (i = 0; i < nb; i++) t[na + i] = b[i];
    qsort(t, (size_t)m, sizeof(gc_real), _cmp_real);
    lo = t[0]; hi = t[m - 1];
    scale = (hi > lo) ? (hi - lo) : 1.0;

    for (k = 1; k <= order; k++) {
        int e = k - 1;
        dom_a = 1; dom_b = 1;
        tol = 1e-9 * pow(scale, (gc_real)e);
        for (i = -1; i <= m; i++) {                  /* -1 e m: code estese */
            x = (i < 0) ? lo - 0.5 * scale : (i >= m) ? hi + 0.5 * scale : t[i];
            if (i >= 0 && i < m) {
                dk = _mean_pospart(a, na, x, e) - _mean_pospart(b, nb, x, e);
                if (dk > tol) dom_a = 0;
                if (dk < -tol) dom_b = 0;
            }
            if (i >= 0 && i < m - 1) {               /* punto medio t[i], t[i+1] */
                gc_real xm = 0.5 * (t[i] + t[i + 1]);
                dk = _mean_pospart(a, na, xm, e) - _mean_pospart(b, nb, xm, e);
                if (dk > tol) dom_a = 0;
                if (dk < -tol) dom_b = 0;
            }
            if (i == -1 || i == m) {                 /* coda: verifica segno del limite */
                dk = _mean_pospart(a, na, x, e) - _mean_pospart(b, nb, x, e);
                if (dk > tol) dom_a = 0;
                if (dk < -tol) dom_b = 0;
            }
        }
        if (dom_a && !dom_b) { res = k; break; }
        if (dom_b && !dom_a) { res = -k; break; }
        if (dom_a && dom_b) break;
    }

    free(t);
    return res;
}

int gs_alpha_decay_rate(const gc_real *alpha, int nh, gc_real *out_lambda, gc_real *out_halflife)
{
    gc_real sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, slope, inter, lam, hl;
    int h, cnt = 0;

    if (!alpha || nh < 2 || !out_lambda || !out_halflife) return -1;
    for (h = 0; h < nh; h++) {
        gc_real y;
        if (fabs(alpha[h]) < 1e-300) continue;
        y = log(fabs(alpha[h]));
        sx += (gc_real)h; sy += y; sxx += (gc_real)h * (gc_real)h; sxy += (gc_real)h * y;
        cnt++;
    }
    if (cnt < 2) return -1;
    { gc_real den = (gc_real)cnt * sxx - sx * sx;
      if (fabs(den) < 1e-300) return -1;
      slope = ((gc_real)cnt * sxy - sx * sy) / den; }
    inter = (sy - slope * sx) / (gc_real)cnt;
    (void)inter;
    lam = -slope;
    if (lam <= 0.0) { *out_lambda = lam; *out_halflife = 0.0 / 0.0; return -1; }
    hl = log(2.0) / lam;
    *out_lambda = lam; *out_halflife = hl;
    return 0;
}
