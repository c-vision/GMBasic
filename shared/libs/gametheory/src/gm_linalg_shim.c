/* gm_linalg_shim.c — gm_linalg_solve minimale (Gauss con pivot parziale),
 * per rendere gmbgametheory self-contained (niente gmbmath/gmbcompute). */
#include "gm_linalg.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

gm_status gm_linalg_solve(int n, const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real *m;
    int i, j, k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    m = (gm_real *)malloc((size_t)n * (size_t)(n + 1) * sizeof(gm_real));
    if (m == NULL) return GM_ERR_NO_MEM;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) m[(size_t)i * (size_t)(n + 1) + (size_t)j] = A[(size_t)i * (size_t)n + (size_t)j];
        m[(size_t)i * (size_t)(n + 1) + (size_t)n] = b[i];
    }
    for (k = 0; k < n; k++) {
        int piv = k;
        for (i = k + 1; i < n; i++)
            if (fabs(m[(size_t)i * (size_t)(n + 1) + (size_t)k]) > fabs(m[(size_t)piv * (size_t)(n + 1) + (size_t)k])) piv = i;
        if (fabs(m[(size_t)piv * (size_t)(n + 1) + (size_t)k]) < 1e-14) { free(m); return GM_ERR_SINGULAR; }
        if (piv != k)
            for (j = 0; j <= n; j++) { gm_real t = m[(size_t)k * (size_t)(n + 1) + (size_t)j]; m[(size_t)k * (size_t)(n + 1) + (size_t)j] = m[(size_t)piv * (size_t)(n + 1) + (size_t)j]; m[(size_t)piv * (size_t)(n + 1) + (size_t)j] = t; }
        for (i = k + 1; i < n; i++) {
            gm_real f = m[(size_t)i * (size_t)(n + 1) + (size_t)k] / m[(size_t)k * (size_t)(n + 1) + (size_t)k];
            for (j = k; j <= n; j++) m[(size_t)i * (size_t)(n + 1) + (size_t)j] -= f * m[(size_t)k * (size_t)(n + 1) + (size_t)j];
        }
    }
    for (i = n - 1; i >= 0; i--) {
        gm_real s = m[(size_t)i * (size_t)(n + 1) + (size_t)n];
        for (j = i + 1; j < n; j++) s -= m[(size_t)i * (size_t)(n + 1) + (size_t)j] * x[j];
        x[i] = s / m[(size_t)i * (size_t)(n + 1) + (size_t)i];
    }
    free(m);
    return GM_OK;
}
