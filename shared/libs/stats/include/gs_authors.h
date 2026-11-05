/*
 * gs_authors.h — metodi AI/Authors deep-mining (shrinkage, spread, entropy).
 */
#ifndef GMBSTATS_GS_AUTHORS_H
#define GMBSTATS_GS_AUTHORS_H

#ifdef __cplusplus
extern "C" {
#endif

int gs_ledoit_wolf(const double *rets, int T, int N, double *out_cov);
double gs_corwin_schultz(const double *high, const double *low, int n);
double gs_roll_spread(const double *price, int n);
int gs_realized_skew_kurt(const double *r, int n, double *out_skew, double *out_kurt);
double gs_mutual_information(const double *x, const double *y, int n, int n_bins);
double gs_transfer_entropy(const double *x, const double *y, int n, int n_bins);

#ifdef __cplusplus
}
#endif

#endif
