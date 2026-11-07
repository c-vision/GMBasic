/* gm_quant_volatility.h — volatilità e rischio (namespace VOLATILITY). */
#ifndef GMBQUANT_VOLATILITY_H
#define GMBQUANT_VOLATILITY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Volatilità annualizzata: sqrt(ppy) * deviazione standard campionaria. */
double gq_annualized_volatility(const double *returns, int n, int periods_per_year);
/* Deviazione downside rispetto a una soglia. */
double gq_vol_downside_deviation(const double *returns, int n, double threshold);
/* Deviazione downside annualizzata: sqrt(ppy) * downside. */
double gq_annualized_downside_deviation(const double *returns, int n, double threshold,
                                        int periods_per_year);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_VOLATILITY_H */
