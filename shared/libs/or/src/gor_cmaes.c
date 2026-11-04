/* gor_cmaes.c — CMA-ES (Covariance Matrix Adaptation Evolution Strategy), clean-room.
 * Riferimento (concetto/BSD-3): JQSakaJoo/Population-optimization-algorithms-MQL5 (AO_CMAES).
 * Standard Hansen: (mu/mu_w, lambda)-ES, path evolution, rank-one + rank-mu, eigen via Jacobi,
 * clamp al box [lo,hi]. Solo libc/libm. */
#include "gor_meta.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned long long g_cs = 88172645463325252ULL;
static double u01(void){ g_cs ^= g_cs << 13; g_cs ^= g_cs >> 7; g_cs ^= g_cs << 17;
    return (double)((g_cs >> 11) & ((1ULL<<53)-1)) / (double)(1ULL<<53); }
static double gauss(void){ double u1=u01()+1e-12, u2=u01(); return sqrt(-2.0*log(u1))*cos(2.0*M_PI*u2); }

/* Jacobi ciclico per simmetriche: A distrutta, V[i*n+j]=componente i dell'eigvec j, eval[j]. */
static void jacobi(int n, double *A, double *eval, double *V)
{
    int i, j, p, q, k, sweep;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) V[i*n+j] = (i == j) ? 1.0 : 0.0;
    for (sweep = 0; sweep < 100; sweep++) {
        double off = 0.0;
        for (i = 0; i < n; i++) for (j = i+1; j < n; j++) off += A[i*n+j]*A[i*n+j];
        if (off < 1e-30) break;
        for (p = 0; p < n-1; p++) for (q = p+1; q < n; q++) {
            if (fabs(A[p*n+q]) < 1e-300) continue;
            double theta = (A[q*n+q] - A[p*n+p]) / (2.0*A[p*n+q]);
            double t = (theta >= 0.0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta*theta + 1.0));
            double c = 1.0/sqrt(t*t + 1.0), s = t*c;
            for (k = 0; k < n; k++) { double apk=A[p*n+k], aqk=A[q*n+k];
                A[p*n+k]=c*apk - s*aqk; A[q*n+k]=s*apk + c*aqk; }
            for (k = 0; k < n; k++) { double akp=A[k*n+p], akq=A[k*n+q];
                A[k*n+p]=c*akp - s*akq; A[k*n+q]=s*akp + c*akq; }
            for (k = 0; k < n; k++) { double vkp=V[k*n+p], vkq=V[k*n+q];
                V[k*n+p]=c*vkp - s*vkq; V[k*n+q]=s*vkp + c*vkq; }
        }
    }
    for (i = 0; i < n; i++) eval[i] = A[i*n+i];
}

gor_real gor_meta_cmaes(gor_meta_fn f, void *ctx, gor_real *x, int d,
                        const gor_real *lo, const gor_real *hi, int maxit, unsigned long seed)
{
    int n = d, i, j, k, it, a, b;
    double sigma, best = 0.0, sumw, sw2, mueff, cc, cs, c1, cmu, damps, chiN;
    double *w=NULL,*m=NULL,*C=NULL,*W=NULL,*V=NULL,*sd=NULL,*ev=NULL,*pc=NULL,*ps=NULL;
    double *X=NULL,*Y=NULL,*fit=NULL,*xnew=NULL,*yw=NULL,*z=NULL,*bx=NULL;
    int *ord=NULL;
    if (!f || !x || d < 1 || maxit < 1) return 0.0;
    if (seed) g_cs = seed;

    int lambda = 4 + (int)(3.0*log((double)n));
    int mu = lambda / 2;
    w = malloc((size_t)mu*sizeof(double)); m = malloc((size_t)n*sizeof(double));
    C = malloc((size_t)n*n*sizeof(double)); W = malloc((size_t)n*n*sizeof(double));
    V = malloc((size_t)n*n*sizeof(double)); sd = malloc((size_t)n*sizeof(double));
    ev = malloc((size_t)n*sizeof(double)); pc = calloc((size_t)n,sizeof(double));
    ps = calloc((size_t)n,sizeof(double)); X = malloc((size_t)lambda*n*sizeof(double));
    Y = malloc((size_t)lambda*n*sizeof(double)); fit = malloc((size_t)lambda*sizeof(double));
    ord = malloc((size_t)lambda*sizeof(int)); xnew = malloc((size_t)n*sizeof(double));
    yw = malloc((size_t)n*sizeof(double)); z = malloc((size_t)n*sizeof(double));
    bx = malloc((size_t)n*sizeof(double));
    if (!w||!m||!C||!W||!V||!sd||!ev||!pc||!ps||!X||!Y||!fit||!ord||!xnew||!yw||!z||!bx) goto done;

    sumw = 0.0;
    for (i = 0; i < mu; i++) { w[i] = log(mu + 0.5) - log((double)(i+1)); sumw += w[i]; }
    for (i = 0; i < mu; i++) w[i] /= sumw;
    sw2 = 0.0; for (i = 0; i < mu; i++) sw2 += w[i]*w[i];
    mueff = 1.0/sw2;
    cc = (4.0 + mueff/n) / (n + 4.0 + 2.0*mueff/n);
    cs = (mueff + 2.0) / (n + mueff + 5.0);
    c1 = 2.0 / ((n + 1.3)*(n + 1.3) + mueff);
    cmu = 2.0*(mueff - 2.0 + 1.0/mueff) / ((n + 2.0)*(n + 2.0) + mueff);
    if (cmu > 1.0 - c1) cmu = 1.0 - c1;
    damps = 1.0 + 2.0*fmax(0.0, sqrt((mueff - 1.0)/(n + 1.0)) - 1.0) + cs;
    chiN = sqrt((double)n) * (1.0 - 1.0/(4.0*n) + 1.0/(21.0*n*n));

    for (i = 0; i < n; i++) m[i] = x[i];
    for (i = 0; i < n*n; i++) C[i] = 0.0;
    for (i = 0; i < n; i++) C[i*n+i] = 1.0;
    sigma = 0.3;
    if (lo && hi) { double s = 0.0; for (i = 0; i < n; i++) s += (hi[i]-lo[i]); sigma = 0.3*(s/(double)n); }
    if (sigma <= 0.0) sigma = 0.3;

    best = f(m, n, ctx); memcpy(bx, m, (size_t)n*sizeof(double));

    for (it = 0; it < maxit; it++) {
        double normps, hs, corr, pcnorm = 0.0;
        memcpy(W, C, (size_t)n*n*sizeof(double));
        jacobi(n, W, ev, V);
        for (j = 0; j < n; j++) sd[j] = sqrt(fmax(ev[j], 1e-30));

        for (k = 0; k < lambda; k++) {
            for (i = 0; i < n; i++) z[i] = gauss();
            for (i = 0; i < n; i++) {
                double yi = 0.0, xi;
                for (j = 0; j < n; j++) yi += V[i*n+j]*sd[j]*z[j];
                Y[k*n+i] = yi;
                xi = m[i] + sigma*yi;
                if (lo && xi < lo[i]) xi = lo[i];
                if (hi && xi > hi[i]) xi = hi[i];
                X[k*n+i] = xi;
            }
            fit[k] = f(&X[k*n], n, ctx);
            if (fit[k] < best) { best = fit[k]; memcpy(bx, &X[k*n], (size_t)n*sizeof(double)); }
        }
        for (k = 0; k < lambda; k++) ord[k] = k;
        for (a = 1; a < lambda; a++) { int key=ord[a]; double kv=fit[key]; b=a-1;
            while (b>=0 && fit[ord[b]]>kv){ ord[b+1]=ord[b]; b--; } ord[b+1]=key; }

        for (i = 0; i < n; i++) { double s = 0.0; for (k = 0; k < mu; k++) s += w[k]*X[ord[k]*n+i]; xnew[i] = s; }
        for (i = 0; i < n; i++) yw[i] = (xnew[i] - m[i]) / sigma;

        /* invsqrt(C)*yw = V diag(1/sd) V^T yw */
        for (j = 0; j < n; j++) { double s = 0.0; for (i = 0; i < n; i++) s += V[i*n+j]*yw[i]; ev[j] = s; }
        for (i = 0; i < n; i++) { double s = 0.0; for (j = 0; j < n; j++) s += V[i*n+j]*(1.0/sd[j])*ev[j]; z[i] = s; }
        normps = 0.0;
        for (i = 0; i < n; i++) { ps[i] = (1.0-cs)*ps[i] + sqrt(cs*(2.0-cs)*mueff)*z[i]; normps += ps[i]*ps[i]; }
        normps = sqrt(normps);
        hs = (normps / sqrt(1.0 - pow(1.0-cs, 2.0*(it+1)))) < (1.4 + 2.0/(n+1.0))*chiN ? 1.0 : 0.0;
        for (i = 0; i < n; i++) pc[i] = (1.0-cc)*pc[i] + hs*sqrt(cc*(2.0-cc)*mueff)*yw[i];
        for (i = 0; i < n; i++) pcnorm += pc[i]*pc[i];

        corr = (1.0 - hs)*cc*(2.0 - cc);
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) {
            double rmu = 0.0;
            for (k = 0; k < mu; k++) rmu += w[k]*Y[ord[k]*n+i]*Y[ord[k]*n+j];
            C[i*n+j] = (1.0-c1-cmu)*C[i*n+j] + c1*(pc[i]*pc[j] + corr*C[i*n+j]) + cmu*rmu;
        }
        sigma *= exp((cs/damps)*(normps/chiN - 1.0));
        memcpy(m, xnew, (size_t)n*sizeof(double));
        (void)pcnorm;
    }
    memcpy(x, bx, (size_t)n*sizeof(double));
done:
    free(w); free(m); free(C); free(W); free(V); free(sd); free(ev); free(pc); free(ps);
    free(X); free(Y); free(fit); free(ord); free(xnew); free(yw); free(z); free(bx);
    return best;
}
