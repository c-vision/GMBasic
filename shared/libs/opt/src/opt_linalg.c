/* opt_linalg.c — LU con pivoting parziale + Cholesky (self-contained, solo libm). */
#include "opt_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int gm_linalg_solve(int n, const gm_real *A, const gm_real *b, gm_real *x)
{
    double *m, *rhs;
    int *piv;
    int i, j, k, rc = GM_OK;

    if (A == NULL || b == NULL || x == NULL || n <= 0) return GM_ERR_ARG;
    m = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    rhs = (double *)malloc((size_t)n * sizeof(double));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (m == NULL || rhs == NULL || piv == NULL) { free(m); free(rhs); free(piv); return GM_ERR_ARG; }
    memcpy(m, A, (size_t)n * (size_t)n * sizeof(double));
    memcpy(rhs, b, (size_t)n * sizeof(double));
    for (i = 0; i < n; i++) piv[i] = i;

    for (k = 0; k < n; k++) {
        int p = k;
        double best = fabs(m[(size_t)k * (size_t)n + (size_t)k]);
        for (i = k + 1; i < n; i++) {
            double v = fabs(m[(size_t)i * (size_t)n + (size_t)k]);
            if (v > best) { best = v; p = i; }
        }
        if (best < 1e-300) { rc = GM_ERR_SINGULAR; break; }
        if (p != k) {
            for (j = 0; j < n; j++) {
                double t = m[(size_t)k * (size_t)n + (size_t)j];
                m[(size_t)k * (size_t)n + (size_t)j] = m[(size_t)p * (size_t)n + (size_t)j];
                m[(size_t)p * (size_t)n + (size_t)j] = t;
            }
            { double t = rhs[k]; rhs[k] = rhs[p]; rhs[p] = t; }
        }
        for (i = k + 1; i < n; i++) {
            double f = m[(size_t)i * (size_t)n + (size_t)k] / m[(size_t)k * (size_t)n + (size_t)k];
            if (f == 0.0) continue;
            for (j = k; j < n; j++)
                m[(size_t)i * (size_t)n + (size_t)j] -= f * m[(size_t)k * (size_t)n + (size_t)j];
            rhs[i] -= f * rhs[k];
        }
    }

    if (rc == GM_OK) {
        for (i = n - 1; i >= 0; i--) {
            double s = rhs[i];
            for (j = i + 1; j < n; j++) s -= m[(size_t)i * (size_t)n + (size_t)j] * x[j];
            x[i] = s / m[(size_t)i * (size_t)n + (size_t)i];
        }
    }
    free(m); free(rhs); free(piv);
    return rc;
}

int gmi_cholesky_solve(const gm_real *A, const gm_real *b, int n, gm_real *x)
{
    double *L = NULL, *y = NULL;
    int i, j, k;

    if (A == NULL || b == NULL || x == NULL || n <= 0) return 0;
    L = (double *)calloc((size_t)n * (size_t)n, sizeof(double));
    y = (double *)calloc((size_t)n, sizeof(double));
    if (L == NULL || y == NULL) { free(L); free(y); return 0; }

    for (i = 0; i < n; i++) {
        for (j = 0; j <= i; j++) {
            double s = A[(size_t)i * (size_t)n + (size_t)j];
            for (k = 0; k < j; k++)
                s -= L[(size_t)i * (size_t)n + (size_t)k] * L[(size_t)j * (size_t)n + (size_t)k];
            if (i == j) {
                if (s <= 0.0) { free(L); free(y); return 0; }
                L[(size_t)i * (size_t)n + (size_t)j] = sqrt(s);
            } else {
                L[(size_t)i * (size_t)n + (size_t)j] = s / L[(size_t)j * (size_t)n + (size_t)j];
            }
        }
    }
    for (i = 0; i < n; i++) {
        double s = b[i];
        for (k = 0; k < i; k++) s -= L[(size_t)i * (size_t)n + (size_t)k] * y[k];
        y[i] = s / L[(size_t)i * (size_t)n + (size_t)i];
    }
    for (i = n - 1; i >= 0; i--) {
        double s = y[i];
        for (k = i + 1; k < n; k++) s -= L[(size_t)k * (size_t)n + (size_t)i] * x[k];
        x[i] = s / L[(size_t)i * (size_t)n + (size_t)i];
    }
    free(L); free(y);
    return 1;
}
