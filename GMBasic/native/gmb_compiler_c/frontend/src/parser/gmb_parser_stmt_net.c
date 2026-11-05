/* gmb_parser_stmt_net.c — parser NET (TCP/UDP/SSL/HTTP/WS) + GFX (SCREEN/PSET/...).
 * Estratto da gmb_parser_statements.c (2026-08-30 — split, file <= 1200 righe). */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

char* net_target_name(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->kind == GMB_TOK_Newline || t->kind == GMB_TOK_Eof) return NULL;
    char* n = gmb_p_tok_strdup(t);
    gmb_p_adv(p);
    return n;
}

void net_next_arg(gmb_parser_t* p)
{
    gmb_p_match(p, GMB_TOK_Comma);
    gmb_p_skip_eol(p);
}

gmb_stmt_t* gmb_p_parse_net_cmd(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    gmb_tokenkind_t ns = t->kind;
    gmb_p_adv(p);
    const gmb_token_t* v = gmb_p_cur(p);
    if (!v) return gmb_stmt_new(GMB_STMT_REM, line);

    if (ns == GMB_TOK_Fetch) {
        /* FETCH url [, opts] INTO Callback[()] — comando asincrono (parity Parser.Stmt.Network.cs ParseFetchStatement).
         * net_sub 255 = FETCH-async: net_args = {url, opts|NULL}, net_coderef = nome della SUB callback (response$, error$). */
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
        s->net_sub = 255; s->coderef_pos = 2;
        s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_Into)) s->net_args[1] = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Into)) { gmb_p_set_err(p, "FETCH: atteso 'INTO' con la SUB di callback", line); return s; }
        const gmb_token_t* cb = gmb_p_cur(p);
        if (!cb || cb->kind != GMB_TOK_Identifier) { gmb_p_set_err(p, "FETCH: atteso il nome della SUB di callback dopo INTO", line); return s; }
        s->net_coderef = gmb_p_tok_strdup(cb);
        gmb_p_adv(p);
        if (gmb_p_match(p, GMB_TOK_LeftParen)) gmb_p_match(p, GMB_TOK_RightParen);
        return s;
    }
    if (ns == GMB_TOK_Shutdown) {
        /* SHUTDOWN handle, mode */
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
        s->net_sub = 12; s->coderef_pos = -1;
        s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
        s->net_args[0] = gmb_p_parse_expr(p);
        net_next_arg(p);
        s->net_args[1] = gmb_p_parse_expr(p);
        return s;
    }
    if (ns == GMB_TOK_SetOption) {
        /* SETOPTION handle, option, value */
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
        s->net_sub = 13; s->coderef_pos = -1;
        s->net_args = (gmb_expr_t**)calloc(3, sizeof(gmb_expr_t*)); s->n_netargs = 3;
        s->net_args[0] = gmb_p_parse_expr(p);
        net_next_arg(p);
        s->net_args[1] = gmb_p_parse_expr(p);
        net_next_arg(p);
        s->net_args[2] = gmb_p_parse_expr(p);
        return s;
    }

    gmb_tokenkind_t verb = v->kind;
    gmb_p_adv(p);
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
    s->coderef_pos = -1;

    if (ns == GMB_TOK_Tcp) {
        if (verb == GMB_TOK_Open) {
            /* TCP OPEN port, handle [, engine] → sub 0 argc 2 */
            s->net_sub = 0;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
        } else if (verb == GMB_TOK_Connect) {
            /* TCP CONNECT host, port, handle [, timeout [, engine]] → sub 1 argc 4
               (ordine emissione C#: host, port, engine, timeout) */
            s->net_sub = 1;
            s->net_args = (gmb_expr_t**)calloc(4, sizeof(gmb_expr_t*)); s->n_netargs = 4;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_args[1] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); gmb_expr_t* to = gmb_p_parse_expr(p); s->net_args[3] = to; }
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[2] = gmb_p_parse_expr(p); }
        } else if (verb == GMB_TOK_Accept) {
            /* TCP ACCEPT server, client [, timeout] → sub 2 argc 2 */
            s->net_sub = 2;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
        } else if (verb == GMB_TOK_Send) {
            /* TCP SEND handle, data → sub 3 argc 2 */
            s->net_sub = 3;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_args[1] = gmb_p_parse_expr(p);
        } else if (verb == GMB_TOK_Recv) {
            /* TCP RECV handle, buffer, bytes [, timeout] → sub 4 argc 2;
               pop: bytes (top), poi buffer */
            s->net_sub = 4;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            char* buf = net_target_name(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            s->net_assign2 = buf;
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
        } else if (verb == GMB_TOK_Close) {
            /* TCP CLOSE handle → sub 5 argc 1 */
            s->net_sub = 5;
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*)); s->n_netargs = 1;
            s->net_args[0] = gmb_p_parse_expr(p);
        } else { gmb_stmt_free(s); while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_REM, line); }
    } else if (ns == GMB_TOK_Udp) {
        if (verb == GMB_TOK_Open) {
            /* UDP OPEN [port,] handle [, engine] → sub 6 (na[0]=port o engine, assign=handle).
             * Forma SENZA porta ma COL comma iniziale ("UDP OPEN, h"): se si prova a
             * parse_expr sul comma il recupero d'errore perde il target (emetteva
             * `_2C = udp_open(0)` e h restava 0). Il lookahead sul comma DISTINGUE
             * la forma: porta omessa → salta il comma, legge l'handle. */
            s->net_sub = 6;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
            if (gmb_p_check(p, GMB_TOK_Comma)) {
                net_next_arg(p);
                s->net_assign = net_target_name(p);
                /* engine in na[1] come nella forma con porta: na[0]=port resta NULL → 0 */
                if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
            } else {
                gmb_expr_t* first = gmb_p_parse_expr(p);
                if (gmb_p_check(p, GMB_TOK_Comma)) {
                    s->net_args[0] = first;
                    net_next_arg(p);
                    s->net_assign = net_target_name(p);
                    if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
                } else {
                    /* solo handle: primo token era il target */
                    if (first && first->kind == GMB_EXPR_VAR && first->var) s->net_assign = strdup(first->var);
                    gmb_expr_free(first);
                }
            }
        } else if (verb == GMB_TOK_Send) {
            /* UDP SEND handle, host, port, data → sub 7 argc 4 */
            s->net_sub = 7;
            s->net_args = (gmb_expr_t**)calloc(4, sizeof(gmb_expr_t*)); s->n_netargs = 4;
            for (int i = 0; i < 4; i++) { s->net_args[i] = gmb_p_parse_expr(p); if (i < 3) net_next_arg(p); }
        } else if (verb == GMB_TOK_Recv) {
            /* UDP RECV handle, buffer, remoteIp, remotePort [, timeout] → sub 8 argc 2;
               pop: remotePort, remoteIp, buffer */
            s->net_sub = 8;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*)); s->n_netargs = 2;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            char* buf = net_target_name(p);
            net_next_arg(p);
            char* rip = net_target_name(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            s->net_assign2 = rip;
            s->net_assign3 = buf;
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
        } else if (verb == GMB_TOK_Close) {
            /* UDP CLOSE handle → sub 9 argc 1 */
            s->net_sub = 9;
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*)); s->n_netargs = 1;
            s->net_args[0] = gmb_p_parse_expr(p);
        } else { gmb_stmt_free(s); while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_REM, line); }
    } else if (ns == GMB_TOK_Ssl) {
        if (verb == GMB_TOK_Open) {
            /* SSL OPEN port, handle, certPath [, certPass] → sub 10 argc 3
               (emissione: port, certPath, certPass) */
            s->net_sub = 10;
            s->net_args = (gmb_expr_t**)calloc(3, sizeof(gmb_expr_t*)); s->n_netargs = 3;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            net_next_arg(p);
            s->net_args[1] = gmb_p_parse_expr(p);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[2] = gmb_p_parse_expr(p); }
        } else if (verb == GMB_TOK_Connect) {
            /* SSL CONNECT host, handle [, port [, timeout]] → sub 11 argc 3
               (emissione: host, port, timeout) */
            s->net_sub = 11;
            s->net_args = (gmb_expr_t**)calloc(3, sizeof(gmb_expr_t*)); s->n_netargs = 3;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[1] = gmb_p_parse_expr(p); }
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[2] = gmb_p_parse_expr(p); }
        } else { gmb_stmt_free(s); while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_REM, line); }
    } else if (ns == GMB_TOK_WebSocket) {
        if (verb == GMB_TOK_Connect) {
            /* WEBSOCKET CONNECT host, port, handle [, path [, timeout]] → sub 20 argc 4
               (emissione: host, port, path|"/", timeout) */
            s->net_sub = 20;
            s->net_args = (gmb_expr_t**)calloc(4, sizeof(gmb_expr_t*)); s->n_netargs = 4;
            s->net_args[0] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_args[1] = gmb_p_parse_expr(p);
            net_next_arg(p);
            s->net_assign = net_target_name(p);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[2] = gmb_p_parse_expr(p); }
            else s->net_args[2] = gmb_expr_str("/", line);
            if (gmb_p_check(p, GMB_TOK_Comma)) { net_next_arg(p); s->net_args[3] = gmb_p_parse_expr(p); }
        } else { gmb_stmt_free(s); while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_REM, line); }
    }
    return s;
}

/* ─────────────── 2.6(r): statement grafici (GMB_STMT_GFX) ───────────────
 * SCREEN/SYNC/POINT/LOAD TEXTURE: emissione gmb_c99_emit.c → gmb_gfx_* del
 * runtime condiviso (null-mode GMB_GFX_NULL = parità NullGraphicsService).
 * net_sub: 1=SCREEN, 2=SYNC, 3=POINT, 4=LOAD TEXTURE, 5=LOAD TEXTURE CELL. */
gmb_stmt_t* gfx_stmt(gmb_parser_t* p, int sub, int argc, int line) {
    (void)p;
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GFX, line);
    s->net_sub = sub;
    if (argc > 0) { s->net_args = (gmb_expr_t**)calloc((size_t)argc, sizeof(gmb_expr_t*)); s->n_netargs = argc; }
    return s;
}

gmb_stmt_t* gmb_p_parse_gfx(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (t->kind == GMB_TOK_Sync) {
        gmb_p_adv(p);
        return gfx_stmt(p, 2, 0, line);
    }
    if (t->kind == GMB_TOK_Screen) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 1, 4, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        /* SCREEN n decide la MODALITA': n==0 => TUI, n!=0 => GRAFICA (controlli pixel). */
        if (s->net_args[0] && s->net_args[0]->kind == GMB_EXPR_NUM) p->gui_mode = (s->net_args[0]->num != 0.0);
        for (int i = 1; i < 4; i++) {
            if (!gmb_p_check(p, GMB_TOK_Comma)) break;
            gmb_p_adv(p);
            s->net_args[i] = gmb_p_parse_expr(p);
        }
        return s;
    }
    if (t->kind == GMB_TOK_TextMode) {
        /* TEXTMODE n — modalita' console (0=%SDL,1=%C64,2=%ANSI,3=%SYSTEM) */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 47, 1, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Point) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 3, 2, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_check(p, GMB_TOK_Comma)) { gmb_p_adv(p); s->net_assign = net_target_name(p); }
        return s;
    }
    if (t->kind == GMB_TOK_ScreenSet) {
        /* SCREENSET [visibile][, attiva] → gmb_gfx_screenset (omesso = -1) */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 6, 2, line);
        if (!gmb_p_check(p, GMB_TOK_Comma)) s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_PSet || t->kind == GMB_TOK_Preset) {
        /* PSET (x,y) [, colore] / PRESET (x,y) */
        bool is_pset = (t->kind == GMB_TOK_PSet);
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, is_pset ? 7 : 8, is_pset ? 3 : 2, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (is_pset && gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Line) {
        /* LINE [(x1,y1)]-(x2,y2) [, colore [, stile]] (stile: B/BF/n)
         * Forma relativa `LINE -(x2,y2)`: x1/y1 restano NULL e il runtime parte
         * dal punto corrente (g_gfx.cursor, semantica QuickBASIC). */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 9, 6, line);
        if (!gmb_p_check(p, GMB_TOK_Minus)) {
            gmb_p_match(p, GMB_TOK_LeftParen);
            s->net_args[0] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            s->net_args[1] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        gmb_p_match(p, GMB_TOK_Minus);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[2] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[3] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_match(p, GMB_TOK_Comma)) {
            if (gmb_p_match(p, GMB_TOK_Comma)) { s->net_assign = net_target_name(p); }
            else {
                s->net_args[4] = gmb_p_parse_expr(p);
                if (gmb_p_match(p, GMB_TOK_Comma)) s->net_assign = net_target_name(p);
            }
        }
        return s;
    }
    if (t->kind == GMB_TOK_Circle) {
        /* CIRCLE (x,y), raggio [, colore [, aspetto]] */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 10, 5, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        /* radius [, color] [, start] [, end] [, aspect] — ogni arg omesso (virgola
         * immediata) lascia lo slot NULL e AVANZA (parità ParseCircleStatement/POT).
         * net_args: [2]=raggio, [3]=colore, [4]=aspetto (start/end ignorati). */
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
        { gmb_expr_t* opt[4] = { 0, 0, 0, 0 };
          for (int a = 0; a < 4 && gmb_p_match(p, GMB_TOK_Comma); a++) {
              if (gmb_p_check(p, GMB_TOK_Comma) || gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) continue;
              opt[a] = gmb_p_parse_expr(p);
          }
          s->net_args[3] = opt[0];   /* colore */
          s->net_args[4] = opt[3];   /* aspetto */
        }
        return s;
    }
    if (t->kind == GMB_TOK_Paint) {
        /* PAINT (x,y) [, riempimento [, bordo]] */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 11, 4, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[3] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Draw && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Textures) {
        /* DRAW TEXTURES tex, X(), Y(), Rot(), Scale()  |  DRAW TEXTURES Sprites() */
        gmb_p_adv(p); gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 29, 5, line);
        int i = 0;
        s->net_args[i++] = gmb_p_parse_expr(p);
        while (i < 5 && gmb_p_match(p, GMB_TOK_Comma)) s->net_args[i++] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Draw && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Texture) {
        /* DRAW TEXTURE id, x, y [, rot [, sx, sy]] */
        gmb_p_adv(p); gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 19, 6, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        for (int i = 1; i < 6; i++) { if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[i] = gmb_p_parse_expr(p); }
        return s;
    }
    if (t->kind == GMB_TOK_Draw) {
        gmb_p_adv(p);
        if (gmb_p_check(p, GMB_TOK_Text)) {
            /* DRAW TEXT "s", x, y [, align] */
            gmb_p_adv(p);
            gmb_stmt_t* s = gfx_stmt(p, 12, 4, line);
            s->net_args[0] = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) {
                /* ALIGN_LEFT/CENTER/RIGHT sono KEYWORD di allineamento (TextAlign C#:
                 * 0/1/2), non variabili. Prima restavano identificatori → variabile
                 * implicita 0 e DRAW TEXT sempre LEFT (bug GraphicsDrawText). */
                int ak = gmb_p_cur(p) ? (int)gmb_p_cur(p)->kind : 0;
                if (ak == GMB_TOK_AlignCenter) { gmb_p_adv(p); s->net_args[3] = gmb_expr_num(1, line); }
                else if (ak == GMB_TOK_AlignRight) { gmb_p_adv(p); s->net_args[3] = gmb_expr_num(2, line); }
                else if (ak == GMB_TOK_AlignLeft) { gmb_p_adv(p); s->net_args[3] = gmb_expr_num(0, line); }
                else s->net_args[3] = gmb_p_parse_expr(p);
            }
            return s;
        }
        /* DRAW <stringa comando> */
        gmb_stmt_t* s = gfx_stmt(p, 13, 1, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Font) {
        /* FONT "file", size */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 14, 2, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_PCopy) {
        /* PCOPY src, dst */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 15, 2, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Palette && gmb_p_peek(p, 1) &&
        (gmb_p_peek(p, 1)->kind == GMB_TOK_Use || gmb_p_peek(p, 1)->kind == GMB_TOK_Define ||
         gmb_p_peek(p, 1)->kind == GMB_TOK_Replace || gmb_p_peek(p, 1)->kind == GMB_TOK_Save ||
         gmb_p_peek(p, 1)->kind == GMB_TOK_Load)) {
        /* PALETTE USE/DEFINE/REPLACE/SAVE/LOAD: palette registrate (no-op sul runtime
         * software, parita' emitter C#). Verbo conservato per il commento. */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 45, 2, line);
        s->net_assign = net_target_name(p);
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->net_args[0] = gmb_p_parse_expr(p);
        /* DEFINE/REPLACE: secondo argomento = array dei colori (arr()). */
        if (gmb_p_match(p, GMB_TOK_Comma))
            if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p))
                s->net_args[1] = gmb_p_parse_expr(p);
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) gmb_p_adv(p);
        return s;
    }
    if (t->kind == GMB_TOK_Palette) {
        /* PALETTE idx, r, g, b */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 16, 4, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        for (int i = 1; i < 4; i++) { if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[i] = gmb_p_parse_expr(p); }
        return s;
    }
    if (t->kind == GMB_TOK_View || t->kind == GMB_TOK_Viewport) {
        /* VIEW/WINDOW [(x1,y1)-(x2,y2)] — senza coordinate: reset (0,0,0,0) */
        bool is_view = (t->kind == GMB_TOK_View);
        gmb_p_adv(p);
        /* VIEW PRINT [top TO bottom] — regione di scroll della console: no-op (parità
         * emitter C# ViewPrintStmt). Senza, `PRINT` restava e dava "statement non
         * riconosciuto". */
        if (is_view && (gmb_p_check(p, GMB_TOK_Print) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Print"))) {
            gmb_p_adv(p);
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
            return gmb_stmt_new(GMB_STMT_REM, line);
        }
        gmb_p_match(p, GMB_TOK_Screen);   /* VIEW/WINDOW SCREEN (...): flag asse Y */
        gmb_stmt_t* s = gfx_stmt(p, is_view ? 17 : 18, 4, line);
        if (gmb_p_check(p, GMB_TOK_LeftParen)) {
            gmb_p_match(p, GMB_TOK_LeftParen);
            s->net_args[0] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            s->net_args[1] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
            gmb_p_match(p, GMB_TOK_Minus);
            gmb_p_match(p, GMB_TOK_LeftParen);
            s->net_args[2] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            s->net_args[3] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        /* VIEW [SCREEN] (...) [, color_sfondo [, bordo]] — memorizzati in value/cond
         * per l'emitter (gmb_gfx_view_style). */
        if (is_view && gmb_p_match(p, GMB_TOK_Comma)) {
            if (!gmb_p_check(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_Newline)
                && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p))
                s->value = gmb_p_parse_expr(p); /* color */
            if (gmb_p_match(p, GMB_TOK_Comma))
                if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p))
                    s->cond = gmb_p_parse_expr(p); /* border */
        }
        return s;
    }
    if (t->kind == GMB_TOK_Window) {
        /* WINDOW grafico [SCREEN] [(x1,y1)-(x2,y2)] (sub 18): il WINDOW TUI
         * (nome/DEFINE/THEME/...) e' instradato a gmb_p_parse_window dal dispatcher. */
        gmb_p_adv(p);
        gmb_p_match(p, GMB_TOK_Screen);
        gmb_stmt_t* s = gfx_stmt(p, 18, 4, line);
        if (gmb_p_check(p, GMB_TOK_LeftParen)) {
            gmb_p_match(p, GMB_TOK_LeftParen);
            s->net_args[0] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            s->net_args[1] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
            gmb_p_match(p, GMB_TOK_Minus);
            gmb_p_match(p, GMB_TOK_LeftParen);
            s->net_args[2] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            s->net_args[3] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        return s;
    }
    if (t->kind == GMB_TOK_Unload && gmb_p_peek(p, 1) &&
        (gmb_p_peek(p, 1)->kind == GMB_TOK_Texture || gmb_p_peek(p, 1)->kind == GMB_TOK_Font)) {
        /* UNLOAD TEXTURE id / UNLOAD FONT */
        gmb_p_adv(p); gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 26, 1, line);
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->net_args[0] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_BSave) {
        /* BSAVE (x1,y1)-(x2,y2) [, buffer] */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 27, 4, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        gmb_p_match(p, GMB_TOK_Minus);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[2] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[3] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_match(p, GMB_TOK_Comma)) (void)gmb_p_parse_expr(p); /* buffer ignorato (parita' C#) */
        return s;
    }
    if (t->kind == GMB_TOK_BLoad) {
        /* BLOAD (x,y) [, buffer] */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 28, 2, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_match(p, GMB_TOK_Comma)) (void)gmb_p_parse_expr(p); /* buffer ignorato (parita' C#) */
        return s;
    }
    if (t->kind == GMB_TOK_ImageGet) {
        /* IMAGEGET (x1,y1)-(x2,y2), arrayVar */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 30, 4, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        gmb_p_match(p, GMB_TOK_Minus);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[2] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[3] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_assign = net_target_name(p);
        return s;
    }
    if (t->kind == GMB_TOK_ImagePut) {
        /* IMAGEPUT (x,y), arrayVar [, mode] */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 31, 3, line);
        gmb_p_match(p, GMB_TOK_LeftParen);
        s->net_args[0] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
        s->net_args[1] = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_RightParen);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_assign = net_target_name(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_ImagePuts) {
        /* IMAGEPUTS spriteArray, positionsArray [, mode] */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 32, 3, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Polygon) {
        /* POLYGON array(), colore */
        gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 33, 2, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Canvas) {
        /* CANVAS NEW/USE/END/CLEAR/SET/GET/BLIT/SAVE/LOAD/FREE/WIDTH/HEIGHT.
         * sub 53..64; argomenti in net_args[0..3] (path compreso), INTO in net_assign. */
        const gmb_token_t* act = gmb_p_peek(p, 1);
        int sub = 0;
        if (gmb_p_tok_is_soft_keyword(act, "NEW")) sub = 53;
        else if (gmb_p_tok_is_soft_keyword(act, "USE")) sub = 54;
        else if (gmb_p_tok_is_soft_keyword(act, "END")) sub = 55;
        else if (gmb_p_tok_is_soft_keyword(act, "CLEAR")) sub = 56;
        else if (gmb_p_tok_is_soft_keyword(act, "SET")) sub = 57;
        else if (gmb_p_tok_is_soft_keyword(act, "GET")) sub = 58;
        else if (gmb_p_tok_is_soft_keyword(act, "BLIT")) sub = 59;
        else if (gmb_p_tok_is_soft_keyword(act, "SAVE")) sub = 60;
        else if (gmb_p_tok_is_soft_keyword(act, "LOAD")) sub = 61;
        else if (gmb_p_tok_is_soft_keyword(act, "FREE")) sub = 62;
        else if (gmb_p_tok_is_soft_keyword(act, "WIDTH")) sub = 63;
        else if (gmb_p_tok_is_soft_keyword(act, "HEIGHT")) sub = 64;
        if (sub) {
            gmb_p_adv(p); gmb_p_adv(p);
            gmb_stmt_t* s = gfx_stmt(p, sub, 4, line);
            int i = 0;
            if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Into)) {
                s->net_args[i++] = gmb_p_parse_expr(p);
                while (i < 4 && gmb_p_match(p, GMB_TOK_Comma)) s->net_args[i++] = gmb_p_parse_expr(p);
            }
            if (gmb_p_check(p, GMB_TOK_Into)) { gmb_p_adv(p); s->net_assign = net_target_name(p); }
            s->n_netargs = i;
            return s;
        }
    }
    if (t->kind == GMB_TOK_Chart) {
        const gmb_token_t* act = gmb_p_peek(p, 1);
        int sub = 0; int cfg_get = 0;
        if (act && act->kind == GMB_TOK_Plot) sub = 34;
        else if (act && act->kind == GMB_TOK_Show) sub = 35;
        else if (act && act->kind == GMB_TOK_Save) sub = 36;
        else if (act && act->kind == GMB_TOK_Clear) sub = 37;
        else if (act && act->kind == GMB_TOK_Close) sub = 38;
        else if (act && act->kind == GMB_TOK_Financial) sub = 48;
        else if (act && act->kind == GMB_TOK_Set) sub = 49;
        else if (act && act->kind == GMB_TOK_Get) { sub = 50; cfg_get = 1; }
        else if (gmb_p_tok_is_soft_keyword(act, "GETS")) { sub = 51; cfg_get = 1; }
        else if (act && act->kind == GMB_TOK_Draw) sub = 52;
        if (sub) {
            gmb_p_adv(p); gmb_p_adv(p);
            int cap = (sub == 52) ? 9 : 6;   /* DRAW: handle, op, + fino a 7 argomenti */
            gmb_stmt_t* s = gfx_stmt(p, sub, cap, line);
            int i = 0;
            s->net_args[i++] = gmb_p_parse_expr(p);
            while (i < cap && gmb_p_match(p, GMB_TOK_Comma)) s->net_args[i++] = gmb_p_parse_expr(p);
            /* CHART DRAW: n_netargs = argomenti EFFETTIVI (cap 9 = sola allocazione).
             * Con 9 fissi, gmb_gfx_chart_draw riceveva 7 argomenti con zeri in coda e
             * interpretava il colore (args[cidx]) come 0 = nero (vline/hline/rect
             * invisibili, bug ChartDrawDemo). */
            if (sub == 52) s->n_netargs = i;
            /* CHART GET/GETS/DRAW ... INTO <var> */
            if ((cfg_get || sub == 52) && gmb_p_check(p, GMB_TOK_Into)) { gmb_p_adv(p); s->net_assign = net_target_name(p); }
            return s;
        }
    }
    if (t->kind == GMB_TOK_PixelBuffer
        || (t->kind == GMB_TOK_Pixel && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Buffer)) {
        /* PIXELBUFFER w, h, buffer — anche forma a spazio `PIXEL BUFFER w, h, buffer`
         * (PIXEL=GMB_TOK_Pixel + BUFFER=GMB_TOK_Buffer; prima cadeva in "statement non
         * riconosciuto"). Parità C# PixelBufferStmt. */
        gmb_p_adv(p);
        if (t->kind == GMB_TOK_Pixel) gmb_p_adv(p);   /* consuma BUFFER */
        gmb_stmt_t* s = gfx_stmt(p, 39, 3, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[1] = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->net_args[2] = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Tilemap) {
        const gmb_token_t* act = gmb_p_peek(p, 1);
        int sub = 0;
        if (act && act->kind == GMB_TOK_Load) sub = 40;
        else if (act && act->kind == GMB_TOK_Set) sub = 41;
        else if (act && act->kind == GMB_TOK_Offset) sub = 42;
        else if (act && act->kind == GMB_TOK_Scroll) sub = 43;
        else if (act && act->kind == GMB_TOK_Draw) sub = 44;
        if (sub) {
            gmb_p_adv(p); gmb_p_adv(p);
            gmb_stmt_t* s = gfx_stmt(p, sub, 7, line);
            int i = 0;
            if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
                s->net_args[i++] = gmb_p_parse_expr(p);
                while (i < 7 && gmb_p_match(p, GMB_TOK_Comma)) s->net_args[i++] = gmb_p_parse_expr(p);
            }
            return s;
        }
    }
    if (t->kind == GMB_TOK_Spritedef) {
        /* SPRITEDEF START id 
 "riga"... 
 SPRITEDEF END — righe in net_args (string expr),
         * id in value; sub 46. */
        gmb_p_adv(p);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Start || gmb_p_tok_is_keyword(gmb_p_cur(p), "Start")))
            gmb_p_adv(p);
        gmb_stmt_t* s = gfx_stmt(p, 46, 64, line);
        s->n_netargs = 0;
        s->value = gmb_p_parse_expr(p);
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
        if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
        for (;;) {
            const gmb_token_t* ct = gmb_p_cur(p);
            if (!ct || ct->kind == GMB_TOK_Eof) break;
            if (ct->kind == GMB_TOK_Newline) { gmb_p_adv(p); continue; }
            if (ct->kind == GMB_TOK_Spritedef) {
                gmb_p_adv(p);
                if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_End || gmb_p_tok_is_keyword(gmb_p_cur(p), "End")))
                    gmb_p_adv(p);
                break;
            }
            if (ct->kind == GMB_TOK_StringLiteral) {
                if (s->n_netargs < 64) s->net_args[s->n_netargs++] = gmb_p_parse_expr(p);
                else gmb_p_adv(p);
                continue;
            }
            break;
        }
        return s;
    }
    if (t->kind == GMB_TOK_Load) {
        gmb_p_adv(p);   /* LOAD */
        gmb_p_adv(p);   /* TEXTURE */
        gmb_stmt_t* s = gfx_stmt(p, 4, 3, line);
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_check(p, GMB_TOK_Into) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Into")) {
            gmb_p_adv(p);
            s->net_assign = net_target_name(p);
        }
        (void)gmb_p_match(p, GMB_TOK_Comma);   /* `INTO target, CELL(w,h)` — virgola opzionale */
        if (gmb_p_check(p, GMB_TOK_Cell) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Cell")) {
            gmb_p_adv(p);
            s->net_sub = 5;
            gmb_p_match(p, GMB_TOK_LeftParen);
            s->net_args[1] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            s->net_args[2] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        return s;
    }
    return NULL;
}
