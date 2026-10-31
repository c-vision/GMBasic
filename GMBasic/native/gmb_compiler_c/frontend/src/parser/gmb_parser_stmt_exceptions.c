/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_exceptions.c — speculare a
 * Parser.Stmt.Exceptions.cs: TRY/CATCH/FINALLY/END TRY, ERROR <code>, THROW <expr>. */
#include "gmb_parser_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TRY <body> CATCH [<var>] <body> [CATCH ...] [FINALLY <body>] END TRY */
gmb_stmt_t* gmb_p_parse_try_catch(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TRY, line);
    *multiline = true;
    gmb_stmt_list_t body = gmb_p_stmt_list_new();
    gmb_p_parse_block(p, &body, GMB_TOK_Catch, "Catch", line);
    s->try_body = body.items; s->n_try = body.n;
    int cap_catches = 0;
    for (;;) {
        gmb_p_skip_eol(p);
        if (gmb_p_at_end(p)) break;
        const gmb_token_t* t = gmb_p_cur(p);
        if (t->kind == GMB_TOK_Catch || gmb_p_tok_is_keyword(t, "Catch")) {
            gmb_p_adv(p);
            gmb_stmt_t* c = gmb_stmt_new(GMB_STMT_CATCH, line);
            /* variabile errore opzionale (Identifier o tipo) */
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->kind == GMB_TOK_Identifier) {
                c->catch_var = (char*)malloc((size_t)v->length + 1);
                memcpy(c->catch_var, v->lexeme, (size_t)v->length); c->catch_var[v->length] = 0;
                gmb_p_adv(p);   /* pasto la variabile */
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) { gmb_p_adv(p); while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p); }
            }
            gmb_stmt_list_t cb = gmb_p_stmt_list_new();
            gmb_p_parse_block(p, &cb, GMB_TOK_Catch, "Catch", line);
            c->catch_body = cb.items; c->n_catch = cb.n;
            if (s->n_catch >= cap_catches) { cap_catches = cap_catches ? cap_catches * 2 : 4; s->catch_body = (gmb_stmt_t**)realloc(s->catch_body, (size_t)cap_catches * sizeof(gmb_stmt_t*)); }
            /* usiamo catch_body come lista di clause CATCH */
            s->catch_body[s->n_catch++] = c;
            continue;
        }
        break;
    }
    /* FINALLY */
    gmb_p_skip_eol(p);
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Finally || gmb_p_tok_is_keyword(gmb_p_cur(p), "Finally"))) {
        gmb_p_adv(p);
        gmb_stmt_list_t fb = gmb_p_stmt_list_new();
        gmb_p_parse_block(p, &fb, GMB_TOK_EndTry, "EndTry", line);
        s->finally_body = fb.items; s->n_finally = fb.n;
    }
    /* END TRY */
    gmb_p_skip_eol(p);
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End) {
        gmb_p_adv(p);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Try || gmb_p_tok_is_keyword(gmb_p_cur(p), "Try"))) gmb_p_adv(p);
    }
    return s;
}

/* ERROR <code> [, <descrizione>] (parità C# ErrorStmt: ErrorCode + Description) */
gmb_stmt_t* gmb_p_parse_error(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ERROR, line);
    s->error_code = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Comma)) s->throw_expr = gmb_p_parse_expr(p);   /* descrizione */
    return s;
}

/* THROW <expr> */
gmb_stmt_t* gmb_p_parse_throw(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_THROW, line);
    s->throw_expr = gmb_p_parse_expr(p);
    return s;
}
