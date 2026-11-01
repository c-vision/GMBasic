/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * netlb.h — netlb: libreria di load balancing (soluzione gmbnet di gmbsuite).
 *
 * Libreria C99 PURA: nessun thread, nessun socket, nessun I/O. L'host possiede l'I/O e il tempo
 * e PASSA gli eventi (begin/end/probe); netlb scegle solo il backend.
 * Determinismo garantito (hash stabile + RNG seedabile) -> test riproducibili e scelte locali
 * identiche su nodi diversi (HRW/Maglev).
 *
 * Politeness/null-safety: ogni funzione accetta NULL e degrada (nessun crash). */
#ifndef NETLB_H
#define NETLB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Politiche. */
typedef enum {
    NETLB_RR = 0,   /* round robin */
    NETLB_WRR,      /* smooth weighted round robin (nginx) */
    NETLB_RANDOM,   /* random */
    NETLB_LC,       /* least connections (inflight) */
    NETLB_WLC,      /* weighted least connections (inflight/weight) */
    NETLB_LRT,      /* least response time (EWMA latency) */
    NETLB_P2C,      /* power of two choices + EWMA (default moderno) */
    NETLB_CHASH,    /* consistent hashing + virtual nodes (richiede chiave) */
    NETLB_HRW,      /* rendezvous / highest-random-weight (richiede chiave) */
    NETLB_MAGLEV,   /* Maglev (tabella O(1); richiede chiave; netlb_maglev_build) */
    NETLB_JIQ       /* join-idle-queue: idle-first O(1), fallback P2C */
} netlb_policy;

typedef int netlb_backend_id;   /* < 0 = nessun backend */

typedef struct {
    char     name[64];   /* etichetta (per log/test) */
    double   weight;     /* capacita' relativa (default 1.0; <0 -> 1.0) */
    uint32_t cap;        /* capacita' max connessioni (0 = illimitata) */
    int      healthy;    /* stato iniziale (default 1) */
} netlb_backend_spec;

typedef struct netlb_pool netlb_pool;

/* ── vita ── */
netlb_pool*       netlb_pool_new(netlb_policy p, uint64_t seed);
void            netlb_free(netlb_pool*);
int             netlb_pool_ready(const netlb_pool*);

/* ── backend ── */
netlb_backend_id  netlb_add_backend(netlb_pool*, const netlb_backend_spec*);   /* ritorna id, o <0 */
int             netlb_remove_backend(netlb_pool*, netlb_backend_id);         /* draining: no nuovi pick */
size_t          netlb_backend_count(const netlb_pool*);

/* ── config ── */
void            netlb_set_policy(netlb_pool*, netlb_policy);
netlb_policy      netlb_get_policy(const netlb_pool*);
void            netlb_set_weight(netlb_pool*, netlb_backend_id, double w);
void            netlb_set_cap(netlb_pool*, netlb_backend_id, uint32_t cap);   /* 0 = illimitato */
void            netlb_set_slow_start(netlb_pool*, double window_ms);       /* 0 = off (default) */
void            netlb_set_retry_budget(netlb_pool*, double max_ratio);     /* default 0.2 (20%) */
int             netlb_maglev_build(netlb_pool*, uint32_t m);               /* M primo consigliato */
/* HRW gerarchico "skeleton" (Thaler-Ravishankar): O(log n) invece di O(n) per NETLB_HRW.
 * m = dimensione cluster (foglie), f = fanout dell'albero virtuale. 0 = off (HRW piatto, default).
 * Da usare con N molto grande. Il rebuild (add/remove) e' O(n) ma raro; il pick e' O(log n). */
void            netlb_set_hrw_skeleton(netlb_pool*, uint32_t m, uint32_t f);
long            netlb_hrw_hashes(const netlb_pool*);                       /* hash per pick (introspezione) */
/* Sticky soft: affinita' cookie->backend con fallback. Attiva per key!=NULL e ttl>0.
 * Se il backend mappato e' sano e non escluso -> riusato; altrimenti si ri-dispatcha e si
 * ri-mappa. ttl_ms=0 = off (default). */
void            netlb_set_sticky(netlb_pool*, double ttl_ms);

/* ── HOT PATH ── key==NULL -> policy dinamica; key!=NULL -> policy d'affinita'. */
netlb_backend_id  netlb_pick(netlb_pool*, const void* key, size_t keylen);
/* variante con esclusione (retry: evita un backend gia' provato) */
netlb_backend_id  netlb_pick_ex(netlb_pool*, const void* key, size_t keylen, const netlb_backend_id* exclude, int n_exclude);

/* ── EVENTI (contabilita': P2C/LC/EWMA/health/slow-start) ── */
void            netlb_begin(netlb_pool*, netlb_backend_id);
void            netlb_end(netlb_pool*, netlb_backend_id, int ok, double lat_ms);
void            netlb_probe(netlb_pool*, netlb_backend_id, int ok);          /* health attivo */

/* ── salute / retry / introspezione ── */
int             netlb_is_healthy(const netlb_pool*, netlb_backend_id);
int             netlb_should_retry(netlb_pool*, netlb_backend_id, int attempt);  /* budget+jitter */
double          netlb_backend_load(const netlb_pool*, netlb_backend_id);     /* inflight se no-ewma */
double          netlb_backend_ewma(const netlb_pool*, netlb_backend_id);     /* latenza stimata (ms) */
long            netlb_backend_inflight(const netlb_pool*, netlb_backend_id);
uint64_t        netlb_requests(const netlb_pool*);
long            netlb_idle_count(const netlb_pool*);   /* backend idle (coda JIQ) */
int             netlb_sticky_count(const netlb_pool*); /* voci sticky soft correnti */
/* numero di pick serviti dal backend (per i test di distribuzione) */
uint64_t        netlb_pick_count(const netlb_pool*, netlb_backend_id);
/* orologio dell'host (ms monotoni): serve SOLO allo slow-start. La lib non legge il tempo. */
void            netlb_set_now(netlb_pool*, double now_ms);

#ifdef __cplusplus
}
#endif
#endif /* NETLB_H */
