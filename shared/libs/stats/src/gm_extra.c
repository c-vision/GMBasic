/* gm_extra.c — OMP, DWT/CWT, Legendre, Wasserstein, Sinkhorn, spettrali. */
#include "gm_extra.h"
#include "gm_mat.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Risolve A_S x = y ai minimi quadrati (A_S: m x k, row-major) via normali. */
static int _lstsq(const double *A, int m, int n, int *cols, int k,
                  const double *y, double *out_full)
{
    double *M, *v, *xs; int i, j, a, c;
    M = (double *)calloc((size_t)k*k, sizeof(double));
    v = (double *)calloc((size_t)k, sizeof(double));
    xs = (double *)calloc((size_t)k, sizeof(double));
    if (!M || !v || !xs) { free(M); free(v); free(xs); return -1; }
    for (i = 0; i < m; i++) for (a = 0; a < k; a++) {
        double Aia = A[i*n+cols[a]];
        v[a] += Aia*y[i];
        for (c = 0; c < k; c++) M[a*k+c] += Aia*A[i*n+cols[c]];
    }
    for (i = 0; i < k; i++) {                    /* Gauss */
        int piv = i; double mx = fabs(M[i*k+i]);
        for (a = i+1; a < k; a++) if (fabs(M[a*k+i]) > mx) { mx=fabs(M[a*k+i]); piv=a; }
        if (mx < 1e-14) { free(M); free(v); free(xs); return -1; }
        if (piv != i) { for (j=0;j<k;j++){double t=M[i*k+j];M[i*k+j]=M[piv*k+j];M[piv*k+j]=t;} {double t=v[i];v[i]=v[piv];v[piv]=t;} }
        for (a=i+1;a<k;a++){ double f=M[a*k+i]/M[i*k+i]; for(j=i;j<k;j++) M[a*k+j]-=f*M[i*k+j]; v[a]-=f*v[i]; }
    }
    for (i=k-1;i>=0;i--){ double s=v[i]; for(j=i+1;j<k;j++) s-=M[i*k+j]*xs[j]; xs[i]=s/M[i*k+i]; }
    for (i=0;i<k;i++) out_full[cols[i]] = xs[i];
    free(M); free(v); free(xs);
    return 0;
}

int gm_omp(const double *A, const double *y, int m, int n, int k, double *out_x)
{
    int *cols; double *r; int i, j, it, kk = 0;
    if (!A || !y || !out_x || m < 1 || n < 1 || k < 1) return -1;
    if (k > n) k = n;
    cols = (int *)malloc((size_t)k*sizeof(int));
    r = (double *)malloc((size_t)m*sizeof(double));
    if (!cols || !r) { free(cols); free(r); return -1; }
    for (i = 0; i < n; i++) out_x[i] = 0.0;
    memcpy(r, y, (size_t)m*sizeof(double));
    for (it = 0; it < k; it++) {
        int best = -1; double bv = -1.0;
        for (j = 0; j < n; j++) {
            double dot = 0.0; int used = 0;
            for (i = 0; i < kk; i++) if (cols[i] == j) used = 1;
            if (used) continue;
            for (i = 0; i < m; i++) dot += A[i*n+j]*r[i];
            if (fabs(dot) > bv) { bv = fabs(dot); best = j; }
        }
        if (best < 0) break;
        cols[kk++] = best;
        if (_lstsq(A, m, n, cols, kk, y, out_x)) break;
        for (i = 0; i < m; i++) { double p = 0.0; for (j = 0; j < kk; j++) p += A[i*n+cols[j]]*out_x[cols[j]]; r[i] = y[i]-p; }
    }
    free(cols); free(r);
    return 0;
}

int gm_dwt_haar(const double *x, int n, int levels, double *out)
{
    double *a, *tmp; int len, lev, i;
    if (!x || !out || n < 2) return -1;
    a = (double *)malloc((size_t)n*sizeof(double));
    tmp = (double *)malloc((size_t)n*sizeof(double));
    if (!a || !tmp) { free(a); free(tmp); return -1; }
    memcpy(a, x, (size_t)n*sizeof(double));
    len = n;
    for (lev = 0; lev < levels && len >= 2; lev++) {
        int half = len/2;
        for (i = 0; i < half; i++) {
            tmp[i] = (a[2*i] + a[2*i+1]) / sqrt(2.0);
            tmp[half+i] = (a[2*i] - a[2*i+1]) / sqrt(2.0);
        }
        memcpy(a, tmp, (size_t)len*sizeof(double));
        len = half;
    }
    /* sfalsamento: dettagli in coda (convenzione piramidale semplice) */
    memcpy(out, a, (size_t)n*sizeof(double));
    free(a); free(tmp);
    return 0;
}

int gm_cwt_morlet(const double *x, int n, const double *scales, int nscales, double *out)
{
    int s, b, k;
    if (!x || !scales || !out || n < 2 || nscales < 1) return -1;
    for (s = 0; s < nscales; s++) {
        double a = scales[s]; double inv = 1.0/a;
        for (b = 0; b < n; b++) {
            double re = 0.0, im = 0.0;
            for (k = 0; k < n; k++) {
                double t = (k-b)*inv;
                double g = exp(-0.5*t*t) * 0.75112554446;   /* pi^-1/4 */
                re += g*cos(5.0*t)*x[k];
                im += g*sin(5.0*t)*x[k];
            }
            out[s*n+b] = sqrt(re*re + im*im)*sqrt(inv);
        }
    }
    return 0;
}

int gm_legendre_transform(const double *x, const double *f, int n,
                          const double *p, int np, double *out)
{
    int ip, i;
    if (!x || !f || !p || !out || n < 1 || np < 1) return -1;
    for (ip = 0; ip < np; ip++) {
        double best = -1e300;
        for (i = 0; i < n; i++) { double v = p[ip]*x[i] - f[i]; if (v > best) best = v; }
        out[ip] = best;
    }
    return 0;
}

static int _cmp(const void *a, const void *b) { double x=*(const double*)a, y=*(const double*)b; return x<y?-1:(x>y?1:0); }

gm_real gm_wasserstein1_1d(const gm_real *a, int na, const gm_real *b, int nb)
{
    double *sa, *sb; int i, j; double w = 0.0, Fa = 0.0, Fb = 0.0, xprev;
    if (!a || !b || na < 1 || nb < 1) return 0.0;
    sa = (double *)malloc((size_t)na*sizeof(double));
    sb = (double *)malloc((size_t)nb*sizeof(double));
    if (!sa || !sb) { free(sa); free(sb); return 0.0; }
    memcpy(sa, a, (size_t)na*sizeof(double)); memcpy(sb, b, (size_t)nb*sizeof(double));
    qsort(sa, na, sizeof(double), _cmp); qsort(sb, nb, sizeof(double), _cmp);
    i = j = 0;
    xprev = (sa[0] < sb[0]) ? sa[0] : sb[0];
    while (i < na || j < nb) {
        double nx;
        if (i >= na) nx = sb[j];
        else if (j >= nb) nx = sa[i];
        else nx = (sa[i] < sb[j]) ? sa[i] : sb[j];
        w += fabs(Fa - Fb) * (nx - xprev);
        while (i < na && sa[i] == nx) { Fa += 1.0/na; i++; }
        while (j < nb && sb[j] == nx) { Fb += 1.0/nb; j++; }
        xprev = nx;
    }
    free(sa); free(sb);
    return w;
}

int gm_sinkhorn(const double *a, int na, const double *b, int nb,
                const double *M, double lam, int iters, double *out_T)
{
    double *u, *v, *K; int i, j, it;
    if (!a || !b || !M || !out_T || na < 1 || nb < 1) return -1;
    u = (double *)malloc((size_t)na*sizeof(double));
    v = (double *)malloc((size_t)nb*sizeof(double));
    K = (double *)malloc((size_t)na*nb*sizeof(double));
    if (!u || !v || !K) { free(u); free(v); free(K); return -1; }
    for (i = 0; i < na; i++) u[i] = 1.0;
    for (j = 0; j < nb; j++) v[j] = 1.0;
    for (i = 0; i < na; i++) for (j = 0; j < nb; j++) K[i*nb+j] = exp(-lam*M[i*nb+j]);
    for (it = 0; it < iters; it++) {
        for (j = 0; j < nb; j++) { double s = 0.0; for (i = 0; i < na; i++) s += K[i*nb+j]*u[i]; v[j] = s > 1e-300 ? b[j]/s : 0.0; }
        for (i = 0; i < na; i++) { double s = 0.0; for (j = 0; j < nb; j++) s += K[i*nb+j]*v[j]; u[i] = s > 1e-300 ? a[i]/s : 0.0; }
    }
    for (i = 0; i < na; i++) for (j = 0; j < nb; j++) out_T[i*nb+j] = u[i]*K[i*nb+j]*v[j];
    free(u); free(v); free(K);
    return 0;
}

gm_real gm_spectral_radius(const gm_real *A, int n, int iters)
{
    double *v, *w; int i, j, it; double lam = 0.0, nrm = 0.0;
    if (!A || n < 1) return 0.0;
    v = (double *)malloc((size_t)n*sizeof(double));
    w = (double *)malloc((size_t)n*sizeof(double));
    if (!v || !w) { free(v); free(w); return 0.0; }
    for (j = 0; j < n; j++) v[j] = 1.0;
    for (it = 0; it < iters; it++) {
        nrm = 0.0;
        for (i = 0; i < n; i++) { double s = 0.0; for (j = 0; j < n; j++) s += fabs(A[i*n+j])*v[j]; w[i] = s; nrm += s*s; }
        nrm = sqrt(nrm); lam = nrm;
        if (nrm < 1e-300) break;
        for (i = 0; i < n; i++) v[i] = w[i]/nrm;
    }
    free(v); free(w);
    return lam;
}

gm_real gm_hs_norm(const gm_real *A, int n)
{
    int i, j; double s = 0.0;
    if (!A || n < 1) return 0.0;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) s += A[i*n+j]*A[i*n+j];
    return sqrt(s);
}

/* --- Genetic algorithm (real-coded) per minimizzazione su box --- */
static double _ga_rnd(long *sd){ *sd=*sd*6364136223846793005LL+1442695040888963407LL; return ((*sd>>11)&((1LL<<53)-1))/(double)(1LL<<53); }

int gm_genetic_optimize(gm_ga_fn f, void *ctx, const double *lo, const double *hi,
                        int d, int pop, int gens, long seed, double *out_x)
{
    double **P, *fit, *best, *child; int i, j, g; double bf = 1e300; long sd = seed?seed:1;
    if (!f || !lo || !hi || !out_x || d < 1 || pop < 4 || gens < 1) return -1;
    P = (double **)malloc((size_t)pop*sizeof(double *));
    fit = (double *)malloc((size_t)pop*sizeof(double));
    best = (double *)malloc((size_t)d*sizeof(double));
    child = (double *)malloc((size_t)d*sizeof(double));
    if (!P||!fit||!best||!child){free(P);free(fit);free(best);free(child);return -1;}
    for (i = 0; i < pop; i++) {
        P[i] = (double *)malloc((size_t)d*sizeof(double));
        for (j = 0; j < d; j++) P[i][j] = lo[j] + _ga_rnd(&sd)*(hi[j]-lo[j]);
        fit[i] = f(P[i], ctx);
        if (fit[i] < bf) { bf = fit[i]; for (j=0;j<d;j++) best[j]=P[i][j]; }
    }
    for (g = 0; g < gens; g++) {
        for (i = 0; i < pop; i++) {
            int a=_ga_rnd(&sd)*pop, b=_ga_rnd(&sd)*pop, c=_ga_rnd(&sd)*pop, p = fit[a]<fit[b]?a:b; p = fit[p]<fit[c]?p:c;
            int q1=_ga_rnd(&sd)*pop, q2=_ga_rnd(&sd)*pop, q3=_ga_rnd(&sd)*pop, m = fit[q1]<fit[q2]?q1:q2; m = fit[m]<fit[q3]?m:q3;
            for (j = 0; j < d; j++) {
                double alpha = _ga_rnd(&sd);
                child[j] = alpha*P[p][j] + (1.0-alpha)*P[m][j];
                if (_ga_rnd(&sd) < 0.1) child[j] += 0.2*(_ga_rnd(&sd)-0.5)*(hi[j]-lo[j]);
                if (child[j] < lo[j]) child[j] = lo[j];
                if (child[j] > hi[j]) child[j] = hi[j];
            }
            { double cf = f(child, ctx);
              if (cf < fit[i]) { for (j=0;j<d;j++) P[i][j]=child[j]; fit[i]=cf;
                  if (cf < bf) { bf = cf; for (j=0;j<d;j++) best[j]=child[j]; } } }
        }
    }
    for (j = 0; j < d; j++) out_x[j] = best[j];
    for (i = 0; i < pop; i++) free(P[i]);
    free(P); free(fit); free(best); free(child);
    return 0;
}
