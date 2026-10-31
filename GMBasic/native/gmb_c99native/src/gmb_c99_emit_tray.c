/* gmb_c99_emit_tray.c — namespace TRAY (icona nella barra del SO) nel C99Native.
 * §0quater: file di dominio, tabella dichiarativa. §0bis: l'implementazione vive SOLO
 * nella libreria del runtime condiviso (native/gmb_runtime_c/src/gmb_tray*.c); qui si
 * emette la chiamata. Predicati (SELFTEST/AVAILABLE) → convenzione -1 vero / 0 falso,
 * allineata a IL (IlRuntime.Tray.cs) e all'emitter C# (C99CodeEmitter.Builtin.TRAY.cs).
 */
#include "gmb_c99_emit.h"
#include "gmb_c99_emit_internal.h"

/* Argomento stringa i-esimo con `.data` (stringa nuda gmb_string_t). */
static void tray_s(ctx_t* c, gmb_expr_t** a, int n, int i) {
    if (i < n && a[i]) emit_str_operand(c, a[i]);
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
}

static int tray_num(ctx_t* c, gmb_expr_t** a, int n, const char* fn, int nargs, int has_str2) {
    sb_put(&c->out, "(gmb_double_t)");
    sb_put(&c->out, fn);
    sb_put(&c->out, "(");
    if (nargs >= 1) {
        if (has_str2) { tray_s(c, a, n, 0); sb_put(&c->out, ".data"); }
        else { sb_put(&c->out, "(gmb_int_t)("); if (n >= 1 && a[0]) emit_expr(c, a[0]); else sb_put(&c->out, "0"); sb_put(&c->out, ")"); }
    }
    if (nargs >= 2) { sb_put(&c->out, ", "); tray_s(c, a, n, 1); sb_put(&c->out, ".data"); }
    sb_put(&c->out, ")");
    return 1;
}

int emit_bf_tray(ctx_t* c, const char* name, const char* ns,
                 gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    c->uses_tray = 1;   /* prologo: #include "gmb_tray.h" */
    switch (tk) {
        case GMB_TOK_TrayCreate:   return tray_num(c, args, nargs, "gmb_tray_create", 2, 1);
        case GMB_TOK_TrayItem:     return tray_num(c, args, nargs, "gmb_tray_item", 2, 0);
        case GMB_TOK_TraySep:      sb_put(&c->out, "(gmb_double_t)gmb_tray_sep()"); return 1;
        case GMB_TOK_TrayTooltip:  return tray_num(c, args, nargs, "gmb_tray_tooltip", 1, 1);
        case GMB_TOK_TrayPoll:     sb_put(&c->out, "(gmb_double_t)gmb_tray_poll()"); return 1;
        case GMB_TOK_TrayRemove:   sb_put(&c->out, "(gmb_tray_remove(), 0.0)"); return 1;
        case GMB_TOK_TrayBackend:  sb_put(&c->out, "gmb_string_alloc(gmb_tray_backend())"); return 1;
        case GMB_TOK_TraySelfTest: sb_put(&c->out, "(gmb_tray_selftest() != 0 ? -1.0 : 0.0)"); return 1;
        case GMB_TOK_TrayAvailable:sb_put(&c->out, "(gmb_tray_available() != 0 ? -1.0 : 0.0)"); return 1;
        case GMB_TOK_TrayLast:     sb_put(&c->out, "(gmb_double_t)gmb_tray_last()"); return 1;
        default: return 0;
    }
}
