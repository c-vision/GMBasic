/* ============================================================================
 * gmb_matrix_bridge.c — PONTE TRANSITORIO handle ↔ descrittore a valore
 * (ADR-MATRIX-Valore-Descrittore-INOUT.md §5).
 *
 * Durante la migrazione alcune API storiche lavorano ancora su handle
 * (registry `gmb_matrix.c`), mentre i nuovi percorsi usano il descrittore
 * `gmb_matrix_t` (`gmb_matrix_value.c`). Questo file e' l'UNICO punto di
 * conversione; i nuovi percorsi a valore non devono dipendere dal registry.
 * ============================================================================ */
#include "gmb_matrix.h"
#include "gmb_matrix_value.h"
#include <stdlib.h>

gmb_matrix_t gmb_matrix_value_of(int64_t handle)
{
    int rows = gmb_matrix_rows(handle);
    int cols = gmb_matrix_cols(handle);
    if (rows <= 0 || cols <= 0) return gmb_matrix_empty();
    gmb_matrix_t m = gmb_matrix_create_value(rows, cols);
    if (!m.st) return gmb_matrix_empty();
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            gmb_matrix_value_set(m, r, c, gmb_matrix_get(handle, r, c));
    return m;
}

int64_t gmb_matrix_handle_of(gmb_matrix_t m)
{
    if (!m.st || m.rows <= 0 || m.cols <= 0) return -1;
    int64_t h = gmb_matrix_create((int)m.rows, (int)m.cols);
    if (h <= 0) return -1;
    for (int64_t r = 0; r < m.rows; r++)
        for (int64_t c = 0; c < m.cols; c++)
            gmb_matrix_set(h, (int)r, (int)c, gmb_matrix_value_get(m, r, c));
    return h;
}

int64_t gmb_matrix_scratch_handle(gmb_matrix_t m)
{
    if (!m.st || m.rows <= 0 || m.cols <= 0) return -1;
    double* flat = gmb_matrix_value_to_flat(m);
    if (!flat) return -1;
    int64_t h = gmb_matrix_scratch((int)m.rows, (int)m.cols, flat);
    free(flat);
    return h;
}

gmb_matrix_t gmb_matrix_decomp_factor_value_named(int64_t decomp, const char* name)
{
    int owned = 0;
    int64_t h = gmb_matrix_decomp_get_factor_named_ex(decomp, name, &owned);
    if (h <= 0) return gmb_matrix_empty();
    int rows = gmb_matrix_rows(h), cols = gmb_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { if (owned) gmb_matrix_free(h); return gmb_matrix_empty(); }
    gmb_matrix_t out = gmb_matrix_value_zeros(rows, cols);
    if (!out.st) { if (owned) gmb_matrix_free(h); return out; }
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            gmb_matrix_value_set(out, r, c, gmb_matrix_get(h, r, c));
    if (owned) gmb_matrix_free(h);   /* fattore generato (es. EIG DREAL) */
    return out;
}

int64_t gmb_matrix_decomp_factor_scratch_named(int64_t decomp, const char* name)
{
    int owned = 0;
    int64_t h = gmb_matrix_decomp_get_factor_named_ex(decomp, name, &owned);
    if (h <= 0) return -1;
    gmb_matrix_t src = gmb_matrix_value_of(h);
    if (owned) gmb_matrix_free(h);
    if (!src.st) return -1;
    int rows = (int)src.rows, cols = (int)src.cols;
    double* flat = gmb_matrix_value_to_flat(src);
    gmb_matrix_value_free(&src);
    if (!flat) return -1;
    int64_t sh = gmb_matrix_scratch(rows, cols, flat);
    free(flat);
    return sh;
}
