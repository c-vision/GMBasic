/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_events.c — speculare a
 * Parser.Stmt.Events.cs: RAISE EVENT nome(args), ADD HANDLER e, h, REMOVE HANDLER e, h. */
#include "gmb_parser_internal.h"
#include <stdlib.h>

static gmb_stmt_t* gmb_p_parse_handler(gmb_parser_t* p, int line, int kind)
{
    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    gmb_p_adv(p);   /* ADD o REMOVE */
    if (!gmb_p_match(p, GMB_TOK_Handler)) { gmb_p_set_err(p, "Atteso HANDLER dopo ADD/REMOVE", line); return s; }
    s->channel = gmb_p_parse_expr(p);           /* riferimento evento */
    if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il riferimento evento", line); return s; }
    s->send_value = gmb_p_parse_expr(p);        /* handler */
    return s;
}

gmb_stmt_t* gmb_p_parse_raise_event(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_RAISE_EVENT, line);
    gmb_p_adv(p);   /* RAISE */
    if (!gmb_p_match(p, GMB_TOK_Event)) { gmb_p_set_err(p, "Atteso EVENT dopo RAISE", line); return s; }
    const gmb_token_t* n = gmb_p_cur(p);
    if (!n) { gmb_p_set_err(p, "Atteso nome evento", line); return s; }
    s->proc_name = gmb_expr_var(gmb_p_tok_strdup(n), n->line);   /* ConsumeEventName: id o keyword */
    gmb_p_adv(p);
    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
        int cap = 0;
        while (!gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p)) {
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 4; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = gmb_p_parse_expr(p);
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        }
        gmb_p_match(p, GMB_TOK_RightParen);
    }
    return s;
}

gmb_stmt_t* gmb_p_parse_events(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    if (t->kind == GMB_TOK_Raise) return gmb_p_parse_raise_event(p, line);
    if (t->kind == GMB_TOK_Add) return gmb_p_parse_handler(p, line, GMB_STMT_ADD_HANDLER);
    if (t->kind == GMB_TOK_Remove) return gmb_p_parse_handler(p, line, GMB_STMT_REMOVE_HANDLER);
    return NULL;
}
