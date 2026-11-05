/*
 * gs_fit.h — curve fitting e minimi quadrati (riscrittura, da ccmath).
 *
 * Chebyshev (coefficienti + Clenshaw), Padé, minimi quadrati polinomiali
 * (equazioni normali) e QR (Householder).
 */
#ifndef GMBSTATS_GS_FIT_H
#define GMBSTATS_GS_FIT_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Coefficienti di Chebyshev di una funzione f sui nodi di Chebyshev [0,m). */
gc_status gs_fit_cheb_coef(gc_real (*f)(gc_real, void *), void *ctx, int m, gc_real *coef);
/* Valutazione di una serie di Chebyshev (Clenshaw). */
gc_real gs_fit_cheb_eval(gc_real x, const gc_real *coef, int m);

/* Padé [na/nb] dai coefficienti di Taylor t[na+nb+1]. */
gc_status gs_fit_pade(const gc_real *t, int m, int nb, gc_real *a, gc_real *b);
/* Regressione lineare y = w*x + b via discesa del gradiente (da numerical-
 * computing-toolkit). kind: 0=BGD (full batch), 1=SGD (per esempio),
 * 2=mini-batch (dimensione `batch`). out[0]=w, out[1]=b; loss != NULL riceve
 * l'MSE finale. Ritorna 0 su successo, -1 su errore. */
int gs_regression_gd(int n, const gc_real *x, const gc_real *y, gc_real lr,
                     int epochs, int kind, int batch, gc_real *out, gc_real *loss);

gc_real gs_fit_pade_eval(gc_real x, const gc_real *a, int na, const gc_real *b, int nb);

/* Minimi quadrati polinomiali: grado m (coefficienti 0..m). Ritorna la somma
 * dei quadrati dei residui in *ssq. */
gc_real gs_fit_poly_ls(const gc_real *x, const gc_real *y, int n, int m, gc_real *coef);
gc_real gs_fit_poly_eval(gc_real x, const gc_real *coef, int m);

/* Minimi quadrati via QR (Householder): A (m×n row-major) x = b. */
gc_real gs_fit_qr_ls(gc_real *A, gc_real *b, int m, int n);

/* R² (coefficiente di determinazione) della retta dei minimi quadrati sui punti
 * (x_i = i+1, y_i = data[i]); salta EMPTY_VALUE (DBL_MAX) e i valori non finiti.
 * *r2 = coefficiente, *angle = pendenza (tangente). */
void gs_rsquared(const gc_real *data, int n, gc_real *r2, gc_real *angle);
/* Criterio R² pesato (1 - 1/sqrt(n+1)), negativo se pendenza < 0 (da RSquared.mqh). */
gc_real gs_rsquared_test(const gc_real *data, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_FIT_H */
