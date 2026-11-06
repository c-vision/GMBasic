/*
 * localsearch_ffi.c — shim ABI per il motore local search (namespace OPTIMIZE).
 *
 * La valutazione C è `gor_ls_score (*)(const int*, int, void*)`: il trampolino FFI
 * fornisce `double (*)(const double*, int64)`, quindi il bridge converte lo stato
 * e mappa il valore su hard=0 (ammissibile) e soft=valore restituito da BASIC.
 * Nearest/cheapest insertion usano una matrice distanza (niente callback a 2 int).
 */
#include "gor_ls.h"

#include <stdlib.h>
#include <string.h>

typedef long long i64;
typedef double (*ffx_ls_cb)(const double *, long long);

static ffx_ls_cb g_ls_cb;

static gor_ls_score ffx_ls_bridge(const int *state, int n, void *ctx)
{
    gor_ls_score s;
    double *buf;
    int i;
    (void)ctx;
    s.hard = 0;
    s.soft = 0.0;
    if (state == NULL || n <= 0 || g_ls_cb == NULL) return s;
    buf = (double *)malloc((size_t)n * sizeof(double));
    if (buf == NULL) return s;
    for (i = 0; i < n; i++) buf[i] = (double)state[i];
    s.soft = (gor_real)g_ls_cb(buf, (long long)n);
    free(buf);
    return s;
}

static int run_ls(ffx_ls_cb f, i64 *initial, i64 n, i64 shape, i64 max_steps,
                  i64 starts, i64 acceptor, double *out)
{
    gor_ls_config cfg;
    gor_ls_result res;
    int *init, *best;
    int r;
    i64 i;
    if (f == NULL || initial == NULL || out == NULL || n <= 0 || max_steps <= 0) return -1;

    init = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    if (init == NULL || best == NULL) { free(init); free(best); return -1; }
    for (i = 0; i < n; i++) init[i] = (int)initial[i];

    gor_ls_config_default(&cfg, (gor_ls_shape)shape, (int)n, ffx_ls_bridge, NULL);
    g_ls_cb = f;                         /* impostato PRIMA della solve */
    cfg.termination.spent_ms = 0;        /* niente tempo: solo step_count */
    cfg.termination.step_count = max_steps;
    cfg.starts = (int)(starts > 0 ? starts : 1);
    if (acceptor >= 0) gor_ls_acceptor_init(&cfg.acceptor, (gor_ls_acceptor_kind)acceptor,
                                            (gor_ls_score){0, 0.0}, 1u);

    memset(&res, 0, sizeof(res));
    res.best_state = best;
    r = gor_ls_solve(&cfg, init, &res);
    if (r == GOR_OK) {
        /* Lo stato MIGLIORE torna al chiamante dentro `initial` (in/out): senza questo
         * il risultato della ricerca era inutilizzabile (il buffer best restava interno
         * e veniva liberato). Vedi localsearch.inc. */
        for (i = 0; i < n; i++) initial[i] = (i64)res.best_state[i];
        out[0] = (double)res.best.hard;
        out[1] = (double)res.best.soft;
        out[2] = (double)res.steps;
    }
    free(init);
    free(best);
    return (r == GOR_OK) ? 1 : -1;
}

i64 ofx_ls_solve(ffx_ls_cb f, i64 *initial, i64 n, i64 shape, i64 max_steps, double *out)
{
    return run_ls(f, initial, n, shape, max_steps, 1, 0 /* hill climbing */, out);
}

i64 ofx_ls_solve_cfg(ffx_ls_cb f, i64 *initial, i64 n, i64 shape, i64 max_steps,
                     i64 starts, i64 acceptor, double *out)
{
    return run_ls(f, initial, n, shape, max_steps, starts, acceptor, out);
}

/* Nearest neighbour da `start` sulla matrice dist(n*n). */
i64 ofx_ls_nearest(const double *dist, i64 n, i64 start, i64 *perm)
{
    int *used;
    i64 i, k, cur;
    if (n <= 0 || dist == NULL || perm == NULL || start < 0 || start >= n) return -1;
    used = (int *)calloc((size_t)n, sizeof(int));
    if (used == NULL) return -1;
    cur = start;
    used[cur] = 1;
    perm[0] = cur;
    for (k = 1; k < n; k++) {
        i64 best = -1;
        double bd = 0.0;
        for (i = 0; i < n; i++) {
            if (used[i]) continue;
            if (best < 0 || dist[(size_t)cur * (size_t)n + (size_t)i] < bd) {
                bd = dist[(size_t)cur * (size_t)n + (size_t)i];
                best = i;
            }
        }
        if (best < 0) break;
        perm[k] = best;
        used[best] = 1;
        cur = best;
    }
    free(used);
    return 1;
}

/* Cheapest insertion (ciclo): parte da {0}. */
i64 ofx_ls_cheapest(const double *dist, i64 n, i64 *perm)
{
    int *route, *used;
    i64 len = 1, node, i, pos;
    if (n <= 0 || dist == NULL || perm == NULL) return -1;
    route = (int *)malloc((size_t)n * sizeof(int));
    used = (int *)calloc((size_t)n, sizeof(int));
    if (route == NULL || used == NULL) { free(route); free(used); return -1; }
    route[0] = 0;
    used[0] = 1;
    for (node = 1; node < n; node++) {
        i64 best_pos = 0;
        double best_delta = 0.0;
        int have = 0;
        for (pos = 0; pos < len; pos++) {
            i64 a = route[pos];
            i64 b = route[(pos + 1) % len];
            double delta = dist[(size_t)a * (size_t)n + (size_t)node]
                         + dist[(size_t)node * (size_t)n + (size_t)b]
                         - dist[(size_t)a * (size_t)n + (size_t)b];
            if (!have || delta < best_delta) { best_delta = delta; best_pos = pos + 1; have = 1; }
        }
        for (i = len; i > best_pos; i--) route[i] = route[i - 1];
        route[best_pos] = (int)node;
        used[node] = 1;
        len++;
    }
    for (i = 0; i < n; i++) perm[i] = (i64)route[i];
    free(route);
    free(used);
    return 1;
}

double ofx_ls_now_ms(void) { return (double)gor_ls_now_ms(); }

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
i64 ofx_ls_cheapest__ffi(const double *dist, int64_t dist_ext, i64 n, i64 *perm, int64_t perm_ext) { return ofx_ls_cheapest(dist, n, perm); }
i64 ofx_ls_nearest__ffi(const double *dist, int64_t dist_ext, i64 n, i64 start, i64 *perm, int64_t perm_ext) { return ofx_ls_nearest(dist, n, start, perm); }
i64 ofx_ls_solve__ffi(ffx_ls_cb f, i64 *initial, int64_t initial_ext, i64 n, i64 shape, i64 max_steps, double *out, int64_t out_ext) { return ofx_ls_solve(f, initial, n, shape, max_steps, out); }
i64 ofx_ls_solve_cfg__ffi(ffx_ls_cb f, i64 *initial, int64_t initial_ext, i64 n, i64 shape, i64 max_steps, i64 starts, i64 acceptor, double *out, int64_t out_ext) { return ofx_ls_solve_cfg(f, initial, n, shape, max_steps, starts, acceptor, out); }
