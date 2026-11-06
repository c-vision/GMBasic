/*
 * ofx_mv.c — shim FFI per la prima slice di MULTIVARIATE (scalari, senza
 * oggetti-risultato): Bayesian-opt acquisition, gambler's ruin, ATE,
 * Random Matrix Theory, MV gauss/student e copule archimedee.
 */
#include <math.h>
#include <stdlib.h>

#include "gs_spectral.h"
#include "gs_math.h"
#include "gs_mvdist.h"
#include "gs_dist.h"

#define MVX_NAN (0.0 / 0.0)

/* ── Bayesian optimization: acquisition scalari (canone CommonLib.Numerics.Bo) ── */
static double bo_phi(double z) { return 0.3989422804014327 * exp(-0.5 * z * z); }

double ofx_bo_ei(double mu, double sd, double best, double xi)
{
    double d, z;
    if (sd <= 1e-300) { d = mu - best - xi; return d > 0.0 ? d : 0.0; }
    d = mu - best - xi;
    z = d / sd;
    return d * gs_dist_normal_cdf(0.0, 1.0, z) + sd * bo_phi(z);
}

double ofx_bo_pi(double mu, double sd, double best, double xi)
{
    if (sd <= 1e-300) return (mu - best - xi) > 0.0 ? 1.0 : 0.0;
    return gs_dist_normal_cdf(0.0, 1.0, (mu - best - xi) / sd);
}

double ofx_bo_ucb(double mu, double sd, double kappa) { return mu + kappa * sd; }

/* ── gambler's ruin ─────────────────────────────────────────────────────── */
double ofx_gambler_p(double i, double N, double p) { return gs_gambler_ruin_prob(i, N, p); }
double ofx_gambler_d(double i, double N, double p) { return gs_gambler_ruin_duration(i, N, p); }

/* ── inferenza causale ──────────────────────────────────────────────────── */
double ofx_ate_ipw(const double *t, const double *y, const double *ps, int n)
{
    if (!t || !y || !ps || n < 1) return MVX_NAN;
    return gs_ate_ipw(t, y, ps, n);
}

double ofx_ate_matching(const double *t, const double *y, const double *ps, int n)
{
    if (!t || !y || !ps || n < 1) return MVX_NAN;
    return gs_ate_matching(t, y, ps, n);
}

/* ── RMT ─────────────────────────────────────────────────────────────────── */
double ofx_rmt_lambda_plus(int n, int T, double sigma2)
{
    double lp = 0.0, lm = 0.0;
    if (gs_rmt_mp_bounds(n, T, sigma2, &lp, &lm) != 0) return MVX_NAN;
    return lp;
}

double ofx_rmt_lambda_minus(int n, int T, double sigma2)
{
    double lp = 0.0, lm = 0.0;
    if (gs_rmt_mp_bounds(n, T, sigma2, &lp, &lm) != 0) return MVX_NAN;
    return lm;
}

double ofx_rmt_coherence(const double *v, int n)
{
    if (!v || n < 1) return MVX_NAN;
    return gs_rmt_coherence(v, n);
}

double ofx_rmt_spectral_norm(const double *A, int n)
{
    if (!A || n < 1) return MVX_NAN;
    return gs_rmt_spectral_norm(A, n);
}

double ofx_rmt_davis_kahan(double e_norm, double gap) { return gs_rmt_davis_kahan_bound(e_norm, gap); }

double ofx_rmt_principal_angle(const double *u, const double *v, int n)
{
    if (!u || !v || n < 1) return MVX_NAN;
    return gs_rmt_principal_angle(u, v, n);
}

double ofx_rmt_tightness(double observed, double bound) { return gs_rmt_tightness(observed, bound); }

/* ── distribuzioni multivariate e copule ────────────────────────────────── */
double ofx_mv_gauss_pdf(const double *x, const double *loc, const double *cov, int d)
{
    if (!x || !loc || !cov || d < 1) return MVX_NAN;
    return gs_mvgauss_pdf(d, x, loc, cov);
}

double ofx_mv_student_pdf(const double *x, const double *loc, const double *cov, double nu, int d)
{
    if (!x || !loc || !cov || d < 1) return MVX_NAN;
    return gs_mvstudent_pdf(d, x, loc, cov, nu);
}

double ofx_copula_clayton(double u, double v, double theta) { return gs_copula_clayton_cdf(u, v, theta); }
double ofx_copula_gumbel(double u, double v, double theta) { return gs_copula_gumbel_cdf(u, v, theta); }
double ofx_copula_frank(double u, double v, double theta) { return gs_copula_frank_cdf(u, v, theta); }
