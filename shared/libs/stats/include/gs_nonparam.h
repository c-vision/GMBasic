/*
 * gs_nonparam.h — correlazioni, test non parametrici e descrittive (SciPy).
 *
 * Da scipy.stats: pearsonr, spearmanr, kendalltau, linregress, mannwhitneyu,
 * wilcoxon, kruskal, jarque_bera, skewtest, kurtosistest, mode, moment, iqr,
 * describe. Non presenti nel legacy GMBasic.
 */
#ifndef GMBSTATS_GS_NONPARAM_H
#define GMBSTATS_GS_NONPARAM_H

#include "gc_core.h"
#include "gs_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gs_linreg_result {
    gc_real slope, intercept, r, pvalue, stderr;
} gs_linreg_result;

typedef struct gs_describe_result {
    gc_real nobs, min, max, mean, variance, skewness, kurtosis;
} gs_describe_result;

/* Correlazioni (ritornano gs_test_result: statistic=coefficiente, pvalue). */
gs_test_result gs_pearsonr(const gc_real *x, const gc_real *y, int n);
gs_test_result gs_spearmanr(const gc_real *x, const gc_real *y, int n);
gs_test_result gs_kendalltau(const gc_real *x, const gc_real *y, int n);

/* Regressione lineare OLS (SciPy linregress). */
gs_linreg_result gs_linregress(const gc_real *x, const gc_real *y, int n);

/* Test non parametrici. */
gs_test_result gs_mannwhitneyu(const gc_real *x, int nx, const gc_real *y, int ny);
gs_test_result gs_wilcoxon(const gc_real *x, const gc_real *y, int n);
gs_test_result gs_kruskal(const gc_real *groups, const int *sizes, int k);

/* Normalità. */
gs_test_result gs_jarque_bera(const gc_real *x, int n);
gs_test_result gs_skewtest(const gc_real *x, int n);
gs_test_result gs_kurtosistest(const gc_real *x, int n);

/* Descrittive (SciPy). */
gc_real gs_mode(const gc_real *x, int n);
gc_real gs_moment(const gc_real *x, int n, int order);
gc_real gs_iqr(const gc_real *x, int n);
gs_describe_result gs_describe(const gc_real *x, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_NONPARAM_H */
