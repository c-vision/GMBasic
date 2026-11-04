/* jacobi.c — autodecomposizione simmetrica con rotazioni di Jacobi.
 * Copiata da gmbmath-linalg (gm_sparse_linalg.c) per rendere gmbconic autonomo. */
#include "linalg.h"
#include <math.h>
#include <stdlib.h>

void dmat_eig_jacobi(int n, const double *A, double *eval, double *evec) {
    int nn = n > 0 ? n : 1;
    double *W = (double *)calloc((size_t)nn * (size_t)nn, sizeof(double));
    double *V = (double *)calloc((size_t)nn * (size_t)nn, sizeof(double));
    int sweep, p, q, k, i, j;
    if (!W || !V) { free(W); free(V); return; }
    for (i = 0; i < n * n; i++) W[i] = A[i];
    for (i = 0; i < n; i++) V[i * n + i] = 1.0;
    for (sweep = 0; sweep < 100; sweep++) {
        double off = 0.0;
        for (p = 0; p < n; p++)
            for (q = p + 1; q < n; q++) off += W[p * n + q] * W[p * n + q];
        if (off < 1e-30) break;
        for (p = 0; p < n; p++) {
            for (q = p + 1; q < n; q++) {
                double apq = W[p * n + q];
                double theta, t, c, sn;
                if (fabs(apq) < 1e-32) continue;
                theta = (W[q * n + q] - W[p * n + p]) / (2.0 * apq);
                t = (theta >= 0.0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1.0));
                c = 1.0 / sqrt(t * t + 1.0);
                sn = t * c;
                for (k = 0; k < n; k++) {
                    double wp = W[p * n + k], wq = W[q * n + k];
                    W[p * n + k] = c * wp - sn * wq;
                    W[q * n + k] = sn * wp + c * wq;
                }
                for (k = 0; k < n; k++) {
                    double wp = W[k * n + p], wq = W[k * n + q];
                    W[k * n + p] = c * wp - sn * wq;
                    W[k * n + q] = sn * wp + c * wq;
                }
                for (k = 0; k < n; k++) {
                    double vp = V[k * n + p], vq = V[k * n + q];
                    V[k * n + p] = c * vp - sn * vq;
                    V[k * n + q] = sn * vp + c * vq;
                }
            }
        }
    }
    for (i = 0; i < n; i++) eval[i] = W[i * n + i];
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) evec[i * n + j] = V[i * n + j];
    free(W);
    free(V);
}
