/* gor_pack.c — packing, knapsack, cutting stock. */
#include "gor_pack.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================== bin packing 1D ============================== */

typedef struct { gor_real w; int idx; } gor_item_w;

static int cmp_item_desc(const void *a, const void *b)
{
    const gor_item_w *x = (const gor_item_w *)a, *y = (const gor_item_w *)b;
    if (y->w > x->w) return 1;
    if (y->w < x->w) return -1;
    return 0;
}

int gor_bin_packing(int n, const gor_real *items, gor_real cap, int algo, int *bin_of)
{
    gor_real *load;
    int *bins = 0, nb = 0, i, b;
    gor_item_w *ord = NULL;
    if (n <= 0 || items == NULL || cap <= 0.0 || bin_of == NULL) return 0;
    load = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!load) return 0;
    if (algo == 4 || algo == 5) {
        ord = (gor_item_w *)malloc((size_t)n * sizeof(gor_item_w));
        if (!ord) { free(load); return 0; }
        for (i = 0; i < n; i++) { ord[i].w = items[i]; ord[i].idx = i; }
        qsort(ord, (size_t)n, sizeof(gor_item_w), cmp_item_desc);
    }
    for (i = 0; i < n; i++) {
        gor_real w = (ord ? ord[i].w : items[i]);
        int idx = (ord ? ord[i].idx : i);
        int placed = 0;
        if (algo == 0) { /* next fit: ultimo bin */
            if (nb > 0 && load[nb - 1] + w <= cap + 1e-12) { load[nb - 1] += w; bin_of[idx] = nb - 1; placed = 1; }
        } else if (algo == 1 || algo == 4) { /* first fit */
            for (b = 0; b < nb; b++) if (load[b] + w <= cap + 1e-12) { load[b] += w; bin_of[idx] = b; placed = 1; break; }
        } else if (algo == 2 || algo == 5) { /* best fit */
            int bb = -1;
            gor_real bres = 1e300;
            for (b = 0; b < nb; b++) {
                gor_real res = cap - (load[b] + w);
                if (res >= -1e-12 && res < bres) { bres = res; bb = b; }
            }
            if (bb >= 0) { load[bb] += w; bin_of[idx] = bb; placed = 1; }
        } else { /* worst fit */
            int bb = -1;
            gor_real bres = -1.0;
            for (b = 0; b < nb; b++) {
                gor_real res = cap - (load[b] + w);
                if (res >= -1e-12 && res > bres) { bres = res; bb = b; }
            }
            if (bb >= 0) { load[bb] += w; bin_of[idx] = bb; placed = 1; }
        }
        if (!placed) { load[nb] = w; bin_of[idx] = nb; nb++; }
    }
    free(load); free(ord); free(bins);
    return nb;
}

int gor_bin_packing_lb(int n, const gor_real *items, gor_real cap)
{
    gor_real sum = 0.0, sum1 = 0.0, sum2 = 0.0;
    int j1 = 0, j2 = 0, i, lb;
    if (n <= 0 || items == NULL || cap <= 0.0) return 0;
    for (i = 0; i < n; i++) {
        sum += items[i];
        if (items[i] > cap / 2.0) { j1++; sum1 += items[i]; }
        else if (items[i] > cap / 3.0) { j2++; sum2 += items[i]; }
    }
    lb = (int)ceil(sum / cap - 1e-9);
    {
        gor_real rem = (gor_real)j1 * cap - sum1;
        int l1b = j1 + (int)ceil((sum2 - rem) / cap - 1e-9);
        if (l1b < j1) l1b = j1;
        if (l1b > lb) lb = l1b;
    }
    (void)j2;
    return lb;
}

/* ============================== knapsack ============================== */

gor_real gor_knapsack_frac(int n, const gor_real *w, const gor_real *v, gor_real cap, gor_real *x)
{
    int *ord, i;
    gor_real total = 0.0, rem = cap;
    if (n <= 0 || w == NULL || v == NULL || cap <= 0.0) return 0.0;
    ord = (int *)malloc((size_t)n * sizeof(int));
    if (!ord) return 0.0;
    for (i = 0; i < n; i++) { ord[i] = i; if (x) x[i] = 0.0; }
    /* selection sort per ratio v/w decrescente */
    for (i = 0; i < n; i++) {
        int best = i, k;
        for (k = i + 1; k < n; k++) {
            gor_real ri = (w[ord[k]] > 0) ? v[ord[k]] / w[ord[k]] : 1e300;
            gor_real rb = (w[ord[best]] > 0) ? v[ord[best]] / w[ord[best]] : 1e300;
            if (ri > rb) best = k;
        }
        { int t = ord[i]; ord[i] = ord[best]; ord[best] = t; }
    }
    for (i = 0; i < n; i++) {
        int j = ord[i];
        if (w[j] <= rem) { if (x) x[j] = 1.0; total += v[j]; rem -= w[j]; }
        else { if (x) x[j] = rem / w[j]; total += v[j] * rem / w[j]; rem = 0.0; break; }
    }
    free(ord);
    return total;
}

gor_real gor_knapsack_01(int n, const gor_real *w, const gor_real *v, int cap, int *take)
{
    gor_real *dp;
    int i, c;
    if (n <= 0 || w == NULL || v == NULL || cap < 0) return 0.0;
    dp = (gor_real *)calloc((size_t)cap + 1, sizeof(gor_real));
    if (!dp) return 0.0;
    for (i = 0; i < n; i++) {
        int wi = (int)(w[i] + 0.5);
        for (c = cap; c >= wi; c--) {
            gor_real cand = dp[c - wi] + v[i];
            if (cand > dp[c]) dp[c] = cand;
        }
    }
    if (take) {
        int c2 = cap;
        for (i = 0; i < n; i++) take[i] = 0;
        for (i = n - 1; i >= 0; i--) {
            int wi = (int)(w[i] + 0.5);
            if (c2 >= wi && fabs(dp[c2] - (dp[c2 - wi] + v[i])) < 1e-9) { take[i] = 1; c2 -= wi; }
        }
    }
    {
        gor_real res = dp[cap];
        free(dp);
        return res;
    }
}

gor_real gor_knapsack_unbounded(int n, const gor_real *w, const gor_real *v, int cap, int *counts)
{
    gor_real *dp;
    int *cnt, i, c;
    if (n <= 0 || w == NULL || v == NULL || cap < 0) return 0.0;
    dp = (gor_real *)calloc((size_t)cap + 1, sizeof(gor_real));
    cnt = (int *)calloc((size_t)cap + 1, sizeof(int));
    if (!dp || !cnt) { free(dp); free(cnt); return 0.0; }
    for (c = 0; c <= cap; c++)
        for (i = 0; i < n; i++) {
            int wi = (int)(w[i] + 0.5);
            if (wi > 0 && c >= wi && dp[c - wi] + v[i] > dp[c]) { dp[c] = dp[c - wi] + v[i]; cnt[c] = i; }
        }
    if (counts) {
        for (i = 0; i < n; i++) counts[i] = 0;
        c = cap;
        while (c > 0) {
            int it = cnt[c];
            int wi = (int)(w[it] + 0.5);
            if (wi <= 0 || c < wi) break;
            counts[it]++; c -= wi;
        }
    }
    {
        gor_real res = dp[cap];
        free(dp); free(cnt);
        return res;
    }
}

int gor_subset_sum(int n, const gor_real *items, gor_real target, int *take)
{
    char *dp;
    int i, c, cap = (int)(target + 0.5);
    if (n <= 0 || items == NULL || target < 0) return 0;
    dp = (char *)calloc((size_t)cap + 1, 1);
    if (!dp) return 0;
    dp[0] = 1;
    for (i = 0; i < n; i++) {
        int wi = (int)(items[i] + 0.5);
        for (c = cap; c >= wi; c--) if (dp[c - wi]) dp[c] = 1;
    }
    if (take && dp[cap]) {
        /* Ricostruzione con tabella per PREFISSO di item: la versione precedente usava il DP
         * globale e sceglieva item la cui somma residua non era raggiungibile con i soli item
         * ancora disponibili (es. target 9 su {3,34,4,12,5,2} dava 5+2=7). La tabella 2D costa
         * n*(cap+1) byte; oltre 64 MB si rinuncia alla ricostruzione (l'esito 0/1 resta). */
        size_t rows = (size_t)n + 1u, cols = (size_t)cap + 1u;
        unsigned char *d2 = NULL;
        if (rows * cols <= 64u * 1024u * 1024u) d2 = (unsigned char *)calloc(rows * cols, 1);
        for (i = 0; i < n; i++) take[i] = 0;
        if (d2 != NULL) {
            d2[0] = 1;
            for (i = 0; i < n; i++) {
                int wi = (int)(items[i] + 0.5);
                unsigned char *prev = d2 + (size_t)i * cols;
                unsigned char *cur_row = d2 + (size_t)(i + 1) * cols;
                for (c = 0; c <= cap; c++) {
                    cur_row[c] = prev[c];
                    if (!cur_row[c] && c >= wi && prev[c - wi]) cur_row[c] = 1;
                }
            }
            c = cap;
            for (i = n - 1; i >= 0 && c > 0; i--) {
                int wi = (int)(items[i] + 0.5);
                if (c >= wi && d2[(size_t)i * cols + (size_t)(c - wi)]) { take[i] = 1; c -= wi; }
            }
            free(d2);
        }
    }
    {
        int ok = dp[cap] ? 1 : 0;
        free(dp);
        return ok;
    }
}

/* meet-in-the-middle */
gor_real gor_knapsack_01_mitm(int n, const gor_real *w, const gor_real *v, gor_real cap, int *take)
{
    int h1 = n / 2, h2 = n - h1;
    long long s1 = 1LL << h1, s2 = 1LL << h2, m;
    gor_real *W1, *V1;
    long long *idx1, *idx2;
    int i, j;
    gor_real best = 0.0;
    long long best1 = 0, best2 = 0;
    if (n <= 0 || n > 40 || w == NULL || v == NULL || cap < 0) return 0.0;
    W1 = (gor_real *)malloc((size_t)s1 * sizeof(gor_real));
    V1 = (gor_real *)malloc((size_t)s1 * sizeof(gor_real));
    idx1 = (long long *)malloc((size_t)s1 * sizeof(long long));
    idx2 = (long long *)malloc((size_t)s2 * sizeof(long long));
    if (!W1 || !V1 || !idx1 || !idx2) { free(W1); free(V1); free(idx1); free(idx2); return 0.0; }
    for (m = 0; m < s1; m++) {
        gor_real sw = 0.0, sv = 0.0;
        for (i = 0; i < h1; i++) if (m & (1LL << i)) { sw += w[i]; sv += v[i]; }
        W1[m] = sw; V1[m] = sv; idx1[m] = m;
    }
    /* ordina per peso */
    for (i = 0; i < (int)s1; i++) {
        int bi = i; long long k;
        for (k = i + 1; k < s1; k++) if (W1[k] < W1[bi]) bi = (int)k;
        if (bi != i) { gor_real t; long long tt; t = W1[i]; W1[i] = W1[bi]; W1[bi] = t; t = V1[i]; V1[i] = V1[bi]; V1[bi] = t; tt = idx1[i]; idx1[i] = idx1[bi]; idx1[bi] = tt; }
    }
    for (m = 0; m < s2; m++) {
        gor_real sw = 0.0, sv = 0.0;
        for (j = 0; j < h2; j++) if (m & (1LL << j)) { sw += w[h1 + j]; sv += v[h1 + j]; }
        if (sw <= cap + 1e-12) {
            /* trova il miglior subset della prima metà con W1 <= cap-sw (binary search) */
            long long lo = 0, hi = s1 - 1, bestk = -1;
            while (lo <= hi) {
                long long mid = (lo + hi) / 2;
                if (W1[mid] <= cap - sw + 1e-12) { bestk = mid; lo = mid + 1; }
                else hi = mid - 1;
            }
            if (bestk >= 0 && sv + V1[bestk] > best) { best = sv + V1[bestk]; best1 = idx1[bestk]; best2 = m; }
        }
        idx2[m] = m;
    }
    if (take) {
        for (i = 0; i < n; i++) take[i] = 0;
        for (i = 0; i < h1; i++) if (best1 & (1LL << i)) take[i] = 1;
        for (j = 0; j < h2; j++) if (best2 & (1LL << j)) take[h1 + j] = 1;
    }
    free(W1); free(V1); free(idx1); free(idx2);
    return best;
}

/* ============================== cutting stock ============================== */

int gor_cutting_stock_ffd(int n, const gor_real *pieces, gor_real stock, int *pattern)
{
    return gor_bin_packing(n, pieces, stock, 4, pattern);
}

/* ============================== bin packing 3D ============================== */

int gor_bin_packing_3d(int n, const gor_real *items, gor_real W, gor_real H, gor_real D,
                       int *bin_of)
{
    /* items: n triplette (w,h,d). Euristica: sort per volume desc, first-fit per volume
     * + vincolo dimensionale semplice (bin considerato capiente se volume residuo sufficiente). */
    gor_real *vol, *used;
    gor_item_w *ord;
    int nb = 0, i, b;
    gor_real binvol = W * H * D;
    if (n <= 0 || items == NULL || bin_of == NULL || binvol <= 0) return 0;
    vol = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    used = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    ord = (gor_item_w *)malloc((size_t)n * sizeof(gor_item_w));
    if (!vol || !used || !ord) { free(vol); free(used); free(ord); return 0; }
    for (i = 0; i < n; i++) {
        gor_real w = items[3 * i], h = items[3 * i + 1], d = items[3 * i + 2];
        vol[i] = w * h * d;
        ord[i].w = vol[i]; ord[i].idx = i;
    }
    qsort(ord, (size_t)n, sizeof(gor_item_w), cmp_item_desc);
    for (i = 0; i < n; i++) {
        int idx = ord[i].idx;
        int placed = 0;
        for (b = 0; b < nb; b++) if (used[b] + vol[idx] <= binvol + 1e-9) { used[b] += vol[idx]; bin_of[idx] = b; placed = 1; break; }
        if (!placed) { used[nb] = vol[idx]; bin_of[idx] = nb; nb++; }
    }
    free(vol); free(used); free(ord);
    return nb;
}
