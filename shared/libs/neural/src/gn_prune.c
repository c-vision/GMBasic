/*
 * gn_prune.c — 2:4 structured sparsity e channel pruning (da lightNet).
 */
#include "gn_prune.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

int gn_sparsify_2_4(gn_real *w, int n, int window, int nonzeros)
{
    int base, removed = 0;
    if (!w || n < 1 || window < 1 || nonzeros < 0 || nonzeros > window) return 0;
    for (base = 0; base + window <= n; base += window) {
        int i, j;
        /* soglia = magnitudine del `nonzeros`-esimo più grande nella finestra */
        gn_real thr;
        gn_real buf[64];
        int m = window < 64 ? window : 64;
        for (i = 0; i < m; i++) buf[i] = fabs(w[base + i]);
        /* ordina decrescente (insertion) */
        for (i = 1; i < m; i++) {
            gn_real k = buf[i]; j = i - 1;
            while (j >= 0 && buf[j] < k) { buf[j + 1] = buf[j]; j--; }
            buf[j + 1] = k;
        }
        thr = (nonzeros == 0) ? 0.0 : (nonzeros <= m ? buf[nonzeros - 1] : 0.0);
        for (i = 0; i < window; i++) {
            if (fabs(w[base + i]) < thr - 1e-15) { if (w[base + i] != 0.0) removed++; w[base + i] = 0.0; }
        }
        /* se ci sono pareggi sulla soglia, azzera i restanti fino a `nonzeros` */
        {
            int kept = 0;
            for (i = 0; i < window; i++) if (w[base + i] != 0.0) kept++;
            for (i = 0; i < window && kept > nonzeros; i++)
                if (w[base + i] != 0.0 && fabs(w[base + i]) <= thr + 1e-15) { w[base + i] = 0.0; removed++; kept--; }
        }
    }
    return removed;
}

void gn_prune_channel_l1(const gn_real *w, int out_c, int per_ch, gn_real *l1)
{
    int oc, i;
    if (!w || !l1 || out_c < 1 || per_ch < 1) return;
    for (oc = 0; oc < out_c; oc++) {
        gn_real s = 0.0;
        const gn_real *row = w + (size_t)oc * per_ch;
        for (i = 0; i < per_ch; i++) s += fabs(row[i]);
        l1[oc] = s;
    }
}

int gn_prune_channels_mask(const gn_real *l1, int out_c, gn_real keep_frac,
                           unsigned char *mask)
{
    gn_real *sorted;
    int i, keep;
    if (!l1 || !mask || out_c < 1) return 0;
    if (keep_frac < 0.0) keep_frac = 0.0;
    if (keep_frac > 1.0) keep_frac = 1.0;
    keep = (int)(keep_frac * out_c + 0.5);
    if (keep < 1) keep = 1;
    if (keep > out_c) keep = out_c;
    sorted = (gn_real *)malloc((size_t)out_c * sizeof(gn_real));
    if (!sorted) return 0;
    for (i = 0; i < out_c; i++) sorted[i] = l1[i];
    for (i = 1; i < out_c; i++) {
        gn_real k = sorted[i]; int j = i - 1;
        while (j >= 0 && sorted[j] < k) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = k;
    }
    { gn_real thr = sorted[keep - 1]; int kept = 0;
      for (i = 0; i < out_c; i++) {
          if (l1[i] > thr + 1e-15 || (l1[i] >= thr - 1e-15 && kept < keep)) { mask[i] = 1; kept++; }
          else mask[i] = 0;
      }
      free(sorted);
      return kept; }
}
