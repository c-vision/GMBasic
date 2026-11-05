/*
 * gs_hmm.h — Hidden Markov Model gaussiano (da hmmlearn/Libs). Emissioni
 * multivariate normali per stato; inferenza in log-domain (forward), decoding
 * (Viterbi) e fit (Baum-Welch EM).
 */
#ifndef GMBSTATS_GS_HMM_H
#define GMBSTATS_GS_HMM_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward (log): ritorna il log-likelihood P(O|model). logpi[n_states],
 * logA[n_states x n_states], logE[n x n_states] (emissioni log). */
gc_real gs_hmm_forward(int n, int n_states, const gc_real *logpi,
                       const gc_real *logA, const gc_real *logE);

/* Viterbi: path[0..n-1] = stato piu' probabile. Ritorna il log-lik del path. */
gc_real gs_hmm_viterbi(int n, int n_states, const gc_real *logpi,
                       const gc_real *logA, const gc_real *logE, int *path);

/* Baum-Welch con emissioni gaussiane (d-dim). X = n x d.
 * pi[n_states] iniziale/out, A[n_states x n_states], means[n_states x d],
 * covs[n_states x d x d]. Ritorna 0/-1. */
int gs_hmm_fit(const gc_real *X, int n, int d, int n_states, int n_iter,
               gc_real *pi, gc_real *A, gc_real *means, gc_real *covs);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_HMM_H */