// ============================================================================
// math_rust.rs — modulo Rust per GMBasic, SOLO c-abi (nessun gmb_ffi.h).
//
// Chiamabile da BASIC con:
//   DECLARE FUNCTION MathAdd LIB "./NativeRust/math_rust" (a AS LONG, b AS LONG) AS LONG
//
// Regole:
//   - funzioni `#[no_mangle] pub extern "C"` = ABI C pura (scalari + char*);
//   - le STRING ritornate sono `*const c_char` allocate con CString: GMBasic le
//     COPIA subito (copy-out), ma il buffer Rust resta allocato → esporre sempre
//     `math_label_free` e chiamarla a fine uso per non fare leak.
//   - input `*const c_char` letto con CStr (mai tenere il puntatore dopo il call).
//
// Build:
//   rustc --crate-type cdylib math_rust.rs -o libmath_rust.dylib
// ============================================================================

use std::ffi::{CStr, CString};

#[no_mangle]
pub extern "C" fn math_add(a: i64, b: i64) -> i64 {
    a + b
}

#[no_mangle]
pub extern "C" fn math_scale(a: f64, b: f64) -> f64 {
    a * b
}

#[no_mangle]
pub extern "C" fn math_isprime(n: i64) -> i64 {
    if n < 2 {
        return 0;
    }
    let mut i = 2i64;
    while i * i <= n {
        if n % i == 0 {
            return 0;
        }
        i += 1;
    }
    1
}

#[no_mangle]
pub extern "C" fn math_factorial(n: i64) -> i64 {
    if n <= 1 {
        return 1;
    }
    let mut r = 1i64;
    let mut k = 1i64;
    while k <= n {
        r = r.saturating_mul(k);
        k += 1;
    }
    r
}

#[no_mangle]
pub extern "C" fn math_label(id: i64, name: *const std::os::raw::c_char) -> *const std::os::raw::c_char {
    let n = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };
    CString::new(format!("{}:{}", id, n)).unwrap().into_raw()
}

/// Libera una stringa creata da math_label (chiamare a fine uso da BASIC via DECLARE).
#[no_mangle]
pub unsafe extern "C" fn math_label_free(s: *mut std::os::raw::c_char) {
    if !s.is_null() {
        drop(CString::from_raw(s));
    }
}
