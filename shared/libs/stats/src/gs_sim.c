/* gs_sim.c — supporto simulazione (riscrittura). */
#include "gs_sim.h"
#include "gm_linalg.h"
#include "gs_dist2.h"
#include <stdlib.h>

#include <math.h>

gs_rng gs_rng_make(unsigned long long seed)
{
    gs_rng r;
    r.s = seed ? seed : 0x9E3779B97F4A7C15ULL;
    return r;
}

/* xorshift64* */
static unsigned long long next_u64(gs_rng *r)
{
    unsigned long long x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

gc_real gs_rng_uniform(gs_rng *r)
{
    return (gc_real)(next_u64(r) >> 11) / 9007199254740992.0; /* 2^53 */
}

int gs_rng_int(gs_rng *r, int n)
{
    if (n <= 0) return 0;
    return (int)(next_u64(r) % (unsigned long long)n);
}

gc_real gs_rng_normal(gs_rng *r)
{
    gc_real u1 = gs_rng_uniform(r);
    gc_real u2 = gs_rng_uniform(r);
    if (u1 < 1e-300) u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(6.28318530717958647692 * u2);
}

int gs_rng_bernoulli(gs_rng *r, gc_real p)
{
    return (gs_rng_uniform(r) < p) ? 1 : 0;
}

gc_status gs_hist(const gc_real *x, int n, gc_real xmin, gc_real xmax, int kbin, gc_real *counts)
{
    int i;
    if (x == NULL || counts == NULL || kbin <= 0 || xmax <= xmin) return GC_ERR_ARG;
    for (i = 0; i < kbin; i++) counts[i] = 0.0;
    for (i = 0; i < n; i++) {
        int b = (int)((x[i] - xmin) / (xmax - xmin) * (gc_real)kbin);
        if (b < 0) b = 0;
        if (b >= kbin) b = kbin - 1;
        counts[b] += 1.0;
    }
    return GC_OK;
}

void gs_sim_shuffle(void **v, int n, gs_rng *r)
{
    int i;
    for (i = n - 1; i > 0; i--) {
        int j = gs_rng_int(r, i + 1);
        void *t = v[i]; v[i] = v[j]; v[j] = t;
    }
}

void gs_sim_sample(void **s, int n, void **d, int m, gs_rng *r)
{
    void **copy;
    int i;
    if (s == NULL || d == NULL || m <= 0 || n <= 0) return;
    copy = (void **)malloc((size_t)n * sizeof(void *));
    if (!copy) return;
    for (i = 0; i < n; i++) copy[i] = s[i];
    gs_sim_shuffle(copy, n, r);
    for (i = 0; i < m && i < n; i++) d[i] = copy[i];
    free(copy);
}

gc_real gs_sim_autocorr(const gc_real *x, int n, int lag)
{
    gc_real m = 0.0, num = 0.0, den = 0.0;
    int i;
    if (x == NULL || n <= lag || lag < 0) return 0.0;
    for (i = 0; i < n; i++) m += x[i];
    m /= n;
    for (i = 0; i < n - lag; i++) num += (x[i] - m) * (x[i + lag] - m);
    for (i = 0; i < n; i++) den += (x[i] - m) * (x[i] - m);
    return (den != 0.0) ? num / den : 0.0;
}

/* ---- MRG32k3a (L'Ecuyer 1999), AI/Sources clean-room ---- */

static const double GS_M1 = 4294967087.0, GS_M2 = 4294944443.0;

void gs_mrg32k3a_init(gs_mrg *g, unsigned long a, unsigned long b)
{
    int i;
    if (!g) return;
    for (i = 0; i < 3; i++) { g->s1[i] = a ? (double)a : 12345.0; g->s2[i] = b ? (double)b : 12345.0; }
}

gc_real gs_mrg32k3a_next(gs_mrg *g)
{
    double s1n, s2n, p;
    if (!g) return 0.0;
    s1n = fmod(1403580.0 * g->s1[1] + 810728.0 * g->s1[0], GS_M1);
    s2n = fmod(527612.0 * g->s2[2] + 1370589.0 * g->s2[0], GS_M2);
    g->s1[0] = g->s1[1]; g->s1[1] = g->s1[2]; g->s1[2] = s1n;
    g->s2[0] = g->s2[1]; g->s2[1] = g->s2[2]; g->s2[2] = s2n;
    p = s1n - s2n;
    if (p < 0.0) p += GS_M1;
    return p / GS_M1;
}

/* ---- Ziggurat normal RNG (Marsaglia 256-layer, da generic_math). ---- */
static unsigned long long gs_splitmix64(unsigned long long *z)
{
    *z += 0x9e3779b97f4a7c15ULL;
    unsigned long long r = *z;
    r = (r ^ (r >> 30)) * 0xbf58476d1ce4e5b9ULL;
    r = (r ^ (r >> 27)) * 0x94d049bb133111ebULL;
    return r ^ (r >> 31);
}
static unsigned long long gs_xoshiro_next(unsigned long long s[4])
{
    unsigned long long rotl = (s[1] << 7) | (s[1] >> 57);
    unsigned long long result = rotl * 9;
    unsigned long long t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = (s[3] << 45) | (s[3] >> 19);
    return result;
}
static double gs_z_uniform(gs_ziggurat *g)
{
    return (double)(gs_xoshiro_next(g->s) >> 11) * 0x1.0p-53;
}
static double gs_z_f(double x) { return exp(-0.5 * x * x); }
static double gs_z_finv(double y)
{
    return y >= 0.9 ? sqrt(-2.0 * log1p(y - 1.0)) : sqrt(-2.0 * log(y));
}
static double gs_z_volume(double r)
{
    return r * gs_z_f(r) + 1.2533141373155003 * erfc(r * 0.7071067811865476);
}

void gs_ziggurat_init(gs_ziggurat *g, unsigned long long seed)
{
    const int N = 256;
    double r_lo, r_hi, r_, v;
    unsigned long long z = seed;
    int i;
    if (!g) return;
    /* bisection per r (chiusura) */
    r_lo = 3.0; r_hi = 5.0;
    for (i = 0; i < 64; i++) {
        double r_mid = 0.5 * (r_lo + r_hi);
        double vv = gs_z_volume(r_mid);
        double yi = gs_z_f(r_mid), xi = r_mid;
        int j;
        double residual;
        for (j = 0; j < N - 2; j++) {
            yi += vv / xi;
            if (yi >= 1.0) break;
            xi = gs_z_finv(yi);
        }
        residual = yi + vv / xi - 1.0;
        if (residual > 0) r_lo = r_mid; else r_hi = r_mid;
    }
    r_ = 0.5 * (r_lo + r_hi);
    v = gs_z_volume(r_);
    g->r = r_;
    g->A = v;
    g->x[0] = r_;
    g->y[0] = gs_z_f(r_);
    for (i = 1; i <= N - 2; i++) {
        double yi = g->y[i - 1] + v / g->x[i - 1];
        if (yi > 1.0) yi = 1.0;
        g->y[i] = yi;
        g->x[i] = gs_z_finv(yi);
    }
    g->x[N - 1] = 0.0;
    g->y[N - 1] = g->y[N - 2] + v / g->x[N - 2];
    if (g->y[N - 1] > 1.0) g->y[N - 1] = 1.0;
    {
        const double scale = 1.0 / 9223372036854775808.0; /* 2^63 */
        double base_width = v / g->y[0];
        g->wtab[0] = base_width * scale;
        g->ktab[0] = (unsigned long long)((r_ / base_width) * 9223372036854775808.0);
        for (i = 1; i < N - 1; i++) {
            g->wtab[i] = g->x[i - 1] * scale;
            g->ktab[i] = (unsigned long long)((g->x[i] / g->x[i - 1]) * 9223372036854775808.0);
        }
        g->wtab[N - 1] = g->x[N - 2] * scale;
        g->ktab[N - 1] = 0;
    }
    /* xoshiro256** seed */
    g->s[0] = gs_splitmix64(&z);
    g->s[1] = gs_splitmix64(&z);
    g->s[2] = gs_splitmix64(&z);
    g->s[3] = gs_splitmix64(&z);
}

gc_real gs_ziggurat_next(gs_ziggurat *g)
{
    const int N = 256;
    if (!g) return 0.0;
    for (;;) {
        long long u = (long long)gs_xoshiro_next(g->s);
        int i = (int)((unsigned long long)u & 0xFF);
        double x = (double)u * g->wtab[i];
        unsigned long long abs_s = u < 0 ? (unsigned long long)(-u) : (unsigned long long)u;
        if (abs_s < g->ktab[i])
            return x;
        if (i == 0) {
            double abs_x = fabs(x);
            if (abs_x < g->r)
                return x;
            for (;;) {
                double u1 = gs_z_uniform(g), u2 = gs_z_uniform(g);
                double tail_x = -log(u1) / g->r + g->r;
                if (-2.0 * log(u2) >= (tail_x - g->r) * (tail_x - g->r))
                    return (u < 0) ? -tail_x : tail_x;
            }
        }
        {
            double abs_x = fabs(x);
            double y_lo = g->y[i - 1];
            double y_hi = (i < N - 1) ? g->y[i] : 1.0;
            double y_test = y_lo + gs_z_uniform(g) * (y_hi - y_lo);
            if (y_test < exp(-0.5 * abs_x * abs_x))
                return x;
        }
    }
}

/* GBM bridge esatto (generic_math #211): campiona S_t | S_0, S_T. */
gc_real gs_mc_gmb_bridge(gc_real s0, gc_real st, gc_real t, gc_real T,
                         gc_real sigma, gc_real z)
{
    gc_real u;
    if (s0 <= 0.0 || st <= 0.0 || t <= 0.0 || t >= T || sigma < 0.0) return 0.0 / 0.0;
    u = t / T;
    return exp((1.0 - u) * log(s0) + u * log(st)
               + z * sigma * sqrt(T * u * (1.0 - u)));
}

/* Poisson(lam): PTRS (Hörmann 1993) per lam>=10, altrimenti Knuth. */
int gs_rng_poisson(gs_rng *r, gc_real lam)
{
    if (!(lam >= 0.0) || lam > 1e9) return 0;
    if (lam < 1e-12) return 0;
    if (lam < 10.0) {
        double prod = 1.0, t = exp(-lam);
        int k = 0;
        while (k < 1024) {
            prod *= gs_rng_uniform(r);
            if (prod <= t) break;
            k++;
        }
        return k;
    } else {
        double b = 0.931 + 2.53 * sqrt(lam);
        double a = -0.059 + 0.02483 * b, inva = 1.1239 + 1.1328 / (b - 3.4);
        double vr = 0.9277 - 3.6224 / (b - 2.0);
        for (;;) {
            double U = gs_rng_uniform(r) - 0.5, V = gs_rng_uniform(r);
            double us = 0.5 - fabs(U);
            int k = (int)floor((2.0 * a / us + b) * U + lam + 0.43);
            if (us >= 0.07 && V <= vr) return k;
            if (k < 0 || (us < 0.013 && V > us)) continue;
            if (log(V) + log(inva) - log(a / (us * us) + b) <=
                lam + k * log(b + 1.0) - lgamma((double)k + 1.0))
                return k;
        }
    }
}

/* Gamma(shape, rate): Marsaglia-Tsang 2000. */
gc_real gs_rng_gamma(gs_rng *r, gc_real shape, gc_real rate)
{
    double a;
    if (shape <= 0.0 || rate <= 0.0) return 0.0 / 0.0;
    a = shape;
    if (a < 1.0) {
        double u = gs_rng_uniform(r);
        if (u < 1e-300) u = 1e-300;
        return gs_rng_gamma(r, a + 1.0, rate) * pow(u, 1.0 / a);
    }
    {
        double d = a - 1.0 / 3.0, c = 1.0 / sqrt(9.0 * d), x, v, u;
        for (;;) {
            x = gs_rng_normal(r);
            v = 1.0 + c * x;
            if (v <= 0.0) continue;
            v = v * v * v;
            u = gs_rng_uniform(r);
            if (u < 1.0 - 0.0331 * x * x * x * x) return d * v / rate;
            if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) return d * v / rate;
        }
    }
}

/* Bessel discreta Bes(mu,b): P{n} = (b/2)^(2n+mu) / (I_mu(b) n! Gamma(n+mu+1)).
 * Campionamento per inversione della CDF calcolata con ricorrenza esatta. */
static int bessel_variate(gs_rng *r, double mu, double b)
{
    double t, tot, u, acc;
    int n, cap;
    if (b < 1e-15) return 0;
    t = pow(0.5 * b, mu) / tgamma(mu + 1.0);
    tot = t;
    for (n = 1; n < 1000000; n++) {
        t *= (0.5 * b) * (0.5 * b) / ((double)n * ((double)n + mu));
        if (t / tot < 1e-16 && t < 1e-300) break;
        tot += t;
    }
    cap = n;
    u = gs_rng_uniform(r) * tot;
    acc = 0.0;
    t = pow(0.5 * b, mu) / tgamma(mu + 1.0);
    for (n = 0; n < cap; n++) {
        acc += t;
        if (u <= acc) return n;
        t *= (0.5 * b) * (0.5 * b) / ((double)(n + 1) * ((double)n + 1.0 + mu));
    }
    return cap - 1;
}

/* CIR bridge esatto via time-change del ponte SQB (Makarov-Glew 2010):
 * CIR -> SQB con X_t = e^(k*t) * Y_t a tempo s(t)=(e^(k*t)-1)/k; scala nu=2. */
gc_real gs_mc_cir_bridge(gc_real r0, gc_real rT, gc_real t, gc_real T,
                         gc_real kappa, gc_real theta, gc_real sigma, gs_rng *r)
{
    double mu, S, st, scale, x, z, lamP, bB, Y, Z, shape, rate, Xt2, r_t;
    if (r0 < 0.0 || rT < 0.0 || t <= 0.0 || t >= T || kappa < 0.0 || sigma <= 0.0)
        return 0.0 / 0.0;
    mu = 2.0 * kappa * theta / (sigma * sigma) - 1.0;
    if (mu <= -1.0) return 0.0 / 0.0;
    S = kappa > 1e-12 ? expm1(kappa * T) / kappa : T;
    st = kappa > 1e-12 ? expm1(kappa * t) / kappa : t;
    scale = (2.0 / sigma) * (2.0 / sigma);
    x = scale * r0;
    z = scale * exp(kappa * T) * rT;
    lamP = (1.0 / (2.0 * S)) * ((S - st) / st * x + st / (S - st) * z);
    bB = sqrt(x * z) / S;
    Y = gs_rng_poisson(r, lamP);
    Z = bessel_variate(r, mu, bB);
    shape = Y + 2.0 * Z + mu + 1.0;
    rate = S / (2.0 * st * (S - st));
    Xt2 = gs_rng_gamma(r, shape, rate);
    r_t = exp(-kappa * t) * (Xt2 / scale);
    return r_t;
}

/* G2++ bridge esatto: condizionamento gaussiano bivariato (Kalman). */
gc_status gs_mc_g2pp_bridge(gc_real x1, gc_real y1, gc_real x2, gc_real y2,
                            gc_real t1, gc_real tk, gc_real t2,
                            gc_real a, gc_real b, gc_real sigma, gc_real eta,
                            gc_real rho, gs_rng *r, gc_real *x_out, gc_real *y_out)
{
    double dt1, dt2, dtT, ea, eb;
    double vx1, vy1, cxy1, vxT, vyT, cxyT;
    double c00, c01, c10, c11, k00, k01, k10, k11, m0, m1, q, det;
    double bm0, bm1, p, s, l11, l21, l22, z0, z1;
    if (x_out == NULL || y_out == NULL || tk <= t1 || tk >= t2 || r == NULL)
        return GC_ERR_ARG;
    if (a < 0.0 || b < 0.0 || sigma < 0.0 || eta < 0.0 || rho < -1.0 || rho > 1.0)
        return GC_ERR_ARG;
    dt1 = tk - t1; dt2 = t2 - tk; dtT = t2 - t1;
    /* varianze/covarianze forward (stabili per a,b->0). */
    ea = -expm1(-2.0 * a * dt1) / (2.0 * a);
    eb = -expm1(-2.0 * b * dt1) / (2.0 * b);
    vx1 = sigma * sigma * ea;
    vy1 = eta * eta * eb;
    cxy1 = rho * sigma * eta * (-expm1(-(a + b) * dt1)) / (a + b);
    ea = -expm1(-2.0 * a * dtT) / (2.0 * a);
    eb = -expm1(-2.0 * b * dtT) / (2.0 * b);
    vxT = sigma * sigma * ea;
    vyT = eta * eta * eb;
    cxyT = rho * sigma * eta * (-expm1(-(a + b) * dtT)) / (a + b);
    /* cross-cov Cov[state_tk, state_t2]. */
    c00 = vx1 * exp(-a * dt2);
    c11 = vy1 * exp(-b * dt2);
    c01 = cxy1 * exp(-b * dt2);   /* Cov[x_tk, y_t2] */
    c10 = cxy1 * exp(-a * dt2);   /* Cov[y_tk, x_t2] */
    /* Var_t2^{-1} (simmetrica). */
    det = vxT * vyT - cxyT * cxyT;
    if (det <= 0.0) return GC_ERR_ARG;
    {
        double iv00 = vyT / det, iv01 = -cxyT / det, iv11 = vxT / det;
        /* K = C * Var_t2^{-1}. */
        k00 = c00 * iv00 + c01 * iv01;
        k01 = c00 * iv01 + c01 * iv11;
        k10 = c10 * iv00 + c11 * iv01;
        k11 = c10 * iv01 + c11 * iv11;
    }
    /* media bridge. */
    m0 = x1 * exp(-a * dtT);
    m1 = y1 * exp(-b * dtT);
    bm0 = x1 * exp(-a * dt1) + k00 * (x2 - m0) + k01 * (y2 - m1);
    bm1 = y1 * exp(-b * dt1) + k10 * (x2 - m0) + k11 * (y2 - m1);
    /* cov bridge = Var_tk - K*C^T. */
    p = vx1 - (k00 * c00 + k01 * c10);
    q = cxy1 - (k00 * c01 + k01 * c11);
    s = vy1 - (k10 * c01 + k11 * c11);
    if (p <= 0.0 || (p * s - q * q) <= 0.0) return GC_ERR_ARG;
    l11 = sqrt(p);
    l21 = q / l11;
    l22 = sqrt(s - l21 * l21);
    z0 = gs_rng_normal(r);
    z1 = gs_rng_normal(r);
    *x_out = bm0 + l11 * z0;
    *y_out = bm1 + l21 * z0 + l22 * z1;
    return GC_OK;
}

gc_status gs_sde_euler(gs_sde_drift drift, gs_sde_diff diff, void *ctx,
                       gc_real x0, gc_real t0, gc_real t1, int nsteps,
                       int npaths, gs_rng *rng, gc_real *out)
{
    gc_real dt, x;
    int i, j;
    if (!drift || !diff || !out || nsteps < 1 || npaths < 1 || t1 <= t0) return GC_ERR_ARG;
    dt = (t1 - t0) / (gc_real)nsteps;
    for (j = 0; j < npaths; j++) {
        x = x0;
        out[j * (nsteps + 1)] = x;
        for (i = 0; i < nsteps; i++) {
            gc_real t = t0 + (gc_real)i * dt;
            gc_real dW = sqrt(dt) * gs_rng_normal(rng);
            x = x + drift(x, t, ctx) * dt + diff(x, t, ctx) * dW;
            out[j * (nsteps + 1) + i + 1] = x;
        }
    }
    return GC_OK;
}

gc_status gs_sde_milstein(gs_sde_drift drift, gs_sde_diff diff, gs_sde_ddiff ddiff,
                          void *ctx, gc_real x0, gc_real t0, gc_real t1,
                          int nsteps, int npaths, gs_rng *rng, gc_real *out)
{
    gc_real dt, x;
    int i, j;
    if (!drift || !diff || !ddiff || !out || nsteps < 1 || npaths < 1 || t1 <= t0) return GC_ERR_ARG;
    dt = (t1 - t0) / (gc_real)nsteps;
    for (j = 0; j < npaths; j++) {
        x = x0;
        out[j * (nsteps + 1)] = x;
        for (i = 0; i < nsteps; i++) {
            gc_real t = t0 + (gc_real)i * dt;
            gc_real dW = sqrt(dt) * gs_rng_normal(rng);
            gc_real s = diff(x, t, ctx);
            x = x + drift(x, t, ctx) * dt + s * dW
                + 0.5 * s * ddiff(x, t, ctx) * (dW * dW - dt);
            out[j * (nsteps + 1) + i + 1] = x;
        }
    }
    return GC_OK;
}

gc_status gs_sde_milstein_df(gs_sde_drift drift, gs_sde_diff diff, void *ctx,
                             gc_real x0, gc_real t0, gc_real t1, int nsteps,
                             int npaths, gs_rng *rng, gc_real *out)
{
    gc_real dt, x;
    int i, j;
    if (!drift || !diff || !out || nsteps < 1 || npaths < 1 || t1 <= t0) return GC_ERR_ARG;
    dt = (t1 - t0) / (gc_real)nsteps;
    for (j = 0; j < npaths; j++) {
        x = x0;
        out[j * (nsteps + 1)] = x;
        for (i = 0; i < nsteps; i++) {
            gc_real t = t0 + (gc_real)i * dt;
            gc_real dW = sqrt(dt) * gs_rng_normal(rng);
            gc_real s = diff(x, t, ctx);
            gc_real sx = diff(x + s * sqrt(dt), t, ctx);
            gc_real ds = (sx - s) / sqrt(dt);   /* approssimazione derivata */
            x = x + drift(x, t, ctx) * dt + s * dW
                + 0.5 * s * ds * (dW * dW - dt);
            out[j * (nsteps + 1) + i + 1] = x;
        }
    }
    return GC_OK;
}

int gs_copula_t(const double *corr, int n, double df, unsigned seed, double *out)
{
    double *L, *z, *y;
    double chi2 = 0.0;
    int i, j, k;
    unsigned s = seed ? seed : 12345u;
    if (!corr || !out || n < 1 || df <= 0) return GC_ERR_ARG;
    L = (double *)malloc((size_t)n * n * sizeof(double));
    z = (double *)malloc((size_t)n * sizeof(double));
    y = (double *)malloc((size_t)n * sizeof(double));
    if (!L || !z || !y) { free(L); free(z); free(y); return GC_ERR_NO_MEM; }
    for (i = 0; i < n * n; i++) L[i] = corr[i];
    if (gm_linalg_cholesky(n, L) != 0) { free(L); free(z); free(y); return GC_ERR_SINGULAR; }
    /* chi2 con df gradi di liberta' (Box-Muller basato) */
    {
        int halves = (int)df;
        double d = df - halves;
        double s2 = 0.0;
        for (k = 0; k < halves; k++) {
            double u1 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            double u2 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            double z2 = sqrt(-2.0 * log(u1 > 0 ? u1 : 1e-9)) * cos(2.0 * 3.14159265358979 * u2);
            s2 += z2 * z2;
        }
        if (d > 0.0) {
            double u1 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            double u2 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            double z2 = sqrt(-2.0 * log(u1 > 0 ? u1 : 1e-9)) * cos(2.0 * 3.14159265358979 * u2);
            s2 += z2 * z2;
        }
        chi2 = s2;
    }
    for (i = 0; i < n; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        z[i] = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
    }
    /* y = L z */
    for (i = 0; i < n; i++) {
        y[i] = 0.0;
        for (j = 0; j < n; j++) y[i] += L[i * n + j] * z[j];
    }
    for (i = 0; i < n; i++) {
        double c = sqrt(chi2 / df);
        double g = y[i] / c;
        double u1 = gs_dist2_nc_t_cdf(g, df, 0.0);
        out[i] = u1;
    }
    free(L); free(z); free(y);
    return GC_OK;
}

int gs_fgn_hosking(double hurst, int n, unsigned seed, double *x)
{
    double *c, *phi, *psi;
    double e;
    int k, j;
    unsigned s = seed ? seed : 12345u;
    if (!x || n < 1 || hurst <= 0.0 || hurst >= 1.0) return GC_ERR_ARG;
    c = (double *)malloc((size_t)n * sizeof(double));
    phi = (double *)malloc((size_t)n * sizeof(double));
    psi = (double *)malloc((size_t)n * sizeof(double));
    if (!c || !phi || !psi) { free(c); free(phi); free(psi); return GC_ERR_NO_MEM; }
    for (k = 0; k < n; k++) {
        double km1 = k > 0 ? pow((double)(k - 1), 2.0 * hurst) : 0.0;
        double kp1 = pow((double)(k + 1), 2.0 * hurst);
        double kk = pow((double)k, 2.0 * hurst);
        c[k] = k == 0 ? 1.0 : (kp1 - 2.0 * kk + km1) / 2.0;
    }
    e = c[0];
    {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        x[0] = sqrt(e) * sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
    }
    for (k = 1; k < n; k++) {
        double sum = 0.0, e_new;
        for (j = 1; j <= k - 1; j++) sum += phi[j - 1] * c[k - j];
        phi[k - 1] = (c[k] - sum) / e;
        for (j = 1; j <= k - 1; j++) psi[j - 1] = phi[j - 1] - phi[k - 1] * phi[k - 1 - j];
        e_new = e * (1.0 - phi[k - 1] * phi[k - 1]);
        for (j = 1; j <= k - 1; j++) phi[j - 1] = psi[j - 1];
        /* x[k] = sum_j phi[j-1]*x[k-j] + sqrt(e_new)*z */
        {
            double xk = 0.0;
            for (j = 1; j <= k - 1; j++) xk += phi[j - 1] * x[k - j];
            {
                double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
                double z = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
                x[k] = xk + sqrt(e_new > 0 ? e_new : 0.0) * z;
            }
        }
        e = e_new;
    }
    free(c); free(phi); free(psi);
    return GC_OK;
}

double gs_rand_inverse_gaussian(double mean, double scale, unsigned *seed)
{
    double u, x, y, c;
    unsigned s = seed ? *seed : 12345u;
    u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    y = gs_dist2_normal_quantile(u);
    y *= y;
    x = mean + 0.5 * mean * mean * y / scale
        - 0.5 * mean / scale * sqrt(4.0 * mean * scale * y + mean * mean * y * y);
    c = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    if (seed) *seed = s;
    return c <= mean / (mean + x) ? x : mean * mean / x;
}

int gs_process_poisson(double rate, int n, unsigned seed, double *times)
{
    unsigned s = seed ? seed : 12345u;
    double t = 0.0;
    int i;
    if (!times || n < 1 || rate <= 0) return GC_ERR_ARG;
    for (i = 0; i < n; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        t += -log(u > 0 ? u : 1e-9) / rate;
        times[i] = t;
    }
    return GC_OK;
}

int gs_process_cev(double x0, double mu, double sigma, double gamma, double t0,
                   double t1, int nsteps, unsigned seed, double *path)
{
    double dt, x = x0;
    unsigned s = seed ? seed : 12345u;
    int i;
    if (!path || nsteps < 1 || t1 <= t0 || sigma < 0 || x0 < 0) return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    path[0] = x;
    for (i = 1; i <= nsteps; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        double z = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
        x = x + mu * x * dt + sigma * pow(x > 0 ? x : 0.0, gamma) * sqrt(dt) * z;
        path[i] = x;
    }
    return GC_OK;
}

int gs_process_sq_bessel(double x0, double a, double t0, double t1, int nsteps,
                         unsigned seed, double *path)
{
    double dt, x = x0;
    unsigned s = seed ? seed : 12345u;
    int i;
    if (!path || nsteps < 1 || t1 <= t0 || x0 < 0) return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    path[0] = x;
    for (i = 1; i <= nsteps; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        double z = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
        x = x + a * dt + 2.0 * sqrt(x > 0 ? x : 0.0) * sqrt(dt) * z;
        path[i] = x;
    }
    return GC_OK;
}

int gs_brownian_bridge(double wT, double b, double t0, double t1, int nsteps,
                       unsigned seed, double *path)
{
    double dt, T = t1 - t0, Wbm = 0.0;
    unsigned s = seed ? seed : 12345u;
    int i;
    if (!path || nsteps < 1 || T <= 0) return GC_ERR_ARG;
    dt = T / nsteps;
    path[0] = 0.0;
    for (i = 1; i <= nsteps; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        double z = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
        Wbm += sqrt(dt) * z;
        path[i] = Wbm - (double)i * dt / T * wT + (double)i * dt / T * b;
    }
    return GC_OK;
}

/* chi-quadro(df): chi2 = 2*gamma(shape=df/2, scale=1) via Marsaglia-Tsang. */
static double gs_chi2_sample(unsigned *s, double df)
{
    double a = df / 2.0;
    if (a < 1.0) {
        /* gamma(a) = gamma(a+1) * u^(1/a) */
        double u;
        a += 1.0;
        u = (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
        return 2.0 * gs_chi2_sample(s, df + 2.0) * pow(u > 0 ? u : 1e-9, 1.0 / a);
    }
    {
        double d = a - 1.0 / 3.0, c = 1.0 / sqrt(9.0 * d), v;
        int guard = 0;
        do {
            double x1, u1;
            do {
                double u = (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
                x1 = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0);
            } while (x1 <= -1.0 / c);
            v = (1.0 + c * x1);
            v = v * v * v;
            u1 = (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
            if (log(u1 > 0 ? u1 : 1e-9) < 0.5 * x1 * x1 + d * (1.0 - v + log(v))) break;
        } while (++guard < 10000);
        return 2.0 * d * v;
    }
}

/* Poisson(lam) via Knuth (per lam moderati). */
static int gs_poisson_sample(unsigned *s, double lam)
{
    double L = exp(-lam), p = 1.0;
    int k = 0;
    if (lam <= 0.0) return 0;
    do {
        double u = (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
        p *= u > 0 ? u : 1e-9;
        k++;
    } while (p > L);
    return k - 1;
}

/* CIR/SRD discretizzazione ESATTA (campionamento noncentral-chi2, DAW):
 *   d = 4 kappa theta / sigma^2, c = sigma^2 (1-e^{-kappa dt})/(4 kappa),
 *   lambda_t = x e^{-kappa dt}/c.
 *   d > 1 : x = c ((z + sqrt(lambda))^2 + chi2(d-1))
 *   else  : x = c chi2(d + 2 Poisson(lambda/2))
 * path[0..nsteps]. */
int gs_process_cir_exact(double x0, double kappa, double theta, double sigma,
                         double t0, double t1, int nsteps, unsigned seed, double *path)
{
    double dt, d, c, x = x0;
    unsigned s = seed ? seed : 12345u;
    int i;
    if (!path || nsteps < 1 || t1 <= t0 || kappa <= 0 || theta <= 0 || sigma <= 0 || x0 < 0)
        return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    d = 4.0 * kappa * theta / (sigma * sigma);
    c = sigma * sigma * (1.0 - exp(-kappa * dt)) / (4.0 * kappa);
    if (c <= 0.0) return GC_ERR_ARG;
    path[0] = x;
    for (i = 1; i <= nsteps; i++) {
        double lam = x * exp(-kappa * dt) / c;
        if (d > 1.0) {
            double u1 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            double u2 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            double z = sqrt(-2.0 * log(u1 > 0 ? u1 : 1e-9)) * cos(2.0 * 3.14159265358979 * u2);
            x = c * ((z + sqrt(lam)) * (z + sqrt(lam)) + gs_chi2_sample(&s, d - 1.0));
        } else {
            int npois = gs_poisson_sample(&s, lam / 2.0);
            x = c * gs_chi2_sample(&s, d + 2.0 * npois);
        }
        path[i] = x;
    }
    return GC_OK;
}

/* CIR/SRD via 7 schemi Euler (DAW/H93_european_mcs):
 *   scheme 0 Full Truncation, 1 Partial Truncation, 2 Truncation,
 *          3 Reflection, 4 Higham-Mao, 5 Simple Reflection, 6 Absorption. */
int gs_process_cir_schemes(double x0, double kappa, double theta, double sigma,
                           double t0, double t1, int nsteps, int scheme,
                           unsigned seed, double *path)
{
    double dt, sdt, x = x0, xh = x0;
    unsigned s = seed ? seed : 12345u;
    int i;
    if (!path || nsteps < 1 || t1 <= t0 || kappa <= 0 || theta <= 0 || sigma <= 0
        || x0 < 0 || scheme < 0 || scheme > 6)
        return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    sdt = sqrt(dt);
    path[0] = x;
    for (i = 1; i <= nsteps; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        double z = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0);
        double v = x, vh = xh;
        double xm = v > 0 ? v : 0.0, xhm = vh > 0 ? vh : 0.0, xha = vh > 0 ? vh : -vh;
        switch (scheme) {
        case 0: xh = xhm + kappa * (theta - xhm) * dt + sqrt(xhm) * sigma * z * sdt; x = xh > 0 ? xh : 0.0; break;
        case 1: xh = xh + kappa * (theta - vh) * dt + sqrt(xhm) * sigma * z * sdt; x = xh > 0 ? xh : 0.0; break;
        case 2: x = x > 0 ? (v + kappa * (theta - v) * dt + sqrt(xm) * sigma * z * sdt) : 0.0; break;
        case 3: xh = xh + kappa * (theta - xha) * dt + sqrt(xha) * sigma * z * sdt; x = xh > 0 ? xh : -xh; break;
        case 4: xh = xh + kappa * (theta - vh) * dt + sqrt(xha) * sigma * z * sdt; x = xh > 0 ? xh : -xh; break;
        case 5: x = v + kappa * (theta - v) * dt + sqrt(xm) * sigma * z * sdt; x = x > 0 ? x : -x; break;
        default: xh = xhm + kappa * (theta - xhm) * dt + sqrt(xhm) * sigma * z * sdt; x = xh > 0 ? xh : 0.0; break;
        }
        path[i] = x;
    }
    return GC_OK;
}

/* Jump-diffusion (Merton M76) log-Euler, DAW M76_valuation_MCS:
 *   S_t = S_{t-1} (exp((r - rj - 0.5 sigma^2) dt + sigma sqrt(dt) z)
 *                   + (exp(mu + delta z') - 1) N),  rj = lambda(e^{mu+0.5 delta^2}-1),
 *   N ~ Poisson(lambda dt). path[0..nsteps]. */
int gs_process_jump_diffusion(double s0, double r, double sigma, double lamb,
                              double mu, double delta, double t0, double t1,
                              int nsteps, unsigned seed, double *path)
{
    double dt, sdt, rj, s = s0;
    unsigned sd = seed ? seed : 12345u;
    int i;
    if (!path || nsteps < 1 || t1 <= t0 || s0 <= 0 || sigma <= 0 || lamb < 0)
        return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    sdt = sqrt(dt);
    rj = lamb * (exp(mu + 0.5 * delta * delta) - 1.0);
    path[0] = s;
    for (i = 1; i <= nsteps; i++) {
        double u = (double)((sd = sd * 1103515245u + 12345u) % 100000u) / 100000.0;
        double z = sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((sd = sd * 1103515245u + 12345u) % 100000u) / 100000.0);
        double u2 = (double)((sd = sd * 1103515245u + 12345u) % 100000u) / 100000.0;
        double zj = sqrt(-2.0 * log(u2 > 0 ? u2 : 1e-9)) * cos(2.0 * 3.14159265358979 * (double)((sd = sd * 1103515245u + 12345u) % 100000u) / 100000.0);
        int n = gs_poisson_sample(&sd, lamb * dt);
        s = s * (exp((r - rj - 0.5 * sigma * sigma) * dt + sigma * sdt * z)
                 + (exp(mu + delta * zj) - 1.0) * n);
        path[i] = s;
    }
    return GC_OK;
}

/* --- Simulatori processi (cartella simulators/) --- */
static double gs_sim_z(unsigned *s)
{
    double u = (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
    double v = (double)((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
    return sqrt(-2.0 * log(u > 0 ? u : 1e-9)) * cos(2.0 * 3.14159265358979 * v);
}

int gs_process_gbm(double s0, double mu, double sigma, double t0, double t1,
                   int nsteps, unsigned seed, double *path)
{
    double dt, s = s0; unsigned st = seed ? seed : 12345u; int i;
    if (!path || nsteps < 1 || t1 <= t0 || sigma < 0.0 || s0 <= 0.0) return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    path[0] = s;
    for (i = 1; i <= nsteps; i++) {
        s *= exp((mu - 0.5 * sigma * sigma) * dt + sigma * sqrt(dt) * gs_sim_z(&st));
        path[i] = s;
    }
    return GC_OK;
}

int gs_process_ou(double x0, double kappa, double theta, double sigma,
                  double t0, double t1, int nsteps, unsigned seed, double *path)
{
    double dt, x = x0, e, sd; unsigned st = seed ? seed : 12345u; int i;
    if (!path || nsteps < 1 || t1 <= t0 || sigma < 0.0 || kappa < 0.0) return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    e = exp(-kappa * dt);
    sd = kappa > 0.0 ? sigma * sqrt((1.0 - exp(-2.0 * kappa * dt)) / (2.0 * kappa)) : sigma * sqrt(dt);
    path[0] = x;
    for (i = 1; i <= nsteps; i++) {
        x = theta + (x - theta) * e + sd * gs_sim_z(&st);
        path[i] = x;
    }
    return GC_OK;
}

int gs_process_hull_white(double r0, double a, double sigma, double t0, double t1,
                          int nsteps, unsigned seed, double *path)
{
    double dt, r = r0, e, sd; unsigned st = seed ? seed : 12345u; int i;
    if (!path || nsteps < 1 || t1 <= t0 || sigma < 0.0 || a < 0.0) return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    e = exp(-a * dt);
    sd = a > 0.0 ? sigma * sqrt((1.0 - exp(-2.0 * a * dt)) / (2.0 * a)) : sigma * sqrt(dt);
    path[0] = r;
    for (i = 1; i <= nsteps; i++) { r = r * e + sd * gs_sim_z(&st); path[i] = r; }
    return GC_OK;
}

int gs_process_ho_lee(double r0, double theta, double sigma, double t0, double t1,
                      int nsteps, unsigned seed, double *path)
{
    double dt, r = r0; unsigned st = seed ? seed : 12345u; int i;
    if (!path || nsteps < 1 || t1 <= t0 || sigma < 0.0) return GC_ERR_ARG;
    dt = (t1 - t0) / nsteps;
    path[0] = r;
    for (i = 1; i <= nsteps; i++) { r = r + theta * dt + sigma * sqrt(dt) * gs_sim_z(&st); path[i] = r; }
    return GC_OK;
}

int gs_sim_levy_stable(double alpha, double beta, int n, unsigned seed, double *out)
{
    unsigned st = seed ? seed : 12345u; int i;
    if (!out || n < 1 || alpha <= 0.0 || alpha > 2.0 || fabs(beta) > 1.0) return GC_ERR_ARG;
    for (i = 0; i < n; i++) {
        double u = ((double)((st = st * 1103515245u + 12345u) % 100000u) / 100000.0) * 3.14159265358979 - 3.14159265358979 / 2.0;
        double w = (double)((st = st * 1103515245u + 12345u) % 100000u) / 100000.0;
        if (w < 1e-9) w = 1e-9;
        w = -log(w);
        if (fabs(alpha - 1.0) < 1e-9) {
            out[i] = (2.0 / 3.14159265358979) *
                     ((3.14159265358979 / 2.0 + beta * u) * tan(u) -
                      beta * log((3.14159265358979 / 2.0 * w * cos(u)) / (3.14159265358979 / 2.0 + beta * u)));
        } else {
            double zeta = beta * tan(3.14159265358979 * alpha / 2.0);
            double th0 = atan(zeta) / alpha;
            out[i] = (sin(alpha * (u + th0)) / pow(cos(u), 1.0 / alpha)) *
                     pow(cos(u - alpha * (u + th0)) / w, (1.0 - alpha) / alpha);
        }
    }
    return GC_OK;
}

int gs_process_fbm(double hurst, int n, unsigned seed, double *path)
{
    int i;
    if (!path || n < 1 || hurst <= 0.0 || hurst >= 1.0) return GC_ERR_ARG;
    if (gs_fgn_hosking(hurst, n, seed, path) != GC_OK) return GC_ERR_ARG;
    for (i = 1; i < n; i++) path[i] += path[i - 1];
    return GC_OK;
}

int gs_process_flsm(double hurst, double alpha, double beta, int n, unsigned seed, double *path)
{
    double *inc; int i, j;
    if (!path || n < 1 || hurst <= 0.0 || alpha <= 1.0 || alpha > 2.0) return GC_ERR_ARG;
    inc = (double *)malloc((size_t)n * sizeof(double));
    if (!inc) return GC_ERR_ARG;
    if (gs_sim_levy_stable(alpha, beta, n, seed, inc) != GC_OK) { free(inc); return GC_ERR_ARG; }
    path[0] = 0.0;
    for (i = 1; i < n; i++) {
        double s = 0.0, p = hurst - 1.0/alpha;
        for (j = 1; j <= i; j++) s += pow((double)j, p) * inc[i-j];
        path[i] = s;
    }
    free(inc);
    return GC_OK;
}

