/* ============================================================================
 * gmb_matrix_value.c — Descrittore di matrice a valore (ADR-MATRIX-Valore).
 * Implementazione UNICA per VM/C e C99Native (§0bis). Vedi gmb_matrix_value.h.
 * ============================================================================ */
#include "gmb_matrix_value.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static int64_t gmb_mref_inc(int64_t* p) { return (int64_t)InterlockedIncrement64((volatile LONG64*)p); }
static int64_t gmb_mref_dec(int64_t* p) { return (int64_t)InterlockedDecrement64((volatile LONG64*)p); }
#else
static int64_t gmb_mref_inc(int64_t* p) { return (int64_t)__sync_add_and_fetch(p, 1); }
static int64_t gmb_mref_dec(int64_t* p) { return (int64_t)__sync_sub_and_fetch(p, 1); }
#endif

gmb_matrix_storage_t* gmb_matrix_storage_new(int64_t capacity)
{
    if (capacity < 0) return NULL;
    gmb_matrix_storage_t* st = (gmb_matrix_storage_t*)calloc(1, sizeof(*st));
    if (!st) return NULL;
    if (capacity > 0) {
        st->data = (double*)calloc((size_t)capacity, sizeof(double));
        if (!st->data) { free(st); return NULL; }
    }
    st->capacity = capacity;
    st->refs = 1;
    st->flags = GMB_MATRIX_F_OWNED;
    return st;
}

void gmb_matrix_storage_retain(gmb_matrix_storage_t* st)
{
    if (st) gmb_mref_inc(&st->refs);
}

void gmb_matrix_storage_release(gmb_matrix_storage_t* st)
{
    if (!st) return;
    if (gmb_mref_dec(&st->refs) != 0) return;
    if (st->flags & GMB_MATRIX_F_OWNED) free(st->data);
    free(st);
}

static gmb_matrix_t gmb_matrix_make(gmb_matrix_storage_t* st,
                                    int64_t rows, int64_t cols,
                                    int64_t row_stride, int64_t col_stride,
                                    int64_t offset)
{
    gmb_matrix_t m;
    m.st = st;
    m.rows = rows;
    m.cols = cols;
    m.row_stride = row_stride;
    m.col_stride = col_stride;
    m.offset = offset;
    return m;
}

gmb_matrix_t gmb_matrix_create_value(int64_t rows, int64_t cols)
{
    if (rows < 0 || cols < 0) return gmb_matrix_empty();
    /* rows*cols in int64 con overflow-check: capacity negativa → vuota. */
    if (rows != 0 && cols > INT64_MAX / rows) return gmb_matrix_empty();
    int64_t n = rows * cols;
    gmb_matrix_storage_t* st = gmb_matrix_storage_new(n);
    if (!st) return gmb_matrix_empty();
    int64_t d = rows < cols ? rows : cols;
    for (int64_t i = 0; i < d; i++) st->data[i * cols + i] = 1.0;
    return gmb_matrix_make(st, rows, cols, cols, 1, 0);
}

gmb_matrix_t gmb_matrix_from_flat(const double* data, int64_t rows, int64_t cols)
{
    if (!data || rows < 0 || cols < 0) return gmb_matrix_empty();
    if (rows != 0 && cols > INT64_MAX / rows) return gmb_matrix_empty();
    int64_t n = rows * cols;
    gmb_matrix_storage_t* st = gmb_matrix_storage_new(n);
    if (!st) return gmb_matrix_empty();
    if (n > 0) memcpy(st->data, data, (size_t)n * sizeof(double));
    return gmb_matrix_make(st, rows, cols, cols, 1, 0);
}

gmb_matrix_t gmb_matrix_adopt_flat(double* data, int64_t rows, int64_t cols)
{
    if (rows < 0 || cols < 0) { free(data); return gmb_matrix_empty(); }
    if (rows != 0 && cols > INT64_MAX / rows) { free(data); return gmb_matrix_empty(); }
    gmb_matrix_storage_t* st = (gmb_matrix_storage_t*)calloc(1, sizeof(*st));
    if (!st) { free(data); return gmb_matrix_empty(); }
    st->data = data;
    st->capacity = rows * cols;
    st->refs = 1;
    st->flags = GMB_MATRIX_F_OWNED;
    return gmb_matrix_make(st, rows, cols, cols, 1, 0);
}

int gmb_matrix_is_contiguous(gmb_matrix_t m)
{
    if (!m.st) return 1;
    if (m.rows <= 1 || m.cols <= 1) return 1;
    return m.col_stride == 1 && m.row_stride == m.cols;
}

double* gmb_matrix_flat(gmb_matrix_t m)
{
    if (!m.st) return NULL;
    if (!gmb_matrix_is_contiguous(m)) return NULL;
    return m.st->data + m.offset;
}

double gmb_matrix_get(gmb_matrix_t m, int64_t r, int64_t c)
{
    if (!m.st || r < 0 || r >= m.rows || c < 0 || c >= m.cols) return 0.0;
    return *gmb_matrix_elem(&m, r, c);
}

void gmb_matrix_set(gmb_matrix_t m, int64_t r, int64_t c, double v)
{
    if (!m.st || r < 0 || r >= m.rows || c < 0 || c >= m.cols) return;
    *gmb_matrix_elem(&m, r, c) = v;
}

gmb_matrix_t gmb_matrix_clone(gmb_matrix_t m)
{
    if (!m.st) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_create_value(m.rows, m.cols);
    if (!out.st) return out;
    for (int64_t r = 0; r < m.rows; r++)
        for (int64_t c = 0; c < m.cols; c++)
            *gmb_matrix_elem(&out, r, c) = *gmb_matrix_elem(&m, r, c);
    return out;
}

void gmb_matrix_free(gmb_matrix_t* m)
{
    if (!m) return;
    gmb_matrix_storage_release(m->st);
    *m = gmb_matrix_empty();
}

gmb_matrix_t gmb_matrix_view(gmb_matrix_t m, int64_t r0, int64_t c0,
                             int64_t rows, int64_t cols)
{
    if (!m.st || r0 < 0 || c0 < 0 || rows < 0 || cols < 0) return gmb_matrix_empty();
    if (r0 + rows > m.rows || c0 + cols > m.cols) return gmb_matrix_empty();
    gmb_matrix_storage_retain(m.st);
    return gmb_matrix_make(m.st, rows, cols, m.row_stride, m.col_stride,
                           m.offset + r0 * m.row_stride + c0 * m.col_stride);
}

gmb_matrix_t gmb_matrix_transpose_view(gmb_matrix_t m)
{
    if (!m.st) return gmb_matrix_empty();
    gmb_matrix_storage_retain(m.st);
    return gmb_matrix_make(m.st, m.cols, m.rows, m.col_stride, m.row_stride, m.offset);
}

gmb_matrix_t gmb_matrix_row_view(gmb_matrix_t m, int64_t row)
{
    return gmb_matrix_view(m, row, 0, 1, m.cols);
}

gmb_matrix_t gmb_matrix_col_view(gmb_matrix_t m, int64_t col)
{
    return gmb_matrix_view(m, 0, col, m.rows, 1);
}

/* ── operazioni a valore (storage nuovo contiguo) ─────────────────────────── */
/* Unica implementazione per i backend C quando la matrice e' un descrittore.
 * Leggono SEMPRE via gmb_matrix_get (strides-safe) e scrivono su un risultato
 * contiguo. */

gmb_matrix_t gmb_matrix_value_multiply(gmb_matrix_t a, gmb_matrix_t b)
{
    if (!a.st || !b.st || a.cols != b.rows) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_create_value(a.rows, b.cols);
    if (!out.st) return gmb_matrix_empty();
    /* create_value inizializza l'identità: azzera prima del prodotto. */
    for (int64_t r = 0; r < out.rows; r++)
        for (int64_t c = 0; c < out.cols; c++) out.st->data[r * out.cols + c] = 0.0;
    for (int64_t r = 0; r < a.rows; r++)
        for (int64_t k = 0; k < a.cols; k++)
        {
            double av = gmb_matrix_get(a, r, k);
            if (av == 0.0) continue;
            for (int64_t c = 0; c < b.cols; c++)
                out.st->data[r * out.cols + c] += av * gmb_matrix_get(b, k, c);
        }
    return out;
}

gmb_matrix_t gmb_matrix_value_add_row_vector(gmb_matrix_t a, const double* v, int64_t n)
{
    if (!a.st || !v) return gmb_matrix_empty();
    if (n < a.cols) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_clone(a);
    if (!out.st) return gmb_matrix_empty();
    for (int64_t r = 0; r < out.rows; r++)
        for (int64_t c = 0; c < out.cols; c++)
            out.st->data[r * out.cols + c] += v[c];
    return out;
}

gmb_matrix_t gmb_matrix_value_add_col_vector(gmb_matrix_t a, const double* v, int64_t n)
{
    if (!a.st || !v) return gmb_matrix_empty();
    if (n < a.rows) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_clone(a);
    if (!out.st) return gmb_matrix_empty();
    for (int64_t r = 0; r < out.rows; r++)
        for (int64_t c = 0; c < out.cols; c++)
            out.st->data[r * out.cols + c] += v[r];
    return out;
}

gmb_matrix_t gmb_matrix_value_concat_rows(gmb_matrix_t a, gmb_matrix_t b)
{
    if (!a.st || !b.st || a.cols != b.cols) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_create_value(a.rows + b.rows, a.cols);
    if (!out.st) return gmb_matrix_empty();
    for (int64_t r = 0; r < out.rows; r++)
        for (int64_t c = 0; c < out.cols; c++)
            out.st->data[r * out.cols + c] = r < a.rows
                ? gmb_matrix_get(a, r, c)
                : gmb_matrix_get(b, r - a.rows, c);
    return out;
}

gmb_matrix_t gmb_matrix_value_concat_cols(gmb_matrix_t a, gmb_matrix_t b)
{
    if (!a.st || !b.st || a.rows != b.rows) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_create_value(a.rows, a.cols + b.cols);
    if (!out.st) return gmb_matrix_empty();
    for (int64_t r = 0; r < out.rows; r++)
        for (int64_t c = 0; c < out.cols; c++)
            out.st->data[r * out.cols + c] = c < a.cols
                ? gmb_matrix_get(a, r, c)
                : gmb_matrix_get(b, r, c - a.cols);
    return out;
}
