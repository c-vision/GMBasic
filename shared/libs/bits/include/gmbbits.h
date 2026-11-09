/* gmbbits.h — API della lib `bits` (shared/libs/bits).
 *
 * Manipolazione bit a 64 bit: stesse semantiche del dominio BITS storico
 * (indici 0-based con 0 = LSB; n fuori [0,63] non altera x; width clampato a 64;
 * predicati con convenzione Bit: -1 vero / 0 falso).
 *
 * La lib è self-contained (solo stdint): nessuna dipendenza esterna.
 * Simboli con prefisso ofx_bits_ per non collidere col runtime.
 */
#ifndef GMBBITS_H
#define GMBBITS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t gmb_int_t;

gmb_int_t ofx_bits_count(gmb_int_t x);
gmb_int_t ofx_bits_get(gmb_int_t x, gmb_int_t n);
gmb_int_t ofx_bits_set(gmb_int_t x, gmb_int_t n);
gmb_int_t ofx_bits_clear(gmb_int_t x, gmb_int_t n);
gmb_int_t ofx_bits_toggle(gmb_int_t x, gmb_int_t n);
gmb_int_t ofx_bits_test(gmb_int_t x, gmb_int_t n);
gmb_int_t ofx_bits_field(gmb_int_t x, gmb_int_t lo, gmb_int_t hi);
gmb_int_t ofx_bits_reverse(gmb_int_t x, gmb_int_t width);
gmb_int_t ofx_bits_rotl(gmb_int_t x, gmb_int_t n, gmb_int_t width);
gmb_int_t ofx_bits_rotr(gmb_int_t x, gmb_int_t n, gmb_int_t width);
gmb_int_t ofx_bits_gray(gmb_int_t x);
gmb_int_t ofx_bits_fromgray(gmb_int_t x);
gmb_int_t ofx_bits_parity(gmb_int_t x);
gmb_int_t ofx_bits_scanfwd(gmb_int_t x);
gmb_int_t ofx_bits_scanrev(gmb_int_t x);
gmb_int_t ofx_bits_byteswap(gmb_int_t x);
gmb_int_t ofx_bits_hamming(gmb_int_t a, gmb_int_t b);
gmb_int_t ofx_bits_interleave(gmb_int_t a, gmb_int_t b);
gmb_int_t ofx_bits_deinterleave(gmb_int_t m, gmb_int_t which);
gmb_int_t ofx_bits_nextpow2(gmb_int_t x);
gmb_int_t ofx_bits_ispow2(gmb_int_t x);

#ifdef __cplusplus
}
#endif

#endif /* GMBBITS_H */
