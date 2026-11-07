/* gq_internal.h — helper interni di gmbquant (non esportati dalla superficie pubblica). */
#ifndef GQ_INTERNAL_H
#define GQ_INTERNAL_H

double gq_mean(const double *x, int n);
double gq_var_sample(const double *x, int n);
double gq_std_sample(const double *x, int n);
double gq_cov_sample(const double *x, const double *y, int n);
double gq_skew_sample(const double *x, int n);
double gq_kurt_excess(const double *x, int n);
/* Percentile "nearest" con arrotondamento bancario (MathNet), come il canone. */
double gq_percentile_nearest(const double *x, int n, double pct);
void gq_sort(double *x, int n);

#endif /* GQ_INTERNAL_H */
