/* gs_algo.c — Hawkes point processes, toxic flow, MF-DFA (AI/Algorithms). */
#include "gs_algo.h"
#include <math.h>
#include <stdlib.h>

/* ---------- Hawkes esponenziale ---------- */








/* ---------- Toxic flow ---------- */


/* ---------- Relative-entropy tilt (Avellaneda) ---------- */
double gs_relentropy_tilt(const double *p_market, const double *p_prior, int n,
                          double *out_weights)
{
    int i; double sumw = 0.0, min_kl = 1e300;
    if (!p_market || !p_prior || n < 1) return 0.0/0.0;
    for (i = 0; i < n; i++) {
        double pm = p_market[i] > 1e-12 ? p_market[i] : 1e-12;
        double pp = p_prior[i] > 1e-12 ? p_prior[i] : 1e-12;
        double kl = pm*log(pm/pp) - pm + pp;   /* divergenza di Bregman/KL */
        if (out_weights) { out_weights[i] = exp(-kl); sumw += out_weights[i]; }
        if (kl < min_kl) min_kl = kl;
    }
    if (out_weights && sumw > 0.0) for (i = 0; i < n; i++) out_weights[i] /= sumw;
    return min_kl;
}

/* ---------- MF-DFA ---------- */
static double _slope(const double *lx, const double *ly, int k)
{
    int i; double mx = 0.0, my = 0.0, sxy = 0.0, sxx = 0.0;
    for (i = 0; i < k; i++) { mx += lx[i]; my += ly[i]; }
    mx /= k; my /= k;
    for (i = 0; i < k; i++) { sxy += (lx[i]-mx)*(ly[i]-my); sxx += (lx[i]-mx)*(lx[i]-mx); }
    return sxx > 1e-18 ? sxy/sxx : 0.0;
}

int gs_mfdfa(const double *x, int n, const double *q, int nq, int smin, int smax,
             int nscale, double *out_hq)
{
    double *Y; int i, s, iq, k;
    if (!x || !q || !out_hq || n < 2*smin || smin < 4 || nscale < 2) return -1;
    Y = (double *)malloc((size_t)n*sizeof(double));
    if (!Y) return -1;
    /* profilo integrato */
    { double mu = 0.0; for (i = 0; i < n; i++) mu += x[i]; mu /= n;
      Y[0] = x[0]-mu; for (i = 1; i < n; i++) Y[i] = Y[i-1] + (x[i]-mu); }
    for (iq = 0; iq < nq; iq++) {
        double lx[64], ly[64]; int cnt = 0;
        for (k = 0; k < nscale; k++) {
            double Fq = 0.0, lF;
            s = smin + (int)((double)(smax - smin) * k / (nscale > 1 ? nscale-1 : 1));
            if (s < 4 || s > n) continue;
            {
                int Ns = n / s, v, j;
                if (Ns < 2) continue;
                for (v = 0; v < Ns; v++) {
                    int base = v*s; double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, a, b, fv = 0.0;
                    for (j = 0; j < s; j++) { sx += j; sy += Y[base+j]; }
                    { double mx = sx/s, my = sy/s;
                      for (j = 0; j < s; j++) { sxx += (j-mx)*(j-mx); sxy += (j-mx)*(Y[base+j]-my); }
                      b = sxx > 1e-18 ? sxy/sxx : 0.0; a = my - b*mx; }
                    for (j = 0; j < s; j++) { double e = Y[base+j] - (a + b*j); fv += e*e; }
                    fv = sqrt(fv / s);
                    if (fv < 1e-12) fv = 1e-12;
                    if (fabs(q[iq]) < 1e-9) Fq += log(fv*fv);
                    else Fq += pow(fv, q[iq]);
                }
                if (fabs(q[iq]) < 1e-9) lF = 0.5 * exp(Fq / Ns);
                else lF = pow(Fq / Ns, 1.0 / q[iq]);
                lx[cnt] = log((double)s); ly[cnt] = log(lF > 1e-300 ? lF : 1e-300); cnt++;
            }
        }
        out_hq[iq] = cnt >= 2 ? _slope(lx, ly, cnt) : 0.0/0.0;
    }
    free(Y);
    return 0;
}

/* Scaling di volatilita': sigma(tau) ~ tau^alpha. */
int gs_vol_scaling(const double *ret, int n, const int *taus, int ntau,
                   double *out_alpha, double *out_r2)
{
    double *lx = (double *)malloc((size_t)ntau * sizeof(double));
    double *ly = (double *)malloc((size_t)ntau * sizeof(double));
    int i, k, cnt = 0;
    if (!ret || !taus || !lx || !ly || ntau < 2 || n < 16) { free(lx); free(ly); return -1; }
    for (k = 0; k < ntau; k++) {
        int tau = taus[k], nb, b;
        double mean = 0.0, var = 0.0, *S;
        if (tau < 1 || n / tau < 2) continue;
        nb = n / tau;
        S = (double *)malloc((size_t)nb * sizeof(double));
        if (!S) { free(lx); free(ly); return -1; }
        for (b = 0; b < nb; b++) { double s = 0.0; for (i = b * tau; i < (b + 1) * tau; i++) s += ret[i]; S[b] = s; }
        for (b = 0; b < nb; b++) mean += S[b];
        mean /= nb;
        for (b = 0; b < nb; b++) { double d = S[b] - mean; var += d * d; }
        var /= nb;                       /* Var(S_tau); sigma_tau = sqrt(var) ~ tau^alpha */
        if (var > 1e-300) { lx[cnt] = log((double)tau); ly[cnt] = 0.5 * log(var); cnt++; }
        free(S);
    }
    if (cnt < 2) { free(lx); free(ly); return -1; }
    {
        double mx = 0.0, my = 0.0, sxx = 0.0, sxy = 0.0, syy = 0.0, alpha, r2 = 0.0;
        for (i = 0; i < cnt; i++) { mx += lx[i]; my += ly[i]; }
        mx /= cnt; my /= cnt;
        for (i = 0; i < cnt; i++) { double dx = lx[i] - mx, dy = ly[i] - my; sxx += dx * dx; sxy += dx * dy; syy += dy * dy; }
        alpha = sxx > 0.0 ? sxy / sxx : 0.0;
        if (sxx > 0.0 && syy > 0.0) r2 = (sxy * sxy) / (sxx * syy);
        if (out_alpha) *out_alpha = alpha;
        if (out_r2) *out_r2 = r2;
    }
    free(lx); free(ly);
    return 0;
}

/* ---------------- ACD (durate) ---------------- */

typedef struct { const double *x; int n, p, q; } acd_ctx;

static void acd_psi(const double *x, int n, int p, int q, const double *par, double *psi)
{
    int start = p > q ? p : q, i, j;
    double mean = 0.0;
    for (i = 0; i < n; i++) mean += x[i];
    mean /= (n > 0 ? n : 1);
    for (i = 0; i < n; i++) {
        double s;
        if (i < start) { psi[i] = mean > 1e-12 ? mean : 1e-12; continue; }
        s = par[0];
        for (j = 1; j <= p; j++) s += par[j] * x[i - j];
        for (j = 1; j <= q; j++) s += par[p + j] * psi[i - j];
        psi[i] = s > 1e-12 ? s : 1e-12;
    }
}

static double acd_negll(const double *par, void *vctx)
{
    acd_ctx *c = (acd_ctx *)vctx;
    double *psi = (double *)malloc((size_t)c->n * sizeof(double));
    double ll = 0.0, sumab = 0.0;
    int i, j;
    if (!psi) return 1e300;
    if (par[0] <= 0.0) { free(psi); return 1e300; }
    for (j = 1; j <= c->p; j++) sumab += par[j];
    for (j = 1; j <= c->q; j++) sumab += par[c->p + j];
    if (sumab >= 1.0) { free(psi); return 1e300; }
    acd_psi(c->x, c->n, c->p, c->q, par, psi);
    for (i = 0; i < c->n; i++) ll += -log(psi[i]) - c->x[i] / psi[i];
    free(psi);
    return -ll;
}

static double acd_nm(int nd, double *x, const double *step, int maxit, double tol,
                     double (*f)(const double *, void *), void *ctx)
{
    double *sim = (double *)malloc((size_t)(nd + 1) * nd * sizeof(double));
    double *fv = (double *)malloc((size_t)(nd + 1) * sizeof(double));
    double *cen = (double *)malloc((size_t)nd * sizeof(double));
    double *xr = (double *)malloc((size_t)nd * sizeof(double));
    double *xe = (double *)malloc((size_t)nd * sizeof(double));
    double *xc = (double *)malloc((size_t)nd * sizeof(double));
    int i, j, it, best = 0;
    if (!sim || !fv || !cen || !xr || !xe || !xc) { free(sim); free(fv); free(cen); free(xr); free(xe); free(xc); return 0.0 / 0.0; }
#define S(k, i) sim[(size_t)(k) * nd + (i)]
    for (i = 0; i < nd; i++) S(0, i) = x[i];
    fv[0] = f(&S(0, 0), ctx);
    for (i = 0; i < nd; i++) {
        for (j = 0; j < nd; j++) S(i + 1, j) = x[j];
        S(i + 1, i) += step[i];
        fv[i + 1] = f(&S(i + 1, 0), ctx);
    }
    for (it = 0; it < maxit; it++) {
        int worst = 0; double fr, fe, fcc;
        best = 0; worst = 0;
        for (i = 1; i <= nd; i++) { if (fv[i] < fv[best]) best = i; if (fv[i] > fv[worst]) worst = i; }
        if (fv[worst] - fv[best] < tol) break;
        for (i = 0; i < nd; i++) { cen[i] = 0.0; for (j = 0; j <= nd; j++) if (j != worst) cen[i] += S(j, i); cen[i] /= (double)nd; }
        for (i = 0; i < nd; i++) xr[i] = cen[i] + (cen[i] - S(worst, i));
        fr = f(xr, ctx);
        if (fr < fv[best]) {
            for (i = 0; i < nd; i++) xe[i] = cen[i] + 2.0 * (cen[i] - S(worst, i));
            fe = f(xe, ctx);
            if (fe < fr) { for (i = 0; i < nd; i++) S(worst, i) = xe[i]; fv[worst] = fe; }
            else { for (i = 0; i < nd; i++) S(worst, i) = xr[i]; fv[worst] = fr; }
        } else if (fr < fv[worst]) {
            for (i = 0; i < nd; i++) S(worst, i) = xr[i]; fv[worst] = fr;
        } else {
            for (i = 0; i < nd; i++) xc[i] = cen[i] + 0.5 * (S(worst, i) - cen[i]);
            fcc = f(xc, ctx);
            if (fcc < fv[worst]) { for (i = 0; i < nd; i++) S(worst, i) = xc[i]; fv[worst] = fcc; }
            else for (j = 0; j <= nd; j++) { if (j == best) continue; for (i = 0; i < nd; i++) S(j, i) = 0.5 * (S(j, i) + S(best, i)); fv[j] = f(&S(j, 0), ctx); }
        }
    }
    best = 0; for (i = 1; i <= nd; i++) if (fv[i] < fv[best]) best = i;
    for (i = 0; i < nd; i++) x[i] = S(best, i);
    { double rv = fv[best]; free(sim); free(fv); free(cen); free(xr); free(xe); free(xc); return rv; }
#undef S
}

int gs_acd_fit(const double *x, int n, int p, int q, gs_acd *out)
{
    acd_ctx c;
    double par[9], step[9], mean = 0.0;
    int i, start = p > q ? p : q;
    if (!x || !out || n < (start + 5) || p < 0 || q < 0 || p > 4 || q > 4 || p + q < 1) return -1;
    for (i = 0; i < n; i++) mean += x[i];
    mean /= n;
    if (mean <= 1e-12) mean = 1e-3;
    par[0] = 0.1 * mean; step[0] = 0.05 * mean;
    for (i = 1; i <= p; i++) { par[i] = 0.1; step[i] = 0.05; }
    for (i = 1; i <= q; i++) { par[p + i] = 0.7 / (q > 0 ? q : 1); step[p + i] = 0.05; }
    { double save = par[0]; par[0] = mean * (1.0 - 0.1 * p - 0.7); if (par[0] <= 1e-6) par[0] = save; }
    c.x = x; c.n = n; c.p = p; c.q = q;
    acd_nm(1 + p + q, par, step, 3000, 1e-9, acd_negll, &c);
    out->p = p; out->q = q; out->omega = par[0];
    for (i = 0; i < 4; i++) { out->alpha[i] = 0.0; out->beta[i] = 0.0; }
    for (i = 1; i <= p; i++) out->alpha[i - 1] = par[i];
    for (i = 1; i <= q; i++) out->beta[i - 1] = par[p + i];
    out->loglik = -acd_negll(par, &c);
    out->nparams = 1 + p + q;
    return 0;
}

double gs_acd_forecast(const gs_acd *m, const double *x, int n)
{
    double *psi, s;
    int start, j;
    if (!m || !x || n < 1) return 0.0 / 0.0;
    start = m->p > m->q ? m->p : m->q;
    if (n <= start) return 0.0 / 0.0;
    psi = (double *)malloc((size_t)n * sizeof(double));
    if (!psi) return 0.0 / 0.0;
    {
        double par[9];
        par[0] = m->omega;
        for (j = 1; j <= m->p; j++) par[j] = m->alpha[j - 1];
        for (j = 1; j <= m->q; j++) par[m->p + j] = m->beta[j - 1];
        acd_psi(x, n, m->p, m->q, par, psi);
    }
    s = m->omega;
    for (j = 1; j <= m->p; j++) s += m->alpha[j - 1] * x[n - j];
    for (j = 1; j <= m->q; j++) s += m->beta[j - 1] * psi[n - j];
    free(psi);
    return s;
}
