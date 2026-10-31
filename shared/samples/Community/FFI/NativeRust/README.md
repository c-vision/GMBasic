# NativeRust — FFI Rust puro per GMBasic (c-abi, senza `gmb_ffi.h`)

Modulo Rust **self-contained** chiamabile da BASIC via `DECLARE`. Nessun header
GMBasic richiesto: solo ABI C pura (`#[no_mangle] pub extern "C"`).

## File

| File | Ruolo |
|---|---|
| `math_rust.rs` | Modulo Rust: `math_add`, `math_scale`, `math_isprime`, `math_factorial`, `math_label`, `math_label_free` |
| `MathRust.bas` | Programma BASIC che usa `DECLARE FUNCTION ... LIB "./NativeRust/math_rust"` |
| `build.sh` | `rustc --crate-type cdylib` → `libmath_rust.dylib`/`.so` |

## Build & run

```bash
cd samples/Community/FFI/NativeRust
sh build.sh        # → libmath_rust.dylib
# poi esegui MathRust.bas
```

## Regole c-abi

- **Scalari**: `i64`/`f64` → `LONG`/`DOUBLE` BASIC, mappati direttamente.
- **Stringhe OUT**: `CString::into_raw()` → `*const c_char`; GMBasic **copia** subito,
  ma il buffer Rust resta allocato → il modulo deve esporre una `*_free`
  (`math_label_free`) che il BASIC chiama (o il modulo usa un buffer statico).
- **Stringhe IN**: `CStr::from_ptr` valido solo durante la chiamata — non conservarlo.
- **Errori**: Rust non ha un meccanismo C-ABI nativo; per errore ritornare un
  sentinella (es. `-1`) + funzione `last_error` (vedi pattern C++ `gmb_ffi_error`).
