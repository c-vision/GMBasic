/*
 * gmbstats.h — API pubblica aggregata di gmbstats (fondazione).
 *
 * Libreria C99 di statistica *pura* delle librerie native di GMBasic: descrittiva,
 * distribuzioni, regressione/modelli, serie temporali, RNG/simulazione, curve fitting.
 * Nessun tipo/semantica di dominio (i metodi finance-specifici sono in
 * gmbstatistics, soluzione gmbquant).
 * Dipende da gmbcompute (gc_*) e gmbmath (gm_linalg, gmi_math).
 *
 * C99 puro, sole dipendenze libc/libm.
 */
#ifndef GMBSTATS_H
#define GMBSTATS_H

#include "gc_core.h"
#include "gs_sim.h"
#include "gs_hmm.h"
#include "gs_kalman.h"
#include "gs_lpc.h"
#include "gs_stats.h"
#include "gs_stats2.h"
#include "gs_regression.h"
#include "gs_dist2.h"
#include "gs_dist.h"
#include "gs_pca.h"
#include "gs_spectral.h"
#include "gs_armagarch.h"
#include "gs_kelly.h"
#include "gs_arima.h"
#include "gs_research.h"
#include "gs_test.h"
#include "gs_nonparam.h"
#include "gs_alpha.h"
#include "gs_rolling.h"
#include "gs_qmc.h"
#include "gs_num.h"
#include "gs_math.h"
#include "gs_fit.h"
#include "gs_estimation.h"
#include "gs_ts2.h"
#include "gs_mvdist.h"
#include "gs_infer.h"
#include "gs_frac.h"
#include "gs_bspline.h"
#include "gs_mcs.h"
#include "gs_validation.h"
#include "gs_convex.h"
#include "gs_tailrisk.h"
#include "gs_cs.h"
#include "gs_ad.h"
#include "gs_mcmc.h"
#include "gs_diag.h"
#include "gs_algo.h"
#include "gs_curve.h"
#include "gs_extra.h"
#include "gs_authors.h"

#include "gs_boost.h"
#endif /* GMBSTATS_H */

/* Fractional calculus (gs_frac.c, da Libs/differint) */
double gs_gamma_godfrey(double z);
int gs_frac_gl_coeffs(double alpha, int n, double *b);
int gs_frac_gl(double alpha, const double *f, int n, double h, double *out);
int gs_frac_rl(double alpha, const double *f, int n, double h, double *out);
