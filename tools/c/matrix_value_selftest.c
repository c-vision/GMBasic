/* ============================================================================
 * matrix_value_selftest.c — self-test del descrittore di matrice a valore
 * (ADR-MATRIX-Valore-Descrittore-INOUT.md, fase F0).
 *
 * Verifica: create/identity, get/set, view a strides (slice/row/col),
 * transpose view (O(1)), clone indipendente, contiguità e refcount.
 *
 * Build+run (dalla radice del monorepo):
 *   cc -std=c11 -Wall -Wextra -O1 -I GMBasic/native/gmb_runtime_c/include \
 *      tools/c/matrix_value_selftest.c \
 *      GMBasic/native/gmb_runtime_c/src/gmb_matrix_value.c \
 *      -o /tmp/matrix_value_selftest && /tmp/matrix_value_selftest
 * ========================================================================== */
#include "gmb_matrix_value.h"

#include <stdio.h>
#include <stdlib.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail++; } \
    else          { printf("ok: %s\n", msg); } \
} while (0)

int main(void)
{
    /* 3x3 identità + modifica */
    gmb_matrix_t a = gmb_matrix_create_value(3, 3);
    CHECK(a.st != NULL && a.rows == 3 && a.cols == 3, "create 3x3");
    CHECK(a.st->refs == 1, "create: refs=1");
    CHECK(gmb_matrix_get(a, 0, 0) == 1.0 && gmb_matrix_get(a, 1, 2) == 0.0, "create: identità");
    gmb_matrix_set(a, 1, 2, 7.0);
    CHECK(gmb_matrix_get(a, 1, 2) == 7.0, "set/get");
    CHECK(gmb_matrix_flat(a) != NULL && gmb_matrix_is_contiguous(a), "3x3 contigua");
    CHECK(gmb_matrix_flat(a)[1 * 3 + 2] == 7.0, "flat = row-major");

    /* view slice 2x2 da (1,1): condivide lo storage (refs=2) */
    gmb_matrix_t v = gmb_matrix_view(a, 1, 1, 2, 2);
    CHECK(v.st == a.st && a.st->refs == 2, "slice: storage condiviso (refs=2)");
    CHECK(v.rows == 2 && v.cols == 2, "slice: dimensioni");
    CHECK(gmb_matrix_get(v, 0, 1) == 7.0, "slice: valore (1,2) via view");
    gmb_matrix_set(v, 0, 0, 42.0);
    CHECK(gmb_matrix_get(a, 1, 1) == 42.0, "slice: scrittura visibile sull'owner");

    /* row/col view */
    gmb_matrix_t r1 = gmb_matrix_row_view(a, 2);
    CHECK(r1.rows == 1 && r1.cols == 3 && gmb_matrix_get(r1, 0, 2) == 1.0, "row view");
    gmb_matrix_t c1 = gmb_matrix_col_view(a, 2);
    CHECK(c1.rows == 3 && c1.cols == 1 && gmb_matrix_get(c1, 0, 0) == 0.0, "col view");

    /* transpose view: O(1), strides scambiati, non contigua */
    gmb_matrix_t t = gmb_matrix_transpose_view(a);
    CHECK(t.st == a.st && a.st->refs == 5, "transpose: view (refs=5)");
    CHECK(t.rows == 3 && t.cols == 3, "transpose: dimensioni");
    CHECK(gmb_matrix_get(t, 2, 1) == 7.0, "transpose: a(1,2) -> t(2,1)");
    CHECK(!gmb_matrix_is_contiguous(t) && gmb_matrix_flat(t) == NULL, "transpose: non contigua, flat=NULL");

    /* clone: indipendente e contiguo */
    gmb_matrix_t c = gmb_matrix_clone(a);
    CHECK(c.st != a.st && c.st->refs == 1, "clone: storage nuovo");
    CHECK(gmb_matrix_is_contiguous(c), "clone: contigua");
    CHECK(gmb_matrix_get(c, 1, 2) == 7.0 && gmb_matrix_get(c, 1, 1) == 42.0, "clone: valori");
    gmb_matrix_set(c, 1, 2, -1.0);
    CHECK(gmb_matrix_get(a, 1, 2) == 7.0, "clone: scrittura NON tocca l'owner");

    /* from_flat / adopt_flat */
    double src[4] = {1, 2, 3, 4};
    gmb_matrix_t f = gmb_matrix_from_flat(src, 2, 2);
    CHECK(gmb_matrix_get(f, 1, 0) == 3.0, "from_flat");
    double* owned = (double*)malloc(6 * sizeof(double));
    for (int i = 0; i < 6; i++) owned[i] = (double)i;
    gmb_matrix_t ad = gmb_matrix_adopt_flat(owned, 2, 3);
    CHECK(gmb_matrix_get(ad, 1, 2) == 5.0, "adopt_flat");

    /* release: refs torna a 0 solo dopo owner + 3 view */
    gmb_matrix_storage_t* st = a.st;
    gmb_matrix_free(&v);
    gmb_matrix_free(&r1);
    gmb_matrix_free(&c1);
    CHECK(st->refs == 2, "release view: restano owner+transpose");
    gmb_matrix_free(&t);
    CHECK(st->refs == 1, "release transpose: resta l'owner");
    gmb_matrix_free(&a);
    /* st è libero: non dereferenziare */
    CHECK(a.st == NULL, "free: descrittore azzerato");

    gmb_matrix_free(&c);
    gmb_matrix_free(&f);
    gmb_matrix_free(&ad);

    /* out-of-range non fa crash */
    gmb_matrix_t e = gmb_matrix_empty();
    CHECK(gmb_matrix_get(e, 0, 0) == 0.0, "empty: get=0");
    gmb_matrix_set(e, 0, 0, 1.0);
    CHECK(gmb_matrix_get(e, 0, 0) == 0.0, "empty: set no-op");
    gmb_matrix_free(&e);

    if (g_fail) { fprintf(stderr, "%d FAIL\n", g_fail); return 1; }
    printf("matrix_value_selftest: OK\n");
    return 0;
}
