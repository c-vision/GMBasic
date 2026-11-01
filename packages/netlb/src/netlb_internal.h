/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_internal.h — strutture interne di netlb (non pubbliche). */
#ifndef NETLB_INTERNAL_H
#define NETLB_INTERNAL_H
#include "netlb.h"
#include <stdlib.h>

#define NETLB_VNODES 128          /* vnodes per backend nel consistent-hash */
#define NETLB_EWMA_TAU 10.0       /* costante di tempo (campioni) della EWMA di latenza */
#define NETLB_OUTLIER_CONSEC 3    /* fallimenti consecutivi -> eiezione passiva */
#define NETLB_OUTLIER_EWMA_MS 5000.0
#define NETLB_SLOWSTART_CALLS 1000.0 /* usato se ss_window==0 ma slow-start attivo su conteggio */
#define NETLB_STICKY_MAX 1024     /* voci della tabella sticky soft (open addressing) */
#define NETLB_STICKY_KEY 64       /* lunghezza massima della chiave sticky (byte, oltre: troncata) */

typedef struct {
    char     name[64];
    double   weight;         /* capacita' relativa */
    double   eff_weight;     /* peso corrente (smooth WRR / slow-start) */
    uint32_t cap;            /* max connessioni (0 = illimitata) */
    int      present;        /* 1 = in rotazione (0 = rimosso/draining) */
    int      healthy;        /* stato di salute corrente */
    uint64_t hrw;            /* hash stabile del backend (HRW/ring) */

    long     inflight;       /* connessioni in corso */
    double   ewma_lat;       /* latenza stimata (ms); 0 = non ancora misurata */
    int      measured;       /* 1 se ewma_lat e' valida */
    double   curw;           /* smooth WRR: credito corrente */
    int      consec_fail;    /* fallimenti consecutivi (passivo) */
    double   t_added;        /* orologio host all'aggiunta (slow-start) */

    uint64_t picks;          /* statistica: n. di pick serviti */

    /* coda idle (JIQ): lista intruse doppiamente collegata, O(1) push/remove/pop */
    int      q_prev;         /* indice precedente, -1 = testa */
    int      q_next;         /* indice successivo, -1 = coda */
    int      q_in;           /* 1 = presente nella coda idle */
} netlb_backend;

/* 1 = il backend ha capacita' residua (limite pending: cap==0 = illimitata). */
static inline int netlb_has_room(const netlb_backend* b) {
    return b->cap == 0 || (uint32_t)b->inflight < b->cap;
}

typedef struct { uint64_t h; int be; } netlb_ringnode;

/* HRW gerarchico (skeleton): albero virtuale di fanout f su cluster di m backend.
 * I cluster sono indicizzati per id STABILE (`id/m`): rimuovere un backend tocca solo il suo
 * cluster (disruption quasi-1/N). clu_cnt/clu_pref danno l'occupanza per saltare i sottoalberi
 * vuoti durante la discesa. */
typedef struct {
    uint32_t m, f;
    int      C;            /* numero di cluster = ceil((max_id+1)/m) */
    int      height;       /* più piccolo H con f^H >= C */
    int*     clu_cnt;      /* clu_cnt[c] = n. backend present nel cluster c */
    int*     clu_pref;     /* prefix sum (size C+1) */
    unsigned long hashes;  /* contatore hash (test O(log n)) */
} netlb_skel;

/* voce sticky soft: mappa una chiave (cookie) al backend scelto, con scadenza. */
typedef struct {
    uint64_t hash;           /* hash della chiave (0 = slot vuoto) */
    int      be;             /* backend mappato */
    int      valid;          /* 1 = voce usata */
    double   t_store;        /* orologio host all'inserimento (TTL) */
    size_t   len;            /* lunghezza chiave */
    char     key[NETLB_STICKY_KEY];
} netlb_sticky;

struct netlb_pool {
    netlb_policy   policy;
    uint64_t     seed;
    uint64_t     rng;          /* stato RNG (splitmix64) */

    netlb_backend* be;
    int          n, cap;

    uint64_t     rr;           /* cursore round-robin */
    double       now_ms;       /* orologio host (netlb_set_now) */
    double       ss_window;    /* slow-start: finestra (ms); 0 = off */

    double       retry_ratio;  /* budget di retry (frazione) */
    long         attempts;     /* tentativi totali */
    long         retries;      /* retry effettuati */

    netlb_ringnode* ring;        /* consistent hash (ordinato per h) */
    size_t        ring_n;

    uint32_t*    mt;           /* Maglev */
    uint32_t     mt_m;
    int          mt_ok;

    int          q_head, q_tail;   /* coda idle (JIQ): testa/coda (-1 = vuota) */
    long         q_n;              /* numero di backend nella coda idle */

    netlb_sticky*  st;               /* tabella sticky soft (lazy alloc) */
    double       st_ttl;           /* TTL sticky (ms); 0 = off */
    int          st_n;             /* voci valide nella tabella sticky */

    netlb_skel*    skel;             /* HRW gerarchico (lazy alloc); NULL = HRW piatto */
};

/* Helper INTERNI: visibilita' nascosta nella dylib (esporta solo l'API pubblica
 * netlb.h e i simboli FFI = nomi dei membri). */
#pragma GCC visibility push(hidden)

/* hash: splitmix64 + hash chiave (deterministico). */
uint64_t netlb_mix64(uint64_t x);
uint64_t netlb_hash_key(const void* key, size_t len, uint64_t seed);
double   netlb_rng01(netlb_pool* p);          /* [0,1) */
uint32_t netlb_rng_n(netlb_pool* p, uint32_t n);

/* helper condivisi */
int          netlb_active_count(netlb_pool* p);          /* present && healthy */
double       netlb_ss_factor(const netlb_pool* p, const netlb_backend* b);  /* slow-start 0..1 */
double       netlb_load_of(const netlb_pool* p, int i);  /* metrica per LC/P2C */
void         netlb_ring_build(netlb_pool* p);
void         netlb_maglev_rebuild(netlb_pool* p);

/* coda idle (JIQ) — mantenuta sempre (O(1)); usata dalla policy NETLB_JIQ. */
void netlb_idle_push(netlb_pool*, int i);
void netlb_idle_remove(netlb_pool*, int i);

/* sticky soft: lookup (>=0 backend, -1 miss/scaduto/non eleggibile) e store. */
int  netlb_sticky_lookup(netlb_pool*, const void* key, size_t len);
void netlb_sticky_store(netlb_pool*, const void* key, size_t len, int be);

/* HRW gerarchico: rebuild dell'albero e selezione O(log n). */
void netlb_skel_build(netlb_pool*);
int  netlb_sel_hrw_skel(netlb_pool*, const void* key, size_t len);

/* selection per policy (ritornano l'indice o -1); ex/nex = esclusione (retry) */
int netlb_sel_rr(netlb_pool*, const netlb_backend_id* ex, int nex);
int netlb_sel_wrr(netlb_pool*, const netlb_backend_id* ex, int nex);
int netlb_sel_random(netlb_pool*, const netlb_backend_id* ex, int nex);
int netlb_sel_lc(netlb_pool*, int weighted, const netlb_backend_id* ex, int nex);
int netlb_sel_lrt(netlb_pool*, const netlb_backend_id* ex, int nex);
int netlb_sel_p2c(netlb_pool*, const netlb_backend_id* ex, int nex);
int netlb_sel_chash(netlb_pool*, const void* key, size_t len);
int netlb_sel_hrw(netlb_pool*, const void* key, size_t len);
int netlb_sel_maglev(netlb_pool*, const void* key, size_t len);
int netlb_sel_jiq(netlb_pool*, const netlb_backend_id* ex, int nex);
int netlb_dispatch(netlb_pool*, const void* key, size_t len, const netlb_backend_id* ex, int nex);

#pragma GCC visibility pop

#endif
