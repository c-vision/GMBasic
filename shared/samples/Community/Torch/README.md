# samples/torch — esempi del namespace `TORCH` (runtime ML nativo, PyTorch/libtorch)

> **Prerequisito**: le librerie runtime ML vanno scaricate nella cache utente
> `~/.gmbasic/ml/<rid>/` — una volta sola:
> ```
> gmb setup TORCH      # CLI, con progress bar (CPU per default)
> # oppure nel REPL:   SETUP TORCH
> # stato:             gmb setup status / SETUP STATUS
> ```
> Senza le librerie, `TORCH.*` ritorna handle -1 / `NOTLOADED` (mai crash).
> Device attivo: `GMB_ML_DEVICE=cpu|cuda|rocm|mlx` (default auto→CPU).

## Esempi

| File | Cosa mostra |
|---|---|
| `TorchBasics.bas` | device/loader (`DEVICE`/`LOADED`/`LIBPATH`), costruttori (`ZEROS/ONES/EYE`), `FROM`+aritmetica (`ADD/MUL/SUM/MEAN/MAX/MIN`), `MATMUL`, attivazioni (`RELU/SIGMOID/TANH/GELU/SQRT`), accesso (`GET/ALLCLOSE`) |
| `TorchTensorOps.bas` | superficie completa: costruttori (`RANDN`), attivazioni (`SOFTMAX`), puntuali (`EXP/LOG/POW/ABS`), shape ops (`RESHAPE/TRANSPOSE/PERMUTE/CONCAT/STACK/SPLIT`), riduzioni, `SET` |
| `TorchBonus.bas` | funzioni bonus del piano ML §3.6: `TOPK`/`ARGSORT`/`ONEHOT`/`CLIP`/`NORMALIZE`/`STANDARDIZE`/`SIMILARITY` (via libtorch) |
| `TorchTrain.bas` | **addestramento**: `MODELCREATE` (linear 2→1), `MODELTRAINSTEP` (Adam/MSE), `MODELLOSS`, `MODELFORWARD` (predizione), `MODELPARAMETERS`, `MODELSTATE`, `MODELFREE` |
| `TorchCommands.bas` | **comandi legacy a spazio** (nessun valore di ritorno): `TORCH INFO`, `TORCH LOAD LIB`, `TORCH UNLOAD LIB`, `TORCH SET DEVICE "cpu"`, `TORCH SET LIB PATH "dir"` — completano le funzioni di lettura `DEVICE()`/`LOADED()`/`LIBPATH()` |

## Esecuzione

```
gmb samples/torch/TorchBasics.bas        # backend VM/C (default)
gmb --pcode samples/torch/TorchBasics.bas  # PCodeVM managed (C#)
```

I tre sample girano identici su tutti i backend: **PCodeVM managed, IL, VM/C, C99**
(tolleranza floating point). Verificati su macOS arm64, libtorch 2.13 CPU.

## Note

- `TORCH.FROM(array)` crea una riga 1×N (per matrici usa `RESHAPE`).
- Handle opachi `Long`: non sono compatibili con `MATRIX` (registry separati).
- `TORCH.SHAPE`/`TOARRAY` restituiscono array; su C99 `SHAPE` ha un limite di tipo
  noto (gli altri backend sono corretti).
- Nomi `MODEL*` piatti (`MODELCREATE` non `MODEL.CREATE`): il parser non supporta
  qualifiche a 3 livelli.
- **Comandi legacy TORCH**: `TORCH INFO`, `TORCH LOAD LIB`, `TORCH UNLOAD LIB`,
  `TORCH SET DEVICE "auto|cpu|cuda|rocm|mlx"`, `TORCH SET LIB PATH "dir"` — usano
  la sintassi a spazio (niente parentesi, nessun valore di ritorno). Se torch non
  è caricato o il device non è disponibile sollevano un errore chiaro su tutti i
  backend (PCodeVM/IL, VM/C, C99). `SET LIB PATH` verso una cartella senza lib
  rende la lib non caricabile (errore al successivo `LOAD`).
- **Nota (risolto 2026-08-28)**: `MODELTRAINSTEP` aveva un deadlock su VM/C, **corretto**.
  La causa NON era il threading libtorch↔GPM: era un self-deadlock su `g_lock` nel host
  (`gml_err` chiamato sotto la lock non-reentrant) innescato da un bug di dimensione array
  in VM/C (`OP_INIT_ARRAY` creava `DIM x(3)={0,1,2}` con 3 elementi invece di 4 → `TORCH.FROM`
  produceva un tensor di dimensione sbagliata). Fixati entrambi
  (`native/gmb_torch/src/gmb_host_ml.cpp`, `native/gmb_vm_c/gmb_vm_core.c`). Ora il training
  gira identico su tutti i backend.
