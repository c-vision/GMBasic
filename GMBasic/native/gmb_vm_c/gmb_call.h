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

/* 1 = usa libffi (default), 0 = forza il dispatch storico (GMB_FFI_LIBFFI=0). */
int gmb_call_enabled(void);

#ifdef __cplusplus
}
#endif


/* Slot tipizzato per chiamate con struct/UDT per valore (2026-11-06).
 * kind: 0 = double, 1 = int64/puntatore, 2 = struct (nfields campi da 8 byte).
 * Per le struct il valore in vals[i] punta a un buffer di nfields*8 byte con i campi
 * consecutivi (il layout ABI di campi tutti a 8 byte è sequenziale). */
typedef struct gmb_call_slot {
    int kind;
    int nfields;
    const unsigned char* field_is_int;   /* nfields: 1 = int64, 0 = double */
} gmb_call_slot;

int gmb_call_invoke_slots(void* fn, int n, const gmb_call_slot* slots,
                          void* const* vals, int ret_kind,
                          int ret_nfields, const unsigned char* ret_field_is_int,
                          void* ret);

#endif /* GMB_CALL_H */
