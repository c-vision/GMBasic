/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_controls.c — speculare a
 * Parser.Stmt.Controls.cs: widget TUI (CHECKBOX, LISTBOX, BUTTON, ...). Handler generico:
 * <NOME> <arg>, <arg>, ... fino a fine riga (gli argomenti sono espressioni/variabili). */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int ctl_is_kw(const gmb_token_t* t, const char* kw)
{
    size_t n = strlen(kw);
    return t && t->lexeme && t->length == (int)n && strncasecmp(t->lexeme, kw, n) == 0;
}
/* Eventi validi per le clausole inline attaccate ON<EVENTO> (ONCLICK, ONCHANGE, ...). */
static const char* ctl_event_names[] = {
    "CLICK","DBLCLICK","CHANGE","GOTFOCUS","LOSTFOCUS","MOUSEDOWN","MOUSEUP","MOUSEMOVE",
    "KEYDOWN","KEYPRESS","KEYUP","DRAGDROP","DRAGOVER","VALIDATE","SCROLL","DROPDOWN",
    "UPCLICK","DOWNCLICK","NODECLICK","EXPAND","COLLAPSE","SELCHANGE","ROWCOLCHANGE"
};
/* ONCLICK/ONCHANGE/... → nome evento (in out), 1 se match. */
static int ctl_attached_event(const gmb_token_t* t, char* out, size_t outsz)
{
    if (!t || !t->lexeme || t->length <= 2) return 0;
    char b[64]; int n = t->length; if (n > 63) n = 63; memcpy(b, t->lexeme, (size_t)n); b[n] = 0;
    if (strncasecmp(b, "ON", 2) != 0) return 0;
    for (size_t i = 0; i < sizeof(ctl_event_names)/sizeof(ctl_event_names[0]); i++)
        if (strcasecmp(b + 2, ctl_event_names[i]) == 0) { snprintf(out, outsz, "%s", ctl_event_names[i]); return 1; }
    return 0;
}

/* Azione di un evento di controllo dopo `ON <EV>` / `ONCLICK`:
 * GOSUB label | GOTO label | STOP | CALL sub() | sub() | sub (alias nudo).
 * Ritorna la stringa azione (strdup, da liberare): "sub" (CALL) | "" (STOP) |
 * "L:label" (GOSUB) | "G:label" (GOTO). NULL su errore. */
static char* ctl_parse_event_action(gmb_parser_t* p)
{
    char sub[160];
    if (ctl_is_kw(gmb_p_cur(p), "GOSUB") || ctl_is_kw(gmb_p_cur(p), "GOTO")) {
        int is_gosub = ctl_is_kw(gmb_p_cur(p), "GOSUB");
        gmb_p_adv(p);
        const gmb_token_t* L = gmb_p_cur(p);
        if (!L || !L->lexeme) return NULL;
        snprintf(sub, sizeof(sub), "%s:%.*s", is_gosub ? "L" : "G", (int)L->length, L->lexeme);
        gmb_p_adv(p);
    } else if (ctl_is_kw(gmb_p_cur(p), "STOP")) {
        gmb_p_adv(p); sub[0] = 0;
    } else {
        if (ctl_is_kw(gmb_p_cur(p), "CALL")) gmb_p_adv(p);
        const gmb_token_t* t = gmb_p_cur(p);
        if (!t || !t->lexeme) return NULL;
        snprintf(sub, sizeof(sub), "%.*s", (int)t->length, t->lexeme);
        gmb_p_adv(p);
        if (gmb_p_check(p, GMB_TOK_LeftParen)) {   /* consuma (...) bilanciate, se presenti */
            int depth = 0;
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
                int k = gmb_p_cur(p) ? gmb_p_cur(p)->kind : GMB_TOK_Newline;
                if (k == GMB_TOK_LeftParen) depth++;
                else if (k == GMB_TOK_RightParen) depth--;
                gmb_p_adv(p);
                if (depth <= 0) break;
            }
        }
    }
    return (char*)strdup(sub);
}

static void ctl_append_attr(gmb_stmt_t* s, const char* txt)
{
    if (!txt || !*txt) return;
    if (!s->set_attr) { s->set_attr = (char**)malloc(sizeof(char*)); s->set_attr[0] = (char*)strdup(txt); s->n_set_attr = 1; return; }
    size_t L = strlen(s->set_attr[0]), B = strlen(txt);
    char* n = (char*)malloc(L + B + 1);
    memcpy(n, s->set_attr[0], L); memcpy(n + L, txt, B + 1);
    free(s->set_attr[0]); s->set_attr[0] = n;
}

gmb_stmt_t* gmb_p_parse_control(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONTROL, line);
    const gmb_token_t* n = gmb_p_cur(p);
    if (n) {
        s->control_args = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*));
        s->control_args[0] = gmb_expr_var(gmb_p_tok_strdup(n), line);
        s->n_control_args = 1;
        gmb_p_adv(p);   /* consuma il nome del controllo */
        /* STYLE "spec" (override colori/stile del controllo) — opzionale, ovunque sulla riga. */
        const gmb_token_t* sc = gmb_p_cur(p);
        if (gmb_p_tok_is_soft_keyword(sc, "STYLE")) {
            gmb_p_adv(p);
            const gmb_token_t* stt = gmb_p_cur(p);
            if (stt && (stt->kind == GMB_TOK_StringLiteral || stt->kind == GMB_TOK_InterpolatedStringLiteral)) {
                char* spec = gmb_p_tok_strdup(stt);
                if (spec) { size_t L = strlen(spec); if (L >= 2 && spec[0] == '"' && spec[L-1] == '"') { memmove(spec, spec + 1, L - 2); spec[L-2] = 0; } }
                s->set_attr = (char**)malloc(sizeof(char*)); s->set_attr[0] = spec ? spec : (char*)""; s->n_set_attr = 1;
                gmb_p_adv(p);
            }
        }
        /* argomenti: <expr> [, <expr> ...] fino a fine riga (o fino a INTO). */
        int cap = 1;
        int _at_into = gmb_p_tok_is_keyword(gmb_p_cur(p), "Into") || gmb_p_check(p, GMB_TOK_Into);
        if (!_at_into && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
            for (;;) {
                if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p) ||
                    gmb_p_tok_is_keyword(gmb_p_cur(p), "Into") || gmb_p_check(p, GMB_TOK_Into)) break;
                /* Sintassi VERBOSA: AT x,y | SIZE w,h | TEXT e | VALUE e | MIN e | MAX e |
                 * STEP e | CAPTION e → normalizza in posizionale (AT/SIZE = coppia). */
                int is_pair = gmb_p_check(p, GMB_TOK_At) || gmb_p_check(p, GMB_TOK_Size);
                int is_key = is_pair || gmb_p_check(p, GMB_TOK_Text) || gmb_p_check(p, GMB_TOK_Value) ||
                             gmb_p_check(p, GMB_TOK_Min) || gmb_p_check(p, GMB_TOK_Max) ||
                             gmb_p_check(p, GMB_TOK_Step) || gmb_p_check(p, GMB_TOK_Caption) ||
                             gmb_p_check(p, GMB_TOK_Width) || gmb_p_check(p, GMB_TOK_Height) ||
                             gmb_p_check(p, GMB_TOK_Items) || gmb_p_check(p, GMB_TOK_Title) ||
                             gmb_p_check(p, GMB_TOK_Mask);
                if (is_key) {
                    gmb_p_adv(p);
                    int nk = is_pair ? 2 : 1;
                    for (int j = 0; j < nk; j++) {
                        if (s->n_control_args >= cap) { cap = cap ? cap * 2 : 4; s->control_args = (gmb_expr_t**)realloc(s->control_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                        s->control_args[s->n_control_args++] = gmb_p_parse_expr(p);
                        if (j == 0) gmb_p_match(p, GMB_TOK_Comma);
                    }
                    gmb_p_match(p, GMB_TOK_Comma);
                    continue;
                }
                if (s->n_control_args >= cap) { cap = cap ? cap * 2 : 4; s->control_args = (gmb_expr_t**)realloc(s->control_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                s->control_args[s->n_control_args++] = gmb_p_parse_expr(p);
                if (gmb_p_match(p, GMB_TOK_Comma)) continue;
                break;
            }
        }
        /* INTO <var> (es. TOGGLE ... INTO t, MENU ... INTO scelta) — PUO' essere
         * MULTI-TARGET: PRINTERDIALOG INTO stampante$, copie, duplex (parità con
         * Parser.Stmt.Controls.cs ParsePrinterDialogStandalone). */
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Into") || gmb_p_check(p, GMB_TOK_Into)) {
            gmb_p_adv(p);
            if (!s->control_into) s->control_into = s->n_control_args;
            for (;;) {
                if (s->n_control_args >= cap) { cap = cap ? cap * 2 : 4; s->control_args = (gmb_expr_t**)realloc(s->control_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                s->control_args[s->n_control_args++] = gmb_p_parse_expr(p);
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) break;
            }
        }
    }
    /* Clausole ripetibili: ON <EVENT> <sub> | NAME/TAG/TOOLTIP "v" |
     * VISIBLE/ENABLED/TABINDEX/TABSTOP n | FOCUS | STYLE "spec".
     * Le proprieta' base vengono FUSE nella stessa spec CSS-like (set_attr[0]),
     * cosi' l'emitter (che gia' emette gmb_form_set_last_style) non cambia. */
    for (;;) {
        const gmb_token_t* t = gmb_p_cur(p);
        if (!t || !t->lexeme) break;
        char aev[32];
        if (ctl_is_kw(t, "ON") || ctl_attached_event(t, aev, sizeof(aev))) {
            char evn[32];
            if (ctl_is_kw(t, "ON")) {
                gmb_p_adv(p);
                const gmb_token_t* ev = gmb_p_cur(p); if (!ev || !ev->lexeme) break;
                snprintf(evn, sizeof(evn), "%.*s", (int)ev->length, ev->lexeme); gmb_p_adv(p);
            } else { snprintf(evn, sizeof(evn), "%s", aev); gmb_p_adv(p); }
            char* act = ctl_parse_event_action(p); if (!act) break;
            { char* q = evn; for (; *q; q++) *q = (char)toupper((unsigned char)*q); }
            s->ev_pairs = (char**)realloc(s->ev_pairs, (size_t)(s->n_ev + 2) * sizeof(char*));
            s->ev_pairs[s->n_ev++] = (char*)strdup(evn);
            s->ev_pairs[s->n_ev++] = act;
            continue;
        }
        if (ctl_is_kw(t, "NAME") || ctl_is_kw(t, "TAG") || ctl_is_kw(t, "TOOLTIP")) {
            char key[16]; snprintf(key, sizeof(key), "%.*s", (int)t->length, t->lexeme);
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) {
                char raw[192]; snprintf(raw, sizeof(raw), "%.*s", (int)v->length, v->lexeme);
                char* vp = raw; size_t L = strlen(vp);
                if (L >= 2 && vp[0] == '"' && vp[L-1] == '"') { vp[L-1] = 0; vp++; }
                char add[256]; snprintf(add, sizeof(add), " %s=\"%s\"", key, vp);
                ctl_append_attr(s, add);
                gmb_p_adv(p);
            }
            continue;
        }
        if (ctl_is_kw(t, "VISIBLE") || ctl_is_kw(t, "ENABLED") || ctl_is_kw(t, "TABINDEX") || ctl_is_kw(t, "TABSTOP")) {
            char key[16]; snprintf(key, sizeof(key), "%.*s", (int)t->length, t->lexeme);
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) { char add[256]; snprintf(add, sizeof(add), " %s=%.*s", key, (int)v->length, v->lexeme); ctl_append_attr(s, add); gmb_p_adv(p); }
            continue;
        }
        if (ctl_is_kw(t, "FOCUS")) { ctl_append_attr(s, " focus"); gmb_p_adv(p); continue; }
        /* Clausole COLORE del controllo (parità Parser.Stmt.Controls.cs ParseControlProps):
         * COLOR/COLORFOCUS/COLORSEL/COLORERROR "spec" → fuse nella spec di stile, che
         * l'emitter applica con gmb_form_set_last_style (il runtime mappa
         * color=/focuscolor=/selcolor=/errorcolor= sulla style del controllo). Solo con
         * LETTERALE stringa: una espressione resta allo statement COLOR storico (che
         * imposta il colore globale), così nessun programma già valido cambia significato. */
        if (ctl_is_kw(t, "COLOR") || ctl_is_kw(t, "COLORFOCUS") || ctl_is_kw(t, "COLORSEL") || ctl_is_kw(t, "COLORERROR")) {
            const gmb_token_t* nx = gmb_p_peek(p, 1);
            if (nx && (nx->kind == GMB_TOK_StringLiteral || nx->kind == GMB_TOK_InterpolatedStringLiteral)) {
                const char* key = ctl_is_kw(t, "COLOR") ? "color"
                                : ctl_is_kw(t, "COLORFOCUS") ? "focuscolor"
                                : ctl_is_kw(t, "COLORSEL") ? "selcolor" : "errorcolor";
                gmb_p_adv(p);
                const gmb_token_t* v = gmb_p_cur(p);
                if (v && v->lexeme) {
                    char raw[192]; snprintf(raw, sizeof(raw), "%.*s", (int)v->length, v->lexeme);
                    char* vp = raw; size_t L = strlen(vp);
                    if (L >= 2 && vp[0] == '"' && vp[L-1] == '"') { vp[L-1] = 0; vp++; }
                    char add[256]; snprintf(add, sizeof(add), " %s=%s", key, vp);
                    ctl_append_attr(s, add);
                    gmb_p_adv(p);
                }
                continue;
            }
        }
        if (ctl_is_kw(t, "UPPERCASE")) { ctl_append_attr(s, " uppercase"); gmb_p_adv(p); continue; }
        if (ctl_is_kw(t, "ID")) {
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) { s->id_var = gmb_p_tok_strdup(v); gmb_p_adv(p); }
            continue;
        }
        if (ctl_is_kw(t, "STYLE")) {
            gmb_p_adv(p);
            const gmb_token_t* stt = gmb_p_cur(p);
            if (stt && (stt->kind == GMB_TOK_StringLiteral || stt->kind == GMB_TOK_InterpolatedStringLiteral)) {
                char* spec = gmb_p_tok_strdup(stt);
                if (spec) { size_t L = strlen(spec); if (L >= 2 && spec[0] == '"' && spec[L-1] == '"') { memmove(spec, spec + 1, L - 2); spec[L-2] = 0; } }
                char add[640]; snprintf(add, sizeof(add), " %s", spec ? spec : "");
                ctl_append_attr(s, add);
                free(spec);
                gmb_p_adv(p);
            }
            continue;
        }
        if (ctl_is_kw(t, "USE")) {
            /* Spazio coordinate DICHIARATO dal controllo: USE CELLS → metric=1
             * (CellsToPixel, solo su schermo grafico), USE PIXELS → metric=2
             * (PixelToCells, solo su TUI a celle). Fuso nella spec come le altre
             * proprietà base, così l'emitter non cambia. */
            gmb_p_adv(p);
            if (ctl_is_kw(gmb_p_cur(p), "CELLS"))  { ctl_append_attr(s, " metric=1"); gmb_p_adv(p); continue; }
            if (ctl_is_kw(gmb_p_cur(p), "PIXELS")) { ctl_append_attr(s, " metric=2"); gmb_p_adv(p); continue; }
            gmb_p_set_err(p, "USE nel controllo: atteso CELLS o PIXELS", line);
            break;
        }
        break;
    }
    return s;
}

/* BUTTON x, y, w, h, "label" ON CLICK <sub> [STYLE "spec"]  →  GMB_STMT_CONTROL con:
 *   [0]="BUTTON" [1]=x [2]=y [3]=w [4]=h [5]=label [6]=<sub> [7]=spec (str, opz.). */
gmb_stmt_t* gmb_p_parse_button(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONTROL, line);
    s->control_args = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*) * 8);
    s->n_control_args = 8;
    for (int i = 0; i < 8; i++) s->control_args[i] = NULL;

    const gmb_token_t* n = gmb_p_cur(p);
    s->control_args[0] = gmb_expr_var(gmb_p_tok_strdup(n), line);
    gmb_p_adv(p);                       /* BUTTON */

    s->control_args[1] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* x */
    s->control_args[2] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* y */
    s->control_args[3] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* w */
    s->control_args[4] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* h */
    s->control_args[5] = gmb_p_parse_expr(p);                                  /* label */

    /* Clausole (come i widget): ON CLICK <sub> | NAME/TAG/TOOLTIP "v" |
     * VISIBLE/ENABLED/TABINDEX/TABSTOP n | FOCUS | STYLE "spec".
     * Callback in control_args[6]; stile+proprietà base fusi in [7]. */
    char spec[512]; spec[0] = 0;
    for (;;) {
        const gmb_token_t* t = gmb_p_cur(p);
        if (!t || !t->lexeme) break;
        char aev[32];
        if (ctl_is_kw(t, "ON") || ctl_attached_event(t, aev, sizeof(aev))) {
            if (ctl_is_kw(t, "ON")) {
                gmb_p_adv(p);
                const gmb_token_t* ev = gmb_p_cur(p); if (!ev || !ev->lexeme) break;
                gmb_p_adv(p);                   /* evento */
            } else { gmb_p_adv(p); }            /* ONCLICK/ONDBLCLICK/... */
            char* act = ctl_parse_event_action(p); if (!act) break;
            const char* nm = ((act[0] == 'L' || act[0] == 'G') && act[1] == ':') ? act + 2 : act;
            s->control_args[6] = (*act) ? gmb_expr_var((char*)strdup(nm), line) : NULL;
            free(act);
            continue;
        }
        if (ctl_is_kw(t, "NAME") || ctl_is_kw(t, "TAG") || ctl_is_kw(t, "TOOLTIP")) {
            char key[16]; snprintf(key, sizeof(key), "%.*s", (int)t->length, t->lexeme);
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) {
                char raw[192]; snprintf(raw, sizeof(raw), "%.*s", (int)v->length, v->lexeme);
                char* vp = raw; size_t L = strlen(vp);
                if (L >= 2 && vp[0] == '"' && vp[L-1] == '"') { vp[L-1] = 0; vp++; }
                size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " %s=\"%s\"", key, vp);
                gmb_p_adv(p);
            }
            continue;
        }
        if (ctl_is_kw(t, "VISIBLE") || ctl_is_kw(t, "ENABLED") || ctl_is_kw(t, "TABINDEX") || ctl_is_kw(t, "TABSTOP")) {
            char key[16]; snprintf(key, sizeof(key), "%.*s", (int)t->length, t->lexeme);
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) { size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " %s=%.*s", key, (int)v->length, v->lexeme); gmb_p_adv(p); }
            continue;
        }
        if (ctl_is_kw(t, "FOCUS")) { size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " focus"); gmb_p_adv(p); continue; }
        if (ctl_is_kw(t, "ID")) {
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) { s->id_var = gmb_p_tok_strdup(v); gmb_p_adv(p); }
            continue;
        }
        if (ctl_is_kw(t, "USE")) {
            /* Spazio coordinate DICHIARATO dal controllo (vedi gmb_p_parse_control). */
            gmb_p_adv(p);
            if (ctl_is_kw(gmb_p_cur(p), "CELLS"))  { size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " metric=1"); gmb_p_adv(p); continue; }
            if (ctl_is_kw(gmb_p_cur(p), "PIXELS")) { size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " metric=2"); gmb_p_adv(p); continue; }
            gmb_p_set_err(p, "USE nel controllo: atteso CELLS o PIXELS", line);
            break;
        }
        if (ctl_is_kw(t, "STYLE")) {
            gmb_p_adv(p);
            const gmb_token_t* stt = gmb_p_cur(p);
            if (stt && (stt->kind == GMB_TOK_StringLiteral || stt->kind == GMB_TOK_InterpolatedStringLiteral)) {
                char* sp = gmb_p_tok_strdup(stt);
                if (sp) { size_t L = strlen(sp); if (L >= 2 && sp[0] == '"' && sp[L-1] == '"') { memmove(sp, sp + 1, L - 2); sp[L-2] = 0; } }
                size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " %s", sp ? sp : "");
                free(sp);
                gmb_p_adv(p);
            }
            continue;
        }
        break;
    }
    if (spec[0]) {
        const char* trimmed = (spec[0] == ' ') ? spec + 1 : spec;
        s->control_args[7] = gmb_expr_str(strdup(trimmed), line);
    }
    /* consuma eventuali token residui fino a fine riga */
    while (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline
           && gmb_p_cur(p)->kind != GMB_TOK_Colon && gmb_p_cur(p)->kind != GMB_TOK_Eof)
        gmb_p_adv(p);
    return s;
}

/* CONTROL SET "nome","prop",val[,str$] | CONTROL GET "nome","prop" INTO var |
 * CONTROL GETS "nome","prop" INTO var$ | CONTROL MOVE "n",x,y[,w,h] |
 * CONTROL FOCUS "n" | CONTROL ZORDER "n",front.
 * Due token (CONTROL <SUB>), convenzione GMBasic. control_args: [0]="CONTROL" [1]=<SUB> ... */
/* FIRE EVENT <handle>, "<EVENTO>" — comando a due token: innalza un evento controllo. */
gmb_stmt_t* gmb_p_parse_fire(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONTROL, line);
    s->control_args = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*) * 4);
    s->n_control_args = 0;
    s->control_args[s->n_control_args++] = gmb_expr_var((char*)strdup("FIRE"), line);
    gmb_p_adv(p);   /* FIRE */
    const gmb_token_t* ev = gmb_p_cur(p);
    if (!gmb_p_tok_is_soft_keyword(ev, "EVENT")) { gmb_p_set_err(p, "Atteso EVENT dopo FIRE", line); return s; }
    s->control_args[s->n_control_args++] = gmb_expr_var(gmb_p_tok_strdup(ev), line);
    gmb_p_adv(p);   /* EVENT */
    s->control_args[s->n_control_args++] = gmb_p_parse_expr(p);
    gmb_p_match(p, GMB_TOK_Comma);
    s->control_args[s->n_control_args++] = gmb_p_parse_expr(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_control_method(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONTROL, line);
    int cap = 8;
    s->control_args = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*) * (size_t)cap);
    s->n_control_args = 0;
    s->control_args[s->n_control_args++] = gmb_expr_var(strdup("CONTROL"), line);
    gmb_p_adv(p);   /* CONTROL */
    const gmb_token_t* sub = gmb_p_cur(p);
    if (sub && sub->lexeme) { s->control_args[s->n_control_args++] = gmb_expr_var(gmb_p_tok_strdup(sub), line); gmb_p_adv(p); }

    int _at_into = gmb_p_tok_is_keyword(gmb_p_cur(p), "Into") || gmb_p_check(p, GMB_TOK_Into);
    int _parent_seen = 0;
    if (!_at_into && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
        for (;;) {
            /* CONTROL ADD instance AT x,y SIZE w,h [PARENT container]: AT/SIZE/PARENT sono keyword. */
            if (gmb_p_check(p, GMB_TOK_Parent)) { _parent_seen = 1; gmb_p_adv(p); continue; }
            if (gmb_p_check(p, GMB_TOK_At) || gmb_p_check(p, GMB_TOK_Size)) { gmb_p_adv(p); continue; }
            if (s->n_control_args >= cap) { cap *= 2; s->control_args = (gmb_expr_t**)realloc(s->control_args, sizeof(gmb_expr_t*) * (size_t)cap); }
            s->control_args[s->n_control_args++] = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) continue;
            if (gmb_p_check(p, GMB_TOK_At) || gmb_p_check(p, GMB_TOK_Size) || gmb_p_check(p, GMB_TOK_Parent)) continue;
            break;
        }
    }
    s->flag = _parent_seen;   /* marca PARENT: distingue [7]=parent da [7]=INTO */
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Into") || gmb_p_check(p, GMB_TOK_Into)) {
        gmb_p_adv(p);
        s->on_expr = gmb_p_parse_expr(p);   /* INTO: target del handle */
        /* L'emitter C99 legge il target INTO come ULTIMO control_arg (GET/GETS/ADD):
         * lo si accoda anche qui (on_expr resta per il resto della pipeline). */
        if (s->on_expr) {
            if (s->n_control_args >= cap) { cap *= 2; s->control_args = (gmb_expr_t**)realloc(s->control_args, sizeof(gmb_expr_t*) * (size_t)cap); }
            s->control_args[s->n_control_args++] = s->on_expr;
        }
    }
    return s;
}

/* PANEL x, y, w, h, "titolo" [STYLE "spec"] — card di sezione (CONTROL args come BUTTON). */
gmb_stmt_t* gmb_p_parse_panel(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONTROL, line);
    s->control_args = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*) * 8);
    s->n_control_args = 8;
    for (int i = 0; i < 8; i++) s->control_args[i] = NULL;
    s->control_args[0] = gmb_expr_var(strdup("PANEL"), line);
    gmb_p_adv(p);                                   /* PANEL */
    s->control_args[1] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* x */
    s->control_args[2] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* y */
    s->control_args[3] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* w */
    s->control_args[4] = gmb_p_parse_expr(p); gmb_p_match(p, GMB_TOK_Comma);   /* h */
    s->control_args[5] = gmb_p_parse_expr(p);                                  /* titolo */
    /* Clausole: STYLE "spec", USE CELLS|PIXELS e le proprietà base (NAME/TAG/TOOLTIP/
     * VISIBLE/ENABLED/TABINDEX/TABSTOP/FOCUS). Prima PANEL accettava solo STYLE: la
     * clausola NAME veniva scartata e `CONTROL SET "<nome>"` non trovava il pannello. */
    char spec[512]; spec[0] = 0;
    for (;;) {
        const gmb_token_t* t = gmb_p_cur(p);
        if (!t || !t->lexeme) break;
        if (ctl_is_kw(t, "STYLE")) {
            gmb_p_adv(p);
            const gmb_token_t* stt = gmb_p_cur(p);
            if (stt && (stt->kind == GMB_TOK_StringLiteral || stt->kind == GMB_TOK_InterpolatedStringLiteral)) {
                char* sp = gmb_p_tok_strdup(stt);
                if (sp) {
                    size_t L = strlen(sp);
                    if (L >= 2 && sp[0] == '"' && sp[L-1] == '"') { memmove(sp, sp + 1, L - 2); sp[L-2] = 0; }
                    size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, "%s%s", Ls ? " " : "", sp);
                    free(sp);
                    gmb_p_adv(p);
                }
            }
            continue;
        }
        if (ctl_is_kw(t, "USE")) {
            gmb_p_adv(p);
            const char* met = ctl_is_kw(gmb_p_cur(p), "CELLS") ? "metric=1"
                            : ctl_is_kw(gmb_p_cur(p), "PIXELS") ? "metric=2" : NULL;
            if (!met) { gmb_p_set_err(p, "USE nel controllo: atteso CELLS o PIXELS", line); break; }
            gmb_p_adv(p);
            size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, "%s%s", Ls ? " " : "", met);
            continue;
        }
        if (ctl_is_kw(t, "NAME") || ctl_is_kw(t, "TAG") || ctl_is_kw(t, "TOOLTIP")) {
            char key[16]; snprintf(key, sizeof(key), "%.*s", (int)t->length, t->lexeme);
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) {
                char raw[192]; snprintf(raw, sizeof(raw), "%.*s", (int)v->length, v->lexeme);
                char* vp = raw; size_t L = strlen(vp);
                if (L >= 2 && vp[0] == '"' && vp[L-1] == '"') { vp[L-1] = 0; vp++; }
                size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " %s=\"%s\"", key, vp);
                gmb_p_adv(p);
            }
            continue;
        }
        if (ctl_is_kw(t, "VISIBLE") || ctl_is_kw(t, "ENABLED") || ctl_is_kw(t, "TABINDEX") || ctl_is_kw(t, "TABSTOP") || ctl_is_kw(t, "FONT")) {
            char key[16]; snprintf(key, sizeof(key), "%.*s", (int)t->length, t->lexeme);
            gmb_p_adv(p);
            const gmb_token_t* v = gmb_p_cur(p);
            if (v && v->lexeme) { size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " %s=%.*s", key, (int)v->length, v->lexeme); gmb_p_adv(p); }
            continue;
        }
        if (ctl_is_kw(t, "FOCUS")) { size_t Ls = strlen(spec); snprintf(spec + Ls, sizeof(spec) - Ls, " focus"); gmb_p_adv(p); continue; }
        break;
    }
    if (spec[0]) {
        s->control_args[7] = gmb_expr_str(strdup(spec), line);
        /* Stessa spec in set_attr: è il canale che l'emitter applica a OGNI controllo. */
        s->set_attr = (char**)malloc(sizeof(char*));
        s->set_attr[0] = strdup(spec);
        s->n_set_attr = 1;
    }
    while (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline
           && gmb_p_cur(p)->kind != GMB_TOK_Colon && gmb_p_cur(p)->kind != GMB_TOK_Eof)
        gmb_p_adv(p);
    return s;
}
