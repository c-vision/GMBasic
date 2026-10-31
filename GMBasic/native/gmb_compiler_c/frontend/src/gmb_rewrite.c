/* gmb_rewrite.c — Rewrite del linker C: risolve SymbolId + ResolvedType su ogni espressione. */
#include "gmb_rewrite.h"
#include "gmb_resolver.h"
#include <string.h>
#include <stdlib.h>

static int g_n;

/* Arricchisce una singola expr-nome (VAR/CALL): simbolo risolto + tipo risolto. */
static void rewrite_name(gmb_expr_t* e, const gmb_symtab_t* st, const gmb_typereg_t* tr, int is_call) {
    if (!e || !e->var || !*e->var) return;
    e->resolved_sym = -1;   /* default: non risolto (calloc parte da 0) */
    int id = gmb_symtab_find_any((gmb_symtab_t*)st, e->var);
    if (id >= 0) e->resolved_sym = id;
    char ty[64] = "";
    if (gmb_resolve_name_type(st, tr, e->var, ty, sizeof ty, NULL, 0) == 0) {
        free(e->resolved_type);
        e->resolved_type = strdup(ty);
    }
    (void)is_call;
    g_n++;
}

static void rw_expr(gmb_expr_t* e, const gmb_symtab_t* st, const gmb_typereg_t* tr);
static void rw_body(gmb_stmt_t** body, int n, const gmb_symtab_t* st, const gmb_typereg_t* tr);
static void rw_stmt(gmb_stmt_t* s, const gmb_symtab_t* st, const gmb_typereg_t* tr);

static void rw_expr(gmb_expr_t* e, const gmb_symtab_t* st, const gmb_typereg_t* tr) {
    if (!e) return;
    switch (e->kind) {
        case GMB_EXPR_VAR:  rewrite_name(e, st, tr, 0); break;
        case GMB_EXPR_CALL:
            rewrite_name(e, st, tr, 1);
            for (int i = 0; i < e->nargs; i++) rw_expr(e->args[i], st, tr);
            break;
        case GMB_EXPR_BINARY: rw_expr(e->left, st, tr); rw_expr(e->right, st, tr); break;
        case GMB_EXPR_UNARY:  rw_expr(e->left, st, tr); break;
        case GMB_EXPR_COMPREHENSION:
            rw_expr(e->comp_src, st, tr); rw_expr(e->comp_where, st, tr);
            rw_expr(e->comp_order, st, tr); rw_expr(e->comp_sel, st, tr); rw_expr(e->comp_key, st, tr);
            for (int i = 0; i < e->n_comp_more; i++) rw_expr(e->comp_more_src[i], st, tr);
            break;
        case GMB_EXPR_LAMBDA:
            rw_expr(e->lambda_expr, st, tr);
            rw_body(e->lambda_body, e->n_lambda_body, st, tr);
            break;
        default: break;
    }
}

static void rw_body(gmb_stmt_t** body, int n, const gmb_symtab_t* st, const gmb_typereg_t* tr) {
    for (int i = 0; i < n; i++) rw_stmt(body[i], st, tr);
}

static void rw_stmt(gmb_stmt_t* s, const gmb_symtab_t* st, const gmb_typereg_t* tr) {
    if (!s) return;
    switch (s->kind) {
        case GMB_STMT_FUNCTION: rw_body(s->func_body, s->n_funcbody, st, tr); break;
        case GMB_STMT_LET: case GMB_STMT_ASSIGN:
            rw_expr(s->value, st, tr); rw_expr(s->array_index, st, tr); break;
        case GMB_STMT_CALL: rw_expr(s->expr, st, tr); break;
        case GMB_STMT_IF:
            rw_expr(s->cond, st, tr);
            rw_body(s->then_body, s->n_then, st, tr); rw_body(s->else_body, s->n_else, st, tr); break;
        case GMB_STMT_FOR:
            rw_expr(s->for_from, st, tr); rw_expr(s->for_to, st, tr); rw_expr(s->for_step, st, tr);
            rw_body(s->for_body, s->n_for, st, tr); break;
        case GMB_STMT_WHILE: rw_expr(s->cond, st, tr); rw_body(s->while_body, s->n_while, st, tr); break;
        case GMB_STMT_DO: rw_expr(s->do_until, st, tr); rw_expr(s->do_while, st, tr); rw_body(s->do_body, s->n_do, st, tr); break;
        case GMB_STMT_PRINT: for (int i = 0; i < s->nprint; i++) rw_expr(s->print_args[i], st, tr); break;
        case GMB_STMT_RETURN: rw_expr(s->value, st, tr); break;
        case GMB_STMT_EXPR: rw_expr(s->expr, st, tr); break;
        case GMB_STMT_DIM: rw_expr(s->array_size, st, tr); break;
        default: break;
    }
}

int gmb_rewrite_program(gmb_stmt_t** stmts, int n, const gmb_symtab_t* st, const gmb_typereg_t* tr) {
    g_n = 0;
    if (!stmts) return 0;
    for (int i = 0; i < n; i++) rw_stmt(stmts[i], st, tr);
    return g_n;
}
