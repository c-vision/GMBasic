/*
 * gn_ops.h — operazioni tensoriali di base (da tsnlab/connx, ONNX).
 *
 * Gather (embedding lookup), Slice e Concat su matrici row-major.
 */
#ifndef GMBNEURAL_GN_OPS_H
#define GMBNEURAL_GN_OPS_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gather di righe (embedding lookup): out[i][:] = data[indices[i]][:].
 * data: rows×cols; indices: n_idx; out: n_idx×cols. */
gn_status gn_gather_rows(gn_real *out, const gn_real *data, int rows, int cols,
                         const int *indices, int n_idx);
/* Slice di righe: out[k][:] = data[start+k][:], k=0..count-1. */
gn_status gn_slice_rows(gn_real *out, const gn_real *data, int rows, int cols,
                        int start, int count);
/* Concat di righe: le parti parts[i] (part_rows[i]×cols) sono copiate in
 * sequenza in out (somma(part_rows)×cols). */
gn_status gn_concat_rows(gn_real *out, const gn_real *const *parts,
                         const int *part_rows, int n_parts, int cols);
/* Copia un blocco (righe x colonne) da un tensore. */
gn_status gn_copy_block(gn_real *out, const gn_real *data, int rows, int cols,
                        int r0, int c0, int nr, int nc);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_OPS_H */
