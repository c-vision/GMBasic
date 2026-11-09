/* ============================================================================
 * kdtree.c — namespace KDTREE (scipy.spatial.KDTree).
 *
 * Self-contained: tabella statica di alberi (handle int), nessuna dipendenza
 * dal runtime. Le query scrivono in buffer forniti dal chiamante:
 *   ofx_kd_query*  ritornano il numero di risultati (<= cap), -1 su errore.
 * ============================================================================ */
#include "gmbkdtree.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GMB_KD_MAX 256
#define GMB_KD_MAXNODES 8192

typedef struct {
    double* pts;          /* n*m flat (row-major) */
    int n, m;
    int node_count;
    int* node_pt;         /* indice del punto per nodo */
    int* node_dim;        /* dimensione di split */
    double* node_val;     /* piano di split */
    int* node_left;
    int* node_right;
    int in_use;
} gmb_kdtree_t;

static gmb_kdtree_t g_kdtrees[GMB_KD_MAX];

static gmb_kdtree_t* kd_get(int h)
{
    if (h <= 0 || h >= GMB_KD_MAX) return NULL;
    return g_kdtrees[h].in_use ? &g_kdtrees[h] : NULL;
}

static int kd_build(gmb_kdtree_t* t, int* idx, int nidx)
{
    if (nidx <= 0) return -1;
    int dim = 0;
    double best = -1;
    for (int d = 0; d < t->m; d++)
    {
        double mn = 1e300, mx = -1e300;
        for (int i = 0; i < nidx; i++)
        {
            double v = t->pts[idx[i] * t->m + d];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        if (mx - mn > best) { best = mx - mn; dim = d; }
    }
    /* insertion sort degli indici sulla dimensione di split (n piccolo) */
    for (int i = 1; i < nidx; i++)
    {
        int key = idx[i];
        int j = i - 1;
        while (j >= 0 && t->pts[idx[j] * t->m + dim] > t->pts[key * t->m + dim]) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = key;
    }
    int mid = nidx / 2;
    int ni = t->node_count++;
    t->node_pt[ni] = idx[mid];
    t->node_dim[ni] = dim;
    t->node_val[ni] = t->pts[idx[mid] * t->m + dim];
    t->node_left[ni] = kd_build(t, idx, mid);
    t->node_right[ni] = kd_build(t, idx + mid + 1, nidx - mid - 1);
    return ni;
}

int ofx_kd_create(const double* flat, int n, int m)
{
    if (flat == NULL || n <= 0 || m <= 0) return 0;
    gmb_kdtree_t* t = NULL;
    for (int i = 1; i < GMB_KD_MAX; i++)
        if (!g_kdtrees[i].in_use) { t = &g_kdtrees[i]; break; }
    if (!t) return 0;
    memset(t, 0, sizeof(*t));
    t->pts = (double*)malloc(sizeof(double) * (size_t)n * (size_t)m);
    t->node_pt = (int*)malloc(sizeof(int) * GMB_KD_MAXNODES);
    t->node_dim = (int*)malloc(sizeof(int) * GMB_KD_MAXNODES);
    t->node_val = (double*)malloc(sizeof(double) * GMB_KD_MAXNODES);
    t->node_left = (int*)malloc(sizeof(int) * GMB_KD_MAXNODES);
    t->node_right = (int*)malloc(sizeof(int) * GMB_KD_MAXNODES);
    if (!t->pts || !t->node_pt || !t->node_dim || !t->node_val || !t->node_left || !t->node_right)
    {
        free(t->pts); free(t->node_pt); free(t->node_dim); free(t->node_val); free(t->node_left); free(t->node_right);
        return 0;
    }
    memcpy(t->pts, flat, sizeof(double) * (size_t)n * (size_t)m);
    t->n = n;
    t->m = m;
    int* idx = (int*)malloc(sizeof(int) * (size_t)n);
    if (!idx) return 0;
    for (int i = 0; i < n; i++) idx[i] = i;
    kd_build(t, idx, n);
    free(idx);
    t->in_use = 1;
    for (int i = 1; i < GMB_KD_MAX; i++)
        if (t == &g_kdtrees[i]) return i;
    return 0;
}

int ofx_kd_count(int h)
{
    gmb_kdtree_t* t = kd_get(h);
    return t ? t->n : 0;
}

static double kd_dist2(const gmb_kdtree_t* t, const double* q, int pi)
{
    double d2 = 0;
    const double* p = t->pts + (size_t)pi * t->m;
    for (int d = 0; d < t->m; d++) { double z = q[d] - p[d]; d2 += z * z; }
    return d2;
}

/* Inserimento ordinato (ascendente) nei k migliori; ritorna il nuovo worst (d2 max). */
static double kd_insert(double* best_d, int* best_i, int k, int* count, double d2, int pi)
{
    if (*count < k)
    {
        int pos = (*count)++;
        while (pos > 0 && d2 < best_d[pos - 1]) { best_d[pos] = best_d[pos - 1]; best_i[pos] = best_i[pos - 1]; pos--; }
        best_d[pos] = d2; best_i[pos] = pi;
    }
    else if (d2 < best_d[k - 1])
    {
        int pos = k - 1;
        while (pos > 0 && d2 < best_d[pos - 1]) { best_d[pos] = best_d[pos - 1]; best_i[pos] = best_i[pos - 1]; pos--; }
        best_d[pos] = d2; best_i[pos] = pi;
    }
    return best_d[*count - 1];
}

static void kd_search(const gmb_kdtree_t* t, int node, const double* q,
                      double* best_d, int* best_i, int k, int* count, double* worst)
{
    if (node < 0) return;
    double d2 = kd_dist2(t, q, t->node_pt[node]);
    if (*count < k || d2 < *worst) *worst = kd_insert(best_d, best_i, k, count, d2, t->node_pt[node]);
    double plane = q[t->node_dim[node]] - t->node_val[node];
    int near = plane < 0 ? t->node_left[node] : t->node_right[node];
    int far = plane < 0 ? t->node_right[node] : t->node_left[node];
    kd_search(t, near, q, best_d, best_i, k, count, worst);
    if (*count < k || plane * plane < *worst) kd_search(t, far, q, best_d, best_i, k, count, worst);
}

int ofx_kd_query(int h, const double* q, int k, double* out_idx, int out_cap)
{
    gmb_kdtree_t* t = kd_get(h);
    if (!t || !q || k <= 0 || !out_idx || out_cap < k) return -1;
    double* best_d = (double*)malloc(sizeof(double) * (size_t)k);
    int* best_i = (int*)malloc(sizeof(int) * (size_t)k);
    if (!best_d || !best_i) { free(best_d); free(best_i); return -1; }
    int count = 0;
    double worst = 1e300;
    kd_search(t, 0, q, best_d, best_i, k, &count, &worst);
    for (int i = 0; i < count; i++) out_idx[i] = (double)best_i[i];
    free(best_d); free(best_i);
    return count;
}

int ofx_kd_query_dist(int h, const double* q, int k, double* out_d, int out_cap)
{
    gmb_kdtree_t* t = kd_get(h);
    if (!t || !q || k <= 0 || !out_d || out_cap < k) return -1;
    int* idx = (int*)malloc(sizeof(int) * (size_t)k);
    double* best_d = (double*)malloc(sizeof(double) * (size_t)k);
    if (!idx || !best_d) { free(idx); free(best_d); return -1; }
    int count = 0;
    double worst = 1e300;
    kd_search(t, 0, q, best_d, idx, k, &count, &worst);
    for (int i = 0; i < count; i++) out_d[i] = sqrt(best_d[i]);
    free(idx); free(best_d);
    return count;
}

int ofx_kd_query_ball(int h, const double* q, double r, double* out_idx, int out_cap)
{
    gmb_kdtree_t* t = kd_get(h);
    if (!t || !q || !out_idx || out_cap < t->n) return -1;
    double r2 = r * r;
    int count = 0;
    int* stack = (int*)malloc(sizeof(int) * GMB_KD_MAXNODES * 2);
    if (!stack) return -1;
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0)
    {
        int node = stack[--sp];
        if (node < 0) continue;
        if (kd_dist2(t, q, t->node_pt[node]) <= r2)
            out_idx[count++] = (double)t->node_pt[node];
        double plane = q[t->node_dim[node]] - t->node_val[node];
        int near = plane < 0 ? t->node_left[node] : t->node_right[node];
        int far = plane < 0 ? t->node_right[node] : t->node_left[node];
        if (near >= 0) stack[sp++] = near;
        if (plane * plane <= r2 && far >= 0) stack[sp++] = far;
    }
    free(stack);
    return count;
}
