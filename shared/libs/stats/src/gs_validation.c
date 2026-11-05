/*
 * gs_validation.c — White Reality Check, Hansen SPA, BH-FDR, Qn robusto.
 */
#include "gs_validation.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* xorshift RNG deterministico (nessuna dipendenza). */
static unsigned gsv_rng(unsigned *s)
{
    unsigned x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x ? x : 0x9e3779b9u;
    return *s;
}

/* Genera un campione bootstrap stazionario: idx[t] in [0,T). */
static void gsv_stationary_idx(int *idx, int T, int window, unsigned *s)
{
    int t;
    double pnew = 1.0/(double)(window > 0 ? window : 1);
    idx[0] = (int)(gsv_rng(s) % (unsigned)T);
    for (t = 1; t < T; t++) {
        double u = (double)(gsv_rng(s) % 1000000u)/1000000.0;
        idx[t] = (u < pnew) ? (int)(gsv_rng(s) % (unsigned)T) : (idx[t-1]+1) % T;
    }
}

int gs_white_reality_check(const double *L, int T, int K, int B, int window,
                           unsigned seed, double *out_v, double *out_pvalue)
{
    double *mean, *vstar; int *idx; unsigned s = seed ? seed : 12345u;
    int b, k, t, count = 0; double V, sqT;
    if (!L || T < 2 || K < 1 || B < 1) return -1;
    mean = (double*)malloc((size_t)K*sizeof(double));
    vstar = (double*)malloc((size_t)B*sizeof(double));
    idx = (int*)malloc((size_t)T*sizeof(int));
    if (!mean || !vstar || !idx) { free(mean); free(vstar); free(idx); return -1; }
    sqT = sqrt((double)T);
    for (k = 0; k < K; k++) {
        double ssum = 0.0;
        for (t = 0; t < T; t++) ssum += L[(size_t)t*K + k];
        mean[k] = ssum/T;
    }
    V = mean[0];
    for (k = 1; k < K; k++) if (mean[k] > V) V = mean[k];
    V *= sqT;
    for (b = 0; b < B; b++) {
        double acc; double mx = -1e300;
        gsv_stationary_idx(idx, T, window, &s);
        for (k = 0; k < K; k++) {
            acc = 0.0;
            for (t = 0; t < T; t++) acc += L[(size_t)idx[t]*K + k];
            acc = sqT*(acc/T - mean[k]);
            if (acc > mx) mx = acc;
        }
        vstar[b] = mx;
        if (mx >= V) count++;
    }
    *out_v = V;
    *out_pvalue = (double)(count+1)/(double)(B+1);
    free(mean); free(vstar); free(idx);
    return 0;
}

int gs_hansen_spa(const double *L, int T, int K, int B, int window,
                  unsigned seed, double *out_v, double *out_pvalue)
{
    double *mean, *omega, *z, *vstar; int *idx; unsigned s = seed ? seed : 54321u;
    int b, k, t, count = 0; double V, sqT;
    if (!L || T < 2 || K < 1 || B < 1) return -1;
    mean = (double*)malloc((size_t)K*sizeof(double));
    omega = (double*)malloc((size_t)K*sizeof(double));
    z = (double*)malloc((size_t)K*sizeof(double));
    vstar = (double*)malloc((size_t)B*sizeof(double));
    idx = (int*)malloc((size_t)T*sizeof(int));
    if (!mean || !omega || !z || !vstar || !idx) { free(mean); free(omega); free(z); free(vstar); free(idx); return -1; }
    sqT = sqrt((double)T);
    for (k = 0; k < K; k++) {
        double ssum = 0.0; for (t = 0; t < T; t++) ssum += L[(size_t)t*K + k];
        mean[k] = ssum/T;
    }
    /* varianza bootstrap per studentizzazione */
    for (k = 0; k < K; k++) omega[k] = 0.0;
    for (b = 0; b < B; b++) {
        gsv_stationary_idx(idx, T, window, &s);
        for (k = 0; k < K; k++) {
            double acc = 0.0; for (t = 0; t < T; t++) acc += L[(size_t)idx[t]*K + k];
            acc = sqT*(acc/T - mean[k]);
            omega[k] += acc*acc;
        }
    }
    for (k = 0; k < K; k++) omega[k] = sqrt(omega[k]/(double)B);
    for (k = 0; k < K; k++) {
        if (omega[k] < 1e-12) omega[k] = 1e-12;
        z[k] = sqT*mean[k]/omega[k];
    }
    V = z[0]; for (k = 1; k < K; k++) if (z[k] > V) V = z[k];
    for (b = 0; b < B; b++) {
        double mx = -1e300;
        gsv_stationary_idx(idx, T, window, &s);
        for (k = 0; k < K; k++) {
            double acc = 0.0, zb; for (t = 0; t < T; t++) acc += L[(size_t)idx[t]*K + k];
            acc = sqT*(acc/T - mean[k]);
            zb = (acc + (z[k] > 0.0 ? z[k] : 0.0))/omega[k];
            if (zb > mx) mx = zb;
        }
        vstar[b] = mx;
        if (mx >= V) count++;
    }
    *out_v = V;
    *out_pvalue = (double)(count+1)/(double)(B+1);
    free(mean); free(omega); free(z); free(vstar); free(idx);
    return 0;
}

static int gsv_cmp_dbl(const void *a, const void *b)
{
    double x = *(const double*)a, y = *(const double*)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

int gs_fdr_bh(const double *pvals, int m, double alpha, int *reject, int *out_nreject)
{
    double *p; int *ord, i, kmax = -1;
    if (!pvals || m < 1) return -1;
    p = (double*)malloc((size_t)m*sizeof(double));
    ord = (int*)malloc((size_t)m*sizeof(int));
    if (!p || !ord) { free(p); free(ord); return -1; }
    for (i = 0; i < m; i++) { p[i] = pvals[i]; ord[i] = i; }
    for (i = 1; i < m; i++) { /* insertion sort con indici */
        double key = p[i]; int ki = ord[i], j = i-1;
        while (j >= 0 && p[j] > key) { p[j+1] = p[j]; ord[j+1] = ord[j]; j--; }
        p[j+1] = key; ord[j+1] = ki;
    }
    for (i = 0; i < m; i++) if (p[i] <= alpha*(double)(i+1)/(double)m) kmax = i;
    for (i = 0; i < m; i++) reject[i] = 0;
    for (i = 0; i <= kmax; i++) reject[ord[i]] = 1;
    if (out_nreject) *out_nreject = kmax+1;
    free(p); free(ord);
    return 0;
}

int gs_fdr_bh_adaptive(const double *pvals, int m, double alpha, int *reject, int *out_nreject)
{
    double *p; int *ord, i, kmax = -1, m0, i0, cnt; double sx, sy, sxx, sxy, slope, m0hat;
    if (!pvals || m < 4) return gs_fdr_bh(pvals, m, alpha, reject, out_nreject);
    p = (double*)malloc((size_t)m*sizeof(double));
    ord = (int*)malloc((size_t)m*sizeof(int));
    if (!p || !ord) { free(p); free(ord); return -1; }
    for (i = 0; i < m; i++) { p[i] = pvals[i]; ord[i] = i; }
    for (i = 1; i < m; i++) {
        double key = p[i]; int ki = ord[i], j = i-1;
        while (j >= 0 && p[j] > key) { p[j+1] = p[j]; ord[j+1] = ord[j]; j--; }
        p[j+1] = key; ord[j+1] = ki;
    }
    /* stima m0 dalla pendenza (regressione) sul tratto alto dei p-value ordinati:
       p_(i) ~ i/m0  =>  m0 = 1/slope. */
    i0 = m/2; sx = sy = sxx = sxy = 0.0; cnt = 0;
    for (i = i0; i < m; i++) { double x = (double)i, y = p[i]; sx += x; sy += y; sxx += x*x; sxy += x*y; cnt++; }
    slope = (cnt*sxx - sx*sx) > 1e-12 ? (cnt*sxy - sx*sy)/(cnt*sxx - sx*sx) : 0.0;
    m0hat = (slope > 1e-9) ? 1.0/slope : (double)m;
    if (m0hat > (double)m) m0hat = (double)m;
    m0 = (int)(m0hat + 0.5);
    if (m0 < 1) m0 = 1;
    for (i = 0; i < m; i++) if (p[i] <= alpha*(double)(i+1)/(double)m0) kmax = i;
    for (i = 0; i < m; i++) reject[i] = 0;
    for (i = 0; i <= kmax; i++) reject[ord[i]] = 1;
    if (out_nreject) *out_nreject = kmax+1;
    free(p); free(ord);
    return 0;
}

double gs_robust_qn(const double *x, int n)
{
    static const double corr[10] = {0.0,0.399,0.994,0.512,0.844,0.611,0.857,0.669,0.872,0.0};
    double *d; int np, i, j, h, k, idx; double qn;
    if (!x || n < 2) return 0.0/0.0;
    np = n*(n-1)/2;
    d = (double*)malloc((size_t)np*sizeof(double));
    if (!d) return 0.0/0.0;
    idx = 0;
    for (i = 0; i < n; i++)
        for (j = i+1; j < n; j++)
            d[idx++] = fabs(x[i]-x[j]);
    qsort(d, (size_t)np, sizeof(double), gsv_cmp_dbl);
    h = n/2 + 1;
    k = h*(h-1)/2;               /* C(h,2), 1-based */
    qn = 2.2219*d[k-1];
    if (n < 10) qn *= corr[n];
    free(d);
    return qn;
}


static double gsv_norm_inv(double p)
{
    double a[4]={2.50662823884,-18.61500062529,41.39119773534,-25.44106049637};
    double b[4]={-8.47351093090,23.08336743743,-21.06224101826,3.13082909833};
    double c[9]={0.3374754822726147,0.9761690190917186,0.1607979714918209,0.0276438810333863,
                 0.0038405729373609,0.0003951896511919,0.0000321767881768,0.0000002888167364,0.0000003960315187};
    double x=p-0.5,y,r;
    if (fabs(x)<0.42){ y=x*x; return x*(((a[3]*y+a[2])*y+a[1])*y+a[0])/((((b[3]*y+b[2])*y+b[1])*y+b[0])*y+1.0); }
    r=(p<0.5)?p:1.0-p; r=log(-log(r));
    y=c[0]+r*(c[1]+r*(c[2]+r*(c[3]+r*(c[4]+r*(c[5]+r*(c[6]+r*(c[7]+r*c[8])))))));
    return (p<0.5)?-y:y;
}

double gs_profit_hurdle(int N, double alpha, double rho)
{
    double neff, p0;
    if (N < 1 || alpha <= 0.0 || alpha >= 1.0) return 0.0/0.0;
    if (rho < 0.0) rho = 0.0; if (rho > 1.0) rho = 1.0;
    neff = 1.0 + ((double)N - 1.0)*(1.0 - rho);
    p0 = alpha/(2.0*neff);
    if (p0 <= 0.0) return 0.0/0.0;
    return gsv_norm_inv(1.0 - p0);
}

/* --- Cold Blood Index (Financial Hacker, the-cold-blood-index; Zorro ddscale.htm) --- */
double gs_cold_blood_index(const double *balance, int n, int width, double depth, int horizon)
{
    int M, T, i, N = 0;
    double P;
    if (!balance || n < 3 || width < 1 || depth <= 0.0 || horizon < 1) return 0.0 / 0.0;
    M = n - width + 1;
    T = horizon - width + 1;
    if (M <= 1 || T < 1 || M <= T) return 0.0 / 0.0;
    for (i = 0; i < M - 1; i++)
        if (balance[i + width] - balance[i] <= -depth) N++;
    if (horizon > width) {
        if (M - N - T < 0) P = 1.0;
        else P = 1.0 - exp(lgamma((double)(M - N + 1)) + lgamma((double)(M - T + 1))
                           - lgamma((double)(M + 1)) - lgamma((double)(M - N - T + 1)));
    } else {
        P = (double)N / (double)M;
    }
    return 100.0 * P;
}
