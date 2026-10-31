/* native/gmb_compiler_c/src/parser/gmb_parser_stmt_ai_server.c — speculare a
 * Parser.Stmt.Ai.cs (ParseAiServerStatement):
 *   AI SERVER START <dir>      → GMB_STMT_AISERVER_START  (1 arg: dir)
 *   AI SERVER STOP             → GMB_STMT_AISERVER_STOP
 *   AI SERVER STATUS           → GMB_STMT_AISERVER_STATUS
 *   AI SERVER MODEL <dir>      → GMB_STMT_AISERVER_MODEL  (1 arg: dir)
 *   AI SERVER UNLOAD           → GMB_STMT_AISERVER_UNLOAD
 * Il chiamante ha gia' consumato 'AI' + 'SERVER'; qui il token corrente e' il verbo.
 * Runtime: gmb_ai_server_* del runtime condiviso (gmb_runtime_c) — il server LLM come
 * processo esterno; go_args[0] = dir (espressione stringa). */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
static const gmb_tokenkind_t gmb_tk_nx_term_toks[] = { GMB_TOK_LeftParen, GMB_TOK_LeftBracket, GMB_TOK_Dot, GMB_TOK_Arrow };
static int gmb_tk_nx_term_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_nx_term_toks) / sizeof(gmb_tk_nx_term_toks[0]); i++)
        if (gmb_tk_nx_term_toks[i] == k) return 1;
    return 0;
}

static void gmb_p_aisrv_add_arg(gmb_stmt_t* s, gmb_expr_t* e)
{
    int cap = s->n_go_args + 1;
    s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*));
    s->go_args[s->n_go_args++] = e;
}

/* Termine argomento: stringa/numero/`value` → espressione; identifier semplice → stringa
 * letterale (parita' con AI AGENT <nome>); `x(`/`x[`/`x.` → espressione. */
static gmb_expr_t* gmb_p_aisrv_term(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->length == 0) return NULL;
    if (t->kind == GMB_TOK_StringLiteral || t->kind == GMB_TOK_Number || t->kind == GMB_TOK_Value
        || t->kind == GMB_TOK_Ampersand) {
        return gmb_p_parse_expr(p);
    }
    if ((t->kind == GMB_TOK_Identifier || t->kind == GMB_TOK_Self) && gmb_p_peek(p, 1)) {
        gmb_tokenkind_t nx = gmb_p_peek(p, 1)->kind;
        if (gmb_tk_nx_term_in(nx)) return gmb_p_parse_expr(p);
    }
    /* identifier nudo DICHIARATO (DIM/param) → variabile, non letterale-nome (bug P4) */
    if (t->kind == GMB_TOK_Identifier && gmb_p_is_declared_tok(p, t)) return gmb_p_parse_expr(p);
    /* token nudo → stringa letterale */
    char buf[512];
    int l = t->length < 511 ? t->length : 511;
    memcpy(buf, t->lexeme, (size_t)l);
    buf[l] = 0;
    size_t bl = strlen(buf);
    if (bl >= 2 && buf[0] == '"' && buf[bl - 1] == '"') { memmove(buf, buf + 1, bl - 2); buf[bl - 2] = 0; }
    gmb_p_adv(p);
    return gmb_expr_str(buf, line);
}

static void gmb_p_aisrv_to_eol(gmb_parser_t* p)
{
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)
        && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
}

gmb_stmt_t* gmb_p_parse_ai_server(gmb_parser_t* p, int line)
{
    const gmb_token_t* v = gmb_p_cur(p);
    if (!v) return NULL;

    gmb_stmt_kind_t kind;
    int with_arg = 0;

    if (v->kind == GMB_TOK_Start || gmb_p_tok_is_keyword(v, "Start")) { kind = GMB_STMT_AISERVER_START; with_arg = 1; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Stop || gmb_p_tok_is_keyword(v, "Stop")) { kind = GMB_STMT_AISERVER_STOP; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_AiSubStatus || gmb_p_tok_is_keyword(v, "Status")) { kind = GMB_STMT_AISERVER_STATUS; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_AiSubModel || gmb_p_tok_is_keyword(v, "Model")) { kind = GMB_STMT_AISERVER_MODEL; with_arg = 1; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Unload || gmb_p_tok_is_keyword(v, "Unload")) { kind = GMB_STMT_AISERVER_UNLOAD; gmb_p_adv(p); }
    else {
        gmb_p_set_err(p, "Atteso START, STOP, STATUS, MODEL o UNLOAD dopo AI SERVER", line);
        return NULL;
    }

    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    if (with_arg) {
        gmb_expr_t* dir = gmb_p_aisrv_term(p, line);
        if (dir) gmb_p_aisrv_add_arg(s, dir);
        /* START <dir> [port]: porta opzionale come secondo argomento in riga */
        if (kind == GMB_STMT_AISERVER_START) {
            const gmb_token_t* nxt = gmb_p_cur(p);
            if (nxt && nxt->kind != GMB_TOK_Newline && nxt->kind != GMB_TOK_Colon && nxt->length > 0) {
                gmb_expr_t* port = gmb_p_aisrv_term(p, line);
                if (port) gmb_p_aisrv_add_arg(s, port);
            }
        }
    }
    gmb_p_aisrv_to_eol(p);
    return s;
}
