/* ============================================================================
 * GMBasic — Native Module FFI Header (gmb_ffi.h)
 * File: native/gmb_runtime_c/include/gmb_ffi.h
 * Descrizione: ABI C stabile per moduli nativi GMBasic (contentType: c-ffi,
 *              ffi.api: "gmb-native"). NON è un Python.h: nessun GIL, nessun
 *              refcount di valore, marshalling copy-in/copy-out. Il BASIC resta
 *              facile (DECLARE semplice resta il default per librerie arbitrarie);
 *              questo header è SOLO per chi vuole performance/tipi COLLECTIONS.
 *
 * Regole di memoria (contratto, vedi docs FFI):
 *   - Input  : i puntatori stringa/lista sono validi SOLO durante la chiamata;
 *              se il modulo li conserva dopo il return → UAF. Copiare se serve.
 *   - Output : gmb_ffi_make_string/len COPIA in memoria del chiamante (ownership
 *              passa a GMBasic). Mai restituire char* mallocato dal modulo senza
 *              copia (Go/Rust GC spostano).
 *   - Lista  : costruire con gmb_list_new + gmb_list_add; GMBasic ne prende
 *              ownership dopo il ritorno (gmb_list_free).
 *   - Errori : gmb_ffi_error() imposta l'ultimo errore → visibile a BASIC via
 *              ON ERROR (ERR/ERL) come eccezione BASIC, non segfault.
 * ============================================================================ */

#ifndef GMB_FFI_H
#define GMB_FFI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Versione ABI: incrementare su cambiamenti incompatibili di questo header --- */
#define GMB_FFI_VERSION 1

/* --- Tipi ri-esportati (compat: un modulo include solo gmb_ffi.h) --- */
typedef gmb_int_t    gmb_ffi_int_t;
typedef gmb_double_t gmb_ffi_double_t;
typedef gmb_bool_t   gmb_ffi_bool_t;
typedef gmb_string_t gmb_ffi_string_t;
typedef gmb_val_t    gmb_ffi_val_t;
typedef gmb_list_t   gmb_ffi_list_t;
typedef gmb_fn_t     gmb_ffi_closure_t;   /* callback/closure BASIC (gmb_fn_t) */

/* ============================================================================
 * 1. Firma delle funzioni esportate
 *
 *   gmb_ffi_fn_t : riceve l'array degli argomenti (gmb_ffi_val_t) e il conteggio.
 *                  Ritorna un gmb_ffi_val_t (numero/stringa/lista/nil).
 *                  Per output multipli usare i *out* sotto (BYREF di tipo C).
 *
 *   Firme possibili nella signature (per type-checking host):
 *     'L' LONG   'D' DOUBLE   'S' STRING   'B' BOOL
 *     'A' ARRAY/LIST   'P' PTR (BYREF/out)   'C' CALLBACK (gmb_ffi_fn_t)
 * ============================================================================ */
typedef gmb_ffi_val_t (*gmb_ffi_fn_t)(const gmb_ffi_val_t* args, size_t argc);

typedef struct gmb_ffi_fn_descr {
    const char* name;        /* Nome BASIC (es. "VecAdd") — senza namespace */
    gmb_ffi_fn_t fn;         /* Implementazione */
    int min_args;            /* Minimo numero di argomenti */
    int max_args;            /* Massimo, -1 = variadico */
    const char* signature;   /* es. "L,D,D" (LONG, DOUBLE, DOUBLE) */
} gmb_ffi_fn_descr_t;

/* ============================================================================
 * 2. Modulo: l'host cerca in ogni .so/.dylib la entry `gmb_ffi_module_entry`.
 * ============================================================================ */
typedef struct gmb_ffi_module {
    const char* name;                  /* nome pacchetto, es. "vec" */
    int version;                       /* deve valere GMB_FFI_VERSION */
    const gmb_ffi_fn_descr_t* functions; /* array NULL-terminated */
    void (*on_load)(void);             /* opzionale: init risorse */
    void (*on_unload)(void);           /* opzionale: cleanup */
} gmb_ffi_module_t;

gmb_ffi_module_t* gmb_ffi_module_entry(void);

/* ============================================================================
 * 3. Marshalling (safe copy-in/copy-out)
 * ============================================================================ */

/* --- Costruzione valori di ritorno (copia; ownership passa a GMBasic) --- */
gmb_ffi_val_t gmb_ffi_make_int(gmb_ffi_int_t v);
gmb_ffi_val_t gmb_ffi_make_double(gmb_ffi_double_t v);
gmb_ffi_val_t gmb_ffi_make_bool(gmb_ffi_bool_t v);
gmb_ffi_val_t gmb_ffi_make_string(const char* s);            /* strlen; copia */
gmb_ffi_val_t gmb_ffi_make_string_len(const char* s, size_t n); /* copia, anche \0 */
gmb_ffi_val_t gmb_ffi_make_nil(void);

/* --- Callback (closure BASIC → C): boxa in un OBJECT con data="gmb_ffi_closure".
 *   La chiamata dal modulo usa la convenzione standard: cb(ctx, ...). */
gmb_ffi_val_t gmb_ffi_make_closure(gmb_ffi_closure_t c);
gmb_ffi_closure_t gmb_ffi_get_closure(const gmb_ffi_val_t* v);

/* --- Lettura argomenti (validi solo durante la chiamata) --- */
gmb_ffi_int_t    gmb_ffi_get_int(const gmb_ffi_val_t* v, gmb_ffi_int_t def);
gmb_ffi_double_t gmb_ffi_get_double(const gmb_ffi_val_t* v, gmb_ffi_double_t def);
gmb_ffi_bool_t   gmb_ffi_get_bool(const gmb_ffi_val_t* v, gmb_ffi_bool_t def);
/* Ritorna il puntatore alla stringa + lunghezza (NON copia; valida solo nel call). */
const char* gmb_ffi_get_string(const gmb_ffi_val_t* v, size_t* len_out);
gmb_ffi_list_t  gmb_ffi_get_list(const gmb_ffi_val_t* v);

/* --- Output multiplo (BYREF di tipo C): il chiamante passa il puntatore ---
 *   BASIC: DECLARE SUB Foo LIB "m" (x AS LONG, BYREF out AS DOUBLE)
 *   C    : scrive in `out` il valore; il puntatore è garantito valido dal runtime. */
void gmb_ffi_set_double_out(double* out, gmb_ffi_double_t v);
void gmb_ffi_set_int_out(gmb_ffi_int_t* out, gmb_ffi_int_t v);
void gmb_ffi_set_bool_out(gmb_ffi_bool_t* out, gmb_ffi_bool_t v);
/* Stringa OUT: copia s (len) in *out_str (buffer fornito da GMBasic). */
void gmb_ffi_set_string_out(char* out_str, size_t out_cap, const char* s);

/* ============================================================================
 * 4. Errori → visibili a BASIC via ON ERROR (mai segfault dal modulo).
 * ============================================================================ */
void gmb_ffi_error(const char* fmt, ...);
void gmb_ffi_errorv(const char* fmt, va_list ap);
const char* gmb_ffi_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FFI_H */
