/* gc_blas3.c — BLAS di livello 3 (da MuBLIS). */
#include "gc_blas3.h"

#include <stdlib.h>

/* accesso simmetrico: A[i][p] con solo il triangolo `uplo` memorizzato. */
#define SYM(A, lda, i, p, uplo) ((uplo) ? (((p) >= (i)) ? (A)[(size_t)(i)*(lda)+(p)] : (A)[(size_t)(p)*(lda)+(i)]) \
                                        : (((p) <= (i)) ? (A)[(size_t)(i)*(lda)+(p)] : (A)[(size_t)(p)*(lda)+(i)]))

/* ---- SYMM ---- */
void gc_blas_dsymm(int m, int n, double alpha, const double *A, int lda,
                   const double *B, int ldb, double beta, double *C, int ldc,
                   int side, int uplo)
{
    int i, j, p;
    if (m <= 0 || n <= 0 || !A || !B || !C) return;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) {
            double s = 0.0;
            if (alpha != 0.0) {
                if (side == 0)
                    for (p = 0; p < m; p++) s += SYM(A, lda, i, p, uplo) * B[(size_t)p * ldb + j];
                else
                    for (p = 0; p < n; p++) s += B[(size_t)i * ldb + p] * SYM(A, lda, p, j, uplo);
            }
            C[(size_t)i * ldc + j] = (beta == 0.0) ? alpha * s
                                                   : beta * C[(size_t)i * ldc + j] + alpha * s;
        }
}

void gc_blas_ssymm(int m, int n, float alpha, const float *A, int lda,
                   const float *B, int ldb, float beta, float *C, int ldc,
                   int side, int uplo)
{
    int i, j, p;
    if (m <= 0 || n <= 0 || !A || !B || !C) return;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) {
            float s = 0.0f;
            if (alpha != 0.0f) {
                if (side == 0)
                    for (p = 0; p < m; p++) s += SYM(A, lda, i, p, uplo) * B[(size_t)p * ldb + j];
                else
                    for (p = 0; p < n; p++) s += B[(size_t)i * ldb + p] * SYM(A, lda, p, j, uplo);
            }
            C[(size_t)i * ldc + j] = (beta == 0.0f) ? alpha * s
                                                    : beta * C[(size_t)i * ldc + j] + alpha * s;
        }
}

/* ---- SYRK ---- */
void gc_blas_dsyrk(int n, int k, double alpha, const double *A, int lda,
                   double beta, double *C, int ldc, int uplo, int trans)
{
    int i, j, p;
    if (n <= 0 || !A || !C) return;
    for (i = 0; i < n; i++) {
        int j0 = (uplo == 1) ? i : 0;
        int j1 = (uplo == 1) ? n : i + 1;
        for (j = j0; j < j1; j++) {
            double s = 0.0;
            if (k > 0 && alpha != 0.0) {
                if (trans == 0)
                    for (p = 0; p < k; p++) s += A[(size_t)i * lda + p] * A[(size_t)j * lda + p];
                else
                    for (p = 0; p < k; p++) s += A[(size_t)p * lda + i] * A[(size_t)p * lda + j];
            }
            C[(size_t)i * ldc + j] = (beta == 0.0) ? alpha * s
                                                   : beta * C[(size_t)i * ldc + j] + alpha * s;
        }
    }
}

void gc_blas_ssyrk(int n, int k, float alpha, const float *A, int lda,
                   float beta, float *C, int ldc, int uplo, int trans)
{
    int i, j, p;
    if (n <= 0 || !A || !C) return;
    for (i = 0; i < n; i++) {
        int j0 = (uplo == 1) ? i : 0;
        int j1 = (uplo == 1) ? n : i + 1;
        for (j = j0; j < j1; j++) {
            float s = 0.0f;
            if (k > 0 && alpha != 0.0f) {
                if (trans == 0)
                    for (p = 0; p < k; p++) s += A[(size_t)i * lda + p] * A[(size_t)j * lda + p];
                else
                    for (p = 0; p < k; p++) s += A[(size_t)p * lda + i] * A[(size_t)p * lda + j];
            }
            C[(size_t)i * ldc + j] = (beta == 0.0f) ? alpha * s
                                                    : beta * C[(size_t)i * ldc + j] + alpha * s;
        }
    }
}

/* ---- SYR2K ---- */
void gc_blas_dsyr2k(int n, int k, double alpha, const double *A, int lda,
                    const double *B, int ldb, double beta, double *C, int ldc,
                    int uplo, int trans)
{
    int i, j, p;
    if (n <= 0 || !A || !B || !C) return;
    for (i = 0; i < n; i++) {
        int j0 = (uplo == 1) ? i : 0;
        int j1 = (uplo == 1) ? n : i + 1;
        for (j = j0; j < j1; j++) {
            double s = 0.0;
            if (k > 0 && alpha != 0.0) {
                if (trans == 0)
                    for (p = 0; p < k; p++)
                        s += A[(size_t)i * lda + p] * B[(size_t)j * ldb + p]
                           + B[(size_t)i * ldb + p] * A[(size_t)j * lda + p];
                else
                    for (p = 0; p < k; p++)
                        s += A[(size_t)p * lda + i] * B[(size_t)p * ldb + j]
                           + B[(size_t)p * ldb + i] * A[(size_t)p * lda + j];
            }
            C[(size_t)i * ldc + j] = (beta == 0.0) ? alpha * s
                                                   : beta * C[(size_t)i * ldc + j] + alpha * s;
        }
    }
}

void gc_blas_ssyr2k(int n, int k, float alpha, const float *A, int lda,
                    const float *B, int ldb, float beta, float *C, int ldc,
                    int uplo, int trans)
{
    int i, j, p;
    if (n <= 0 || !A || !B || !C) return;
    for (i = 0; i < n; i++) {
        int j0 = (uplo == 1) ? i : 0;
        int j1 = (uplo == 1) ? n : i + 1;
        for (j = j0; j < j1; j++) {
            float s = 0.0f;
            if (k > 0 && alpha != 0.0f) {
                if (trans == 0)
                    for (p = 0; p < k; p++)
                        s += A[(size_t)i * lda + p] * B[(size_t)j * ldb + p]
                           + B[(size_t)i * ldb + p] * A[(size_t)j * lda + p];
                else
                    for (p = 0; p < k; p++)
                        s += A[(size_t)p * lda + i] * B[(size_t)p * ldb + j]
                           + B[(size_t)p * ldb + i] * A[(size_t)p * lda + j];
            }
            C[(size_t)i * ldc + j] = (beta == 0.0f) ? alpha * s
                                                    : beta * C[(size_t)i * ldc + j] + alpha * s;
        }
    }
}

/* valore di una matrice triangolare (con opzionale trasposizione). */
static double tri_d(const double *A, int lda, int i, int j, int uplo, int trans, int diag)
{
    if (diag && i == j) return 1.0;
    if (trans) return (uplo ? (i <= j) : (i >= j)) ? A[(size_t)j * lda + i] : 0.0;
    return (uplo ? (i <= j) : (i >= j)) ? A[(size_t)i * lda + j] : 0.0;
}
static float tri_s(const float *A, int lda, int i, int j, int uplo, int trans, int diag)
{
    if (diag && i == j) return 1.0f;
    if (trans) return (uplo ? (i <= j) : (i >= j)) ? A[(size_t)j * lda + i] : 0.0f;
    return (uplo ? (i <= j) : (i >= j)) ? A[(size_t)i * lda + j] : 0.0f;
}

/* ---- TRMM: B = alpha*op(A)*B (left) o alpha*B*op(A) (right) ---- */
void gc_blas_dtrmm(int m, int n, double alpha, const double *A, int lda,
                   double *B, int ldb, int side, int uplo, int trans, int diag)
{
    double *tmp;
    int i, j, p;
    if (m <= 0 || n <= 0 || !A || !B) return;
    tmp = (double *)malloc((size_t)m * n * sizeof(double));
    if (!tmp) return;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) {
            double s = 0.0;
            if (side == 0) {
                for (p = 0; p < m; p++) s += tri_d(A, lda, i, p, uplo, trans, diag) * B[(size_t)p * ldb + j];
            } else {
                for (p = 0; p < n; p++) s += B[(size_t)i * ldb + p] * tri_d(A, lda, p, j, uplo, trans, diag);
            }
            tmp[(size_t)i * n + j] = alpha * s;
        }
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) B[(size_t)i * ldb + j] = tmp[(size_t)i * n + j];
    free(tmp);
}

void gc_blas_strmm(int m, int n, float alpha, const float *A, int lda,
                   float *B, int ldb, int side, int uplo, int trans, int diag)
{
    float *tmp;
    int i, j, p;
    if (m <= 0 || n <= 0 || !A || !B) return;
    tmp = (float *)malloc((size_t)m * n * sizeof(float));
    if (!tmp) return;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) {
            float s = 0.0f;
            if (side == 0) {
                for (p = 0; p < m; p++) s += (float)tri_s(A, lda, i, p, uplo, trans, diag) * B[(size_t)p * ldb + j];
            } else {
                for (p = 0; p < n; p++) s += B[(size_t)i * ldb + p] * (float)tri_s(A, lda, p, j, uplo, trans, diag);
            }
            tmp[(size_t)i * n + j] = alpha * s;
        }
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) B[(size_t)i * ldb + j] = tmp[(size_t)i * n + j];
    free(tmp);
}

/* ---- TRSM: risolve op(A)*X = alpha*B (left) o X*op(A) = alpha*B (right) ---- */
void gc_blas_dtrsm(int m, int n, double alpha, const double *A, int lda,
                   double *B, int ldb, int side, int uplo, int trans, int diag)
{
    int i, j, p;
    if (m <= 0 || n <= 0 || !A || !B) return;
    if (side == 0) {
        /* A (m×m) * X = alpha*B */
        for (j = 0; j < n; j++) {
            /* applica alpha */
            for (i = 0; i < m; i++) B[(size_t)i * ldb + j] *= alpha;
            /* sostituzione: se (trans==0 e uplo==lower) o (trans==1 e uplo==upper) -> forward */
            int forward = (!trans && !uplo) || (trans && uplo);
            if (forward) {
                for (i = 0; i < m; i++) {
                    double s = B[(size_t)i * ldb + j];
                    for (p = 0; p < i; p++) s -= tri_d(A, lda, i, p, uplo, trans, 0) * B[(size_t)p * ldb + j];
                    if (!diag) s /= tri_d(A, lda, i, i, uplo, trans, 0);
                    B[(size_t)i * ldb + j] = s;
                }
            } else {
                for (i = m - 1; i >= 0; i--) {
                    double s = B[(size_t)i * ldb + j];
                    for (p = i + 1; p < m; p++) s -= tri_d(A, lda, i, p, uplo, trans, 0) * B[(size_t)p * ldb + j];
                    if (!diag) s /= tri_d(A, lda, i, i, uplo, trans, 0);
                    B[(size_t)i * ldb + j] = s;
                }
            }
        }
    } else {
        /* X * A (n×n) = alpha*B */
        for (i = 0; i < m; i++) {
            for (j = 0; j < n; j++) B[(size_t)i * ldb + j] *= alpha;
            int forward = (!trans && uplo) || (trans && !uplo);
            if (forward) {
                for (j = 0; j < n; j++) {
                    double s = B[(size_t)i * ldb + j];
                    for (p = 0; p < j; p++) s -= B[(size_t)i * ldb + p] * tri_d(A, lda, p, j, uplo, trans, 0);
                    if (!diag) s /= tri_d(A, lda, j, j, uplo, trans, 0);
                    B[(size_t)i * ldb + j] = s;
                }
            } else {
                for (j = n - 1; j >= 0; j--) {
                    double s = B[(size_t)i * ldb + j];
                    for (p = j + 1; p < n; p++) s -= B[(size_t)i * ldb + p] * tri_d(A, lda, p, j, uplo, trans, 0);
                    if (!diag) s /= tri_d(A, lda, j, j, uplo, trans, 0);
                    B[(size_t)i * ldb + j] = s;
                }
            }
        }
    }
}

void gc_blas_strsm(int m, int n, float alpha, const float *A, int lda,
                   float *B, int ldb, int side, int uplo, int trans, int diag)
{
    int i, j, p;
    if (m <= 0 || n <= 0 || !A || !B) return;
    if (side == 0) {
        for (j = 0; j < n; j++) {
            for (i = 0; i < m; i++) B[(size_t)i * ldb + j] *= alpha;
            {
                int forward = (!trans && !uplo) || (trans && uplo);
                if (forward) {
                    for (i = 0; i < m; i++) {
                        float s = B[(size_t)i * ldb + j];
                        for (p = 0; p < i; p++) s -= (float)tri_s(A, lda, i, p, uplo, trans, 0) * B[(size_t)p * ldb + j];
                        if (!diag) s /= (float)tri_s(A, lda, i, i, uplo, trans, 0);
                        B[(size_t)i * ldb + j] = s;
                    }
                } else {
                    for (i = m - 1; i >= 0; i--) {
                        float s = B[(size_t)i * ldb + j];
                        for (p = i + 1; p < m; p++) s -= (float)tri_s(A, lda, i, p, uplo, trans, 0) * B[(size_t)p * ldb + j];
                        if (!diag) s /= (float)tri_s(A, lda, i, i, uplo, trans, 0);
                        B[(size_t)i * ldb + j] = s;
                    }
                }
            }
        }
    } else {
        for (i = 0; i < m; i++) {
            for (j = 0; j < n; j++) B[(size_t)i * ldb + j] *= alpha;
            {
                int forward = (!trans && uplo) || (trans && !uplo);
                if (forward) {
                    for (j = 0; j < n; j++) {
                        float s = B[(size_t)i * ldb + j];
                        for (p = 0; p < j; p++) s -= B[(size_t)i * ldb + p] * (float)tri_s(A, lda, p, j, uplo, trans, 0);
                        if (!diag) s /= (float)tri_s(A, lda, j, j, uplo, trans, 0);
                        B[(size_t)i * ldb + j] = s;
                    }
                } else {
                    for (j = n - 1; j >= 0; j--) {
                        float s = B[(size_t)i * ldb + j];
                        for (p = j + 1; p < n; p++) s -= B[(size_t)i * ldb + p] * (float)tri_s(A, lda, p, j, uplo, trans, 0);
                        if (!diag) s /= (float)tri_s(A, lda, j, j, uplo, trans, 0);
                        B[(size_t)i * ldb + j] = s;
                    }
                }
            }
        }
    }
}
