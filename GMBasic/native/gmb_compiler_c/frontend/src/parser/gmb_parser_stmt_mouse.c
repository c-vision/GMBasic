/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_mouse.c — speculare a
 * Parser.Stmt.Mouse.cs: MOUSE ON/OFF, SHOW/HIDE, SET x,y, LIMIT x1,y1,x2,y2,
 * UNLIMIT, CURSOR id, WAIT. */
#include "gmb_parser_internal.h"
#include <stdlib.h>

gmb_stmt_t* gmb_p_parse_mouse(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->kind != GMB_TOK_Mouse) return NULL;
    gmb_p_adv(p);   /* MOUSE */
    t = gmb_p_cur(p);
    if (!t) return NULL;
    if (t->kind == GMB_TOK_On || t->kind == GMB_TOK_Off) {
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MOUSE_CONFIG, line);
        s->mutex_action = t->kind == GMB_TOK_On ? "On" : "Off";
        gmb_p_adv(p);
        return s;
    }
    if (t->kind == GMB_TOK_Show || t->kind == GMB_TOK_Hide) {
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MOUSE_VISIBILITY, line);
        s->mutex_action = t->kind == GMB_TOK_Show ? "Show" : "Hide";
        gmb_p_adv(p);
        return s;
    }
    if (t->kind == GMB_TOK_Set) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MOUSE_SET, line);
        s->win_coords[0] = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo X in MOUSE SET", line); return s; }
        s->win_coords[1] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Unlimit) {
        gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_MOUSE_LIMIT, line);
    }
    if (t->kind == GMB_TOK_Limit) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MOUSE_LIMIT, line);
        for (int i = 0; i < 4; i++) {
            s->win_coords[i] = gmb_p_parse_expr(p);
            if (i < 3 && !gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' in MOUSE LIMIT", line); break; }
        }
        return s;
    }
    if (t->kind == GMB_TOK_Cursor) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MOUSE_CURSOR, line);
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Wait) {
        gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_MOUSE_WAIT, line);
    }
    gmb_p_set_err(p, "Sottocomando MOUSE non riconosciuto", line);
    return NULL;
}
