/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_window.c — speculare a
 * Parser.Stmt.Window.cs: WINDOW THEME/DESKTOP/FRAME/SHADOW/SELECT/DEFINE/SET TITLE,
 * WINDOW <nome> PRINT/TEXT/CLEAR/SCROLL/EDIT/DESKTOP/FRAME/SHADOW/<azione>. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static const gmb_tokenkind_t gmb_tk_win_nonid[] = {
    GMB_TOK_Window, GMB_TOK_Define, GMB_TOK_Set, GMB_TOK_Select, GMB_TOK_Theme,
    GMB_TOK_Desktop, GMB_TOK_Frame, GMB_TOK_Shadow, GMB_TOK_From, GMB_TOK_To,
    GMB_TOK_Title, GMB_TOK_Screen, GMB_TOK_Viewport, GMB_TOK_Pixel
};
static int gmb_tk_win_nonid_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_win_nonid) / sizeof(gmb_tk_win_nonid[0]); i++)
        if (gmb_tk_win_nonid[i] == k) return 1;
    return 0;
}

/* parità IsWindowNameToken(C#): identifier/keyword non strutturali del comando WINDOW */
static bool gmb_p_is_win_name(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || !t->lexeme || t->length == 0) return false;
    char c = t->lexeme[0];
    if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
    if (gmb_tk_win_nonid_in(t->kind)) return false;
    return true;
}

gmb_stmt_t* gmb_p_parse_window(gmb_parser_t* p, int line)
{
    gmb_p_adv(p);   /* WINDOW */
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    /* WINDOW SHOW | WINDOW HIDE — mostra/nasconde la finestra gfx (SDL main-thread). */
    if (t->kind == GMB_TOK_Show || t->kind == GMB_TOK_Hide) {
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_ACTION, line);
        s->mutex_action = t->kind == GMB_TOK_Show ? "Show" : "Hide";
        gmb_p_adv(p);
        return s;
    }


    /* WINDOW CLOSE — richiede la chiusura (COMANDO globale). */
    if (t->kind == GMB_TOK_Close) { gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_WINDOW_CLOSE, line); }
    /* WINDOW PUMP <ms> — un frame del loop form (COMANDO a due token). */
    if (gmb_p_tok_is_soft_keyword(t, "PUMP")) {
        gmb_p_adv(p);
        gmb_stmt_t* sp = gmb_stmt_new(GMB_STMT_WINDOW_PUMP, line);
        sp->channel = gmb_p_parse_expr(p);
        return sp;
    }
    /* WINDOW RESIZE <w>, <h> — ridimensiona la pagina logica (come il resize della
     * finestra): è ciò che fa scattare il LAYOUT dei controlli (EXPAND/ANCHOR/ALIGN).
     * Comando, non funzione: due argomenti separati da virgola. */
    if (gmb_p_tok_is_soft_keyword(t, "RESIZE")) {
        gmb_p_adv(p);
        gmb_stmt_t* sp = gmb_stmt_new(GMB_STMT_WINDOW_RESIZE, line);
        sp->win_coords[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        sp->win_coords[1] = gmb_p_parse_expr(p);
        sp->win_coords[2] = NULL;
        sp->win_coords[3] = NULL;
        return sp;
    }

    /* WINDOW da solo o SCREEN -> viewport (graphics); qui WINDOW THEME/DESKTOP/FRAME/SHADOW/... */
    if (t->kind == GMB_TOK_Theme) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_THEME, line);
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Desktop) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_DESKTOP, line);
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Frame) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_FRAME, line);
        const gmb_token_t* st = gmb_p_cur(p);
        if (st && st->kind == GMB_TOK_Double) { s->mutex_action = "Double"; gmb_p_adv(p); }
        else if (st && st->kind == GMB_TOK_Single) { s->mutex_action = "Single"; gmb_p_adv(p); }
        else if (st && st->kind == GMB_TOK_Rounded) { s->mutex_action = "Rounded"; gmb_p_adv(p); }
        else gmb_p_set_err(p, "Attesi DOUBLE, SINGLE o ROUNDED dopo WINDOW FRAME", line);
        return s;
    }
    if (t->kind == GMB_TOK_Shadow) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_SHADOW, line);
        if (gmb_p_match(p, GMB_TOK_On)) s->mutex_action = "On";
        else if (gmb_p_match(p, GMB_TOK_Off)) s->mutex_action = "Off";
        else gmb_p_set_err(p, "Attesi ON o OFF dopo WINDOW SHADOW", line);
        return s;
    }
    if (t->kind == GMB_TOK_Select) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_SELECT, line);
        if (gmb_p_is_win_name(p)) { s->win_name = gmb_p_tok_strdup(gmb_p_cur(p)); gmb_p_adv(p); }
        else s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Define) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_DEFINE, line);
        if (gmb_p_is_win_name(p)) { s->win_name = gmb_p_tok_strdup(gmb_p_cur(p)); gmb_p_adv(p); }
        if (!gmb_p_match(p, GMB_TOK_From)) { gmb_p_set_err(p, "Atteso FROM", line); return s; }
        s->win_coords[0] = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ','", line); return s; }
        s->win_coords[1] = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_To)) { gmb_p_set_err(p, "Atteso TO", line); return s; }
        s->win_coords[2] = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ','", line); return s; }
        s->win_coords[3] = gmb_p_parse_expr(p);
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
            if (gmb_p_match(p, GMB_TOK_Title)) s->channel = gmb_p_parse_expr(p);
            else if (gmb_p_match(p, GMB_TOK_Double)) { s->mutex_action = "Double"; s->flag = 1; }
            else if (gmb_p_match(p, GMB_TOK_Single)) { s->mutex_action = "Single"; }
            else if (gmb_p_match(p, GMB_TOK_Rounded)) { s->mutex_action = "Rounded"; }
            else if (gmb_p_match(p, GMB_TOK_Shadow)) { s->flag = 2; }
            else if (gmb_p_match(p, GMB_TOK_Pixel)) { s->flag = 3; }
            else if (gmb_p_match(p, GMB_TOK_Desktop)) s->send_value = gmb_p_parse_expr(p);
            else break;
        }
        return s;
    }
    if (t->kind == GMB_TOK_Set) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WINDOW_SETTITLE, line);
        if (gmb_p_match(p, GMB_TOK_Title)) s->channel = gmb_p_parse_expr(p);
        else gmb_p_set_err(p, "Atteso TITLE dopo WINDOW SET", line);
        return s;
    }

    /* WINDOW <nome> <azione|contenuto> */
    if (!gmb_p_is_win_name(p)) { gmb_p_set_err(p, "Atteso nome finestra o DEFINE dopo WINDOW", line); return NULL; }
    char* name = gmb_p_tok_strdup(t);
    gmb_p_adv(p);
    const gmb_token_t* a = gmb_p_cur(p);
    if (!a) return NULL;
    gmb_stmt_t* s = NULL;
    if (a->kind == GMB_TOK_Print) { gmb_p_adv(p); s = gmb_stmt_new(GMB_STMT_WINDOW_APPEND, line); s->channel = gmb_p_parse_expr(p); }
    else if (a->kind == GMB_TOK_Text) { gmb_p_adv(p); s = gmb_stmt_new(GMB_STMT_WINDOW_CONTENT, line); s->channel = gmb_p_parse_expr(p); }
    else if (a->kind == GMB_TOK_Clear) { gmb_p_adv(p); s = gmb_stmt_new(GMB_STMT_WINDOW_CLEAR, line); }
    else if (a->kind == GMB_TOK_Scroll) { gmb_p_adv(p); s = gmb_stmt_new(GMB_STMT_WINDOW_SCROLL, line); s->channel = gmb_p_parse_expr(p); }
    else if (a->kind == GMB_TOK_Edit) {
        gmb_p_adv(p);
        s = gmb_stmt_new(GMB_STMT_WINDOW_EDIT, line);
        if (gmb_p_match(p, GMB_TOK_On)) s->mutex_action = "On";
        else if (gmb_p_match(p, GMB_TOK_Off)) s->mutex_action = "Off";
        else gmb_p_set_err(p, "Attesi ON o OFF dopo WINDOW EDIT", line);
    }
    else if (a->kind == GMB_TOK_Desktop) { gmb_p_adv(p); s = gmb_stmt_new(GMB_STMT_WINDOW_MOVEDESKTOP, line); s->channel = gmb_p_parse_expr(p); }
    else if (a->kind == GMB_TOK_Frame) {
        gmb_p_adv(p);
        s = gmb_stmt_new(GMB_STMT_WINDOW_FRAME, line);
        const gmb_token_t* st = gmb_p_cur(p);
        if (st && st->kind == GMB_TOK_Double) { s->mutex_action = "Double"; gmb_p_adv(p); }
        else if (st && st->kind == GMB_TOK_Single) { s->mutex_action = "Single"; gmb_p_adv(p); }
        else if (st && st->kind == GMB_TOK_Rounded) { s->mutex_action = "Rounded"; gmb_p_adv(p); }
        else gmb_p_set_err(p, "Attesi DOUBLE, SINGLE o ROUNDED dopo WINDOW <nome> FRAME", line);
    }
    else if (a->kind == GMB_TOK_Shadow) {
        gmb_p_adv(p);
        s = gmb_stmt_new(GMB_STMT_WINDOW_SHADOW, line);
        if (gmb_p_match(p, GMB_TOK_On)) s->mutex_action = "On";
        else if (gmb_p_match(p, GMB_TOK_Off)) s->mutex_action = "Off";
        else gmb_p_set_err(p, "Attesi ON o OFF dopo WINDOW <nome> SHADOW", line);
    }
    else {
        const char* act = NULL;
        if (a->kind == GMB_TOK_Activate) act = "Activate";
        else if (a->kind == GMB_TOK_Deactivate) act = "Deactivate";
        else if (a->kind == GMB_TOK_Release) act = "Release";
        else if (a->kind == GMB_TOK_Close) act = "Close";
        else if (a->kind == GMB_TOK_Minimize) act = "Minimize";
        else if (a->kind == GMB_TOK_Restore) act = "Restore";
        else { gmb_p_set_err(p, "Attesa azione finestra dopo il nome", line); free(name); return NULL; }
        gmb_p_adv(p);
        s = gmb_stmt_new(GMB_STMT_WINDOW_ACTION, line);
        s->mutex_action = act;
    }
    if (s) s->win_name = name;
    else free(name);
    return s;
}
