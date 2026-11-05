/*
 * gs_sim.h — supporto simulazione (riscrittura, da ccmath).
 *
 * RNG (xorshift64*), distribuzioni, istogrammi, campionamento, autocorrelazione.
 */
#ifndef GMBSTATS_GS_SIM_H
#define GMBSTATS_GS_SIM_H

#include "gc_core.h"

/* Student-t copula (FinancePy): campiona uniforms con dipendenza t-copula:
 * y = chol(corr)*z (z N(0,1)), g = y/sqrt(chi2_df/df), u = t_cdf(g, df).
 * out[0..n-1] per trial. */
int gs_copula_t(const double *corr, int n, double df, unsigned seed, double *out);

/* Fractional Gaussian noise via Hosking (mmar): autocovarianza
 * gamma(k)=(|k+1|^2H - 2|k|^2H + |k-1|^2H)/2, ricorsione esatta. */
int gs_fgn_hosking(double hurst, int n, unsigned seed, double *x);

/* Processi stocastici aggiuntivi (mmar):
 * Poisson: tempi di arrivo cumsum di esponenziali (rate). 
 * CEV: dX = mu X dt + sigma X^gamma dW (Euler). 
 * Squared Bessel: dX = a dt + 2 sqrt(X) dW. 
 * Brownian bridge: B_t = W_t - t/T W_T + t/T b. 
 * Inverse Gaussian: campione IG(mean, scale) via Michael-Schucany-Haas. */
int gs_process_poisson(double rate, int n, unsigned seed, double *times);
int gs_process_cev(double x0, double mu, double sigma, double gamma, double t0,
                   double t1, int nsteps, unsigned seed, double *path);
int gs_process_sq_bessel(double x0, double a, double t0, double t1, int nsteps,
                         unsigned seed, double *path);
int gs_brownian_bridge(double wT, double b, double t0, double t1, int nsteps,
                       unsigned seed, double *path);
double gs_rand_inverse_gaussian(double mean, double scale, unsigned *seed);
/* CIR/SRD discretizzazione esatta (noncentral-chi2, DAW): path[0..nsteps]. */
int gs_process_cir_exact(double x0, double kappa, double theta, double sigma,
                         double t0, double t1, int nsteps, unsigned seed, double *path);
/* CIR/SRD 7 schemi Euler (DAW): 0 FullTrunc,1 PartialTrunc,2 Trunc,3 Reflect,
 * 4 Higham-Mao,5 SimpleReflect,6 Absorb. path[0..nsteps]. */
int gs_process_cir_schemes(double x0, double kappa, double theta, double sigma,
                           double t0, double t1, int nsteps, int scheme,
                           unsigned seed, double *path);
/* Jump-diffusion Merton M76 log-Euler (DAW): path[0..nsteps]. */
int gs_process_jump_diffusion(double s0, double r, double sigma, double lamb,
                              double mu, double delta, double t0, double t1,
                              int nsteps, unsigned seed, double *path);
/* GBM (dS = mu S dt + sigma S dW) esatto log-normale: path[0..nsteps]. */
int gs_process_gbm(double s0, double mu, double sigma, double t0, double t1,
                   int nsteps, unsigned seed, double *path);
/* Ornstein-Uhlenbeck / Vasicek (dX = kappa(theta-X)dt + sigma dW) esatto. */
int gs_process_ou(double x0, double kappa, double theta, double sigma,
                  double t0, double t1, int nsteps, unsigned seed, double *path);
/* Hull-White short rate (dr = -a r dt + sigma dW) esatto (theta(t)=0). */
int gs_process_hull_white(double r0, double a, double sigma, double t0, double t1,
                          int nsteps, unsigned seed, double *path);
/* Ho-Lee short rate (dr = theta dt + sigma dW). */
int gs_process_ho_lee(double r0, double theta, double sigma, double t0, double t1,
                      int nsteps, unsigned seed, double *path);
/* Campiona n valori Levy-stable S(alpha,beta,1,0) via Chambers-Mallows-Stuck. */
int gs_sim_levy_stable(double alpha, double beta, int n, unsigned seed, double *out);
/* Fractional Brownian motion (Hosking): path[0..n-1], Hurst in (0,1). */
int gs_process_fbm(double hurst, int n, unsigned seed, double *path);
/* Fractional Levy stable motion (FLSM): incrementi Levy-stable S(alpha,beta)
 * pesati con j^(H-1/alpha). path[0..n-1]. 0/-1. */
int gs_process_flsm(double hurst, double alpha, double beta, int n, unsigned seed, double *path);
/* MSM (Markov-Switching Multifractal, Calvet-Fisher): probabilita' di transizione
 * gamma[0..kbar-1], gamma1=1-(1-gbar)^(1/b^(kbar-1)), gamma_i=1-(1-gamma1)^(b^(i-1)). */
int gs_msm_transition_probs(int kbar, double b, double gamma_kbar, double *out);
/* Simula T rendimenti MSM: M_t=prod_k M_{k,t} (m0 o 2-m0), vol=sqrt(M)*sigma*z. */
int gs_msm_simulate(int kbar, double b, double m0, double gamma_kbar, double sigma,
                    int T, unsigned seed, double *out);


#ifdef __cplusplus
extern "C" {
#endif

typedef struct gs_rng { unsigned long long s; } gs_rng;

gs_rng gs_rng_make(unsigned long long seed);
/* uniforme in [0,1). */
gc_real gs_rng_uniform(gs_rng *r);
/* intero in [0,n). */
int gs_rng_int(gs_rng *r, int n);
/* normale standard (Box-Muller). */
gc_real gs_rng_normal(gs_rng *r);
/* Bernoulli: 1 con probabilità p. */
int gs_rng_bernoulli(gs_rng *r, gc_real p);

/* MRG32k3a (L'Ecuyer 1999): generatore combinato a 6 stato, periodo ~2^191.
 * Init con 2 seed (s1 e s2, valori iniziali; default 12345). */
typedef struct { double s1[3]; double s2[3]; } gs_mrg;
void gs_mrg32k3a_init(gs_mrg *g, unsigned long a, unsigned long b);
/* uniforme in (0,1). */
gc_real gs_mrg32k3a_next(gs_mrg *g);

/* Istogramma: conta i campioni in kbin intervalli di [xmin,xmax]. */
gc_status gs_hist(const gc_real *x, int n, gc_real xmin, gc_real xmax, int kbin, gc_real *counts);
/* Shuffle Fisher-Yates di un array di puntatori. */
void gs_sim_shuffle(void **v, int n, gs_rng *r);
/* Campionamento senza reinserimento: m elementi da s[n] in d[m]. */
void gs_sim_sample(void **s, int n, void **d, int m, gs_rng *r);
/* Autocorrelazione al lag dato (normalizzata a lag 0 = 1). */
gc_real gs_sim_autocorr(const gc_real *x, int n, int lag);

/* Ziggurat normal RNG (Marsaglia 256-layer, da generic_math): veloce
 * generatore di normali standard. gs_ziggurat_init/next con xoshiro256**. */
typedef struct { double x[256], y[256], wtab[256]; unsigned long long ktab[256];
                 double A, r; unsigned long long s[4]; } gs_ziggurat;
void gs_ziggurat_init(gs_ziggurat *g, unsigned long long seed);
gc_real gs_ziggurat_next(gs_ziggurat *g);

/* GBM bridge esatto (generic_math #211): campiona S_t | S_0, S_T (ponte
 * geometrico). u=t/T: ln S_t = (1-u) ln S0 + u ln ST + z*sigma*sqrt(T*u*(1-u)).
 * Ritorna il valore del ponte a tempo t. */
gc_real gs_mc_gmb_bridge(gc_real s0, gc_real st, gc_real t, gc_real T,
                         gc_real sigma, gc_real z);

/* Poisson(lam). */
int gs_rng_poisson(gs_rng *r, gc_real lam);
/* Gamma(shape, rate): rate=1/scale. */
gc_real gs_rng_gamma(gs_rng *r, gc_real shape, gc_real rate);

/* CIR bridge esatto (Makarov-Glew 2010 via ponte SQB): campiona r_t | r0, rT
 * per dCIR = kappa*(theta-r)dt + sigma*sqrt(r)dW, 0<t<T. Ritorna il valore. */
gc_real gs_mc_cir_bridge(gc_real r0, gc_real rT, gc_real t, gc_real T,
                         gc_real kappa, gc_real theta, gc_real sigma, gs_rng *r);

/* G2++ bridge esatto (gaussiano bivariato, Kalman smoothing): stato (x,y) con
 * dx=-a*x dt+sigma dW1, dy=-b*y dt+eta dW2, corr(dW1,dW2)=rho. Campiona
 * (x_tk,y_tk) | (x_t1,y_t1),(x_t2,y_t2). Ritorna GC_OK/GC_ERR. */
gc_status gs_mc_g2pp_bridge(gc_real x1, gc_real y1, gc_real x2, gc_real y2,
                            gc_real t1, gc_real tk, gc_real t2,
                            gc_real a, gc_real b, gc_real sigma, gc_real eta,
                            gc_real rho, gs_rng *r, gc_real *x_out, gc_real *y_out);

/* Solver SDE (da stochPathSim): x = drift(x,t) dt + diff(x,t) dW.
 * Schemi: Euler-Maruyama, Milstein (richiede ddiff = derivata di diff) e
 * Milstein derivative-free (0.5-strong). Simula npaths traiettorie su nsteps
 * passi in [t0,t1], out[npaths x (nsteps+1)]. Ritorna 0/-1. */
typedef gc_real (*gs_sde_drift)(gc_real x, gc_real t, void *ctx);
typedef gc_real (*gs_sde_diff)(gc_real x, gc_real t, void *ctx);
typedef gc_real (*gs_sde_ddiff)(gc_real x, gc_real t, void *ctx);
gc_status gs_sde_euler(gs_sde_drift drift, gs_sde_diff diff, void *ctx,
                       gc_real x0, gc_real t0, gc_real t1, int nsteps,
                       int npaths, gs_rng *rng, gc_real *out);
gc_status gs_sde_milstein(gs_sde_drift drift, gs_sde_diff diff, gs_sde_ddiff ddiff,
                          void *ctx, gc_real x0, gc_real t0, gc_real t1,
                          int nsteps, int npaths, gs_rng *rng, gc_real *out);
gc_status gs_sde_milstein_df(gs_sde_drift drift, gs_sde_diff diff, void *ctx,
                             gc_real x0, gc_real t0, gc_real t1, int nsteps,
                             int npaths, gs_rng *rng, gc_real *out);

/* Student-t copula (FinancePy): campiona uniforms con dipendenza t-copula:
 * y = chol(corr)*z (z N(0,1)), g = y/sqrt(chi2_df/df), u = t_cdf(g, df).
 * out[0..n-1] per trial. */
int gs_copula_t(const double *corr, int n, double df, unsigned seed, double *out);

/* Fractional Gaussian noise via Hosking (mmar): autocovarianza
 * gamma(k)=(|k+1|^2H - 2|k|^2H + |k-1|^2H)/2, ricorsione esatta. */
int gs_fgn_hosking(double hurst, int n, unsigned seed, double *x);

/* Processi stocastici aggiuntivi (mmar):
 * Poisson: tempi di arrivo cumsum di esponenziali (rate). 
 * CEV: dX = mu X dt + sigma X^gamma dW (Euler). 
 * Squared Bessel: dX = a dt + 2 sqrt(X) dW. 
 * Brownian bridge: B_t = W_t - t/T W_T + t/T b. 
 * Inverse Gaussian: campione IG(mean, scale) via Michael-Schucany-Haas. */
int gs_process_poisson(double rate, int n, unsigned seed, double *times);
int gs_process_cev(double x0, double mu, double sigma, double gamma, double t0,
                   double t1, int nsteps, unsigned seed, double *path);
int gs_process_sq_bessel(double x0, double a, double t0, double t1, int nsteps,
                         unsigned seed, double *path);
int gs_brownian_bridge(double wT, double b, double t0, double t1, int nsteps,
                       unsigned seed, double *path);
double gs_rand_inverse_gaussian(double mean, double scale, unsigned *seed);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_SIM_H */

/* Forecasting (AI/Sources/Python/Forecasting, gs_forecast.c): */
int gs_ssa(const double *x, int n, int window, int rank_mode, int k_in,
           double threshold, double *rec, int *k_out);
int gs_ssa_forecast(const double *x, int n, int window, int k, int h,
                    double *forecast);
double gs_ses_forecast(const double *x, int n, double rho);
void gs_online_ols_init(double *state);
void gs_online_ols_update(double *state, double a, double y);
void gs_online_ols_fit(const double *state, double *alpha, double *beta);
int gs_drift_forecast(const double *x, int n, int h, double *forecast);
double gs_huber_mean(const double *x, int n, double a, double b);
int gs_precision_ensemble(const double *y, int k, double e, int nmod,
                          double *forecast);
double gs_diebold_mariano(const double *actual, const double *p1,
                          const double *p2, int n, int h, int crit, double power);
int gs_wiener_hopf(const double *x, const double *y, int n, int order,
                   double *w, double *mse);
int gs_heat_smooth(const double *x, int n, double a, double k, int steps,
                   double *out);
double gs_perm_test_pvalue(const double *a, int na, const double *b, int nb,
                           int samples, unsigned seed);
int ssa_svd_econ(int m, int n, const double *A, int kmax, double *U, double *s, double *V);
/* Factor characteristics (Analysis/): */
double gs_skewness_corrected(const double *x, int n);
double gs_coskew(const double *res, const double *mkt, int n);
double gs_max5(const double *x, int n, int k);
double gs_amihud(const double *ret, const double *dollar_vol, int n);
int gs_quantile_anomaly(const double *x, int n, double lq, double uq, double k,
                        int mode, int *out);
/* Jupyter extra: */
double gs_kendall_tau(const double *x, const double *y, int n);
double gs_copula_gaussian_cdf2(double u, double v, double rho);
double gs_copula_joe_cdf(double u, double v, double alpha);
double gs_copula_n13_cdf(double u, double v, double alpha);
double gs_copula_n14_cdf(double u, double v, double alpha);
int gs_kmeans(const double *x, int n, int d, int k, int max_iter,
              double *out_centers, int *out_assign);
int gs_tls_regression(const double *x, const double *y, int n, double *slope, double *intercept);
int gs_probabilistic_obv(const double *c, const double *v, int n, double *out);
double gs_effective_spread(const double *bid, const double *ask, const double *last, int n);
int gs_riskmetrics_cov(const double *rets, int n, int d, double lambda, double *out_cov);
int gs_savgol_smooth(const double *x, int n, int win, int order, double *out);
/* Sentiment/ML/scoring (Sentiment/): */
int gs_svm_multiclass(const double *X, const double *y, int n, int d, int k,
                      double C, int max_iter, double *W);
int gs_svm_predict(const double *W, const double *x, int d, int k, int *label);
int gs_nb_bernoulli_fit(const double *X, const double *y, int n, int d, int k,
                        double *prior, double *alpha);
int gs_nb_bernoulli_predict(const double *prior, const double *alpha, const double *x,
                            int d, int k, int *label);
double gs_edit_distance_sim(const char **a, int m, const char **b, int n);
double gs_sentiment_lm_composite(const double *pos, const double *neg, int n);
double gs_sentiment_norm_diff(double ps, double ns);
double gs_sentiment_fusion(double tb, double vd, int *label);
void gs_sentiment_decay_reweight(const double *s, const int *is_retweet, int n, double *out);
int gs_sentiment_ticker_agg(const double *word_scores, const int *word_is_ticker,
                            int n_words, double *out_score, int *out_count);
double gs_log_trin(double adv, double dec, double adv_vol, double dec_vol);
int gs_volume_profile(const double *close, const double *vol, int n, int n_bins,
                      double *bin_vol, int *poc_bin);
double gs_tfidf_score(const double *counts, const double *df, int n_terms,
                      int n_docs, int method, int normalize);
int gs_accrual_wc_nco(double ni, double dep, double cfo, double cfi, double cff,
                      double div, double *wc, double *nco, double *tacc, double *acc_income);
double gs_dtw_idw_predict(const double *price, int n, int L, int k);
int gs_srf_signal(const double *ret, int n, int *horizons, int n_h, double *out);

/* Cokurtosi (co-momento del 4o ordine centrato): (1/n) sum (x-xb)(y-yb)(z-zb)(w-wb). */
double gs_cokurt(const double *x, const double *y, const double *z, const double *w, int n);
/* Co-momento di portafoglio del 4o ordine: momento centrale 4o dei rendimenti di
 * portafoglio r_t = sum_k w_k R[t*N+k]. R: T x N row-major. */
double gs_portfolio_cokurt(const double *R, int T, int N, const double *w);
