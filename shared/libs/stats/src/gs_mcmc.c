/* gs_mcmc.c — sampler MCMC ensemble affine-invariant (da Monte-Carlo-Markov-Chain-Library). */
#include "gs_mcmc.h"
#include "gs_sim.h"

#include <math.h>
#include <stdlib.h>

int gs_mcmc_stretch(int dim, int n_walkers, int n_samples, int burn_in, int thinning,
                    gs_logpdf_fn logpdf, void *ctx,
                    gc_real *walkers, gc_real *samples,
                    unsigned long long seed, gc_real *accept_ratio)
{
    gs_rng rng;
    gc_real *lp, *prop;
    int half, w, k, saved = 0, round = 0, max_rounds;
    long accepted = 0, proposed = 0;

    if (dim < 1 || n_walkers < 4 || (n_walkers & 1) || n_samples < 1
        || !logpdf || !walkers || !samples) return -1;
    if (thinning < 1) thinning = 1;
    if (burn_in < 0) burn_in = 0;
    rng = gs_rng_make(seed ? seed : 88172645463325252ULL);
    half = n_walkers / 2;
    lp = (gc_real *)malloc((size_t)n_walkers * sizeof(gc_real));
    prop = (gc_real *)malloc((size_t)dim * sizeof(gc_real));
    if (!lp || !prop) { free(lp); free(prop); return -1; }
    for (w = 0; w < n_walkers; w++) lp[w] = logpdf(walkers + (size_t)w * dim, ctx);

    /* limite di sicurezza sui round per evitare loop infiniti */
    max_rounds = (burn_in + 1) * 1000 + n_samples * thinning * 10 + 100000;

    while (saved < n_samples && round < max_rounds) {
        /* prima metà aggiornata usando la seconda come complemento */
        for (w = 0; w < half; w++) {
            int comp = half + gs_rng_int(&rng, n_walkers - half);
            gc_real z = (gs_rng_uniform(&rng) + 1.0);
            gc_real lz, u;
            z = z * z / 2.0;               /* z in [0.5, 2) */
            for (k = 0; k < dim; k++)
                prop[k] = walkers[(size_t)comp * dim + k]
                        + z * (walkers[(size_t)comp * dim + k] - walkers[(size_t)w * dim + k]);
            lz = logpdf(prop, ctx);
            proposed++;
            u = gs_rng_uniform(&rng);
            if (log(u) < (gc_real)(dim - 1) * log(z) + lz - lp[w]) {
                for (k = 0; k < dim; k++) walkers[(size_t)w * dim + k] = prop[k];
                lp[w] = lz;
                accepted++;
            }
        }
        /* seconda metà aggiornata usando la prima */
        for (w = half; w < n_walkers; w++) {
            int comp = gs_rng_int(&rng, half);
            gc_real z = (gs_rng_uniform(&rng) + 1.0);
            gc_real lz, u;
            z = z * z / 2.0;
            for (k = 0; k < dim; k++)
                prop[k] = walkers[(size_t)comp * dim + k]
                        + z * (walkers[(size_t)comp * dim + k] - walkers[(size_t)w * dim + k]);
            lz = logpdf(prop, ctx);
            proposed++;
            u = gs_rng_uniform(&rng);
            if (log(u) < (gc_real)(dim - 1) * log(z) + lz - lp[w]) {
                for (k = 0; k < dim; k++) walkers[(size_t)w * dim + k] = prop[k];
                lp[w] = lz;
                accepted++;
            }
        }
        round++;
        if (round > burn_in && (round % thinning == 0)) {
            for (w = 0; w < n_walkers && saved < n_samples; w++) {
                for (k = 0; k < dim; k++)
                    samples[(size_t)saved * dim + k] = walkers[(size_t)w * dim + k];
                saved++;
            }
        }
    }
    if (accept_ratio) *accept_ratio = proposed > 0 ? (gc_real)accepted / (gc_real)proposed : 0.0;
    free(lp);
    free(prop);
    return saved;
}

/* ---------------- Hamiltonian Monte Carlo (Neal) ---------------- */

static void hmc_grad(gs_logpdf_fn logpdf, void *ctx, const gc_real *q, int d, gc_real *g)
{
    gc_real *qp = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    gc_real *qm = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    const gc_real h = 1e-5;
    int i;
    if (!qp || !qm) { free(qp); free(qm); for (i = 0; i < d; i++) g[i] = 0.0; return; }
    for (i = 0; i < d; i++) { qp[i] = q[i]; qm[i] = q[i]; }
    for (i = 0; i < d; i++) {
        qp[i] = q[i] + h; qm[i] = q[i] - h;
        g[i] = (logpdf(qp, ctx) - logpdf(qm, ctx)) / (2.0 * h);
        qp[i] = q[i]; qm[i] = q[i];
    }
    free(qp); free(qm);
}

int gs_hmc(int dim, int n_samples, int burn_in, int thinning,
           gs_logpdf_fn logpdf, void *ctx, const gc_real *q0,
           gc_real epsilon, int n_leap, unsigned long long seed,
           gc_real *samples, gc_real *accept_ratio)
{
    gs_rng rng;
    gc_real *q, *p, *g, *qnew, *pnew;
    int i, s, saved = 0, total = burn_in + n_samples * thinning;
    long accepted = 0;
    if (dim < 1 || n_samples < 1 || !logpdf || !q0 || !samples || epsilon <= 0.0 || n_leap < 1)
        return -1;
    if (thinning < 1) thinning = 1;
    if (burn_in < 0) burn_in = 0;
    q = (gc_real *)malloc((size_t)dim * sizeof(gc_real));
    p = (gc_real *)malloc((size_t)dim * sizeof(gc_real));
    g = (gc_real *)malloc((size_t)dim * sizeof(gc_real));
    qnew = (gc_real *)malloc((size_t)dim * sizeof(gc_real));
    pnew = (gc_real *)malloc((size_t)dim * sizeof(gc_real));
    if (!q || !p || !g || !qnew || !pnew) {
        free(q); free(p); free(g); free(qnew); free(pnew); return -1;
    }
    rng = gs_rng_make(seed);
    for (i = 0; i < dim; i++) q[i] = q0[i];
    for (s = 0; s < total; s++) {
        double U0 = -logpdf(q, ctx), K0 = 0.0, U1, K1, logu;
        for (i = 0; i < dim; i++) { p[i] = gs_rng_normal(&rng); K0 += 0.5 * p[i] * p[i]; }
        for (i = 0; i < dim; i++) { qnew[i] = q[i]; pnew[i] = p[i]; }
        hmc_grad(logpdf, ctx, qnew, dim, g);
        for (i = 0; i < dim; i++) pnew[i] += 0.5 * epsilon * g[i];
        {
            int l;
            for (l = 0; l < n_leap; l++) {
                for (i = 0; i < dim; i++) qnew[i] += epsilon * pnew[i];
                hmc_grad(logpdf, ctx, qnew, dim, g);
                for (i = 0; i < dim; i++)
                    pnew[i] += (l == n_leap - 1 ? 0.5 : 1.0) * epsilon * g[i];
            }
        }
        U1 = -logpdf(qnew, ctx);
        K1 = 0.0; for (i = 0; i < dim; i++) K1 += 0.5 * pnew[i] * pnew[i];
        logu = log(gs_rng_uniform(&rng) + 1e-300);
        if (logu < (U0 + K0) - (U1 + K1)) {
            for (i = 0; i < dim; i++) q[i] = qnew[i];
            accepted++;
        }
        if (s >= burn_in && (s - burn_in) % thinning == 0 && saved < n_samples) {
            for (i = 0; i < dim; i++) samples[(size_t)saved * dim + i] = q[i];
            saved++;
        }
    }
    if (accept_ratio) *accept_ratio = (double)accepted / (double)total;
    free(q); free(p); free(g); free(qnew); free(pnew);
    return saved;
}
