/* test_resolver.c — self-test del Resolver C: tipo di nome puntato + arity. */
#include "gmb_lexer.h"
#include "gmb_parser.h"
#include "gmb_symtab.h"
#include "gmb_types.h"
#include "gmb_resolver.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* src =
        "CLASS Point\n"
        "  DIM x AS DOUBLE\n"
        "END CLASS\n"
        "SUB Take(a, b)\n"
        "END SUB\n"
        "SUB Main()\n"
        "  DIM p AS Point\n"
        "  DIM v AS DOUBLE\n"
        "  p.x = 3\n"
        "  v = p.x\n"
        "END SUB\n"
        "IMPORT TRADING.PORTFOLIO AS TP\n"
        "IMPORT MATH (Sqrt, Abs) AS M\n";
    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    if (lx.error) { printf("LEX ERR %s\n", lx.error); return 1; }
    char* err = NULL; gmb_program_t p = gmb_parse(&lx, &err);
    if (err) { printf("PARSE ERR %s\n", err); return 1; }
    gmb_symtab_t st; gmb_symtab_init(&st); gmb_symtab_build(&st, p.stmts, p.n);
    gmb_typereg_t tr; gmb_typereg_init(&tr); gmb_typereg_build(&tr, p.stmts, p.n);
    char ty[128]; int r1 = gmb_resolve_name_type(&st, &tr, "p.x", ty, sizeof ty, NULL, 0);
    char e[128] = ""; int r2 = gmb_resolve_member(&st, &tr, "p", "y", e, sizeof e);
    char e3[128] = ""; int r3 = gmb_resolve_call_arity(&st, "Take", 1, e3, sizeof e3);
    char ns[128] = ""; int r4 = gmb_resolve_import((const gmb_stmt_t**)p.stmts, p.n, "TP", ns, sizeof ns);
    int oid = -1; char eo[128] = "";
    int r7 = gmb_resolve_overload(&st, "Take", 2, &oid, eo, sizeof eo);
    int oid2 = -1; char eo2[128] = "";
    int r8 = gmb_resolve_overload(&st, "Take", 1, &oid2, eo2, sizeof eo2);
    char ns2[128] = ""; int r5 = gmb_resolve_import_member((const gmb_stmt_t**)p.stmts, p.n, "M", "Sqrt", ns2, sizeof ns2);
    char e5[128] = ""; int r6 = gmb_resolve_import_member((const gmb_stmt_t**)p.stmts, p.n, "M", "Cos", e5, sizeof e5);
    printf("p.x -> %d (%s)\n", r1, r1 == 0 ? ty : "-");
    printf("p.y -> %d [%s]\n", r2, e);
    printf("Take(1) -> %d [%s]\n", r3, e3);
    printf("IMPORT TP -> %d (%s)\n", r4, r4 == 0 ? ns : "-");
    printf("overload Take(2) -> %d id=%d slot=%d\n", r7, oid, r7 == 0 ? oid : -1);
    printf("overload Take(1) -> %d [%s]\n", r8, eo2);
    printf("IMPORT M membro Sqrt -> %d (%s)\n", r5, r5 == 0 ? ns2 : "NON oltre");
    printf("IMPORT M membro Cos -> %d (atteso -1: non selezionato)\n", r6);
    int sc_ok = 0;
    for (int i = 0; i < p.n; i++)
        if (p.stmts[i] && p.stmts[i]->kind == GMB_STMT_FUNCTION && p.stmts[i]->scope_id >= 0)
            sc_ok = 1;
    printf("decl scope_id (FUNCTION) -> %s\n", sc_ok ? "SET" : "(none)");
    int ok = (r1 == 0 && strcmp(ty, "DOUBLE") == 0 && r2 == -1 && r3 == -1 && r4 == 0 && strcmp(ns, "TRADING.PORTFOLIO") == 0 && r5 == 0 && r6 == -1 && r7 == 0 && oid >= 0 && r8 == -1 && sc_ok);
    gmb_typereg_free(&tr); gmb_symtab_free(&st); gmb_program_free(&p); gmb_lex_result_free(&lx);
    printf("%s\n", ok ? "RESOLVER OK" : "RESOLVER FAIL");
    return ok ? 0 : 1;
}
