# Backend GPU (opzionali)

La superficie pubblica (`gc_compute.h`) è identica su CPU e GPU; cambia solo il
backend attivo. I flag di build sono `GC_HAS_CUDA` / `GC_HAS_HIP` / `GC_HAS_METAL`
(distinti dagli enum `GC_BACKEND_*`).

## HIP / CUDA (AMD e NVIDIA) — `gc_compute_gpu.cu`

Un solo sorgente per GPU AMD e NVIDIA:

| Backend | Toolchain | Comando | Runtime da linkare |
|---|---|---|---|
| HIP (AMD ROCm) | `hipcc` | `make gpu-hip` | `-lamdhip64` |
| HIP (NVIDIA) | `hipcc` (ROCm) | `make gpu-hip` | `-lamdhip64` |
| CUDA (NVIDIA) | `nvcc` | `make gpu-cuda` | `-lcudart` |

Compilando con `-DGC_HAS_HIP` o `-DGC_HAS_CUDA`, `gc_available()` include il bit
corrispondente e `gc_set_backend(GC_BACKEND_HIP)` diventa valido.

## Metal (Apple) — `gc_compute_metal.m`

```sh
make test-metal        # compila la lib + il backend Metal e lancia la suite
```

- Compilazione: `cc -x objective-c -fobjc-arc -DGC_HAS_METAL ... -framework Metal -framework Foundation`.
- Shader compilati a runtime (`newLibraryWithSource:`), nessun `.metallib` a parte.
- **Le GPU Apple non hanno fp64**: il backend usa **float32** internamente, con
  conversione ai bordi (`gc_real` double ↔ `float`). Le altre GPU (HIP/CUDA)
  usano fp64.

## Cosa è offloadato

| Backend | Kernel su GPU | Fallback CPU |
|---|---|---|
| HIP/CUDA | `spmv_csr`, `spmv_csr_t` (atomicAdd), `sum`, `scale`, `count_nonzero` | spmv_csc, spmspv_csc, spmm_csr, semiring, scan, select |
| Metal | `spmv_csr`, `spmm_csr`, `sum`, `scale`, `count_nonzero` | spmv_csc, spmv_csr_t, spmspv_csc, semiring, scan, select |

La superficie resta uniforme: i kernel non offloadati girano su CPU.

## Nota

Nessuno di questi backend è compilato dal `make` di default (richiedono SDK
specifici). Il default resta CPU/SIMD (C99 + libc/libm).
