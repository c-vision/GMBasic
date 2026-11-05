/*
 * gs_arima.h — ARIMA(p,d,q), differenziazione/integrazione, Box-Cox, diagnostica
 * residui (Ljung-Box, Box-Pierce, ARCH-LM), ordini di differenziazione
 * (ndiffs/nsdiffs), auto-ARIMA e bande di previsione.
 *
 * Adattato da KoplaNum/ARIMA (lite-C, MIT) all'API cronologica di gmbsuite.
 * Serie in ordine cronologico (oldest first); n = lunghezza.
 */
#ifndef GMBSTATS_GS_ARIMA_H
#define GMBSTATS_GS_ARIMA_H

#include "gc_core.h"

#define GS_ARIMA_MAXP 4
#define GS_ARIMA_MAXQ 4
#define GS_ARIMA_MAXD 2

typedef struct {
    int p, d, q, n;
    double c;                        /* costante/intercetta */
    double phi[GS_ARIMA_MAXP];       /* AR */
    double theta[GS_ARIMA_MAXQ];     /* MA */
    double sigma2;                   /* varianza residui */
    double loglik, aic;
} gs_arima;

/* Differenza di lag `lag`: out[i] = x[i+lag]-x[i], i=0..n-lag-1. Ritorna n-lag. */
int gs_diff(const double *x, int n, int lag, double *out);
/* Integrazione (inversa di gs_diff): init = primi `lag` valori; out n+lag. */
int gs_integrate(const double *dx, int n, int lag, const double *init, double *out);
/* Differenza stagionale (lag=period) applicata `order` volte. Ritorna la lunghezza. */
int gs_seasonal_diff(const double *x, int n, int period, int order, double *out);

/* Box-Cox (x>0): y=(x^l-1)/l per l!=0, log(x) per l=0; lambda via MLE su griglia. */
int gs_boxcox(const double *x, int n, double lambda, double *out);
int gs_boxcox_inv(const double *y, int n, double lambda, double *out);
double gs_boxcox_lambda(const double *x, int n);

/* ARIMA: fit (ARMA via gs_armagarch_fit sui dati differenziati d volte),
 * previsione multi-step (integrazione inclusa), bande (alpha, es. 0.05),
 * auto-ARIMA (griglia p,q con d = ndiffs, minimo AIC). */
int gs_arima_fit(const double *x, int n, int p, int d, int q, gs_arima *out);
int gs_arima_forecast(const gs_arima *m, const double *x, int n, int h, double *out);
int gs_arima_forecast_bands(const gs_arima *m, const double *x, int n, int h, double alpha,
                            double *mean, double *lo, double *hi);
int gs_auto_arima(const double *x, int n, int pmax, int dmax, int qmax, gs_arima *out);

/* Diagnostica residui: statistiche e p-value (chi2). */
double gs_ljung_box(const double *x, int n, int h, double *out_p);
double gs_box_pierce(const double *x, int n, int h, double *out_p);
double gs_arch_lm(const double *x, int n, int lags, double *out_p);

/* Ordini di differenziazione (euristici): ADF per d, forza stagionale per D. */
int gs_ndiffs(const double *x, int n, int max_d);
int gs_nsdiffs(const double *x, int n, int period, int max_d);

/* Winsorize ai quantili pct e 1-pct. */
int gs_winsorize(const double *x, int n, double pct, double *out);

#endif /* GMBSTATS_GS_ARIMA_H */
