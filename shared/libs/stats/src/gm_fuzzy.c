/* gm_fuzzy.c — insiemi e logica fuzzy (riscrittura). */
#include "gm_fuzzy.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gm_tfn gm_tfn_make(gm_real lo, gm_real mid, gm_real hi)
{
    gm_tfn t;
    t.lo = lo; t.mid = mid; t.hi = hi;
    return t;
}

gm_real gm_fuzzy_membership_tri(gm_tfn t, gm_real x)
{
    if (x <= t.lo || x >= t.hi) return 0.0;
    if (x == t.mid) return 1.0;
    if (x < t.mid) return (t.mid > t.lo) ? (x - t.lo) / (t.mid - t.lo) : 1.0;
    return (t.hi > t.mid) ? (t.hi - x) / (t.hi - t.mid) : 1.0;
}

gm_tfn gm_fuzzy_union(gm_tfn a, gm_tfn b)
{
    return gm_tfn_make(a.lo < b.lo ? a.lo : b.lo,
                       a.mid > b.mid ? a.mid : b.mid,
                       a.hi > b.hi ? a.hi : b.hi);
}

gm_tfn gm_fuzzy_intersection(gm_tfn a, gm_tfn b)
{
    return gm_tfn_make(a.lo > b.lo ? a.lo : b.lo,
                       a.mid < b.mid ? a.mid : b.mid,
                       a.hi < b.hi ? a.hi : b.hi);
}

gm_real gm_fuzzy_defuzz_centroid(const gm_real *x, const gm_real *mu, int n)
{
    gm_real num = 0.0, den = 0.0;
    int i;
    if (x == NULL || mu == NULL || n <= 0) return 0.0;
    for (i = 0; i < n; i++) { num += x[i] * mu[i]; den += mu[i]; }
    return (den != 0.0) ? num / den : 0.0;
}

gm_real gm_fuzzy_defuzz_bisector(const gm_real *x, const gm_real *mu, int n)
{
    gm_real total = 0.0, acc = 0.0;
    int i;
    if (x == NULL || mu == NULL || n <= 0) return 0.0;
    for (i = 0; i < n; i++) total += mu[i];
    for (i = 0; i < n; i++) {
        acc += mu[i];
        if (acc >= 0.5 * total) return x[i];
    }
    return x[n - 1];
}

gm_real gm_fuzzy_defuzz_mom(const gm_real *x, const gm_real *mu, int n)
{
    gm_real mx = mu[0], sum = 0.0;
    int i, cnt = 0;
    if (x == NULL || mu == NULL || n <= 0) return 0.0;
    for (i = 1; i < n; i++) if (mu[i] > mx) mx = mu[i];
    for (i = 0; i < n; i++) if (mu[i] == mx) { sum += x[i]; cnt++; }
    return cnt ? sum / (gm_real)cnt : 0.0;
}

gm_status gm_fuzzy_fcm(const gm_real *data, int n, int d, int c, gm_real m,
                       gm_real *centers, gm_real *U, int maxit, gm_real tol)
{
    int i, j, k, it;
    if (data == NULL || centers == NULL || U == NULL || n <= 0 || d <= 0 || c <= 0 || m <= 1.0)
        return GM_ERR_ARG;
    /* inizializza i centroidi ai primi c punti */
    for (j = 0; j < c; j++)
        for (k = 0; k < d; k++)
            centers[(size_t)j * (size_t)d + (size_t)k] = data[(size_t)(j % n) * (size_t)d + (size_t)k];
    for (it = 0; it < maxit; it++) {
        gm_real maxd = 0.0;
        for (i = 0; i < n; i++) {
            gm_real dsum = 0.0;
            gm_real *dist = (gm_real *)malloc((size_t)c * sizeof(gm_real));
            if (!dist) return GM_ERR_NO_MEM;
            for (j = 0; j < c; j++) {
                gm_real s = 0.0;
                for (k = 0; k < d; k++) {
                    gm_real diff = data[(size_t)i * (size_t)d + (size_t)k] - centers[(size_t)j * (size_t)d + (size_t)k];
                    s += diff * diff;
                }
                dist[j] = sqrt(s);
                if (dist[j] < 1e-12) dist[j] = 1e-12;
            }
            for (j = 0; j < c; j++) {
                gm_real denom = 0.0;
                for (k = 0; k < c; k++)
                    denom += pow(dist[j] / dist[k], 2.0 / (m - 1.0));
                U[(size_t)j * (size_t)n + (size_t)i] = 1.0 / denom;
            }
            free(dist);
            (void)dsum;
        }
        /* aggiorna i centroidi */
        for (j = 0; j < c; j++) {
            gm_real wsum = 0.0;
            gm_real *num = (gm_real *)calloc((size_t)d, sizeof(gm_real));
            if (!num) return GM_ERR_NO_MEM;
            for (i = 0; i < n; i++) {
                gm_real w = pow(U[(size_t)j * (size_t)n + (size_t)i], m);
                wsum += w;
                for (k = 0; k < d; k++) num[k] += w * data[(size_t)i * (size_t)d + (size_t)k];
            }
            for (k = 0; k < d; k++) {
                gm_real nv = (wsum > 0.0) ? num[k] / wsum : centers[(size_t)j * (size_t)d + (size_t)k];
                gm_real dd = fabs(nv - centers[(size_t)j * (size_t)d + (size_t)k]);
                if (dd > maxd) maxd = dd;
                centers[(size_t)j * (size_t)d + (size_t)k] = nv;
            }
            free(num);
        }
        if (maxd <= tol) break;
    }
    return GM_OK;
}
