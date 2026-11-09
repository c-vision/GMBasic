/* ============================================================================
 * gmb_matrix_value.h — Descrittore di matrice a VALORE (ADR-MATRIX-Valore).
 *
 * Il tipo `MATRIX OF (I1, …, In, T)` è un descrittore {storage, dims, strides}:
 * gli elementi NON sono indicizzati a mano (row-major fisso), ma SEMPRE tramite
 * gmb_matrix_elem()/gmb_matrix_get()/gmb_matrix_set() — così una "view"
 * (SLICE/TRANSPOSE/ROW/COL) condivide lo storage del proprietario senza copiare.
 *
 * Lo storage ha un REFCOUNT: un valore lo possiede (refs=1); le view fanno
 * retain sul proprietario e lo tengono vivo. La copia di un descrittore
 * condivide lo storage (reference semantics, come gli array CLR); la copia
 * INDIPENDENTE è gmb_matrix_clone().
 *
 * Questa è l'UNICA implementazione (§0bis): VM/C, C99Native e (via P/Invoke)
 * IL/PCode chiamano queste funzioni. Niente logica duplicata nei backend.
 * ============================================================================ */
#ifndef GMB_MATRIX_VALUE_H
#define GMB_MATRIX_VALUE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmb_matrix_storage {
    double*  data;       /* base dello storage (row-major o layout arbitrario) */
    int64_t  capacity;   /* double allocati in data                          */
    int64_t  refs;       /* refcount (>0 = vivo; 0 = libero)                 */
    uint32_t flags;      /* GMB_MATRIX_F_*                                   */
} gmb_matrix_storage_t;

#define GMB_MATRIX_F_OWNED      1u   /* lo storage possiede data (free)        */
/* La CONTIGUITÀ è del DESCRITTORE (dims/strides/offset), non dello storage:
 * vedi gmb_matrix_is_contiguous(). */

typedef struct gmb_matrix {
    gmb_matrix_storage_t* st;         /* NULL = matrice vuota/nil              */
    int64_t rows, cols;
    int64_t row_stride;               /* salto tra righe, in double             */
    int64_t col_stride;               /* salto tra colonne (1 = contigua)       */
    int64_t offset;                   /* indice del primo elemento in st->data  */
} gmb_matrix_t;

/* ── descrittore ─────────────────────────────────────────────────────────── */
static inline gmb_matrix_t gmb_matrix_empty(void)
{
    gmb_matrix_t m;
    m.st = NULL; m.rows = 0; m.cols = 0;
    m.row_stride = 0; m.col_stride = 1; m.offset = 0;
    return m;
}
static inline int gmb_matrix_is_empty(const gmb_matrix_t* m)
{ return !m || !m->st || m->rows <= 0 || m->cols <= 0; }

/* Elemento (r,c) — UNICO punto di indicizzazione. */
static inline double* gmb_matrix_elem(const gmb_matrix_t* m, int64_t r, int64_t c)
{
    return &m->st->data[m->offset + r * m->row_stride + c * m->col_stride];
}

/* ── storage + refcount ──────────────────────────────────────────────────── */
/* Storage nuovo (data azzerato), refs=1, OWNED. NULL su OOM/dimensioni invalide. */
gmb_matrix_storage_t* gmb_matrix_storage_new(int64_t capacity);
void gmb_matrix_storage_retain(gmb_matrix_storage_t* st);
void gmb_matrix_storage_release(gmb_matrix_storage_t* st);

/* ── costruzione ─────────────────────────────────────────────────────────── */
/* Matrice rows×cols contigua, identità sui primi min(rows,cols) (canone MATRIX.CREATE). */
gmb_matrix_t gmb_matrix_create_value(int64_t rows, int64_t cols);
/* Copia di un buffer flat row-major (ownership dello storage: nuovo). */
gmb_matrix_t gmb_matrix_from_flat(const double* data, int64_t rows, int64_t cols);
/* Adotta un buffer malloc'ed (ownership trasferita allo storage). */
gmb_matrix_t gmb_matrix_adopt_flat(double* data, int64_t rows, int64_t cols);
/* Copia INDIPENDENTE (contigua); rompe l'aliasing con eventuali view. */
gmb_matrix_t gmb_matrix_clone(gmb_matrix_t m);
/* Rilascia il valore (refcount--; azzera il descrittore). */
void gmb_matrix_free(gmb_matrix_t* m);

/* ── accesso ─────────────────────────────────────────────────────────────── */
int  gmb_matrix_is_contiguous(gmb_matrix_t m);
/* Puntatore al primo elemento SE contigua (per FFI/legacy), altrimenti NULL. */
double* gmb_matrix_flat(gmb_matrix_t m);
double gmb_matrix_get(gmb_matrix_t m, int64_t r, int64_t c);       /* 0 se fuori range */
void   gmb_matrix_set(gmb_matrix_t m, int64_t r, int64_t c, double v); /* no-op se fuori range */

/* ── view (O(1), condividono lo storage) ─────────────────────────────────── */
/* Sotto-matrice righe [r0, r0+rows) e colonne [c0, c0+cols). */
gmb_matrix_t gmb_matrix_view(gmb_matrix_t m, int64_t r0, int64_t c0,
                             int64_t rows, int64_t cols);
gmb_matrix_t gmb_matrix_transpose_view(gmb_matrix_t m);
gmb_matrix_t gmb_matrix_row_view(gmb_matrix_t m, int64_t row);
gmb_matrix_t gmb_matrix_col_view(gmb_matrix_t m, int64_t col);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MATRIX_VALUE_H */
