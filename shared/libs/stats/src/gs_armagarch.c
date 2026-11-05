/* gs_armagarch.c — ARMA(p,q)-GARCH/EGARCH, MLE gaussiana + AIC. */
#include "gs_armagarch.h"

#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define SQ2PI 0.79788456080286535588   /* sqrt(2/pi) = E|z| per z~N(0,1) */

static int var_nparams(int varmodel) { return varmodel == 0 ? 1 : (varmodel == 1 ? 3 : 4); }

/* Log-verosimiglianza gaussiana condizionale; scrive ultima var condizionale. */
static double ag_loglik(const double *x, int n, int p, int q, int varmodel,
                        const double *par, double *out_last_s2)
{
    int start = p > q ? p : q, t, i, off = 1 + p + q;
    double c = par[0], ll = 0.0, s2, lns2, mean_x = 0.0, var_x = 0.0;
    double *e;
    if (start < 0) start = 0;
    if (n - start < 5) return -1e300;
    e = (double *)calloc((size_t)n, sizeof(double));
    if (!e) return -1e300;
    for (i = start; i < n; i++) mean_x += x[i];
    mean_x /= (double)(n - start);
    for (i = start; i < n; i++) { double d = x[i] - mean_x; var_x += d * d; }
    var_x /= (double)(n - start);
    if (var_x <= 0.0) var_x = 1e-8;
    s2 = var_x; lns2 = log(s2);
    if (varmodel == 2) lns2 = log(var_x);
    for (t = start; t < n; t++) {
        double pred = c, s2t;
        for (i = 1; i <= p; i++) pred += par[i] * x[t - i];              /* par[1..p] = phi */
        for (i = 1; i <= q; i++) pred += par[p + i] * e[t - i];          /* par[p+1..p+q] = theta */
        e[t] = x[t] - pred;
        if (varmodel == 0) {
            double sv = par[off];
            if (sv <= 1e-10) { free(e); return -1e300; }
            s2t = sv;
        } else if (varmodel == 1) {
            double om = par[off], al = par[off + 1], be = par[off + 2];
            if (om <= 0.0 || al < 0.0 || be < 0.0 || al + be >= 1.0) { free(e); return -1e300; }
            s2t = om + al * e[t - 1] * e[t - 1] + be * s2;
        } else {
            double om = par[off], al = par[off + 1], ga = par[off + 2], be = par[off + 3];
            double z = (s2 > 1e-12) ? e[t - 1] / sqrt(s2) : 0.0;
            if (be <= -0.999 || be >= 0.999) { free(e); return -1e300; }
            lns2 = om + al * (fabs(z) - SQ2PI) + ga * z + be * lns2;
            s2t = exp(lns2);
        }
        if (s2t <= 1e-12) { free(e); return -1e300; }
        ll += -0.5 * (log(2.0 * PI) + log(s2t) + e[t] * e[t] / s2t);
        s2 = s2t;
    }
    if (out_last_s2) *out_last_s2 = s2;
    free(e);
    return ll;
}

/* --- Nelder-Mead (minimizzazione) --- */
typedef double (*ag_fn)(const double *x, void *ctx);

static double ag_nm(int nd, double *x, const double *step, int maxit, double tol,
                    ag_fn f, void *ctx)
{
    double *sim = malloc((size_t)(nd + 1) * (size_t)nd * sizeof(double));
    double *fv = malloc((size_t)(nd + 1) * sizeof(double));
    double *cen = malloc((size_t)nd * sizeof(double));
    double *xr = malloc((size_t)nd * sizeof(double));
    double *xe = malloc((size_t)nd * sizeof(double));
    double *xc = malloc((size_t)nd * sizeof(double));
    int i, j, it, best = 0;
    if (!sim || !fv || !cen || !xr || !xe || !xc) { free(sim); free(fv); free(cen); free(xr); free(xe); free(xc); return 0.0 / 0.0; }
#define SIM(k, i) sim[(size_t)(k) * nd + (i)]
    for (i = 0; i < nd; i++) SIM(0, i) = x[i];
    fv[0] = f(&SIM(0, 0), ctx);
    for (i = 0; i < nd; i++) {
        for (j = 0; j < nd; j++) SIM(i + 1, j) = x[j];
        SIM(i + 1, i) += step[i];
        fv[i + 1] = f(&SIM(i + 1, 0), ctx);
    }
    for (it = 0; it < maxit; it++) {
        int worst = 0; double fr, fe, fcc;
        best = 0; worst = 0;
        for (i = 1; i <= nd; i++) { if (fv[i] < fv[best]) best = i; if (fv[i] > fv[worst]) worst = i; }
        if (fv[worst] - fv[best] < tol) break;
        for (i = 0; i < nd; i++) { cen[i] = 0.0; for (j = 0; j <= nd; j++) if (j != worst) cen[i] += SIM(j, i); cen[i] /= (double)nd; }
        for (i = 0; i < nd; i++) xr[i] = cen[i] + (cen[i] - SIM(worst, i));
        fr = f(xr, ctx);
        if (fr < fv[best]) {
            for (i = 0; i < nd; i++) xe[i] = cen[i] + 2.0 * (cen[i] - SIM(worst, i));
            fe = f(xe, ctx);
            if (fe < fr) { for (i = 0; i < nd; i++) SIM(worst, i) = xe[i]; fv[worst] = fe; }
            else { for (i = 0; i < nd; i++) SIM(worst, i) = xr[i]; fv[worst] = fr; }
        } else if (fr < fv[worst]) {
            for (i = 0; i < nd; i++) SIM(worst, i) = xr[i]; fv[worst] = fr;
        } else {
            for (i = 0; i < nd; i++) xc[i] = cen[i] + 0.5 * (SIM(worst, i) - cen[i]);
            fcc = f(xc, ctx);
            if (fcc < fv[worst]) { for (i = 0; i < nd; i++) SIM(worst, i) = xc[i]; fv[worst] = fcc; }
            else for (j = 0; j <= nd; j++) { if (j == best) continue; for (i = 0; i < nd; i++) SIM(j, i) = 0.5 * (SIM(j, i) + SIM(best, i)); fv[j] = f(&SIM(j, 0), ctx); }
        }
    }
    best = 0; for (i = 1; i <= nd; i++) if (fv[i] < fv[best]) best = i;
    for (i = 0; i < nd; i++) x[i] = SIM(best, i);
    { double rv = fv[best]; free(sim); free(fv); free(cen); free(xr); free(xe); free(xc); return rv; }
#undef SIM
}

typedef struct { const double *x; int n, p, q, vm; } ag_ctx;
static double ag_negll(const double *par, void *vctx)
{
    ag_ctx *c = (ag_ctx *)vctx;
    return -ag_loglik(c->x, c->n, c->p, c->q, c->vm, par, NULL);
}

gc_status gs_armagarch_fit(const gc_real *x, int n, int p, int q, int varmodel,
                           gs_armagarch *out)
{
    double par[1 + 2 * GS_AG_MAXP + 4], step[1 + 2 * GS_AG_MAXP + 4], mean = 0.0, var = 0.0;
    int i, off, nd, start;
    ag_ctx c;
    double last_s2 = 0.0, ll;
    if (!x || !out || n < 20 || p < 0 || q < 0 || p > GS_AG_MAXP || q > GS_AG_MAXP
        || varmodel < 0 || varmodel > 2) return GC_ERR_ARG;
    start = p > q ? p : q;
    for (i = 0; i < n; i++) mean += x[i]; mean /= n;
    for (i = 0; i < n; i++) { double d = x[i] - mean; var += d * d; } var /= n;
    if (var <= 0.0) var = 1e-6;
    nd = 1 + p + q + var_nparams(varmodel);
    par[0] = mean; step[0] = 0.1 * sqrt(var);
    for (i = 0; i < p; i++) { par[1 + i] = 0.1; step[1 + i] = 0.1; }
    for (i = 0; i < q; i++) { par[1 + p + i] = 0.1; step[1 + p + i] = 0.1; }
    off = 1 + p + q;
    if (varmodel == 0) { par[off] = var; step[off] = 0.1 * var; }
    else if (varmodel == 1) { par[off] = 0.05 * var; par[off + 1] = 0.1; par[off + 2] = 0.8; step[off] = 0.02 * var; step[off + 1] = 0.05; step[off + 2] = 0.05; }
    else { par[off] = 0.0; par[off + 1] = 0.1; par[off + 2] = 0.0; par[off + 3] = 0.9; step[off] = 0.05; step[off + 1] = 0.05; step[off + 2] = 0.05; step[off + 3] = 0.05; }
    (void)start;
    c.x = x; c.n = n; c.p = p; c.q = q; c.vm = varmodel;
    ag_nm(nd, par, step, 3000, 1e-10, ag_negll, &c);
    ll = ag_loglik(x, n, p, q, varmodel, par, &last_s2);
    out->p = p; out->q = q; out->varmodel = varmodel;
    out->c = par[0];
    for (i = 0; i < GS_AG_MAXP; i++) { out->phi[i] = 0.0; out->theta[i] = 0.0; }
    for (i = 0; i < p; i++) out->phi[i] = par[1 + i];
    for (i = 0; i < q; i++) out->theta[i] = par[1 + p + i];
    out->sigma2 = (varmodel == 0) ? par[off] : last_s2;
    out->omega = par[off];
    out->alpha = (varmodel >= 1) ? par[off + 1] : 0.0;
    out->beta = (varmodel == 1) ? par[off + 2] : (varmodel == 2 ? par[off + 3] : 0.0);
    out->gamma = (varmodel == 2) ? par[off + 2] : 0.0;
    out->loglik = ll;
    out->nparams = nd;
    out->aic = -2.0 * ll + 2.0 * (double)nd;
    return GC_OK;
}

gc_status gs_armagarch_select(const gc_real *x, int n, int pmax, int qmax,
                              gs_armagarch *out_best)
{
    gs_armagarch m;
    int p, q, vm;
    double best_aic = 1e300;
    int found = 0;
    if (!x || !out_best || n < 20 || pmax < 0 || qmax < 0) return GC_ERR_ARG;
    if (pmax > GS_AG_MAXP) pmax = GS_AG_MAXP;
    if (qmax > GS_AG_MAXP) qmax = GS_AG_MAXP;
    for (vm = 0; vm <= 2; vm++)
        for (p = 0; p <= pmax; p++)
            for (q = 0; q <= qmax; q++) {
                if (p == 0 && q == 0 && vm != 0) continue;   /* ARMA(0,0) solo var cost */
                if (gs_armagarch_fit(x, n, p, q, vm, &m) != GC_OK) continue;
                if (m.aic < best_aic) { best_aic = m.aic; *out_best = m; found = 1; }
            }
    return found ? GC_OK : GC_ERR_ARG;
}

gc_real gs_armagarch_forecast(const gs_armagarch *m, const gc_real *x, int n,
                              gc_real *out_sigma2)
{
    int i, t;
    double pred, last_s2 = 0.0, *e;
    if (!m || !x || n < 2) return 0.0 / 0.0;
    e = (double *)calloc((size_t)n, sizeof(double));
    if (!e) return 0.0 / 0.0;
    /* ricostruzione residui (var non necessaria alla media) */
    for (t = m->p; t < n; t++) {
        pred = m->c;
        for (i = 1; i <= m->p; i++) pred += m->phi[i - 1] * x[t - i];
        for (i = 1; i <= m->q; i++) pred += m->theta[i - 1] * e[t - i];
        e[t] = x[t] - pred;
    }
    pred = m->c;
    for (i = 1; i <= m->p; i++) pred += m->phi[i - 1] * x[n - i];
    for (i = 1; i <= m->q; i++) pred += m->theta[i - 1] * e[n - i];
    if (out_sigma2) {
        (void)last_s2;
        *out_sigma2 = m->sigma2;
    }
    free(e);
    return pred;
}
