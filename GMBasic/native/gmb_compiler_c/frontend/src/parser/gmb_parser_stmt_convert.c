/* native/gmb_compiler_c/src/parser/gmb_parser_stmt_convert.c — CONVERT/CONVBAT.
 * Speculare a Parser.Stmt.Ai.cs: ParseConvertBatchStatement (comando legacy
 * CONVBAT <dir>[, <fmt>][, <rec>] INTO <target>). */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

/* CONVBAT <dir>[, <fmt>][, <rec>] INTO <target> — forma comando di CONVERT.BATCH.
 * Statement = GMB_STMT_ASSIGN con call GMB_TOK_CvtBatch; emissione cv_batch →
 * gmb_doc_batch nel runtime C. */
gmb_stmt_t* gmb_p_parse_convbat(gmb_parser_t* p, int line)
{
    gmb_p_adv(p);   /* CONVBAT */
    gmb_expr_t* args[3]; int n = 0;
    args[n++] = gmb_p_parse_expr(p);
    while (n < 3 && gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) {
        gmb_p_adv(p);
        args[n++] = gmb_p_parse_expr(p);
    }
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
        gmb_p_adv(p);
        const gmb_token_t* tgt = gmb_p_cur(p);
        if (tgt && tgt->length > 0) {
            char* name = (char*)malloc((size_t)tgt->length + 1);
            memcpy(name, tgt->lexeme, (size_t)tgt->length); name[tgt->length] = 0;
            gmb_p_adv(p);
            gmb_expr_t** a = (gmb_expr_t**)malloc((size_t)n * sizeof(gmb_expr_t*));
            for (int i = 0; i < n; i++) a[i] = args[i];
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ASSIGN, line);
            s->target = name;
            s->value = gmb_expr_call_typed("BATCH", GMB_TOK_CvtBatch, a, n, line);
            return s;
        }
    }
    gmb_expr_t** a2 = (gmb_expr_t**)malloc((size_t)n * sizeof(gmb_expr_t*));
    for (int i = 0; i < n; i++) a2[i] = args[i];
    gmb_stmt_t* se = gmb_stmt_new(GMB_STMT_EXPR, line);
    se->expr = gmb_expr_call_typed("BATCH", GMB_TOK_CvtBatch, a2, n, line);
    return se;
}

/* WEB2MD <url> INTO <target> — forma comando di CONVERT.WEB(url), parità C#. */
gmb_stmt_t* gmb_p_parse_web2md(gmb_parser_t* p, int line)
{
    gmb_p_adv(p);   /* WEB2MD */
    gmb_expr_t* url = gmb_p_parse_expr(p);
    if (!url) return NULL;
    if (!gmb_p_match(p, GMB_TOK_Into)) { gmb_p_set_err(p, "atteso INTO dopo WEB2MD", line); return NULL; }
    const gmb_token_t* tgt = gmb_p_cur(p);
    if (!tgt || tgt->length == 0) { gmb_p_set_err(p, "target mancante dopo WEB2MD", line); return NULL; }
    char* name = (char*)malloc((size_t)tgt->length + 1);
    memcpy(name, tgt->lexeme, (size_t)tgt->length); name[tgt->length] = 0;
    gmb_p_adv(p);
    gmb_expr_t** a = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*));
    a[0] = url;
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ASSIGN, line);
    s->target = name;
    s->value = gmb_expr_call_typed("WEB", GMB_TOK_WebToMd, a, 1, line);
    return s;
}
