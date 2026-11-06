/*
 * gn_ops.c — Gather/Slice/Concat (da tsnlab/connx).
 */
#include "gn_ops.h"
#include <string.h>

gn_status gn_gather_rows(gn_real *out, const gn_real *data, int rows, int cols,
                         const int *indices, int n_idx)
{
    int i;
    if (!out || !data || !indices || rows < 1 || cols < 1 || n_idx < 1) return GN_ERR_ARG;
    for (i = 0; i < n_idx; i++) {
        int idx = indices[i];
        if (idx < 0 || idx >= rows) return GN_ERR_RANGE;
        memcpy(out + (size_t)i * cols, data + (size_t)idx * cols, (size_t)cols * sizeof(gn_real));
    }
    return GN_OK;
}

gn_status gn_slice_rows(gn_real *out, const gn_real *data, int rows, int cols,
                        int start, int count)
{
    if (!out || !data || rows < 1 || cols < 1 || count < 0 || start < 0 || start + count > rows)
        return GN_ERR_ARG;
    if (count > 0) memcpy(out, data + (size_t)start * cols, (size_t)count * cols * sizeof(gn_real));
    return GN_OK;
}

gn_status gn_concat_rows(gn_real *out, const gn_real *const *parts,
                         const int *part_rows, int n_parts, int cols)
{
    int i, off = 0;
    if (!out || !parts || !part_rows || n_parts < 1 || cols < 1) return GN_ERR_ARG;
    for (i = 0; i < n_parts; i++) {
        if (!parts[i] || part_rows[i] < 0) return GN_ERR_ARG;
        if (part_rows[i] > 0)
            memcpy(out + (size_t)off * cols, parts[i], (size_t)part_rows[i] * cols * sizeof(gn_real));
        off += part_rows[i];
    }
    return GN_OK;
}

gn_status gn_copy_block(gn_real *out, const gn_real *data, int rows, int cols,
                        int r0, int c0, int nr, int nc)
{
    int i;
    if (!out || !data || rows < 1 || cols < 1 || nr < 0 || nc < 0 ||
        r0 < 0 || c0 < 0 || r0 + nr > rows || c0 + nc > cols) return GN_ERR_ARG;
    for (i = 0; i < nr; i++)
        memcpy(out + (size_t)i * nc, data + (size_t)(r0 + i) * cols + c0, (size_t)nc * sizeof(gn_real));
    return GN_OK;
}
