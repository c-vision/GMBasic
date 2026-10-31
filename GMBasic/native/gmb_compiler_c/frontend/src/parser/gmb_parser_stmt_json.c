/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_json.c — speculare a
 * Parser.Stmt.Json.cs: JSON SAVE src, path / WRITEVALUE key, value [, section] /
 * WRITESECTION src [, section] / OPEN file FOR mode AS #handle. */
#include "gmb_parser_internal.h"
#include <stdlib.h>

gmb_stmt_t* gmb_p_parse_json(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->kind != GMB_TOK_Json) return NULL;
    gmb_p_adv(p);   /* JSON */
    t = gmb_p_cur(p);
    if (!t) return NULL;
    if (t->kind == GMB_TOK_Save) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_JSON_SAVE, line);
        s->channel = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' tra sorgente e percorso in JSON SAVE", line); return s; }
        s->send_value = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_WriteValue) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_JSON_WRITEVALUE, line);
        s->go_args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        s->n_go_args = 2;
        s->go_args[0] = gmb_p_parse_expr(p);   /* key */
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo la chiave in JSON WRITEVALUE", line); return s; }
        s->go_args[1] = gmb_p_parse_expr(p);   /* value */
        if (gmb_p_match(p, GMB_TOK_Comma)) s->channel = gmb_p_parse_expr(p);   /* section */
        return s;
    }
    if (t->kind == GMB_TOK_WriteSection) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_JSON_WRITESECTION, line);
        s->channel = gmb_p_parse_expr(p);   /* source */
        if (gmb_p_match(p, GMB_TOK_Comma)) s->send_value = gmb_p_parse_expr(p);   /* section */
        return s;
    }
    if (t->kind == GMB_TOK_Open) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_JSON_OPEN, line);
        s->channel = gmb_p_parse_expr(p);   /* file */
        if (!gmb_p_match(p, GMB_TOK_For)) { gmb_p_set_err(p, "Atteso FOR dopo il percorso in JSON OPEN", line); return s; }
        s->send_value = gmb_p_parse_expr(p); /* mode */
        if (!gmb_p_match(p, GMB_TOK_As)) { gmb_p_set_err(p, "Atteso AS prima dell'handle in JSON OPEN", line); return s; }
        gmb_p_match(p, GMB_TOK_Hash);
        const gmb_token_t* h = gmb_p_cur(p);
        if (h && h->length > 0) { s->win_name = gmb_p_tok_strdup(h); gmb_p_adv(p); }
        return s;
    }
    gmb_p_set_err(p, "Comando non riconosciuto dopo 'JSON'", line);
    return NULL;
}
