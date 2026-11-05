/* gs_extra.c — information geometry, convex analysis, stat-mech, TDA, Landau. */
#include "gs_extra.h"
#include <math.h>
#include <stdlib.h>

int gs_fisher_info(const double *scores, int n_obs, int n_par, double *out_F)
{
    int t, a, c;
    if (!scores || !out_F || n_obs < 1 || n_par < 1) return -1;
    for (a = 0; a < n_par; a++) for (c = 0; c < n_par; c++) out_F[a*n_par+c] = 0.0;
    for (t = 0; t < n_obs; t++)
        for (a = 0; a < n_par; a++)
            for (c = 0; c < n_par; c++)
                out_F[a*n_par+c] += scores[t*n_par+a]*scores[t*n_par+c];
    for (a = 0; a < n_par*n_par; a++) out_F[a] /= n_obs;
    return 0;
}

int gs_natural_gradient(const double *grad, const double *F, int n, double *out)
{
    double *M, *inv; int i, j, a;
    if (!grad || !F || !out || n < 1) return -1;
    M = (double *)malloc((size_t)n*n*sizeof(double));
    inv = (double *)malloc((size_t)n*n*sizeof(double));
    if (!M || !inv) { free(M); free(inv); return -1; }
    for (i = 0; i < n*n; i++) { M[i] = F[i]; inv[i] = (i%(n+1)==0); }
    for (i = 0; i < n; i++) {
        int piv = i; double mx = fabs(M[i*n+i]);
        for (a = i+1; a < n; a++) if (fabs(M[a*n+i]) > mx) { mx=fabs(M[a*n+i]); piv=a; }
        if (mx < 1e-14) { free(M); free(inv); return -1; }
        if (piv != i) { for (j=0;j<n;j++){double t=M[i*n+j];M[i*n+j]=M[piv*n+j];M[piv*n+j]=t; t=inv[i*n+j];inv[i*n+j]=inv[piv*n+j];inv[piv*n+j]=t;} }
        { double d=M[i*n+i]; for(j=0;j<n;j++){M[i*n+j]/=d;inv[i*n+j]/=d;} }
        for (a=0;a<n;a++) if (a!=i){ double f=M[a*n+i]; for(j=0;j<n;j++){M[a*n+j]-=f*M[i*n+j];inv[a*n+j]-=f*inv[i*n+j];} }
    }
    for (i = 0; i < n; i++) { double s = 0; for (j = 0; j < n; j++) s += inv[i*n+j]*grad[j]; out[i] = s; }
    free(M); free(inv);
    return 0;
}

double gs_cramer_rao_efficiency(double variance, double inv_fisher)
{
    if (variance <= 0.0) return 0.0;
    return inv_fisher/variance;
}

int gs_moreau_envelope(const double *f, double x0, double dx, int n, double lam, double *out)
{
    int i, j;
    if (!f || !out || n < 1 || lam <= 0.0) return -1;
    for (i = 0; i < n; i++) {
        double x = x0 + i*dx, best = 1e300;
        for (j = 0; j < n; j++) { double y = x0+j*dx, d = (x-y)*(x-y); double v = f[j] + d/(2.0*lam); if (v < best) best = v; }
        out[i] = best;
    }
    return 0;
}

int gs_soft_threshold(const double *v, int n, double lambda, double *out)
{
    int i;
    if (!v || !out || n < 1) return -1;
    for (i = 0; i < n; i++) {
        if (v[i] > lambda) out[i] = v[i]-lambda;
        else if (v[i] < -lambda) out[i] = v[i]+lambda;
        else out[i] = 0.0;
    }
    return 0;
}

int gs_maxwell_boltzmann(const double *v, int n, double kT, double *out)
{
    int i; double s = 0.0;
    if (!v || !out || n < 1 || kT <= 0.0) return -1;
    for (i = 0; i < n; i++) { out[i] = exp(-v[i]*v[i]/(2.0*kT)); s += out[i]; }
    if (s > 1e-300) for (i = 0; i < n; i++) out[i] /= s;
    return 0;
}

/* Persistenza 0-dim sublevel: union-find con nascita = f del rappresentante. */
static int _uf_find(int *p, int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }

int gs_persistence_1d(const double *f, int n, double *out_birth, double *out_death, int *out_n)
{
    int *order, *parent, *birth_idx, *active; int i, j, cnt = 0;
    if (!f || !out_birth || !out_death || n < 2) return -1;
    order = (int *)malloc((size_t)n*sizeof(int));
    parent = (int *)malloc((size_t)n*sizeof(int));
    birth_idx = (int *)malloc((size_t)n*sizeof(int));
    active = (int *)calloc((size_t)n, sizeof(int));
    if (!order||!parent||!birth_idx||!active) { free(order);free(parent);free(birth_idx);free(active); return -1; }
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 0; i < n; i++) for (j = i+1; j < n; j++) if (f[order[j]] < f[order[i]]) { int t=order[i]; order[i]=order[j]; order[j]=t; }
    for (i = 0; i < n; i++) { parent[i]=i; birth_idx[i]=i; }
    for (i = 0; i < n; i++) {
        int v = order[i]; active[v] = 1;
        { int nb[2], nn = 0, k;
          if (v-1 >= 0 && active[v-1]) nb[nn++] = _uf_find(parent, v-1);
          if (v+1 < n && active[v+1]) nb[nn++] = _uf_find(parent, v+1);
          if (nn == 0) { parent[v]=v; birth_idx[v]=v; }
          else {
              int keep = nb[0], kk;
              for (k = 1; k < nn; k++) {
                  int other = nb[k];
                  if (other != keep) {
                      /* il componente con nascita piu' alta muore ora */
                      int bi = birth_idx[keep], oi = birth_idx[other];
                      int die = (f[bi] >= f[oi]) ? bi : oi;   /* piu' recente (f piu' alta) */
                      int surv = (f[bi] >= f[oi]) ? oi : bi;
                      out_birth[cnt] = f[die]; out_death[cnt] = f[v]; cnt++;
                      keep = _uf_find(parent, surv);
                      birth_idx[keep] = surv;
                  }
              }
              kk = _uf_find(parent, v);
              parent[v] = keep; parent[kk] = keep;
              parent[keep] = keep;
              if (f[birth_idx[v]] < f[birth_idx[keep]]) birth_idx[keep] = birth_idx[v];
          }
        }
    }
    /* componente finale: morte = massimo */
    { double mx = f[0]; for (i = 1; i < n; i++) if (f[i] > mx) mx = f[i];
      { int root = _uf_find(parent, order[n-1]);
        out_birth[cnt] = f[birth_idx[root]]; out_death[cnt] = mx; cnt++; } }
    if (out_n) *out_n = cnt;
    free(order); free(parent); free(birth_idx); free(active);
    return 0;
}

static int _cmpd(const void *a, const void *b) { double x=*(const double*)a,y=*(const double*)b; return x<y?-1:(x>y?1:0); }

double gs_bottleneck_distance(const double *b1, const double *d1, int n1,
                              const double *b2, const double *d2, int n2)
{
    double *sb1,*sd1,*sb2,*sd2; double mx = 0.0; int i, m;
    if (!b1||!d1||!b2||!d2||n1<1||n2<1) return 0.0;
    m = n1 < n2 ? n1 : n2;
    sb1=(double*)malloc((size_t)n1*sizeof(double)); sd1=(double*)malloc((size_t)n1*sizeof(double));
    sb2=(double*)malloc((size_t)n2*sizeof(double)); sd2=(double*)malloc((size_t)n2*sizeof(double));
    if (!sb1||!sd1||!sb2||!sd2){free(sb1);free(sd1);free(sb2);free(sd2);return 0.0;}
    for(i=0;i<n1;i++){sb1[i]=b1[i];sd1[i]=d1[i];} for(i=0;i<n2;i++){sb2[i]=b2[i];sd2[i]=d2[i];}
    qsort(sb1,n1,sizeof(double),_cmpd); qsort(sd1,n1,sizeof(double),_cmpd);
    qsort(sb2,n2,sizeof(double),_cmpd); qsort(sd2,n2,sizeof(double),_cmpd);
    for (i = 0; i < m; i++) {
        double db = fabs(sb1[i]-sb2[i]), dd = fabs(sd1[i]-sd2[i]);
        if (db > mx) mx = db; if (dd > mx) mx = dd;
    }
    /* eccedenti matchati alla diagonale: contributo (death-birth)/2 */
    { int k; for (k = m; k < n1; k++) { double hp = (sd1[k]-sb1[k])/2.0; if (hp>mx) mx=hp; }
      for (k = m; k < n2; k++) { double hp = (sd2[k]-sb2[k])/2.0; if (hp>mx) mx=hp; } }
    free(sb1);free(sd1);free(sb2);free(sd2);
    return mx;
}

double gs_landau_free_energy(double a, double b, double phi)
{
    return 0.5*a*phi*phi + 0.25*b*phi*phi*phi*phi;
}

/* ---------- Cornish-Fisher VaR ---------- */
static double _invnorm_cf(double p)
{
    double a[6]={-3.969683028665376e+01,2.209460984245205e+02,-2.759285104469687e+02,1.383577518672690e+02,-3.066479806614716e+01,2.506628277459239e+00};
    double b[5]={-5.447609879822406e+01,1.615858368580409e+02,-1.556989798598866e+02,6.680131188771972e+01,-1.328068155288572e+01};
    double c[6]={-7.784894002430293e-03,-3.223964580411365e-01,-2.400758277161838e+00,-2.549732539343734e+00,4.374664141464968e+00,2.938163982698783e+00};
    double d[4]={7.784695709041462e-03,3.224671290700398e-01,2.445134137142996e+00,3.754408661907416e+00};
    double q, r;
    if (p<=0) return -1e9; if (p>=1) return 1e9;
    if (p < 0.02425) { q=sqrt(-2*log(p)); return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])/((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1); }
    if (p > 0.97575) { q=sqrt(-2*log(1-p)); return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])/((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1); }
    q=p-0.5; r=q*q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q/(((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
}

/* VaR Cornish-Fisher (loss positiva) con skew/kurtosi. */
double gs_cornish_fisher_var(double mu, double sigma, double skew, double kurt, double conf)
{
    double z, zcf;
    if (sigma <= 0.0) return 0.0/0.0;
    z = _invnorm_cf(1.0-conf);   /* coda sinistra */
    zcf = z + (z*z-1.0)*skew/6.0 + (z*z*z-3.0*z)*kurt/24.0 - (2.0*z*z*z-5.0*z)*skew*skew/36.0;
    return -(mu + zcf*sigma);
}
