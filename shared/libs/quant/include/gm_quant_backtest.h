/* gm_quant_backtest.h — backtest di strategia (namespace BACKTEST). */
#ifndef GMBQUANT_BACKTEST_H
#define GMBQUANT_BACKTEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Backtest a pesi uguali: data e' una matrice periods*assets ROW-MAJOR; il
 * rendimento di ogni periodo e' la media degli asset della riga.
 * out layout: [0]=TotalReturn(%), [1]=AnnualizedReturn(%), [2]=MaxDrawdown,
 *             [3]=SharpeRatio, [4]=TradesCount, [5]=Turnover, [6..]=curva dei
 *             rendimenti per periodo (periods valori).
 * Ritorna periods (numero di valori di curva scritti), 0 su input non valido. */
int gq_backtest_run(const double *data, int periods, int assets, double *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_BACKTEST_H */
