/* ============================================================================
 * gmb_matrix_flat.c — ABI piatta dei kernel matriciali (P/Invoke C# → C).
 *
 * Implementazione minima: costruisce la matrice dall'input piatto, chiama lo
 * STESSO kernel usato da VM/C e C99Native (`gmb_matrix_sinm/tanm/orth`) e copia
 * il risultato in un buffer del chiamante. Nessuno stato condiviso, nessun
 * handle che attraversa il confine: evita il doppio registry managed/nativo.
 *
 * Vedi gmb_matrix_flat.h per il contratto. Esposta da `libgmb_matrix_flat`
 * (target SHARED di gmb_runtime_c/CMakeLists.txt).
 * ============================================================================ */
#include "gmb_matrix.h"
#include "gmb_matrix_flat.h"
#include <stddef.h>

enum {
    GMB_MATRIX_FLAT_SINM = 1,
    GMB_MATRIX_FLAT_TANM = 2,
    GMB_MATRIX_FLAT_ORTH = 3
};

int gmb_matrix_flat_op(const double* a, int64_t rows, int64_t cols, int op,
                       double* out, int64_t out_cap,
                       int64_t* out_rows, int64_t* out_cols)
{
    if (!a || !out || !out_rows || !out_cols) return 0;
    if (rows <= 0 || cols <= 0 || rows > 0x7fffffff || cols > 0x7fffffff) return 0;
    *out_rows = 0;
    *out_cols = 0;
    int64_t h = gmb_matrix_create_from((int)rows, (int)cols, a);
    if (h <= 0) return 0;
    int64_t r = -1;
    switch (op)
    {
    case GMB_MATRIX_FLAT_SINM: r = gmb_matrix_sinm(h); break;
    case GMB_MATRIX_FLAT_TANM: r = gmb_matrix_tanm(h); break;
    case GMB_MATRIX_FLAT_ORTH: r = gmb_matrix_orth(h); break;
    default: break;
    }
    gmb_matrix_free(h);
    if (r <= 0) return 0;
    int rr = gmb_matrix_rows(r);
    int rc = gmb_matrix_cols(r);
    if (rr <= 0 || rc <= 0 || (int64_t)rr * (int64_t)rc > out_cap)
    {
        gmb_matrix_free(r);
        return 0;
    }
    for (int i = 0; i < rr; i++)
        for (int j = 0; j < rc; j++)
            out[(int64_t)i * rc + j] = gmb_matrix_get(r, i, j);
    *out_rows = rr;
    *out_cols = rc;
    gmb_matrix_free(r);
    return 1;
}
