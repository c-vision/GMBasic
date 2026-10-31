/* gmb_semquery.c — §4.10 esposizione agenti (lato C): query sul grafo semantico di un file .bas.
 * Indipendente dal C# (§4.10 già duale: C# `SemanticQueryHook` / C `gmb_semfold`). Riusa la
 * STESSA `query()` di `gmb_semfold` (costruendo un expr sintetico) → un solo formato canonico.
 * Usata dal tool-loop agentico `code.*` (VM/C, C99Native). Nessun P/Invoke. */
#include "gmb_semquery.h"
#include "gmb_lexer.h"
#include "gmb_macro_expand.h"
#include "gmb_parser.h"
#include "gmb_ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Definiti in gmb_semfold.c (stessa TU-link del compilatore C). */
void gmb_semfold_program(gmb_stmt_t** stmts, int n, const char* file);
int  gmb_semfold_query_str(const char* verb, const char* arg, int depth, char* out, size_t cap);

int gmb_semquery_file(const char* path, const char* verb, const char* arg, int depth, char* out, size_t cap) {
    if (path == NULL || verb == NULL || out == NULL || cap == 0) return -1;
    out[0] = 0;
    FILE* f = fopen(path, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return -1; }
    char* src = (char*)malloc((size_t)n + 1);
    if (src == NULL) { fclose(f); return -1; }
    if (n > 0 && fread(src, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(src); return -1; }
    src[n] = 0; fclose(f);

    int rc = -1;
    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    if (perr == NULL) {
        gmb_semfold_program(prog.stmts, prog.n, path);
        rc = gmb_semfold_query_str(verb, arg, depth, out, cap);
    } else {
        free(perr);
    }
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2);
    gmb_lex_result_free(&lx);
    free(src);
    return rc;
}
