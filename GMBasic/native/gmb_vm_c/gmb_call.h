/*
 * gmb_call.h — esecutore di chiamate FFI su libffi.
 *
 * Sostituisce il dispatch generato `GMB_FFI_DISPATCH`/`GMB_FFI_MIXn` (una funzione C per
 * ogni combinazione Int64/Double, 2^N casi) con una singola chiamata a `ffi_call`: la
 * classificazione ABI (registri interi/FP, stack, struct, varargs) la fa libffi, che è la
 * stessa scelta di CPython/ctypes, Ruby Fiddle, PHP FFI.
 *
 * La firma è descritta da un vettore di kind per slot: 1 = intero/puntatore (int64_t),
 * 0 = double. I valori vivono nel buffer del chiamante e `vals[i]` punta al valore dello
 * slot i-esimo (contratto di `ffi_call`).
 */
#ifndef GMB_CALL_H
#define GMB_CALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ritorna 0 = chiamata eseguita, -1 = firma non preparabile (il chiamante può ricadere
 * sul dispatch storico). `ret` punta a un double o a un int64_t secondo `ret_is_int`. */
int gmb_call_invoke(void* fn, int n, const unsigned char* is_int,
                    void* const* vals, int ret_is_int, void* ret);

/* Sempre 1 (libffi): l'ex interruttore via env `GMB_FFI_LIBFFI` è stato rimosso il 2026-11-06
 * (AGENTS: nessuna env per governare il comportamento). Il ripiego sul dispatch storico resta
 * automatico quando `ffi_prep_cif` fallisce. */
int gmb_call_enabled(void);

#ifdef __cplusplus
}
#endif


/* Slot tipizzato per chiamate con struct/UDT per valore (2026-11-06).
 * kind: 0 = double, 1 = int64/puntatore, 2 = struct (nfields campi).
 * Per le struct, `vals[i]` punta a un buffer di `nfields` campi POSIZIONATI secondo il
 * layout C reale: `field_type[f]` è il codice del campo f (0=double 1=int64 2=int8 3=uint8
 * 4=int16 5=uint16 6=int32 7=uint32 8=float32 9=pointer 10=uint64) e `field_off[f]` il suo
 * offset in byte nel buffer. Il chiamante riempie il buffer; libffi ricostruisce la stessa
 * classificazione ABI dai tipi (gli offset coincidono perché entrambi seguono le regole C).
 * `struct_size` è la size totale (per il return, dove il buffer è scritto da libffi). */
typedef struct gmb_call_slot {
    int kind;
    int nfields;
    const unsigned char* field_type;   /* codici 0..10, nfields elementi */
    const int* field_off;              /* offset in byte, nfields elementi */
} gmb_call_slot;

int gmb_call_invoke_slots(void* fn, int n, const gmb_call_slot* slots,
                          void* const* vals, int ret_kind,
                          int ret_nfields, const unsigned char* ret_field_type,
                          const int* ret_field_off, int ret_struct_size,
                          void* ret);

/* ── Closure (callback NATIVA → codice BASIC, 2026-11-11) ──────────────────────────────
 * Una callback FFI è un function pointer che il codice nativo invoca; la sua firma è quella
 * dichiarata in BASIC (`AS FUNCTION(a AS LONG, b AS DOUBLE) AS DOUBLE`), quindi NON è
 * omogenea in generale e può avere arità > 8. Anziché un pool di trampolini pre-compilati
 * (una funzione C per ogni combinazione kind×arità) si usa una closure libffi: il CIF è
 * costruito a runtime dalla firma per-slot e `ffi_prep_closure_loc` genera il codice
 * eseguibile che richiama `handler(user, ret, args)`.
 *
 * `param_is_int[i]` != 0 → parametro i intero/pointer a 8 byte (int64_t), 0 → double.
 * `ret_kind`: 0 = double, 1 = int64_t, 2 = void.
 * `*out_handle` riceve l'handle da passare a gmb_closure_free (il puntatore al codice è il
 * valore di ritorno). Ritorna NULL se la firma non è preparabile. */
void* gmb_closure_create(int n, const unsigned char* param_is_int, int ret_kind,
                         void (*handler)(void* user, void* ret, void** args),
                         void* user, void** out_handle);
void gmb_closure_free(void* handle);

#endif /* GMB_CALL_H */
