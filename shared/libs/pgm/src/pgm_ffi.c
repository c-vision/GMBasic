/*
 * pgm_ffi.c — shim ABI piatto per il binding BASIC del namespace PGM.
 *
 * `gor_pgm` è una struct con stato (segmenti allocati). L'FFI di GMBasic passa solo
 * scalari e array, quindi qui c'è una piccola tabella di handle (massimo 16 indici
 * vivi per processo). Uso: BUILD -> handle, RANK/RANGE, FREE.
 *
 * Non è thread-safe: gli strumenti BASIC usano l'indice in modo sequenziale
 * (documentato nel README del pacchetto).
 */
#include "gor_pgm.h"

#include <stdlib.h>

typedef long long i64;

#define PGM_MAX 16

static gor_pgm *g_pgm[PGM_MAX];

/* Ritorna lo slot, o -1. */
static int pgm_slot(i64 h)
{
    if (h < 1 || h > PGM_MAX) return -1;
    return (int)(h - 1);
}

i64 gfx_pgm_build(const double *x, i64 n, i64 eps)
{
    int i;
    gor_pgm *p;
    if (n <= 0 || x == NULL) return -1;
    for (i = 0; i < PGM_MAX; i++) {
        if (g_pgm[i] != NULL) continue;
        p = (gor_pgm *)calloc(1, sizeof(*p));
        if (p == NULL) return -1;
        if (gor_pgm_build(p, (int)n, (const gor_real *)x, (int)eps) != 0) {
            free(p);
            return -1;
        }
        g_pgm[i] = p;
        return (i64)(i + 1);
    }
    return -1; /* tabella piena: manca FREE */
}

i64 gfx_pgm_rank(i64 h, double key)
{
    int s = pgm_slot(h);
    long lo = 0, hi = -1;
    if (s < 0 || g_pgm[s] == NULL) return -1;
    return (i64)gor_pgm_search(g_pgm[s], (gor_real)key, &lo, &hi);
}

/* out(0)=lo, out(1)=hi; ritorna il rank (o -1). */
i64 gfx_pgm_range(i64 h, double key, i64 *out)
{
    int s = pgm_slot(h);
    long lo = 0, hi = -1, rank;
    if (s < 0 || g_pgm[s] == NULL || out == NULL) return -1;
    rank = gor_pgm_search(g_pgm[s], (gor_real)key, &lo, &hi);
    out[0] = (i64)lo;
    out[1] = (i64)hi;
    return (i64)rank;
}

i64 gfx_pgm_free(i64 h)
{
    int s = pgm_slot(h);
    if (s < 0 || g_pgm[s] == NULL) return -1;
    gor_pgm_free(g_pgm[s]);
    free(g_pgm[s]);
    g_pgm[s] = NULL;
    return 0;
}
