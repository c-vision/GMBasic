/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_hrw_skel.c — HRW gerarchico "skeleton" (Thaler-Ravishankar, CSE-TR-316-96; IEEE/ACM ToN 1998).
 *
 * L'HRW piatto e' O(n): per ogni backend calcola hash(key, backend) e prende il massimo.
 * Lo skeleton riduce a O(log n): i backend sono raggruppati in cluster di m; si costruisce un albero
 * virtuale di fanout f sui cluster e si applica HRW scendendo livello per livello (a ogni livello HRW
 * sui figli NON vuoti), fino a un cluster; poi HRW sui backend di quel cluster.
 *
 * I cluster sono indicizzati per id STABILE (`id/m`): aggiungere/rimuovere un backend tocca solo il suo
 * cluster -> la disruption resta limitata (vicina a 1/n). clu_pref scarta in O(1) i sottoalberi vuoti.
 *
 * Il rebuild (add/remove) e' O(n) ma raro; il PICK e' O(log n). Default: OFF (HRW piatto). */
#include "netlb_internal.h"

void netlb_skel_build(netlb_pool* p) {
    if (!p || !p->skel) return;
    netlb_skel* s = p->skel;
    free(s->clu_cnt); s->clu_cnt = NULL;
    free(s->clu_pref); s->clu_pref = NULL;
    int maxid = -1;
    for (int i = 0; i < p->n; i++) if (p->be[i].present && i > maxid) maxid = i;
    if (maxid < 0) { s->C = 0; s->height = 0; return; }
    s->C = (maxid + (int)s->m) / (int)s->m;                   /* ceil((maxid+1)/m) */
    s->clu_cnt = (int*)calloc((size_t)s->C, sizeof(int));
    s->clu_pref = (int*)calloc((size_t)s->C + 1, sizeof(int));
    if (!s->clu_cnt || !s->clu_pref) { s->C = 0; s->height = 0; return; }
    for (int i = 0; i < p->n; i++) if (p->be[i].present) s->clu_cnt[i / (int)s->m]++;
    for (int c = 0; c < s->C; c++) s->clu_pref[c + 1] = s->clu_pref[c] + s->clu_cnt[c];
    int h = 0; long cap = 1;
    while (cap < s->C) { cap *= (long)s->f; h++; }
    s->height = h;
}

static int skel_range(const netlb_skel* s, int lo, int hi) {
    if (lo < 0) lo = 0;
    if (hi > s->C) hi = s->C;
    return (hi > lo) ? s->clu_pref[hi] - s->clu_pref[lo] : 0;
}

int netlb_sel_hrw_skel(netlb_pool* p, const void* key, size_t len) {
    netlb_skel* s = p->skel;
    if (!s || s->C <= 0) return -1;
    long span = 1; for (int i = 0; i < s->height; i++) span *= (long)s->f;   /* f^height >= C */
    int base = 0, level = 0;
    while (span > 1) {
        long cspan = span / (long)s->f;
        int best = -1, beststart = -1; uint64_t bw = 0;
        for (uint32_t i = 0; i < s->f; i++) {
            int start = base + (int)((long)i * cspan);
            if (start >= s->C) break;
            int end = start + (int)cspan; if (end > s->C) end = s->C;
            if (skel_range(s, start, end) == 0) continue;             /* sottoalbero vuoto */
            s->hashes++;
            uint64_t nodeid = ((uint64_t)level << 48) ^ ((uint64_t)start * 0x9E3779B97F4A7C15ULL);
            uint64_t w = netlb_hash_key(key, len, nodeid ^ p->seed);
            if (best < 0 || w > bw) { bw = w; best = (int)i; beststart = start; }
        }
        if (best < 0) return -1;
        base = beststart; span = cspan; level++;
    }
    /* foglia = cluster `base`: HRW sui backend id in [base*m, (base+1)*m) */
    int lo = base * (int)s->m, hi = lo + (int)s->m; if (hi > p->n) hi = p->n;
    int best = -1; uint64_t bw = 0;
    for (int idx = lo; idx < hi; idx++) {
        if (!p->be[idx].present || !p->be[idx].healthy || !netlb_has_room(&p->be[idx])) continue;
        s->hashes++;
        uint64_t w = netlb_hash_key(key, len, p->be[idx].hrw);
        if (best < 0 || w > bw) { bw = w; best = idx; }
    }
    return best;   /* -1 se il cluster scelto non ha eleggibili: il dispatch ricade sull'HRW piatto */
}

void netlb_set_hrw_skeleton(netlb_pool* p, uint32_t m, uint32_t f) {
    if (!p) return;
    if (m == 0 || f < 2) {                                     /* off */
        if (p->skel) { free(p->skel->clu_cnt); free(p->skel->clu_pref); free(p->skel); p->skel = NULL; }
        return;
    }
    if (!p->skel) p->skel = (netlb_skel*)calloc(1, sizeof(netlb_skel));
    if (!p->skel) return;
    p->skel->m = m; p->skel->f = f;
    netlb_skel_build(p);
}

long netlb_hrw_hashes(const netlb_pool* p) { return (p && p->skel) ? (long)p->skel->hashes : 0; }
