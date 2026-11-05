/*
 * gs_rolling.h — statistiche rolling e time series (SciPy/pandas-like).
 *
 * Finestre mobili (media/std/var/min/max/mediana/somma/correlazione/covarianza),
 * EMA, rendimenti, volatilità, z-score, Sharpe/Sortino rolling, max drawdown,
 * Yule-Walker, coefficienti MA. Non presenti nel legacy GMBasic.
 */
#ifndef GMBSTATS_GS_ROLLING_H
#define GMBSTATS_GS_ROLLING_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Finestre mobili: out[i] = stat(x[i-w+1 .. i]) per i >= w-1 (NaN prima).
 * out deve avere n elementi. */
void gs_rolling_mean(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_sum(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_var(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_std(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_min(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_max(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_median(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_cov(const gc_real *x, const gc_real *y, int n, int w, gc_real *out);
void gs_rolling_corr(const gc_real *x, const gc_real *y, int n, int w, gc_real *out);
/* (x - media rolling) / std rolling. */
void gs_rolling_zscore(const gc_real *x, int n, int w, gc_real *out);

/* Filtro di Hampel (robusto agli outlier): per ogni i con finestra completa,
 * med = mediana(x[i-w+1..i]), mad = MAD = mediana(|x-med|); se
 * |x[i]-med| > k*1.4826*mad (scala MAD ~ sigma normale) l'osservazione e'
 * un outlier: out[i]=med e (se flag!=NULL) flag[i]=1; altrimenti out[i]=x[i],
 * flag[i]=0. I primi w-1 valori restano invariati (out=x, flag=0).
 * Da authors/connor_roberts (EDA, pulizia serie). */
void gs_rolling_hampel(const gc_real *x, int n, int w, gc_real k,
                       gc_real *out, int *flag);
/* Sharpe rolling: media/std dei rendimenti (x sono rendimenti). */
void gs_rolling_sharpe(const gc_real *x, int n, int w, gc_real *out);

/* Media mobile esponenziale: out[0]=x[0], out[i]=alpha*x[i]+(1-alpha)*out[i-1]. */
void gs_rolling_skew(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_kurt(const gc_real *x, int n, int w, gc_real *out);
void gs_rolling_regression(const gc_real *x, const gc_real *y, int n, int w,
                           gc_real *slope, gc_real *intercept, gc_real *r2);
void gs_expanding_mean(const gc_real *x, int n, gc_real *out);
void gs_expanding_sum(const gc_real *x, int n, gc_real *out);
void gs_expanding_var(const gc_real *x, int n, gc_real *out);
void gs_expanding_std(const gc_real *x, int n, gc_real *out);
void gs_expanding_quantile_p2(const gc_real *x, int n, gc_real q, gc_real *out);

void gs_ema(const gc_real *x, int n, gc_real alpha, gc_real *out);

/* Rendimenti semplici/log: out[i] = (x[i]-x[i-1])/x[i-1] (o log(x[i]/x[i-1])).
 * out[0]=0. */
void gs_returns(const gc_real *x, int n, int log_ret, gc_real *out);

/* Volatilità rolling (std dei rendimenti * sqrt(periodi)). */
void gs_volatility(const gc_real *x, int n, int w, int periods, gc_real *out);

/* Max drawdown di una serie di prezzi. Ritorna il valore (negativo o 0). */
gc_real gs_max_drawdown(const gc_real *x, int n);

/* Yule-Walker: coefficienti AR di ordine p da una serie. ar[p]. Ritorna la
 * varianza del rumore. */
gc_real gs_yule_walker(const gc_real *x, int n, int p, gc_real *ar);

/* Coefficienti MA(q) da una serie (via autocovarianze). ma[q]. */
void gs_ma_coefficients(const gc_real *x, int n, int q, gc_real *ma);

/* Autocorrelazione (ACF) sui lag 0..maxlag (out[maxlag+1], out[0]=1), stessa
 * normalizzazione di statsmodels (denominatore = somma dei quadrati deviati). */
void gs_acf(const gc_real *x, int n, int maxlag, gc_real *out);
/* Soglia di significativita' dell'ACF: 1.96/sqrt(n). */
gc_real gs_acf_band(int n);
/* Autocorrelazione parziale (PACF) via Durbin-Levinson (out[maxlag+1]). */
void gs_pacf(const gc_real *x, int n, int maxlag, gc_real *out);

/* Decomposizione classica (media mobile centrata, stile statsmodels): trend +
 * stagionale + residuo. model: 0 additivo, 1 moltiplicativo. Richiede n>=2*period.
 * I puntatori possono essere NULL. Ritorna 0/-1. */
int gs_seasonal_decompose(const gc_real *x, int n, int period, int model,
                          gc_real *trend, gc_real *seasonal, gc_real *resid);
/* Forza della stagionalita' (additiva): max(0, 1 - Var(resid)/Var(resid+seasonal)). */
gc_real gs_seasonal_strength(const gc_real *seasonal, const gc_real *resid, int n);

/* Streak di variazioni consecutive concordi. out_len/out_sign opzionali (n
 * elementi). Ritorna la lunghezza massima. */
int gs_streaks(const gc_real *x, int n, int *out_len, int *out_sign);

/* Robust z-score (Qlib): (x - mediana)/(1.4826*MAD), troncato a [-clip,clip].
 * clip<=0 = nessun troncamento. out[n]. */
void gs_robust_zscore(const gc_real *x, int n, gc_real clip, gc_real *out);

/* Trend filter L2 (Hodrick-Prescott con order=2): minimizza
 * 0.5||y-x||^2 + lambda*||D^k x||^2; risolve (I + lambda D'D) x = y con Cholesky
 * a banda (order=1 tridiagonale, order=2 pentadiagonale). out[n]. Ritorna 0/-1. */
int gs_trend_filter(const gc_real *y, int n, gc_real lambda, int order, gc_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_ROLLING_H */
