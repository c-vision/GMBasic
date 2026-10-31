# samples/torch — examples of the `TORCH` namespace (native ML runtime, PyTorch/libtorch)

> **Prerequisite**: the ML runtime libraries must be downloaded into the user cache
> `~/.gmbasic/ml/<rid>/` — once only:
> ```
> gmb setup TORCH      # CLI, with a progress bar (CPU by default)
> # or in the REPL:    SETUP TORCH
> # status:            gmb setup status / SETUP STATUS
> ```
> Without the libraries, `TORCH.*` returns handle -1 / `NOTLOADED` (never a crash).
> Active device: `GMB_ML_DEVICE=cpu|cuda|rocm|mlx` (default auto→CPU).

## Examples

| File | What it shows |
|---|---|
| `TorchBasics.bas` | device/loader (`DEVICE`/`LOADED`/`LIBPATH`), constructors (`ZEROS/ONES/EYE`), `FROM`+arithmetic (`ADD/MUL/SUM/MEAN/MAX/MIN`), `MATMUL`, activations (`RELU/SIGMOID/TANH/GELU/SQRT`), access (`GET/ALLCLOSE`) |
| `TorchTensorOps.bas` | complete surface: constructors (`RANDN`), activations (`SOFTMAX`), pointwise (`EXP/LOG/POW/ABS`), shape ops (`RESHAPE/TRANSPOSE/PERMUTE/CONCAT/STACK/SPLIT`), reductions, `SET` |
| `TorchBonus.bas` | bonus functions of the ML plan §3.6: `TOPK`/`ARGSORT`/`ONEHOT`/`CLIP`/`NORMALIZE`/`STANDARDIZE`/`SIMILARITY` (via libtorch) |
| `TorchTrain.bas` | **training**: `MODELCREATE` (linear 2→1), `MODELTRAINSTEP` (Adam/MSE), `MODELLOSS`, `MODELFORWARD` (prediction), `MODELPARAMETERS`, `MODELSTATE`, `MODELFREE` |
| `TorchCommands.bas` | **legacy space commands** (no return value): `TORCH INFO`, `TORCH LOAD LIB`, `TORCH UNLOAD LIB`, `TORCH SET DEVICE "cpu"`, `TORCH SET LIB PATH "dir"` — they complete the reading functions `DEVICE()`/`LOADED()`/`LIBPATH()` |

## Execution

```
gmb samples/torch/TorchBasics.bas        # VM/C backend (default)
gmb --pcode samples/torch/TorchBasics.bas  # PCodeVM managed (C#)
```

The three samples run identical on all backends: **PCodeVM managed, IL, VM/C, C99**
(floating point tolerance). Verified on macOS arm64, libtorch 2.13 CPU.

## Notes

- `TORCH.FROM(array)` creates a 1×N row (for matrices use `RESHAPE`).
- Opaque `Long` handles: they are not compatible with `MATRIX` (separate registries).
- `TORCH.SHAPE`/`TOARRAY` return arrays; on C99 `SHAPE` has a known type
  limit (the other backends are correct).
- Flat `MODEL*` names (`MODELCREATE` not `MODEL.CREATE`): the parser does not support
  3-level qualifications.
- **TORCH legacy commands**: `TORCH INFO`, `TORCH LOAD LIB`, `TORCH UNLOAD LIB`,
  `TORCH SET DEVICE "auto|cpu|cuda|rocm|mlx"`, `TORCH SET LIB PATH "dir"` — they use
  the space syntax (no parentheses, no return value). If torch is not
  loaded or the device is not available they raise a clear error on all
  backends (PCodeVM/IL, VM/C, C99). `SET LIB PATH` toward a folder without libs
  makes the lib not loadable (error at the next `LOAD`).
- **Note (solved 2026-08-28)**: `MODELTRAINSTEP` had a deadlock on VM/C, **fixed**.
  The cause was NOT libtorch↔GPM threading: it was a self-deadlock on `g_lock` in the host
  (`gml_err` called under the non-reentrant lock) triggered by an array size bug
  in VM/C (`OP_INIT_ARRAY` created `DIM x(3)={0,1,2}` with 3 elements instead of 4 → `TORCH.FROM`
  produced a tensor of the wrong size). Both were fixed
  (`native/gmb_torch/src/gmb_host_ml.cpp`, `native/gmb_vm_c/gmb_vm_core.c`). Now the training
  runs identical on all backends.
