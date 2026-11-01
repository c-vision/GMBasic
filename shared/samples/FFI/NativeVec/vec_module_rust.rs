// ============================================================================
// vec_module_rust.rs — valutazione modulo nativo GMBasic in Rust
// File: samples/FFI/NativeVec/vec_module_rust.rs
//
// Rust NON può `#include` gmb_ffi.h, ma può MIRRORARE le struct con #[repr(C)]
// e esporre `#[no_mangle] extern "C"`. Due livelli:
//
//   LIVELLO 1 (c-abi, usabile OGGI con `DECLARE`):
//     #[no_mangle] pub extern "C" fn vec_add(a: i64, b: i64) -> i64
//     Le stringhe DEVONO usare std::ffi::CString/CStr e ritornare `*const c_char`
//     (malloc o leak statico: GMBasic copia subito, quindi `Box::leak` è accettabile
//     per i sample ma in produzione meglio un free esplicito `vec_free_label`).
//
//   LIVELLO 2 (gmb-native, futuro): mirrorare `gmb_val_t`/`gmb_list_t`/`gmb_fn_t`
//     con #[repr(C)] + `#[repr(C, align(8))]` per il union, e una tabella statica
//     equivalente a `gmb_ffi_module_entry()`. Il C ABI di una struct con union è
//     fragile: Rust vuole Copy; per i valori si passa meglio `*mut gmb_ffi_val_t`.
//
// Build (lib vec nativa):
//   rustc --crate-type cdylib -C link-arg=-undefined -C link-arg=dynamic_lookup \
//         vec_module_rust.rs -o libvec_rust.dylib
// ============================================================================

use std::ffi::{CStr, CString};

// --- Livello 1: funzioni piatte (DECLARE-compatibili) -----------------------

#[no_mangle]
pub extern "C" fn vec_add(a: i64, b: i64) -> i64 {
    a + b
}

#[no_mangle]
pub extern "C" fn vec_scale(a: f64, b: f64) -> f64 {
    a * b
}

#[no_mangle]
pub extern "C" fn vec_isprime(n: i64) -> i64 {
    if n < 2 { return 0; }
    let mut i = 2i64;
    while i * i <= n {
        if n % i == 0 { return 0; }
        i += 1;
    }
    1
}

/// Ritorna una stringa con lifetime statica (copia del runtime GMBasic richiesta).
#[no_mangle]
pub extern "C" fn vec_label(id: i64, name: *const std::os::raw::c_char) -> *const std::os::raw::c_char {
    let n = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };
    // Box::leak = memoria mai liberata; per un modulo reale esporre vec_label_free.
    CString::new(format!("{}:{}", id, n)).unwrap().into_raw()
}

/// Libera una stringa creata da vec_label (per chi vuole evitare leak).
#[no_mangle]
pub unsafe extern "C" fn vec_label_free(s: *mut std::os::raw::c_char) {
    if !s.is_null() {
        let _ = CString::from_raw(s);
    }
}

// --- Livello 2 (gmb-native): mirroring minimo di gmb_ffi.h -------------------
// NOTA: per il marshalling completo servirebbe replicare gmb_val_t (union + kind)
// e gmb_list_t con `#[repr(C)]`. Il punto fragile è la union: Rust la tratta come
// Copy di 8 byte; conviene passare *mut gmb_ffi_val_t invece che by-value. Non
// include `gmb_ffi.h` — è SOLO il mirror per l'ABI, da tenere allineato a mano.

#[repr(C)]
#[derive(Clone, Copy)]
pub struct gmb_ffi_val_mirror {
    pub kind: i32,
    pub pad: i32,
    pub data: u64, // union: int/double/bool/ptr — 8 byte
}

#[no_mangle]
pub extern "C" fn vec_module_entry() -> *const (i32, i32) {
    // placeholder: in una implementazione completa qui si costruirebbe la
    // tabella dei descriptor (come g_vec_fns in C). Vedere vec_module.c.
    std::ptr::null()
}
