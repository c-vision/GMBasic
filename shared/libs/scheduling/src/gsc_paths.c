/* gsc_paths.c — cammini con finestre temporali (arrivo da gmbortools/gor_graph2.c). */
#include "gmbscheduling.h"

#include <stdlib.h>

#define GSC_VERSION "0.1.0"

const char *gsc_version(void) { return GSC_VERSION; }

const char *gsc_status_str(gsc_status status)
{
    switch (status) {
    case GSC_OK: return "OK";
    case GSC_ERR_ARG: return "ARGOMENTO NON VALIDO";
    case GSC_ERR_NOMEM: return "MEMORIA ESAURITA";
    case GSC_ERR_RANGE: return "INDICE FUORI RANGE";
    }
    return "SCONOSCIUTO";
}

static void ptw_dfs(int n, const int *adj, const double *start, const double *end,
                    double mc, int u, int t, char *used, int *cur, int curlen,
                    int *paths, int *lens, int max_paths, int *found)
{
    int v;
    if (u == t) {
        if (*found < max_paths) {
            int i;
            for (i = 0; i < curlen; i++) paths[(size_t)(*found) * (size_t)n + (size_t)i] = cur[i];
            lens[*found] = curlen;
            (*found)++;
        }
        return;
    }
    if (curlen >= n) return;
    for (v = 0; v < n; v++) {
        if (!adj[(size_t)u * (size_t)n + (size_t)v] || used[v]) continue;
        if (end[u] + mc > start[v]) continue;   /* connessione non ammissibile */
        used[v] = 1;
        cur[curlen] = v;
        ptw_dfs(n, adj, start, end, mc, v, t, used, cur, curlen + 1,
                paths, lens, max_paths, found);
        used[v] = 0;
    }
}

int gsc_paths_time_window(int n, const int *adj, const double *windows,
                          int s, int t, int *paths, int *lens, int max_paths)
{
    char *used;
    int *cur, found = 0;
    if (n < 1 || adj == NULL || windows == NULL || paths == NULL || lens == NULL || max_paths < 1)
        return -1;
    if (s < 0 || s >= n || t < 0 || t >= n) return -1;

    used = (char *)calloc((size_t)n, 1);
    cur = (int *)malloc((size_t)n * sizeof(int));
    if (used == NULL || cur == NULL) { free(used); free(cur); return -1; }

    used[s] = 1;
    cur[0] = s;
    ptw_dfs(n, adj, windows, windows + n, windows[2 * (size_t)n], s, t,
            used, cur, 1, paths, lens, max_paths, &found);
    free(used);
    free(cur);
    return found;
}
