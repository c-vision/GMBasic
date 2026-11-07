/*
 * gn_memplan.c — pianificazione di memoria (da LightNet).
 */
#include "gn_memplan.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int overlap(const gn_memplan_tensor *a, const gn_memplan_tensor *b)
{
    return !(a->death < b->birth || b->death < a->birth);
}

gn_real gn_mem_plan(const gn_memplan_tensor *t, int n, gn_real *offsets)
{
    int *order;
    int *placed;
    gn_real *placed_off;
    int i, j, nplaced = 0;
    gn_real total = 0.0;
    if (!t || !offsets || n < 1) return 0.0;
    order = (int *)malloc((size_t)n * sizeof(int));
    placed = (int *)malloc((size_t)n * sizeof(int));
    placed_off = (gn_real *)malloc((size_t)n * sizeof(gn_real));
    if (!order || !placed || !placed_off) { free(order); free(placed); free(placed_off); return 0.0; }
    for (i = 0; i < n; i++) order[i] = i;
    /* ordina per dimensione decrescente */
    for (i = 1; i < n; i++) {
        int k = order[i], q = i - 1;
        while (q >= 0 && t[order[q]].size < t[k].size) { order[q + 1] = order[q]; q--; }
        order[q + 1] = k;
    }
    for (i = 0; i < n; i++) {
        int idx = order[i];
        gn_real off = 0.0;
        int changed = 1;
        /* cerca l'offset più basso che non si sovrappone ai buffer vivi che
         * hanno intervallo sovrapposto a questo */
        while (changed) {
            changed = 0;
            for (j = 0; j < nplaced; j++) {
                int p = placed[j];
                if (!overlap(&t[idx], &t[p])) continue;
                /* se [off, off+size) interseca [po, po+size) allora sposta */
                if (off < placed_off[j] + t[p].size && placed_off[j] < off + t[idx].size) {
                    off = placed_off[j] + t[p].size;
                    changed = 1;
                }
            }
        }
        offsets[idx] = off;
        placed[nplaced] = idx;
        placed_off[nplaced] = off;
        nplaced++;
        if (off + t[idx].size > total) total = off + t[idx].size;
    }
    free(order); free(placed); free(placed_off);
    return total;
}
