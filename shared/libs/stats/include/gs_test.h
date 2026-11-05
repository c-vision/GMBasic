/*
 * gs_test.h — test di ipotesi classici (SciPy stats), non presenti nel legacy.
 *
 * t-test (1/2 campioni, paired), z-test, chi-quadro (bontà/contingenza),
 * Fisher esatto, Kolmogorov-Smirnov, Shapiro-Wilk, D'Agostino, ANOVA, F-test.
 */
#ifndef GMBSTATS_GS_TEST_H
#define GMBSTATS_GS_TEST_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
void gs_analysis_anova(const double *groups, int *group_sizes, int k, double *f, double *p, double *ssb, double *ssw);

#endif

typedef struct gs_test_result {
    gc_real statistic;
    gc_real pvalue;
    gc_real df;      /* gradi di libertà, NaN se non applicabili */
} gs_test_result;

/* t-test: due campioni indipendenti (varianza combinata, equal_var=True). */
gs_test_result gs_test_ttest_ind(const gc_real *x, int nx, const gc_real *y, int ny);
/* t-test a un campione. */
gs_test_result gs_test_ttest_1samp(const gc_real *x, int n, gc_real popmean);
/* t-test per dati appaiati (x-y). */
gs_test_result gs_test_ttest_rel(const gc_real *x, const gc_real *y, int n);
/* z-test a un campione con deviazione standard nota. */
gs_test_result gs_test_ztest_1samp(const gc_real *x, int n, gc_real popmean, gc_real popstd);
/* chi-quadro di bontà (osservati vs attesi). */
gs_test_result gs_test_chi2_gof(const gc_real *observed, const gc_real *expected, int k);
/* chi-quadro di indipendenza su tabella di contingenza (rows×cols row-major). */
gs_test_result gs_test_chi2_contingency(const gc_real *table, int rows, int cols);
/* Fisher esatto 2x2 (odds ratio, p a due code). */
gs_test_result gs_test_fisher_exact(int a, int b, int c, int d);
/* Kolmogorov-Smirnov a due campioni. */
gs_test_result gs_test_ks_2samp(const gc_real *x, int nx, const gc_real *y, int ny);
/* Shapiro-Wilk (AS R94), 3..5000 osservazioni. */
gs_test_result gs_test_shapiro(const gc_real *x, int n);
/* D'Agostino-Pearson omnibus (skew + kurtosis), >= 20 osservazioni. */
gs_test_result gs_test_normaltest(const gc_real *x, int n);
/* ANOVA a una via: groups = tutti i valori concatenati, sizes = dimensione di
 * ciascuno dei k gruppi. */
gs_test_result gs_test_anova(const gc_real *groups, const int *sizes, int k);
/* F-test per l'uguaglianza delle varianze di due campioni. */
gs_test_result gs_test_ftest(const gc_real *x, int nx, const gc_real *y, int ny);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_TEST_H */