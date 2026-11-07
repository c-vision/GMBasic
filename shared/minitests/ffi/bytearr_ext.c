/* bytearr_ext.c — libreria di test FFI per array di BYTE/SBYTE (2026-11-07).
 *
 * ATTENZIONE all'ABI: nello scratch FFI l'elemento di un array BYTE/SBYTE viaggia
 * sempre a **8 byte** (come tutti gli elementi interi), NON a 1 byte. Il nativo
 * riceve quindi `int64_t*` con i valori 0..255 (BYTE) o -128..127 (SBYTE); il
 * copy-back verso l'array BASIC satura ai limiti. Non usare `uint8_t*`.
 *
 * Le funzioni non dichiarano il conteggio: il compilatore BASIC emette le
 * estensioni automatiche `d0` dopo il puntatore (dimensione dell'array).
 */
#include <stdint.h>

double byte_sum(const int64_t *a, int64_t n)
{
    int64_t s = 0;
    for (int64_t i = 0; i < n; i++) s += a[i];
    return (double)s;
}

/* Modifica in place: verifica il copy-back (e la saturazione a 255). */
void byte_inc(int64_t *a, int64_t n)
{
    for (int64_t i = 0; i < n; i++) a[i] += 1;
}

double sbyte_sum(const int64_t *a, int64_t n)
{
    int64_t s = 0;
    for (int64_t i = 0; i < n; i++) s += a[i];
    return (double)s;
}
