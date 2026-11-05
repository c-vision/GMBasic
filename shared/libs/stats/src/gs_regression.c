/* gs_regression.c — regressione, distanze multivariate e metriche di errore.
 * Progetto gmbstats-regression (link minimo). */
#include "gs_regression.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define IDX(i, j, n) ((size_t)(i) * (size_t)(n) + (size_t)(j))

gc_real gs_mahalanobis(const gc_real *x, const gc_real *mu, const gc_real *Sinv, int n)
{
    gc_real s = 0.0;
    int i, j;
    if (x == NULL || mu == NULL || Sinv == NULL || n <= 0) return 0.0;
    for (i = 0; i < n; i++) {
        gc_real d = x[i] - mu[i], acc = 0.0;
        for (j = 0; j < n; j++) acc += Sinv[IDX(i, j, n)] * (x[j] - mu[j]);
        s += d * acc;
    }
    return s;
}

gc_real gs_hotelling_t2(const gc_real *xbar, const gc_real *mu, const gc_real *Sinv, int p)
{
    return gs_mahalanobis(xbar, mu, Sinv, p);
}

gc_status gs_regression_multi(const gc_real *X, const gc_real *y, int n, int p, gc_real *beta)
{
    gc_real *XtX, *Xty;
    int i, j, k;
    if (X == NULL || y == NULL || beta == NULL || n <= 0 || p <= 0) return GC_ERR_ARG;
    XtX = (gc_real *)calloc((size_t)p * (size_t)p, sizeof(gc_real));
    Xty = (gc_real *)calloc((size_t)p, sizeof(gc_real));
    if (!XtX || !Xty) { free(XtX); free(Xty); return GC_ERR_NO_MEM; }
    for (i = 0; i < p; i++) {
        for (k = 0; k < n; k++) Xty[i] += X[IDX(k, i, p)] * y[k];
        for (j = 0; j < p; j++)
            for (k = 0; k < n; k++) XtX[IDX(i, j, p)] += X[IDX(k, i, p)] * X[IDX(k, j, p)];
    }
    if (gm_linalg_solve(p, XtX, Xty, beta) != GC_OK) { free(XtX); free(Xty); return GC_ERR_SINGULAR; }
    free(XtX); free(Xty);
    return GC_OK;
}

gc_status gs_ridge_regression(const gc_real *X, const gc_real *y, int n, int p,
                              gc_real lambda, gc_real *beta)
{
    gc_real *XtX, *Xty;
    int i, j, k;
    if (X == NULL || y == NULL || beta == NULL || n <= 0 || p <= 0) return GC_ERR_ARG;
    XtX = (gc_real *)calloc((size_t)p * (size_t)p, sizeof(gc_real));
    Xty = (gc_real *)calloc((size_t)p, sizeof(gc_real));
    if (!XtX || !Xty) { free(XtX); free(Xty); return GC_ERR_NO_MEM; }
    for (i = 0; i < p; i++) {
        for (k = 0; k < n; k++) Xty[i] += X[IDX(k, i, p)] * y[k];
        for (j = 0; j < p; j++)
            for (k = 0; k < n; k++) XtX[IDX(i, j, p)] += X[IDX(k, i, p)] * X[IDX(k, j, p)];
        XtX[IDX(i, i, p)] += lambda;
    }
    if (gm_linalg_solve(p, XtX, Xty, beta) != GC_OK) { free(XtX); free(Xty); return GC_ERR_SINGULAR; }
    free(XtX); free(Xty);
    return GC_OK;
}

gc_status gs_logistic_regression(const gc_real *X, const gc_real *y, int n, int p,
                                 gc_real *beta, int maxit, gc_real tol)
{
    int i, j, k, it;
    if (X == NULL || y == NULL || beta == NULL || n <= 0 || p <= 0) return GC_ERR_ARG;
    for (i = 0; i < p; i++) beta[i] = 0.0;
    for (it = 0; it < maxit; it++) {
        gc_real *g = (gc_real *)calloc((size_t)p, sizeof(gc_real));
        gc_real *H = (gc_real *)calloc((size_t)p * (size_t)p, sizeof(gc_real));
        gc_real *d = (gc_real *)calloc((size_t)p, sizeof(gc_real));
        gc_real maxg = 0.0;
        if (!g || !H || !d) { free(g); free(H); free(d); return GC_ERR_NO_MEM; }
        for (k = 0; k < n; k++) {
            gc_real eta = 0.0, mu, w;
            for (j = 0; j < p; j++) eta += X[IDX(k, j, p)] * beta[j];
            mu = 1.0 / (1.0 + exp(-eta));
            w = mu * (1.0 - mu);
            for (i = 0; i < p; i++) {
                g[i] += X[IDX(k, i, p)] * (y[k] - mu);
                for (j = 0; j < p; j++) H[IDX(i, j, p)] += X[IDX(k, i, p)] * w * X[IDX(k, j, p)];
            }
        }
        for (i = 0; i < p; i++) { H[IDX(i, i, p)] += 1e-10; }
        for (i = 0; i < p; i++) if (fabs(g[i]) > maxg) maxg = fabs(g[i]);
        if (gm_linalg_solve(p, H, g, d) != GC_OK) { free(g); free(H); free(d); return GC_ERR_SINGULAR; }
        for (i = 0; i < p; i++) beta[i] += d[i];
        free(g); free(H); free(d);
        if (maxg <= tol) return GC_OK;
    }
    return GC_OK;
}

void gs_online_update(gs_online *o, gc_real x)
{
    gc_real delta;
    if (!o) return;
    o->count += 1.0;
    delta = x - o->mean;
    o->mean += delta / o->count;
    o->m2 += delta * (x - o->mean);
}
gc_real gs_online_mean(const gs_online *o) { return o ? o->mean : 0.0; }
gc_real gs_online_var(const gs_online *o) { return (o && o->count > 1) ? o->m2 / (o->count - 1.0) : 0.0; }
gc_real gs_online_std(const gs_online *o) { return sqrt(gs_online_var(o)); }
void gs_online_merge(gs_online *a, const gs_online *b)
{
    gc_real na, nb, n, delta;
    if (!a || !b || b->count <= 0) return;
    na = a->count; nb = b->count; n = na + nb;
    delta = b->mean - a->mean;
    a->mean = (na * a->mean + nb * b->mean) / n;
    a->m2 = a->m2 + b->m2 + delta * delta * na * nb / n;
    a->count = n;
}


/* Hodges-Tompkins volatility (close-to-close + bias correction). */
double gs_mape(const double *y, const double *yh, int n)
{
    double s=0.0; int i; if(!y||!yh||n<1) return 0.0/0.0;
    for(i=0;i<n;i++){ if(fabs(y[i])<1e-300) continue; s+=fabs((y[i]-yh[i])/y[i]); }
    return s/(double)n;
}
double gs_mae(const double *y, const double *yh, int n)
{
    double s=0.0; int i; if(!y||!yh||n<1) return 0.0/0.0;
    for(i=0;i<n;i++) s+=fabs(y[i]-yh[i]);
    return s/(double)n;
}
double gs_directional_accuracy(const double *y, const double *yh, int n)
{
    int i, ok=0, tot=0;
    if(!y||!yh||n<2) return 0.0/0.0;
    for(i=1;i<n;i++){
        double da=y[i]-y[i-1], df=yh[i]-yh[i-1];
        if(da==0.0||df==0.0) continue;
        tot++; if((da>0.0)==(df>0.0)) ok++;
    }
    return tot? (double)ok/(double)tot : 0.0/0.0;
}
double gs_smape(const double *y, const double *yh, int n)
{
    double s=0.0; int i; if(!y||!yh||n<1) return 0.0/0.0;
    for(i=0;i<n;i++){ double d=fabs(y[i])+fabs(yh[i]); if(d<1e-300) continue; s+=2.0*fabs(y[i]-yh[i])/d; }
    return s/(double)n;
}
double gs_rmse(const double *y, const double *yh, int n)
{
    double s=0.0; int i; if(!y||!yh||n<1) return 0.0/0.0;
    for(i=0;i<n;i++){ double e=y[i]-yh[i]; s+=e*e; }
    return sqrt(s/(double)n);
}
double gs_rae(const double *y, const double *yh, int n)
{
    double num=0.0, den=0.0, my=0.0; int i; if(!y||!yh||n<1) return 0.0/0.0;
    for(i=0;i<n;i++) my+=y[i]; my/=n;
    for(i=0;i<n;i++){ num+=fabs(y[i]-yh[i]); den+=fabs(y[i]-my); }
    return (den>1e-300)? num/den : 0.0/0.0;
}
double gs_theil_u2(const double *y, const double *yh, int n)
{
    double num=0.0, den=0.0; int i; if(!y||!yh||n<2) return 0.0/0.0;
    for(i=0;i<n;i++){ double e=y[i]-yh[i]; num+=e*e; }
    for(i=1;i<n;i++){ double e=y[i]-y[i-1]; den+=e*e; }
    return (den>1e-300)? sqrt(num/den) : 0.0/0.0;
}

double gs_theil_u1(const double *y, const double *yh, int n)
{
    double num=0.0, a=0.0, b=0.0; int i;
    if(!y||!yh||n<1) return 0.0/0.0;
    for(i=0;i<n;i++){ double e=y[i]-yh[i]; num+=e*e; a+=yh[i]*yh[i]; b+=y[i]*y[i]; }
    num=sqrt(num/n); a=sqrt(a/n); b=sqrt(b/n);
    return (a+b>1e-300)? num/(a+b) : 0.0/0.0;
}
