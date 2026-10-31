# NativeRust — pure Rust FFI for GMBasic (c-abi, without `gmb_ffi.h`)

**Self-contained** Rust module callable from BASIC via `DECLARE`. No GMBasic header required: only
pure C ABI (`#[no_mangle] pub extern "C"`).

## Files

| File | Role |
|---|---|
| `math_rust.rs` | Rust module: `math_add`, `math_scale`, `math_isprime`, `math_factorial`, `math_label`, `math_label_free` |
| `MathRust.bas` | BASIC program using `DECLARE FUNCTION ... LIB "./NativeRust/math_rust"` |
| `build.sh` | `rustc --crate-type cdylib` → `libmath_rust.dylib`/`.so` |

## Build & run

```bash
cd shared/samples/Community/FFI/NativeRust
sh build.sh        # -> libmath_rust.dylib
# then run MathRust.bas
```

## c-abi rules

- **Scalars**: `i64`/`f64` → BASIC `LONG`/`DOUBLE`, mapped directly.
- **OUT strings**: `CString::into_raw()` → `*const c_char`; GMBasic **copies** immediately, but the
  Rust buffer stays allocated → the module must expose a `*_free` (`math_label_free`) that BASIC
  calls (or the module uses a static buffer).
- **IN strings**: `CStr::from_ptr` is valid only during the call — do not keep it.
- **Errors**: Rust has no native C-ABI mechanism; to signal an error return a sentinel (e.g. `-1`)
  plus a `last_error` function (see the C++ pattern `gmb_ffi_error`).
