/* gm_quant_metrics.h — metriche di performance e rischio (namespace METRICS). */
#ifndef GMBQUANT_METRICS_H
#define GMBQUANT_METRICS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Rendimento totale composto: prod(1 + r_i) - 1. */
double gq_total_return(const double *returns, int n);
/* Rendimento annualizzato: (1 + total_return)^(ppy / n) - 1. */
double gq_annualized_return(const double *returns, int n, int periods_per_year);
/* Sharpe a un argomento (canone TRADING.METRICS): mean / sample_std * sqrt(252). */
double gq_sharpe_ratio(const double *returns, int n);
/* Sharpe con risk-free e periodsPerYear: sqrt(ppy) * mean(r-rf) / std(r-rf). */
double gq_sharpe_ratio_rf(const double *returns, int n, double risk_free, int periods_per_year);
/* Sorino: sqrt(ppy) * mean(r - rf) / downside_dev(r - rf, 0). */
double gq_sortino_ratio(const double *returns, int n, double risk_free, int periods_per_year);
/* Calmar: rendimento annualizzato / |max drawdown|. */
double gq_calmar_ratio(const double *returns, int n, int periods_per_year);
/* Information ratio: sqrt(ppy) * mean(attivo) / std(attivo). */
double gq_information_ratio(const double *returns, const double *benchmark, int n, int periods_per_year);
/* Treynor: (AR - AR(rf)) / beta. */
double gq_treynor_ratio(const double *returns, const double *benchmark, int n,
                        double risk_free, int periods_per_year);
/* Massimo drawdown (composto, valore negativo). */
double gq_max_drawdown(const double *returns, int n);
/* Deviazione downside: sqrt(media dei (r - soglia)^2 per r < soglia). */
double gq_downside_deviation(const double *returns, int n, double threshold);
/* VaR: percentile nearest (MathNet) al livello (1 - confidenza). */
double gq_value_at_risk(const double *returns, int n, double confidence);
/* CVaR: media dei rendimenti <= VaR. */
double gq_conditional_value_at_risk(const double *returns, int n, double confidence);
/* Beta: cov(asset, benchmark) / var(benchmark) (campionarie). */
double gq_beta(const double *returns, const double *benchmark, int n);
/* Alpha di Jensen annualizzato. */
double gq_alpha(const double *returns, const double *benchmark, int n,
                double risk_free, int periods_per_year);
/* Skewness campionaria (bias-corrected, MathNet). */
double gq_returns_skewness(const double *returns, int n);
/* Curtosi campionaria in eccesso (non distorta, MathNet). */
double gq_returns_kurtosis(const double *returns, int n);
/* Jarque-Bera: out[0] = statistica, out[1] = p-value (chi2 a 2 gradi). */
int gq_jarque_bera(const double *returns, int n, double out[2]);
/* Tracking error annualizzato: sqrt(ppy) * std(r - benchmark). */
double gq_annualized_tracking_error(const double *returns, const double *benchmark, int n,
                                    int periods_per_year);
/* Omega ratio: guadagni / perdite rispetto a una soglia (INF se perdite nulle). */
double gq_omega_ratio(const double *returns, int n, double threshold);
/* Ulcer index: sqrt(media dei drawdown percentuali al quadrato, composti). */
double gq_ulcer_index(const double *returns, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_METRICS_H */
