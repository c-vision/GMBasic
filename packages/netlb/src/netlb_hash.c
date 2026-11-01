/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_hash.c — hash stabile (splitmix64 + fold) e RNG deterministico. */
#include "netlb_internal.h"

/* splitmix64: buona valanga, veloce, deterministico. */
uint64_t netlb_mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

/* hash di una chiave (bytes) con seed: FNV-1a fold + finalizzazione splitmix. */
uint64_t netlb_hash_key(const void* key, size_t len, uint64_t seed) {
    const unsigned char* p = (const unsigned char*)key;
    uint64_t h = 0xcbf29ce484222325ULL ^ (seed * 0x9E3779B97F4A7C15ULL) ^ (uint64_t)len;
    if (p) for (size_t i = 0; i < len; i++) { h ^= (uint64_t)p[i]; h *= 0x100000001B3ULL; }
    return netlb_mix64(h);
}

/* RNG splitmix64 -> [0,1). */
double netlb_rng01(netlb_pool* p) {
    p->rng += 0x9E3779B97F4A7C15ULL;
    uint64_t z = netlb_mix64(p->rng);
    return (double)(z >> 11) * (1.0 / 9007199254740992.0); /* 2^53 */
}

uint32_t netlb_rng_n(netlb_pool* p, uint32_t n) {
    if (n == 0) return 0;
    uint32_t r = (uint32_t)(netlb_rng01(p) * (double)n);
    return r < n ? r : n - 1;
}
