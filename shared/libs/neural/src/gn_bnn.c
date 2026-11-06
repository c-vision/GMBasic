/*
 * gn_bnn.c — BNN 1-bit: binarizzazione, soglia batch-norm, XNOR-popcount.
 */
#include "gn_bnn.h"
#include <stdlib.h>

int gn_bnn_bytes(int n) { return n <= 0 ? 0 : (n + 7) / 8; }
int gn_bnn_words(int n) { return n <= 0 ? 0 : (n + 63) / 64; }

void gn_bnn_pack(unsigned long long *out, const gn_real *x, int n)
{
    int w, i;
    if (!out || !x || n < 1) return;
    for (w = 0; w < gn_bnn_words(n); w++) out[w] = 0ULL;
    for (i = 0; i < n; i++)
        if (x[i] < 0.0)   /* convenzione bnn-fpga: negativo -> bit 1 */
            out[i >> 6] |= (1ULL << (i & 63));
}

gn_real gn_bnn_thresh(gn_real k, gn_real h)
{
    const gn_real M_INT = 32767.0;
    if (k == 0.0) return (h >= 0.0) ? -M_INT : M_INT;
    return -(h / k);
}

int gn_bnn_xnor_popcount(const unsigned long long *a, const unsigned long long *b, int n)
{
    int w, count = 0;
    if (!a || !b || n < 1) return 0;
    for (w = 0; w < gn_bnn_words(n); w++) {
        unsigned long long x = ~(a[w] ^ b[w]);
        int bits = (w == gn_bnn_words(n) - 1 && (n & 63)) ? (n & 63) : 64;
        unsigned long long mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
        x &= mask;
#if defined(__GNUC__) || defined(__clang__)
        count += __builtin_popcountll(x);
#else
        while (x) { count += (int)(x & 1ULL); x >>= 1; }
#endif
    }
    return count;
}

int gn_bnn_dot(const unsigned long long *a, const unsigned long long *b, int n)
{
    return 2 * gn_bnn_xnor_popcount(a, b, n) - n;
}
