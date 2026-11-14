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
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>

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

/* ============================================================================
 * Valori MATRICE dentro un gmb_val_t (ADR-MATRIX F4): le DECOMPOSIZIONI sono
 * DIZIONARI di valori matrice, non handle. Il box è refcountato: condivide lo
 * storage della matrice (retain) e lo libera alla release.
 * ============================================================================ */
static void matrix_box_release(gmb_obj_t* o)
{
    if (!o) return;
    gmb_matrix_t* m = (gmb_matrix_t*)o->data;
    if (m) { gmb_matrix_value_free(m); free(m); }
    free(o);
}

gmb_val_t gmb_make_val_matrix(gmb_matrix_t m)
{
    if (!m.st) return gmb_val_nil();
    gmb_matrix_t* p = (gmb_matrix_t*)calloc(1, sizeof(gmb_matrix_t));
    if (!p) return gmb_val_nil();
    *p = gmb_matrix_empty();
    gmb_matrix_value_assign(p, m);            /* retain dello storage */
    gmb_obj_t* o = (gmb_obj_t*)calloc(1, sizeof(gmb_obj_t));
    if (!o) { gmb_matrix_value_free(p); free(p); return gmb_val_nil(); }
    o->refcount = 1;
    o->kind = GMB_OBJ_MATRIX;
    o->release_fn = matrix_box_release;
    o->data = p;
    o->type_name = "gmb_matrix";
    return gmb_make_val_obj(o);
}

int gmb_val_as_matrix(gmb_val_t v, gmb_matrix_t* out)
{
    if (!out) return 0;
    *out = gmb_matrix_empty();
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return 0;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (o->kind != GMB_OBJ_MATRIX || !o->data) return 0;
    *out = *(gmb_matrix_t*)o->data;           /* borrow: nessun retain */
    return 1;
}

static const char* const* decomp_keys(const char* kind, int* n)
{
    static const char* k_lu[]    = { "L", "U" };
    static const char* k_qr[]    = { "Q", "R" };
    static const char* k_svd[]   = { "U", "S", "VT" };
    static const char* k_evd[]   = { "D", "V" };
    static const char* k_chol[]  = { "L" };
    static const char* k_polar[] = { "U", "P" };
    static const char* k_eig[]   = { "DREAL", "VRREAL", "DIMAG", "VRIMAG" };
    static const char* k_schur[] = { "T", "Z" };
    *n = 0;
    if (!kind) return NULL;
    if (!strcasecmp(kind, "LU"))       { *n = 2; return k_lu; }
    if (!strcasecmp(kind, "QR"))       { *n = 2; return k_qr; }
    if (!strcasecmp(kind, "SVD"))      { *n = 3; return k_svd; }
    if (!strcasecmp(kind, "EVD"))      { *n = 2; return k_evd; }
    if (!strcasecmp(kind, "CHOLESKY")) { *n = 1; return k_chol; }
    if (!strcasecmp(kind, "POLAR"))    { *n = 2; return k_polar; }
    if (!strcasecmp(kind, "EIG"))      { *n = 4; return k_eig; }
    if (!strcasecmp(kind, "SCHUR"))    { *n = 2; return k_schur; }
    return NULL;
}

/* Costruisce il DIZIONARIO dei fattori a partire dall'handle di DECOMPOSIZIONE. */
static gmb_val_t decomp_dict_from_decomp(int64_t dh, const char* kind)
{
    if (dh <= 0) return gmb_val_nil();
    int nk = 0;
    const char* const* keys = decomp_keys(kind, &nk);
    gmb_dict_t d = gmb_dict_new();
    for (int i = 0; i < nk; i++)
    {
        int owned = 0;
        int64_t fh = gmb_matrix_decomp_get_factor_named_ex(dh, keys[i], &owned);
        if (fh <= 0) continue;
        gmb_matrix_t m = gmb_matrix_value_of(fh);
        if (owned) gmb_matrix_free(fh);
        if (!m.st) continue;
        gmb_val_t mv = gmb_make_val_matrix(m);
        gmb_matrix_value_free(&m);
        if (mv.kind == GMB_TYPE_OBJECT)
        {
            gmb_dict_set(&d, keys[i], mv);
            gmb_val_discard(mv);              /* il dizionario ha il suo riferimento */
        }
    }
    gmb_matrix_decomp_free(dh);               /* i fattori sono gia' copiati nei valori */
    return gmb_make_val_dict_take(&d);
}

gmb_val_t gmb_matrix_decomp_dict(int64_t mh, const char* kind)
{
    if (mh <= 0 || !kind) return gmb_val_nil();
    /* `mh` è l'handle della matrice di INGRESSO: esegue la decomposizione richiesta e
     * restituisce i fattori come valori (stessa mappa chiavi di VM/C). */
    int64_t dh = 0;
    if (!strcasecmp(kind, "LU"))            dh = gmb_matrix_lu(mh);
    else if (!strcasecmp(kind, "QR"))       dh = gmb_matrix_qr(mh);
    else if (!strcasecmp(kind, "SVD"))      dh = gmb_matrix_svd(mh);
    else if (!strcasecmp(kind, "EVD"))      dh = gmb_matrix_evd(mh);
    else if (!strcasecmp(kind, "CHOLESKY")) dh = gmb_matrix_cholesky(mh);
    else if (!strcasecmp(kind, "SCHUR"))    dh = gmb_matrix_schur(mh);
    else if (!strcasecmp(kind, "POLAR"))    dh = gmb_matrix_polar(mh);
    else if (!strcasecmp(kind, "EIG"))      dh = gmb_matrix_eig(mh);
    else return gmb_val_nil();
    return decomp_dict_from_decomp(dh, kind);
}

static int decomp_factor_matrix(gmb_val_t dv, const char* name, gmb_matrix_t* out)
{
    *out = gmb_matrix_empty();
    if (dv.kind != GMB_TYPE_OBJECT || !dv.v_ptr) return 0;
    gmb_dict_t d = gmb_val_as_dict(dv);
    if (!d.items) return 0;
    gmb_val_t fv = gmb_dict_get(d, name);
    return gmb_val_as_matrix(fv, out);
}

gmb_matrix_t gmb_val_decomp_factor_matrix(gmb_val_t dv, const char* name)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return gmb_matrix_empty();
    gmb_matrix_t out = gmb_matrix_empty();
    gmb_matrix_value_assign(&out, m);         /* retain: il chiamante libera */
    return out;
}

int gmb_val_decomp_factor_into(gmb_val_t dv, const char* name, gmb_matrix_t* out)
{
    if (!out) return 0;
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) { *out = gmb_matrix_empty(); return 0; }
    gmb_matrix_value_assign(out, m);
    return 1;
}

double gmb_val_decomp_factor_get(gmb_val_t dv, const char* name, gmb_int_t r, gmb_int_t c)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return 0.0;
    return gmb_matrix_value_get(m, r, c);
}

int64_t gmb_val_decomp_factor_scratch(gmb_val_t dv, const char* name)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return -1;
    return gmb_matrix_scratch_handle(m);
}

double* gmb_val_decomp_factor_flat(gmb_val_t dv, const char* name)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return NULL;
    return gmb_matrix_flat(m);
}

gmb_int_t gmb_val_decomp_factor_rows(gmb_val_t dv, const char* name)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return 0;
    return (gmb_int_t)m.rows;
}

gmb_int_t gmb_val_decomp_factor_cols(gmb_val_t dv, const char* name)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return 0;
    return (gmb_int_t)m.cols;
}

gmb_string_t gmb_val_decomp_factor_display(gmb_val_t dv, const char* name)
{
    gmb_matrix_t m;
    if (!decomp_factor_matrix(dv, name, &m)) return gmb_string_alloc("");
    double* flat = gmb_matrix_flat(m);
    if (!flat) return gmb_string_alloc("");
    /* Display UMANO del fattore: separatore ", " (parità IL/PCode, che rende "[4, 2, 0, 2]").
     * Il JSON compatto (`gmb_json_double_array`, senza spazi) resta per JSON.STRINGIFY: qui si
     * stampa con PRINT, e la virgola senza spazio faceva divergere `matrix/mat_decomp_fields`. */
    return gmb_display_double_array(flat, (gmb_int_t)(m.rows * m.cols));
}
