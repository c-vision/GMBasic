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
#include "gmb_runtime.h"   /* gmb_solve_lin_tol (solver unico del runtime) */
#include <stddef.h>

enum {
    GMB_MATRIX_FLAT_SINM = 1,
    GMB_MATRIX_FLAT_TANM = 2,
    GMB_MATRIX_FLAT_ORTH = 3,
    GMB_MATRIX_FLAT_COSM = 4     /* matrix cosine (V·cos(Λ)·V^T), come sinm/tanm */
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
    case GMB_MATRIX_FLAT_COSM: r = gmb_matrix_cosm(h); break;
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

/* Solver lineare denso (P/Invoke dei backend managed): copia A/b e usa l'UNICO solver del
 * runtime `gmb_solve_lin_tol`. `tol` è la soglia di singolarità; ritorna 1 se risolto. */
int gmb_matrix_solve_lin(int64_t n, const double* a, const double* b, double* x, double tol)
{
    if (n <= 0 || !a || !b || !x) return 0;
    size_t nn = (size_t)n * (size_t)n;
    double* ac = (double*)malloc(sizeof(double) * nn);
    double* bc = (double*)malloc(sizeof(double) * (size_t)n);
    if (!ac || !bc) { free(ac); free(bc); return 0; }
    for (size_t i = 0; i < nn; i++) ac[i] = a[i];
    for (int64_t i = 0; i < n; i++) bc[i] = b[i];
    int ok = gmb_solve_lin_tol((int)n, ac, bc, x, tol);
    free(ac); free(bc);
    return ok;
}
