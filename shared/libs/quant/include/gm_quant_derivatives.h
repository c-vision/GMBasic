/* gm_quant_derivatives.h — opzioni e greche (namespace DERIVATIVES). */
#ifndef GMBQUANT_DERIVATIVES_H
#define GMBQUANT_DERIVATIVES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Black-Scholes (senza dividendi). */
double gq_bs_call(double s, double k, double t, double r, double v);
double gq_bs_put(double s, double k, double t, double r, double v);
/* BS con dividend yield (q = 0 -> Black-Scholes; q != 0 -> Merton). */
double gq_bs_call_q(double s, double k, double t, double r, double v, double q);
double gq_bs_put_q(double s, double k, double t, double r, double v, double q);
/* Merton con dividend yield (ordine C: q prima di v). */
double gq_merton_call(double s, double k, double t, double r, double q, double v);
double gq_merton_put(double s, double k, double t, double r, double q, double v);
/* Superficie BASIC: (s,k,t,r,volatility,dividendYield) -> Merton. */
double gq_merton_call_surface(double s, double k, double t, double r, double v, double q);
double gq_merton_put_surface(double s, double k, double t, double r, double v, double q);
/* Binomiale CRR; is_put/is_american 0/1. */
double gq_binomial_ex(double s, double k, double t, double r, double v, int steps, int is_put, int is_american);
/* Variante europea (americano = 0). */
double gq_binomial(double s, double k, double t, double r, double v, int steps, int is_put);

/* Greche. */
double gq_delta(double s, double k, double t, double r, double v, int is_put);
double gq_gamma(double s, double k, double t, double r, double v);
double gq_vega(double s, double k, double t, double r, double v);
double gq_theta(double s, double k, double t, double r, double v, int is_put);
double gq_rho(double s, double k, double t, double r, double v, int is_put);
double gq_vanna(double s, double k, double t, double r, double v);
double gq_volga(double s, double k, double t, double r, double v);
double gq_speed(double s, double k, double t, double r, double v);
double gq_zomma(double s, double k, double t, double r, double v);
double gq_charm(double s, double k, double t, double r, double v, int is_put);
double gq_color(double s, double k, double t, double r, double v);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_DERIVATIVES_H */
