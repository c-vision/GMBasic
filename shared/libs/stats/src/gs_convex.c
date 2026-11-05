/* gs_convex.c — convessità (regressione quadratica HC0) e capture up/down. */
#include "gs_convex.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>

/* C = A * B, matrici 3x3 row-major. */
static void mat3_mul(const gc_real *A, const gc_real *B, gc_real *C)
{
    int i, j, k;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            gc_real s = 0.0;
            for (k = 0; k < 3; k++) s += A[i * 3 + k] * B[k * 3 + j];
            C[i * 3 + j] = s;
        }
}

gc_status gs_convex_fit_ols(const gc_real *y, const gc_real *x, int n, gs_convex_fit *out)
{
    gc_real XtX[9], Xty[3], XtXinv[9], meat[9], tmp[9], V[9], beta[3];
    gc_real s0, sx, sx2, sx3, sx4, sy, sxy, sx2y;
    gc_real ybar, rss, tss;
    int i;

    if (!y || !x || !out || n < 3) return GC_ERR_ARG;

    s0 = (gc_real)n; sx = sx2 = sx3 = sx4 = 0.0;
    sy = sxy = sx2y = 0.0;
    for (i = 0; i < n; i++) {
        gc_real xi = x[i], x2 = xi * xi;
        sx   += xi;
        sx2  += x2;
        sx3  += x2 * xi;
        sx4  += x2 * x2;
        sy   += y[i];
        sxy  += xi * y[i];
        sx2y += x2 * y[i];
    }

    XtX[0] = s0;  XtX[1] = sx;  XtX[2] = sx2;
    XtX[3] = sx;  XtX[4] = sx2; XtX[5] = sx3;
    XtX[6] = sx2; XtX[7] = sx3; XtX[8] = sx4;
    Xty[0] = sy; Xty[1] = sxy; Xty[2] = sx2y;

    if (gm_linalg_solve(3, XtX, Xty, beta) != GC_OK) return GC_ERR_SINGULAR;
    if (gm_linalg_inverse(3, XtX, XtXinv) != GC_OK) return GC_ERR_SINGULAR;

    /* residui e R^2 */
    ybar = sy / s0;
    rss = 0.0; tss = 0.0;
    for (i = 0; i < n; i++) {
        gc_real xi = x[i];
        gc_real fit = beta[0] + beta[1] * xi + beta[2] * xi * xi;
        gc_real e = y[i] - fit, d = y[i] - ybar;
        rss += e * e;
        tss += d * d;
    }

    /* HC0: meat = sum_i e_i^2 * col_i col_i^T ; V = (X'X)^-1 meat (X'X)^-1 */
    {
        int r, c;
        for (r = 0; r < 9; r++) meat[r] = 0.0;
        for (i = 0; i < n; i++) {
            gc_real xi = x[i];
            gc_real fit = beta[0] + beta[1] * xi + beta[2] * xi * xi;
            gc_real e = y[i] - fit;
            gc_real e2 = e * e;
            gc_real col[3];
            int j, k;
            col[0] = 1.0; col[1] = xi; col[2] = xi * xi;
            for (j = 0; j < 3; j++)
                for (k = 0; k < 3; k++)
                    meat[j * 3 + k] += e2 * col[j] * col[k];
        }
        (void)r; (void)c;
    }
    mat3_mul(XtXinv, meat, tmp);
    mat3_mul(tmp, XtXinv, V);

    out->a = beta[0];
    out->b = beta[1];
    out->g = beta[2];
    out->r2 = (tss > 0.0) ? (1.0 - rss / tss) : 0.0;
    out->se_g = (V[8] > 0.0) ? sqrt(V[8]) : 0.0;
    out->t_g = (out->se_g > 0.0) ? (out->g / out->se_g) : 0.0;
    out->n = n;
    return GC_OK;
}

gc_status gs_convex_capture_calc(const gc_real *y, const gc_real *x, int n, gs_convex_capture *out)
{
    gc_real sum_yu = 0.0, sum_xu = 0.0, sum_yd = 0.0, sum_xd = 0.0;
    int i;

    if (!y || !x || !out || n < 1) return GC_ERR_ARG;

    out->n_up = 0;
    out->n_down = 0;
    for (i = 0; i < n; i++) {
        if (x[i] > 0.0) { sum_yu += y[i]; sum_xu += x[i]; out->n_up++; }
        else if (x[i] < 0.0) { sum_yd += y[i]; sum_xd += x[i]; out->n_down++; }
    }
    out->up_capture = (sum_xu != 0.0) ? (100.0 * sum_yu / sum_xu) : 0.0;
    out->down_capture = (sum_xd != 0.0) ? (100.0 * sum_yd / sum_xd) : 0.0;
    return GC_OK;
}

gc_status gs_convex_index_mean(const gc_real *rets, int n_obs, int n_assets, gc_real *out_index)
{
    int t, a;

    if (!rets || !out_index || n_obs < 3 || n_assets < 1) return GC_ERR_ARG;

    for (t = 0; t < n_obs; t++) {
        const gc_real *row = rets + (size_t)t * n_assets;
        gc_real s = 0.0;
        for (a = 0; a < n_assets; a++) s += row[a];
        out_index[t] = s / (gc_real)n_assets;
    }
    return GC_OK;
}

gc_status gs_convex_cross_section(const gc_real *rets, int n_obs, int n_assets, gs_convex_score *out_scores)
{
    gc_real *idx;
    int a;

    if (!rets || !out_scores || n_obs < 3 || n_assets < 1) return GC_ERR_ARG;

    idx = (gc_real *)malloc((size_t)n_obs * sizeof(gc_real));
    if (!idx) return GC_ERR_NO_MEM;

    if (gs_convex_index_mean(rets, n_obs, n_assets, idx) != GC_OK) {
        free(idx);
        return GC_ERR_ARG;
    }

    for (a = 0; a < n_assets; a++) {
        gc_real *y = (gc_real *)malloc((size_t)n_obs * sizeof(gc_real));
        gs_convex_fit f;
        if (!y) { free(idx); return GC_ERR_NO_MEM; }
        {
            int t;
            for (t = 0; t < n_obs; t++) y[t] = rets[(size_t)t * n_assets + a];
        }
        if (gs_convex_fit_ols(y, idx, n_obs, &f) == GC_OK) {
            out_scores[a].g = f.g;
            out_scores[a].t_g = f.t_g;
            out_scores[a].r2 = f.r2;
            out_scores[a].n = f.n;
        } else {
            out_scores[a].g = 0.0;
            out_scores[a].t_g = 0.0;
            out_scores[a].r2 = 0.0;
            out_scores[a].n = 0;
        }
        free(y);
    }

    free(idx);
    return GC_OK;
}

gc_status gs_convex_rank_desc(const gs_convex_score *scores, int n, int *out_order)
{
    int i, j;

    if (!scores || !out_order || n < 1) return GC_ERR_ARG;

    for (i = 0; i < n; i++) out_order[i] = i;

    /* Insertion sort: O(n^2), adatto a n piccolo; stabile, tie-break per indice crescente. */
    for (i = 1; i < n; i++) {
        int key = out_order[i];
        gc_real kg = scores[key].g;
        j = i - 1;
        while (j >= 0 && scores[out_order[j]].g < kg) {
            out_order[j + 1] = out_order[j];
            j--;
        }
        out_order[j + 1] = key;
    }
    return GC_OK;
}
