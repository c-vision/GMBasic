/* gs_num.c — metodi numerici/statistici da AI/Math. */
#include "gs_num.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

double gs_mc_antithetic_mean(const gc_real *h_u, const gc_real *h_1mu, int n)
{
    int i; double s = 0.0;
    if (!h_u || !h_1mu || n < 1) return 0.0/0.0;
    for (i = 0; i < n; i++) s += 0.5*(h_u[i] + h_1mu[i]);
    return s/(double)n;
}

double gs_mc_control_variate(const gc_real *y, const gc_real *x, int n,
                             gc_real mu_x, gc_real *out_beta)
{
    int i; double my=0, mx=0, sxy=0, sxx=0, beta=0, res;
    if (!y || !x || n < 2) return 0.0/0.0;
    for (i=0;i<n;i++){ my+=y[i]; mx+=x[i]; }
    my/=n; mx/=n;
    for (i=0;i<n;i++){ double a=x[i]-mx; sxy+=a*(y[i]-my); sxx+=a*a; }
    if (sxx > 0.0) beta = sxy/sxx;
    res = (my - beta*(mx - mu_x));
    if (out_beta) *out_beta = beta;
    return res;
}

double gs_mc_importance_mean(const gc_real *h_over_p, int n)
{
    int i; double s=0.0;
    if (!h_over_p || n < 1) return 0.0/0.0;
    for (i=0;i<n;i++) s += h_over_p[i];
    return s/(double)n;
}

double gs_halton(int index, int base)
{
    double f = 1.0, r = 0.0;
    int i = index;
    if (base < 2 || index < 0) return 0.0/0.0;
    while (i > 0) { f /= (double)base; r += f*(double)(i % base); i /= base; }
    return r;
}

static int prime_at(int j)
{
    int cand = 2, cnt = -1, d, isp;
    if (j <= 0) return 2;
    for (cand = 2; ; cand++) {
        isp = 1;
        for (d = 2; d*d <= cand; d++) if (cand % d == 0) { isp = 0; break; }
        if (isp) { cnt++; if (cnt == j) return cand; }
    }
}

int gs_halton_seq(int n, int dim, gc_real *out)
{
    int i, d;
    if (!out || n < 1 || dim < 1) return -1;
    for (i = 1; i <= n; i++)
        for (d = 0; d < dim; d++)
            out[(i-1)*dim + d] = gs_halton(i, prime_at(d));
    return 0;
}

int gs_bootstrap_block(const gc_real *x, int n, int block, int nrep,
                       gs_rng *rng, gc_real *out_mean, gc_real *out_se)
{
    double *means;
    double m = 0.0, s = 0.0;
    int rep;
    if (!x || !rng || n < 2 || block < 1 || nrep < 1) return -1;
    means = malloc(sizeof(double)*nrep);
    if (!means) return -1;
    for (rep = 0; rep < nrep; rep++) {
        double acc = 0.0; int got = 0;
        while (got < n) {
            int start = (int)(gs_rng_uniform(rng)*(double)(n-block+1));
            int j; if (start < 0) start = 0;
            if (start > n-block) start = n-block;
            for (j = 0; j < block && got < n; j++) { acc += x[start+j]; got++; }
        }
        means[rep] = acc/(double)n;
    }
    for (rep=0;rep<nrep;rep++) m += means[rep];
    m /= (double)nrep;
    for (rep=0;rep<nrep;rep++){ double d=means[rep]-m; s += d*d; }
    if (out_mean) *out_mean = m;
    if (out_se) *out_se = sqrt(s/(double)(nrep-1));
    free(means);
    return 0;
}

int gs_jackknife_mean(const gc_real *x, int n, gc_real *out_mean, gc_real *out_se)
{
    int i; double mean = 0.0, s = 0.0;
    if (!x || n < 2) return -1;
    for (i=0;i<n;i++) mean += x[i];
    mean /= (double)n;
    for (i=0;i<n;i++) {
        double mean_i = (mean*(double)n - x[i])/(double)(n-1);
        s += (mean_i - mean)*(mean_i - mean);
    }
    if (out_mean) *out_mean = mean;
    if (out_se) *out_se = sqrt((double)(n-1)/(double)n * s);
    return 0;
}

static int cmp_asc(const void *a, const void *b)
{
    double x = *(const double*)a, y = *(const double*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

int gs_evt_hill(const gc_real *x, int n, int k, gc_real *out_xi)
{
    double *s; int i; double sum = 0.0;
    if (!x || !out_xi || n < 2 || k < 2 || k >= n) return -1;
    s = malloc(sizeof(double)*n);
    if (!s) return -1;
    memcpy(s, x, sizeof(double)*n);
    qsort(s, n, sizeof(double), cmp_asc);
    for (i = n-k; i < n-1; i++) {
        if (s[i] > 0.0) sum += log(s[n-1]/s[i]);
    }
    *out_xi = sum/(double)(k-1);
    free(s);
    return 0;
}

int gs_evt_gpd_fit(const gc_real *exceed, int n, gc_real *out_xi, gc_real *out_beta)
{
    int i; double m=0, v=0, xi, beta;
    if (!exceed || !out_xi || !out_beta || n < 3) return -1;
    for (i=0;i<n;i++) m += exceed[i];
    m /= (double)n;
    for (i=0;i<n;i++){ double d=exceed[i]-m; v += d*d; }
    v /= (double)(n-1);
    if (v <= 0.0 || m <= 0.0) return -1;
    xi = 0.5*(1.0 - m*m/v);
    beta = 0.5*m*(m*m/v + 1.0);
    *out_xi = xi; *out_beta = beta;
    return 0;
}

int gs_evt_pot_var_es(gc_real thr, gc_real xi, gc_real beta, int n, int nexc,
                      gc_real p, gc_real *out_var, gc_real *out_es)
{
    gc_real lam, z, var, es;
    if (n < 1 || nexc < 1 || p <= 0.0 || p >= 1.0) return -1;
    lam = (gc_real)nexc/(gc_real)n;
    if (p >= lam) { if (out_var) *out_var = thr; if (out_es) *out_es = thr + beta/(1.0-xi); return 0; }
    if (xi == 0.0) {
        var = thr + beta*log(lam/p);
        es = var + beta;
    } else {
        z = beta/xi * (pow(lam/p, xi) - 1.0);
        var = thr + z;
        es = var + (beta + xi*(var - thr))/(1.0 - xi);
    }
    if (out_var) *out_var = var;
    if (out_es) *out_es = es;
    return 0;
}

int gs_tail_dependence(const gc_real *x, const gc_real *y, int n, int k,
                       gc_real *out_upper, gc_real *out_lower)
{
    double *sx, *sy; int i, cu=0, cl=0;
    if (!x || !y || n < 3 || k < 2 || k >= n) return -1;
    sx = malloc(sizeof(double)*n); sy = malloc(sizeof(double)*n);
    if (!sx || !sy) { free(sx); free(sy); return -1; }
    memcpy(sx, x, sizeof(double)*n); memcpy(sy, y, sizeof(double)*n);
    qsort(sx, n, sizeof(double), cmp_asc);
    qsort(sy, n, sizeof(double), cmp_asc);
    { gc_real ux = sx[n-k-1], uy = sy[n-k-1];    /* soglia: oltre il k-esimo estremo */
      gc_real lx = sx[k], ly = sy[k];
      for (i=0;i<n;i++) {
          if (x[i] > ux && y[i] > uy) cu++;
          if (x[i] < lx && y[i] < ly) cl++;
      }
    }
    if (out_upper) *out_upper = (gc_real)cu/(gc_real)k;
    if (out_lower) *out_lower = (gc_real)cl/(gc_real)k;
    free(sx); free(sy);
    return 0;
}

int gs_gmm_em_1d(const gc_real *x, int n, int k, int iters,
                 gc_real *out_mu, gc_real *out_var, gc_real *out_w)
{
    double *mu, *var, *w, *gam, gmin, gmax, gmean = 0.0, gvar = 0.0;
    int i, j, it;
    if (!x || !out_mu || !out_var || !out_w || n < 2 || k < 1) return -1;
    mu = malloc(sizeof(double)*k); var = malloc(sizeof(double)*k);
    w = malloc(sizeof(double)*k); gam = malloc(sizeof(double)*(size_t)n*k);
    if (!mu||!var||!w||!gam){ free(mu);free(var);free(w);free(gam); return -1; }
    gmin = gmax = x[0];
    for (i=0;i<n;i++){ gmean += x[i]; if (x[i]<gmin) gmin=x[i]; if (x[i]>gmax) gmax=x[i]; }
    gmean /= n;
    for (i=0;i<n;i++) gvar += (x[i]-gmean)*(x[i]-gmean);
    gvar /= n;
    if (gvar <= 0.0) gvar = 1e-6;
    for (j=0;j<k;j++) { mu[j] = gmin + (gmax-gmin)*(j+0.5)/k; var[j] = gvar; w[j] = 1.0/k; }
    for (it=0; it<iters; it++) {
        for (i=0;i<n;i++) {
            double tot = 0.0;
            for (j=0;j<k;j++) {
                double d = x[i]-mu[j];
                double g = w[j]/sqrt(2.0*3.141592653589793*var[j]) * exp(-0.5*d*d/var[j]);
                gam[(size_t)i*k+j] = g; tot += g;
            }
            if (tot <= 0.0) tot = 1e-300;
            for (j=0;j<k;j++) gam[(size_t)i*k+j] /= tot;
        }
        for (j=0;j<k;j++) {
            double nk=0, mk=0, vk=0;
            for (i=0;i<n;i++) nk += gam[(size_t)i*k+j];
            if (nk < 1e-12) { w[j] = 1e-12; continue; }
            for (i=0;i<n;i++) mk += gam[(size_t)i*k+j]*x[i];
            mk /= nk;
            for (i=0;i<n;i++){ double d=x[i]-mk; vk += gam[(size_t)i*k+j]*d*d; }
            vk /= nk;
            mu[j]=mk; var[j]= vk>1e-12?vk:1e-12; w[j]=nk/(double)n;
        }
    }
    for (j=0;j<k;j++){ out_mu[j]=mu[j]; out_var[j]=var[j]; out_w[j]=w[j]; }
    free(mu);free(var);free(w);free(gam);
    return 0;
}

static int solve_spd(double *A, double *b, int n)
{
    int i, j, k;
    for (i=0;i<n;i++) {
        for (j=0;j<=i;j++) {
            double s = A[(size_t)i*n+j];
            for (k=0;k<j;k++) s -= A[(size_t)i*n+k]*A[(size_t)j*n+k];
            if (i==j) {
                int tries=0;
                while (s <= 1e-12 && tries < 6) { A[(size_t)i*n+i] += 1e-8; s += 1e-8; tries++; }
                if (s <= 0.0) return -1;
                A[(size_t)i*n+i] = sqrt(s);
            } else A[(size_t)i*n+j] = s / A[(size_t)j*n+j];
        }
    }
    for (i=0;i<n;i++) { double s=b[i]; for (k=0;k<i;k++) s -= A[(size_t)i*n+k]*b[k]; b[i]=s/A[(size_t)i*n+i]; }
    for (i=n-1;i>=0;i--){ double s=b[i]; for (k=i+1;k<n;k++) s -= A[(size_t)k*n+i]*b[k]; b[i]=s/A[(size_t)i*n+i]; }
    return 0;
}

int gs_gp_regression(const gc_real *x, const gc_real *y, int n,
                     const gc_real *xs, int m, gc_real l2, gc_real noise,
                     gc_real *out_mean, gc_real *out_var)
{
    double *K, *K0, *alpha, *kx, *v;
    int i, j, q;
    if (!x || !y || !xs || !out_mean || !out_var || n < 1 || m < 1 || l2 <= 0.0) return -1;
    K = malloc(sizeof(double)*(size_t)n*n);
    K0 = malloc(sizeof(double)*(size_t)n*n);
    alpha = malloc(sizeof(double)*n);
    kx = malloc(sizeof(double)*n);
    v = malloc(sizeof(double)*n);
    if (!K||!K0||!alpha||!kx||!v){ free(K);free(K0);free(alpha);free(kx);free(v); return -1; }
    for (i=0;i<n;i++) for (j=0;j<n;j++) {
        double d = x[i]-x[j];
        K[(size_t)i*n+j] = exp(-0.5*d*d/l2) + (i==j ? noise : 0.0);
    }
    /* solve_spd fattorizza K IN PLACE: serve la matrice originale per ogni RHS
     * (la versione precedente riusava il fattore Cholesky come se fosse K, quindi
     * la varianza predetta collassava a `noise`). */
    memcpy(K0, K, sizeof(double)*(size_t)n*n);
    for (i=0;i<n;i++) alpha[i] = y[i];
    if (solve_spd(K, alpha, n) != 0) { free(K);free(K0);free(alpha);free(kx);free(v); return -1; }
    for (q=0;q<m;q++) {
        double mu = 0.0, kv;
        for (i=0;i<n;i++) { double d=xs[q]-x[i]; kx[i]=exp(-0.5*d*d/l2); mu += kx[i]*alpha[i]; }
        for (i=0;i<n;i++) v[i]=kx[i];
        memcpy(K, K0, sizeof(double)*(size_t)n*n);
        if (solve_spd(K, v, n) != 0) { out_mean[q]=mu; out_var[q]=noise; continue; }
        kv = 1.0 + noise;
        for (i=0;i<n;i++) kv -= kx[i]*v[i];
        out_mean[q] = mu;
        out_var[q] = kv > 0.0 ? kv : 0.0;
    }
    free(K);free(K0);free(alpha);free(kx);free(v);
    return 0;
}

int gs_particle_filter(const gc_real *obs, int T, gc_real a, gc_real q,
                       gc_real r, int N, gs_rng *rng,
                       gc_real *out_mean, gc_real *out_var)
{
    double *part, *w, *cdf, m = 0.0, v = 0.0;
    int t, i;
    if (!obs || !rng || !out_mean || !out_var || T < 1 || N < 2) return -1;
    part = malloc(sizeof(double)*N); w = malloc(sizeof(double)*N); cdf = malloc(sizeof(double)*N);
    if (!part||!w||!cdf){ free(part);free(w);free(cdf); return -1; }
    for (i=0;i<N;i++) part[i] = obs[0] + sqrt(r)*gs_rng_normal(rng);
    for (t=0;t<T;t++) {
        double wsum = 0.0;
        for (i=0;i<N;i++) {
            if (t > 0) part[i] = a*part[i] + sqrt(q)*gs_rng_normal(rng);
            { double d = obs[t]-part[i]; w[i] = exp(-0.5*d*d/r); }
            wsum += w[i];
        }
        if (wsum <= 0.0) wsum = 1e-300;
        for (i=0;i<N;i++) w[i] /= wsum;
        m = 0.0; for (i=0;i<N;i++) m += w[i]*part[i];
        v = 0.0; for (i=0;i<N;i++){ double d=part[i]-m; v += w[i]*d*d; }
        out_mean[t] = m; out_var[t] = v;
        { double acc = 0.0; for (i=0;i<N;i++){ acc += w[i]; cdf[i]=acc; }
          if (acc > 0.0) for (i=0;i<N;i++) cdf[i]/=acc; }
        { double *np = malloc(sizeof(double)*N);
          if (np) {
              for (i=0;i<N;i++) {
                  double u = gs_rng_uniform(rng); int lo=0, hi=N-1, mid;
                  while (lo < hi) { mid=(lo+hi)/2; if (cdf[mid] < u) lo=mid+1; else hi=mid; }
                  np[i] = part[lo];
              }
              memcpy(part, np, sizeof(double)*N); free(np);
          }
        }
    }
    free(part);free(w);free(cdf);
    return 0;
}

int gs_butterworth_lp(const double *x, int n, double period, int order, double *out)
{
    double K, K2, norm, b0, b1, b2, a1, a2, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    int i;
    if (!x || !out || n < 1 || period <= 0.0) return -1;
    K = tan(3.141592653589793/period);
    if (order <= 1) {
        norm = 1.0/(1.0 + K);
        b0 = K*norm; b1 = b0;
        a1 = (K - 1.0)*norm;
        for (i = 0; i < n; i++) {
            double y = b0*x[i] + b1*x1 - a1*y1;
            out[i] = y; x1 = x[i]; y1 = y;
        }
        return 0;
    }
    K2 = K*K;
    norm = 1.0/(1.0 + 1.4142135623730951*K + K2);
    b0 = K2*norm; b1 = 2.0*b0; b2 = b0;
    a1 = 2.0*(K2 - 1.0)*norm;
    a2 = (1.0 - 1.4142135623730951*K + K2)*norm;
    for (i = 0; i < n; i++) {
        double y = b0*x[i] + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        out[i] = y; x2 = x1; x1 = x[i]; y2 = y1; y1 = y;
    }
    return 0;
}
int gs_butterworth_bp(const double *x, int n, double low_period, double high_period,
                      int order, double *out)
{
    double *lohi; int i, r;
    if (!x || !out || n < 1) return -1;
    lohi = (double*)malloc((size_t)n*sizeof(double));
    if (!lohi) return -1;
    r = gs_butterworth_lp(x, n, high_period, order, out);   /* passa le alte freq fino a high */
    if (r == 0) r = gs_butterworth_lp(x, n, low_period, order, lohi);
    if (r == 0) for (i = 0; i < n; i++) out[i] -= lohi[i];  /* togli le freq basse (< low) */
    free(lohi);
    return r;
}
