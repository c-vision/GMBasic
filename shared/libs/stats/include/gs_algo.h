/*
 * gs_algo.h — strumenti statistici dai paper di AI/Algorithms.
 *  - Processi di punto / Hawkes (order flow) + toxic flow (Cartea)
 *  - Multifractal DFA (MF-DFA): H(q) generalizzato
 */
#ifndef GMBSTATS_GS_ALGO_H
#define GMBSTATS_GS_ALGO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Intensita' di Hawkes esponenziale all'istante t:
 * lambda(t) = mu + alpha * sum_{t_i < t} exp(-beta (t - t_i)). */

/* Stima MLE (gradient ascent) dei parametri di un Hawkes esponenziale su [0,T].
 * mu,alpha,beta in ingresso = iniziali, in uscita = stimati. Ritorna 0. */

/* Simulazione via thinning di Ogata. out_times: capacita' >= attesa; *out_n. */

/* Toxic flow: autocorrelazione lag-1 dei segni di trade (flow informato). */

/* MF-DFA: esponente di Hurst generalizzato H(q) per ogni q in q[0..nq-1].
 * x: serie; scale in [smin, smax]; out_hq[nq]. Ritorna 0. */
int gs_mfdfa(const double *x, int n, const double *q, int nq, int smin, int smax,
             int nscale, double *out_hq);

/* Scaling della volatilita' (Nava/Di Matteo/Aste): stima alpha in
 * sigma(tau) ~ tau^alpha via regressione log-log delle varianze dei rendimenti
 * aggregati a ciascun tau. alpha=0.5 (iid); >0.5 persistente/long-memory;
 * <0.5 anti-persistente. out_alpha, out_r2 (opz). Ritorna 0/-1. */
int gs_vol_scaling(const double *ret, int n, const int *taus, int ntau,
                   double *out_alpha, double *out_r2);

/* ACD — Autoregressive Conditional Duration (Engle-Russell 1998; da Engle-Sun 2012):
 * durate x_i, psi_i = omega + sum_j alpha_j x_{i-j} + sum_j beta_j psi_{i-j},
 * x_i = psi_i * eps_i con eps ~ Exp(1). Stima MLE (Nelder-Mead), forecast di psi. */
typedef struct {
    int p, q;
    double omega;
    double alpha[4];
    double beta[4];
    double loglik;
    int nparams;
} gs_acd;

int gs_acd_fit(const double *x, int n, int p, int q, gs_acd *out);
double gs_acd_forecast(const gs_acd *m, const double *x, int n);

/* Relative-entropy (KL) tilt tra due distribuzioni/dispersioni (Avellaneda):
 * pesi w_i ~ exp(-KL_i) normalizzati; ritorna la KL minima. */
int gs_mcmc_gibbs(double mu1, double mu2, double s1, double s2, double rho, int n, int burn, long seed, double *out_x1, double *out_x2);
double gs_relentropy_tilt(const double *p_market, const double *p_prior, int n,
                          double *out_weights);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_ALGO_H */
