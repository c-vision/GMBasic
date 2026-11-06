/*
 * ofx_mv2.c — seconda slice MULTIVARIATE: KMeans, PCA, GMM 1D, GP, BONEXT,
 * SVM fit/predict. Risultati composti in out() piatto documentato.
 */
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#include "gs_sim.h"
#include "gs_num.h"
#include "gs_pca.h"
#include "gs_boost.h"

/* KMeans 1D: out = centri(k) seguiti dalle assegnazioni(n). */
int ofx_kmeans(const double *x, int n, int k, int max_iter, double *out)
{
    int *assign;
    int i;
    if (!x || !out || n < k || k < 1) return 0;
    assign = (int *)malloc((size_t)n * sizeof(int));
    if (!assign) return 0;
    if (gs_kmeans(x, n, 1, k, max_iter, out, assign) != 0) { free(assign); return 0; }
    for (i = 0; i < n; i++) out[k + i] = (double)assign[i];
    free(assign);
    return 1;
}

/* PCA: out = autovalori(cols) seguiti dagli autovettori(cols*cols, row-major). */
int ofx_pca(const double *data, int rows, int cols, double *out)
{
    gs_pca_result r;
    int i;
    if (!data || !out || rows < 1 || cols < 1) return 0;
    if (gs_pca(data, rows, cols, &r) != GC_OK) return 0;
    for (i = 0; i < cols; i++) out[i] = r.eigenvalues[i];
    for (i = 0; i < cols * cols; i++) out[cols + i] = r.eigenvectors[i];
    gs_pca_free(&r);
    return 1;
}

/* GMM 1D: out = mu(k), var(k), weight(k). */
int ofx_gmm_1d(const double *x, int n, int k, int iters, double *out)
{
    if (!x || !out || n < 2 || k < 1) return 0;
    return gs_gmm_em_1d(x, n, k, iters, out, out + k, out + 2 * k) == 0 ? 1 : 0;
}

/* GP: out = mean(m) + var(m). */
int ofx_gp(const double *x, const double *y, int n, const double *xs, int m,
           double l2, double noise, double *out)
{
    if (!x || !y || !xs || !out || n < 1 || m < 1) return 0;
    return gs_gp_regression(x, y, n, xs, m, l2, noise, out, out + m) == 0 ? 1 : 0;
}

/* BONEXT: un passo di Bayesian optimization (UCB). out = {index, value}. */
int ofx_bo_next(const double *x, const double *y, const double *cand, int n, int m,
                const double *hyper, double *out)
{
    double *mean, *var, l2, noise, kappa, bv = -1e300;
    int i, best = -1;
    if (!x || !y || !cand || !hyper || !out || n < 2 || m < 1) return 0;
    l2 = hyper[0];
    noise = hyper[1];
    kappa = hyper[2];
    mean = (double *)malloc((size_t)m * sizeof(double));
    var = (double *)malloc((size_t)m * sizeof(double));
    if (!mean || !var) { free(mean); free(var); return 0; }
    if (gs_gp_regression(x, y, n, cand, m, l2, noise, mean, var) != 0) {
        free(mean); free(var);
        return 0;
    }
    for (i = 0; i < m; i++) {
        double v = var[i] > 0.0 ? var[i] : 0.0;
        double u = mean[i] + kappa * sqrt(v);
        if (u > bv) { bv = u; best = i; }
    }
    out[0] = (double)best;
    out[1] = bv;
    free(mean);
    free(var);
    return 1;
}

/* SVM multiclasse: out = W(k*d). */
int ofx_svm_fit(const double *X, const double *y, int n, int d, int k,
                double C, int max_iter, double *out)
{
    if (!X || !y || !out || n < 1 || d < 1 || k < 2) return 0;
    return gs_svm_multiclass(X, y, n, d, k, C, max_iter, out) == 0 ? 1 : 0;
}

int ofx_svm_predict(const double *W, const double *x, int d, int k)
{
    int label = 0;
    if (!W || !x || d < 1 || k < 1) return 0;
    gs_svm_predict(W, x, d, k, &label);
    return label;
}

/* ── gradient boosting: modello opaco (handle LONG) ─────────────────────── */
long long ofx_boost_fit(const double *X, const double *y, int n, int d,
                        int engine, int n_estimators, double lr, int max_depth)
{
    gs_boost_model *m;
    if (!X || !y || n < 1 || d < 1) return 0;
    m = gs_boost_new((gs_boost_engine)engine, n_estimators, lr, max_depth);
    if (!m) return 0;
    if (gs_boost_fit(m, X, y, n, d) != 0) { gs_boost_free(m); return 0; }
    return (long long)(intptr_t)m;
}

double ofx_boost_predict(long long handle, const double *x, int d)
{
    if (!handle || !x || d < 1) return 0.0 / 0.0;
    return gs_boost_predict((const gs_boost_model *)(intptr_t)handle, x);
}

double ofx_boost_score(long long handle, const double *X, const double *y, int n, int d)
{
    if (!handle || !X || !y || n < 1 || d < 1) return 0.0 / 0.0;
    return gs_boost_score((const gs_boost_model *)(intptr_t)handle, X, y, n, d);
}

void ofx_boost_free(long long handle)
{
    if (handle) gs_boost_free((gs_boost_model *)(intptr_t)handle);
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
int ofx_bo_next__ffi(const double *x, int64_t x_ext, const double *y, int64_t y_ext, const double *cand, int64_t cand_ext, int n, int m, const double *hyper, int64_t hyper_ext, double *out, int64_t out_ext) { return ofx_bo_next(x, y, cand, n, m, hyper, out); }
long long ofx_boost_fit__ffi(const double *X, int64_t X_ext, const double *y, int64_t y_ext, int n, int d, int engine, int n_estimators, double lr, int max_depth) { return ofx_boost_fit(X, y, n, d, engine, n_estimators, lr, max_depth); }
double ofx_boost_predict__ffi(long long handle, const double *x, int64_t x_ext, int d) { return ofx_boost_predict(handle, x, d); }
double ofx_boost_score__ffi(long long handle, const double *X, int64_t X_ext, const double *y, int64_t y_ext, int n, int d) { return ofx_boost_score(handle, X, y, n, d); }
int ofx_gmm_1d__ffi(const double *x, int64_t x_ext, int n, int k, int iters, double *out, int64_t out_ext) { return ofx_gmm_1d(x, n, k, iters, out); }
int ofx_gp__ffi(const double *x, int64_t x_ext, const double *y, int64_t y_ext, int n, const double *xs, int64_t xs_ext, int m, double l2, double noise, double *out, int64_t out_ext) { return ofx_gp(x, y, n, xs, m, l2, noise, out); }
int ofx_pca__ffi(const double *data, int64_t data_ext, int rows, int cols, double *out, int64_t out_ext) { return ofx_pca(data, rows, cols, out); }
int ofx_svm_fit__ffi(const double *X, int64_t X_ext, const double *y, int64_t y_ext, int n, int d, int k, double C, int max_iter, double *out, int64_t out_ext) { return ofx_svm_fit(X, y, n, d, k, C, max_iter, out); }
int ofx_svm_predict__ffi(const double *W, int64_t W_ext, const double *x, int64_t x_ext, int d, int k) { return ofx_svm_predict(W, x, d, k); }
