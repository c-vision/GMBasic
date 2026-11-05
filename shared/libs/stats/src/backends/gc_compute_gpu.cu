/*
 * gc_compute_gpu.cu — backend GPU portabile (HIP per AMD ROCm e NVIDIA; CUDA).
 *
 * Un solo sorgente copre AMD e NVIDIA: si compila con
 *   hipcc -DGC_HAS_HIP   ...   (AMD ROCm o NVIDIA via HIP)
 *   nvcc  -DGC_HAS_CUDA  ...   (NVIDIA CUDA)
 *
 * La superficie è quella di gc_compute (buffer host in/out): ogni chiamata
 * copia H2D, lancia il kernel, copia D2H. I kernel non implementati qui
 * (scan/select/spmspv/semiring) delegano al backend CPU.
 *
 * NON compilato dal Makefile di default: richiede il toolchain GPU.
 */
#if defined(GC_HAS_HIP) || defined(GC_HAS_CUDA)

#include "../gc_compute_internal.h"

#if defined(GC_HAS_HIP)
  #include <hip/hip_runtime.h>
  #define GC_MALLOC            hipMalloc
  #define GC_FREE              hipFree
  #define GC_MEMCPY            hipMemcpy
  #define GC_H2D               hipMemcpyHostToDevice
  #define GC_D2H               hipMemcpyDeviceToHost
  #define GC_SYNC              hipDeviceSynchronize
  #define GC_LAUNCH(k, g, b, ...) hipLaunchKernelGGL(k, g, b, 0, 0, __VA_ARGS__)
#else /* GC_BACKEND_CUDA */
  #include <cuda_runtime.h>
  #define GC_MALLOC            cudaMalloc
  #define GC_FREE              cudaFree
  #define GC_MEMCPY            cudaMemcpy
  #define GC_H2D               cudaMemcpyHostToDevice
  #define GC_D2H               cudaMemcpyDeviceToHost
  #define GC_SYNC              cudaDeviceSynchronize
  #define GC_LAUNCH(k, g, b, ...) k<<<g, b>>>(__VA_ARGS__)
#endif

#define GC_TPB 256

/* ---- kernel ---- */

__global__ void gc_k_spmv_csr(const int *rowptr, const int *colidx, const gc_real *val,
                              const gc_real *x, gc_real *y, int rows)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < rows) {
        gc_real s = 0.0;
        int j;
        for (j = rowptr[i]; j < rowptr[i + 1]; j++) s += val[j] * x[colidx[j]];
        y[i] = s;
    }
}

__global__ void gc_k_spmv_csr_t(const int *rowptr, const int *colidx, const gc_real *val,
                                const gc_real *x, gc_real *y, int rows)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < rows) {
        gc_real xi = x[i];
        int j;
        for (j = rowptr[i]; j < rowptr[i + 1]; j++) atomicAdd(&y[colidx[j]], val[j] * xi);
    }
}

__global__ void gc_k_sum(const gc_real *x, int n, gc_real *acc)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;
    gc_real local = 0.0;
    for (; i < n; i += stride) local += x[i];
    atomicAdd(acc, local);
}

__global__ void gc_k_scale(gc_real *x, int n, gc_real a)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;
    for (; i < n; i += stride) x[i] *= a;
}

__global__ void gc_k_count_nonzero(const gc_real *x, int n, int *acc)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = gridDim.x * blockDim.x;
    for (; i < n; i += stride) if (x[i] != 0.0) atomicAdd(acc, 1);
}

/* ---- helper host ---- */

static int gc_grid(int n) { return (n + GC_TPB - 1) / GC_TPB; }

static gc_status gc_upload_csr(const gc_csr *A, int **rp, int **ci, gc_real **v)
{
    *rp = NULL; *ci = NULL; *v = NULL;
    if (GC_MALLOC((void **)rp, (size_t)(A->rows + 1) * sizeof(int)) != 0) return GC_ERR_NO_MEM;
    if (GC_MALLOC((void **)ci, (size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(int)) != 0) { GC_FREE(*rp); return GC_ERR_NO_MEM; }
    if (GC_MALLOC((void **)v, (size_t)(A->nnz > 0 ? A->nnz : 1) * sizeof(gc_real)) != 0) { GC_FREE(*rp); GC_FREE(*ci); return GC_ERR_NO_MEM; }
    GC_MEMCPY(*rp, A->rowptr, (size_t)(A->rows + 1) * sizeof(int), GC_H2D);
    GC_MEMCPY(*ci, A->colidx, (size_t)A->nnz * sizeof(int), GC_H2D);
    GC_MEMCPY(*v, A->val, (size_t)A->nnz * sizeof(gc_real), GC_H2D);
    return GC_OK;
}

/* ---- ops ---- */

static gc_status gpu_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int *rp, *ci; gc_real *v, *xd, *yd;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    if (gc_upload_csr(A, &rp, &ci, &v) != GC_OK) return GC_ERR_NO_MEM;
    GC_MALLOC((void **)&xd, (size_t)A->cols * sizeof(gc_real));
    GC_MALLOC((void **)&yd, (size_t)A->rows * sizeof(gc_real));
    GC_MEMCPY(xd, x, (size_t)A->cols * sizeof(gc_real), GC_H2D);
    GC_LAUNCH(gc_k_spmv_csr, gc_grid(A->rows), GC_TPB, rp, ci, v, xd, yd, A->rows);
    GC_MEMCPY(y, yd, (size_t)A->rows * sizeof(gc_real), GC_D2H);
    GC_SYNC;
    GC_FREE(rp); GC_FREE(ci); GC_FREE(v); GC_FREE(xd); GC_FREE(yd);
    return GC_OK;
}

static gc_status gpu_spmv_csr_t(const gc_csr *A, const gc_real *x, gc_real *y)
{
    int *rp, *ci; gc_real *v, *xd, *yd;
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    if (gc_upload_csr(A, &rp, &ci, &v) != GC_OK) return GC_ERR_NO_MEM;
    GC_MALLOC((void **)&xd, (size_t)A->rows * sizeof(gc_real));
    GC_MALLOC((void **)&yd, (size_t)A->cols * sizeof(gc_real));
    GC_MEMCPY(xd, x, (size_t)A->rows * sizeof(gc_real), GC_H2D);
    GC_MEMCPY(yd, y, (size_t)A->cols * sizeof(gc_real), GC_H2D);
    GC_LAUNCH(gc_k_spmv_csr_t, gc_grid(A->rows), GC_TPB, rp, ci, v, xd, yd, A->rows);
    GC_MEMCPY(y, yd, (size_t)A->cols * sizeof(gc_real), GC_D2H);
    GC_SYNC;
    GC_FREE(rp); GC_FREE(ci); GC_FREE(v); GC_FREE(xd); GC_FREE(yd);
    return GC_OK;
}

static gc_real gpu_sum(const gc_real *x, int n)
{
    gc_real *xd, *accd, acc = 0.0;
    if (x == NULL || n <= 0) return 0.0;
    GC_MALLOC((void **)&xd, (size_t)n * sizeof(gc_real));
    GC_MALLOC((void **)&accd, sizeof(gc_real));
    GC_MEMCPY(xd, x, (size_t)n * sizeof(gc_real), GC_H2D);
    GC_MEMCPY(accd, &acc, sizeof(gc_real), GC_H2D);
    GC_LAUNCH(gc_k_sum, gc_grid(n), GC_TPB, xd, n, accd);
    GC_MEMCPY(&acc, accd, sizeof(gc_real), GC_D2H);
    GC_SYNC;
    GC_FREE(xd); GC_FREE(accd);
    return acc;
}

static void gpu_scale(gc_real *x, int n, gc_real a)
{
    gc_real *xd;
    if (x == NULL || n <= 0) return;
    GC_MALLOC((void **)&xd, (size_t)n * sizeof(gc_real));
    GC_MEMCPY(xd, x, (size_t)n * sizeof(gc_real), GC_H2D);
    GC_LAUNCH(gc_k_scale, gc_grid(n), GC_TPB, xd, n, a);
    GC_MEMCPY(x, xd, (size_t)n * sizeof(gc_real), GC_D2H);
    GC_SYNC;
    GC_FREE(xd);
}

static int gpu_count_nonzero(const gc_real *x, int n)
{
    gc_real *xd;
    int *accd, acc = 0;
    if (x == NULL || n <= 0) return 0;
    GC_MALLOC((void **)&xd, (size_t)n * sizeof(gc_real));
    GC_MALLOC((void **)&accd, sizeof(int));
    GC_MEMCPY(xd, x, (size_t)n * sizeof(gc_real), GC_H2D);
    GC_MEMCPY(accd, &acc, sizeof(int), GC_H2D);
    GC_LAUNCH(gc_k_count_nonzero, gc_grid(n), GC_TPB, xd, n, accd);
    GC_MEMCPY(&acc, accd, sizeof(int), GC_D2H);
    GC_SYNC;
    GC_FREE(xd); GC_FREE(accd);
    return acc;
}

/* Kernel non offloadati: delega al backend CPU (superficie uniforme). */
static const gc_compute_ops g_gpu_ops = {
    gpu_spmv_csr,
    gc_compute_ops_cpu()->spmv_csc,
    gpu_spmv_csr_t,
    gc_compute_ops_cpu()->spmspv_csc,
    gc_compute_ops_cpu()->spmm_csr,
    gc_compute_ops_cpu()->spmv_minplus,
    gc_compute_ops_cpu()->spmv_maxplus,
    gc_compute_ops_cpu()->scan,
    gc_compute_ops_cpu()->scan_exclusive,
    gpu_sum,
    gpu_count_nonzero,
    gc_compute_ops_cpu()->select,
    gpu_scale
};

#if defined(GC_HAS_HIP)
const gc_compute_ops *gc_compute_ops_hip(void) { return &g_gpu_ops; }
#else
const gc_compute_ops *gc_compute_ops_cuda(void) { return &g_gpu_ops; }
#endif

#endif /* GC_BACKEND_HIP || GC_BACKEND_CUDA */
