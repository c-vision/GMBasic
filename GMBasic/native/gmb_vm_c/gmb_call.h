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

#endif /* GMB_CALL_H */
