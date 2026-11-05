/*
 * gc_compute_metal.m — backend Metal (Apple GPU).
 *
 * Compilazione: cc -x objective-c -fobjc-arc -DGC_HAS_METAL ...
 *               -framework Metal -framework Foundation
 *
 * Le GPU Apple non hanno fp64: il backend usa float32 internamente, con
 * conversione ai bordi (gc_real double <-> float). I kernel offloadati sono
 * quelli row-parallel (spmv_csr, spmm, scale, sum, count_nonzero); gli altri
 * delegano al backend CPU per mantenere la superficie uniforme.
 */
#if defined(GC_HAS_METAL)

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "../gc_compute_internal.h"
#include "gc_dd.h"

#include <stdlib.h>

#define GC_METAL_TPB 256

static const char *GC_METAL_SRC =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "kernel void k_spmv_csr(device const int *rowptr [[buffer(0)]],\n"
    "                       device const int *colidx [[buffer(1)]],\n"
    "                       device const float *val [[buffer(2)]],\n"
    "                       device const float *x [[buffer(3)]],\n"
    "                       device float *y [[buffer(4)]],\n"
    "                       constant int &rows [[buffer(5)]],\n"
    "                       uint gid [[thread_position_in_grid]]) {\n"
    "  if (gid < (uint)rows) {\n"
    "    float s = 0.0f;\n"
    "    for (int j = rowptr[gid]; j < rowptr[gid+1]; j++) s += val[j] * x[colidx[j]];\n"
    "    y[gid] = s;\n"
    "  }\n"
    "}\n"
    "kernel void k_spmm(device const int *rowptr [[buffer(0)]],\n"
    "                   device const int *colidx [[buffer(1)]],\n"
    "                   device const float *val [[buffer(2)]],\n"
    "                   device const float *B [[buffer(3)]],\n"
    "                   device float *C [[buffer(4)]],\n"
    "                   constant int &rows [[buffer(5)]],\n"
    "                   constant int &n [[buffer(6)]],\n"
    "                   uint gid [[thread_position_in_grid]]) {\n"
    "  int i = gid / (uint)n;\n"
    "  int c = gid % (uint)n;\n"
    "  if (i < rows) {\n"
    "    float s = 0.0f;\n"
    "    for (int j = rowptr[i]; j < rowptr[i+1]; j++) s += val[j] * B[colidx[j]*n + c];\n"
    "    C[i*n + c] = s;\n"
    "  }\n"
    "}\n"
    "kernel void k_scale(device float *x [[buffer(0)]],\n"
    "                    constant int &n [[buffer(1)]],\n"
    "                    constant float &a [[buffer(2)]],\n"
    "                    uint gid [[thread_position_in_grid]]) {\n"
    "  if (gid < (uint)n) x[gid] *= a;\n"
    "}\n"
    "kernel void k_sum(device const float *x [[buffer(0)]],\n"
    "                  device float *partial [[buffer(1)]],\n"
    "                  constant int &n [[buffer(2)]],\n"
    "                  uint gid [[thread_position_in_grid]],\n"
    "                  uint lid [[thread_position_in_threadgroup]],\n"
    "                  uint tgid [[threadgroup_position_in_grid]],\n"
    "                  uint tgsize [[threads_per_threadgroup]]) {\n"
    "  threadgroup float sh[256];\n"
    "  sh[lid] = (gid < (uint)n) ? x[gid] : 0.0f;\n"
    "  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    "  for (uint s = tgsize/2; s > 0; s >>= 1) {\n"
    "    if (lid < s) sh[lid] += sh[lid+s];\n"
    "    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    "  }\n"
    "  if (lid == 0) partial[tgid] = sh[0];\n"
    "}\n"
    "kernel void k_count(device const float *x [[buffer(0)]],\n"
    "                    device int *partial [[buffer(1)]],\n"
    "                    constant int &n [[buffer(2)]],\n"
    "                    uint gid [[thread_position_in_grid]],\n"
    "                    uint lid [[thread_position_in_threadgroup]],\n"
    "                    uint tgid [[threadgroup_position_in_grid]],\n"
    "                    uint tgsize [[threads_per_threadgroup]]) {\n"
    "  threadgroup int sh[256];\n"
    "  sh[lid] = (gid < (uint)n && x[gid] != 0.0f) ? 1 : 0;\n"
    "  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    "  for (uint s = tgsize/2; s > 0; s >>= 1) {\n"
    "    if (lid < s) sh[lid] += sh[lid+s];\n"
    "    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    "  }\n"
    "  if (lid == 0) partial[tgid] = sh[0];\n"
    "}\n";

static const char *GC_METAL_DD_SRC ="\n/* ===================== DD (double-float) shaders, da apple-bottom ===================== */\nstruct GC_DD { float hi; float lo; };\ninline void gc_two_sum(float a, float b, thread float &s, thread float &e) {\n    s = a + b; float v = s - a; e = (a - (s - v)) + (b - v);\n}\ninline void gc_fast_two_sum(float a, float b, thread float &s, thread float &e) {\n    s = a + b; e = b - (s - a);\n}\ninline void gc_two_product(float a, float b, thread float &p, thread float &e) {\n    p = a * b; e = fma(a, b, -p);\n}\ninline GC_DD gc_dd_fma(GC_DD a, GC_DD b, GC_DD c) {\n    float p1, e1, s2, e2, s, e;\n    gc_two_product(a.hi, b.hi, p1, e1);\n    e1 = fma(a.hi, b.lo, fma(a.lo, b.hi, e1));\n    gc_two_sum(p1, c.hi, s2, e2);\n    e2 += e1 + c.lo;\n    gc_fast_two_sum(s2, e2, s, e);\n    return GC_DD{s, e};\n}\ninline GC_DD gc_dd_renorm(GC_DD a) {\n    float s, e; gc_two_sum(a.hi, a.lo, s, e); return GC_DD{s, e};\n}\n\n#define DD_BM 64\n#define DD_BN 64\n#define DD_TK 16\n#define DD_TM 4\n#define DD_TN 4\n#define DD_NT 256\nkernel void gc_dd_dgemm_kernel(device const GC_DD *A [[buffer(0)]],\n                               device const GC_DD *B [[buffer(1)]],\n                               device GC_DD *C [[buffer(2)]],\n                               constant uint &M [[buffer(3)]],\n                               constant uint &N [[buffer(4)]],\n                               constant uint &K [[buffer(5)]],\n                               uint2 lid [[thread_position_in_threadgroup]],\n                               uint2 tgid [[threadgroup_position_in_grid]],\n                               uint flatId [[thread_index_in_threadgroup]]) {\n    uint bRow = tgid.y * DD_BM, bCol = tgid.x * DD_BN;\n    GC_DD acc[DD_TM][DD_TN];\n    for (uint i = 0; i < DD_TM; i++)\n        for (uint j = 0; j < DD_TN; j++) acc[i][j] = GC_DD{0.0f, 0.0f};\n    threadgroup GC_DD tileA[DD_BM * DD_TK];\n    threadgroup GC_DD tileB[DD_TK * DD_BN];\n    uint nkt = (K + DD_TK - 1) / DD_TK;\n    for (uint kt = 0; kt < nkt; kt++) {\n        for (uint i = flatId; i < DD_BM * DD_TK; i += DD_NT) {\n            uint r = i / DD_TK, c = i % DD_TK;\n            uint gr = bRow + r, gc = kt * DD_TK + c;\n            tileA[i] = (gr < M && gc < K) ? A[gr * K + gc] : GC_DD{0.0f, 0.0f};\n        }\n        for (uint i = flatId; i < DD_TK * DD_BN; i += DD_NT) {\n            uint r = i / DD_BN, c = i % DD_BN;\n            uint gr = kt * DD_TK + r, gc = bCol + c;\n            tileB[i] = (gr < K && gc < N) ? B[gr * N + gc] : GC_DD{0.0f, 0.0f};\n        }\n        threadgroup_barrier(mem_flags::mem_threadgroup);\n        for (uint k = 0; k < DD_TK; k++) {\n            GC_DD av[DD_TM], bv[DD_TN];\n            for (uint i = 0; i < DD_TM; i++) av[i] = tileA[(lid.y * DD_TM + i) * DD_TK + k];\n            for (uint j = 0; j < DD_TN; j++) bv[j] = tileB[k * DD_BN + lid.x * DD_TN + j];\n            for (uint i = 0; i < DD_TM; i++)\n                for (uint j = 0; j < DD_TN; j++) acc[i][j] = gc_dd_fma(av[i], bv[j], acc[i][j]);\n        }\n        threadgroup_barrier(mem_flags::mem_threadgroup);\n        if ((kt & 15u) == 15u)\n            for (uint i = 0; i < DD_TM; i++)\n                for (uint j = 0; j < DD_TN; j++) acc[i][j] = gc_dd_renorm(acc[i][j]);\n    }\n    for (uint i = 0; i < DD_TM; i++)\n        for (uint j = 0; j < DD_TN; j++) {\n            uint gr = bRow + lid.y * DD_TM + i, gc = bCol + lid.x * DD_TN + j;\n            if (gr < M && gc < N) C[gr * N + gc] = gc_dd_renorm(acc[i][j]);\n        }\n}\nkernel void gc_dd_dot_kernel(device const GC_DD *a [[buffer(0)]],\n                             device const GC_DD *b [[buffer(1)]],\n                             device GC_DD *partial [[buffer(2)]],\n                             constant int &n [[buffer(3)]],\n                             constant uint &stride [[buffer(4)]],\n                             uint gid [[thread_position_in_grid]],\n                             uint lid [[thread_position_in_threadgroup]],\n                             uint tgid [[threadgroup_position_in_grid]],\n                             uint tgsize [[threads_per_threadgroup]]) {\n    threadgroup GC_DD sh[256];\n    GC_DD s = GC_DD{0.0f, 0.0f};\n    for (uint i = gid; i < (uint)n; i += stride) s = gc_dd_fma(a[i], b[i], s);\n    sh[lid] = s;\n    threadgroup_barrier(mem_flags::mem_threadgroup);\n    for (uint t = tgsize / 2; t > 0; t >>= 1) {\n        if (lid < t) {\n            float s1,e1,s2,e2,t1,t2,hh,ll;\n            gc_two_sum(sh[lid].hi, sh[lid+t].hi, s1, e1);\n            gc_two_sum(sh[lid].lo, sh[lid+t].lo, s2, e2);\n            gc_two_sum(e1, s2, t1, t2); t2 += e2;\n            gc_two_sum(s1, t1, s1, e1); e1 += t2;\n            gc_fast_two_sum(s1, e1, hh, ll);\n            sh[lid] = GC_DD{hh, ll};\n        }\n        threadgroup_barrier(mem_flags::mem_threadgroup);\n    }\n    if (lid == 0) partial[tgid] = sh[0];\n}\nkernel void gc_dd_spmv_csr_kernel(device const int *rowptr [[buffer(0)]],\n                                  device const int *colidx [[buffer(1)]],\n                                  device const GC_DD *val [[buffer(2)]],\n                                  device const GC_DD *x [[buffer(3)]],\n                                  device GC_DD *y [[buffer(4)]],\n                                  constant int &rows [[buffer(5)]],\n                                  uint gid [[thread_position_in_grid]]) {\n    if (gid < (uint)rows) {\n        GC_DD s = GC_DD{0.0f, 0.0f};\n        for (int j = rowptr[gid]; j < rowptr[gid + 1]; j++)\n            s = gc_dd_fma(val[j], x[colidx[j]], s);\n        y[gid] = gc_dd_renorm(s);\n    }\n}\n";

typedef struct {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLLibrary> lib;
    id<MTLComputePipelineState> p_spmv, p_spmm, p_scale, p_sum, p_count;
    id<MTLComputePipelineState> p_dd_gemm, p_dd_spmv, p_dd_dot;
    int state; /* 0 = non init, 1 = ok, -1 = fallito */
} gc_metal_ctx;

static gc_metal_ctx g_ctx;

static int gc_metal_init(void)
{
    NSError *err = nil;
    if (g_ctx.state == 1) return 1;
    if (g_ctx.state == -1) return 0;
    g_ctx.device = MTLCreateSystemDefaultDevice();
    if (g_ctx.device == nil) { g_ctx.state = -1; return 0; }
    g_ctx.queue = [g_ctx.device newCommandQueue];
    {
        MTLCompileOptions *opts = [[MTLCompileOptions alloc] init];
        opts.fastMathEnabled = NO; /* necessario per Dekker/Knuth (DD) */
        g_ctx.lib = [g_ctx.device newLibraryWithSource:[NSString stringWithFormat:@"%s\n%s", GC_METAL_SRC, GC_METAL_DD_SRC]
                                               options:opts error:&err];
    }
    if (g_ctx.lib == nil) { g_ctx.state = -1; return 0; }
    g_ctx.p_spmv  = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"k_spmv_csr"] error:&err];
    g_ctx.p_spmm  = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"k_spmm"] error:&err];
    g_ctx.p_scale = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"k_scale"] error:&err];
    g_ctx.p_sum   = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"k_sum"] error:&err];
    g_ctx.p_count = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"k_count"] error:&err];
    g_ctx.p_dd_gemm = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"gc_dd_dgemm_kernel"] error:&err];
    g_ctx.p_dd_spmv = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"gc_dd_spmv_csr_kernel"] error:&err];
    g_ctx.p_dd_dot = [g_ctx.device newComputePipelineStateWithFunction:[g_ctx.lib newFunctionWithName:@"gc_dd_dot_kernel"] error:&err];
    if (g_ctx.p_spmv == nil || g_ctx.p_spmm == nil || g_ctx.p_scale == nil
        || g_ctx.p_sum == nil || g_ctx.p_count == nil) { g_ctx.state = -1; return 0; }
    g_ctx.state = 1;
    return 1;
}

/* ---- buffer helper (conversione fp64 -> fp32 ai bordi) ---- */

static id<MTLBuffer> buf_f32_in(const gc_real *src, int n)
{
    float *tmp = (float *)malloc((size_t)(n > 0 ? n : 1) * sizeof(float));
    id<MTLBuffer> b;
    int i;
    if (tmp == NULL) return nil;
    for (i = 0; i < n; i++) tmp[i] = (float)src[i];
    b = [g_ctx.device newBufferWithBytes:tmp length:(size_t)n * sizeof(float)
                                 options:MTLResourceStorageModeShared];
    free(tmp);
    return b;
}

static id<MTLBuffer> buf_i32_in(const int *src, int n)
{
    return [g_ctx.device newBufferWithBytes:src length:(size_t)n * sizeof(int)
                                     options:MTLResourceStorageModeShared];
}

static id<MTLBuffer> buf_f32_out(int n)
{
    return [g_ctx.device newBufferWithLength:(size_t)n * sizeof(float)
                                     options:MTLResourceStorageModeShared];
}

/* ---- kernel offloadati ---- */

static gc_status metal_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y)
{
    id<MTLBuffer> brp, bci, bv, bx, by;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    int rows;
    int i;
    if (!gc_metal_init()) return gc_compute_ops_cpu()->spmv_csr(A, x, y);
    if (A == NULL || x == NULL || y == NULL) return GC_ERR_ARG;
    brp = buf_i32_in(A->rowptr, A->rows + 1);
    bci = buf_i32_in(A->colidx, A->nnz > 0 ? A->nnz : 1);
    bv  = buf_f32_in(A->val, A->nnz > 0 ? A->nnz : 1);
    bx  = buf_f32_in(x, A->cols);
    by  = buf_f32_out(A->rows);
    rows = A->rows;
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_spmv];
    [enc setBuffer:brp offset:0 atIndex:0];
    [enc setBuffer:bci offset:0 atIndex:1];
    [enc setBuffer:bv offset:0 atIndex:2];
    [enc setBuffer:bx offset:0 atIndex:3];
    [enc setBuffer:by offset:0 atIndex:4];
    [enc setBytes:&rows length:sizeof(int) atIndex:5];
    [enc dispatchThreadgroups:MTLSizeMake((A->rows + GC_METAL_TPB - 1) / GC_METAL_TPB, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(GC_METAL_TPB, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    {
        float *out = (float *)[by contents];
        for (i = 0; i < A->rows; i++) y[i] = (gc_real)out[i];
    }
    return GC_OK;
}

static gc_status metal_spmm_csr(const gc_csr *A, const gc_real *B, int n, gc_real *C)
{
    id<MTLBuffer> brp, bci, bv, bB, bC;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    int rows, nn;
    int i, total;
    if (!gc_metal_init()) return gc_compute_ops_cpu()->spmm_csr(A, B, n, C);
    if (A == NULL || B == NULL || C == NULL) return GC_ERR_ARG;
    brp = buf_i32_in(A->rowptr, A->rows + 1);
    bci = buf_i32_in(A->colidx, A->nnz > 0 ? A->nnz : 1);
    bv  = buf_f32_in(A->val, A->nnz > 0 ? A->nnz : 1);
    bB  = buf_f32_in(B, A->cols * n);
    bC  = buf_f32_out(A->rows * n);
    rows = A->rows; nn = n;
    total = A->rows * n;
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_spmm];
    [enc setBuffer:brp offset:0 atIndex:0];
    [enc setBuffer:bci offset:0 atIndex:1];
    [enc setBuffer:bv offset:0 atIndex:2];
    [enc setBuffer:bB offset:0 atIndex:3];
    [enc setBuffer:bC offset:0 atIndex:4];
    [enc setBytes:&rows length:sizeof(int) atIndex:5];
    [enc setBytes:&nn length:sizeof(int) atIndex:6];
    [enc dispatchThreadgroups:MTLSizeMake((total + GC_METAL_TPB - 1) / GC_METAL_TPB, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(GC_METAL_TPB, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    {
        float *out = (float *)[bC contents];
        for (i = 0; i < total; i++) C[i] = (gc_real)out[i];
    }
    return GC_OK;
}

static void metal_scale(gc_real *x, int n, gc_real a)
{
    id<MTLBuffer> bx;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    float af = (float)a;
    int nn = n, i;
    if (!gc_metal_init()) { gc_compute_ops_cpu()->scale(x, n, a); return; }
    if (x == NULL || n <= 0) return;
    bx = buf_f32_in(x, n);
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_scale];
    [enc setBuffer:bx offset:0 atIndex:0];
    [enc setBytes:&nn length:sizeof(int) atIndex:1];
    [enc setBytes:&af length:sizeof(float) atIndex:2];
    [enc dispatchThreadgroups:MTLSizeMake((n + GC_METAL_TPB - 1) / GC_METAL_TPB, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(GC_METAL_TPB, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    {
        float *out = (float *)[bx contents];
        for (i = 0; i < n; i++) x[i] = (gc_real)out[i];
    }
}

static gc_real metal_sum(const gc_real *x, int n)
{
    id<MTLBuffer> bx, bp;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    int nn = n, ntg, i;
    gc_real s = 0.0;
    if (!gc_metal_init()) return gc_compute_ops_cpu()->sum(x, n);
    if (x == NULL || n <= 0) return 0.0;
    ntg = (n + GC_METAL_TPB - 1) / GC_METAL_TPB;
    bx = buf_f32_in(x, n);
    bp = buf_f32_out(ntg);
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_sum];
    [enc setBuffer:bx offset:0 atIndex:0];
    [enc setBuffer:bp offset:0 atIndex:1];
    [enc setBytes:&nn length:sizeof(int) atIndex:2];
    [enc dispatchThreadgroups:MTLSizeMake(ntg, 1, 1) threadsPerThreadgroup:MTLSizeMake(GC_METAL_TPB, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    {
        float *out = (float *)[bp contents];
        for (i = 0; i < ntg; i++) s += (gc_real)out[i];
    }
    return s;
}

static int metal_count_nonzero(const gc_real *x, int n)
{
    id<MTLBuffer> bx, bp;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    int nn = n, ntg, i, c = 0;
    if (!gc_metal_init()) return gc_compute_ops_cpu()->count_nonzero(x, n);
    if (x == NULL || n <= 0) return 0;
    ntg = (n + GC_METAL_TPB - 1) / GC_METAL_TPB;
    bx = buf_f32_in(x, n);
    bp = [g_ctx.device newBufferWithLength:(size_t)ntg * sizeof(int) options:MTLResourceStorageModeShared];
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_count];
    [enc setBuffer:bx offset:0 atIndex:0];
    [enc setBuffer:bp offset:0 atIndex:1];
    [enc setBytes:&nn length:sizeof(int) atIndex:2];
    [enc dispatchThreadgroups:MTLSizeMake(ntg, 1, 1) threadsPerThreadgroup:MTLSizeMake(GC_METAL_TPB, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    {
        int *out = (int *)[bp contents];
        for (i = 0; i < ntg; i++) c += out[i];
    }
    return c;
}

/* Le ops Metal partono dalla tabella CPU (fallback) e sovrascrivono i kernel
 * offloadati. Inizializzazione a runtime (i puntatori CPU non sono costanti). */

/* ===================== DD host: gemm e dot (FP64-class su Metal) ===================== */

void gc_dd_metal_gemm(int m, int n, int k, const gc_dd *A, const gc_dd *B, gc_dd *C)
{
    id<MTLBuffer> bA, bB, bC;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    uint um = (uint)m, un = (uint)n, uk = (uint)k;
    if (!gc_metal_init() || m <= 0 || n <= 0 || k <= 0) return;
    bA = [g_ctx.device newBufferWithBytes:A length:(size_t)m * (size_t)k * sizeof(gc_dd) options:MTLResourceStorageModeShared];
    bB = [g_ctx.device newBufferWithBytes:B length:(size_t)k * (size_t)n * sizeof(gc_dd) options:MTLResourceStorageModeShared];
    bC = [g_ctx.device newBufferWithLength:(size_t)m * (size_t)n * sizeof(gc_dd) options:MTLResourceStorageModeShared];
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_dd_gemm];
    [enc setBuffer:bA offset:0 atIndex:0];
    [enc setBuffer:bB offset:0 atIndex:1];
    [enc setBuffer:bC offset:0 atIndex:2];
    [enc setBytes:&um length:sizeof(uint) atIndex:3];
    [enc setBytes:&un length:sizeof(uint) atIndex:4];
    [enc setBytes:&uk length:sizeof(uint) atIndex:5];
    [enc dispatchThreadgroups:MTLSizeMake((n + 63) / 64, (m + 63) / 64, 1)
        threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    memcpy(C, [bC contents], (size_t)m * (size_t)n * sizeof(gc_dd));
}

gc_dd gc_dd_metal_dot(int n, const gc_dd *a, const gc_dd *b)
{
    id<MTLBuffer> bA, bB, bP;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> enc;
    uint un = (uint)n, stride;
    int ntg = (n + 255) / 256, i;
    gc_dd sum; sum.hi = 0.0f; sum.lo = 0.0f;
    if (!gc_metal_init() || n <= 0) return sum;
    stride = (uint)(ntg * 256);
    bA = [g_ctx.device newBufferWithBytes:a length:(size_t)n * sizeof(gc_dd) options:MTLResourceStorageModeShared];
    bB = [g_ctx.device newBufferWithBytes:b length:(size_t)n * sizeof(gc_dd) options:MTLResourceStorageModeShared];
    bP = [g_ctx.device newBufferWithLength:(size_t)ntg * sizeof(gc_dd) options:MTLResourceStorageModeShared];
    cb = [g_ctx.queue commandBuffer];
    enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:g_ctx.p_dd_dot];
    [enc setBuffer:bA offset:0 atIndex:0];
    [enc setBuffer:bB offset:0 atIndex:1];
    [enc setBuffer:bP offset:0 atIndex:2];
    [enc setBytes:&un length:sizeof(uint) atIndex:3];
    [enc setBytes:&stride length:sizeof(uint) atIndex:4];
    [enc dispatchThreadgroups:MTLSizeMake(ntg, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    {
        gc_dd *part = (gc_dd *)[bP contents];
        for (i = 0; i < ntg; i++) {
            float s1,e1,s2,e2,t1,t2,hh,ll;
            gc_dd_two_sum(sum.hi, part[i].hi, &s1, &e1);
            gc_dd_two_sum(sum.lo, part[i].lo, &s2, &e2);
            gc_dd_two_sum(e1, s2, &t1, &t2); t2 += e2;
            gc_dd_two_sum(s1, t1, &s1, &e1); e1 += t2;
            gc_dd_fast_two_sum(s1, e1, &hh, &ll);
            sum.hi = hh; sum.lo = ll;
        }
    }
    return sum;
}

static gc_compute_ops g_metal_ops;
static int g_metal_ops_init = 0;

static void init_metal_ops(void)
{
    if (g_metal_ops_init) return;
    g_metal_ops = *gc_compute_ops_cpu();
    g_metal_ops.spmv_csr = metal_spmv_csr;
    g_metal_ops.spmm_csr = metal_spmm_csr;
    g_metal_ops.sum = metal_sum;
    g_metal_ops.count_nonzero = metal_count_nonzero;
    g_metal_ops.scale = metal_scale;
    g_metal_ops_init = 1;
}

const gc_compute_ops *gc_compute_ops_metal(void)
{
    init_metal_ops();
    return &g_metal_ops;
}

#endif /* GC_BACKEND_METAL */
