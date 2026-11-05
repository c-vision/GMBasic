/* gc_blas2.c — BLAS di livello 2 (da stdlib-js blas-base). */
#include "gc_blas2.h"

#include <stdlib.h>

void gc_blas_sgemv(int m, int n, float alpha, const float *A, int lda,
                   const float *x, int incx, float beta, float *y, int incy)
{
    int i, j;
    if (m <= 0 || n <= 0 || !A || !x || !y) return;
    for (i = 0; i < m; i++) {
        float s = 0.0f;
        if (alpha != 0.0f)
            for (j = 0; j < n; j++) s += A[(size_t)i * lda + j] * x[(size_t)j * incx];
        y[(size_t)i * incy] = (beta == 0.0f) ? alpha * s : beta * y[(size_t)i * incy] + alpha * s;
    }
}

void gc_blas_dgemv(int m, int n, double alpha, const double *A, int lda,
                   const double *x, int incx, double beta, double *y, int incy)
{
    int i, j;
    if (m <= 0 || n <= 0 || !A || !x || !y) return;
    for (i = 0; i < m; i++) {
        double s = 0.0;
        if (alpha != 0.0)
            for (j = 0; j < n; j++) s += A[(size_t)i * lda + j] * x[(size_t)j * incx];
        y[(size_t)i * incy] = (beta == 0.0) ? alpha * s : beta * y[(size_t)i * incy] + alpha * s;
    }
}

void gc_blas_sger(int m, int n, float alpha, const float *x, int incx,
                  const float *y, int incy, float *A, int lda)
{
    int i, j;
    if (m <= 0 || n <= 0 || !A || !x || !y || alpha == 0.0f) return;
    for (i = 0; i < m; i++) {
        float xi = alpha * x[(size_t)i * incx];
        for (j = 0; j < n; j++) A[(size_t)i * lda + j] += xi * y[(size_t)j * incy];
    }
}

void gc_blas_dger(int m, int n, double alpha, const double *x, int incx,
                  const double *y, int incy, double *A, int lda)
{
    int i, j;
    if (m <= 0 || n <= 0 || !A || !x || !y || alpha == 0.0) return;
    for (i = 0; i < m; i++) {
        double xi = alpha * x[(size_t)i * incx];
        for (j = 0; j < n; j++) A[(size_t)i * lda + j] += xi * y[(size_t)j * incy];
    }
}

/* ---- triangolari (A n×n) ---- */

void gc_blas_dtrmv(int n, int upper, int trans, int unit, const double *A, int lda,
                   double *x, int incx)
{
    double *buf;
    int i, j;
    if (n <= 0 || !A || !x) return;
    buf = (double *)malloc((size_t)n * sizeof(double));
    if (!buf) return;
    for (i = 0; i < n; i++) buf[i] = x[(size_t)i * incx];
    for (i = 0; i < n; i++) {
        double s = 0.0;
        int diag = unit ? 1 : 0;
        if (!trans) {
            if (upper) { for (j = i + 1; j < n; j++) s += A[(size_t)i * lda + j] * buf[j]; }
            else { for (j = 0; j < i; j++) s += A[(size_t)i * lda + j] * buf[j]; }
            if (!diag) s += A[(size_t)i * lda + i] * buf[i];
        } else {
            if (upper) { for (j = 0; j < i; j++) s += A[(size_t)j * lda + i] * buf[j]; }
            else { for (j = i + 1; j < n; j++) s += A[(size_t)j * lda + i] * buf[j]; }
            if (!diag) s += A[(size_t)i * lda + i] * buf[i];
        }
        x[(size_t)i * incx] = s;
    }
    free(buf);
}

void gc_blas_strmv(int n, int upper, int trans, int unit, const float *A, int lda,
                   float *x, int incx)
{
    float *buf;
    int i, j;
    if (n <= 0 || !A || !x) return;
    buf = (float *)malloc((size_t)n * sizeof(float));
    if (!buf) return;
    for (i = 0; i < n; i++) buf[i] = x[(size_t)i * incx];
    for (i = 0; i < n; i++) {
        float s = 0.0f;
        int diag = unit ? 1 : 0;
        if (!trans) {
            if (upper) { for (j = i + 1; j < n; j++) s += A[(size_t)i * lda + j] * buf[j]; }
            else { for (j = 0; j < i; j++) s += A[(size_t)i * lda + j] * buf[j]; }
            if (!diag) s += A[(size_t)i * lda + i] * buf[i];
        } else {
            if (upper) { for (j = 0; j < i; j++) s += A[(size_t)j * lda + i] * buf[j]; }
            else { for (j = i + 1; j < n; j++) s += A[(size_t)j * lda + i] * buf[j]; }
            if (!diag) s += A[(size_t)i * lda + i] * buf[i];
        }
        x[(size_t)i * incx] = s;
    }
    free(buf);
}

void gc_blas_dtrsv(int n, int upper, int trans, int unit, const double *A, int lda,
                   double *x, int incx)
{
    int i, j;
    if (n <= 0 || !A || !x) return;
    if (!trans) {
        /* op = A: forward se lower, backward se upper */
        if (upper) {
            for (i = n - 1; i >= 0; i--) {
                double s = x[(size_t)i * incx];
                for (j = i + 1; j < n; j++) s -= A[(size_t)i * lda + j] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        } else {
            for (i = 0; i < n; i++) {
                double s = x[(size_t)i * incx];
                for (j = 0; j < i; j++) s -= A[(size_t)i * lda + j] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        }
    } else {
        /* op = A^T */
        if (upper) {
            for (i = 0; i < n; i++) {
                double s = x[(size_t)i * incx];
                for (j = 0; j < i; j++) s -= A[(size_t)j * lda + i] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        } else {
            for (i = n - 1; i >= 0; i--) {
                double s = x[(size_t)i * incx];
                for (j = i + 1; j < n; j++) s -= A[(size_t)j * lda + i] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        }
    }
}

void gc_blas_strsv(int n, int upper, int trans, int unit, const float *A, int lda,
                   float *x, int incx)
{
    int i, j;
    if (n <= 0 || !A || !x) return;
    if (!trans) {
        if (upper) {
            for (i = n - 1; i >= 0; i--) {
                float s = x[(size_t)i * incx];
                for (j = i + 1; j < n; j++) s -= A[(size_t)i * lda + j] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        } else {
            for (i = 0; i < n; i++) {
                float s = x[(size_t)i * incx];
                for (j = 0; j < i; j++) s -= A[(size_t)i * lda + j] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        }
    } else {
        if (upper) {
            for (i = 0; i < n; i++) {
                float s = x[(size_t)i * incx];
                for (j = 0; j < i; j++) s -= A[(size_t)j * lda + i] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        } else {
            for (i = n - 1; i >= 0; i--) {
                float s = x[(size_t)i * incx];
                for (j = i + 1; j < n; j++) s -= A[(size_t)j * lda + i] * x[(size_t)j * incx];
                if (!unit) s /= A[(size_t)i * lda + i];
                x[(size_t)i * incx] = s;
            }
        }
    }
}

/* ---- simmetriche (A n×n, solo triangolo `upper`) ---- */

void gc_blas_dsymv(int n, int upper, double alpha, const double *A, int lda,
                   const double *x, int incx, double beta, double *y, int incy)
{
    int i, j;
    if (n <= 0 || !A || !x || !y) return;
    for (i = 0; i < n; i++) {
        double s = 0.0;
        if (alpha != 0.0) {
            if (upper) {
                for (j = i; j < n; j++) s += A[(size_t)i * lda + j] * x[(size_t)j * incx];
                for (j = 0; j < i; j++) s += A[(size_t)j * lda + i] * x[(size_t)j * incx];
            } else {
                for (j = 0; j <= i; j++) s += A[(size_t)i * lda + j] * x[(size_t)j * incx];
                for (j = i + 1; j < n; j++) s += A[(size_t)j * lda + i] * x[(size_t)j * incx];
            }
        }
        y[(size_t)i * incy] = (beta == 0.0) ? alpha * s : beta * y[(size_t)i * incy] + alpha * s;
    }
}

void gc_blas_ssymv(int n, int upper, float alpha, const float *A, int lda,
                   const float *x, int incx, float beta, float *y, int incy)
{
    int i, j;
    if (n <= 0 || !A || !x || !y) return;
    for (i = 0; i < n; i++) {
        float s = 0.0f;
        if (alpha != 0.0f) {
            if (upper) {
                for (j = i; j < n; j++) s += A[(size_t)i * lda + j] * x[(size_t)j * incx];
                for (j = 0; j < i; j++) s += A[(size_t)j * lda + i] * x[(size_t)j * incx];
            } else {
                for (j = 0; j <= i; j++) s += A[(size_t)i * lda + j] * x[(size_t)j * incx];
                for (j = i + 1; j < n; j++) s += A[(size_t)j * lda + i] * x[(size_t)j * incx];
            }
        }
        y[(size_t)i * incy] = (beta == 0.0f) ? alpha * s : beta * y[(size_t)i * incy] + alpha * s;
    }
}

void gc_blas_dsyr(int n, int upper, double alpha, const double *x, int incx, double *A, int lda)
{
    int i, j;
    if (n <= 0 || !A || !x || alpha == 0.0) return;
    if (upper) {
        for (i = 0; i < n; i++)
            for (j = i; j < n; j++) A[(size_t)i * lda + j] += alpha * x[(size_t)i * incx] * x[(size_t)j * incx];
    } else {
        for (i = 0; i < n; i++)
            for (j = 0; j <= i; j++) A[(size_t)i * lda + j] += alpha * x[(size_t)i * incx] * x[(size_t)j * incx];
    }
}

void gc_blas_ssyr(int n, int upper, float alpha, const float *x, int incx, float *A, int lda)
{
    int i, j;
    if (n <= 0 || !A || !x || alpha == 0.0f) return;
    if (upper) {
        for (i = 0; i < n; i++)
            for (j = i; j < n; j++) A[(size_t)i * lda + j] += alpha * x[(size_t)i * incx] * x[(size_t)j * incx];
    } else {
        for (i = 0; i < n; i++)
            for (j = 0; j <= i; j++) A[(size_t)i * lda + j] += alpha * x[(size_t)i * incx] * x[(size_t)j * incx];
    }
}

void gc_blas_dsyr2(int n, int upper, double alpha, const double *x, int incx,
                   const double *y, int incy, double *A, int lda)
{
    int i, j;
    if (n <= 0 || !A || !x || !y || alpha == 0.0) return;
    if (upper) {
        for (i = 0; i < n; i++)
            for (j = i; j < n; j++)
                A[(size_t)i * lda + j] += alpha * (x[(size_t)i * incx] * y[(size_t)j * incy]
                                                 + y[(size_t)i * incy] * x[(size_t)j * incx]);
    } else {
        for (i = 0; i < n; i++)
            for (j = 0; j <= i; j++)
                A[(size_t)i * lda + j] += alpha * (x[(size_t)i * incx] * y[(size_t)j * incy]
                                                 + y[(size_t)i * incy] * x[(size_t)j * incx]);
    }
}

void gc_blas_ssyr2(int n, int upper, float alpha, const float *x, int incx,
                   const float *y, int incy, float *A, int lda)
{
    int i, j;
    if (n <= 0 || !A || !x || !y || alpha == 0.0f) return;
    if (upper) {
        for (i = 0; i < n; i++)
            for (j = i; j < n; j++)
                A[(size_t)i * lda + j] += alpha * (x[(size_t)i * incx] * y[(size_t)j * incy]
                                                 + y[(size_t)i * incy] * x[(size_t)j * incx]);
    } else {
        for (i = 0; i < n; i++)
            for (j = 0; j <= i; j++)
                A[(size_t)i * lda + j] += alpha * (x[(size_t)i * incx] * y[(size_t)j * incy]
                                                 + y[(size_t)i * incy] * x[(size_t)j * incx]);
    }
}
