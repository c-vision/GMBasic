/* gs_sim_finance.c — funzioni di finanza quantitativa rimaste in gmbstats. */
#include "gs_sim.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

double gs_cokurt(const double *x, const double *y, const double *z, const double *w, int n)
{
    double mx=0,my=0,mz=0,mw=0,s=0; int i;
    if (!x||!y||!z||!w||n<1) return 0.0/0.0;
    for (i=0;i<n;i++){ mx+=x[i]; my+=y[i]; mz+=z[i]; mw+=w[i]; }
    mx/=n; my/=n; mz/=n; mw/=n;
    for (i=0;i<n;i++) s += (x[i]-mx)*(y[i]-my)*(z[i]-mz)*(w[i]-mw);
    return s/n;
}
double gs_portfolio_cokurt(const double *R, int T, int N, const double *w)
{
    double *p, mp=0, s=0; int t,k;
    if (!R||!w||T<1||N<1) return 0.0/0.0;
    p=(double*)malloc((size_t)T*sizeof(double)); if(!p) return 0.0/0.0;
    for (t=0;t<T;t++){ double v=0; for(k=0;k<N;k++) v+=w[k]*R[t*N+k]; p[t]=v; mp+=v; }
    mp/=T;
    for (t=0;t<T;t++){ double d=p[t]-mp; s+=d*d*d*d; }
    free(p);
    return s/T;
}

int gs_msm_transition_probs(int kbar, double b, double gamma_kbar, double *out)
{
    int i; double g1;
    if (!out || kbar < 1 || b <= 0.0) return -1;
    g1 = 1.0 - pow(1.0 - gamma_kbar, 1.0/pow(b, (double)(kbar-1)));
    out[0] = g1;
    for (i = 1; i < kbar; i++) out[i] = 1.0 - pow(1.0 - g1, pow(b, (double)i));
    return 0;
}
int gs_msm_simulate(int kbar, double b, double m0, double gamma_kbar, double sigma,
                    int T, unsigned seed, double *out)
{
    double *g, m1 = 2.0 - m0, *mult; unsigned s = seed ? seed : 12345u; int i, j;
    if (!out || kbar < 1 || T < 1) return -1;
    g = (double*)malloc((size_t)kbar*sizeof(double));
    mult = (double*)malloc((size_t)kbar*sizeof(double));
    if (!g || !mult) { free(g); free(mult); return -1; }
    gs_msm_transition_probs(kbar, b, gamma_kbar, g);
    for (i = 0; i < kbar; i++) mult[i] = (((s>>8)&1) ? m1 : m0), (void)s;
    for (i = 0; i < T; i++) {
        double u, M = 1.0, z;
        for (j = 0; j < kbar; j++) {
            s ^= s<<13; s ^= s>>17; s ^= s<<5; if (!s) s = 0x9e3779b9u;
            u = (double)(s >> 8)/16777216.0;
            if (u < g[j]) mult[j] = ((s>>9)&1) ? m0 : m1;
            M *= mult[j];
        }
        { double u1, u2;
          do { s ^= s<<13; s ^= s>>17; s ^= s<<5; if(!s)s=1u; u1=(double)(s>>8)/16777216.0; } while (u1<1e-12);
          s ^= s<<13; s ^= s>>17; s ^= s<<5; if(!s)s=1u; u2=(double)(s>>8)/16777216.0;
          z = sqrt(-2.0*log(u1))*cos(2.0*3.141592653589793*u2); }
        out[i] = sqrt(M)*sigma*z;
    }
    free(g); free(mult);
    return 0;
}
