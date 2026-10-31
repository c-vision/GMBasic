/* test_rewrite.c — self-test del REWRITE C: lex -> parse -> symtab/typereg -> rewrite -> verifica. */
#include "gmb_lexer.h"
#include "gmb_parser.h"
#include "gmb_symtab.h"
#include "gmb_types.h"
#include "gmb_rewrite.h"
#include <stdio.h>
#include <string.h>

static gmb_expr_t* find_call(gmb_stmt_t** body, int n, const char* name) {
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = body[i];
        if (!s) continue;
        if (s->expr && s->expr->kind == GMB_EXPR_CALL && s->expr->var && strcmp(s->expr->var, name) == 0) return s->expr;
        if (s->expr && s->expr->kind == GMB_EXPR_BINARY) {
            if (s->expr->right && s->expr->right->kind == GMB_EXPR_CALL && s->expr->right->var
                && strcmp(s->expr->right->var, name) == 0) return s->expr->right;
        }
        if (s->value && s->value->kind == GMB_EXPR_CALL && s->value->var && strcmp(s->value->var, name) == 0) return s->value;
        gmb_expr_t* c = find_call(s->func_body, s->n_funcbody, name);
        if (c) return c;
    }
    return NULL;
}

int main(void) {
    const char* src =
        "FUNCTION Sq(x) AS DOUBLE\n"
        "  Sq = x\n"
        "END FUNCTION\n"
        "SUB Main()\n"
        "  a = Sq(2)\n"
        "END SUB\n";
    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    if (lx.error) { printf("LEX ERR: %s\n", lx.error); return 1; }
    char* err = NULL;
    gmb_program_t p = gmb_parse(&lx, &err);
    if (err) { printf("PARSE ERR: %s\n", err); return 1; }
    gmb_symtab_t st; gmb_symtab_init(&st); gmb_symtab_build(&st, p.stmts, p.n);
    gmb_typereg_t tr; gmb_typereg_init(&tr); gmb_typereg_build(&tr, p.stmts, p.n);
    int n = gmb_rewrite_program(p.stmts, p.n, &st, &tr);
    gmb_expr_t* call = find_call(p.stmts, p.n, "Sq");
    printf("rewrite: enriched=%d\n", n);
    printf("call Sq: sym=%d type=%s\n", call ? call->resolved_sym : -99,
           call && call->resolved_type ? call->resolved_type : "(none)");
    int ok = (n >= 1 && call && call->resolved_sym >= 0 && call->resolved_type
              && strcmp(call->resolved_type, "DOUBLE") == 0);
    gmb_typereg_free(&tr); gmb_symtab_free(&st);
    gmb_program_free(&p); gmb_lex_result_free(&lx);
    printf("%s\n", ok ? "REWRITE OK" : "REWRITE FAIL");
    return ok ? 0 : 1;
}
