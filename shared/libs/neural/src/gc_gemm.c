/* gc_gemm.c — GEMM/GEMV/outer product, con packing e micro-kernel register-blocked.
 *
 * Metodo GotoBLAS/FLAME: blocking a 3 livelli NC/KC/MC, packing dei pannelli di
 * A e B in buffer contigui, e micro-kernel 4×4 con accumulo su k tenuto in
 * registri (16 FMA per elemento di B riusato 4 volte).
 *
 * Tecnica di pubblico dominio (FLAME "How to optimize GEMM", GotoBLAS); nessun
 * codice copiato dal repo ajeetkbhardwaj/matrix (che non concede licenza).
 */
#include "gc_gemm.h"
#include "gc_array.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define GC_NEON 1
#endif

#define GC_GEMM_NC 256
#define GC_GEMM_KC 256
#define GC_GEMM_MC 64

/* packing di un pannello mc×kc di A (righe da ib, colonne da kb), scalato di alpha.
 * Ap[p*MC + i] = alpha*A[(ib+i)*lda + kb+p] per i<mc, 0 oltre. */
static void pack_a(int mc, int kc, gc_real alpha, const gc_real *A, int lda,
                   int ib, int kb, gc_real *Ap)
{
    int i, p;
    for (i = 0; i < mc; i++) {
        const gc_real *arow = A + (size_t)(ib + i) * (size_t)lda + kb;
        for (p = 0; p < kc; p++) Ap[(size_t)p * GC_GEMM_MC + i] = alpha * arow[p];
    }
    for (; i < GC_GEMM_MC; i++)
        for (p = 0; p < kc; p++) Ap[(size_t)p * GC_GEMM_MC + i] = 0.0;
}

/* packing di un pannello kc×nc di B (righe da kb, colonne da jb).
 * Bp[p*NC + j] = B[(kb+p)*ldb + jb+j] per j<nc, 0 oltre. */
static void pack_b(int kc, int nc, const gc_real *B, int ldb, int kb, int jb,
                   gc_real *Bp)
{
    int p, j;
    for (p = 0; p < kc; p++) {
        const gc_real *brow = B + (size_t)(kb + p) * (size_t)ldb + jb;
        for (j = 0; j < nc; j++) Bp[(size_t)p * GC_GEMM_NC + j] = brow[j];
        for (; j < GC_GEMM_NC; j++) Bp[(size_t)p * GC_GEMM_NC + j] = 0.0;
    }
}

/* micro-kernel 4×4: C[0:4][0:4] += Ap * Bp, con valid_r righe e valid_c colonne
 * effettive. Ap: stride GC_GEMM_MC; Bp: stride GC_GEMM_NC. */
static void adddot4x4(int kc, const gc_real *Ap, const gc_real *Bp,
                      gc_real *c0, gc_real *c1, gc_real *c2, gc_real *c3,
                      int valid_r, int valid_c)
{
    gc_real r0[4] = {0}, r1[4] = {0}, r2[4] = {0}, r3[4] = {0};
    int p, i, j;
#ifdef GC_NEON
    if (valid_c == 4) {
        float64x2_t c00 = vdupq_n_f64(0.0), c01 = vdupq_n_f64(0.0);
        float64x2_t c10 = vdupq_n_f64(0.0), c11 = vdupq_n_f64(0.0);
        float64x2_t c20 = vdupq_n_f64(0.0), c21 = vdupq_n_f64(0.0);
        float64x2_t c30 = vdupq_n_f64(0.0), c31 = vdupq_n_f64(0.0);
        for (p = 0; p < kc; p++) {
            const gc_real *bp = Bp + (size_t)p * GC_GEMM_NC;
            const gc_real *ap = Ap + (size_t)p * GC_GEMM_MC;
            float64x2_t b0 = vld1q_f64(bp), b1 = vld1q_f64(bp + 2);
            float64x2_t a0 = vdupq_n_f64(ap[0]), a1 = vdupq_n_f64(ap[1]);
            float64x2_t a2 = vdupq_n_f64(ap[2]), a3 = vdupq_n_f64(ap[3]);
            c00 = vfmaq_f64(c00, a0, b0); c01 = vfmaq_f64(c01, a0, b1);
            c10 = vfmaq_f64(c10, a1, b0); c11 = vfmaq_f64(c11, a1, b1);
            c20 = vfmaq_f64(c20, a2, b0); c21 = vfmaq_f64(c21, a2, b1);
            c30 = vfmaq_f64(c30, a3, b0); c31 = vfmaq_f64(c31, a3, b1);
        }
        vst1q_f64(r0, c00); vst1q_f64(r0 + 2, c01);
        vst1q_f64(r1, c10); vst1q_f64(r1 + 2, c11);
        vst1q_f64(r2, c20); vst1q_f64(r2 + 2, c21);
        vst1q_f64(r3, c30); vst1q_f64(r3 + 2, c31);
    } else
#endif
    {
        for (p = 0; p < kc; p++) {
            const gc_real *bp = Bp + (size_t)p * GC_GEMM_NC;
            const gc_real *ap = Ap + (size_t)p * GC_GEMM_MC;
            gc_real a0 = ap[0], a1 = ap[1], a2 = ap[2], a3 = ap[3];
            for (j = 0; j < valid_c; j++) {
                gc_real bj = bp[j];
                r0[j] += a0 * bj; r1[j] += a1 * bj; r2[j] += a2 * bj; r3[j] += a3 * bj;
            }
        }
    }
    for (j = 0; j < valid_c; j++) c0[j] += r0[j];
    if (valid_r > 1) for (j = 0; j < valid_c; j++) c1[j] += r1[j];
    if (valid_r > 2) for (j = 0; j < valid_c; j++) c2[j] += r2[j];
    if (valid_r > 3) for (j = 0; j < valid_c; j++) c3[j] += r3[j];
    (void)i;
}

void gc_gemm(int m, int n, int k, gc_real alpha,
             const gc_real *A, int lda,
             const gc_real *B, int ldb,
             gc_real beta, gc_real *C, int ldc)
{
    gc_real *Ap, *Bp;
    int i, j, jb, kb, ib;
    if (m <= 0 || n <= 0 || C == NULL) return;

    if (beta == 0.0) {
        for (i = 0; i < m; i++) gc_array_fill(C + (size_t)i * (size_t)ldc, 0.0, n);
    } else if (beta != 1.0) {
        for (i = 0; i < m; i++) gc_array_scale(C + (size_t)i * (size_t)ldc, beta, n);
    }
    if (k <= 0 || A == NULL || B == NULL || alpha == 0.0) return;

    Ap = (gc_real *)malloc((size_t)GC_GEMM_MC * GC_GEMM_KC * sizeof(gc_real));
    Bp = (gc_real *)malloc((size_t)GC_GEMM_KC * GC_GEMM_NC * sizeof(gc_real));
    if (!Ap || !Bp) {
        /* fallback scalare se la memoria per il packing non è disponibile */
        int p;
        for (i = 0; i < m; i++)
            for (p = 0; p < k; p++) {
                gc_real a = alpha * A[(size_t)i * lda + p];
                if (a != 0.0)
                    gc_array_axpy(a, B + (size_t)p * ldb, C + (size_t)i * ldc, n);
            }
        free(Ap); free(Bp);
        return;
    }

    for (jb = 0; jb < n; jb += GC_GEMM_NC) {
        int nc = (jb + GC_GEMM_NC < n) ? GC_GEMM_NC : (n - jb);
        for (kb = 0; kb < k; kb += GC_GEMM_KC) {
            int kc = (kb + GC_GEMM_KC < k) ? GC_GEMM_KC : (k - kb);
            pack_b(kc, nc, B, ldb, kb, jb, Bp);
            for (ib = 0; ib < m; ib += GC_GEMM_MC) {
                int mc = (ib + GC_GEMM_MC < m) ? GC_GEMM_MC : (m - ib);
                pack_a(mc, kc, alpha, A, lda, ib, kb, Ap);
                for (j = 0; j < nc; j += 4) {
                    int valid_c = (j + 4 <= nc) ? 4 : (nc - j);
                    int ir;
                    for (ir = 0; ir + 4 <= mc; ir += 4) {
                        adddot4x4(kc, Ap + ir, Bp + j,
                                  C + (size_t)(ib + ir) * ldc + (jb + j),
                                  C + (size_t)(ib + ir + 1) * ldc + (jb + j),
                                  C + (size_t)(ib + ir + 2) * ldc + (jb + j),
                                  C + (size_t)(ib + ir + 3) * ldc + (jb + j),
                                  4, valid_c);
                    }
                    if (ir < mc) {
                        int vr = mc - ir;
                        gc_real *cr = C + (size_t)(ib + ir) * ldc + (jb + j);
                        adddot4x4(kc, Ap + ir, Bp + j, cr,
                                  (vr > 1) ? cr + ldc : cr,
                                  (vr > 2) ? cr + 2 * ldc : cr,
                                  (vr > 3) ? cr + 3 * ldc : cr, vr, valid_c);
                    }
                }
            }
        }
    }
    free(Ap);
    free(Bp);
}

void gc_gemv(int m, int k, gc_real alpha,
             const gc_real *A, int lda,
             const gc_real *x,
             gc_real beta, gc_real *y)
{
    int i;
    if (m <= 0 || A == NULL || x == NULL || y == NULL) return;
    for (i = 0; i < m; i++) {
        gc_real d = gc_array_dot(A + (size_t)i * (size_t)lda, x, k);
        y[i] = beta * y[i] + alpha * d;
    }
}

void gc_outer(int m, int n, gc_real alpha,
              const gc_real *x, const gc_real *y,
              gc_real beta, gc_real *A, int lda)
{
    int i;
    if (m <= 0 || n <= 0 || x == NULL || y == NULL || A == NULL) return;
    for (i = 0; i < m; i++) {
        gc_real *row = A + (size_t)i * (size_t)lda;
        gc_real a = alpha * x[i];
        if (beta == 0.0) {
            gc_array_scaled(a, y, row, n);
        } else {
            gc_array_axpby(a, y, beta, row, n);
        }
    }
}
