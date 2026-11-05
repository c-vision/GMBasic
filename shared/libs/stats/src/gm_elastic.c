/* gm_elastic.c — elasticità isotropa (da poly3d_omp). */
#include "gm_elastic.h"

#include <math.h>

gm_status gm_elas_complete(gm_elas *e)
{
    int n = 0;
    gm_real E, nu, mu, K, lam;
    if (!e) return GM_ERR_ARG;
    if (!isnan(e->sm)) n++;
    if (!isnan(e->pr)) n++;
    if (!isnan(e->ym)) n++;
    if (!isnan(e->bm)) n++;
    if (!isnan(e->ll)) n++;
    if (n != 2) return GM_ERR_ARG;
    E = e->ym; nu = e->pr; mu = e->sm; K = e->bm; lam = e->ll;

    if (!isnan(mu) && !isnan(nu)) {                 /* μ, ν */
        E = 2.0 * mu * (1.0 + nu);
        lam = 2.0 * mu * nu / (1.0 - 2.0 * nu);
        K = 2.0 * mu * (1.0 + nu) / (3.0 * (1.0 - 2.0 * nu));
    } else if (!isnan(E) && !isnan(nu)) {           /* E, ν */
        mu = E / (2.0 * (1.0 + nu));
        lam = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
        K = E / (3.0 * (1.0 - 2.0 * nu));
    } else if (!isnan(K) && !isnan(mu)) {           /* K, μ */
        E = 9.0 * K * mu / (3.0 * K + mu);
        nu = (3.0 * K - 2.0 * mu) / (2.0 * (3.0 * K + mu));
        lam = K - 2.0 * mu / 3.0;
    } else if (!isnan(lam) && !isnan(mu)) {         /* λ, μ */
        E = mu * (3.0 * lam + 2.0 * mu) / (lam + mu);
        nu = lam / (2.0 * (lam + mu));
        K = lam + 2.0 * mu / 3.0;
    } else if (!isnan(E) && !isnan(mu)) {           /* E, μ */
        nu = E / (2.0 * mu) - 1.0;
        lam = 2.0 * mu * nu / (1.0 - 2.0 * nu);
        K = lam + 2.0 * mu / 3.0;
    } else if (!isnan(E) && !isnan(K)) {            /* E, K */
        mu = 3.0 * K * E / (9.0 * K - E);
        nu = (3.0 * K - 2.0 * mu) / (2.0 * (3.0 * K + mu));
        lam = K - 2.0 * mu / 3.0;
    } else if (!isnan(lam) && !isnan(nu)) {         /* λ, ν */
        mu = lam * (1.0 - 2.0 * nu) / (2.0 * nu);
        E = 2.0 * mu * (1.0 + nu);
        K = lam + 2.0 * mu / 3.0;
    } else if (!isnan(lam) && !isnan(K)) {          /* λ, K */
        mu = 3.0 * (K - lam) / 2.0;
        nu = lam / (3.0 * K - lam);
        E = 2.0 * mu * (1.0 + nu);
    } else if (!isnan(nu) && !isnan(K)) {           /* ν, K */
        mu = 3.0 * K * (1.0 - 2.0 * nu) / (2.0 * (1.0 + nu));
        E = 2.0 * mu * (1.0 + nu);
        lam = K - 2.0 * mu / 3.0;
    } else {                                        /* E, λ */
        gm_real t1 = 3.0 * lam - E;
        gm_real t2 = sqrt(t1 * t1 + 8.0 * lam * E);
        mu = (-t1 + t2) / 4.0;
        nu = lam / (2.0 * (lam + mu));
        K = lam + 2.0 * mu / 3.0;
    }
    e->sm = mu; e->pr = nu; e->ym = E; e->bm = K; e->ll = lam;
    return GM_OK;
}

void gm_strain_to_stress(const gm_real strain[9], gm_real shear_mod,
                         gm_real lame_lambda, gm_real stress[9])
{
    gm_real vol = strain[0] + strain[4] + strain[8];
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            stress[i * 3 + j] = (i == j)
                ? lame_lambda * vol + 2.0 * shear_mod * strain[i * 3 + j]
                : 2.0 * shear_mod * strain[i * 3 + j];
}

void gm_stress_to_strain(const gm_real stress[9], gm_real youngs_mod,
                         gm_real poisson, gm_real strain[9])
{
    gm_real tr = stress[0] + stress[4] + stress[8];
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            strain[i * 3 + j] = (i == j)
                ? ((1.0 + poisson) * stress[i * 3 + j] - poisson * tr) / youngs_mod
                : (1.0 + poisson) * stress[i * 3 + j] / youngs_mod;
}
