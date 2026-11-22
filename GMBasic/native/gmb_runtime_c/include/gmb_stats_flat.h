/* gmb_stats_flat.h — PONTE dei test di ipotesi per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. `CommonLib/Numerics/HypothesisTests.cs` calcolava in C# (MathNet) gli stessi test che
 * `gmb_stats.c` implementa per VM/C e C99Native: due sorgenti numeriche. I kernel del runtime
 * parlano però `gmb_val_t` (liste in ingresso, dict in uscita): qui c'è SOLO il marshalling
 * lista↔buffer + estrazione di Statistic/PValue/DegreesOfFreedom — la matematica resta nei kernel.
 *
 * CONTRATTO. Ritorno 1 = risultato valido, 0 = input rifiutato. I gradi di libertà valgono NaN
 * dove non si applicano (fisher, ks, shapiro). */
#ifndef GMB_STATS_FLAT_H
#define GMB_STATS_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int gmb_stf_ttest(const double* x, int nx, const double* y, int ny,
                  double* stat, double* p, double* df);
int gmb_stf_ttest_paired(const double* x, int nx, const double* y, int ny,
                         double* stat, double* p, double* df);
int gmb_stf_ttest_1samp(const double* x, int nx, double mu,
                        double* stat, double* p, double* df);
int gmb_stf_chi2(const double* observed, int nobs, const double* expected, int nexp,
                 double* stat, double* p, double* df);
int gmb_stf_fisher(long a, long b, long c, long d,
                   double* stat, double* p, double* df);
int gmb_stf_ks(const double* x, int nx, const double* y, int ny,
               double* stat, double* p, double* df);
int gmb_stf_shapiro(const double* x, int n, double* stat, double* p, double* df);
int gmb_stf_chi2con(const double* table, int rows, int cols, double* stat, double* p, double* df);
int gmb_stf_skewtest(const double* x, int n, double* stat, double* p, double* df);
int gmb_stf_kurtosistest(const double* x, int n, double* stat, double* p, double* df);
int gmb_stf_jarquebera(const double* x, int n, double* stat, double* p, double* df);
int gmb_stf_pearsonr(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df);
int gmb_stf_spearmanr(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df);
int gmb_stf_kendalltau(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df);
int gmb_stf_ftest(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df);
int gmb_stf_mannwhitney(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df);
int gmb_stf_wilcoxon(const double* x, int nx, const double* y, int ny, double* stat, double* p, double* df);
int gmb_stf_kruskal(const double* data, const int* sizes, int k, double* stat, double* p, double* df);
int gmb_stf_linregress(const double* x, int nx, const double* y, int ny, double* slope, double* intercept, double* r, double* pvalue, double* stderr);
int gmb_stf_describe(const double* a, int n, double* nobs, double* vmin, double* vmax, double* mean, double* variance, double* skewness, double* kurtosis);

#ifdef __cplusplus
}
#endif

#endif /* GMB_STATS_FLAT_H */
