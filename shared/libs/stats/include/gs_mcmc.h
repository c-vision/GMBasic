/*
 * gs_mcmc.h — sampler MCMC ensemble affine-invariant (da Monte-Carlo-Markov-Chain-Library).
 *
 * Stretch Move Affine Invariant Ensemble Sampler (Goodman & Weare, 2010).
 * Un ensemble di walker esplora lo spazio; ogni walker propone una mossa
 * "stretch" verso un walker complementare dell'altra metà,
 *   y = x_c + z (x_c - x),  z = (u+1)^2/2,  u ~ U(0,1)
 * accettata con probabilità min(1, z^(d-1) * p(y)/p(x)).
 */
#ifndef GMBSTATS_GS_MCMC_H
#define GMBSTATS_GS_MCMC_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* log-densità (non normalizzata) del punto x (dim componenti). */
typedef gc_real (*gs_logpdf_fn)(const gc_real *x, void *ctx);

/* Esegue il sampler. walkers: n_walkers×dim (in/out, posizioni finali).
 * samples: n_samples×dim (output). burn_in e thinning in round.
 * Ritorna il numero di campioni raccolti (>=0) o -1 su errore.
 * Se accept_ratio != NULL riceve il rapporto di accettazione. */
int gs_mcmc_stretch(int dim, int n_walkers, int n_samples, int burn_in, int thinning,
                    gs_logpdf_fn logpdf, void *ctx,
                    gc_real *walkers, gc_real *samples,
                    unsigned long long seed, gc_real *accept_ratio);

/* Hamiltonian Monte Carlo (Neal 2011; dai testi MCMC). Gradiente numerico
 * (differenze centrali). q0[dim] iniziale; epsilon = passo, n_leap = passi di
 * leapfrog. samples n_samples×dim. Ritorna n_samples o -1. */
int gs_hmc(int dim, int n_samples, int burn_in, int thinning,
           gs_logpdf_fn logpdf, void *ctx, const gc_real *q0,
           gc_real epsilon, int n_leap, unsigned long long seed,
           gc_real *samples, gc_real *accept_ratio);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_MCMC_H */
