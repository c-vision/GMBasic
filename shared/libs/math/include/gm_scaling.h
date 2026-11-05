/* scaling.h — equilibratura righe/colonne (potenze di 2) per LP/QP.
 *
 * Trasforma il problema in min-form prima del solve:
 *   A' = R A D,  b' = R b  (bounds riga: [lc,uc] *= r_i)
 *   c' = D c,    Q' = D Q D (bounds var: [lx,ux] /= d_j)
 * con r_i, d_j potenze esatte di 2 (nessun errore di round-off).
 * Dopo il solve: x_orig = D x', y_orig = R^{-1} y'.
 */
#ifndef GMMATH_GM_SCALING_H
#define GMMATH_GM_SCALING_H

/* Modifica in-place CSC (val), bounds e c/Q; scrive r[ncon], d[nvar] (allocati dal chiamante). */
void scale_equilibrate(int nvar, int ncon,
                       const int *ptr, const int *sub, double *val,
                       double *lc, double *uc, double *lx, double *ux,
                       double *c, double *Q /* nullable, dense nvar*nvar */,
                       double *r, double *d);

#endif /* GMMATH_GM_SCALING_H */