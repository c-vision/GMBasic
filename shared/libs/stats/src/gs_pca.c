/* gs_pca.c — PCA (porting da gmi_pca legacy, Jacobi simmetrico) + PPCA. */
#include "gs_pca.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Jacobi simmetrico: a (n×n) → autovalori e, autovettori v (colonne). */
static void pca_jacobi(gc_real *a, int n, gc_real *e, gc_real *v)
{
    int i, sweep, p, q, k;
    for (i = 0; i < n * n; i++) v[i] = 0.0;
    for (i = 0; i < n; i++) v[i * n + i] = 1.0;
    for (sweep = 0; sweep < 100; sweep++) {
        gc_real off = 0.0;
        for (p = 0; p < n; p++)
            for (q = p + 1; q < n; q++)
                off += a[p * n + q] * a[p * n + q];
        if (off < 1e-30) break;
        for (p = 0; p < n; p++)
            for (q = p + 1; q < n; q++) {
                gc_real app, aqq, apq, theta, t, c, s;
                if (fabs(a[p * n + q]) < 1e-18) continue;
                app = a[p * n + p];
                aqq = a[q * n + q];
                apq = a[p * n + q];
                theta = (aqq - app) / (2.0 * apq);
                t = (theta >= 0.0 ? 1.0 : -1.0)
                    / (fabs(theta) + sqrt(theta * theta + 1.0));
                c = 1.0 / sqrt(t * t + 1.0);
                s = t * c;
                for (k = 0; k < n; k++) {
                    gc_real akp = a[k * n + p], akq = a[k * n + q];
                    a[k * n + p] = c * akp - s * akq;
                    a[k * n + q] = s * akp + c * akq;
                }
                for (k = 0; k < n; k++) {
                    gc_real apk = a[p * n + k], aqk = a[q * n + k];
                    a[p * n + k] = c * apk - s * aqk;
                    a[q * n + k] = s * apk + c * aqk;
                }
                for (k = 0; k < n; k++) {
                    gc_real vkp = v[k * n + p], vkq = v[k * n + q];
                    v[k * n + p] = c * vkp - s * vkq;
                    v[k * n + q] = s * vkp + c * vkq;
                }
            }
    }
    for (i = 0; i < n; i++) e[i] = a[i * n + i];
}

gc_status gs_pca(const gc_real *data, int rows, int cols, gs_pca_result *out)
{
    gc_real *cov, *v, *e, *mean;
    int i, j, r;
    if (!data || !out || rows <= 0 || cols <= 0) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    mean = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    cov = (gc_real *)calloc((size_t)(cols * cols), sizeof(gc_real));
    v = (gc_real *)calloc((size_t)(cols * cols), sizeof(gc_real));
    e = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    out->mean = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    out->eigenvalues = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    out->eigenvectors = (gc_real *)calloc((size_t)(cols * cols), sizeof(gc_real));
    out->explained_variance = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    out->explained_variance_ratio = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    if (!mean || !cov || !v || !e || !out->mean || !out->eigenvalues
        || !out->eigenvectors || !out->explained_variance
        || !out->explained_variance_ratio) {
        free(mean); free(cov); free(v); free(e); gs_pca_free(out);
        return GC_ERR_NO_MEM;
    }

    for (r = 0; r < rows; r++)
        for (j = 0; j < cols; j++) mean[j] += data[r * cols + j];
    for (j = 0; j < cols; j++) mean[j] /= rows;

    for (i = 0; i < cols; i++)
        for (j = 0; j < cols; j++) {
            gc_real s = 0.0;
            for (r = 0; r < rows; r++)
                s += (data[r * cols + i] - mean[i]) * (data[r * cols + j] - mean[j]);
            cov[i * cols + j] = s / (rows > 1 ? rows - 1 : 1);
        }

    pca_jacobi(cov, cols, e, v);

    /* ordina autovalori decrescenti, riordina le colonne di v */
    for (i = 0; i < cols; i++) {
        int best = i;
        for (j = i + 1; j < cols; j++)
            if (e[j] > e[best]) best = j;
        if (best != i) {
            gc_real tmp = e[i]; e[i] = e[best]; e[best] = tmp;
            for (r = 0; r < cols; r++) {
                gc_real t2 = v[r * cols + i];
                v[r * cols + i] = v[r * cols + best];
                v[r * cols + best] = t2;
            }
        }
    }

    {
        gc_real total = 0.0;
        for (i = 0; i < cols; i++) total += e[i] > 0.0 ? e[i] : 0.0;
        for (i = 0; i < cols; i++) {
            out->mean[i] = mean[i];
            out->eigenvalues[i] = e[i];
            out->explained_variance[i] = e[i];
            out->explained_variance_ratio[i] = total > 0.0 ? e[i] / total : 0.0;
            for (j = 0; j < cols; j++) out->eigenvectors[i * cols + j] = v[j * cols + i];
        }
    }
    out->n_features = cols;
    out->n_components = cols;

    free(mean); free(cov); free(v); free(e);
    return GC_OK;
}

gc_status gs_pca_transform(const gc_real *data, int rows, int cols,
                           const gs_pca_result *pca, int k, gc_real *scores)
{
    int r, i, j;
    if (!data || !pca || !scores || rows <= 0 || cols <= 0) return GC_ERR_ARG;
    if (k <= 0 || k > pca->n_components || pca->n_features != cols) return GC_ERR_ARG;
    for (r = 0; r < rows; r++)
        for (i = 0; i < k; i++) {
            gc_real s = 0.0;
            for (j = 0; j < cols; j++)
                s += (data[r * cols + j] - pca->mean[j]) * pca->eigenvectors[i * cols + j];
            scores[r * k + i] = s;
        }
    return GC_OK;
}

void gs_pca_free(gs_pca_result *out)
{
    if (!out) return;
    free(out->mean);
    free(out->eigenvalues);
    free(out->eigenvectors);
    free(out->explained_variance);
    free(out->explained_variance_ratio);
    memset(out, 0, sizeof(*out));
}

/* ============================== PPCA ============================== */

/* Autodecomposizione simmetrica con autovalori ordinati decrescenti e
 * autovettori per colonna (v[k*cols + j]). */
static void ppca_eig_desc(gc_real *a, int n, gc_real *e, gc_real *v)
{
    int i, j, r, best;
    pca_jacobi(a, n, e, v);
    for (i = 0; i < n; i++) {
        best = i;
        for (j = i + 1; j < n; j++)
            if (e[j] > e[best]) best = j;
        if (best != i) {
            gc_real t = e[i]; e[i] = e[best]; e[best] = t;
            for (r = 0; r < n; r++) {
                gc_real t2 = v[r * n + i];
                v[r * n + i] = v[r * n + best];
                v[r * n + best] = t2;
            }
        }
    }
    for (i = 0; i < n; i++)
        if (e[i] < 0.0) e[i] = 0.0;
}

gc_status gs_ppca_fit(const gc_real *data, int rows, int cols, int q,
                      gs_ppca_result *out)
{
    gc_real *mean = NULL, *cov = NULL, *v = NULL, *e = NULL;
    gc_real total = 0.0, s2, emean, s2min, loglik = 0.0;
    const gc_real twopi = 6.28318530717958647692;
    int i, j, r;
    if (!data || !out || rows <= 1 || cols <= 1 || q < 0 || q > cols - 1)
        return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    mean = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    cov = (gc_real *)calloc((size_t)(cols * cols), sizeof(gc_real));
    v = (gc_real *)calloc((size_t)(cols * cols), sizeof(gc_real));
    e = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    out->mean = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    out->W = q > 0 ? (gc_real *)calloc((size_t)(cols * q), sizeof(gc_real)) : NULL;
    if (!mean || !cov || !v || !e || !out->mean || (q > 0 && !out->W)) {
        free(mean); free(cov); free(v); free(e);
        gs_ppca_free(out);
        return GC_ERR_NO_MEM;
    }

    for (r = 0; r < rows; r++)
        for (j = 0; j < cols; j++) mean[j] += data[(size_t)r * cols + j];
    for (j = 0; j < cols; j++) mean[j] /= (gc_real)rows;

    /* covarianza campionaria (denominatore 1/rows, come la MLE PPCA) */
    for (i = 0; i < cols; i++)
        for (j = 0; j < cols; j++) {
            gc_real s = 0.0;
            for (r = 0; r < rows; r++)
                s += (data[(size_t)r * cols + i] - mean[i])
                   * (data[(size_t)r * cols + j] - mean[j]);
            cov[(size_t)i * cols + j] = s / (gc_real)rows;
        }

    ppca_eig_desc(cov, cols, e, v);

    for (i = 0; i < cols; i++) total += e[i];
    emean = total / (gc_real)cols;
    s2min = 1e-12 * emean;              /* pavimento per dati esattamente singolari */
    if (q == 0) {
        s2 = emean;                     /* modello isotropo: sigma2 = traccia/d */
    } else {
        s2 = 0.0;
        for (j = q; j < cols; j++) s2 += e[j];
        s2 /= (gc_real)(cols - q);      /* media degli autovalori scartati */
    }
    if (s2 < s2min) s2 = s2min;
    out->sigma2 = s2;

    for (j = 0; j < q; j++)
        for (i = 0; i < cols; i++) {
            gc_real d = e[j] - s2;
            out->W[(size_t)i * q + j] = d > 0.0 ? sqrt(d) * v[(size_t)i * cols + j] : 0.0;
        }

    /* ln|C| = sum ln c_j ; tr(C^-1 S) = sum e_j / c_j (S e C co-diagonalizzate). */
    for (j = 0; j < cols; j++) {
        gc_real c = s2;
        if (j < q) {
            gc_real d = e[j] - s2;
            c += (d > 0.0 ? d : 0.0);
        }
        if (c < 1e-300) c = 1e-300;
        loglik += log(c) + e[j] / c;
    }
    loglik = -0.5 * (gc_real)rows * ((gc_real)cols * log(twopi) + loglik);

    for (j = 0; j < cols; j++) out->mean[j] = mean[j];
    out->n_features = cols;
    out->n_components = q;
    out->loglik = loglik;
    out->dof = q * cols + 1 - q * (q - 1) / 2;

    free(mean); free(cov); free(v); free(e);
    return GC_OK;
}

gc_status gs_ppca_transform(const gc_real *data, int rows, int cols,
                            const gs_ppca_result *ppca, gc_real *scores)
{
    int q, i, j, r;
    gc_real *M, *Minv, *rhs;
    if (!data || !ppca || !scores || rows <= 0 || cols <= 0) return GC_ERR_ARG;
    if (ppca->n_features != cols || ppca->n_components < 0
        || ppca->n_components >= cols) return GC_ERR_ARG;
    q = ppca->n_components;
    if (q == 0) return GC_OK;

    M = (gc_real *)calloc((size_t)(q * q), sizeof(gc_real));
    Minv = (gc_real *)calloc((size_t)(q * q), sizeof(gc_real));
    rhs = (gc_real *)calloc((size_t)q, sizeof(gc_real));
    if (!M || !Minv || !rhs) {
        free(M); free(Minv); free(rhs);
        return GC_ERR_NO_MEM;
    }
    for (i = 0; i < q; i++)
        for (j = 0; j < q; j++) {
            gc_real s = 0.0;
            for (r = 0; r < cols; r++)
                s += ppca->W[(size_t)r * q + i] * ppca->W[(size_t)r * q + j];
            M[(size_t)i * q + j] = s + (i == j ? ppca->sigma2 : 0.0);
        }
    if (gm_linalg_inverse(q, M, Minv) != GM_OK) {
        free(M); free(Minv); free(rhs);
        return GC_ERR_ARG;
    }
    for (r = 0; r < rows; r++) {
        for (j = 0; j < q; j++) {
            gc_real s = 0.0;
            for (i = 0; i < cols; i++)
                s += ppca->W[(size_t)i * q + j]
                   * (data[(size_t)r * cols + i] - ppca->mean[i]);
            rhs[j] = s;
        }
        for (i = 0; i < q; i++) {
            gc_real s = 0.0;
            for (j = 0; j < q; j++) s += Minv[(size_t)i * q + j] * rhs[j];
            scores[(size_t)r * q + i] = s;
        }
    }
    free(M); free(Minv); free(rhs);
    return GC_OK;
}

gc_status gs_ppca_covariance(const gs_ppca_result *ppca, gc_real *out_cov)
{
    int d, q, i, j, k;
    if (!ppca || !out_cov || ppca->n_features <= 0) return GC_ERR_ARG;
    d = ppca->n_features;
    q = ppca->n_components;
    for (i = 0; i < d; i++)
        for (j = 0; j < d; j++) {
            gc_real s = (i == j) ? ppca->sigma2 : 0.0;
            for (k = 0; k < q; k++)
                s += ppca->W[(size_t)i * q + k] * ppca->W[(size_t)j * q + k];
            out_cov[(size_t)i * d + j] = s;
        }
    return GC_OK;
}

gc_status gs_ppca_select_q(const gc_real *data, int rows, int cols,
                           int q_min, int q_max, int criterion,
                           int *out_q, gc_real *out_score)
{
    int q, best_q = -1;
    gc_real best = 0.0, pen;
    if (!data || !out_q || rows <= 1 || cols <= 1 || q_min < 0
        || q_max > cols - 1 || q_min > q_max) return GC_ERR_ARG;
    pen = (criterion == GS_PPCA_BIC) ? log((gc_real)rows) : 2.0;
    for (q = q_min; q <= q_max; q++) {
        gs_ppca_result r;
        gc_real score;
        if (gs_ppca_fit(data, rows, cols, q, &r) != GC_OK) continue;
        score = -2.0 * r.loglik + pen * (gc_real)r.dof;
        if (best_q < 0 || score < best) { best = score; best_q = q; }
        gs_ppca_free(&r);
    }
    if (best_q < 0) return GC_ERR_NO_MEM;
    *out_q = best_q;
    if (out_score) *out_score = best;
    return GC_OK;
}

void gs_ppca_free(gs_ppca_result *out)
{
    if (!out) return;
    free(out->mean);
    free(out->W);
    memset(out, 0, sizeof(*out));
}

/* M = W^T W + sigma2 I (q x q): inversa e log|M| (via Cholesky). */
static int ppca_inv_logdetM(const gc_real *W, int d, int q, gc_real sigma2,
                            gc_real *Minv, gc_real *out_logdet)
{
    gc_real *M, *L, ld = 0.0;
    int i, j, r;
    M = (gc_real *)malloc((size_t)q * (size_t)q * sizeof(gc_real));
    L = (gc_real *)malloc((size_t)q * (size_t)q * sizeof(gc_real));
    if (!M || !L) { free(M); free(L); return -1; }
    for (i = 0; i < q; i++)
        for (j = 0; j < q; j++) {
            gc_real s = (i == j) ? sigma2 : 0.0;
            for (r = 0; r < d; r++) s += W[(size_t)r * q + i] * W[(size_t)r * q + j];
            M[(size_t)i * q + j] = s;
            L[(size_t)i * q + j] = s;
        }
    if (gm_linalg_cholesky(q, L) != GM_OK) { free(M); free(L); return -1; }
    for (i = 0; i < q; i++) {
        if (L[(size_t)i * q + i] <= 0.0) { free(M); free(L); return -1; }
        ld += 2.0 * log(L[(size_t)i * q + i]);
    }
    if (gm_linalg_inverse(q, M, Minv) != GM_OK) { free(M); free(L); return -1; }
    free(M); free(L);
    *out_logdet = ld;
    return 0;
}

/* Log-verosimiglianza generale del modello PPCA (qualsiasi W).
 * Usa il lemma del determinante e Woodbury: O(rows*d*q + q^3). */
static gc_real ppca_loglik_general(const gc_real *data, int rows, int cols,
                                   const gc_real *mean, const gc_real *W, int q,
                                   gc_real sigma2, int *ok)
{
    const gc_real twopi = 6.28318530717958647692;
    gc_real *A = NULL, *WtA = NULL, *Minv = NULL, *u = NULL, *dot = NULL;
    gc_real trU = 0.0, t1 = 0.0, logdetM = 0.0, lnC, trCinS, L = NAN;
    int i, j, k, r;
    *ok = 0;
    if (sigma2 <= 0.0) return NAN;
    A = (gc_real *)calloc((size_t)cols * (q > 0 ? q : 1), sizeof(gc_real));
    WtA = (gc_real *)calloc((size_t)(q > 0 ? q : 1) * (q > 0 ? q : 1), sizeof(gc_real));
    Minv = (gc_real *)calloc((size_t)(q > 0 ? q : 1) * (q > 0 ? q : 1), sizeof(gc_real));
    u = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    dot = (gc_real *)calloc((size_t)(q > 0 ? q : 1), sizeof(gc_real));
    if (!A || !WtA || !Minv || !u || !dot) goto done;

    for (r = 0; r < rows; r++) {
        for (i = 0; i < cols; i++) {
            u[i] = data[(size_t)r * cols + i] - mean[i];
            trU += u[i] * u[i];
        }
        for (k = 0; k < q; k++) {
            gc_real s = 0.0;
            for (i = 0; i < cols; i++) s += W[(size_t)i * q + k] * u[i];
            dot[k] = s;
        }
        for (i = 0; i < cols; i++)
            for (k = 0; k < q; k++) A[(size_t)i * q + k] += u[i] * dot[k];
    }

    if (q == 0) {
        lnC = (gc_real)cols * log(sigma2);
        trCinS = trU / ((gc_real)rows * sigma2);
    } else {
        if (ppca_inv_logdetM(W, cols, q, sigma2, Minv, &logdetM) != 0) goto done;
        for (i = 0; i < q; i++)
            for (k = 0; k < q; k++) {
                gc_real s = 0.0;
                for (j = 0; j < cols; j++) s += W[(size_t)j * q + i] * A[(size_t)j * q + k];
                WtA[(size_t)i * q + k] = s;
            }
        for (k = 0; k < q; k++)
            for (j = 0; j < q; j++) t1 += Minv[(size_t)k * q + j] * WtA[(size_t)j * q + k];
        /* 2(d-q) ln(sigma) = (d-q) ln(sigma2) */
        lnC = ((gc_real)cols - (gc_real)q) * log(sigma2) + logdetM;
        trCinS = (trU - t1) / ((gc_real)rows * sigma2);
    }
    L = -0.5 * (gc_real)rows * ((gc_real)cols * log(twopi) + lnC + trCinS);
    *ok = 1;

done:
    free(A); free(WtA); free(Minv); free(u); free(dot);
    return L;
}

gc_real gs_ppca_loglik(const gc_real *data, int rows, int cols,
                       const gs_ppca_result *ppca)
{
    int ok = 0;
    gc_real L;
    if (!data || !ppca || rows <= 0 || cols <= 0 || ppca->n_features != cols)
        return NAN;
    L = ppca_loglik_general(data, rows, cols, ppca->mean, ppca->W,
                            ppca->n_components, ppca->sigma2, &ok);
    return ok ? L : NAN;
}

gc_status gs_ppca_em(const gc_real *data, int rows, int cols, int q,
                     int iters, gc_real tol, gs_ppca_result *out)
{
    gc_real *A, *WtA, *Minv, *G, *Ginv, *Wnew, *P, *u, *dot;
    gc_real trU, s2, s2min, L, Lprev = 0.0, logdetM;
    int i, j, k, r, it, ok;
    if (!data || !out || rows <= 1 || cols <= 1 || q < 0 || q > cols - 1 || iters < 1)
        return GC_ERR_ARG;
    if (gs_ppca_fit(data, rows, cols, q, out) != GC_OK) return GC_ERR_NO_MEM;
    if (q == 0) return GC_OK;   /* isotropo: la MLE in forma chiusa e' gia' ottima */

    A = (gc_real *)calloc((size_t)cols * q, sizeof(gc_real));
    WtA = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    Minv = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    G = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    Ginv = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    Wnew = (gc_real *)calloc((size_t)cols * q, sizeof(gc_real));
    P = (gc_real *)calloc((size_t)cols * q, sizeof(gc_real));
    u = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    dot = (gc_real *)calloc((size_t)q, sizeof(gc_real));
    if (!A || !WtA || !Minv || !G || !Ginv || !Wnew || !P || !u || !dot) {
        free(A); free(WtA); free(Minv); free(G); free(Ginv);
        free(Wnew); free(P); free(u); free(dot);
        return GC_ERR_NO_MEM;
    }

    /* init: perturbazione deterministica della stima chiusa per esercitare l'EM */
    for (i = 0; i < cols * q; i++)
        out->W[i] *= 1.0 + 0.05 * (((gc_real)((i * 7) % 5) - 2.0) / 2.0);
    s2min = 1e-12 * out->sigma2;

    L = ppca_loglik_general(data, rows, cols, out->mean, out->W, q, out->sigma2, &ok);
    if (!ok) { free(A); free(WtA); free(Minv); free(G); free(Ginv);
               free(Wnew); free(P); free(u); free(dot); return GC_ERR_ARG; }

    for (it = 0; it < iters; it++) {
        /* A = U W  (d x q): u_n = t_n - mu */
        memset(A, 0, (size_t)cols * q * sizeof(gc_real));
        trU = 0.0;
        for (r = 0; r < rows; r++) {
            for (i = 0; i < cols; i++) {
                u[i] = data[(size_t)r * cols + i] - out->mean[i];
                trU += u[i] * u[i];
            }
            for (k = 0; k < q; k++) {
                gc_real s = 0.0;
                for (i = 0; i < cols; i++) s += out->W[(size_t)i * q + k] * u[i];
                dot[k] = s;
            }
            for (i = 0; i < cols; i++)
                for (k = 0; k < q; k++) A[(size_t)i * q + k] += u[i] * dot[k];
        }
        if (ppca_inv_logdetM(out->W, cols, q, out->sigma2, Minv, &logdetM) != 0) break;
        for (i = 0; i < q; i++)
            for (k = 0; k < q; k++) {
                gc_real s = 0.0;
                for (j = 0; j < cols; j++) s += out->W[(size_t)j * q + i] * A[(size_t)j * q + k];
                WtA[(size_t)i * q + k] = s;
            }
        /* G = sigma2 I + M^-1 (W^T S W) = sigma2 I + (1/rows) M^-1 WtA */
        for (i = 0; i < q; i++)
            for (k = 0; k < q; k++) {
                gc_real s = (i == k) ? out->sigma2 : 0.0;
                for (j = 0; j < q; j++) s += Minv[(size_t)i * q + j] * WtA[(size_t)j * q + k] / (gc_real)rows;
                G[(size_t)i * q + k] = s;
            }
        if (gm_linalg_inverse(q, G, Ginv) != GM_OK) break;
        /* Wtilde = S W G^-1 = (1/rows) A G^-1 */
        for (i = 0; i < cols; i++)
            for (k = 0; k < q; k++) {
                gc_real s = 0.0;
                for (j = 0; j < q; j++) s += A[(size_t)i * q + j] * Ginv[(size_t)j * q + k];
                Wnew[(size_t)i * q + k] = s / (gc_real)rows;
            }
        /* P = A M^-1 ; sigma2 = (trU - tr(Wtilde^T A M^-1)) / (rows*d) */
        for (i = 0; i < cols; i++)
            for (k = 0; k < q; k++) {
                gc_real s = 0.0;
                for (j = 0; j < q; j++) s += A[(size_t)i * q + j] * Minv[(size_t)j * q + k];
                P[(size_t)i * q + k] = s;
            }
        {
            gc_real q2 = 0.0;
            for (i = 0; i < cols; i++)
                for (k = 0; k < q; k++) q2 += Wnew[(size_t)i * q + k] * P[(size_t)i * q + k];
            s2 = (trU - q2) / ((gc_real)rows * (gc_real)cols);
            if (s2 < s2min) s2 = s2min;
        }
        memcpy(out->W, Wnew, (size_t)cols * q * sizeof(gc_real));
        out->sigma2 = s2;

        Lprev = L;
        L = ppca_loglik_general(data, rows, cols, out->mean, out->W, q, out->sigma2, &ok);
        if (!ok) break;
        if (fabs(L - Lprev) < tol) break;
    }

    free(A); free(WtA); free(Minv); free(G); free(Ginv);
    free(Wnew); free(P); free(u); free(dot);
    out->loglik = L;
    out->n_features = cols;
    out->n_components = q;
    out->dof = q * cols + 1 - q * (q - 1) / 2;
    return GC_OK;
}

/* ========================= Mistura di PPCA ========================= */

typedef struct { gc_real s; int i; } ppca_pair;

static int ppca_pair_cmp(const void *a, const void *b)
{
    const ppca_pair *pa = (const ppca_pair *)a, *pb = (const ppca_pair *)b;
    if (pa->s < pb->s) return -1;
    if (pa->s > pb->s) return 1;
    return pa->i < pb->i ? -1 : (pa->i > pb->i ? 1 : 0);
}

/* E-step: responsabilita' gamma_nc e log-verosimiglianza della mistura. */
static gc_status ppca_mix_estep(const gc_real *data, int rows, int cols,
                                const gs_ppca_mixture_result *m,
                                gc_real *resp, gc_real *out_L)
{
    const gc_real twopi = 6.28318530717958647692;
    int d = cols, q = m->n_components, K = m->n_mixtures;
    gc_real *Minv, *ld, *lp, *u, *v;
    gc_real L = 0.0;
    int n, c, k, l, i;
    Minv = (gc_real *)calloc((size_t)K * (q > 0 ? q : 1) * (q > 0 ? q : 1), sizeof(gc_real));
    ld = (gc_real *)calloc((size_t)K, sizeof(gc_real));
    lp = (gc_real *)calloc((size_t)K, sizeof(gc_real));
    u = (gc_real *)calloc((size_t)d, sizeof(gc_real));
    v = (gc_real *)calloc((size_t)(q > 0 ? q : 1), sizeof(gc_real));
    if (!Minv || !ld || !lp || !u || !v) {
        free(Minv); free(ld); free(lp); free(u); free(v);
        return GC_ERR_NO_MEM;
    }
    for (c = 0; c < K; c++) {
        if (q > 0) {
            if (ppca_inv_logdetM(&m->W[(size_t)c * d * q], d, q, m->sigma2[c],
                                 &Minv[(size_t)c * q * q], &ld[c]) != 0) {
                free(Minv); free(ld); free(lp); free(u); free(v);
                return GC_ERR_ARG;
            }
        } else {
            ld[c] = (gc_real)d * log(m->sigma2[c]);
        }
    }
    for (n = 0; n < rows; n++) {
        gc_real mx = -INFINITY, denom;
        for (c = 0; c < K; c++) {
            gc_real qd = 0.0, uu = 0.0;
            const gc_real *Wc = q > 0 ? &m->W[(size_t)c * d * q] : NULL;
            for (i = 0; i < d; i++) {
                u[i] = data[(size_t)n * cols + i] - m->mean[(size_t)c * d + i];
                uu += u[i] * u[i];
            }
            if (q > 0) {
                for (k = 0; k < q; k++) {
                    gc_real s = 0.0;
                    for (i = 0; i < d; i++) s += Wc[(size_t)i * q + k] * u[i];
                    v[k] = s;
                }
                for (k = 0; k < q; k++)
                    for (l = 0; l < q; l++)
                        qd += v[k] * Minv[(size_t)c * q * q + k * q + l] * v[l];
            }
            lp[c] = log(m->weights[c])
                  - 0.5 * ((gc_real)d * log(twopi) + ld[c] + (uu - qd) / m->sigma2[c]);
            if (lp[c] > mx) mx = lp[c];
        }
        denom = 0.0;
        for (c = 0; c < K; c++) denom += exp(lp[c] - mx);
        denom = log(denom) + mx;
        L += denom;
        for (c = 0; c < K; c++) resp[(size_t)n * K + c] = exp(lp[c] - denom);
    }
    free(Minv); free(ld); free(lp); free(u); free(v);
    *out_L = L;
    return GC_OK;
}

gc_status gs_ppca_mixture_responsibilities(const gc_real *data, int rows,
                                           const gs_ppca_mixture_result *m,
                                           gc_real *out_resp)
{
    gc_real L = 0.0;
    if (!data || !m || !out_resp || rows <= 0 || m->n_features <= 0 || m->n_mixtures <= 0)
        return GC_ERR_ARG;
    return ppca_mix_estep(data, rows, m->n_features, m, out_resp, &L);
}

gc_status gs_ppca_mixture(const gc_real *data, int rows, int cols, int q,
                          int n_mix, int iters, gc_real tol,
                          gs_ppca_mixture_result *out)
{
    gs_ppca_result gf, g1;
    gc_real *resp = NULL, *u = NULL, *v = NULL, *Ex = NULL;
    gc_real *Sx = NULL, *Sxx = NULL, *Sxxinv = NULL, *Minv = NULL, *Wnew = NULL;
    gc_real *score = NULL, *count = NULL, *grp = NULL, ldum;
    ppca_pair *pr = NULL;
    int *order = NULL;
    int d = cols, K = n_mix, i, k, l, n, c, it, ret = GC_OK;
    gc_real L = 0.0, Lprev = 0.0, s2min, gvar = 0.0, gmean = 0.0;
    if (!data || !out || rows <= 1 || cols <= 1 || q < 0 || q > cols - 1
        || n_mix < 1 || iters < 1) return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));
    if (gs_ppca_fit(data, rows, cols, q, &gf) != GC_OK) return GC_ERR_NO_MEM;

    /* varianza globale (pavimento per sigma2) */
    for (n = 0; n < rows; n++)
        for (i = 0; i < d; i++) gmean += data[(size_t)n * cols + i];
    gmean /= (gc_real)(rows * d);
    for (n = 0; n < rows; n++)
        for (i = 0; i < d; i++) {
            gc_real t = data[(size_t)n * cols + i] - gmean;
            gvar += t * t;
        }
    gvar /= (gc_real)(rows * d);
    s2min = gvar > 0.0 ? 1e-12 * gvar : 1e-300;

    out->n_features = d;
    out->n_components = q;
    out->n_mixtures = K;
    out->weights = (gc_real *)calloc((size_t)K, sizeof(gc_real));
    out->mean = (gc_real *)calloc((size_t)K * d, sizeof(gc_real));
    out->sigma2 = (gc_real *)calloc((size_t)K, sizeof(gc_real));
    out->W = q > 0 ? (gc_real *)calloc((size_t)K * d * q, sizeof(gc_real)) : NULL;
    count = (gc_real *)calloc((size_t)K, sizeof(gc_real));
    order = (int *)malloc((size_t)rows * sizeof(int));
    resp = (gc_real *)calloc((size_t)rows * K, sizeof(gc_real));
    if (!out->weights || !out->mean || !out->sigma2 || (q > 0 && !out->W)
        || !count || !order || !resp) { ret = GC_ERR_NO_MEM; goto cleanup; }

    /* init deterministica: split per la prima PC globale */
    if (cols >= 2) {
        if (gs_ppca_fit(data, rows, cols, 1, &g1) != GC_OK) { ret = GC_ERR_NO_MEM; goto cleanup; }
        score = (gc_real *)malloc((size_t)rows * sizeof(gc_real));
        pr = (ppca_pair *)malloc((size_t)rows * sizeof(ppca_pair));
        if (!score || !pr) { gs_ppca_free(&g1); ret = GC_ERR_NO_MEM; goto cleanup; }
        gs_ppca_transform(data, rows, cols, &g1, score);
        for (n = 0; n < rows; n++) { pr[n].s = score[n]; pr[n].i = n; }
        qsort(pr, (size_t)rows, sizeof(ppca_pair), ppca_pair_cmp);
        for (n = 0; n < rows; n++) order[n] = pr[n].i;
        gs_ppca_free(&g1);
    } else {
        for (n = 0; n < rows; n++) order[n] = n;
    }
    for (n = 0; n < rows; n++) {
        c = (int)(((long)n * K) / rows);
        if (c >= K) c = K - 1;
        count[c] += 1.0;
    }
    grp = (gc_real *)malloc((size_t)rows * d * sizeof(gc_real));
    if (!grp) { ret = GC_ERR_NO_MEM; goto cleanup; }
    for (c = 0; c < K; c++) {
        int nc = (int)count[c], s = 0;
        gs_ppca_result gp;
        for (n = 0; n < rows; n++) {
            int g = (int)(((long)n * K) / rows);
            if (g >= K) g = K - 1;
            if (g == c) {
                for (i = 0; i < d; i++) grp[(size_t)s * d + i] = data[(size_t)order[n] * cols + i];
                s++;
            }
        }
        out->weights[c] = nc > 0 ? count[c] / (gc_real)rows : 1e-9;
        if (nc >= 2 && gs_ppca_fit(grp, nc, d, q, &gp) == GC_OK) {
            for (i = 0; i < d; i++) out->mean[(size_t)c * d + i] = gp.mean[i];
            out->sigma2[c] = gp.sigma2;
            if (q > 0) memcpy(&out->W[(size_t)c * d * q], gp.W, (size_t)d * q * sizeof(gc_real));
            gs_ppca_free(&gp);
        } else {
            for (i = 0; i < d; i++) out->mean[(size_t)c * d + i] = gf.mean[i];
            out->sigma2[c] = gf.sigma2;
            if (q > 0) memcpy(&out->W[(size_t)c * d * q], gf.W, (size_t)d * q * sizeof(gc_real));
        }
    }

    /* buffer M-step */
    u = (gc_real *)calloc((size_t)d, sizeof(gc_real));
    v = (gc_real *)calloc((size_t)(q > 0 ? q : 1), sizeof(gc_real));
    Ex = (gc_real *)calloc((size_t)(q > 0 ? q : 1), sizeof(gc_real));
    Sx = (gc_real *)calloc((size_t)d * (q > 0 ? q : 1), sizeof(gc_real));
    Sxx = (gc_real *)calloc((size_t)(q > 0 ? q : 1) * (q > 0 ? q : 1), sizeof(gc_real));
    Sxxinv = (gc_real *)calloc((size_t)(q > 0 ? q : 1) * (q > 0 ? q : 1), sizeof(gc_real));
    Minv = (gc_real *)calloc((size_t)(q > 0 ? q : 1) * (q > 0 ? q : 1), sizeof(gc_real));
    Wnew = (gc_real *)calloc((size_t)d * (q > 0 ? q : 1), sizeof(gc_real));
    if (!u || !v || !Ex || !Sx || !Sxx || !Sxxinv || !Minv || !Wnew) { ret = GC_ERR_NO_MEM; goto cleanup; }

    for (it = 0; it < iters; it++) {
        if (ppca_mix_estep(data, rows, cols, out, resp, &L) != GC_OK) { ret = GC_ERR_ARG; goto cleanup; }
        if (it > 0) {
            gc_real diff = L - Lprev;
            if (diff < 0.0) diff = -diff;
            if (diff < tol) break;
        }
        Lprev = L;
        for (c = 0; c < K; c++) {
            gc_real Nc = 0.0, sum_uu = 0.0;
            gc_real *Wc = q > 0 ? &out->W[(size_t)c * d * q] : NULL;
            for (n = 0; n < rows; n++) Nc += resp[(size_t)n * K + c];
            if (Nc < 1e-9) continue;
            out->weights[c] = Nc / (gc_real)rows;
            for (i = 0; i < d; i++) {
                gc_real s = 0.0;
                for (n = 0; n < rows; n++) s += resp[(size_t)n * K + c] * data[(size_t)n * cols + i];
                out->mean[(size_t)c * d + i] = s / Nc;
            }
            if (q == 0) {
                for (n = 0; n < rows; n++) {
                    gc_real uu = 0.0;
                    for (i = 0; i < d; i++) {
                        gc_real t = data[(size_t)n * cols + i] - out->mean[(size_t)c * d + i];
                        uu += t * t;
                    }
                    sum_uu += resp[(size_t)n * K + c] * uu;
                }
                {
                    gc_real s2 = sum_uu / (Nc * (gc_real)d);
                    if (s2 < s2min) s2 = s2min;
                    out->sigma2[c] = s2;
                }
                continue;
            }
            if (ppca_inv_logdetM(Wc, d, q, out->sigma2[c], Minv, &ldum) != 0) { ret = GC_ERR_ARG; goto cleanup; }
            memset(Sx, 0, (size_t)d * q * sizeof(gc_real));
            memset(Sxx, 0, (size_t)q * q * sizeof(gc_real));
            for (n = 0; n < rows; n++) {
                gc_real g = resp[(size_t)n * K + c];
                if (g <= 0.0) continue;
                for (i = 0; i < d; i++) u[i] = data[(size_t)n * cols + i] - out->mean[(size_t)c * d + i];
                for (k = 0; k < q; k++) {
                    gc_real s = 0.0;
                    for (i = 0; i < d; i++) s += Wc[(size_t)i * q + k] * u[i];
                    v[k] = s;
                }
                for (k = 0; k < q; k++) {
                    gc_real s = 0.0;
                    for (l = 0; l < q; l++) s += Minv[(size_t)k * q + l] * v[l];
                    Ex[k] = s;
                }
                for (i = 0; i < d; i++)
                    for (k = 0; k < q; k++) Sx[(size_t)i * q + k] += g * u[i] * Ex[k];
                for (k = 0; k < q; k++)
                    for (l = 0; l < q; l++)
                        Sxx[(size_t)k * q + l] += g * (out->sigma2[c] * Minv[(size_t)k * q + l] + Ex[k] * Ex[l]);
                for (i = 0; i < d; i++) sum_uu += g * u[i] * u[i];
            }
            if (gm_linalg_inverse(q, Sxx, Sxxinv) != GM_OK) { ret = GC_ERR_ARG; goto cleanup; }
            for (i = 0; i < d; i++)
                for (k = 0; k < q; k++) {
                    gc_real s = 0.0;
                    for (l = 0; l < q; l++) s += Sx[(size_t)i * q + l] * Sxxinv[(size_t)l * q + k];
                    Wnew[(size_t)i * q + k] = s;
                }
            {
                gc_real t2 = 0.0, t3 = 0.0, s2;
                for (i = 0; i < d; i++)
                    for (k = 0; k < q; k++) t2 += Wnew[(size_t)i * q + k] * Sx[(size_t)i * q + k];
                for (k = 0; k < q; k++)
                    for (l = 0; l < q; l++) {
                        gc_real s = 0.0;
                        for (i = 0; i < d; i++) s += Wnew[(size_t)i * q + k] * Wnew[(size_t)i * q + l];
                        t3 += Sxx[(size_t)k * q + l] * s;
                    }
                s2 = (sum_uu - 2.0 * t2 + t3) / (Nc * (gc_real)d);
                if (s2 < s2min) s2 = s2min;
                out->sigma2[c] = s2;
            }
            memcpy(Wc, Wnew, (size_t)d * q * sizeof(gc_real));
        }
    }

    if (ppca_mix_estep(data, rows, cols, out, resp, &L) != GC_OK) { ret = GC_ERR_ARG; goto cleanup; }
    out->loglik = L;

cleanup:
    free(resp); free(u); free(v); free(Ex); free(Sx); free(Sxx);
    free(Sxxinv); free(Minv); free(Wnew); free(score); free(count);
    free(grp); free(pr); free(order);
    gs_ppca_free(&gf);
    if (ret != GC_OK) gs_ppca_mixture_free(out);
    return (gc_status)ret;
}

void gs_ppca_mixture_free(gs_ppca_mixture_result *out)
{
    if (!out) return;
    free(out->weights);
    free(out->mean);
    free(out->W);
    free(out->sigma2);
    memset(out, 0, sizeof(*out));
}

/* ==================== PPCA con dati mancanti ==================== */

/* Passata E esatta: log-verosimiglianza sui valori osservati e, se richiesto,
 * le statistiche sufficienti per il passo M:
 *   M1[i,a]   = sum_n E[(t_ni - mu_i) x_a]          (d x q)
 *   X2sum[a,b]= sum_n E[x_a x_b]                     (q x q)
 *   Mmat[n,a] = E[x_a | t_n^O]                       (rows x q)
 *   out_suu   = sum_n E[||t_n - mu||^2]
 * X2_n = E[x x^T | t_n^O] = K^-1 + m m^T ;  E[t_i^M x^T] = X2_n W_i^T. */
static gc_real ppca_miss_pass(const gc_real *data, int rows, int cols,
                              const gc_real *mean, const gc_real *W, int q,
                              gc_real sigma2, const unsigned char *mask,
                              gc_real *M1, gc_real *X2sum, gc_real *out_suu,
                              gc_real *Mmat, int *ok)
{
    const gc_real twopi = 6.28318530717958647692;
    gc_real *K = NULL, *Kinv = NULL, *Kc = NULL, *g = NULL, *m = NULL, *X2 = NULL;
    int *obs = NULL;
    gc_real L = 0.0, suu = 0.0;
    int n, i, a, b, k, no;
    *ok = 0;
    if (sigma2 <= 0.0) return NAN;
    K = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    Kinv = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    Kc = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    g = (gc_real *)calloc((size_t)q, sizeof(gc_real));
    m = (gc_real *)calloc((size_t)q, sizeof(gc_real));
    X2 = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    obs = (int *)malloc((size_t)cols * sizeof(int));
    if (!K || !Kinv || !Kc || !g || !m || !X2 || !obs) {
        free(K); free(Kinv); free(Kc); free(g); free(m); free(X2); free(obs);
        return NAN;
    }
    for (n = 0; n < rows; n++) {
        gc_real e2 = 0.0, vartr = 0.0, mwm2 = 0.0, gm = 0.0, ld = 0.0, quad;
        no = 0;
        for (i = 0; i < cols; i++) if (mask[(size_t)n * cols + i]) obs[no++] = i;
        for (a = 0; a < q; a++)
            for (b = 0; b < q; b++) {
                gc_real s = (a == b) ? 1.0 : 0.0;
                for (k = 0; k < no; k++) {
                    i = obs[k];
                    s += W[(size_t)i * q + a] * W[(size_t)i * q + b] / sigma2;
                }
                K[(size_t)a * q + b] = s;
                Kc[(size_t)a * q + b] = s;
            }
        if (gm_linalg_cholesky(q, Kc) != GM_OK) goto done;
        for (a = 0; a < q; a++) {
            if (Kc[(size_t)a * q + a] <= 0.0) goto done;
            ld += 2.0 * log(Kc[(size_t)a * q + a]);
        }
        if (gm_linalg_inverse(q, K, Kinv) != GM_OK) goto done;
        for (a = 0; a < q; a++) {
            gc_real s = 0.0;
            for (k = 0; k < no; k++) {
                i = obs[k];
                s += W[(size_t)i * q + a] * (data[(size_t)n * cols + i] - mean[i]);
            }
            g[a] = s;
        }
        for (a = 0; a < q; a++) {
            gc_real s = 0.0;
            for (b = 0; b < q; b++) s += Kinv[(size_t)a * q + b] * g[b];
            m[a] = s / sigma2;
        }
        for (a = 0; a < q; a++)
            for (b = 0; b < q; b++)
                X2[(size_t)a * q + b] = Kinv[(size_t)a * q + b] + m[a] * m[b];
        for (k = 0; k < no; k++) {
            i = obs[k];
            { gc_real e = data[(size_t)n * cols + i] - mean[i]; e2 += e * e; }
        }
        for (i = 0; i < cols; i++)
            if (!mask[(size_t)n * cols + i]) {
                gc_real wm = 0.0, s = 0.0;
                for (a = 0; a < q; a++) wm += W[(size_t)i * q + a] * m[a];
                mwm2 += wm * wm;
                for (a = 0; a < q; a++)
                    for (b = 0; b < q; b++)
                        s += W[(size_t)i * q + a] * Kinv[(size_t)a * q + b] * W[(size_t)i * q + b];
                vartr += sigma2 + s;
            }
        for (a = 0; a < q; a++) gm += g[a] * m[a];
        if (M1) {
            for (k = 0; k < no; k++) {
                i = obs[k];
                { gc_real e = data[(size_t)n * cols + i] - mean[i];
                  for (a = 0; a < q; a++) M1[(size_t)i * q + a] += e * m[a]; }
            }
            for (i = 0; i < cols; i++)
                if (!mask[(size_t)n * cols + i]) {
                    for (a = 0; a < q; a++) {
                        gc_real s = 0.0;
                        for (b = 0; b < q; b++) s += X2[(size_t)a * q + b] * W[(size_t)i * q + b];
                        M1[(size_t)i * q + a] += s;
                    }
                }
            for (a = 0; a < q; a++)
                for (b = 0; b < q; b++) X2sum[(size_t)a * q + b] += X2[(size_t)a * q + b];
            suu += e2 + vartr + mwm2;
            if (Mmat) for (a = 0; a < q; a++) Mmat[(size_t)n * q + a] = m[a];
        }
        quad = (e2 - gm) / sigma2;
        /* ln|C_OO| = 2*no*ln(sigma) + ln|K| = no*ln(sigma2) + ln|K| */
        L += -0.5 * ((gc_real)no * log(twopi)
                   + ((gc_real)no * log(sigma2) + ld) + quad);
    }
    if (M1) *out_suu = suu;
    *ok = 1;

done:
    free(K); free(Kinv); free(Kc); free(g); free(m); free(X2); free(obs);
    return L;
}

gc_status gs_ppca_fit_missing(const gc_real *data, int rows, int cols, int q,
                              const unsigned char *mask, int iters, gc_real tol,
                              gs_ppca_result *out)
{
    gc_real *filled = NULL, *cm = NULL, *cc = NULL, *sumt = NULL;
    gc_real *M1 = NULL, *X2sum = NULL, *X2inv = NULL, *Wnew = NULL, *Mmat = NULL;
    gc_real L = 0.0, Lprev = 0.0, suu = 0.0, s2min, gvar = 0.0;
    int i, n, a, b, it, ok, ret = GC_OK;
    if (!data || !out || !mask || rows <= 1 || cols <= 1 || q < 0 || q > cols - 1 || iters < 1)
        return GC_ERR_ARG;
    memset(out, 0, sizeof(*out));

    cm = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    cc = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    sumt = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    if (!cm || !cc || !sumt) { free(cm); free(cc); free(sumt); return GC_ERR_NO_MEM; }
    for (n = 0; n < rows; n++)
        for (i = 0; i < cols; i++)
            if (mask[(size_t)n * cols + i]) {
                cm[i] += data[(size_t)n * cols + i];
                sumt[i] += data[(size_t)n * cols + i];
                cc[i] += 1.0;
            }
    for (i = 0; i < cols; i++) if (cc[i] > 0.0) cm[i] /= cc[i];

    if (q == 0) {
        const gc_real twopi = 6.28318530717958647692;
        gc_real ss = 0.0, nobs = 0.0;
        for (n = 0; n < rows; n++)
            for (i = 0; i < cols; i++)
                if (mask[(size_t)n * cols + i]) {
                    gc_real e = data[(size_t)n * cols + i] - cm[i];
                    ss += e * e;
                    nobs += 1.0;
                }
        out->mean = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
        if (!out->mean) { free(cm); free(cc); free(sumt); return GC_ERR_NO_MEM; }
        for (i = 0; i < cols; i++) out->mean[i] = cm[i];
        out->n_features = cols;
        out->n_components = 0;
        out->sigma2 = nobs > 0.0 ? ss / nobs : 0.0;
        out->dof = 1;
        if (out->sigma2 > 0.0) {
            gc_real L0 = 0.0;
            for (n = 0; n < rows; n++)
                for (i = 0; i < cols; i++)
                    if (mask[(size_t)n * cols + i]) {
                        gc_real e = data[(size_t)n * cols + i] - cm[i];
                        L0 += -0.5 * (log(twopi) + log(out->sigma2) + e * e / out->sigma2);
                    }
            out->loglik = L0;
        }
        free(cm); free(cc); free(sumt);
        return GC_OK;
    }

    /* varianza osservata (pavimento) */
    {
        gc_real ss = 0.0, nobs = 0.0;
        for (n = 0; n < rows; n++)
            for (i = 0; i < cols; i++)
                if (mask[(size_t)n * cols + i]) {
                    gc_real e = data[(size_t)n * cols + i] - cm[i];
                    ss += e * e;
                    nobs += 1.0;
                }
        gvar = nobs > 0.0 ? ss / nobs : 0.0;
    }
    s2min = gvar > 0.0 ? 1e-12 * gvar : 1e-300;

    filled = (gc_real *)malloc((size_t)rows * cols * sizeof(gc_real));
    if (!filled) { ret = GC_ERR_NO_MEM; goto cleanup; }
    for (n = 0; n < rows; n++)
        for (i = 0; i < cols; i++)
            filled[(size_t)n * cols + i] = mask[(size_t)n * cols + i]
                ? data[(size_t)n * cols + i] : cm[i];
    if (gs_ppca_fit(filled, rows, cols, q, out) != GC_OK) { ret = GC_ERR_NO_MEM; goto cleanup; }
    for (i = 0; i < cols; i++) out->mean[i] = cm[i];

    M1 = (gc_real *)calloc((size_t)cols * q, sizeof(gc_real));
    X2sum = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    X2inv = (gc_real *)calloc((size_t)q * q, sizeof(gc_real));
    Wnew = (gc_real *)calloc((size_t)cols * q, sizeof(gc_real));
    Mmat = (gc_real *)calloc((size_t)rows * q, sizeof(gc_real));
    if (!M1 || !X2sum || !X2inv || !Wnew || !Mmat) { ret = GC_ERR_NO_MEM; goto cleanup; }

    L = ppca_miss_pass(data, rows, cols, out->mean, out->W, q, out->sigma2,
                       mask, NULL, NULL, NULL, NULL, &ok);
    if (!ok) { ret = GC_ERR_ARG; goto cleanup; }

    for (it = 0; it < iters; it++) {
        memset(M1, 0, (size_t)cols * q * sizeof(gc_real));
        memset(X2sum, 0, (size_t)q * q * sizeof(gc_real));
        suu = 0.0;
        L = ppca_miss_pass(data, rows, cols, out->mean, out->W, q, out->sigma2,
                           mask, M1, X2sum, &suu, Mmat, &ok);
        if (!ok) { ret = GC_ERR_ARG; goto cleanup; }
        if (it > 0) {
            gc_real diff = L - Lprev;
            if (diff < 0.0) diff = -diff;
            if (diff < tol) break;
        }
        Lprev = L;
        if (gm_linalg_inverse(q, X2sum, X2inv) != GM_OK) { ret = GC_ERR_ARG; goto cleanup; }
        for (i = 0; i < cols; i++)
            for (a = 0; a < q; a++) {
                gc_real s = 0.0;
                for (b = 0; b < q; b++) s += M1[(size_t)i * q + b] * X2inv[(size_t)b * q + a];
                Wnew[(size_t)i * q + a] = s;
            }
        {
            gc_real cross = 0.0, trWX2 = 0.0, s2;
            for (i = 0; i < cols; i++)
                for (a = 0; a < q; a++) cross += M1[(size_t)i * q + a] * Wnew[(size_t)i * q + a];
            for (a = 0; a < q; a++)
                for (b = 0; b < q; b++) {
                    gc_real s = 0.0;
                    for (i = 0; i < cols; i++)
                        s += Wnew[(size_t)i * q + a] * Wnew[(size_t)i * q + b];
                    trWX2 += s * X2sum[(size_t)b * q + a];
                }
            s2 = (suu - 2.0 * cross + trWX2) / ((gc_real)rows * (gc_real)cols);
            if (s2 < s2min) s2 = s2min;
            memcpy(out->W, Wnew, (size_t)cols * q * sizeof(gc_real));
            out->sigma2 = s2;
        }
        /* mu_i = media, sui campioni osservati, di t_ni - (W_new m_n)_i */
        for (i = 0; i < cols; i++) {
            if (cc[i] > 1e-9) {
                gc_real num = sumt[i];
                for (n = 0; n < rows; n++)
                    if (mask[(size_t)n * cols + i]) {
                        gc_real s = 0.0;
                        for (a = 0; a < q; a++)
                            s += Wnew[(size_t)i * q + a] * Mmat[(size_t)n * q + a];
                        num -= s;
                    }
                out->mean[i] = num / cc[i];
            }
        }
    }

    L = ppca_miss_pass(data, rows, cols, out->mean, out->W, q, out->sigma2,
                       mask, NULL, NULL, NULL, NULL, &ok);
    if (!ok) { ret = GC_ERR_ARG; goto cleanup; }
    out->loglik = L;

cleanup:
    free(filled); free(cm); free(cc); free(sumt);
    free(M1); free(X2sum); free(X2inv); free(Wnew); free(Mmat);
    if (ret != GC_OK) gs_ppca_free(out);
    return (gc_status)ret;
}
