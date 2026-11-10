/* ============================================================================
 * gmb_matrix_value.h — Descrittore di matrice a VALORE (ADR-MATRIX-Valore).
 *
 * Il tipo `MATRIX OF (I1, …, In, T)` è un descrittore {storage, dims, strides}:
 * gli elementi NON sono indicizzati a mano (row-major fisso), ma SEMPRE tramite
 * gmb_matrix_elem()/gmb_matrix_value_get()/gmb_matrix_value_set() — così una "view"
 * (SLICE/TRANSPOSE/ROW/COL) condivide lo storage del proprietario senza copiare.
 *
 * Lo storage ha un REFCOUNT: un valore lo possiede (refs=1); le view fanno
 * retain sul proprietario e lo tengono vivo. La copia di un descrittore
 * condivide lo storage (reference semantics, come gli array CLR); la copia
 * INDIPENDENTE è gmb_matrix_value_clone().
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
gmb_matrix_t gmb_matrix_value_from_flat(const double* data, int64_t rows, int64_t cols);
/* Adotta un buffer malloc'ed (ownership trasferita allo storage). */
gmb_matrix_t gmb_matrix_adopt_flat(double* data, int64_t rows, int64_t cols);
/* Copia INDIPENDENTE (contigua); rompe l'aliasing con eventuali view. */
gmb_matrix_t gmb_matrix_value_clone(gmb_matrix_t m);
/* Rilascia il valore (refcount--; azzera il descrittore). */
void gmb_matrix_value_free(gmb_matrix_t* m);
/* Assegna un valore a un descrittore ESISTENTE (retain del nuovo, release del vecchio):
 * è ciò che serve a un riassegnamento nella VM/emitter. Sicura con self-assignment
 * (anche quando src è una view dello stesso storage di *dst). */
void gmb_matrix_value_assign(gmb_matrix_t* dst, gmb_matrix_t src);
/* Copia CONTIGUA row-major (malloc: il chiamante la libera con free()), per i confini
 * legacy/FFI che richiedono un double*. Vuota/nil → NULL. */
double* gmb_matrix_value_to_flat(gmb_matrix_t m);
/* RESHAPE: view sullo stesso storage se il descrittore è contiguo (row-major) e la
 * cardinalità coincide, altrimenti copia contigua indipendente. */
gmb_matrix_t gmb_matrix_value_reshape(gmb_matrix_t m, int64_t rows, int64_t cols);

/* ── accesso ─────────────────────────────────────────────────────────────── */
int  gmb_matrix_is_contiguous(gmb_matrix_t m);
/* Puntatore al primo elemento SE contigua (per FFI/legacy), altrimenti NULL. */
double* gmb_matrix_flat(gmb_matrix_t m);
double gmb_matrix_value_get(gmb_matrix_t m, int64_t r, int64_t c);       /* 0 se fuori range */
void   gmb_matrix_value_set(gmb_matrix_t m, int64_t r, int64_t c, double v); /* no-op se fuori range */

/* ── view (O(1), condividono lo storage) ─────────────────────────────────── */
/* Sotto-matrice righe [r0, r0+rows) e colonne [c0, c0+cols). */
gmb_matrix_t gmb_matrix_view(gmb_matrix_t m, int64_t r0, int64_t c0,
                             int64_t rows, int64_t cols);
gmb_matrix_t gmb_matrix_transpose_view(gmb_matrix_t m);
gmb_matrix_t gmb_matrix_row_view(gmb_matrix_t m, int64_t row);
gmb_matrix_t gmb_matrix_col_view(gmb_matrix_t m, int64_t col);

/* ── operazioni a valore (risultato: storage NUOVO contiguo) ──────────────── */
/* A·B; vuota se le dimensioni non sono compatibili. */
gmb_matrix_t gmb_matrix_value_multiply(gmb_matrix_t a, gmb_matrix_t b);
/* A + v per riga (v di lunghezza cols) / per colonna (v di lunghezza rows). */
gmb_matrix_t gmb_matrix_value_add_row_vector(gmb_matrix_t a, const double* v, int64_t n);
gmb_matrix_t gmb_matrix_value_add_col_vector(gmb_matrix_t a, const double* v, int64_t n);
/* Concatenazione: [A; B] (stesse colonne) e [A B] (stesse righe). */
gmb_matrix_t gmb_matrix_value_concat_rows(gmb_matrix_t a, gmb_matrix_t b);
gmb_matrix_t gmb_matrix_value_concat_cols(gmb_matrix_t a, gmb_matrix_t b);

/* ── ponte TRANSITORIO handle ↔ descrittore (ADR §5) ─────────────────────── */
/* value_of: copia contigua dal registry handle. handle_of: registra una copia
 * contigua nel registry handle. Servono solo durante la migrazione: i nuovi
 * percorsi a valore non devono dipendere dal registry. */
gmb_matrix_t gmb_matrix_value_of(int64_t handle);
int64_t gmb_matrix_handle_of(gmb_matrix_t m);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MATRIX_VALUE_H */
