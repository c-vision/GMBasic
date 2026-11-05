/* gs_micro.c — PIN (Easley-O'Hara) + Gibbs sampler (AI/Algorithms deep-mining). */
#include "gs_algo.h"
#include <math.h>
#include <stdlib.h>

/* Probability of Informed Trading: PIN = alpha*mu/(alpha*mu + eps_b + eps_s). */


/* Gibbs sampler per una normale bivariata. out_x1/out_x2: n campioni post burn-in. */
int gs_mcmc_gibbs(double mu1, double mu2, double s1, double s2, double rho,
                  int n, int burn, long seed, double *out_x1, double *out_x2)
{
    int i; double x1 = mu1, x2 = mu2, sd1, sd2, c;
    long sd = seed ? seed : 1;
    if (!out_x1 || !out_x2 || n < 1 || s1 <= 0.0 || s2 <= 0.0) return -1;
    sd1 = s1*sqrt(1.0-rho*rho); sd2 = s2*sqrt(1.0-rho*rho);
    for (i = 0; i < n + burn; i++) {
        double u1, u2, g1, g2;
        sd = sd*6364136223846793005LL + 1442695040888963407LL;
        u1 = ((sd>>11)&((1LL<<53)-1))/(double)(1LL<<53); if(u1<1e-12) u1=1e-12;
        sd = sd*6364136223846793005LL + 1442695040888963407LL;
        u2 = ((sd>>11)&((1LL<<53)-1))/(double)(1LL<<53);
        g1 = sqrt(-2*log(u1))*cos(6.283185307179586*u2);
        sd = sd*6364136223846793005LL + 1442695040888963407LL;
        u1 = ((sd>>11)&((1LL<<53)-1))/(double)(1LL<<53); if(u1<1e-12) u1=1e-12;
        sd = sd*6364136223846793005LL + 1442695040888963407LL;
        u2 = ((sd>>11)&((1LL<<53)-1))/(double)(1LL<<53);
        g2 = sqrt(-2*log(u1))*cos(6.283185307179586*u2);
        /* x1 | x2 ~ N(mu1 + rho (s1/s2)(x2-mu2), sd1) */
        c = rho*(s1/s2);
        x1 = mu1 + c*(x2-mu2) + sd1*g1;
        c = rho*(s2/s1);
        x2 = mu2 + c*(x1-mu1) + sd2*g2;
        if (i >= burn) { out_x1[i-burn] = x1; out_x2[i-burn] = x2; }
    }
    return 0;
}
