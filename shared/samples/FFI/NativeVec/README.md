# NativeVec — FFI example using `gmb_ffi.h`

Self-contained example of a **GMBasic** native module (`contentType: c-ffi`, `ffi.api: gmb-native`).

## Contents

| File | Role |
|---|---|
| `gmb_ffi.h` | ABI header (`GMB_FFI_VERSION=1`, copied from `native/gmb_runtime_c/include/`) |
| `vec_module.c` | C module: level 1 (c-abi) + level 2 (gmb-native `gmb_ffi_module_entry`) |
| `Makefile` | Builds `libvec_module.dylib`/`.so` |
| `NativeVec.bas` | BASIC program (uses `DECLARE`, level 1) |
| `vec_module_rust.rs` | Evaluation of the equivalent Rust module (`#[no_mangle] extern "C"`) |
| `vec_module_go.go` | Evaluation of the equivalent Go module (cgo `//export`) |

## Build & run

```bash
cd shared/samples/FFI/NativeVec
make                 # -> libvec_module.dylib (macOS) / libvec_module.so (Linux)
# then run NativeVec.bas (the runtime resolves ./NativeVec/vec_module)
```

## The two levels

1. **c-abi** (`DECLARE`, used today): flat C functions (`vec_add`, `vec_label`, `vec_split`) as
   normal symbols. BASIC declares and calls them.
2. **gmb-native** (future dispatcher): `gmb_ffi_module_entry()` returns the
   `gmb_ffi_fn_descr_t` table (name/signature `L,L` etc.). The PackageManager/backend will use it to
   route by name and type-check, with `gmb_ffi_make_*`/`gmb_ffi_get_*` marshalling (copy-in/copy-out,
   `gmb_list_t` lists, `gmb_ffi_closure_t` callbacks, errors → `ON ERROR`).

## Memory contract

- **Input**: strings/lists are valid only during the call (copy them if you need them later).
- **Output**: `gmb_ffi_make_string[_len]` copies → the BASIC value stays valid even if the module
  frees its own buffer (no UAF). `vec_label` reuses a static buffer: that is legal because the
  runtime copies immediately.
- **BYREF**: `args[i].v_ptr` points to the BASIC variable; `gmb_ffi_set_*_out` writes it.

## Verification with `ctest`

```bash
cd native/gmb_runtime_c && cmake --build build -j4 && cd build && ctest -R gmb_ffi_selfcheck
# 11/11 groups PASS (strings \0/UTF-8/>64K, UAF copy-out, BYREF, errors, lists, callbacks)
```
