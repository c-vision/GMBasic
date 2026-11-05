/* gs_authors.c — metodi AI/Authors deep-mining: shrinkage, spread, entropia,
 * realized moments, Roll spread. */
#include "gs_authors.h"
#include <math.h>
#include <stdlib.h>

/* Ledoit-Wolf: shrinkage verso il modello a correlazione costante. */
int gs_ledoit_wolf(const double *rets, int T, int N, double *out_cov)
{
    double *S, *mu; int i, j, t; double rbar = 0.0, pi = 0.0, rho = 0.0, gamma = 0.0, delta;
    if (!rets || !out_cov || T < 2 || N < 1) return -1;
    S = (double *)calloc((size_t)N*N, sizeof(double));
    mu = (double *)calloc((size_t)N, sizeof(double));
    if (!S || !mu) { free(S); free(mu); return -1; }
    for (t = 0; t < T; t++) for (j = 0; j < N; j++) mu[j] += rets[t*N+j];
    for (j = 0; j < N; j++) mu[j] /= T;
    for (t = 0; t < T; t++) for (i = 0; i < N; i++) for (j = 0; j < N; j++)
        S[i*N+j] += (rets[t*N+i]-mu[i])*(rets[t*N+j]-mu[j]);
    for (i = 0; i < N*N; i++) S[i] /= (T-1);
    for (i = 0; i < N; i++) for (j = i+1; j < N; j++) { double d = sqrt(S[i*N+i]*S[j*N+j]); if (d>1e-15) rbar += S[i*N+j]/d; }
    rbar = (N>1) ? rbar/(N*(N-1)/2.0) : 0.0;
    for (i = 0; i < N; i++) for (j = 0; j < N; j++) {
        double f = (i==j) ? S[i*N+i] : rbar*sqrt(S[i*N+i]*S[j*N+j]);
        double d = f - S[i*N+j];
        gamma += d*d;
    }
    gamma /= N*N;
    for (t = 0; t < T; t++) for (i = 0; i < N; i++) for (j = 0; j < N; j++) {
        double e = (rets[t*N+i]-mu[i])*(rets[t*N+j]-mu[j]) - S[i*N+j];
        pi += e*e;
    }
    pi /= (double)T*T;
    (void)rho;
    delta = gamma > 1e-18 ? pi/gamma : 0.0;
    if (delta > 1.0) delta = 1.0; if (delta < 0.0) delta = 0.0;
    for (i = 0; i < N; i++) for (j = 0; j < N; j++) {
        double f = (i==j) ? S[i*N+i] : rbar*sqrt(S[i*N+i]*S[j*N+j]);
        out_cov[i*N+j] = delta*f + (1.0-delta)*S[i*N+j];
    }
    free(S); free(mu);
    return 0;
}

/* Corwin-Schultz high-low spread estimator. */
double gs_corwin_schultz(const double *high, const double *low, int n)
{
    int t, cnt = 0; double sum = 0.0, c = 3.0 - 2.0*sqrt(2.0);
    if (!high || !low || n < 2) return 0.0/0.0;
    for (t = 0; t < n-1; t++) {
        double b = log(high[t]/low[t]); b *= b;
        double b1 = log(high[t+1]/low[t+1]); b1 *= b1;
        double beta = b + b1;
        double h = high[t] > high[t+1] ? high[t] : high[t+1];
        double l = low[t] < low[t+1] ? low[t] : low[t+1];
        double gam = log(h/l); gam *= gam;
        double alpha = (sqrt(2.0*beta)-sqrt(beta))/c - sqrt(gam/c);
        double s = 2.0*(exp(alpha)-1.0)/(1.0+exp(alpha));
        if (s < 0.0) s = 0.0;
        sum += s; cnt++;
    }
    return cnt > 0 ? sum/cnt : 0.0/0.0;
}

/* Roll spread implicito: S = 2*sqrt(-Cov(dp_t, dp_{t-1})). */
double gs_roll_spread(const double *price, int n)
{
    int t, cnt = 0; double cov = 0.0, m = 0.0; double *dp;
    if (!price || n < 3) return 0.0/0.0;
    dp = (double *)malloc((size_t)n*sizeof(double));
    if (!dp) return 0.0/0.0;
    for (t = 1; t < n; t++) dp[t] = price[t]-price[t-1];
    for (t = 2; t < n; t++) m += dp[t];
    m /= (n-2);
    for (t = 2; t < n; t++) { cov += (dp[t]-m)*(dp[t-1]-m); cnt++; }
    cov /= cnt;
    free(dp);
    return cov < 0.0 ? 2.0*sqrt(-cov) : 0.0;
}

/* Realized skewness e kurtosis dei rendimenti. */
int gs_realized_skew_kurt(const double *r, int n, double *out_skew, double *out_kurt)
{
    int i; double mu = 0, m2 = 0, m3 = 0, m4 = 0, sd;
    if (!r || n < 3) return -1;
    for (i = 0; i < n; i++) mu += r[i]; mu /= n;
    for (i = 0; i < n; i++){ double d=r[i]-mu; m2+=d*d; m3+=d*d*d; m4+=d*d*d*d; }
    m2/=n; m3/=n; m4/=n; sd=sqrt(m2);
    if (out_skew) *out_skew = sd>1e-12 ? m3/(sd*sd*sd) : 0.0;
    if (out_kurt) *out_kurt = sd>1e-12 ? m4/(sd*sd*sd*sd)-3.0 : 0.0;
    return 0;
}

/* Mutual information (discreto, n_bins) tra due serie. */
double gs_mutual_information(const double *x, const double *y, int n, int n_bins)
{
    int i, bx, by; double *px, *py, *pxy; double mi = 0.0, xmin, xmax, ymin, ymax;
    if (!x || !y || n < 2 || n_bins < 2) return 0.0/0.0;
    px = (double *)calloc((size_t)n_bins, sizeof(double));
    py = (double *)calloc((size_t)n_bins, sizeof(double));
    pxy = (double *)calloc((size_t)n_bins*n_bins, sizeof(double));
    if (!px || !py || !pxy) { free(px); free(py); free(pxy); return 0.0/0.0; }
    xmin = xmax = x[0]; ymin = ymax = y[0];
    for (i = 1; i < n; i++) { if(x[i]<xmin)xmin=x[i]; if(x[i]>xmax)xmax=x[i]; if(y[i]<ymin)ymin=y[i]; if(y[i]>ymax)ymax=y[i]; }
    for (i = 0; i < n; i++) {
        bx = (int)((x[i]-xmin)/(xmax-xmin+1e-15)*n_bins); if(bx>=n_bins)bx=n_bins-1;
        by = (int)((y[i]-ymin)/(ymax-ymin+1e-15)*n_bins); if(by>=n_bins)by=n_bins-1;
        px[bx]++; py[by]++; pxy[bx*n_bins+by]++;
    }
    for (i = 0; i < n_bins; i++) { px[i]/=n; py[i]/=n; }
    for (i = 0; i < n_bins*n_bins; i++) pxy[i]/=n;
    for (bx = 0; bx < n_bins; bx++) for (by = 0; by < n_bins; by++) {
        double p = pxy[bx*n_bins+by];
        if (p > 1e-15 && px[bx] > 1e-15 && py[by] > 1e-15)
            mi += p*log(p/(px[bx]*py[by]));
    }
    free(px); free(py); free(pxy);
    return mi;
}

/* Transfer entropy X->Y: riduzione di incertezza su y_{t+1} data x_t (binned). */
double gs_transfer_entropy(const double *x, const double *y, int n, int n_bins)
{
    int i, bx, by, bn; double *cnt; double te = 0.0, xmin, xmax, ymin, ymax;
    if (!x || !y || n < 3 || n_bins < 2) return 0.0/0.0;
    xmin=xmax=x[0]; ymin=ymax=y[0];
    for (i=1;i<n;i++){ if(x[i]<xmin)xmin=x[i]; if(x[i]>xmax)xmax=x[i]; if(y[i]<ymin)ymin=y[i]; if(y[i]>ymax)ymax=y[i]; }
    cnt = (double *)calloc((size_t)n_bins*n_bins*n_bins, sizeof(double));
    if (!cnt) return 0.0/0.0;
    for (i = 1; i < n-1; i++) {
        bx = (int)((x[i]-xmin)/(xmax-xmin+1e-15)*n_bins); if(bx>=n_bins)bx=n_bins-1;
        by = (int)((y[i]-ymin)/(ymax-ymin+1e-15)*n_bins); if(by>=n_bins)by=n_bins-1;
        bn = (int)((y[i+1]-ymin)/(ymax-ymin+1e-15)*n_bins); if(bn>=n_bins)bn=n_bins-1;
        cnt[(bn*n_bins+by)*n_bins+bx] += 1.0;
    }
    { double tot = (double)(n-2);
      /* TE = sum p3(a,b,c) log[ p3(a,b,c) p(b) / (p2(a,b) p2(b,c)) ]
       * a=y_{t+1}, b=y_t, c=x_t */
      int a,b,c2;
      double *p2ab = (double *)calloc((size_t)n_bins*n_bins, sizeof(double));
      double *p2bc = (double *)calloc((size_t)n_bins*n_bins, sizeof(double));
      double *pb   = (double *)calloc((size_t)n_bins, sizeof(double));
      if (p2ab && p2bc && pb) {
        for (a=0;a<n_bins;a++) for(b=0;b<n_bins;b++) for(c2=0;c2<n_bins;c2++){
            double p = cnt[(a*n_bins+b)*n_bins+c2]/tot;
            if (p > 0.0) { p2ab[a*n_bins+b] += p; p2bc[b*n_bins+c2] += p; pb[b] += p; }
        }
        for (a=0;a<n_bins;a++) for(b=0;b<n_bins;b++) for(c2=0;c2<n_bins;c2++){
            double p = cnt[(a*n_bins+b)*n_bins+c2]/tot;
            double num, den;
            if (p <= 1e-15) continue;
            num = p * pb[b]; den = p2ab[a*n_bins+b] * p2bc[b*n_bins+c2];
            if (den > 1e-15 && num > 1e-15) te += p*log(num/den);
        }
        free(p2ab); free(p2bc); free(pb);
      }
    }
    free(cnt);
    return te;
}
