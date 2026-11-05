/*
 * scheduling_ffi.c — shim ABI per gmbscheduling (namespace OPTIMIZE).
 *
 * L'API C usa `int` per adiacenza e cammini (array LONG di BASIC = int64): conversione
 * esplicita in ingresso e in uscita, come gli altri pacchetti.
 * `windows` = start[n] | end[n] | min_connect  (2*n + 1 valori, come documenta l'header).
 */
#include "gmbscheduling.h"

#include <stddef.h>
#include <stdlib.h>

typedef long long i64;

i64 ofx_sched_paths_time_window(const i64 *adj, const double *windows, i64 n, i64 s, i64 t,
                                i64 *paths, i64 *lens, i64 max_paths)
{
    int *a = NULL, *p = NULL, *l = NULL;
    i64 i;
    int r;
    if (n <= 0 || max_paths <= 0 || adj == NULL || windows == NULL || paths == NULL ||
        lens == NULL || s < 0 || t < 0 || s >= n || t >= n)
        return -1;
    a = (int *)malloc((size_t)(n * n) * sizeof(int));
    p = (int *)malloc((size_t)(max_paths * n) * sizeof(int));
    l = (int *)malloc((size_t)max_paths * sizeof(int));
    if (a == NULL || p == NULL || l == NULL) { free(a); free(p); free(l); return -1; }
    for (i = 0; i < n * n; i++) a[i] = (int)adj[i];
    for (i = 0; i < max_paths * n; i++) p[i] = 0;
    for (i = 0; i < max_paths; i++) l[i] = 0;

    r = gsc_paths_time_window((int)n, a, windows, (int)s, (int)t, p, l, (int)max_paths);
    if (r > 0) {
        for (i = 0; i < (i64)r; i++) lens[i] = (i64)l[i];
        for (i = 0; i < (i64)r * n; i++) paths[i] = (i64)p[i];
    }
    free(a); free(p); free(l);
    return (i64)r;
}
