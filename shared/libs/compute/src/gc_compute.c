/* gc_compute.c — dispatch dei kernel e registry dei backend. */
#include "gc_compute_internal.h"
#include "gc_simd.h"

#include <stdlib.h>

/* ============================== backend CPU (scalare) ============================== */

static gc_status cpu_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int i, j;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) {
        gc_real s = 0.0;
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) s += A->val[j] * x[A->colidx[j]];
        y[i] = s;
    }
    return GC_OK;
}

static gc_status cpu_spmv_csc(const gc_csc *A, const gc_real *x, gc_real *y)
{
    int i, j, k;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) y[i] = 0.0;
    for (j = 0; j < A->cols; j++) {
        gc_real xj = x[j];
        for (k = A->colptr[j]; k < A->colptr[j + 1]; k++) y[A->rowidx[k]] += A->val[k] * xj;
    }
    return GC_OK;
}

static gc_status cpu_spmv_csr_t(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int i, j;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (j = 0; j < A->cols; j++) y[j] = 0.0;
    for (i = 0; i < A->rows; i++) {
        gc_real xi = x[i];
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) y[A->colidx[j]] += A->val[j] * xi;
    }
    return GC_OK;
}

static gc_status cpu_spmspv_csc(const gc_csc *A, const int *idx, int k, gc_real *y)
{
    int i, p, t;
    if (A == NULL || idx == NULL || y == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) y[i] = 0.0;
    for (t = 0; t < k; t++) {
        int j = idx[t];
        gc_real xj = 1.0; /* frontier: valore 1 sui vertici attivi */
        if (j < 0 || j >= A->cols) return GC_ERR_RANGE;
        for (p = A->colptr[j]; p < A->colptr[j + 1]; p++) y[A->rowidx[p]] += A->val[p] * xj;
    }
    return GC_OK;
}

static gc_status cpu_spmm_csr(const gc_csr *A, const gc_real *B, int n, gc_real *C)
{
    int i, j, c;
    if (A == NULL || B == NULL || C == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) {
        for (c = 0; c < n; c++) C[(size_t)i * (size_t)n + (size_t)c] = 0.0;
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) {
            gc_real a = A->val[j];
            const gc_real *brow = B + (size_t)A->colidx[j] * (size_t)n;
            gc_real *crow = C + (size_t)i * (size_t)n;
            for (c = 0; c < n; c++) crow[c] += a * brow[c];
        }
    }
    return GC_OK;
}

static gc_status cpu_spmv_minplus(const gc_csr *A, const gc_real *x, gc_real *y, gc_real identity)
{
    int i, j;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) {
        gc_real best = identity;
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) {
            gc_real v = A->val[j] + x[A->colidx[j]];
            if (v < best) best = v;
        }
        y[i] = best;
    }
    return GC_OK;
}

static gc_status cpu_spmv_maxplus(const gc_csr *A, const gc_real *x, gc_real *y, gc_real identity)
{
    int i, j;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) {
        gc_real best = identity;
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) {
            gc_real v = A->val[j] + x[A->colidx[j]];
            if (v > best) best = v;
        }
        y[i] = best;
    }
    return GC_OK;
}

static gc_status cpu_scan(int n, const int *x, int *y)
{
    int i, acc = 0;
    if (x == NULL || y == NULL || n < 0) return GC_ERR_ARG;
    for (i = 0; i < n; i++) { acc += x[i]; y[i] = acc; }
    return GC_OK;
}

static gc_status cpu_scan_exclusive(int n, const int *x, int *y)
{
    int i, acc = 0;
    if (x == NULL || y == NULL || n < 0) return GC_ERR_ARG;
    for (i = 0; i < n; i++) { y[i] = acc; acc += x[i]; }
    return GC_OK;
}

static gc_real cpu_sum(const gc_real *x, int n)
{
    gc_real s = 0.0;
    int i;
    if (x == NULL || n <= 0) return 0.0;
    for (i = 0; i < n; i++) s += x[i];
    return s;
}

static int cpu_count_nonzero(const gc_real *x, int n)
{
    int i, c = 0;
    if (x == NULL || n <= 0) return 0;
    for (i = 0; i < n; i++) if (x[i] != 0.0) c++;
    return c;
}

static int cpu_select(const gc_real *x, int n, int *out)
{
    int i, c = 0;
    if (x == NULL || out == NULL || n <= 0) return 0;
    for (i = 0; i < n; i++) if (x[i] != 0.0) out[c++] = i;
    return c;
}

static void cpu_scale(gc_real *x, int n, gc_real a)
{
    int i;
    if (x == NULL || n <= 0) return;
    for (i = 0; i < n; i++) x[i] *= a;
}

static const gc_compute_ops g_cpu_ops = {
    cpu_spmv_csr, cpu_spmv_csc, cpu_spmv_csr_t, cpu_spmspv_csc, cpu_spmm_csr,
    cpu_spmv_minplus, cpu_spmv_maxplus, cpu_scan, cpu_scan_exclusive,
    cpu_sum, cpu_count_nonzero, cpu_select, cpu_scale
};

const gc_compute_ops *gc_compute_ops_cpu(void) { return &g_cpu_ops; }

/* ============================== backend SIMD (vettorizzato) ============================== */
/* Riduzioni con 4 accumulatori (rompe la catena di dipendenza) e SpMV unroll. */

static gc_real simd_sum(const gc_real *x, int n)
{
    gc_real a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    int i = 0;
    if (x == NULL || n <= 0) return 0.0;
    for (; i + 4 <= n; i += 4) { a0 += x[i]; a1 += x[i + 1]; a2 += x[i + 2]; a3 += x[i + 3]; }
    for (; i < n; i++) a0 += x[i];
    return (a0 + a1) + (a2 + a3);
}

static int simd_count_nonzero(const gc_real *x, int n)
{
    int c0 = 0, c1 = 0, c2 = 0, c3 = 0, i = 0;
    if (x == NULL || n <= 0) return 0;
    for (; i + 4 <= n; i += 4) {
        c0 += (x[i] != 0.0); c1 += (x[i + 1] != 0.0);
        c2 += (x[i + 2] != 0.0); c3 += (x[i + 3] != 0.0);
    }
    for (; i < n; i++) c0 += (x[i] != 0.0);
    return c0 + c1 + c2 + c3;
}

static void simd_scale(gc_real *x, int n, gc_real a)
{
    int i = 0;
    if (x == NULL || n <= 0) return;
    for (; i + 4 <= n; i += 4) { x[i] *= a; x[i + 1] *= a; x[i + 2] *= a; x[i + 3] *= a; }
    for (; i < n; i++) x[i] *= a;
}

static gc_status simd_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int i;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (i = 0; i < A->rows; i++) {
        int j = A->rowptr[i], end = A->rowptr[i + 1];
        gc_real a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        for (; j + 4 <= end; j += 4) {
            a0 += A->val[j] * x[A->colidx[j]];
            a1 += A->val[j + 1] * x[A->colidx[j + 1]];
            a2 += A->val[j + 2] * x[A->colidx[j + 2]];
            a3 += A->val[j + 3] * x[A->colidx[j + 3]];
        }
        for (; j < end; j++) a0 += A->val[j] * x[A->colidx[j]];
        y[i] = (a0 + a1) + (a2 + a3);
    }
    return GC_OK;
}

static gc_status simd_spmv_csr_t(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int i, j;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    for (j = 0; j < A->cols; j++) y[j] = 0.0;
    for (i = 0; i < A->rows; i++) {
        gc_real xi = x[i];
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) y[A->colidx[j]] += A->val[j] * xi;
    }
    return GC_OK;
}

static const gc_compute_ops g_simd_ops = {
    simd_spmv_csr, cpu_spmv_csc, simd_spmv_csr_t, cpu_spmspv_csc, cpu_spmm_csr,
    cpu_spmv_minplus, cpu_spmv_maxplus, cpu_scan, cpu_scan_exclusive,
    simd_sum, simd_count_nonzero, cpu_select, simd_scale
};

const gc_compute_ops *gc_compute_ops_simd(void) { return &g_simd_ops; }

/* ============================== backend OpenMP (opzionale) ============================== */

#ifdef _OPENMP
static gc_status omp_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int i;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    #pragma omp parallel for schedule(static)
    for (i = 0; i < A->rows; i++) {
        int j;
        gc_real s = 0.0;
        for (j = A->rowptr[i]; j < A->rowptr[i + 1]; j++) s += A->val[j] * x[A->colidx[j]];
        y[i] = s;
    }
    return GC_OK;
}

static gc_real omp_sum(const gc_real *x, int n)
{
    gc_real s = 0.0;
    int i;
    if (x == NULL || n <= 0) return 0.0;
    #pragma omp parallel for reduction(+:s) schedule(static)
    for (i = 0; i < n; i++) s += x[i];
    return s;
}

static void omp_scale(gc_real *x, int n, gc_real a)
{
    int i;
    if (x == NULL || n <= 0) return;
    #pragma omp parallel for schedule(static)
    for (i = 0; i < n; i++) x[i] *= a;
}

static const gc_compute_ops g_omp_ops = {
    omp_spmv_csr, cpu_spmv_csc, cpu_spmv_csr_t, cpu_spmspv_csc, cpu_spmm_csr,
    cpu_spmv_minplus, cpu_spmv_maxplus, cpu_scan, cpu_scan_exclusive,
    omp_sum, cpu_count_nonzero, cpu_select, omp_scale
};

const gc_compute_ops *gc_compute_ops_omp(void) { return &g_omp_ops; }
#endif

/* ============================== registry ============================== */

static const gc_compute_ops *ops_for(gc_backend b)
{
    switch (b) {
    case GC_BACKEND_CPU: return gc_compute_ops_cpu();
    case GC_BACKEND_SIMD: return gc_compute_ops_simd();
#ifdef _OPENMP
    case GC_BACKEND_OPENMP: return gc_compute_ops_omp();
#endif
#ifdef GC_HAS_CUDA
    case GC_BACKEND_CUDA: return gc_compute_ops_cuda();
#endif
#ifdef GC_HAS_HIP
    case GC_BACKEND_HIP: return gc_compute_ops_hip();
#endif
#ifdef GC_HAS_METAL
    case GC_BACKEND_METAL: return gc_compute_ops_metal();
#endif
    default: return NULL;
    }
}

unsigned gc_available(void)
{
    unsigned m = (1u << GC_BACKEND_CPU) | (1u << GC_BACKEND_SIMD);
#ifdef _OPENMP
    m |= (1u << GC_BACKEND_OPENMP);
#endif
#ifdef GC_HAS_CUDA
    m |= (1u << GC_BACKEND_CUDA);
#endif
#ifdef GC_HAS_HIP
    m |= (1u << GC_BACKEND_HIP);
#endif
#ifdef GC_HAS_METAL
    m |= (1u << GC_BACKEND_METAL);
#endif
    return m;
}

const char *gc_backend_name(gc_backend b)
{
    switch (b) {
    case GC_BACKEND_CPU: return "cpu";
    case GC_BACKEND_SIMD: return "simd";
    case GC_BACKEND_OPENMP: return "openmp";
    case GC_BACKEND_CUDA: return "cuda";
    case GC_BACKEND_HIP: return "hip";
    case GC_BACKEND_METAL: return "metal";
    default: return "unknown";
    }
}

/* Backend attivo: il migliore disponibile (OpenMP > SIMD > CPU). */
static gc_backend default_backend(void)
{
#ifdef _OPENMP
    return GC_BACKEND_OPENMP;
#else
    return GC_BACKEND_SIMD;
#endif
}

static gc_backend g_active = GC_BACKEND__COUNT; /* non inizializzato */

gc_backend gc_active_backend(void)
{
    if (g_active == GC_BACKEND__COUNT) g_active = default_backend();
    return g_active;
}

gc_status gc_set_backend(gc_backend b)
{
    if (b < 0 || b >= GC_BACKEND__COUNT) return GC_ERR_ARG;
    if (!((gc_available() >> (unsigned)b) & 1u)) return GC_ERR_ARG;
    g_active = b;
    return GC_OK;
}

const char *gc_backend_str(void)
{
    return gc_backend_name(gc_active_backend());
}

int gc_device_count(void)
{
    return 1; /* backend CPU-family: un solo "device" logico */
}

const char *gc_device_name(int i)
{
    if (i != 0) return NULL;
    return gc_backend_str();
}

/* ============================== dispatch dei kernel ============================== */

#define OPS() ops_for(gc_active_backend())

gc_status gc_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y)
{ return OPS()->spmv_csr(A, x, y); }

gc_status gc_spmv_csc(const gc_csc *A, const gc_real *x, gc_real *y)
{ return OPS()->spmv_csc(A, x, y); }

gc_status gc_spmv_csr_t(const gc_csr *A, const gc_real *x, gc_real *y)
{ return OPS()->spmv_csr_t(A, x, y); }

gc_status gc_spmspv_csc(const gc_csc *A, const int *idx, int k, gc_real *y)
{ return OPS()->spmspv_csc(A, idx, k, y); }

gc_status gc_spmm_csr(const gc_csr *A, const gc_real *B, int n, gc_real *C)
{ return OPS()->spmm_csr(A, B, n, C); }

gc_status gc_spmv_minplus(const gc_csr *A, const gc_real *x, gc_real *y, gc_real identity)
{ return OPS()->spmv_minplus(A, x, y, identity); }

gc_status gc_spmv_maxplus(const gc_csr *A, const gc_real *x, gc_real *y, gc_real identity)
{ return OPS()->spmv_maxplus(A, x, y, identity); }

gc_status gc_scan(int n, const int *x, int *y)
{ return OPS()->scan(n, x, y); }

gc_status gc_scan_exclusive(int n, const int *x, int *y)
{ return OPS()->scan_exclusive(n, x, y); }

gc_real gc_sum(const gc_real *x, int n)
{ return OPS()->sum(x, n); }

int gc_count_nonzero(const gc_real *x, int n)
{ return OPS()->count_nonzero(x, n); }

int gc_select(const gc_real *x, int n, int *out)
{ return OPS()->select(x, n, out); }

void gc_scale(gc_real *x, int n, gc_real a)
{ OPS()->scale(x, n, a); }
