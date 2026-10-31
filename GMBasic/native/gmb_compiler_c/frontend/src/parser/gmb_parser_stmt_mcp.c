/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_mcp.c — speculare a
 * Parser.Stmt.Mcp.cs: AI MCP SERVER ADD|REMOVE|ENABLE|DISABLE|REFRESH|MARKETPLACE GET,
 * SERVE, STOP, REGISTER TOOL|AGENT, UNREGISTER TOOL — con argomenti separati da virgola. */
#include "gmb_parser_internal.h"
#include <stdlib.h>

gmb_stmt_t* gmb_p_parse_mcp(gmb_parser_t* p, int line)
{
    /* token corrente: MCP (il dispatcher ha già consumato AI) */
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->kind != GMB_TOK_Mcp) return NULL;
    gmb_p_adv(p);
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MCP, line);
    t = gmb_p_cur(p);
    if (!t) { gmb_p_set_err(p, "Atteso sottocomando dopo AI MCP", line); return s; }
    if (t->kind == GMB_TOK_Server) {
        gmb_p_adv(p);
        t = gmb_p_cur(p);
        if (!t) return s;
        if (t->kind == GMB_TOK_AiSubMarketplace) {
            gmb_p_adv(p);
            if (gmb_p_match(p, GMB_TOK_Get)) { s->mutex_action = "SERVER_MARKETPLACE_GET"; s->act_tk = GMB_TOK_McpServerMarketplaceGet; }
            else { gmb_p_set_err(p, "Atteso GET dopo AI MCP SERVER MARKETPLACE", line); return s; }
        } else if (t->kind == GMB_TOK_Add) { s->mutex_action = "SERVER_ADD"; s->act_tk = GMB_TOK_McpServerAdd; gmb_p_adv(p); }
        else if (t->kind == GMB_TOK_Remove) { s->mutex_action = "SERVER_REMOVE"; s->act_tk = GMB_TOK_McpServerRemove; gmb_p_adv(p); }
        else if (t->kind == GMB_TOK_Enable) { s->mutex_action = "SERVER_ENABLE"; s->act_tk = GMB_TOK_McpServerEnable; gmb_p_adv(p); }
        else if (t->kind == GMB_TOK_Disable) { s->mutex_action = "SERVER_DISABLE"; s->act_tk = GMB_TOK_McpServerDisable; gmb_p_adv(p); }
        else if (t->kind == GMB_TOK_Refresh) { s->mutex_action = "SERVER_REFRESH"; s->act_tk = GMB_TOK_McpServerRefresh; gmb_p_adv(p); }
        else { gmb_p_set_err(p, "Atteso ADD, REMOVE, ENABLE, DISABLE o REFRESH dopo AI MCP SERVER", line); return s; }
    } else if (t->kind == GMB_TOK_AiSubServe) { s->mutex_action = "SERVE"; s->act_tk = GMB_TOK_McpServe; gmb_p_adv(p); }
    else if (t->kind == GMB_TOK_Stop) { s->mutex_action = "STOP"; s->act_tk = GMB_TOK_McpStop; gmb_p_adv(p); }
    else if (t->kind == GMB_TOK_Register) {
        gmb_p_adv(p);
        t = gmb_p_cur(p);
        if (t && t->kind == GMB_TOK_AiSubTool) { s->mutex_action = "REGISTER_TOOL"; s->act_tk = GMB_TOK_McpRegisterTool; gmb_p_adv(p); }
        else if (t && t->kind == GMB_TOK_Agent) { s->mutex_action = "REGISTER_AGENT"; s->act_tk = GMB_TOK_McpRegisterAgent; gmb_p_adv(p); }
        else { gmb_p_set_err(p, "Atteso TOOL o AGENT dopo AI MCP REGISTER", line); return s; }
    } else if (t->kind == GMB_TOK_AiSubUnregister) {
        gmb_p_adv(p);
        if (!gmb_p_match(p, GMB_TOK_AiSubTool)) { gmb_p_set_err(p, "Atteso TOOL dopo AI MCP UNREGISTER", line); return s; }
        s->mutex_action = "UNREGISTER_TOOL"; s->act_tk = GMB_TOK_McpUnregisterTool;
    } else { gmb_p_set_err(p, "Atteso SERVER, SERVE, STOP, REGISTER o UNREGISTER dopo AI MCP", line); return s; }
    /* argomenti separati da virgola (min 1 per la maggior parte) */
    int cap = 0;
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
        for (;;) {
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 4; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = gmb_p_parse_expr(p);
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
            if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) break;
        }
    }
    return s;
}
