/*
 * gor_linkpred.c — euristiche di link prediction (da muhanzhang/LinkPrediction).
 */
#include "gor_linkpred.h"
#include <stdlib.h>
#include <math.h>

static int deg(const int *adj, int n, int x)
{
    int j, d = 0;
    for (j = 0; j < n; j++) if (adj[(size_t)x * n + j] && j != x) d++;
    return d;
}

int gor_lp_common_neighbors(const int *adj, int n, int u, int v)
{
    int w, c = 0;
    if (!adj || n < 1) return 0;
    for (w = 0; w < n; w++)
        if (w != u && w != v && adj[(size_t)u * n + w] && adj[(size_t)v * n + w]) c++;
    return c;
}

gor_real gor_lp_jaccard(const int *adj, int n, int u, int v)
{
    int w, cn = 0, un = 0;
    if (!adj || n < 1) return 0.0;
    for (w = 0; w < n; w++) {
        int a = (w != u && adj[(size_t)u * n + w]) ? 1 : 0;
        int b = (w != v && adj[(size_t)v * n + w]) ? 1 : 0;
        if (a && b) cn++;
        if (a || b) un++;
    }
    return un == 0 ? 0.0 : (gor_real)cn / (gor_real)un;
}

gor_real gor_lp_adamic_adar(const int *adj, int n, int u, int v)
{
    int w;
    gor_real s = 0.0;
    if (!adj || n < 1) return 0.0;
    for (w = 0; w < n; w++)
        if (w != u && w != v && adj[(size_t)u * n + w] && adj[(size_t)v * n + w]) {
            int d = deg(adj, n, w);
            if (d > 1) s += 1.0 / log((gor_real)d);
        }
    return s;
}

gor_real gor_lp_resource_allocation(const int *adj, int n, int u, int v)
{
    int w;
    gor_real s = 0.0;
    if (!adj || n < 1) return 0.0;
    for (w = 0; w < n; w++)
        if (w != u && w != v && adj[(size_t)u * n + w] && adj[(size_t)v * n + w]) {
            int d = deg(adj, n, w);
            if (d > 0) s += 1.0 / (gor_real)d;
        }
    return s;
}

int gor_lp_preferential_attachment(const int *adj, int n, int u, int v)
{
    if (!adj || n < 1) return 0;
    return deg(adj, n, u) * deg(adj, n, v);
}

int gor_lp_katz(const gor_real *adj, int n, gor_real beta, int u, int v, gor_real *score)
{
    /* Calcola M = (I - beta A)^{-1} - I risolvendo (I - beta A) X = I con
     * Gauss-Jordan, poi legge X[u][v]. O(n^3). */
    gor_real *M, *inv;
    int i, j, k;
    if (!adj || !score || n < 1 || n > 512) return -1;
    M = (gor_real *)malloc((size_t)n * n * sizeof(gor_real));
    inv = (gor_real *)malloc((size_t)n * n * sizeof(gor_real));
    if (!M || !inv) { free(M); free(inv); return -1; }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            M[(size_t)i * n + j] = (i == j ? 1.0 : 0.0) - beta * adj[(size_t)i * n + j];
    /* [M | I] -> [I | M^{-1}] */
    {
        gor_real *W = (gor_real *)malloc((size_t)n * 2 * n * sizeof(gor_real));
        if (!W) { free(M); free(inv); return -1; }
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) W[(size_t)i * 2 * n + j] = M[(size_t)i * n + j];
            for (j = 0; j < n; j++) W[(size_t)i * 2 * n + n + j] = (i == j ? 1.0 : 0.0);
        }
        for (k = 0; k < n; k++) {
            int piv = k; gor_real mx = fabs(W[(size_t)k * 2 * n + k]);
            for (i = k + 1; i < n; i++) { gor_real v = fabs(W[(size_t)i * 2 * n + k]); if (v > mx) { mx = v; piv = i; } }
            if (mx < 1e-12) { free(W); free(M); free(inv); return -1; }
            if (piv != k) for (j = 0; j < 2 * n; j++) { gor_real t = W[(size_t)k*2*n+j]; W[(size_t)k*2*n+j] = W[(size_t)piv*2*n+j]; W[(size_t)piv*2*n+j] = t; }
            { gor_real p = W[(size_t)k * 2 * n + k]; for (j = 0; j < 2 * n; j++) W[(size_t)k * 2 * n + j] /= p; }
            for (i = 0; i < n; i++) {
                gor_real f;
                if (i == k) continue;
                f = W[(size_t)i * 2 * n + k];
                if (f == 0.0) continue;
                for (j = 0; j < 2 * n; j++) W[(size_t)i * 2 * n + j] -= f * W[(size_t)k * 2 * n + j];
            }
        }
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) inv[(size_t)i * n + j] = W[(size_t)i * 2 * n + n + j];
        free(W);
    }
    *score = inv[(size_t)u * n + v];   /* (I - beta A)^{-1}; Katz = X - I (u!=v) */
    free(M); free(inv);
    return 0;
}
