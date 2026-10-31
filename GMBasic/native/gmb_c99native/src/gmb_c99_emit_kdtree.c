/* gmb_c99_emit_kdtree.c — KD-Tree builtin emission (C99Native, §0quater).
 * Dominio KDTREE: 4 funzioni. CREATE (scalar) va nel dispatch builtin;
 * QUERY/QUERYDIST/QUERYBALL (array-returning) vanno in dim_arr_kdtree.
 * Implementazione nel runtime condiviso gmb_kdtree.c (mai inline, §0bis).
 */

#include "gmb_c99_emit.h"
#include "gmb_c99_emit_internal.h"

/* ── helper: verifica che un argomento sia array e ne estrae var_t* ── */
static int kd_arr(ctx_t* c, gmb_expr_t* a, var_t** out) {
    (void)c;
    if (!a || a->kind != GMB_EXPR_VAR || !a->var) return 0;
    var_t* av = lookup_var(c, a->var);
    if (!av || !av->is_array) return 0;
    *out = av; return 1;
}

/* ── KDTREE.CREATE(flatArray, dimensions) → handle (scalar) ── */
int emit_bf_kdtree(ctx_t* c, const char* name, const char* ns,
                   gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns;
    if (tk != GMB_TOK_KdTreeCreate) return 0;
    if (nargs < 2) return 0;
    var_t* av = NULL;
    if (!kd_arr(c, args[0], &av)) {
        c99_fail(c, line, "KDTREE.CREATE: il primo argomento deve essere un array");
        return 0;
    }
    sb_put(&c->out, "(gmb_int_t)gmb_kdtree_create((gmb_double_t*)");
    emit_var_ref(c, av);
    sb_put(&c->out, ", (int)((");
    emit_arr_len_of(c, av);
    sb_put(&c->out, ") / (int)");
    emit_expr(c, args[1]);
    sb_put(&c->out, "), (int)");
    emit_expr(c, args[1]);
    sb_put(&c->out, ")");
    return 1;
}