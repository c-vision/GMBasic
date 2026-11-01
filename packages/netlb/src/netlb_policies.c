/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_policies.c — selezione: RR, smooth-WRR, random, LC, WLC, LRT, P2C. */
#include "netlb_internal.h"
#include <float.h>

static int is_ex(const netlb_backend_id* ex, int nex, int i) {
    for (int k = 0; k < nex; k++) if (ex[k] == i) return 1;
    return 0;
}
static int ok(netlb_pool* p, int i, const netlb_backend_id* ex, int nex) {
    return p->be[i].present && p->be[i].healthy && netlb_has_room(&p->be[i]) && !is_ex(ex, nex, i);
}

int netlb_sel_rr(netlb_pool* p, const netlb_backend_id* ex, int nex) {
    for (int k = 0; k < p->n; k++) {
        int i = (int)((p->rr + (uint64_t)k) % (uint64_t)p->n);
        if (ok(p, i, ex, nex)) { p->rr = (uint64_t)(i + 1); return i; }
    }
    return -1;
}

int netlb_sel_random(netlb_pool* p, const netlb_backend_id* ex, int nex) {
    for (int k = 0; k < p->n * 3; k++) {
        int i = (int)netlb_rng_n(p, (uint32_t)p->n);
        if (ok(p, i, ex, nex)) return i;
    }
    for (int i = 0; i < p->n; i++) if (ok(p, i, ex, nex)) return i;
    return -1;
}

/* nginx smooth WRR: curw += w; pick max curw; curw[best] -= sum(w). */
int netlb_sel_wrr(netlb_pool* p, const netlb_backend_id* ex, int nex) {
    double total = 0.0; int best = -1; double bw = -DBL_MAX;
    for (int i = 0; i < p->n; i++) if (ok(p, i, ex, nex)) {
        double w = p->be[i].weight * netlb_ss_factor(p, &p->be[i]);
        if (w <= 0.0) continue;                 /* backend in slow-start iniziale: non eligibile */
        total += w;
        p->be[i].curw += w;
        if (p->be[i].curw > bw) { bw = p->be[i].curw; best = i; }
    }
    if (best < 0) return -1;
    p->be[best].curw -= total;
    return best;
}

int netlb_sel_lc(netlb_pool* p, int weighted, const netlb_backend_id* ex, int nex) {
    int best = -1; double bl = DBL_MAX;
    for (int i = 0; i < p->n; i++) if (ok(p, i, ex, nex)) {
        double l = weighted ? ((double)p->be[i].inflight / (p->be[i].weight > 0 ? p->be[i].weight : 1.0))
                            : (double)p->be[i].inflight;
        if (best < 0 || l < bl) { bl = l; best = i; }
    }
    return best;
}

/* LRT: latenza EWMA minima; i non-misurati (EWMA=0) sono "esplorati" (tie: LC). */
int netlb_sel_lrt(netlb_pool* p, const netlb_backend_id* ex, int nex) {
    int best = -1; double bl = DBL_MAX; double bin = DBL_MAX;
    for (int i = 0; i < p->n; i++) if (ok(p, i, ex, nex)) {
        double lat = p->be[i].measured ? p->be[i].ewma_lat : 0.0;
        double inf = (double)p->be[i].inflight;
        if (best < 0 || lat < bl || (lat == bl && inf < bin)) { bl = lat; bin = inf; best = i; }
    }
    return best;
}

/* P2C: due eleggibili a caso, il min "attesa stimata" (inflight * latenza). */
int netlb_sel_p2c(netlb_pool* p, const netlb_backend_id* ex, int nex) {
    int a = -1, b = -1;
    for (int k = 0; k < p->n * 4 && (a < 0 || b < 0); k++) {
        int i = (int)netlb_rng_n(p, (uint32_t)p->n);
        if (!ok(p, i, ex, nex)) continue;
        if (a < 0) a = i;
        else if (i != a && b < 0) b = i;
    }
    if (a < 0) return -1;
    if (b < 0) b = a;
    return (netlb_load_of(p, a) <= netlb_load_of(p, b)) ? a : b;
}
