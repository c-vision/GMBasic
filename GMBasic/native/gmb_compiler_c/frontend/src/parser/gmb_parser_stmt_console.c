/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_console.c — speculare a
 * Parser.Stmt.Console.cs: CLS, LOCATE, COLOR, INKEY, INPUT, SET <attr>. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

gmb_stmt_t* gmb_p_parse_cls(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CLS, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
        /* CLS [colore]: il colore NON si scarta (parità ClsStmt.Color / CLS_GPM):
         * in grafica riempie la pagina col colore, in console imposta lo sfondo. */
        s->value = gmb_p_parse_expr(p);
    }
    return s;
}

gmb_stmt_t* gmb_p_parse_locate(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_LOCATE, line);
    s->row = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Comma)) s->col = gmb_p_parse_expr(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_color(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_COLOR, line);
    s->fg = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Comma)) s->bg = gmb_p_parse_expr(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_inkey(gmb_parser_t* p, int line)
{
    /* INKEY [variabile$][, timeoutMs] — parity Parser.Stmt.Console.cs ParseInkeyStatement:
     * target L-value opzionale, timeout opzionale dopo virgola. */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_INKEY, line);
    const gmb_token_t* nm = gmb_p_cur(p);
    if (nm && nm->kind == GMB_TOK_Identifier) {
        s->target = gmb_p_tok_strdup(nm);
        gmb_p_adv(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->channel = gmb_p_parse_expr(p);
    }
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_getkey(gmb_parser_t* p, int line)
{
    /* GET [variabile$] — lettura bloccante di un tasto (parity Parser.Stmt.Console.cs
     * ParseConsoleGetStatement + GetKeyboardStmt). Target L-value opzionale (stile inkey:
     * senza target il tasto letto viene scartato); il chunk path (OP_SYSCALL 7) e st_getkey
     * gestiscono entrambi i casi. IO.GET (`GET #`) escluso dal dispatcher (vedi
     * gmb_parser_statements.c: C# DispatchGetStatement instrada `#` a IO.GET). */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GETKEY, line);
    const gmb_token_t* nm = gmb_p_cur(p);
    if (nm && nm->kind == GMB_TOK_Identifier) {
        s->target = gmb_p_tok_strdup(nm);
        gmb_p_adv(p);
    }
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_savescreen(gmb_parser_t* p, int line)
{
    /* SAVE SCREEN <nome> — snapshot nominato (parity SaveScreenStmt C#). Nome in on_expr. */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SAVESCREEN, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
        s->on_expr = gmb_p_parse_expr(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_restorescreen(gmb_parser_t* p, int line)
{
    /* RESTORE SCREEN <nome> — ripristina snapshot nominato (parity RestoreScreenStmt). */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_RESTORESCREEN, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
        s->on_expr = gmb_p_parse_expr(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_scrollmode(gmb_parser_t* p, int line)
{
    /* SCROLLMODE mode — regione scroll (0,25) se mode!=0 altrimenti (0,0); parity PCode
     * PopInt (CONSOLE_SCROLLMODE). Mode in on_expr. */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SCROLLMODE, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
        s->on_expr = gmb_p_parse_expr(p);
    return s;
}

static int at_kw(const gmb_token_t* t, const char* kw) {
    return t && t->lexeme && t->length == (int)strlen(kw) && strncasecmp(t->lexeme, kw, (size_t)t->length) == 0;
}

/* Appende una coppia (marker, valore) a control_args (marker = var expr col nome). */
static void atform_push(gmb_stmt_t* s, const char* marker, gmb_expr_t* value)
{
    s->control_args = (gmb_expr_t**)realloc(s->control_args, (size_t)(s->n_control_args + 2) * sizeof(gmb_expr_t*));
    s->control_args[s->n_control_args++] = gmb_expr_var(strdup(marker), s->line);
    s->control_args[s->n_control_args++] = value;
}

gmb_stmt_t* gmb_p_parse_atform(gmb_parser_t* p, int line)
{
    /* `@ r, c SAY <expr>` | `@ r, c GET <var> [TYPE/… SIZE/… PICTURE/… ITEMS/…
     * MIN/MAX/STEP/CAPTION/VALID]` — form Clipper in GRAFICA (pixel), governato da READ.
     * act_tk: 1=SAY, 2=GET. win_coords[0]=row,[1]=col,[2]=SIZE w,[3]=SIZE h.
     * channel=TYPE (str), on_expr=PICTURE (str), control_args=(marker,value)*.  */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ATFORM, line);
    s->win_coords[0] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);
    s->win_coords[1] = gmb_p_parse_expr(p);
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && at_kw(t, "Say")) {
        gmb_p_adv(p); s->act_tk = 1;
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->on_expr = gmb_p_parse_expr(p);
        /* clausole di formattazione SAY (PICTURE/...): consumate. PICTURE → channel (usato
         * dall'emitter per il formato; SAY senza PICTURE resta invariato). Senza, `PICTURE`
         * restava e il parser dava "statement non riconosciuto". */
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) {
            const gmb_token_t* c = gmb_p_cur(p);
            if (c && at_kw(c, "Picture")) { gmb_p_adv(p); if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->channel = gmb_p_parse_expr(p); }
            else gmb_p_adv(p);
        }
    } else if (t && at_kw(t, "Get")) {
        gmb_p_adv(p); s->act_tk = 2;
        const gmb_token_t* v = gmb_p_cur(p);
        if (v && v->kind == GMB_TOK_Identifier) {
            s->read_vars = (char**)malloc(sizeof(char*));
            s->read_vars[0] = (char*)malloc((size_t)v->length + 1);
            memcpy(s->read_vars[0], v->lexeme, (size_t)v->length); s->read_vars[0][v->length] = 0;
            s->n_read = 1; gmb_p_adv(p);
        }
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
            const gmb_token_t* c = gmb_p_cur(p);
            if (c && at_kw(c, "Type")) { gmb_p_adv(p); s->channel = gmb_p_parse_expr(p); }
            else if (c && at_kw(c, "Size")) { gmb_p_adv(p); s->win_coords[2] = gmb_p_parse_expr(p); if (gmb_p_match(p, GMB_TOK_Comma)) s->win_coords[3] = gmb_p_parse_expr(p); }
            else if (c && at_kw(c, "Picture")) { gmb_p_adv(p); s->on_expr = gmb_p_parse_expr(p); }
            else if (c && at_kw(c, "Items")) {
                gmb_p_adv(p);
                if (gmb_p_check(p, GMB_TOK_LeftBrace)) {
                    gmb_p_adv(p);
                    for (;;) {
                        if (gmb_p_check(p, GMB_TOK_RightBrace) || gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) break;
                        gmb_expr_t* it = gmb_p_parse_expr(p);
                        atform_push(s, "ITEMS", it);
                        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                    }
                    gmb_p_match(p, GMB_TOK_RightBrace);
                } else { gmb_expr_t* it = gmb_p_parse_expr(p); atform_push(s, "ITEMS", it); }
            }
            else if (c && at_kw(c, "Range")) {
                gmb_p_adv(p);
                atform_push(s, "MIN", gmb_p_parse_expr(p));
                if (gmb_p_match(p, GMB_TOK_Comma)) atform_push(s, "MAX", gmb_p_parse_expr(p));
            }
            else if (c && (at_kw(c, "Min") || at_kw(c, "Max") || at_kw(c, "Step"))) {
                const char* mk = at_kw(c, "Min") ? "MIN" : (at_kw(c, "Max") ? "MAX" : "STEP");
                gmb_p_adv(p); atform_push(s, mk, gmb_p_parse_expr(p));
            }
            else if (c && at_kw(c, "Caption")) { gmb_p_adv(p); atform_push(s, "CAPTION", gmb_p_parse_expr(p)); }
            else if (c && at_kw(c, "Valid")) { gmb_p_adv(p); atform_push(s, "VALID", gmb_p_parse_expr(p)); }
            else if (c && at_kw(c, "When")) { gmb_p_adv(p); atform_push(s, "WHEN", gmb_p_parse_expr(p)); }
            else if (c && at_kw(c, "Message")) { gmb_p_adv(p); atform_push(s, "MESSAGE", gmb_p_parse_expr(p)); }
            else if (c && at_kw(c, "Error")) { gmb_p_adv(p); atform_push(s, "ERROR", gmb_p_parse_expr(p)); }
            else gmb_p_adv(p);   /* clausola non gestita: salta il token */
        }
    } else {
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
    }
    return s;
}

gmb_stmt_t* gmb_p_parse_caption(gmb_parser_t* p, int line)
{
    /* CAPTION <expr> — titolo console/finestra (parity Parser.Stmt.Console.cs
     * ParseConsoleCaptionStatement). Titolo in on_expr (stesso riuso di SENDKEY). */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CAPTION, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
        s->on_expr = gmb_p_parse_expr(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_input(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_INPUT, line);
    /* INPUT ["prompt";] var [, var ...] */
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_StringLiteral) {
        s->input_prompt = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Semicolon);
    }
    int cap = 0;
    for (;;) {
        const gmb_token_t* v = gmb_p_cur(p);
        if (v && v->kind == GMB_TOK_Identifier) {
            if (s->n_read >= cap) { cap = cap ? cap * 2 : 4; s->read_vars = (char**)realloc(s->read_vars, (size_t)cap * sizeof(char*)); }
            s->read_vars[s->n_read] = (char*)malloc((size_t)v->length + 1);
            memcpy(s->read_vars[s->n_read], v->lexeme, (size_t)v->length);
            s->read_vars[s->n_read][v->length] = 0;
            s->n_read++;
            gmb_p_adv(p);
        }
        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
    }
    return s;
}

/* LINE INPUT ["prompt";] var$ [, timeout] — console (sintassi a due parole 'LINE INPUT').
 * Speculare a Parser.Stmt.Console.cs ParseLineInputStatement. Una sola variabile; il
 * timeout opzionale è accettato ma non propagato (parità col VM/C vm_console_5). */
gmb_stmt_t* gmb_p_parse_lineinput_console(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_LINEINPUT, line);
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_StringLiteral) {
        s->input_prompt = gmb_p_parse_expr(p);
        if (!gmb_p_match(p, GMB_TOK_Semicolon) && !gmb_p_match(p, GMB_TOK_Comma)) {
            /* prompt senza separatore: consuma fino a fine riga ed esci (riga malformata) */
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
    }
    const gmb_token_t* v = gmb_p_cur(p);
    if (v && v->kind == GMB_TOK_Identifier) {
        s->read_vars = (char**)malloc(sizeof(char*));
        s->read_vars[0] = (char*)malloc((size_t)v->length + 1);
        memcpy(s->read_vars[0], v->lexeme, (size_t)v->length);
        s->read_vars[0][v->length] = 0;
        s->n_read = 1;
        gmb_p_adv(p);
    }
    /* timeout opzionale: cattura e scarta (parità runtime, nessun timeout console) */
    if (gmb_p_match(p, GMB_TOK_Comma)) {
        gmb_expr_t* te = gmb_p_parse_expr(p);
        if (te) gmb_expr_free(te);
    }
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
    return s;
}

/* SET <attr> ON|OFF  |  SET THEME "name"  |  SET DATE FORMAT "fmt"  |  SET <attr> <value>
 * (speculare a Parser.Stmt.Console.cs ParseSetAttributeStatement + SET THEME/DATE FORMAT) */
gmb_stmt_t* gmb_p_parse_set(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SET, line);
    const gmb_token_t* a = gmb_p_cur(p);
    if (a) {
        s->set_attr = (char**)malloc(sizeof(char*));
        s->set_attr[0] = (char*)malloc((size_t)a->length + 1);
        memcpy(s->set_attr[0], a->lexeme, (size_t)a->length); s->set_attr[0][a->length] = 0;
        s->n_set_attr = 1;
        gmb_p_adv(p);
        /* SET <attr> ON|OFF */
        const gmb_token_t* n = gmb_p_cur(p);
        if (n && (gmb_p_tok_is_keyword(n, "On") || gmb_p_tok_is_keyword(n, "Off"))) {
            s->col = gmb_expr_var(gmb_p_tok_strdup(n), n->line);
            gmb_p_adv(p);
            return s;
        }
        /* SET THEME "name" / SET DATE FORMAT "fmt" — parola composta */
        if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "Date") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Format")
                          || gmb_p_tok_is_keyword(gmb_p_cur(p), "Theme"))) {
            gmb_p_adv(p);
        }
        /* valore */
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
    }
    return s;
}

/* SHELL "comando" — esegue un comando di sistema (runtime condiviso gmb_shell.c).
 * Il comando è l'espressione stringa dopo SHELL, salvata in s->col. */
gmb_stmt_t* gmb_p_parse_shell(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SHELL, line);
    if (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon))
        s->col = gmb_p_parse_expr(p);
    else
        s->col = NULL;
    return s;
}

/* SENDKEY <text>: invia testo al buffer typeahead (parità ConsoleSendKeyStmt C#). */
gmb_stmt_t* gmb_p_parse_sendkey(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SENDKEY, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
        s->on_expr = gmb_p_parse_expr(p);
    return s;
}

/* CLIP GET dest | CLIP SET expr | CLIP CLEAR (2026-09-21, gruppo Console, parità
 * 4 backend): clipboard via runtime condiviso gmb_clipboard_*. flag 0=GET (target),
 * 1=SET (value), 2=CLEAR. */
gmb_stmt_t* gmb_p_parse_clip(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CLIP, line);
    const gmb_token_t* sub = gmb_p_cur(p);
    if (sub && sub->kind == GMB_TOK_Get) {
        s->flag = 0;
        gmb_p_adv(p);
        const gmb_token_t* nm = gmb_p_cur(p);
        if (nm && nm->kind == GMB_TOK_Identifier) { s->target = gmb_p_tok_strdup(nm); gmb_p_adv(p); }
    } else if (sub && sub->kind == GMB_TOK_Set) {
        s->flag = 1;
        gmb_p_adv(p);
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
            s->value = gmb_p_parse_expr(p);
    } else if (sub && sub->kind == GMB_TOK_Clear) {
        s->flag = 2;
        gmb_p_adv(p);
    }
    return s;
}
