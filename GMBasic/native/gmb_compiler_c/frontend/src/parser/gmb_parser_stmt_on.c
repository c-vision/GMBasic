/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_on.c — speculare a Parser.Stmt.On.cs:
 * ON ERROR GOTO/RESUME, ON KEY <expr> GOTO/GOSUB, ON MOUSE, ON WINDOW. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

static int on_event_valid(const gmb_token_t* t)
{
    if (!t || !t->lexeme) return 0;
    static const char* ev[] = { "CLICK","DBLCLICK","CHANGE","GOTFOCUS","LOSTFOCUS","MOUSEDOWN","MOUSEUP",
        "MOUSEMOVE","KEYDOWN","KEYPRESS","KEYUP","DRAGDROP","DRAGOVER","VALIDATE","SCROLL","DROPDOWN",
        "UPCLICK","DOWNCLICK","NODECLICK","EXPAND","COLLAPSE","SELCHANGE","ROWCOLCHANGE" };
    char b[32]; int n = t->length; if (n > 31) n = 31; memcpy(b, t->lexeme, (size_t)n); b[n] = 0;
    for (size_t i = 0; i < sizeof(ev)/sizeof(ev[0]); i++) if (strcasecmp(b, ev[i]) == 0) return 1;
    return 0;
}

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

/* ON ERROR GOTO <label> | ON ERROR RESUME | ON KEY <expr> GOTO/GOSUB <label> |
 * ON MOUSE ... | ON WINDOW ... */
gmb_stmt_t* gmb_p_parse_on(gmb_parser_t* p, int line, bool* multiline)
{
    (void)multiline;
    /* ON <EVENTO> CONTROL <id> <azione> — evento controllo raggruppato (sovrascrive l'inline). */
    {
        const gmb_token_t* t0 = gmb_p_cur(p);
        const gmb_token_t* t1 = gmb_p_peek(p, 1);
        if (on_event_valid(t0) && t1 && gmb_p_tok_is_soft_keyword(t1, "CONTROL")) {
            gmb_stmt_t* sc = gmb_stmt_new(GMB_STMT_ON_CONTROL_EVENT, line);
            char* ev = (char*)malloc((size_t)t0->length + 1);
            memcpy(ev, t0->lexeme, (size_t)t0->length); ev[t0->length] = 0;
            sc->ce_event = ev;
            gmb_p_adv(p);   /* evento */
            gmb_p_adv(p);   /* CONTROL */
            sc->on_expr = gmb_p_parse_expr(p);
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") || gmb_p_check(p, GMB_TOK_Gosub)) { gmb_p_adv(p); sc->flag = 1; sc->target_label = read_label(p); }
            else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Goto") || gmb_p_check(p, GMB_TOK_Goto)) { gmb_p_adv(p); sc->flag = 3; sc->target_label = read_label(p); }
            else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Stop") || gmb_p_check(p, GMB_TOK_Stop)) { gmb_p_adv(p); sc->flag = 2; }
            else {
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Call") || gmb_p_check(p, GMB_TOK_Call)) gmb_p_adv(p);
                sc->flag = 0; sc->target_label = read_label(p);
                if (gmb_p_check(p, GMB_TOK_LeftParen)) { int depth = 0;
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) { int k = gmb_p_cur(p) ? gmb_p_cur(p)->kind : GMB_TOK_Newline; if (k == GMB_TOK_LeftParen) depth++; else if (k == GMB_TOK_RightParen) depth--; gmb_p_adv(p); if (depth <= 0) break; } }
            }
            return sc;
        }
    }
    /* ON MESSAGE <sub> CALL|GOSUB|STOP <handler> — evento del namespace MESSAGE. */
    if (gmb_p_check(p, GMB_TOK_Message) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Message")) {
        gmb_stmt_t* s2 = gmb_stmt_new(GMB_STMT_ON_MESSAGE, line);
        gmb_p_adv(p);
        if (gmb_p_cur(p) && !gmb_p_tok_is_keyword(gmb_p_cur(p), "Call") && !gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") &&
            !gmb_p_tok_is_keyword(gmb_p_cur(p), "Stop") && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Eof)) {
            s2->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*)); s2->net_args[0] = gmb_p_parse_expr(p); s2->n_netargs = 1;
        }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Call") || gmb_p_check(p, GMB_TOK_Call)) { gmb_p_adv(p); s2->flag = 0; s2->target_label = read_label(p); }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") || gmb_p_check(p, GMB_TOK_Gosub)) { gmb_p_adv(p); s2->flag = 1; s2->target_label = read_label(p); }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Stop") || gmb_p_check(p, GMB_TOK_Stop)) { gmb_p_adv(p); s2->flag = 2; }
        return s2;
    }
    /* ON SIGNAL <id> CALL <handler> — evento del namespace SIGNAL. */
    if (gmb_p_tok_is_soft_keyword(gmb_p_cur(p), "SIGNAL")) {
        return gmb_p_parse_on_signal(p, line);
    }
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ON_ERROR, line);
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Error") || gmb_p_check(p, GMB_TOK_Error)) {
        gmb_p_adv(p);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Goto") || gmb_p_check(p, GMB_TOK_Goto)) {
            gmb_p_adv(p);
            s->on_kind = "ERROR_GOTO";
            s->target_label = read_label(p);
        } else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Resume") || gmb_p_check(p, GMB_TOK_Resume)) {
            gmb_p_adv(p);
            /* ON ERROR RESUME NEXT = IMPOSTA l'handler resume-next (NON salta): on_kind
             * distinto da RESUME NEXT (statement di salto, on_kind ERROR_RESUME). */
            s->on_kind = "ERROR_RESUME_HANDLER";
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Next") || gmb_p_check(p, GMB_TOK_Next)) gmb_p_adv(p);
        } else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") || gmb_p_check(p, GMB_TOK_Gosub)) {
            /* ON ERROR GOSUB <handler> */
            gmb_p_adv(p);
            s->on_kind = "ERROR_GOSUB";
            s->target_label = read_label(p);
        } else {
            /* ON ERROR <procedureCall> */
            s->on_kind = "ERROR_CALL";
            s->on_call = gmb_p_parse_primary(p);
        }
        return s;
    }
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Key") || gmb_p_check(p, GMB_TOK_Key)) {
        gmb_p_adv(p);
        s->kind = GMB_STMT_ON_KEY;
        s->on_kind = "KEY";
        s->on_expr = gmb_p_parse_expr(p);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Goto") || gmb_p_check(p, GMB_TOK_Goto)) { gmb_p_adv(p); s->on_goto = true; }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") || gmb_p_check(p, GMB_TOK_Gosub)) { gmb_p_adv(p); s->on_goto = false; }
        s->target_label = read_label(p);
        return s;
    }
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Mouse") || gmb_p_check(p, GMB_TOK_Mouse)) {
        gmb_p_adv(p);
        s->kind = GMB_STMT_ON_MOUSE;
        s->on_kind = "MOUSE";
        s->on_expr = gmb_p_parse_expr(p);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Goto") || gmb_p_check(p, GMB_TOK_Goto)) { gmb_p_adv(p); s->on_goto = true; s->target_label = read_label(p); }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") || gmb_p_check(p, GMB_TOK_Gosub)) { gmb_p_adv(p); s->on_goto = false; s->target_label = read_label(p); }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Call") || gmb_p_check(p, GMB_TOK_Call)) { gmb_p_adv(p); s->target_label = read_label(p); s->on_goto = false; }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Stop") || gmb_p_check(p, GMB_TOK_Stop)) gmb_p_adv(p);
        return s;
    }
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Window") || gmb_p_check(p, GMB_TOK_Window)) {
        gmb_p_adv(p);
        s->kind = GMB_STMT_ON_WINDOW;
        s->on_kind = "WINDOW";
        { const gmb_token_t* evt = gmb_p_cur(p);
          if (evt && evt->lexeme) { char b[32]; int n = evt->length; if (n > 31) n = 31;
              for (int i = 0; i < n; i++) b[i] = (char)toupper((unsigned char)evt->lexeme[i]); b[n] = 0;
              s->mutex_action = (char*)strdup(b); gmb_p_adv(p); } }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Of") || gmb_p_check(p, GMB_TOK_Of)) { gmb_p_adv(p); s->of_expr = gmb_p_parse_expr(p); }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Goto") || gmb_p_check(p, GMB_TOK_Goto)) { gmb_p_adv(p); s->on_goto = true; s->target_label = read_label(p); }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Gosub") || gmb_p_check(p, GMB_TOK_Gosub)) { gmb_p_adv(p); s->on_goto = false; s->target_label = read_label(p); }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Call") || gmb_p_check(p, GMB_TOK_Call)) { gmb_p_adv(p); s->target_label = read_label(p); s->on_goto = false; }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Stop") || gmb_p_check(p, GMB_TOK_Stop)) gmb_p_adv(p);
        return s;
    }
    /* ON SPRITE COLLISION[S] <handler>: l'handler resta in target_label (on_kind SPRITE_COLLISION / SPRITE_COLLISIONS). */
    if (gmb_p_tok_is_soft_keyword(gmb_p_cur(p), "SPRITE") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Sprite")) {
        gmb_p_adv(p);
        const gmb_token_t* ev = gmb_p_cur(p);
        int plural = 0, ok = 0;
        if (ev && ev->lexeme && ev->length == 9 && strncasecmp(ev->lexeme, "COLLISION", 9) == 0) ok = 1;
        else if (ev && ev->lexeme && ev->length == 10 && strncasecmp(ev->lexeme, "COLLISIONS", 10) == 0) { ok = 1; plural = 1; }
        if (ok) {
            gmb_p_adv(p);
            s->on_kind = plural ? "SPRITE_COLLISIONS" : "SPRITE_COLLISION";
            s->target_label = read_label(p);
            return s;
        }
    }
    /* ON <evento> <handler> non riconosciuto (es. ON SPRITE COLLISION ...) — consuma la riga */
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
    s->on_kind = "OTHER";
    return s;
}
