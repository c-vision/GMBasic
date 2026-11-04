/* linalg.h — sottoinsieme minimo per gmbconic: solo l'autodecomposizione di
 * Jacobi usata da expcone.c per le matrici 3x3 (niente linalg completa). */
#ifndef GMBORTOOLS_LINALG_MIN_H
#define GMBORTOOLS_LINALG_MIN_H
#ifdef __cplusplus
extern "C" {
#endif
void dmat_eig_jacobi(int n, const double *A, double *eval, double *evec);
#ifdef __cplusplus
}
#endif
#endif
