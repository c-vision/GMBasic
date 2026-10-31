/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_dataread.c — speculare a
 * Parser.Stmt.DataRead.cs: DATA <values...>, READ <vars...>, RESTORE. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

/* DATA <val> [, <val> ...] fino a fine riga */
gmb_stmt_t* gmb_p_parse_data(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DATA, line);
    int cap = 0;
    if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) return s;
    for (;;) {
        if (s->n_data >= cap) { cap = cap ? cap * 2 : 4; s->data_values = (gmb_expr_t**)realloc(s->data_values, (size_t)cap * sizeof(gmb_expr_t*)); }
        s->data_values[s->n_data++] = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) break;
    }
    return s;
}

/* READ <var> [, <var> ...] fino a fine riga */
gmb_stmt_t* gmb_p_parse_read(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_READ, line);
    int cap = 0;
    for (;;) {
        const gmb_token_t* v = gmb_p_cur(p);
        if (v && v->kind == GMB_TOK_Identifier) {
            if (s->n_read >= cap) { cap = cap ? cap * 2 : 4; s->read_vars = (char**)realloc(s->read_vars, (size_t)cap * sizeof(char*)); }
            s->read_vars[s->n_read] = (char*)malloc((size_t)v->length + 1);
            memcpy(s->read_vars[s->n_read], v->lexeme, (size_t)v->length);
            s->read_vars[s->n_read][v->length] = 0;
            s->n_read++;
            gmb_p_adv(p);
        }
        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) break;
    }
    return s;
}

/* RESTORE [<target>] */
gmb_stmt_t* gmb_p_parse_restore(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_RESTORE, line);
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && (t->kind == GMB_TOK_Identifier || t->kind == GMB_TOK_Number)) {
        s->target_label = (char*)malloc((size_t)t->length + 1);
        memcpy(s->target_label, t->lexeme, (size_t)t->length); s->target_label[t->length] = 0;
        gmb_p_adv(p);
    }
    return s;
}
