/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_sticky.c — sticky soft: affinita' cookie -> backend con fallback.
 *
 * A differenza delle policy d'affinita' pure (CHASH/HRW/Maglev, hash deterministico), qui la
 * mappa e' APPRESA e con TTL: la prima volta una chiave viene dispatchata dalla policy corrente
 * e memorizzata; le successive la ritrovano (finche' il backend e' sano/non escluso e non scade).
 * Se il backend mappato decade, si ri-dispatcha e si ri-mappa (degradazione trasparente).
 *
 * Tabella open-addressing (linear probing) di NETLB_STICKY_MAX voci. Chiavi piu' lunghe di
 * NETLB_STICKY_KEY byte sono troncate (collisioni possibili: sticky e' "soft"). */
#include "netlb_internal.h"
#include <string.h>

#define NETLB_STICKY_SEED 0x9E3779B97F4A7C15ULL

static unsigned st_slot(uint64_t h) { return (unsigned)(h % (uint64_t)NETLB_STICKY_MAX); }

static int st_match(const netlb_sticky* e, uint64_t h, const void* key, size_t len) {
    return e->valid && e->hash == h && e->len == len && memcmp(e->key, key, len) == 0;
}

static int st_expired(netlb_pool* p, const netlb_sticky* e) {
    return p->st_ttl > 0.0 && (p->now_ms - e->t_store) > p->st_ttl;
}

static netlb_sticky* st_ensure(netlb_pool* p) {
    if (!p->st) {
        p->st = (netlb_sticky*)calloc(NETLB_STICKY_MAX, sizeof(netlb_sticky));
        if (!p->st) return NULL;
        p->st_n = 0;
    }
    return p->st;
}

int netlb_sticky_lookup(netlb_pool* p, const void* key, size_t len) {
    if (!p || !p->st || !key || len == 0) return -1;
    if (len > NETLB_STICKY_KEY) len = NETLB_STICKY_KEY;
    uint64_t h = netlb_hash_key(key, len, NETLB_STICKY_SEED);
    unsigned s = st_slot(h);
    for (unsigned k = 0; k < (unsigned)NETLB_STICKY_MAX; k++) {
        netlb_sticky* e = &p->st[(s + k) % NETLB_STICKY_MAX];
        if (!e->valid) return -1;                       /* slot vuoto: fine sequenza di probe */
        if (st_match(e, h, key, len))
            return st_expired(p, e) ? -1 : e->be;       /* scaduta = miss */
    }
    return -1;
}

void netlb_sticky_store(netlb_pool* p, const void* key, size_t len, int be) {
    if (!p || !key || len == 0 || be < 0) return;
    if (!st_ensure(p)) return;
    if (len > NETLB_STICKY_KEY) len = NETLB_STICKY_KEY;
    uint64_t h = netlb_hash_key(key, len, NETLB_STICKY_SEED);
    unsigned s = st_slot(h);
    for (unsigned k = 0; k < (unsigned)NETLB_STICKY_MAX; k++) {
        netlb_sticky* e = &p->st[(s + k) % NETLB_STICKY_MAX];
        if (st_match(e, h, key, len)) { e->be = be; e->t_store = p->now_ms; return; }
        if (!e->valid || st_expired(p, e)) {            /* slot libero o scaduto: riusa */
            if (!e->valid) p->st_n++;
            e->valid = 1; e->hash = h; e->be = be; e->t_store = p->now_ms;
            e->len = len; memcpy(e->key, key, len);
            return;
        }
    }
    /* tabella piena: reset soft (le sessioni ri-mapperanno al prossimo pick). */
    memset(p->st, 0, (size_t)NETLB_STICKY_MAX * sizeof(netlb_sticky));
    p->st_n = 0;
    netlb_sticky_store(p, key, len, be);
}

int netlb_sticky_count(const netlb_pool* p) { return (p && p->st) ? p->st_n : 0; }
long netlb_idle_count(const netlb_pool* p) { return p ? p->q_n : 0; }
