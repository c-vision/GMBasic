/* ============================================================================
 * gmb_bits.h — namespace BITS (manipolazione bit a 64 bit).
 * Unica implementazione condivisa VM/C + C99Native (§0bis):
 * i backend EMETTONO chiamate a queste gmb_bit_* / gmb_gray_* , mai copie inline.
 * Convenzione: input/output gmb_int_t (intero a 64 bit); i predicati ritornano
 * la convenzione Bit del linguaggio (-1 = vero, 0 = falso).
 * ============================================================================ */
#ifndef GMB_BITS_H
#define GMB_BITS_H

#include "gmb_runtime.h"

/* ── Tier 1: operazioni elementari ─────────────────────────────────────────── */
gmb_int_t gmb_bit_count(gmb_int_t x);                        /* popcount 0..64 */
gmb_int_t gmb_bit_get(gmb_int_t x, gmb_int_t n);             /* bit n → 0/1 */
gmb_int_t gmb_bit_set(gmb_int_t x, gmb_int_t n);             /* x | (1<<n) */
gmb_int_t gmb_bit_clear(gmb_int_t x, gmb_int_t n);           /* x & ~(1<<n) */
gmb_int_t gmb_bit_toggle(gmb_int_t x, gmb_int_t n);          /* x ^ (1<<n) */
gmb_int_t gmb_bit_test(gmb_int_t x, gmb_int_t n);            /* Bit: -1/0 */

/* ── Tier 2: campi, rotazioni, codifiche ───────────────────────────────────── */
gmb_int_t gmb_bit_field(gmb_int_t x, gmb_int_t lo, gmb_int_t hi);   /* bit lo..hi inclusi */
gmb_int_t gmb_bit_reverse(gmb_int_t x, gmb_int_t width);            /* inverte i `width` bit bassi */
gmb_int_t gmb_bit_rotate_left(gmb_int_t x, gmb_int_t n, gmb_int_t width);
gmb_int_t gmb_bit_rotate_right(gmb_int_t x, gmb_int_t n, gmb_int_t width);
gmb_int_t gmb_gray_code(gmb_int_t x);                        /* x ^ (x>>1) */
gmb_int_t gmb_from_gray(gmb_int_t x);                        /* inversa del Gray */
gmb_int_t gmb_bit_parity(gmb_int_t x);                       /* 0 = pari, 1 = dispari */
gmb_int_t gmb_bit_scan_fwd(gmb_int_t x);                     /* indice del bit 1 più basso, -1 se 0 */
gmb_int_t gmb_bit_scan_rev(gmb_int_t x);                     /* indice del bit 1 più alto, -1 se 0 */
gmb_int_t gmb_byte_swap(gmb_int_t x);                        /* inverte l'ordine dei 8 byte */

/* ── Tier 3: algoritmi ─────────────────────────────────────────────────────── */
gmb_int_t gmb_hamming_dist(gmb_int_t a, gmb_int_t b);        /* bit diversi */
gmb_int_t gmb_bit_interleave(gmb_int_t a, gmb_int_t b);      /* codice di Morton */
gmb_int_t gmb_bit_deinterleave(gmb_int_t m, gmb_int_t which);/* inversa (which 0/1) */
gmb_int_t gmb_next_pow2(gmb_int_t x);                        /* più piccola potenza di 2 ≥ x */
gmb_int_t gmb_is_pow2(gmb_int_t x);                          /* Bit: -1/0 */

#endif /* GMB_BITS_H */
