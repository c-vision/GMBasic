/* gs_curve.c — Nelson-Siegel / Svensson yield-curve fit. */
#include "gs_curve.h"
#include <math.h>
#include <stdlib.h>

/* OLS k incognite via normal equations (Gauss). Ritorna 0/-1; *sse out. */
static int _ols(const double *X, const double *y, int n, int k, double *beta, double *sse)
{
    double *M = (double *)calloc((size_t)k*k, sizeof(double));
    double *v = (double *)calloc((size_t)k, sizeof(double));
    int i, j, a;
    if (!M || !v) { free(M); free(v); return -1; }
    for (i = 0; i < n; i++) for (j = 0; j < k; j++) { v[j] += X[i*k+j]*y[i]; for (a = 0; a < k; a++) M[j*k+a] += X[i*k+j]*X[i*k+a]; }
    /* Gauss-Jordan sul sistema aumentato [M | v]: soluzione in v */
    for (i = 0; i < k; i++) {
        int piv = i; double mx = fabs(M[i*k+i]);
        for (a = i+1; a < k; a++) if (fabs(M[a*k+i]) > mx) { mx = fabs(M[a*k+i]); piv = a; }
        if (mx < 1e-14) { free(M); free(v); return -1; }
        if (piv != i) { for (j=0;j<k;j++){double t=M[i*k+j];M[i*k+j]=M[piv*k+j];M[piv*k+j]=t;} {double t=v[i];v[i]=v[piv];v[piv]=t;} }
        { double d = M[i*k+i]; for (j=0;j<k;j++) M[i*k+j]/=d; v[i]/=d; }
        for (a=0;a<k;a++) if (a!=i) { double f=M[a*k+i]; for(j=0;j<k;j++) M[a*k+j]-=f*M[i*k+j]; v[a]-=f*v[i]; }
    }
    for (j = 0; j < k; j++) beta[j] = v[j];
    { double ss = 0; for (i = 0; i < n; i++) { double p = 0; for (j = 0; j < k; j++) p += X[i*k+j]*beta[j]; ss += (y[i]-p)*(y[i]-p); } *sse = ss; }
    free(M); free(v);
    return 0;
}

static double _f1(double x) { return x < 1e-9 ? 1.0 : (1.0 - exp(-x))/x; }
static double _f2(double x) { return _f1(x) - exp(-x); }

int gs_nelson_siegel(const double *maturities, const double *yields, int n,
                     double *out_b0, double *out_b1, double *out_b2, double *out_tau, double *out_rmse)
{
    double best_tau = 1.0, best_sse = 1e300, bb[3] = {0,0,0};
    double *X; int i, g; double tau;
    if (!maturities || !yields || n < 4) return -1;
    X = (double *)malloc((size_t)n*3*sizeof(double));
    if (!X) return -1;
    for (g = 0; g < 200; g++) {
        double b[3]; double sse;
        tau = 0.05 * pow(2.0, g/20.0);           /* griglia 0.05..~50 */
        for (i = 0; i < n; i++) { double x = maturities[i]/tau; X[i*3+0]=1.0; X[i*3+1]=_f1(x); X[i*3+2]=_f2(x); }
        if (_ols(X, yields, n, 3, b, &sse) == 0 && sse < best_sse) { best_sse = sse; best_tau = tau; bb[0]=b[0]; bb[1]=b[1]; bb[2]=b[2]; }
    }
    free(X);
    if (out_b0) *out_b0 = bb[0]; if (out_b1) *out_b1 = bb[1]; if (out_b2) *out_b2 = bb[2];
    if (out_tau) *out_tau = best_tau;
    if (out_rmse) *out_rmse = sqrt(best_sse/n);
    return 0;
}

int gs_svensson(const double *maturities, const double *yields, int n,
                double *out_b0, double *out_b1, double *out_b2, double *out_b3,
                double *out_tau1, double *out_tau2, double *out_rmse)
{
    double best_t1 = 1.0, best_t2 = 5.0, best_sse = 1e300, bb[4] = {0,0,0,0};
    double *X; int i, g1, g2; double t1, t2;
    if (!maturities || !yields || n < 6) return -1;
    X = (double *)malloc((size_t)n*4*sizeof(double));
    if (!X) return -1;
    for (g1 = 0; g1 < 40; g1++) {
        t1 = 0.1 * pow(2.0, g1/6.0);
        for (g2 = g1; g2 < 40; g2++) {
            double b[4], sse;
            t2 = 0.1 * pow(2.0, g2/6.0);
            if (t2 <= t1) continue;
            for (i = 0; i < n; i++) { double x1 = maturities[i]/t1, x2 = maturities[i]/t2;
                X[i*4+0]=1.0; X[i*4+1]=_f1(x1); X[i*4+2]=_f2(x1); X[i*4+3]=_f2(x2); }
            if (_ols(X, yields, n, 4, b, &sse) == 0 && sse < best_sse) { best_sse = sse; best_t1 = t1; best_t2 = t2; bb[0]=b[0];bb[1]=b[1];bb[2]=b[2];bb[3]=b[3]; }
        }
    }
    free(X);
    if (out_b0)*out_b0=bb[0]; if(out_b1)*out_b1=bb[1]; if(out_b2)*out_b2=bb[2]; if(out_b3)*out_b3=bb[3];
    if (out_tau1)*out_tau1=best_t1; if(out_tau2)*out_tau2=best_t2;
    if (out_rmse)*out_rmse=sqrt(best_sse/n);
    return 0;
}

int gs_curve_vf_discounts(const double *CF, int m, int T, const double *prices, double *disc)
{
    double *A, *b; int i, j, k;
    if (!CF || !prices || !disc || m < 1 || T < 1) return -1;
    A = (double*)calloc((size_t)T*T, sizeof(double));
    b = (double*)calloc((size_t)T, sizeof(double));
    if (!A || !b) { free(A); free(b); return -1; }
    for (i = 0; i < T; i++) { for (j = 0; j < T; j++) { double s = 0.0; int r; for (r = 0; r < m; r++) s += CF[r*T+i]*CF[r*T+j]; A[i*T+j] = s; }
        { double s = 0.0; int r; for (r = 0; r < m; r++) s += CF[r*T+i]*prices[r]; b[i] = s; } }
    /* Gauss con pivoting */
    for (i = 0; i < T; i++) {
        int piv = i; double mx = fabs(A[i*T+i]);
        for (k = i+1; k < T; k++) if (fabs(A[k*T+i]) > mx) { mx = fabs(A[k*T+i]); piv = k; }
        if (mx < 1e-14) { free(A); free(b); return -1; }
        if (piv != i) { for (j = 0; j < T; j++) { double t = A[i*T+j]; A[i*T+j] = A[piv*T+j]; A[piv*T+j] = t; } { double t = b[i]; b[i] = b[piv]; b[piv] = t; } }
        for (k = 0; k < T; k++) { if (k == i) continue; double f = A[k*T+i]/A[i*T+i]; int c2; for (c2 = i; c2 < T; c2++) A[k*T+c2] -= f*A[i*T+c2]; b[k] -= f*b[i]; }
    }
    for (i = 0; i < T; i++) disc[i] = b[i]/A[i*T+i];
    free(A); free(b);
    return 0;
}
