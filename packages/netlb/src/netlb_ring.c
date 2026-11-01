/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_ring.c — policy d'affinita': consistent-hash (+vnodes), rendezvous/HRW, Maglev. */
#include "netlb_internal.h"
#include <string.h>

/* ── consistent hashing: anello di vnodes ordinato per hash ── */
static int cmp_node(const void* a, const void* b) {
    uint64_t x = ((const netlb_ringnode*)a)->h, y = ((const netlb_ringnode*)b)->h;
    return x < y ? -1 : (x > y ? 1 : 0);
}
void netlb_ring_build(netlb_pool* p) {
    free(p->ring); p->ring = NULL; p->ring_n = 0;
    int act = 0; for (int i = 0; i < p->n; i++) if (p->be[i].present) act++;
    if (act == 0) return;
    size_t n = (size_t)act * NETLB_VNODES;
    p->ring = (netlb_ringnode*)malloc(n * sizeof(netlb_ringnode));
    if (!p->ring) return;
    size_t k = 0;
    for (int i = 0; i < p->n; i++) if (p->be[i].present) {
        for (int v = 0; v < NETLB_VNODES; v++) {
            uint64_t h = netlb_hash_key(p->be[i].name, strlen(p->be[i].name), (uint64_t)v * 0x9E3779B97F4A7C15ULL + p->seed);
            p->ring[k].h = h; p->ring[k].be = i; k++;
        }
    }
    p->ring_n = k;
    qsort(p->ring, k, sizeof(netlb_ringnode), cmp_node);
}
int netlb_sel_chash(netlb_pool* p, const void* key, size_t len) {
    if (!p->ring || p->ring_n == 0) netlb_ring_build(p);
    if (!p->ring || p->ring_n == 0) return -1;
    uint64_t h = netlb_hash_key(key, len, p->seed);
    size_t lo = 0, hi = p->ring_n;
    while (lo < hi) { size_t m = (lo + hi) / 2; if (p->ring[m].h < h) lo = m + 1; else hi = m; }
    if (lo == p->ring_n) lo = 0;
    for (size_t k = 0; k < p->ring_n; k++) {
        int be = p->ring[(lo + k) % p->ring_n].be;
        if (p->be[be].present && p->be[be].healthy) return be;
    }
    return -1;
}

/* ── rendezvous / HRW: max hash(key, backend) — stateless, minimal disruption ── */
int netlb_sel_hrw(netlb_pool* p, const void* key, size_t len) {
    int best = -1; uint64_t bw = 0;
    for (int i = 0; i < p->n; i++) if (p->be[i].present && p->be[i].healthy) {
        uint64_t w = netlb_hash_key(key, len, p->be[i].hrw);
        if (best < 0 || w > bw) { bw = w; best = i; }
    }
    return best;
}

/* ── Maglev: tabella M slot; ogni backend ha (offset, skip); fill a preferenza ── */
void netlb_maglev_rebuild(netlb_pool* p) {
    p->mt_ok = 0;
    uint32_t M = p->mt_m ? p->mt_m : 65537u;
    p->mt_m = M;
    uint32_t* tab = (uint32_t*)malloc((size_t)M * sizeof(uint32_t));
    if (!tab) return;
    for (uint32_t i = 0; i < M; i++) tab[i] = 0xffffffffu;
    int be[256]; int N = 0;
    for (int i = 0; i < p->n && N < 256; i++) if (p->be[i].present && p->be[i].healthy) be[N++] = i;
    if (N == 0) { free(tab); return; }
    uint32_t off[256], skp[256], nxt[256];
    for (int i = 0; i < N; i++) {
        uint64_t h1 = netlb_hash_key(p->be[be[i]].name, strlen(p->be[be[i]].name), 0x1234ULL);
        uint64_t h2 = netlb_hash_key(p->be[be[i]].name, strlen(p->be[be[i]].name), 0x5678ULL);
        off[i] = (uint32_t)(h1 % M);
        skp[i] = (uint32_t)((h2 % (M - 1)) + 1);
        nxt[i] = 0;
    }
    uint32_t filled = 0;
    while (filled < M) {
        for (int i = 0; i < N && filled < M; i++) {
            uint32_t c = (uint32_t)(((uint64_t)off[i] + (uint64_t)nxt[i] * skp[i]) % M);
            nxt[i]++;
            if (tab[c] == 0xffffffffu) { tab[c] = (uint32_t)be[i]; filled++; }  /* memorizza l'ID backend */
        }
    }
    free(p->mt); p->mt = tab; p->mt_ok = 1;
}
int netlb_sel_maglev(netlb_pool* p, const void* key, size_t len) {
    if (!p->mt_ok || !p->mt) netlb_maglev_rebuild(p);
    if (!p->mt_ok || !p->mt) return -1;
    uint32_t M = p->mt_m;
    uint64_t h = netlb_hash_key(key, len, p->seed);
    uint32_t slot = (uint32_t)(h % M);
    for (uint32_t k = 0; k < M; k++) {
        uint32_t be = p->mt[(slot + k) % M];
        if (be < (uint32_t)p->n && p->be[be].present && p->be[be].healthy) return (int)be;
    }
    return -1;
}
