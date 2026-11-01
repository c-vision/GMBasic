/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_pool.c — pool di backend, eventi (begin/end/probe), salute, slow-start, retry, dispatch. */
#include "netlb_internal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ── vita ── */
netlb_pool* netlb_pool_new(netlb_policy pol, uint64_t seed) {
    netlb_pool* p = (netlb_pool*)calloc(1, sizeof *p);
    if (!p) return NULL;
    p->policy = pol;
    p->seed = seed ? seed : 0x123456789ABCDEF0ULL;
    p->rng = p->seed;
    p->retry_ratio = 0.2;
    p->q_head = p->q_tail = -1;
    p->cap = 8;
    p->be = (netlb_backend*)calloc((size_t)p->cap, sizeof(netlb_backend));
    if (!p->be) { free(p); return NULL; }
    return p;
}
void netlb_free(netlb_pool* p) {
    if (!p) return;
    if (p->skel) { free(p->skel->clu_cnt); free(p->skel->clu_pref); free(p->skel); }
    free(p->be); free(p->ring); free(p->mt); free(p->st); free(p);
}

int netlb_active_count(netlb_pool* p) {
    if (!p) return 0;
    int c = 0; for (int i = 0; i < p->n; i++) if (p->be[i].present && p->be[i].healthy) c++;
    return c;
}
int netlb_pool_ready(const netlb_pool* p) { return p && netlb_active_count((netlb_pool*)p) > 0; }

/* ── backend ── */
netlb_backend_id netlb_add_backend(netlb_pool* p, const netlb_backend_spec* s) {
    if (!p || !s) return -1;
    if (p->n == p->cap) {
        int nc = p->cap ? p->cap * 2 : 8;
        netlb_backend* nb = (netlb_backend*)realloc(p->be, (size_t)nc * sizeof(netlb_backend));
        if (!nb) return -1;
        memset(nb + p->cap, 0, (size_t)(nc - p->cap) * sizeof(netlb_backend));
        p->be = nb; p->cap = nc;
    }
    int i = p->n++;
    netlb_backend* b = &p->be[i];
    memset(b, 0, sizeof *b);
    snprintf(b->name, sizeof b->name, "%s", s->name[0] ? s->name : "be");
    b->weight = (s->weight > 0.0) ? s->weight : 1.0;
    b->eff_weight = b->weight;
    b->cap = s->cap;
    b->present = 1;
    b->healthy = s->healthy ? 1 : 0;
    b->hrw = netlb_hash_key(b->name, strlen(b->name), 0x51ED270BULL);
    b->t_added = p->now_ms;
    b->q_prev = b->q_next = -1;
    if (b->present && b->healthy) netlb_idle_push(p, i);   /* nuovo backend: idle */
    if (p->policy == NETLB_CHASH) netlb_ring_build(p);
    if (p->skel) netlb_skel_build(p);
    p->mt_ok = 0;
    return i;
}

int netlb_remove_backend(netlb_pool* p, netlb_backend_id id) {
    if (!p || id < 0 || id >= p->n) return -1;
    p->be[id].present = 0;   /* draining: le richieste in corso restano, niente nuovi pick */
    netlb_idle_remove(p, id);
    if (p->policy == NETLB_CHASH) netlb_ring_build(p);
    if (p->skel) netlb_skel_build(p);
    p->mt_ok = 0;
    return 0;
}

size_t netlb_backend_count(const netlb_pool* p) { return p ? (size_t)p->n : 0; }

/* ── config ── */
void netlb_set_policy(netlb_pool* p, netlb_policy pol) {
    if (!p) return;
    p->policy = pol;
    if (pol == NETLB_CHASH) netlb_ring_build(p);
    if (pol == NETLB_MAGLEV) netlb_maglev_rebuild(p);
}
netlb_policy netlb_get_policy(const netlb_pool* p) { return p ? p->policy : NETLB_RR; }

void netlb_set_weight(netlb_pool* p, netlb_backend_id id, double w) {
    if (!p || id < 0 || id >= p->n) return;
    p->be[id].weight = (w > 0.0) ? w : 1.0;
    p->be[id].eff_weight = p->be[id].weight;
    p->mt_ok = 0;
}
void netlb_set_cap(netlb_pool* p, netlb_backend_id id, uint32_t cap) {
    if (!p || id < 0 || id >= p->n) return;
    p->be[id].cap = cap;
}
void netlb_set_slow_start(netlb_pool* p, double window_ms) { if (p) p->ss_window = window_ms > 0.0 ? window_ms : 0.0; }
void netlb_set_sticky(netlb_pool* p, double ttl_ms) { if (p) p->st_ttl = ttl_ms > 0.0 ? ttl_ms : 0.0; }
void netlb_set_retry_budget(netlb_pool* p, double max_ratio) { if (p) p->retry_ratio = (max_ratio >= 0.0 && max_ratio <= 1.0) ? max_ratio : 0.2; }
void netlb_set_now(netlb_pool* p, double now_ms) { if (p) p->now_ms = now_ms; }

int netlb_maglev_build(netlb_pool* p, uint32_t m) {
    if (!p || m < 16) return -1;
    p->mt_m = m;
    netlb_maglev_rebuild(p);
    return p->mt_ok ? 0 : -1;
}

/* ── eventi ── */
void netlb_begin(netlb_pool* p, netlb_backend_id id) {
    if (p && id >= 0 && id < p->n) { p->be[id].inflight++; netlb_idle_remove(p, id); }  /* non piu' idle */
}
void netlb_end(netlb_pool* p, netlb_backend_id id, int ok, double lat_ms) {
    if (!p || id < 0 || id >= p->n) return;
    netlb_backend* b = &p->be[id];
    if (b->inflight > 0) b->inflight--;
    if (lat_ms > 0.0) {
        double a = 1.0 / NETLB_EWMA_TAU;
        if (!b->measured) { b->ewma_lat = lat_ms; b->measured = 1; }
        else b->ewma_lat = a * lat_ms + (1.0 - a) * b->ewma_lat;
    }
    if (ok) b->consec_fail = 0;
    else if (++b->consec_fail >= NETLB_OUTLIER_CONSEC) b->healthy = 0;  /* eiezione passiva (rientro via netlb_probe) */
    /* torna idle (nessuna richiesta in corso e sano) -> in coda JIQ */
    if (b->inflight <= 0 && b->present && b->healthy) netlb_idle_push(p, id);
    else netlb_idle_remove(p, id);
    p->attempts++;
}
void netlb_probe(netlb_pool* p, netlb_backend_id id, int ok) {
    if (!p || id < 0 || id >= p->n) return;
    netlb_backend* b = &p->be[id];
    if (ok) { b->healthy = 1; b->consec_fail = 0; }
    else    { b->healthy = 0; }
    if (b->present && b->healthy && b->inflight <= 0) netlb_idle_push(p, id);
    else netlb_idle_remove(p, id);
}

int netlb_is_healthy(const netlb_pool* p, netlb_backend_id id) {
    return (p && id >= 0 && id < p->n) ? (p->be[id].present && p->be[id].healthy) : 0;
}

/* ── metrica di carico: attesa stimata (ms) = inflight * latenza (o inflight se non misurata) ── */
double netlb_load_of(const netlb_pool* p, int i) {
    const netlb_backend* b = &p->be[i];
    double lat = (b->measured && b->ewma_lat > 0.0) ? b->ewma_lat : 1.0;
    return ((double)b->inflight + 1.0) * lat;   /* +1 evita preferenza per inflight=0 non misurato */
}

double netlb_backend_load(const netlb_pool* p, netlb_backend_id id) { return (p && id >= 0 && id < p->n) ? netlb_load_of(p, id) : 0.0; }
double netlb_backend_ewma(const netlb_pool* p, netlb_backend_id id) { return (p && id >= 0 && id < p->n && p->be[id].measured) ? p->be[id].ewma_lat : -1.0; }
long   netlb_backend_inflight(const netlb_pool* p, netlb_backend_id id) { return (p && id >= 0 && id < p->n) ? p->be[id].inflight : -1; }
uint64_t netlb_pick_count(const netlb_pool* p, netlb_backend_id id) { return (p && id >= 0 && id < p->n) ? p->be[id].picks : 0; }
uint64_t netlb_requests(const netlb_pool* p) { return p ? (uint64_t)p->attempts : 0; }

/* ── retry: budget (frazione di retry sul totale) + jitter (nessun I/O qui) ── */
int netlb_should_retry(netlb_pool* p, netlb_backend_id id, int attempt) {
    (void)id;
    if (!p || attempt <= 0) return attempt > 0 ? 0 : 0;
    if (attempt > 5) return 0;                                  /* hard cap */
    double used = (p->attempts > 0) ? (double)p->retries / (double)p->attempts : 0.0;
    if (used >= p->retry_ratio) return 0;                       /* budget esaurito */
    p->retries++;
    return 1;
}

/* ── slow-start: fattore 0..1 (ramp lineare sulla finestra) ── */
double netlb_ss_factor(const netlb_pool* p, const netlb_backend* b) {
    if (p->ss_window <= 0.0) return 1.0;
    double dt = p->now_ms - b->t_added;
    if (dt <= 0.0) return 0.0;
    double f = dt / p->ss_window;
    return f >= 1.0 ? 1.0 : f;
}

/* ── esclusione (retry) ── */
static int is_excluded(const netlb_backend_id* ex, int nex, int i) {
    for (int k = 0; k < nex; k++) if (ex[k] == i) return 1;
    return 0;
}
static int eligible(netlb_pool* p, int i, const netlb_backend_id* ex, int nex) {
    return p->be[i].present && p->be[i].healthy && netlb_has_room(&p->be[i]) && !is_excluded(ex, nex, i);
}

/* ── dispatch ── */
int netlb_dispatch(netlb_pool* p, const void* key, size_t len, const netlb_backend_id* ex, int nex) {
    if (!p || p->n == 0) return -1;
    if (netlb_active_count(p) == 0) return -1;
    /* fallback se l'esclusione svuota gli eleggibili: ignora l'esclusione */
    const netlb_backend_id* e2 = ex; int n2 = nex;
    int anyfree = 0;
    for (int i = 0; i < p->n; i++) if (eligible(p, i, ex, nex)) { anyfree = 1; break; }
    if (!anyfree) { e2 = NULL; n2 = 0; }
    /* sticky soft (overlay): se il cookie e' mappato a un backend ancora eleggibile, riusalo. */
    if (p->st_ttl > 0.0 && key && len > 0) {
        int sb = netlb_sticky_lookup(p, key, len);
        if (sb >= 0 && eligible(p, sb, ex, nex)) { p->be[sb].picks++; return sb; }
    }
    int idx = -1;
    switch (p->policy) {
        case NETLB_RR:     idx = netlb_sel_rr(p, e2, n2); break;
        case NETLB_WRR:    idx = netlb_sel_wrr(p, e2, n2); break;
        case NETLB_RANDOM: idx = netlb_sel_random(p, e2, n2); break;
        case NETLB_LC:     idx = netlb_sel_lc(p, 0, e2, n2); break;
        case NETLB_WLC:    idx = netlb_sel_lc(p, 1, e2, n2); break;
        case NETLB_LRT:    idx = netlb_sel_lrt(p, e2, n2); break;
        case NETLB_P2C:    idx = netlb_sel_p2c(p, e2, n2); break;
        case NETLB_CHASH:  idx = key ? netlb_sel_chash(p, key, len) : netlb_sel_rr(p, e2, n2); break;
        case NETLB_HRW:
            if (key && p->skel) { idx = netlb_sel_hrw_skel(p, key, len);
                                  if (idx < 0) idx = netlb_sel_hrw(p, key, len); }   /* fallback piatto */
            else idx = key ? netlb_sel_hrw(p, key, len) : netlb_sel_rr(p, e2, n2);
            break;
        case NETLB_MAGLEV: idx = key ? netlb_sel_maglev(p, key, len) : netlb_sel_rr(p, e2, n2); break;
        case NETLB_JIQ:    idx = netlb_sel_jiq(p, e2, n2); break;
    }
    if (idx < 0) return -1;
    /* lo slow-start e' applicato nel calcolo del peso (netlb_sel_wrr via netlb_ss_factor) */
    p->be[idx].picks++;
    if (p->st_ttl > 0.0 && key && len > 0) netlb_sticky_store(p, key, len, idx);
    return idx;
}

netlb_backend_id netlb_pick(netlb_pool* p, const void* key, size_t len) { return netlb_dispatch(p, key, len, NULL, 0); }
netlb_backend_id netlb_pick_ex(netlb_pool* p, const void* key, size_t len, const netlb_backend_id* exclude, int n_exclude) { return netlb_dispatch(p, key, len, exclude, n_exclude); }
