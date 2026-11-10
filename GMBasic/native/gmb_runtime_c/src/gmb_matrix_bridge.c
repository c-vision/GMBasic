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
