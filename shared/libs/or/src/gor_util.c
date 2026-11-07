/* gor_util.c — code, inventario, MCDM, MDP, simulazione. */
#include "gor_util.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================== code ============================== */

gor_real gor_queue_mm1(gor_real lambda, gor_real mu, gor_real *L, gor_real *Lq,
                       gor_real *W, gor_real *Wq)
{
    gor_real rho, lq;
    if (mu <= 0.0 || lambda < 0.0) { if (L) *L = 0; if (Lq) *Lq = 0; if (W) *W = 0; if (Wq) *Wq = 0; return 0.0; }
    rho = lambda / mu;
    if (rho >= 1.0) { if (L) *L = 1e300; if (Lq) *Lq = 1e300; if (W) *W = 1e300; if (Wq) *Wq = 1e300; return rho; }
    lq = rho * rho / (1.0 - rho);
    if (Lq) *Lq = lq;
    if (L) *L = lq + rho;
    if (W) *W = 1.0 / (mu - lambda);
    if (Wq) *Wq = rho / (mu - lambda);
    return rho;
}

gor_real gor_queue_mmc(gor_real lambda, gor_real mu, int c, gor_real *L, gor_real *Lq,
                       gor_real *W, gor_real *Wq)
{
    gor_real rho, sum = 1.0, term = 1.0, p0, lq, a;
    int k;
    if (c <= 0 || mu <= 0.0 || lambda < 0.0) return 0.0;
    a = lambda / mu;
    rho = a / c;
    if (rho >= 1.0) { if (L) *L = 1e300; if (Lq) *Lq = 1e300; if (W) *W = 1e300; if (Wq) *Wq = 1e300; return rho; }
    for (k = 1; k < c; k++) { term *= a / k; sum += term; }
    {
        gor_real last = term * a / c;
        p0 = 1.0 / (sum + last / (1.0 - rho));
    }
    {
        gor_real fact = 1.0;
        int j;
        for (j = 1; j <= c; j++) fact *= j;
        lq = p0 * pow(a, c) / fact * rho / ((1.0 - rho) * (1.0 - rho));
    }
    if (Lq) *Lq = lq;
    if (L) *L = lq + a;
    if (W) *W = (lq + a) / lambda;
    if (Wq) *Wq = lq / lambda;
    return rho;
}

gor_real gor_littles_law(gor_real lambda, gor_real W) { return lambda * W; }

/* ============================== inventario ============================== */

gor_real gor_eoq(gor_real demand, gor_real setup, gor_real holding)
{
    if (demand <= 0.0 || holding <= 0.0) return 0.0;
    return sqrt(2.0 * demand * setup / holding);
}

gor_real gor_epq(gor_real demand, gor_real setup, gor_real holding, gor_real rate)
{
    if (demand <= 0.0 || holding <= 0.0 || rate <= demand) return 0.0;
    return sqrt(2.0 * demand * setup / (holding * (1.0 - demand / rate)));
}

static gor_real norm_ppf_u(gor_real p)
{
    /* inversa normale standard (Acklam) */
    static const gor_real a[6] = {-3.969683028665376e+01, 2.209460984245205e+02,
        -2.759285104469687e+02, 1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    static const gor_real b[5] = {-5.447609879822406e+01, 1.615858368580409e+02,
        -1.556989798598866e+02, 6.680131188771972e+01, -1.328068155288572e+01};
    static const gor_real c[6] = {-7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    static const gor_real d[4] = {7.784695709041462e-03, 3.224671290700398e-01,
        2.445134137142996e+00, 3.754408661907416e+00};
    gor_real q, r;
    if (p <= 0.0) return -1e30;
    if (p >= 1.0) return 1e30;
    if (p < 0.02425) {
        q = sqrt(-2.0 * log(p));
        return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    if (p > 1.0 - 0.02425) {
        q = sqrt(-2.0 * log(1.0 - p));
        return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    q = p - 0.5; r = q * q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q / (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
}

gor_real gor_newsvendor(gor_real cu, gor_real co, gor_real mean, gor_real std)
{
    gor_real crit;
    if (cu + co <= 0.0) return mean;
    crit = cu / (cu + co);
    return mean + std * norm_ppf_u(crit);
}

gor_real gor_safety_stock(gor_real z, gor_real sigma) { return z * sigma; }

/* ============================== AHP ============================== */

gor_status gor_ahp(int n, const gor_real *pairwise, gor_real *weights, gor_real *consistency)
{
    gor_real *w, sum;
    int i, j, it;
    if (n <= 0 || pairwise == NULL || weights == NULL) return GOR_ERR_ARG;
    w = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!w) return GOR_ERR_NOMEM;
    for (i = 0; i < n; i++) w[i] = 1.0 / (gor_real)n;
    /* power iteration */
    for (it = 0; it < 100; it++) {
        gor_real *nw = (gor_real *)calloc((size_t)n, sizeof(gor_real));
        gor_real norm = 0.0;
        if (!nw) { free(w); return GOR_ERR_NOMEM; }
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++) nw[i] += pairwise[(size_t)i * (size_t)n + (size_t)j] * w[j];
        for (i = 0; i < n; i++) norm += nw[i];
        if (norm > 0) for (i = 0; i < n; i++) nw[i] /= norm;
        { gor_real diff = 0.0; for (i = 0; i < n; i++) diff += fabs(nw[i] - w[i]); memcpy(w, nw, (size_t)n * sizeof(gor_real)); free(nw); if (diff < 1e-12) break; }
    }
    for (i = 0; i < n; i++) weights[i] = w[i];
    if (consistency) {
        /* lambda_max ≈ media di (A w)_i / w_i; CI = (lambda_max - n)/(n-1) */
        gor_real lmax = 0.0;
        for (i = 0; i < n; i++) {
            gor_real s = 0.0;
            for (j = 0; j < n; j++) s += pairwise[(size_t)i * (size_t)n + (size_t)j] * w[j];
            if (w[i] > 0) lmax += s / w[i];
        }
        lmax /= (gor_real)n;
        *consistency = (n > 1) ? (lmax - n) / (gor_real)(n - 1) : 0.0;
    }
    free(w);
    sum = 0.0;
    for (i = 0; i < n; i++) sum += weights[i];
    (void)sum;
    return GOR_OK;
}

/* ============================== TOPSIS ============================== */

gor_status gor_topsis(int m, int n, const gor_real *X, const gor_real *w,
                      const int *benefit, gor_real *scores)
{
    gor_real *norm, *v, *dpos, *dneg;
    int i, j;
    if (m <= 0 || n <= 0 || X == NULL || scores == NULL) return GOR_ERR_ARG;
    norm = (gor_real *)malloc((size_t)m * (size_t)n * sizeof(gor_real));
    v = (gor_real *)malloc((size_t)m * (size_t)n * sizeof(gor_real));
    dpos = (gor_real *)malloc((size_t)m * sizeof(gor_real));
    dneg = (gor_real *)malloc((size_t)m * sizeof(gor_real));
    if (!norm || !v || !dpos || !dneg) { free(norm); free(v); free(dpos); free(dneg); return GOR_ERR_NOMEM; }
    /* normalizzazione vettoriale per colonna */
    for (j = 0; j < n; j++) {
        gor_real denom = 0.0;
        for (i = 0; i < m; i++) denom += X[(size_t)i * (size_t)n + (size_t)j] * X[(size_t)i * (size_t)n + (size_t)j];
        denom = sqrt(denom);
        for (i = 0; i < m; i++)
            norm[(size_t)i * (size_t)n + (size_t)j] = (denom > 0) ? X[(size_t)i * (size_t)n + (size_t)j] / denom : 0.0;
    }
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) v[(size_t)i * (size_t)n + (size_t)j] = norm[(size_t)i * (size_t)n + (size_t)j] * (w ? w[j] : 1.0);
    /* ideali */
    for (i = 0; i < m; i++) { dpos[i] = 0.0; dneg[i] = 0.0; }
    for (j = 0; j < n; j++) {
        gor_real bp = -1e300, bn = 1e300;
        for (i = 0; i < m; i++) { gor_real val = v[(size_t)i * (size_t)n + (size_t)j]; if (val > bp) bp = val; if (val < bn) bn = val; }
        for (i = 0; i < m; i++) {
            gor_real val = v[(size_t)i * (size_t)n + (size_t)j];
            gor_real best = benefit && benefit[j] ? bp : bn;
            gor_real worst = benefit && benefit[j] ? bn : bp;
            dpos[i] += (val - best) * (val - best);
            dneg[i] += (val - worst) * (val - worst);
        }
    }
    for (i = 0; i < m; i++) {
        gor_real dp = sqrt(dpos[i]), dn = sqrt(dneg[i]);
        scores[i] = (dp + dn > 0) ? dn / (dp + dn) : 0.0;
    }
    free(norm); free(v); free(dpos); free(dneg);
    return GOR_OK;
}

/* ============================== MDP ============================== */

int gor_mdp_value_iteration(int ns, int na, const gor_real *P, const gor_real *R,
                            gor_real gamma, gor_real tol, int maxit, gor_real *V, int *policy)
{
    gor_real *Vn;
    int it, s, a, s2;
    if (ns <= 0 || na <= 0 || P == NULL || R == NULL || V == NULL) return 0;
    Vn = (gor_real *)malloc((size_t)ns * sizeof(gor_real));
    if (!Vn) return 0;
    for (s = 0; s < ns; s++) V[s] = 0.0;
    for (it = 0; it < maxit; it++) {
        gor_real diff = 0.0;
        for (s = 0; s < ns; s++) {
            gor_real best = -1e300;
            int ba = 0;
            for (a = 0; a < na; a++) {
                gor_real q = R[(size_t)s * (size_t)na + (size_t)a];
                for (s2 = 0; s2 < ns; s2++)
                    q += gamma * P[((size_t)s * (size_t)na + (size_t)a) * (size_t)ns + (size_t)s2] * V[s2];
                if (q > best) { best = q; ba = a; }
            }
            Vn[s] = best;
            if (policy) policy[s] = ba;
            if (fabs(best - V[s]) > diff) diff = fabs(best - V[s]);
        }
        memcpy(V, Vn, (size_t)ns * sizeof(gor_real));
        if (diff < tol) { it++; break; }
    }
    free(Vn);
    return it;
}

int gor_mdp_policy_iteration(int ns, int na, const gor_real *P, const gor_real *R,
                             gor_real gamma, int maxit, gor_real *V, int *policy)
{
    /* usa value iteration come solver (converge alla stessa soluzione) */
    return gor_mdp_value_iteration(ns, na, P, R, gamma, 1e-10, maxit, V, policy);
}

/* ============================== Monte Carlo ============================== */

static unsigned long mc_rng(unsigned long *s)
{
    *s = (*s * 6364136223846793005UL + 1442695040888963407UL);
    return *s >> 33;
}

gor_real gor_montecarlo(gor_util_fn f, void *ctx, int d, const gor_real *lo, const gor_real *hi,
                        int n, unsigned long seed, gor_real *std)
{
    unsigned long s = seed ? seed : 1;
    gor_real mean = 0.0, m2 = 0.0;
    int i, k;
    gor_real *x;
    if (f == NULL || d <= 0 || n <= 0 || lo == NULL || hi == NULL) { if (std) *std = 0; return 0.0; }
    x = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!x) { if (std) *std = 0; return 0.0; }
    for (i = 0; i < n; i++) {
        gor_real v, delta;
        for (k = 0; k < d; k++)
            x[k] = lo[k] + (hi[k] - lo[k]) * ((gor_real)(mc_rng(&s) % 1000000UL) / 1000000.0);
        v = f(x, d, ctx);
        delta = v - mean;
        mean += delta / (gor_real)(i + 1);
        m2 += delta * (v - mean);
    }
    if (std) *std = (n > 1) ? sqrt(m2 / (gor_real)(n - 1)) : 0.0;
    free(x);
    return mean;
}

/* ============================== CVaR ============================== */

static int cmp_dbl_desc(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (y > x) - (y < x);
}

gor_real gor_cvar(int n, const gor_real *returns, gor_real alpha, gor_real *var)
{
    double *r;
    int k, tail;
    double sum = 0.0;
    if (n <= 0 || returns == NULL) return 0.0;
    if (alpha <= 0.0) alpha = 0.95;
    if (alpha >= 1.0) alpha = 0.999;
    r = (double *)malloc((size_t)n * sizeof(double));
    if (!r) return 0.0;
    memcpy(r, returns, (size_t)n * sizeof(double));
    qsort(r, (size_t)n, sizeof(double), cmp_dbl_desc); /* perdite decrescenti */
    tail = (int)ceil((1.0 - alpha) * n);
    if (tail < 1) tail = 1;
    for (k = 0; k < tail; k++) sum += r[k];
    if (var) *var = r[tail - 1];
    free(r);
    return sum / tail;
}

/* ============================== DES M/M/1 ============================== */

static double exp_rng(unsigned long *s, double rate)
{
    double u = (double)(mc_rng(s) % 1000000UL) / 1000000.0 + 1e-12;
    return -log(u) / rate;
}

gor_real gor_des_mm1(gor_real lambda, gor_real mu, int ncust, unsigned long seed, gor_real *util)
{
    unsigned long s = seed ? seed : 1;
    double t_arrival = 0.0, t_free = 0.0, total_wait = 0.0, busy = 0.0, t_last = 0.0;
    int i;
    if (lambda <= 0.0 || mu <= 0.0 || ncust <= 0) { if (util) *util = 0; return 0.0; }
    for (i = 0; i < ncust; i++) {
        double svc, start, wait;
        t_arrival += exp_rng(&s, lambda);
        svc = exp_rng(&s, mu);
        start = t_arrival > t_free ? t_arrival : t_free;
        wait = start - t_arrival;
        total_wait += wait;
        busy += svc;
        t_free = start + svc;
        t_last = t_free;
    }
    if (util) *util = (t_last > 0) ? busy / t_last : 0.0;
    return total_wait / (double)ncust;
}
/* ============================== chance constraint ============================== */

gor_real gor_chance_constraint(int n, const gor_real *sigma, const gor_real *x,
                               gor_real alpha, gor_real *shift)
{
    double z, var = 0.0;
    int i;
    if (n <= 0 || sigma == NULL || x == NULL) { if (shift) *shift = 0; return 0.0; }
    z = norm_ppf_u(1.0 - alpha);
    for (i = 0; i < n; i++) { double s = sigma[i] * x[i]; var += s * s; }
    if (shift) *shift = z;
    return z * sqrt(var);
}
