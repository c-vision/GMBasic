/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* netlb_policy_jiq.c — JIQ (Join-Idle-Queue, Lu et al. NSDI'16) in forma event-driven.
 *
 * L'host non ha thread concorrenti: ogni backend "idle" (inflight==0, presente e sano) e'
 * tenuto in una coda FIFO intrusiva (O(1) push/remove/pop). Un nuovo pick prende il primo
 * backend idle non escluso; se la coda e' vuota (tutti occupati/non sani) degrada a P2C.
 * Beneficio: nessuna scansione, il nuovo backend viene scelto solo quando e' libero
 * (riduce la latenza di coda rispetto a P2C che "indovina" su due candidati casuali).
 *
 * La coda e' mantenuta SEMPRE dal pool (netlb_add/remove/begin/end/probe); selezionata solo
 * quando policy == NETLB_JIQ. */
#include "netlb_internal.h"

void netlb_idle_push(netlb_pool* p, int i) {
    if (!p || i < 0 || i >= p->n) return;
    netlb_backend* b = &p->be[i];
    if (b->q_in) return;                       /* gia' in coda */
    b->q_in = 1;
    b->q_next = -1;
    b->q_prev = p->q_tail;
    if (p->q_tail >= 0) p->be[p->q_tail].q_next = i;
    else                p->q_head = i;
    p->q_tail = i;
    p->q_n++;
}

void netlb_idle_remove(netlb_pool* p, int i) {
    if (!p || i < 0 || i >= p->n) return;
    netlb_backend* b = &p->be[i];
    if (!b->q_in) return;
    if (b->q_prev >= 0) p->be[b->q_prev].q_next = b->q_next;
    else                p->q_head = b->q_next;
    if (b->q_next >= 0) p->be[b->q_next].q_prev = b->q_prev;
    else                p->q_tail = b->q_prev;
    b->q_prev = b->q_next = -1;
    b->q_in = 0;
    p->q_n--;
}

/* JIQ: O(1) se la testa e' valida e non esclusa; altrimenti scansione della coda;
 * fallback P2C se nessun idle eleggibile. */
int netlb_sel_jiq(netlb_pool* p, const netlb_backend_id* ex, int nex) {
    int i = p->q_head;
    while (i >= 0) {
        netlb_backend* b = &p->be[i];
        int next = b->q_next;                  /* salva prima di un eventuale remove */
        if (!b->present || !b->healthy) { netlb_idle_remove(p, i); i = next; continue; }
        int excluded = 0;
        for (int k = 0; k < nex; k++) if (ex[k] == i) { excluded = 1; break; }
        if (!excluded) { netlb_idle_remove(p, i); return i; }
        i = next;
    }
    return netlb_sel_p2c(p, ex, nex);            /* tutti occupati: unisci a P2C */
}
