/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_goto.c — speculare a
 * Parser.Stmt.Goto.cs: GOTO <label> e GOSUB <label>. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

static char* read_label(gmb_parser_t* p)
{
    const gmb_token_t* lbl = gmb_p_cur(p);
    if (!lbl) return NULL;
    char* s = (char*)malloc((size_t)lbl->length + 1);
    memcpy(s, lbl->lexeme, (size_t)lbl->length);
    s[lbl->length] = 0;
    gmb_p_adv(p);
    return s;
}

/* GOTO <label> */
gmb_stmt_t* gmb_p_parse_goto(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GOTO, line);
    s->target_label = read_label(p);
    return s;
}

/* GOSUB <label> */
gmb_stmt_t* gmb_p_parse_gosub(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GOSUB, line);
    s->target_label = read_label(p);
    return s;
}
