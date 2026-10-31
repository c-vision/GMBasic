/* test_semgraph.c — self-test del GMB Semantic Graph (C): lex -> parse -> extract -> query. */
#include "gmb_lexer.h"
#include "gmb_parser.h"
#include "gmb_semgraph.h"
#include "gmb_symtab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    const char* src =
        "FUNCTION Sq(x) AS DOUBLE\n"
        "  Sq = x\n"
        "END FUNCTION\n"
        "SUB Bar(x)\n"
        "  y = Sq(x)\n"
        "END SUB\n"
        "SUB Foo()\n"
        "  Bar(1)\n"
        "END SUB\n"
        "SUB Main()\n"
        "  Foo()\n"
        "  DIM y\n"
        "  y = 1\n"
        "  DIM z AS DOUBLE\n"
        "  z = 2\n"
        "END SUB\n";
    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    if (lx.error) { printf("LEX ERR: %s\n", lx.error); return 1; }
    char* err = NULL;
    gmb_program_t p = gmb_parse(&lx, &err);
    if (err) { printf("PARSE ERR: %s\n", err); return 1; }
    gmb_semgraph_t g;
    gmb_semgraph_extract(&g, p.stmts, p.n, "test.bas");
    printf("sym=%zu ref=%zu edge=%zu\n", g.nsyms, g.nrefs, g.nedges);
    const gmb_symnode_t* d = gmb_semgraph_definition(&g, "Bar");
    printf("def(Bar)=%s kind=%d\n", d ? d->name : "(none)", d ? (int)d->kind : -1);
    gmb_edge_t* ce = NULL; size_t n = gmb_semgraph_callers(&g, "Bar", &ce);
    printf("callers(Bar)=%zu:", n);
    for (size_t i = 0; i < n; i++) printf(" %s->%s", ce[i].from, ce[i].to);
    printf("\n");
    gmb_semgraph_free_query(ce);
    char** imp = NULL; size_t ni = gmb_semgraph_impact(&g, "Bar", 8, &imp);
    printf("impact(Bar)=%zu:", ni);
    for (size_t i = 0; i < ni; i++) printf(" %s", imp[i]);
    printf("\n");
    /* attribuzione TIPI sui ref (InferTypes) */
    const char* zty = "(none)"; const char* zsym = "";
    for (size_t i = 0; i < g.nrefs; i++)
        if (strcmp(g.refs[i].text, "Sq") == 0) { zty = g.refs[i].type_name; zsym = g.refs[i].sym_id; }
    printf("ref(Sq): type=%s sym_id=%s\n", zty, *zsym ? "SET" : "(none)");
    /* verifica la SYMBOL TABLE RICCA: kind dei simboli + scope */
    gmb_symtab_t st; gmb_symtab_init(&st); gmb_symtab_build(&st, p.stmts, p.n);
    int k_sub=0, k_param=0;
    for (int i = 0; i < st.n_syms; i++) {
        if (st.syms[i].kind == GMB_SYM_PROCEDURE) k_sub++;
        if (st.syms[i].kind == GMB_SYM_PARAMETER) k_param++;
    }
    printf("symtab: syms=%d scope=%d procedure=%d param=%d\n", st.n_syms, st.n_scopes, k_sub, k_param);
    /* diff: grafo "to" = copia di g + un simbolo in piu (senza ri-parse) */
    gmb_semgraph_t g2; memset(&g2, 0, sizeof g2);
    g2.syms = malloc((g.nsyms + 1) * sizeof(gmb_symnode_t));
    memcpy(g2.syms, g.syms, g.nsyms * sizeof(gmb_symnode_t)); g2.nsyms = g.nsyms;
    memset(&g2.syms[g2.nsyms], 0, sizeof(gmb_symnode_t));
    snprintf(g2.syms[g2.nsyms].id, sizeof g2.syms[g2.nsyms].id, "|sub|NewOne|");
    snprintf(g2.syms[g2.nsyms].name, sizeof g2.syms[g2.nsyms].name, "NewOne");
    g2.syms[g2.nsyms].kind = GMB_SYM_PROCEDURE; g2.nsyms++;
    gmb_semgraph_diff_t df; gmb_semgraph_diff(&g, &g2, &df);
    printf("diff: added=%d removed=%d changed=%d edges +%zu -%zu impacted=%zu\n",
           df.n_added, df.n_removed, df.n_changed, df.n_added_edges, df.n_removed_edges, df.n_impacted);
    int d_ok = (df.n_added >= 1);
    gmb_semgraph_diff_free(&df); free(g2.syms);   /* g2 e' shallow: NON gmb_semgraph_free */
    int ok = (g.nsyms >= 5 && n >= 1 && ni >= 1 && k_sub >= 3 && k_param >= 1 && st.n_scopes >= 4 && strcmp(zty, "DOUBLE") == 0 && *zsym && d_ok);
    gmb_symtab_free(&st);
    gmb_semgraph_free(&g); gmb_program_free(&p); gmb_lex_result_free(&lx);
    printf("%s\n", ok ? "SEMGRAPH OK" : "SEMGRAPH FAIL");
    return ok ? 0 : 1;
}
