/*
 * gn_fusedmm.c — SDDMM-SpMM fuso (da HipGraph/FusedMM).
 */
#include "gn_fusedmm.h"

gn_status gn_fusedmm_csr(const int *rowptr, const int *colidx, const gn_real *val,
                         int rows, int cols, int k,
                         const gn_real *X, const gn_real *Y,
                         gn_real alpha, gn_real beta, gn_real *Z)
{
    int i, e, d;
    if (!rowptr || !colidx || !X || !Y || !Z || rows < 1 || cols < 1 || k < 1) return GN_ERR_ARG;
    for (i = 0; i < rows; i++)
        for (d = 0; d < k; d++) Z[(size_t)i * k + d] *= beta;
    for (i = 0; i < rows; i++) {
        const gn_real *xi = X + (size_t)i * k;
        for (e = rowptr[i]; e < rowptr[i + 1]; e++) {
            int j = colidx[e];
            const gn_real *yj;
            gn_real dot = 0.0, w;
            if (j < 0 || j >= cols) continue;
            yj = Y + (size_t)j * k;
            for (d = 0; d < k; d++) dot += xi[d] * yj[d];   /* SDDMM */
            w = (val ? val[e] : 1.0) * dot * alpha;
            for (d = 0; d < k; d++) Z[(size_t)i * k + d] += w * yj[d];   /* SpMM */
        }
    }
    return GN_OK;
}

gn_status gn_aggregate_csr(const int *rowptr, const int *colidx, const gn_real *val,
                           int rows, int cols, int k, const gn_real *Y,
                           gn_real alpha, gn_real beta, gn_real *Z)
{
    int i, e, d;
    if (!rowptr || !colidx || !Y || !Z || rows < 1 || cols < 1 || k < 1) return GN_ERR_ARG;
    for (i = 0; i < rows; i++)
        for (d = 0; d < k; d++) Z[(size_t)i * k + d] *= beta;
    for (i = 0; i < rows; i++)
        for (e = rowptr[i]; e < rowptr[i + 1]; e++) {
            int j = colidx[e];
            gn_real w;
            const gn_real *yj;
            if (j < 0 || j >= cols) continue;
            yj = Y + (size_t)j * k;
            w = (val ? val[e] : 1.0) * alpha;
            for (d = 0; d < k; d++) Z[(size_t)i * k + d] += w * yj[d];
        }
    return GN_OK;
}
