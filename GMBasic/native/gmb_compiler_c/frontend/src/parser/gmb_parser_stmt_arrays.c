/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_arrays.c — speculare a
 * Parser.Stmt.Arrays.cs: AADD, AINS, ADEL, ASHUFFLE, ASORT. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

static gmb_stmt_t* array_stmt_base(gmb_stmt_kind_t kind, gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    const gmb_token_t* a = gmb_p_cur(p);
    if (a) {
        s->array_var = (char*)malloc((size_t)a->length + 1);
        memcpy(s->array_var, a->lexeme, (size_t)a->length); s->array_var[a->length] = 0;
        gmb_p_adv(p);
    }
    return s;
}

/* AADD <array>, <value> */
gmb_stmt_t* gmb_p_parse_aadd(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = array_stmt_base(GMB_STMT_AADD, p, line);
    if (gmb_p_match(p, GMB_TOK_Comma)) s->array_value = gmb_p_parse_expr(p);
    return s;
}

/* AINS <array>, <position>, <value> */
gmb_stmt_t* gmb_p_parse_ains(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = array_stmt_base(GMB_STMT_AINS, p, line);
    if (gmb_p_match(p, GMB_TOK_Comma)) { s->array_position = gmb_p_parse_expr(p); if (gmb_p_match(p, GMB_TOK_Comma)) s->array_value = gmb_p_parse_expr(p); }
    return s;
}

/* ADEL <array>, <position> */
gmb_stmt_t* gmb_p_parse_adel(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = array_stmt_base(GMB_STMT_ADEL, p, line);
    if (gmb_p_match(p, GMB_TOK_Comma)) s->array_position = gmb_p_parse_expr(p);
    return s;
}

/* ASHUFFLE <array> */
gmb_stmt_t* gmb_p_parse_ashuffle(gmb_parser_t* p, int line)
{
    return array_stmt_base(GMB_STMT_ASHUFFLE, p, line);
}

/* ASORT <array> [, <compareFunc> [, <order>]] */
gmb_stmt_t* gmb_p_parse_asort(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = array_stmt_base(GMB_STMT_ASORT, p, line);
    if (gmb_p_match(p, GMB_TOK_Comma)) {
        if (gmb_p_check(p, GMB_TOK_Comma)) {
            gmb_p_adv(p);   /* compareFunc vuota (, ,) — l'ordine segue direttamente */
            s->array_position = gmb_p_parse_expr(p);
        } else {
            s->array_value = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) {
                if (gmb_p_check(p, GMB_TOK_Comma)) { gmb_p_adv(p); }
                else s->array_position = gmb_p_parse_expr(p);
            }
        }
    }
    return s;
}

/* AFILL <array>, <value> [, <start> [, <count>]] */
gmb_stmt_t* gmb_p_parse_afill(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = array_stmt_base(GMB_STMT_AFILL, p, line);
    if (gmb_p_match(p, GMB_TOK_Comma)) {
        s->array_value = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) {
            s->array_start = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) s->array_count = gmb_p_parse_expr(p);
        }
    }
    return s;
}

/* ACOPY <source>, <dest> [, <srcStart> [, <count> [, <dstStart>]]] */
gmb_stmt_t* gmb_p_parse_acopy(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = array_stmt_base(GMB_STMT_ACOPY, p, line);
    if (gmb_p_match(p, GMB_TOK_Comma)) {
        const gmb_token_t* d = gmb_p_cur(p);
        if (d && d->kind != GMB_TOK_Comma && d->kind != GMB_TOK_Newline && d->lexeme) {
            s->array_var2 = (char*)malloc((size_t)d->length + 1);
            memcpy(s->array_var2, d->lexeme, (size_t)d->length); s->array_var2[d->length] = 0;
            gmb_p_adv(p);
        }
        if (gmb_p_match(p, GMB_TOK_Comma)) {
            if (!gmb_p_check(p, GMB_TOK_Comma)) s->array_start = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) {
                if (!gmb_p_check(p, GMB_TOK_Comma)) s->array_count = gmb_p_parse_expr(p);
                if (gmb_p_match(p, GMB_TOK_Comma)) {
                    if (!gmb_p_check(p, GMB_TOK_Comma)) s->array_extra = gmb_p_parse_expr(p);
                }
            }
        }
    }
    return s;
}
