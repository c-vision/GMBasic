# FfiDropinLibs — FFI drop-in (`include/` + `libs/`)

Self-contained example of the FFI **drop-in** UX: the declarations in an `.inc` next to the
program + the native library in `libs/`, **with no REQUIRE package** and no manifest.

| File | Role |
|---|---|
| `FfiDropinLibs.bas` | BASIC program (`IMPORT FFIDROPIN_DEMO` + calls) |
| `FfiDropin.inc` | `DECLARE ... LIB "libvec_module"` declarations (namespace `FFIDROPIN_DEMO`) |
| `libs/libvec_module.dylib` | Native library (resolved by the `libs/` auto-discovery) |
| `Makefile` | Rebuilds `libs/` from `NativeVec/vec_module.c` |

## Build & run

```bash
cd shared/samples/FFI
make            # -> libs/libvec_module.dylib (macOS) / .so (Linux)
# then run FfiDropinLibs.bas with a runner (test_ilrun / test_vmrun / Executor)
```

## Library lookup order (`Guide/FFI-Guida-Utente.md` §9)

1. name as-is (CWD) · 2. name+extension · 3. **`libs/` walk-up** · 4. REQUIRE preload.

Kill-switch: `GMB_FFI_NO_LIBS_SCAN=1` disables step 3 on every leg.
