/*
 * gs_dist2.h — distribuzioni non centrali e quantili (riscrittura, da ccmath).
 *
 * Chi-quadro/gamma/beta/F/t non centrali. Le cdf non centrali sono la somma
 * pesata di Poisson di cdf centrali; pdf per serie di Poisson (o derivata
 * della cdf), quantili per inversione numerica della cdf.
 */
#ifndef GMBSTATS_GS_DIST2_H
#define GMBSTATS_GS_DIST2_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* cdf centrali usate come base (implementate in gmi_math). */
gc_real gs_dist2_chi2_cdf(gc_real x, gc_real df);
gc_real gs_dist2_beta_cdf(gc_real x, gc_real a, gc_real b);

/* Non centrali (scale 1). */
gc_real gs_dist2_nc_chi2_pdf(gc_real x, gc_real df, gc_real nc);
gc_real gs_dist2_nc_chi2_cdf(gc_real x, gc_real df, gc_real nc);
gc_real gs_dist2_nc_chi2_ppf(gc_real p, gc_real df, gc_real nc);
gc_real gs_dist2_nc_chi2_isf(gc_real p, gc_real df, gc_real nc);
gc_real gs_dist2_nc_gamma_pdf(gc_real x, gc_real a, gc_real d);
gc_real gs_dist2_nc_gamma_cdf(gc_real x, gc_real a, gc_real d);
gc_real gs_dist2_nc_gamma_ppf(gc_real p, gc_real a, gc_real d);
gc_real gs_dist2_nc_gamma_isf(gc_real p, gc_real a, gc_real d);
gc_real gs_dist2_nc_beta_pdf(gc_real x, gc_real a, gc_real b, gc_real d);
gc_real gs_dist2_nc_beta_cdf(gc_real x, gc_real a, gc_real b, gc_real d);
gc_real gs_dist2_nc_beta_ppf(gc_real p, gc_real a, gc_real b, gc_real d);
gc_real gs_dist2_nc_beta_isf(gc_real p, gc_real a, gc_real b, gc_real d);

/* F non centrale (d1, d2, nc). */
gc_real gs_dist2_nc_f_pdf(gc_real x, gc_real d1, gc_real d2, gc_real nc);
gc_real gs_dist2_nc_f_cdf(gc_real x, gc_real d1, gc_real d2, gc_real nc);
gc_real gs_dist2_nc_f_ppf(gc_real p, gc_real d1, gc_real d2, gc_real nc);
gc_real gs_dist2_nc_f_isf(gc_real p, gc_real d1, gc_real d2, gc_real nc);

/* t di Student non centrale (df, nc). */
gc_real gs_dist2_nc_t_pdf(gc_real x, gc_real df, gc_real nc);
gc_real gs_dist2_nc_t_cdf(gc_real x, gc_real df, gc_real nc);
gc_real gs_dist2_nc_t_ppf(gc_real p, gc_real df, gc_real nc);
gc_real gs_dist2_nc_t_isf(gc_real p, gc_real df, gc_real nc);

/* Quantili (inversione). */
gc_real gs_dist2_normal_quantile(gc_real p);
gc_real gs_dist2_gamma_quantile(gc_real p, gc_real a);
gc_real gs_dist2_beta_quantile(gc_real p, gc_real a, gc_real b);
gc_real gs_dist2_chi2_quantile(gc_real p, gc_real df);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_DIST2_H */
