/* gs_math.c — metodi da AI/Math (BO, MLE, gambler's ruin, causal). */
#include "gs_math.h"
#include "gs_num.h"
#include "gs_dist.h"
#include "gs_stats2.h"
#include "gs_regression.h"
#include "gs_sim.h"
#include "gm_special.h"
#include <math.h>
#include <stdlib.h>

static double _phi(double x) { return exp(-0.5 * x * x) / sqrt(2.0 * 3.14159265358979323846); }
static double _Phi(double x) { return gs_dist_normal_cdf(0.0, 1.0, x); }

/* ---- Bayesian optimization ---- */

double gs_bo_ei(double mu, double sd, double best, double xi)
{
    double d, z;
    if (sd <= 1e-300) { d = mu - best - xi; return d > 0.0 ? d : 0.0; }
    d = mu - best - xi;
    z = d / sd;
    return d * _Phi(z) + sd * _phi(z);
}

double gs_bo_pi(double mu, double sd, double best, double xi)
{
    if (sd <= 1e-300) return (mu - best - xi > 0.0) ? 1.0 : 0.0;
    return _Phi((mu - best - xi) / sd);
}

double gs_bo_ucb(double mu, double sd, double kappa) { return mu + kappa * sd; }

int gs_bo_next(const double *x, const double *y, int n,
               const double *cand, int m, double l2, double noise, double kappa,
               int *out_idx, double *out_val)
{
    double *mean, *var;
    int i, best = 0;
    double bv = -1e300;
    if (!x || !y || !cand || n < 2 || m < 1) return -1;
    mean = (double *)malloc(sizeof(double) * (size_t)m);
    var = (double *)malloc(sizeof(double) * (size_t)m);
    if (!mean || !var) { free(mean); free(var); return -1; }
    if (gs_gp_regression(x, y, n, cand, m, l2, noise, mean, var) != 0) {
        free(mean); free(var); return -1;
    }
    for (i = 0; i < m; i++) {
        double u = gs_bo_ucb(mean[i], sqrt(var[i] > 0.0 ? var[i] : 0.0), kappa);
        if (u > bv) { bv = u; best = i; }
    }
    if (out_idx) *out_idx = best;
    if (out_val) *out_val = bv;
    free(mean); free(var);
    return 0;
}

/* ---- MLE + model selection ---- */

double gs_gambler_ruin_prob(double i, double N, double p)
{
    double q, r;
    if (N <= 0.0 || i < 0.0 || i > N || p < 0.0 || p > 1.0) return 0.0 / 0.0;
    q = 1.0 - p;
    if (i == 0.0) return 1.0;
    if (i == N) return 0.0;
    if (fabs(p - 0.5) < 1e-12) return 1.0 - i / N;
    r = q / p;
    return (pow(r, i) - pow(r, N)) / (1.0 - pow(r, N));
}

double gs_gambler_ruin_duration(double i, double N, double p)
{
    double q, r;
    if (N <= 0.0 || i < 0.0 || i > N || p < 0.0 || p > 1.0) return 0.0 / 0.0;
    q = 1.0 - p;
    if (fabs(p - 0.5) < 1e-12) return i * (N - i);
    r = q / p;
    return i / (q - p) - (N / (q - p)) * (1.0 - pow(r, i)) / (1.0 - pow(r, N));
}

/* ---- Inferenza causale ---- */

int gs_propensity_score(const double *X, const double *t, int n, int p, double *out_ps)
{
    double *Xa, *beta;
    int i, j;
    if (!X || !t || !out_ps || n < 2 || p < 1) return -1;
    Xa = (double *)malloc(sizeof(double) * (size_t)n * (size_t)(p + 1));
    beta = (double *)malloc(sizeof(double) * (size_t)(p + 1));
    if (!Xa || !beta) { free(Xa); free(beta); return -1; }
    for (i = 0; i < n; i++) {
        Xa[i * (p + 1)] = 1.0;
        for (j = 0; j < p; j++) Xa[i * (p + 1) + j + 1] = X[i * p + j];
    }
    if (gs_logistic_regression(Xa, t, n, p + 1, beta, 200, 1e-8) != GC_OK) {
        free(Xa); free(beta); return -1;
    }
    for (i = 0; i < n; i++) {
        double eta = beta[0];
        for (j = 0; j < p; j++) eta += beta[j + 1] * X[i * p + j];
        out_ps[i] = 1.0 / (1.0 + exp(-eta));
    }
    free(Xa); free(beta);
    return 0;
}

double gs_ate_ipw(const double *t, const double *y, const double *ps, int n)
{
    int i;
    double s1 = 0.0, s0 = 0.0;
    if (!t || !y || !ps || n < 1) return 0.0 / 0.0;
    for (i = 0; i < n; i++) {
        double e = ps[i] < 1e-6 ? 1e-6 : (ps[i] > 1.0 - 1e-6 ? 1.0 - 1e-6 : ps[i]);
        s1 += t[i] * y[i] / e;
        s0 += (1.0 - t[i]) * y[i] / (1.0 - e);
    }
    return s1 / (double)n - s0 / (double)n;
}

double gs_ate_matching(const double *t, const double *y, const double *ps, int n)
{
    int i, j, nt = 0, nc = 0;
    double att = 0.0, atc = 0.0;
    if (!t || !y || !ps || n < 2) return 0.0 / 0.0;
    for (i = 0; i < n; i++) {
        int best = -1;
        double bd = 1e300;
        for (j = 0; j < n; j++) {
            if (t[j] == t[i]) continue;
            { double d = fabs(ps[i] - ps[j]); if (d < bd) { bd = d; best = j; } }
        }
        if (best < 0) continue;
        if (t[i] > 0.5) { att += y[i] - y[best]; nt++; }
        else { atc += y[best] - y[i]; nc++; }
    }
    att = (nt > 0) ? att / (double)nt : 0.0;
    atc = (nc > 0) ? atc / (double)nc : 0.0;
    if (nt > 0 && nc > 0) return 0.5 * (att + atc);
    return (nt > 0) ? att : atc;
}

/* ---- TPE (Tree-structured Parzen Estimator) ---- */

static double tpe_logpdf_dim(double x, double mu, double sd)
{
    double d = x - mu;
    return -0.5 * d * d / (sd * sd) - log(sd);
}

int gs_tpe(gs_tpe_fn f, void *ctx, const gc_real *lo, const gc_real *hi, int d,
           int n_iter, int n_init, long seed, gc_real *out_x, gc_real *out_f)
{
    double *X, *Y, *sigma, *cand, *bestx;
    int *ord;
    gs_rng r;
    int m, count = 0, i, k, it, best = 0;
    const double LNORM = 0.91893853320467274178; /* 0.5 ln(2pi) */
    if (!f || !lo || !hi || !out_x || !out_f || d < 1 || n_init < 2 || n_iter < 1) return -1;
    m = n_init + n_iter;
    X = (double *)malloc((size_t)m * d * sizeof(double));
    Y = (double *)malloc((size_t)m * sizeof(double));
    sigma = (double *)malloc((size_t)d * sizeof(double));
    cand = (double *)malloc((size_t)d * sizeof(double));
    bestx = (double *)malloc((size_t)d * sizeof(double));
    ord = (int *)malloc((size_t)m * sizeof(int));
    if (!X || !Y || !sigma || !cand || !bestx || !ord) {
        free(X); free(Y); free(sigma); free(cand); free(bestx); free(ord); return -1;
    }
    r = gs_rng_make((unsigned long long)seed);
    for (count = 0; count < n_init; count++) {
        for (k = 0; k < d; k++) X[(size_t)count * d + k] = lo[k] + (hi[k] - lo[k]) * gs_rng_uniform(&r);
        Y[count] = f(&X[(size_t)count * d], ctx);
    }
    for (k = 0; k < d; k++) sigma[k] = 0.2 * (hi[k] - lo[k]);
    for (it = 0; it < n_iter; it++) {
        int n_good, g, c, nb;
        double best_ratio = -1e300;
        for (i = 0; i < count; i++) ord[i] = i;
        for (i = 0; i < count; i++)
            for (g = i + 1; g < count; g++)
                if (Y[ord[g]] < Y[ord[i]]) { int t = ord[i]; ord[i] = ord[g]; ord[g] = t; }
        n_good = (int)(0.25 * count);
        if (n_good < 1) n_good = 1;
        if (n_good >= count) n_good = count - 1;
        nb = count - n_good;
        for (c = 0; c < 24; c++) {
            int src = ord[gs_rng_int(&r, n_good)];
            double lg = 0.0, gb = 0.0, ratio;
            for (k = 0; k < d; k++) {
                double xc = X[(size_t)src * d + k] + sigma[k] * gs_rng_normal(&r);
                if (xc < lo[k]) xc = lo[k];
                if (xc > hi[k]) xc = hi[k];
                cand[k] = xc;
            }
            /* log l(x): miscela sui buoni */
            {
                double acc = 0.0;
                for (g = 0; g < n_good; g++) {
                    double lp = -LNORM;
                    int id = ord[g];
                    for (k = 0; k < d; k++) lp += tpe_logpdf_dim(cand[k], X[(size_t)id * d + k], sigma[k]);
                    acc += exp(lp);
                }
                lg = log(acc / n_good + 1e-300);
            }
            /* log g(x): miscela sui cattivi */
            if (nb > 0) {
                double acc = 0.0;
                for (g = n_good; g < count; g++) {
                    double lp = -LNORM;
                    int id = ord[g];
                    for (k = 0; k < d; k++) lp += tpe_logpdf_dim(cand[k], X[(size_t)id * d + k], sigma[k]);
                    acc += exp(lp);
                }
                gb = log(acc / nb + 1e-300);
            } else gb = -1e300;
            ratio = lg - gb;
            if (ratio > best_ratio) { best_ratio = ratio; for (k = 0; k < d; k++) bestx[k] = cand[k]; }
        }
        for (k = 0; k < d; k++) X[(size_t)count * d + k] = bestx[k];
        Y[count] = f(bestx, ctx);
        count++;
    }
    best = 0;
    for (i = 1; i < count; i++) if (Y[i] < Y[best]) best = i;
    for (k = 0; k < d; k++) out_x[k] = X[(size_t)best * d + k];
    *out_f = Y[best];
    free(X); free(Y); free(sigma); free(cand); free(bestx); free(ord);
    return 0;
}
