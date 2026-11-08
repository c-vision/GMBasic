/* crypto_random.c — RANDOMINT / RANDOMDOUBLE / RANDOMNORMAL (CSPRNG). */
#include "gmbcrypto.h"

#include <math.h>
#include <stdint.h>

#include <openssl/rand.h>

int64_t ofx_crypto_random_int(int64_t lo, int64_t hi)
{
    uint64_t r, span;
    if (hi < lo) { int64_t t = lo; lo = hi; hi = t; }
    span = (uint64_t)(hi - lo) + 1;
    if (span == 0) return (int64_t)0; /* intervallo pieno: qualunque int64 */
    if (RAND_bytes((unsigned char*)&r, (int)sizeof r) != 1) return lo;
    return lo + (int64_t)(r % span);
}

double ofx_crypto_random_double(double lo, double hi)
{
    uint64_t r;
    double u;
    if (hi < lo) { double t = lo; lo = hi; hi = t; }
    if (RAND_bytes((unsigned char*)&r, (int)sizeof r) != 1) return lo;
    /* 53 bit di mantissa: uniforme in [0,1) */
    u = (double)(r >> 11) / 9007199254740992.0;
    return lo + (hi - lo) * u;
}

double ofx_crypto_random_normal(double mean, double sd)
{
    static int have_spare = 0;
    static double spare = 0.0;
    uint64_t r;
    double u1, u2, radius;

    if (have_spare) {
        have_spare = 0;
        return mean + sd * spare;
    }
    do {
        if (RAND_bytes((unsigned char*)&r, (int)sizeof r) != 1) return mean;
        u1 = (double)(r >> 11) / 9007199254740992.0;
    } while (u1 <= 1e-15);
    if (RAND_bytes((unsigned char*)&r, (int)sizeof r) != 1) return mean;
    u2 = (double)(r >> 11) / 9007199254740992.0;
    radius = sqrt(-2.0 * log(u1));
    spare = radius * sin(2.0 * M_PI * u2);
    have_spare = 1;
    return mean + sd * (radius * cos(2.0 * M_PI * u2));
}
