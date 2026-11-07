/* gm_quant_portfolio.h — ottimizzazione e metriche di portafoglio (namespace PORTFOLIO).
 *
 * Convenzioni (identiche ai builtin TRADING.PORTFOLIO, parita' VM/C e C99):
 *   - i pesi `w` hanno n elementi e la matrice di covarianza e' n*n ROW-MAJOR;
 *   - le funzioni che ottimizzano scrivono i pesi in `w` e ritornano 1 = ok, 0 = input
 *     non valido; i pesi di fallback sono 1/n;
 *   - i rendimenti sono in frazione (0.05 = 5%).
 */
#ifndef GMBQUANT_PORTFOLIO_H
#define GMBQUANT_PORTFOLIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── metriche scalari ────────────────────────────────────────────────────── */

/* Rendimento atteso: somma w_i*mean_i. */
double gq_port_expected_return(const double *w, const double *mean, int n);
/* Deviazione standard: sqrt(w' cov w). */
double gq_port_stdev(const double *w, const double *cov, int n);
/* Sharpe: (ret - rf)/sd; sd = 0 -> 0. */
double gq_port_sharpe(double ret, double sd, double risk_free);
/* Long-only: -1 (vero BASIC) se tutti i pesi sono >= 0, 0 altrimenti. */
double gq_port_is_long_only(const double *w, int n);
/* MAD da MEDIANA (canone TRADING.PORTFOLIO.MAD). */
double gq_port_mad(const double *r, int n);
/* Semi-MAD: valori < target, scarto medio assoluto dalla loro mediana. */
double gq_port_semimad(const double *r, int n, double target);
/* VaR storico: -percentile((1-conf)*n) * 100 (percentuale, come gmb_trd_var). */
double gq_port_var(const double *flat, int n, double conf);
/* CVaR storico: |media della coda| (frazione, come gmb_trd_portfolio_cvar_jagged). */
double gq_port_cvar(const double *flat, int n, double conf);

/* Valori delle strategie a risultato "speciale" (usati dal packer). */
double gq_port_max_return_value(const double *mean, int n);
double gq_port_kelly_value(const double *r, int n);
double gq_port_min_cvar_value(const double *r, int n, double conf);
double gq_port_min_cdar_value(const double *r, int n, double conf);

/* ── ottimizzatori: scrivono n pesi in `w` ──────────────────────────────── */

int gq_port_min_volatility(const double *cov, int n, double *w);
int gq_port_max_sharpe(const double *mean, const double *cov, int n, double rf, double *w);
/* Mean-variance: stessa formula chiusa di MAXSHARPE con rf = 0. */
int gq_port_mean_variance(const double *mean, const double *cov, int n, double *w);
int gq_port_risk_parity(const double *cov, int n, double *w);
int gq_port_equally_weighted(int n, double *w);
int gq_port_max_return_weights(const double *mean, int n, double *w);
/* Kelly: w[0] = indice di Kelly (un solo "asset"), come il builtin. */
int gq_port_kelly_weights(const double *r, int n, double *w);
/* Min-CVaR / Min-CDaR: w[0] = 1.0 (il builtin non distribuisce pesi). */
int gq_port_min_cvar_weights(const double *r, int n, double conf, double *w);
int gq_port_min_cdar_weights(const double *r, int n, double conf, double *w);
/* Black-Litterman completo:
 *   Pi = lambda*Sigma*w_mkt
 *   post = [(tau*Sigma)^-1 + P' Om^-1 P]^-1 [(tau*Sigma)^-1 Pi + P' Om^-1 Q]
 *   poi MAXSHARPE(post, Sigma, rf).
 * P e Omega sono k*n e k*k ROW-MAJOR. */
int gq_port_black_litterman(const double *caps, const double *cov, int n,
                            double risk_aversion, double tau,
                            const double *P, int k, const double *Q,
                            const double *omega, double rf, double *w);

/* EVOLVE: il builtin usa un GA che minimizza la varianza; qui la soluzione
 * deterministica (min-varianza), come il runtime condiviso. */
int gq_port_evolve(const double *cov, int n, double *w);

/* ── packer per l'FFI ───────────────────────────────────────────────────── */
/* Scrive out[0]=ExpectedReturn, out[1]=Volatility, out[2]=SharpeRatio,
 * out[3..3+n-1]=pesi. `mean` e/o `cov` possono essere NULL (ritorno/vol = 0). */
void gq_port_pack(const double *w, int n, const double *mean, const double *cov,
                  double rf, double *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_PORTFOLIO_H */
