/* gs_timeseries_models.c — GARCH/Kalman/OU/volatilità (gmbstats-timeseries).
 * Estratti da gs_stats2.c; il resto (DCC/MCMC/bootstrap) resta in gmbstats. */
#include "gs_stats2.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static gc_real garch_negloglik(const gc_real *p, const gc_real *r, int n)
{
    gc_real w = p[0], al = p[1], be = p[2], s2, ll = 0.0;
    int t;
    s2 = r[0] * r[0];
    if (s2 <= 0.0) s2 = 1e-6;
    for (t = 1; t < n; t++) {
        s2 = w + al * r[t - 1] * r[t - 1] + be * s2;
        if (s2 < 1e-12) s2 = 1e-12;
        ll += log(s2) + r[t] * r[t] / s2;
    }
    return 0.5 * ll;
}

gc_status gs_garch11(const gc_real *r, int n, gc_real *omega, gc_real *alpha, gc_real *beta)
{
    /* discesa a gradiente numerico con proiezione sui vincoli */
    gc_real p[3], h = 1e-5, step = 0.05;
    int it, i;
    if (r == NULL || omega == NULL || alpha == NULL || beta == NULL || n < 5) return GC_ERR_ARG;
    {
        gc_real v = 0.0;
        for (i = 0; i < n; i++) v += r[i] * r[i];
        v /= (gc_real)n;
        p[0] = 0.1 * v; p[1] = 0.1; p[2] = 0.8;
    }
    for (it = 0; it < 2000; it++) {
        gc_real f0 = garch_negloglik(p, r, n), grad[3], np[3];
        gc_real gn = 0.0;
        for (i = 0; i < 3; i++) {
            gc_real save = p[i];
            p[i] = save + h;
            grad[i] = (garch_negloglik(p, r, n) - f0) / h;
            p[i] = save;
            gn += grad[i] * grad[i];
        }
        gn = sqrt(gn);
        if (gn < 1e-10) break;
        for (i = 0; i < 3; i++) np[i] = p[i] - step * grad[i] / gn;
        /* proiezione: ω>1e-8, α,β>=0, α+β<0.999 */
        if (np[0] < 1e-8) np[0] = 1e-8;
        if (np[1] < 0.0) np[1] = 0.0;
        if (np[2] < 0.0) np[2] = 0.0;
        if (np[1] + np[2] > 0.999) { gc_real s = 0.999 / (np[1] + np[2]); np[1] *= s; np[2] *= s; }
        if (garch_negloglik(np, r, n) < f0) { p[0] = np[0]; p[1] = np[1]; p[2] = np[2]; step *= 1.05; }
        else step *= 0.5;
        if (step < 1e-12) break;
    }
    *omega = p[0]; *alpha = p[1]; *beta = p[2];
    return GC_OK;
}

gc_status gs_kalman1d(const gc_real *z, int n, gc_real a, gc_real h, gc_real q, gc_real r,
                      gc_real x0, gc_real p0, gc_real *xout)
{
    gc_real x = x0, P = p0;
    int k;
    if (z == NULL || xout == NULL || n <= 0) return GC_ERR_ARG;
    for (k = 0; k < n; k++) {
        gc_real xp = a * x, Pp = a * P * a + q, K;
        K = Pp * h / (h * Pp * h + r);
        x = xp + K * (z[k] - h * xp);
        P = (1.0 - K * h) * Pp;
        xout[k] = x;
    }
    return GC_OK;
}

static unsigned long lcg(unsigned long *s)
{
    *s = (*s * 6364136223846793005ULL + 1442695040888963407ULL);
    return (*s >> 33);
}

static gc_real urand(unsigned long *s)
{
    return (gc_real)lcg(s) / 2147483648.0;
}

gc_status gs_vol_ohlc(const gc_real *o, const gc_real *h, const gc_real *l, const gc_real *c,
                      int n, gs_vol_ohlc_method method, gc_real year_frac, gc_real *vol)
{
    gc_real var = 0.0;
    int i;
    if (!o || !h || !l || !c || !vol || n < 2 || year_frac <= 0.0) return GC_ERR_ARG;
    if (method == GS_VOL_YANG_ZHANG) {
        /* Yang-Zhang (1999): sigma2 = sigma2_overnight + k*sigma2_intraday
         *                      + (1-k)*sigma2_rs, k = 0.34/(1.34+(n+1)/(n-1)) */
        gc_real m_o = 0.0, m_c = 0.0, k, oo = 0.0, ic = 0.0;
        for (i = 1; i < n; i++) { m_o += log(o[i] / c[i - 1]); m_c += log(c[i] / o[i]); }
        m_o /= (gc_real)(n - 1);
        m_c /= (gc_real)(n - 1);
        for (i = 1; i < n; i++) { gc_real d = log(o[i] / c[i - 1]) - m_o; oo += d * d; }
        for (i = 0; i < n; i++) { gc_real d = log(c[i] / o[i]) - m_c; ic += d * d; }
        oo /= (gc_real)(n - 1);
        ic /= (gc_real)n;
        k = 0.34 / (1.34 + (gc_real)(n + 1) / (gc_real)(n - 1));
        for (i = 0; i < n; i++) {
            gc_real u = log(h[i] / o[i]), d = log(l[i] / o[i]);
            var += 0.5 * (u - d) * (u - d) - (2.0 * M_LN2 - 1.0) * log(c[i] / o[i]) * log(c[i] / o[i]);
        }
        var = oo + k * ic + (1.0 - k) * (var / (gc_real)n);
    } else {
        for (i = 0; i < n; i++) {
            gc_real u = log(h[i] / o[i]), d = log(l[i] / o[i]), cc = log(c[i] / o[i]);
            switch (method) {
            case GS_VOL_PARKINSON:
                var += (u - d) * (u - d) / (4.0 * M_LN2);
                break;
            case GS_VOL_GARMANKLASS:
                var += 0.511 * (u - d) * (u - d) - 0.019 * (cc * (u + d) - 2.0 * u * d)
                     - 0.383 * cc * cc;
                break;
            case GS_VOL_ROGERS_SATCHELL:
                var += 0.5 * (u - d) * (u - d) - (2.0 * M_LN2 - 1.0) * cc * cc;
                break;
            case GS_VOL_GKYZ: { /* 0.5 ln(O/Cprev)^2 + 0.5 ln(H/L)^2 - (2ln2-1) ln(C/O)^2 */
                gc_real ov = (i >= 1) ? log(o[i] / c[i - 1]) : 0.0;
                var += 0.5 * ov * ov + 0.5 * (u - d) * (u - d) - (2.0 * M_LN2 - 1.0) * cc * cc;
                break; }
            default:
                return GC_ERR_ARG;
            }
        }
        var /= (gc_real)n;
    }
    *vol = sqrt(var / year_frac);
    return GC_OK;
}

/* Permutation test a due campioni (differenza di medie, two-sided).
 * p-value empirico: B shuffle dei label, p = #(|diff| >= |diff_obs|)/B. */
int gs_ou_fit(const gc_real *x, int n, gc_real dt,
              gc_real *theta, gc_real *mu, gc_real *sigma,
              gc_real *phi, gc_real *half_life)
{
    gc_real mean_x = 0.0, mean_y = 0.0, sxx = 0.0, sxy = 0.0, sse = 0.0;
    gc_real alpha, phi_v, log_phi, omphi, mu_v, theta_v, vf, iv, s2;
    int cnt = 0, i;
    if (!x || n < 3 || dt <= 0.0) return -1;
    for (i = 1; i < n; i++) {
        gc_real xv = x[i - 1], yv = x[i], dx, dy, inv_n;
        cnt++;
        dx = xv - mean_x;
        dy = yv - mean_y;
        inv_n = 1.0 / (gc_real)cnt;
        mean_x += dx * inv_n;
        mean_y += dy * inv_n;
        sxx += dx * (xv - mean_x);
        sxy += dx * (yv - mean_y);
    }
    if (!(sxx > 0.0)) return -1;
    phi_v = sxy / sxx;
    if (!(phi_v > 0.0 && phi_v < 1.0)) return -1;
    alpha = mean_y - phi_v * mean_x;
    log_phi = log(phi_v);
    omphi = -expm1(log_phi);
    if (!(omphi > 0.0)) return -1;
    mu_v = alpha / omphi;
    theta_v = -log_phi / dt;
    if (!(theta_v > 0.0)) return -1;
    for (i = 1; i < n; i++) {
        gc_real r = x[i] - alpha - phi_v * x[i - 1];
        sse += r * r;
    }
    vf = -expm1(2.0 * log_phi) / (2.0 * theta_v);
    if (!(vf > 0.0)) return -1;
    iv = sse / (gc_real)cnt;
    s2 = iv / vf;
    if (s2 < 0.0) return -1;
    if (theta) *theta = theta_v;
    if (mu) *mu = mu_v;
    if (sigma) *sigma = sqrt(s2);
    if (phi) *phi = phi_v;
    if (half_life) *half_life = log(2.0) / theta_v;
    return 0;
}

/* ---- Welford online moments (monte-carlo-cpp #210). ---- */
gc_status gs_vol_hodges_tompkins(const gc_real *c, int n, int window,
                                 gc_real year_frac, gc_real *vol)
{
    gc_real *r, mean = 0.0, var = 0.0, adj; int i, cnt;
    if (!c || !vol || n < 3 || window < 2 || year_frac <= 0.0) return GC_ERR_ARG;
    if (window > n - 1) window = n - 1;
    r = (gc_real *)malloc((size_t)(n - 1) * sizeof(gc_real));
    if (!r) return GC_ERR_INTERNAL;
    for (i = 1; i < n; i++) {
        if (c[i - 1] <= 0.0 || c[i] <= 0.0) { free(r); return GC_ERR_ARG; }
        r[i - 1] = log(c[i] / c[i - 1]);
    }
    cnt = n - 1;
    { int lo = cnt - window; for (i = lo; i < cnt; i++) mean += r[i]; mean /= (gc_real)window;
      for (i = lo; i < cnt; i++) { gc_real d = r[i] - mean; var += d * d; }
      var /= (gc_real)(window - 1); }
    adj = 1.0 / (1.0 - (gc_real)window / (gc_real)cnt
                 + ((gc_real)window * (gc_real)window - 1.0) / (3.0 * (gc_real)cnt * (gc_real)cnt));
    *vol = sqrt(var * adj / year_frac);
    free(r);
    return GC_OK;
}

/* Disuguaglianza / distanze */
