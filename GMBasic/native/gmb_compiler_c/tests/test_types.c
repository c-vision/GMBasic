/* test_types.c — self-test del Type Registry (C): CLASS + campi + risoluzione membro/base. */
#include "gmb_lexer.h"
#include "gmb_parser.h"
#include "gmb_types.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* src =
        "CLASS Point\n"
        "  DIM x AS DOUBLE\n"
        "  DIM y AS DOUBLE\n"
        "END CLASS\n"
        "SUB Main()\n"
        "END SUB\n";
    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    if (lx.error) { printf("LEX ERR %s\n", lx.error); return 1; }
    char* err = NULL; gmb_program_t p = gmb_parse(&lx, &err);
    if (err) { printf("PARSE ERR %s\n", err); return 1; }
    gmb_typereg_t r; gmb_typereg_init(&r); int nt = gmb_typereg_build(&r, p.stmts, p.n);
    printf("types=%d\n", nt);
    const gmb_type_t* pt = gmb_typereg_find(&r, "Point");
    printf("Point kind=%s fields=%d\n", pt ? gmb_type_kind_name(pt->kind) : "(none)", pt ? pt->n_fields : -1);
    const gmb_field_t* fx = gmb_typereg_field(&r, "Point", "x");
    printf("Point.x type=%s\n", fx ? fx->type : "(none)");
    int ok = (nt >= 1 && pt && pt->kind == GMB_TY_CLASS && fx && strcmp(fx->type, "DOUBLE") == 0);
    gmb_typereg_free(&r); gmb_program_free(&p); gmb_lex_result_free(&lx);
    printf("%s\n", ok ? "TYPES OK" : "TYPES FAIL");
    return ok ? 0 : 1;
}
