# NativeVec — esempio FFI `gmb_ffi.h`

Esempio self-contained del modulo nativo **GMBasic** (`contentType: c-ffi`, `ffi.api: gmb-native`).

## Contenuto

| File | Ruolo |
|---|---|
| `gmb_ffi.h` | Header ABI (`GMB_FFI_VERSION=1`, copia da `native/gmb_runtime_c/include/`) |
| `vec_module.c` | Modulo C: livello 1 (c-abi) + livello 2 (gmb-native `gmb_ffi_module_entry`) |
| `Makefile` | Compila `libvec_module.dylib`/`.so` |
| `NativeVec.bas` | Programma BASIC (usa `DECLARE`, livello 1) |
| `vec_module_rust.rs` | Valutazione modulo Rust equivalente (`#[no_mangle] extern "C"`) |
| `vec_module_go.go` | Valutazione modulo Go equivalente (cgo `//export`) |

## Build & run

```bash
cd samples/Community/FFI/NativeVec
make                 # → libvec_module.dylib (macOS) / libvec_module.so (Linux)
# poi esegui NativeVec.bas (runtime risolve ./NativeVec/vec_module)
```

## I due livelli

1. **c-abi** (`DECLARE`, usato oggi): funzioni C piatte (`vec_add`, `vec_label`, `vec_split`) come simboli normali. Il BASIC dichiara e chiama.
2. **gmb-native** (futuro dispatcher): `gmb_ffi_module_entry()` restituisce la tabella `gmb_ffi_fn_descr_t` (nome/firma `L,L` ecc.). Il PackageManager/backend la userà per route per nome e type-checking, con marshalling `gmb_ffi_make_*`/`gmb_ffi_get_*` (copy-in/copy-out, liste `gmb_list_t`, callback `gmb_ffi_closure_t`, errori→`ON ERROR`).

## Contratto memoria

- **Input**: stringhe/lista valide solo durante la chiamata (copia se servono).
- **Output**: `gmb_ffi_make_string[_len]` copia → il valore BASIC resta valido anche se il modulo libera (niente UAF). `vec_label` riusa un buffer statico: è legale perché il runtime copia subito.
- **BYREF**: `args[i].v_ptr` punta alla variabile BASIC; `gmb_ffi_set_*_out` scrive.

## Verifica con `ctest`

```bash
cd native/gmb_runtime_c && cmake --build build -j4 && cd build && ctest -R gmb_ffi_selfcheck
# 11/11 gruppi PASS (stringhe \0/UTF-8/>64K, UAF copy-out, BYREF, errori, liste, callback)
```
