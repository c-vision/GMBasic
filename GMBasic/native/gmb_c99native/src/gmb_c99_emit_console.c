/* gmb_c99_emit_console.c — emitter C99 del namespace CONSOLE (§0quater file di dominio).
 * UN handler (emit_bf_console) per i builtin CONSOLE.* — dispatch per TokenKind (regola
 * d'oro). Ogni emissione chiama il runtime C condiviso (§0bis), nessuna logica inline.
 *
 * Coperti qui (deterministici, backing runtime già esistente):
 *   CONSOLE.FGRGB(text,r,g,b) / CONSOLE.BGRGB(text,r,g,b) → gmb_ansi_fg/bg
 *   (true-color \x1b[38;2;r;g;bm<text>\x1b[0m, parità IlRuntime.FgRgb/BgRgb).
 * Non coperti (richiedono runtime/semantica non ancora allineata): GETCURSORPOS,
 * GETTEXTMETRICS, GETPALETTECOLOR (emitter C# emette gmb_get* che non hanno backing
 * runtime), CELL/CELLSTR (stub ""), MESSAGE/ALERT/CONFIRM/INKEY (interattivi). */
#include "gmb_c99_emit_internal.h"

int emit_bf_console(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)line;
    /* CLIP.GET/SET/CLEAR (GET/SET/CLEAR sono Kind generici, nessun TokenKind dedicato →
     * dispatch per nome, regola d'oro §12.2). Clipboard via runtime condiviso
     * gmb_clipboard_* (stessa clipboard di IL/CGfxClipboardService e CLIP GET/SET/CLEAR
     * statement). Prima "CLIP.GET non definita". */
    if (ns && strcasecmp(ns, "CLIP") == 0 && name) {
        if (strcasecmp(name, "GET") == 0) { sb_put(&c->out, "gmb_clipboard_get()"); return 1; }
        if (strcasecmp(name, "SET") == 0 && nargs >= 1) {
            sb_put(&c->out, "gmb_clipboard_set(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data)");
            return 1;
        }
        if (strcasecmp(name, "CLEAR") == 0) { sb_put(&c->out, "gmb_clipboard_clear()"); return 1; }
    }
    if ((tk == GMB_TOK_FgRgb || tk == GMB_TOK_BgRgb) && nargs >= 4) {
        sb_printf(&c->out, "gmb_string_alloc(%s(", tk == GMB_TOK_FgRgb ? "gmb_ansi_fg" : "gmb_ansi_bg");
        emit_str_operand(c, args[0]); sb_put(&c->out, ".data, ");
        sb_put(&c->out, "(int)("); emit_expr(c, args[1]); sb_put(&c->out, "), ");
        sb_put(&c->out, "(int)("); emit_expr(c, args[2]); sb_put(&c->out, "), ");
        sb_put(&c->out, "(int)("); emit_expr(c, args[3]); sb_put(&c->out, ")))");
        return 1;
    }
    /* INKEY() → gmb_string_t (parità IlRuntime.Inkey headless = "") */
    if (tk == GMB_TOK_Inkey) {
        sb_put(&c->out, "gmb_console_inkey()");
        return 1;
    }
    /* MESSAGE(text?) → gmb_string_t (getter/setter, parità IlRuntime.MessageCmd) */
    if (tk == GMB_TOK_Message) {
        sb_put(&c->out, "gmb_console_message(");
        if (nargs >= 1) { emit_str_operand(c, args[0]); sb_put(&c->out, ".data"); }
        else sb_put(&c->out, "NULL");
        sb_put(&c->out, ")");
        return 1;
    }
    /* GETCURSORPOS/GETTEXTMETRICS/GETPALETTECOLOR → gmb_list_t */
    if (tk == GMB_TOK_GetCursorPos) { sb_put(&c->out, "gmb_console_get_cursor_pos()"); return 1; }
    if (tk == GMB_TOK_GetTextMetrics) { sb_put(&c->out, "gmb_console_get_text_metrics()"); return 1; }
    if (tk == GMB_TOK_GetPaletteColor && nargs >= 1) {
        sb_put(&c->out, "gmb_console_get_palette_color((int)(");
        emit_expr(c, args[0]);
        sb_put(&c->out, "))");
        return 1;
    }
    /* ALERT/CONFIRM → 0 headless senza render (parità IlRuntime headless). */
    if (tk == GMB_TOK_Alert || tk == GMB_TOK_Confirm) {
        sb_printf(&c->out, "%s(", tk == GMB_TOK_Alert ? "gmb_console_alert" : "gmb_console_confirm");
        if (nargs >= 1) { emit_str_operand(c, args[0]); sb_put(&c->out, ".data"); }
        else sb_put(&c->out, "NULL");
        sb_put(&c->out, ")");
        return 1;
    }
    return 0;
}
