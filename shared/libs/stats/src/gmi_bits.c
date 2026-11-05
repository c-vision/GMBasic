/* ============================================================================
 * gmi_bits.c — namespace BITS (manipolazione bit a 64 bit).
 * Implementazione UNICA del dominio (§0bis): VM/C, C99Backend e C99Native
 * chiamano queste funzioni, mai copie inline. I backend gestiti (IL, PCodeVM)
 * hanno la controparte in C# (BitsFunctions.cs), con la STESSA semantica.
 *
 * Semantica fissata (documentata nel manuale e nei MiniTest):
 *   - indici 0-based, 0 = LSB;
 *   - n fuori [0,63] → l'operazione non altera x (bit_get/test → 0);
 *   - width <= 0 o > 64 → clampato a 64;
 *   - predicati → convenzione Bit del linguaggio (-1 vero, 0 falso);
 *   - nessun risultato silenzioso: i casi limite sono DEFINITI qui.
 * ============================================================================ */
#include "gmi_bits.h"

static int bits_in_range(gmb_int_t n) { return n >= 0 && n < 64; }

/* ── Tier 1 ────────────────────────────────────────────────────────────────── */

gmb_int_t gmi_bit_count(gmb_int_t x)
{
    uint64_t v = (uint64_t)x;
    int c = 0;
    while (v) { v &= (v - 1); c++; }          /* Kernighan: una iterazione per bit a 1 */
    return (gmb_int_t)c;
}

gmb_int_t gmi_bit_get(gmb_int_t x, gmb_int_t n)
{
    if (!bits_in_range(n)) return 0;
    return (gmb_int_t)((((uint64_t)x) >> (unsigned)n) & 1u);
}

gmb_int_t gmi_bit_set(gmb_int_t x, gmb_int_t n)
{
    if (!bits_in_range(n)) return x;
    return (gmb_int_t)(((uint64_t)x) | (1ull << (unsigned)n));
}

gmb_int_t gmi_bit_clear(gmb_int_t x, gmb_int_t n)
{
    if (!bits_in_range(n)) return x;
    return (gmb_int_t)(((uint64_t)x) & ~(1ull << (unsigned)n));
}

gmb_int_t gmi_bit_toggle(gmb_int_t x, gmb_int_t n)
{
    if (!bits_in_range(n)) return x;
    return (gmb_int_t)(((uint64_t)x) ^ (1ull << (unsigned)n));
}

gmb_int_t gmi_bit_test(gmb_int_t x, gmb_int_t n)
{
    if (!bits_in_range(n)) return 0;
    return ((((uint64_t)x) >> (unsigned)n) & 1u) ? -1 : 0;
}

/* ── Tier 2 ────────────────────────────────────────────────────────────────── */

/* BITFIELD(x, lo, hi): bit da lo a hi INCLUSI, allineati a destra.
 * lo/hi invertiti vengono scambiati; fuori [0,63] vengono clampati; se lo > hi → 0. */
gmb_int_t gmi_bit_field(gmb_int_t x, gmb_int_t lo, gmb_int_t hi)
{
    if (lo < 0) lo = 0;
    if (hi > 63) hi = 63;
    if (lo > hi) { gmb_int_t t = lo; lo = hi; hi = t; }
    if (lo > hi || hi < 0 || lo > 63) return 0;
    unsigned width = (unsigned)(hi - lo + 1);
    uint64_t mask = (width >= 64) ? ~0ull : ((1ull << width) - 1ull);
    return (gmb_int_t)((((uint64_t)x) >> (unsigned)lo) & mask);
}

/* BITREVERSE(x, width): inverte l'ordine dei `width` bit bassi (il resto resta 0). */
gmb_int_t gmi_bit_reverse(gmb_int_t x, gmb_int_t width)
{
    if (width <= 0) return 0;
    if (width > 64) width = 64;
    uint64_t v = (uint64_t)x;
    uint64_t r = 0;
    for (gmb_int_t i = 0; i < width; i++) {
        r = (r << 1) | (v & 1ull);
        v >>= 1;
    }
    return (gmb_int_t)r;
}

gmb_int_t gmi_bit_rotate_left(gmb_int_t x, gmb_int_t n, gmb_int_t width)
{
    if (width <= 0) return 0;
    if (width > 64) width = 64;
    if (n < 0) n = 0;
    unsigned w = (unsigned)width;
    unsigned k = (unsigned)(n % width);
    uint64_t mask = (w >= 64) ? ~0ull : ((1ull << w) - 1ull);
    uint64_t v = ((uint64_t)x) & mask;
    if (k == 0) return (gmb_int_t)v;
    return (gmb_int_t)(((v << k) | (v >> (w - k))) & mask);
}

gmb_int_t gmi_bit_rotate_right(gmb_int_t x, gmb_int_t n, gmb_int_t width)
{
    if (width <= 0) return 0;
    if (width > 64) width = 64;
    if (n < 0) n = 0;
    unsigned w = (unsigned)width;
    unsigned k = (unsigned)(n % width);
    uint64_t mask = (w >= 64) ? ~0ull : ((1ull << w) - 1ull);
    uint64_t v = ((uint64_t)x) & mask;
    if (k == 0) return (gmb_int_t)v;
    return (gmb_int_t)(((v >> k) | (v << (w - k))) & mask);
}

gmb_int_t gmi_gray_code(gmb_int_t x) { return (gmb_int_t)(((uint64_t)x) ^ (((uint64_t)x) >> 1)); }

gmb_int_t gmi_from_gray(gmb_int_t x)
{
    uint64_t v = (uint64_t)x;
    v ^= v >> 1; v ^= v >> 2; v ^= v >> 4; v ^= v >> 8; v ^= v >> 16; v ^= v >> 32;
    return (gmb_int_t)v;
}

gmb_int_t gmi_bit_parity(gmb_int_t x) { return (gmb_int_t)(gmi_bit_count(x) & 1); }

gmb_int_t gmi_bit_scan_fwd(gmb_int_t x)
{
    uint64_t v = (uint64_t)x;
    if (v == 0) return -1;
    gmb_int_t i = 0;
    while ((v & 1ull) == 0) { v >>= 1; i++; }
    return i;
}

gmb_int_t gmi_bit_scan_rev(gmb_int_t x)
{
    uint64_t v = (uint64_t)x;
    if (v == 0) return -1;
    gmb_int_t i = 63;
    while ((v & (1ull << 63)) == 0) { v <<= 1; i--; }
    return i;
}

gmb_int_t gmi_byte_swap(gmb_int_t x)
{
    uint64_t v = (uint64_t)x;
    return (gmb_int_t)(((v & 0x00000000000000FFull) << 56) |
                       ((v & 0x000000000000FF00ull) << 40) |
                       ((v & 0x0000000000FF0000ull) << 24) |
                       ((v & 0x00000000FF000000ull) << 8)  |
                       ((v & 0x000000FF00000000ull) >> 8)  |
                       ((v & 0x0000FF0000000000ull) >> 24) |
                       ((v & 0x00FF000000000000ull) >> 40) |
                       ((v & 0xFF00000000000000ull) >> 56));
}

/* ── Tier 3 ────────────────────────────────────────────────────────────────── */

gmb_int_t gmi_hamming_dist(gmb_int_t a, gmb_int_t b)
{
    return gmi_bit_count((gmb_int_t)(((uint64_t)a) ^ ((uint64_t)b)));
}

/* BITINTERLEAVE(a, b): codice di Morton — bit di a in posizioni pari, bit di b in dispari. */
gmb_int_t gmi_bit_interleave(gmb_int_t a, gmb_int_t b)
{
    uint64_t ua = (uint64_t)a, ub = (uint64_t)b, r = 0;
    for (unsigned i = 0; i < 32; i++) {
        r |= ((ua >> i) & 1ull) << (2u * i);
        r |= ((ub >> i) & 1ull) << (2u * i + 1u);
    }
    return (gmb_int_t)r;
}

/* BITDEINTERLEAVE(m, which): which=0 → i bit pari, which=1 → i bit dispari. */
gmb_int_t gmi_bit_deinterleave(gmb_int_t m, gmb_int_t which)
{
    uint64_t v = (uint64_t)m, r = 0;
    unsigned off = (which != 0) ? 1u : 0u;
    for (unsigned i = 0; i < 32; i++)
        r |= ((v >> (2u * i + off)) & 1ull) << i;
    return (gmb_int_t)r;
}

gmb_int_t gmi_next_pow2(gmb_int_t x)
{
    if (x <= 1) return 1;
    uint64_t v = (uint64_t)(x - 1);
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v |= v >> 32;
    return (gmb_int_t)(v + 1);
}

gmb_int_t gmi_is_pow2(gmb_int_t x)
{
    if (x <= 0) return 0;
    uint64_t v = (uint64_t)x;
    return ((v & (v - 1)) == 0) ? -1 : 0;
}
