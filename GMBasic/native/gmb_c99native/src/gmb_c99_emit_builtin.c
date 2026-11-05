/* gmb_c99_emit_builtin.c — handler_fn per dominio dell'emitter C99 nativo (task 6.1.14/6.1.5).
 * Le 27 handler_fn emit_bf_<dominio>() estratte dal dispatcher monolitico (delegazione
 * §0quater): ogni dominio ha la sua funzione; il dispatcher (gmb_c99_emit.c) le dispaccia
 * via tabella lookup gmb_bf_domains[]. Le funzioni di supporto condivise (sb_put, emit_expr,
 * etype, lookup_var, ...) vivono in gmb_c99_emit.c e sono esposte non-static (prototipi in
 * gmb_c99_emit_internal.h). */
#include "gmb_c99_emit_internal.h"
#include "gmb_ambiguous.h"
#include "gmb_members.h"
typedef int (*ma_sp)(ctx_t*, gmb_expr_t**, int);
/* TY_VAL non è nell'enum condiviso (gmb_c99_emit_internal.h definisce solo TY_NUM/TY_STR):
 * il valore coincide con l'enum di gmb_c99_emit.c { TY_NUM=0, TY_STR=1, TY_VAL=2 }. */
#define BF_TY_VAL 2

int emit_bf_finance_scalar(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
int emit_bf_finance_cashflow(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line);
/* F3 multimodale (2026-09-11): coercizione numeri nei builtin AI (def. sotto, usata anche sopra). */
static void emit_num_or_zero(ctx_t* c, gmb_expr_t* e);
/* CONVERT.* Markdown AST list-returning (def. sotto, usata da bd_ubound). */
static int cv_is_md_list(gmb_tokenkind_t tk);

/* ── dominio: strings (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } st_row;
static int st_left(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_str_left("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", (gmb_int_t)("); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int st_right(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_str_right("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", (gmb_int_t)("); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int st_mid(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2 && n != 3) return 0;
    sb_put(&c->out, "gmb_str_mid("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", (gmb_int_t)("); emit_expr(c, a[1]); sb_put(&c->out, "), ");
    if (n == 3) { sb_put(&c->out, "(gmb_int_t)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "(gmb_int_t)2147483647");   /* 2-arg: len grande -> clamp ad avail (resto); len<=0 esplicito -> "" (canone C#) */
    sb_put(&c->out, ")");
    return 1;
}
static int st_chr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "({ char __cb[2] = { (char)((gmb_int_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")), 0 }; gmb_string_alloc(__cb); })");
    return 1;
}
static int st_asc(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "({ gmb_string_t __as = ");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, "; (__as.data && __as.length > 0 ? (gmb_double_t)(unsigned char)__as.data[0] : 0); })");
    return 1;
}
static int st_str(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    int _t = etype(c, a[0]);
    if (_t == TY_STR) emit_expr(c, a[0]);
    /* Campo nudo DBF/RDD (TY_VAL): STR$ è per definizione numerico → coercizione a
     * double e gmb_str_basic (spazio-segno, G15). Con gmb_cstr_val un campo CURRENCY
     * rendeva "100.00" (scale) invece di " 100" — divergenza dal canone PCode/VM-C. */
    else if (_t == BF_TY_VAL) { sb_put(&c->out, "gmb_str_basic(gmb_val_to_double("); emit_expr(c, a[0]); sb_put(&c->out, "))"); }
    else { sb_put(&c->out, "gmb_str_basic("); emit_num_operand(c, a[0]); sb_put(&c->out, ")"); }
    return 1;
}
static int st_val(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_string_to_double("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int st_upper(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_str_upper("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int st_lower(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_str_lower("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int st_trim(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_str_trim("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int st_ltrim(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_str_ltrim("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int st_rtrim(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_str_rtrim("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int st_space(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_str_repeat(gmb_string_alloc(\" \"), (gmb_int_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), gmb_string_alloc(\"\"))");
    return 1;
}
static int st_repeat(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2 && n != 3) return 0;
    sb_put(&c->out, "gmb_str_repeat("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", ");
    if (n == 3) emit_str_operand(c, a[2]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ")");
    return 1;
}
static int st_pad(ctx_t* c, gmb_expr_t** a, int n, char pad) {
    /* Forma Clipper PADL/PADR/PADC(exp, n [, fill]): fill omesso → spazio
     * (parità IL/C#; prima n!=3 falliva → "simbolo non trovato" sul 2-arg). */
    if (n != 2 && n != 3) return 0;
    sb_printf(&c->out, "gmb_string_pad%c(", pad);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ", (gmb_int_t)("); emit_expr(c, a[1]); sb_put(&c->out, "), ");
    if (n >= 3) emit_str_operand(c, a[2]); else sb_put(&c->out, "gmb_string_alloc(\" \")");
    sb_put(&c->out, ")");
    return 1;
}
static int st_padl(ctx_t* c, gmb_expr_t** a, int n) { return st_pad(c, a, n, 'l'); }
static int st_padr(ctx_t* c, gmb_expr_t** a, int n) { return st_pad(c, a, n, 'r'); }
static int st_padc(ctx_t* c, gmb_expr_t** a, int n) { return st_pad(c, a, n, 'c'); }
static int st_instr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    /* parità C#: ordine BASIC (s$, sub$[, start]); variante Clipper col primo
     * argomento NUMERICO → riordina (s, sub, start) */
    sb_put(&c->out, "gmb_str_instr(");
    if (n >= 3 && etype(c, a[0]) == TY_STR) {
        emit_str_operand(c, a[0]); sb_put(&c->out, ", ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ", (gmb_int_t)(");
        emit_expr(c, a[2]); sb_put(&c->out, ")");
    } else if (n >= 3) {
        emit_str_operand(c, a[1]); sb_put(&c->out, ", ");
        emit_str_operand(c, a[2]); sb_put(&c->out, ", (gmb_int_t)(");
        emit_expr(c, a[0]); sb_put(&c->out, ")");
    } else {
        emit_str_operand(c, a[0]); sb_put(&c->out, ", ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ", 1");
    }
    sb_put(&c->out, ")");
    return 1;
}
static int st_contains(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_str_contains("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static int st_startswith(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_str_startswith("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static int st_endswith(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_str_endswith("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static const st_row ST_TBL[] = {
    { GMB_TOK_Left, st_left }, { GMB_TOK_Right, st_right }, { GMB_TOK_Mid, st_mid },
    { GMB_TOK_Chr, st_chr }, { GMB_TOK_Asc, st_asc }, { GMB_TOK_Str, st_str },
    { GMB_TOK_Val, st_val },
    { GMB_TOK_Upper, st_upper },
    { GMB_TOK_Lower, st_lower },
    { GMB_TOK_Trim, st_trim }, { GMB_TOK_LTrim, st_ltrim }, { GMB_TOK_RTrim, st_rtrim },
    { GMB_TOK_Space, st_space }, { GMB_TOK_RepeatFunc, st_repeat },
    { GMB_TOK_PadL, st_padl }, { GMB_TOK_PadR, st_padr }, { GMB_TOK_PadC, st_padc },
    { GMB_TOK_Instr, st_instr }, { GMB_TOK_Contains, st_contains },
    { GMB_TOK_StartsWith, st_startswith }, { GMB_TOK_EndsWith, st_endswith },
};
int emit_bf_strings(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(ST_TBL) / sizeof(ST_TBL[0]); i++)
        if (ST_TBL[i].tk == tk) return ST_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: strings_extra (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } sx_row;
static int sx_bin(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_bin((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int sx_hex(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_hex((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int sx_oct(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_oct((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int sx_camel(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_camel("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_kebab(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_kebab("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_pascal(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_pascal("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_snake(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_snake("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_sentence(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_sentence("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_title(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_title("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_petscii(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_petscii("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_empty(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (etype(c, a[0]) == TY_STR) { sb_put(&c->out, "(gmb_str_isblank("); emit_str_operand(c, a[0]); sb_put(&c->out, ") ? -1 : 0)"); return 1; }
    sb_put(&c->out, "(("); emit_expr(c, a[0]); sb_put(&c->out, ") == 0 ? -1 : 0)");
    return 1;
}
static int sx_count(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_str_count("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ")");
    return 1;
}
static int sx_replace(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_str_replace("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[2]); sb_put(&c->out, ")");
    return 1;
}
static int sx_search(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_str_search("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static int sx_tab(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_tab((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int sx_dup(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (n >= 2 && etype(c, a[1]) == TY_STR) {
        /* DUP(n, "─") ripete il PRIMO CARATTERE (multibyte), non il primo BYTE:
         * prima usciva una sequenza UTF-8 non valida (box disallineati su C99Native). */
        sb_put(&c->out, "gmb_dup_str((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "), ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
        return 1;
    }
    sb_put(&c->out, "gmb_dup_cp((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "), ");
    if (n >= 2) {
        sb_put(&c->out, "(int)(gmb_int_t)("); emit_expr(c, a[1]); sb_put(&c->out, ")");
    } else sb_put(&c->out, "32");
    sb_put(&c->out, ")");
    return 1;
}
static int sx_wrap(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_str_wrap("); emit_str_operand(c, a[0]);
    sb_put(&c->out, ", (gmb_int_t)("); emit_expr(c, a[1]); sb_put(&c->out, "), ");
    if (n >= 3) { sb_put(&c->out, "(gmb_int_t)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
    return 1;
}
static int sx_reverse(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    /* REVERSE(s$) — namespace STRING: inverte i caratteri (parity oracolo). */
    sb_put(&c->out, "gmb_str_reverse("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int sx_eval(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    c->uses_eval = 1;
    sb_put(&c->out, "({ gmb_string_t __ms = ");
    if (etype(c, a[0]) == TY_STR) emit_expr(c, a[0]);
    else { sb_put(&c->out, "gmb_double_to_string("); emit_expr(c, a[0]); sb_put(&c->out, ")"); }
    sb_put(&c->out, "; if (!__ms.data) __ms = gmb_string_alloc(\"\");"
                  " gmb_val_t __mr = gmb_val_nil();"
                  " (void)gmb_eval_env(__ms.data, 0, gmb_host_expr_get, gmb_host_expr_set,"
                  " NULL, gmb_host_expr_call, gmb_eval_pr_str, gmb_eval_pr_num, gmb_eval_pr_nl, NULL, &__mr); gmb_val_to_double(__mr); })");
    return 1;
}
static int sx_htmlenc(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_string_alloc(gmb_html_encode("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1; }
static int sx_urlenc(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_string_alloc(gmb_url_encode("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1; }
static int sx_jsenc(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_string_alloc(gmb_js_encode("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1; }
static int sx_req(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_bool_t)(gmb_validation_required("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data))"); return 1; }
static int sx_email(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_bool_t)(gmb_validation_email("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data))"); return 1; }
static int sx_isurl(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_bool_t)(gmb_validation_url("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data))"); return 1; }
static int sx_cc(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_bool_t)(gmb_validation_credit_card("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data))"); return 1; }
static int sx_len(ctx_t* c, gmb_expr_t** a, int n) { if (n < 3) return 0; sb_put(&c->out, "(gmb_bool_t)(gmb_validation_length("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, ")))"); return 1; }
static int sx_split(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_split("); emit_expr(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int sx_remove(ctx_t* c, gmb_expr_t** a, int n) { if (n != 2) return 0; sb_put(&c->out, "gmb_str_replace("); emit_str_operand(c, a[0]); sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ", gmb_string_alloc(\"\"))"); return 1; }

static const sx_row SX_TBL[] = {
    { GMB_TOK_HtmlEncode, sx_htmlenc }, { GMB_TOK_UrlEncode, sx_urlenc }, { GMB_TOK_JsEncode, sx_jsenc },
    { GMB_TOK_IsRequired, sx_req }, { GMB_TOK_IsEmail, sx_email }, { GMB_TOK_IsUrl, sx_isurl },
    { GMB_TOK_IsCreditCard, sx_cc }, { GMB_TOK_IsLength, sx_len }, { GMB_TOK_Split, sx_split },
    { GMB_TOK_Remove, sx_remove },
    { GMB_TOK_Bin, sx_bin }, { GMB_TOK_Hex, sx_hex }, { GMB_TOK_Oct, sx_oct },
    { GMB_TOK_Camel, sx_camel }, { GMB_TOK_Kebab, sx_kebab }, { GMB_TOK_Pascal, sx_pascal },
    { GMB_TOK_Snake, sx_snake }, { GMB_TOK_Sentence, sx_sentence }, { GMB_TOK_Title, sx_title },
    { GMB_TOK_Petscii, sx_petscii }, { GMB_TOK_Empty, sx_empty }, { GMB_TOK_Count, sx_count },
    { GMB_TOK_Replace, sx_replace }, { GMB_TOK_Search, sx_search }, { GMB_TOK_Tab, sx_tab },
    { GMB_TOK_Wrap, sx_wrap }, { GMB_TOK_Reverse, sx_reverse }, { GMB_TOK_Eval, sx_eval },
    { GMB_TOK_Dup, sx_dup },
};
int emit_bf_strings_extra(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    /* (AI.SEARCH è AiVectorSearch → dominio AI per-token; mai intercettata qui.) */
    for (size_t i = 0; i < sizeof(SX_TBL) / sizeof(SX_TBL[0]); i++)
        if (SX_TBL[i].tk == tk) return SX_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: io_files (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } io_row;
static int io_fileexists(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_file_exists(("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data) ? -1.0 : 0.0)");
    return 1;
}
static int io_direxists(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_dir_exists("); emit_str_operand(c, a[0]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}

/* FREEFILE: primo numero di file libero, dal runtime condiviso (§0bis). Prima era la
 * COSTANTE 1: un programma con piu' file aperti otteneva sempre 1 (numero gia' occupato). */
static int io_freefile(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_file_free_file())");
    return 1;
}
static int io_seek(ctx_t* c, gmb_expr_t** a, int n) {
    if (n == 1) {
        /* 0-based: parita' IlRuntime.Seek (stream.Position) e uso nei sample. Prima tell+1. */
        sb_put(&c->out, "((long)gmb_file_tell("); emit_expr(c, a[0]); sb_put(&c->out, "))");
        return 1;
    }
    if (n == 2) {
        sb_put(&c->out, "gmb_file_seek("); emit_expr(c, a[0]);
        sb_put(&c->out, ", (long)(("); emit_expr(c, a[1]); sb_put(&c->out, ") - 1), 0)");
        return 1;
    }
    return 0;
}
static int io_fopen(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ gmb_int_t __foh = 0; if (gmb_file_open_auto(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ", \"r\", &__foh) != 0) __foh = 0; __foh; })");
    return 1;
}
static int io_fcreate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "({ gmb_int_t __fch = 0; gmb_file_open_auto(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ", \"w+\", &__fch); __fch; })");
    return 1;
}
static int io_fwrite(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_file_write_string("); emit_expr(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ")");
    return 1;
}
static int io_fwriteline(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "({ gmb_file_write_string("); emit_expr(c, a[0]);
    sb_put(&c->out, ", "); emit_str_operand(c, a[1]);
    sb_put(&c->out, "); gmb_file_write_newline("); emit_expr(c, a[0]); sb_put(&c->out, "); 0; })");
    return 1;
}
/* FREAD(handle [, count]) — FUNZIONE PURA (AGENTS §1/§3): ritorna i dati letti (una riga se
 * count e' omesso, altrimenti count byte). NON scrive in alcun buffer; per scrivere esiste il
 * COMANDO `FREAD handle [, count] INTO buffer`, che il parser desugara in un'assegnazione. */
static int io_fread(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || n > 2) return 0;
    if (n == 2) {
        sb_put(&c->out, "gmb_file_read_n((gmb_int_t)(");
        emit_expr(c, a[0]); sb_put(&c->out, "), (gmb_int_t)(");
        emit_expr(c, a[1]); sb_put(&c->out, "))");
    } else {
        sb_put(&c->out, "gmb_file_read_line_str((gmb_int_t)(");
        emit_expr(c, a[0]); sb_put(&c->out, "))");
    }
    return 1;
}
static int io_fseek(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    /* 0-based: parita' IlRuntime.FSeek. Prima (pos - 1) 1-based → FSeek(h,0) passava -1. */
    sb_put(&c->out, "gmb_file_seek("); emit_expr(c, a[0]);
    sb_put(&c->out, ", (long)("); emit_expr(c, a[1]); sb_put(&c->out, "), 0)");
    return 1;
}
static int io_fclose(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_file_close("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int io_countlines(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_count_lines((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data))");
    return 1;
}
static int io_gettempfile(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_temp_file())");
    return 1;
}
static int io_gettempdir(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_temp_dir())");
    return 1;
}
static int io_apppath(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_search_paths())");
    return 1;
}
static int io_binpath(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_bin_dir())");
    return 1;
}
static int io_projectfolder(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_current_file_dir())");
    return 1;
}
static int io_cwd(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_app_dir())");
    return 1;
}
static int io_parentdir(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_fs_dir_name("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
/* GETFOLDERPATH(name [, maxDepth]): cerca una cartella PER NOME salendo dalla cartella del
 * programma e poi in BFS fino a maxDepth (parità IlRuntime.GetFolderPath). NON è il parent:
 * prima emetteva gmb_fs_dir_name — cioè la PARENTDIR — quindi IO.GETFOLDERPATH(".") ritornava
 * "" (trovato da MiniTests/io/io_misc.bas sulla gamba C99Native: IL/PCode/VMC dicevano "si").
 * Usa la stessa funzione canonica di VMC e del C99 C# (gmb_path_find_folder, §0bis). */
static int io_getfolderpath(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_find_folder((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data, ");
    if (n >= 2) { sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "10");
    sb_put(&c->out, "))");
    return 1;
}
static int io_normalizepath(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_fs_normalize_path("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int io_foldersize(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_fs_folder_size("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int io_resolvefilepath(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_path_resolve_file((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data))");
    return 1;
}
static int io_dir(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_fs_dir("); emit_str_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
/* FERROR: stato ultima operazione file (0=ok, 2=non trovato, 1=altro) dal runtime condiviso. */
static int io_ferror(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_file_error())");
    return 1;
}
/* FINPUT(count, handle): legge `count` byte dall'handle → stringa (parita' IlRuntime.FileInput). */
static int io_fileinput(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_file_read_n((gmb_int_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), (gmb_int_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
/* READBYTES / READLINES: liste costruite dal runtime condiviso (§0bis). */
static int io_readbytes(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_read_all_bytes_list(("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
/* FONTS.TTRENDER / TTMETRICS / TTGLYPHS (funzioni, §1/§3): le liste le costruisce il
 * runtime condiviso (gmb_fonts.c), qui si emette solo la chiamata. */
int emit_bf_fonts(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                  gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    if (tk == GMB_TOK_TtGlyphsFunc) {
        if (nargs != 1 || !args[0]) return 0;
        sb_put(&c->out, "gmb_fonts_tt_glyphs_list((");
        emit_str_operand(c, args[0]);
        sb_put(&c->out, ").data)");
        return 1;
    }
    if (nargs != 3 || !args[0] || !args[1] || !args[2]) return 0;
    if (tk == GMB_TOK_TtRenderFunc) sb_put(&c->out, "gmb_fonts_tt_render_list((");
    else sb_put(&c->out, "gmb_fonts_tt_metrics_list((");
    emit_str_operand(c, args[0]);
    sb_put(&c->out, ").data, (double)(");
    emit_expr(c, args[1]);
    sb_put(&c->out, "), (int)(");
    emit_expr(c, args[2]);
    sb_put(&c->out, "))");
    return 1;
}

static int io_readlines(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_read_all_lines_list(("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
/* READFILE: intero file di testo -> stringa (runtime condiviso, §0bis). */
static int io_readfile(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_read_file_text(("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
/* MEMORYMAP (IO.MEMORYMAP/MMAPPEEK/MMAPREADBYTES): handle int64_t + runtime gmb_mmap_*. */
static int io_mmap_open(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_mmap_open(("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
static int io_mmap_peek(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_mmap_peek((int64_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int64_t)("); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int io_mmap_readbytes(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_mmap_read_bytes_list((int64_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int64_t)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
    emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static const io_row IO_TBL[] = {
    { GMB_TOK_MemoryMap, io_mmap_open }, { GMB_TOK_MemoryMapPeek, io_mmap_peek },
    { GMB_TOK_MemoryMapReadBytes, io_mmap_readbytes },
    { GMB_TOK_FileExistsFunc, io_fileexists }, { GMB_TOK_DirExistsFunc, io_direxists },
    { GMB_TOK_FError, io_ferror }, { GMB_TOK_FileInput, io_fileinput },
    { GMB_TOK_ReadBytesFunc, io_readbytes }, { GMB_TOK_ReadLinesFunc, io_readlines },

    { GMB_TOK_ReadFileFunc, io_readfile },
    { GMB_TOK_FreeFile, io_freefile },
    { GMB_TOK_Seek, io_seek }, { GMB_TOK_FOpen, io_fopen }, { GMB_TOK_FCreate, io_fcreate },
    { GMB_TOK_FWrite, io_fwrite }, { GMB_TOK_FWRITELINE, io_fwriteline }, { GMB_TOK_FRead, io_fread },
    { GMB_TOK_FSeek, io_fseek }, { GMB_TOK_FClose, io_fclose },
    { GMB_TOK_CountLinesFunc, io_countlines },
    { GMB_TOK_GetTempFileFunc, io_gettempfile },
    { GMB_TOK_GetTempDirFunc, io_gettempdir },
    { GMB_TOK_AppPathFunc, io_apppath }, { GMB_TOK_BinPathFunc, io_binpath },
    { GMB_TOK_ProjectFolderFunc, io_projectfolder }, { GMB_TOK_CwdFunc, io_cwd },
    { GMB_TOK_ParentDirFunc, io_parentdir }, { GMB_TOK_GetFolderPathFunc, io_getfolderpath },
    { GMB_TOK_NormalizePathFunc, io_normalizepath }, { GMB_TOK_FolderSizeFunc, io_foldersize },
    { GMB_TOK_ResolveFilePathFunc, io_resolvefilepath }, { GMB_TOK_DirFunc, io_dir },
};
static int io_eof(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_file_eof("); emit_handle(c, a[0], line); sb_put(&c->out, ")");
    return 1;
}
static int io_lof(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_file_length("); emit_handle(c, a[0], line); sb_put(&c->out, ")");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } ioh_row;
static const ioh_row IOH_TBL[] = {
    { GMB_TOK_Eof, io_eof }, { GMB_TOK_Lof, io_lof },
    /* IO.EOF(...) risolve a GMB_TOK_EofFunc (TokenKind CONDIVISO da DBF/IO/ISAM/RDD, vedi
     * gmb_ns_lookup.h): senza questa riga IO.EOF non era raggiungibile dal C99Native
     * ("funzione o subroutine 'IO.EOF' non definita"). La riga composito è filtrata su
     * ns="IO" per non emettere gmb_file_eof su un'area DBF/ISAM. */
    { GMB_TOK_EofFunc, io_eof },
};
int emit_bf_io_files(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns;
    for (size_t i = 0; i < sizeof(IO_TBL) / sizeof(IO_TBL[0]); i++)
        if (IO_TBL[i].tk == tk) return IO_TBL[i].sp(c, args, nargs);
    for (size_t i = 0; i < sizeof(IOH_TBL) / sizeof(IOH_TBL[0]); i++)
        if (IOH_TBL[i].tk == tk) return IOH_TBL[i].sp(c, args, nargs, line);
    return 0;
}
/* ── dominio: net (delegazione §0quater) ── */
/* NET.PEEK(handle, n) → sbircia n byte senza rimuoverli (gmb_net_tcp_peek). */
static int nt_peek(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "({ char __pbuf[8192]; int __pn = gmb_net_tcp_peek((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), (uint8_t*)__pbuf, (size_t)(");
    emit_expr(c, a[1]);
    sb_put(&c->out, ")); if (__pn < 0) __pn = 0; if (__pn > 8191) __pn = 8191; __pbuf[__pn] = 0; gmb_string_alloc(__pbuf); })");
    return 1;
}
/* NET.CONN(handle) → double (1=connesso, 0=no) */
static int nt_conn(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "((double)gmb_net_conn((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")))");
    return 1;
}
/* NET.LOCALPORT(handle) → double */
static int nt_local_port(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "((double)gmb_net_local_port((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")))");
    return 1;
}
/* NET.REMOTEPORT(handle) → double */
static int nt_remote_port(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "((double)gmb_net_remote_port((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")))");
    return 1;
}
/* NET.RECV(handle) → double (bytes available) */
static int nt_recv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "((double)gmb_net_tcp_available((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")))");
    return 1;
}
static int nt_host(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "({ char __hbuf[128]=\"\"; gmb_net_local_host(__hbuf, sizeof(__hbuf)); gmb_string_alloc(__hbuf); })");
    return 1;
}
static int nt_gethostip(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    sb_put(&c->out, "({ char __hipbuf[128]=\"\"; gmb_net_resolve_host(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __hipbuf, sizeof(__hipbuf)); gmb_string_alloc(__hipbuf); })");
    return 1;
}
static int nt_remoteip(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    sb_put(&c->out, "({ char __ripbuf[64]=\"\"; gmb_net_remote_ip((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), __ripbuf, sizeof(__ripbuf)); gmb_string_alloc(__ripbuf); })");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } net_row;
/* Tabella token-first (regola d'oro §12): l'audit C99Native la conta come emissione reale. */
static const net_row NET_TBL[] = {
    { GMB_TOK_Peek, nt_peek }, { GMB_TOK_Conn, nt_conn },
    { GMB_TOK_LocalPort, nt_local_port }, { GMB_TOK_RemotePort, nt_remote_port },
    { GMB_TOK_Recv, nt_recv }, { GMB_TOK_Host, nt_host },
    { GMB_TOK_GetHostIp, nt_gethostip }, { GMB_TOK_RemoteIp, nt_remoteip },
};
int emit_bf_net(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(NET_TBL) / sizeof(NET_TBL[0]); i++)
        if (NET_TBL[i].tk == tk) return NET_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: connector (delegazione §0quater) ── */
static int cx_str1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int cx_str2(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int cx_str3(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))");
    return 1;
}
static int cx_num1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_printf(&c->out, "((double)%s(", fn);
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int cx_num1b(ctx_t* c, gmb_expr_t** a, int n, const char* fn, int b) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_printf(&c->out, "((double)%s(", fn);
    emit_str_operand(c, a[0]); sb_printf(&c->out, ".data, %d))", b);
    return 1;
}
static int cx_num2(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) { sb_put(&c->out, "0.0"); return 1; }
    sb_printf(&c->out, "((double)%s(", fn);
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int cx_num3(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) { sb_put(&c->out, "0.0"); return 1; }
    sb_printf(&c->out, "((double)%s(", fn);
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))");
    return 1;
}
int emit_bf_connector(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    switch (tk) {
        case GMB_TOK_ConnectorList: sb_put(&c->out, "gmb_string_alloc(gmb_connector_list())"); return 1;
        case GMB_TOK_ConnectorMarketplaceList: sb_put(&c->out, "gmb_string_alloc(gmb_connector_marketplace_list())"); return 1;
        case GMB_TOK_ConnectorStatus: return cx_str1(c, args, nargs, "gmb_connector_status");
        case GMB_TOK_ConnectorTools: return cx_str1(c, args, nargs, "gmb_connector_tools");
        case GMB_TOK_ConnectorSearch: return cx_str1(c, args, nargs, "gmb_connector_search");
        case GMB_TOK_ConnectorMarketplaceGet: return cx_str1(c, args, nargs, "gmb_connector_marketplace_get");
        case GMB_TOK_ConnectorResources: return cx_str1(c, args, nargs, "gmb_connector_resources");
        case GMB_TOK_ConnectorPrompts: return cx_str1(c, args, nargs, "gmb_connector_prompts");
        case GMB_TOK_ConnectorPolicyGet: return cx_str2(c, args, nargs, "gmb_connector_policy_get");
        case GMB_TOK_ConnectorSecretGet: return cx_str2(c, args, nargs, "gmb_connector_secret_get");
        case GMB_TOK_ConnectorCall: return cx_str3(c, args, nargs, "gmb_connector_call");
        case GMB_TOK_ConnectorRegister: {
            if (nargs < 2) { sb_put(&c->out, "0.0"); return 1; }
            sb_put(&c->out, "((double)gmb_connector_register(");
            emit_str_operand(c, args[0]); sb_put(&c->out, ".data, ");
            emit_str_operand(c, args[1]); sb_put(&c->out, ".data, ");
            if (nargs >= 3) { emit_str_operand(c, args[2]); sb_put(&c->out, ".data))"); }
            else sb_put(&c->out, "NULL))");
            return 1;
        }
        case GMB_TOK_ConnectorUnregister: return cx_num1(c, args, nargs, "gmb_connector_unregister");
        case GMB_TOK_ConnectorLoad: return cx_num1(c, args, nargs, "gmb_connector_load");
        case GMB_TOK_ConnectorEnable: return cx_num1b(c, args, nargs, "gmb_connector_enable", 1);
        case GMB_TOK_ConnectorDisable: return cx_num1b(c, args, nargs, "gmb_connector_enable", 0);
        case GMB_TOK_ConnectorRefresh: return cx_num1(c, args, nargs, "gmb_connector_refresh");
        case GMB_TOK_ConnectorPolicySet: return cx_num3(c, args, nargs, "gmb_connector_policy_set");
        case GMB_TOK_ConnectorSecretSet: return cx_num3(c, args, nargs, "gmb_connector_secret_set");
        case GMB_TOK_ConnectorAuth: return cx_num2(c, args, nargs, "gmb_connector_auth");
        case GMB_TOK_ConnectorApproval: sb_put(&c->out, "0.0"); return 1;
        default: return 0;
    }
}
/* ── dominio: system (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } sys_row;
static int sys_ti(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_timer()"); return 1; }
static int sys_stri(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_time_string()"); return 1; }
static int sys_datemoney(ctx_t* c, gmb_expr_t** a, int n) {
    /* DATE$() → stringa dd/MM/yyyy (oracolo C99CodeEmitter.Builtin.cs:2079) */
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_dt_datetime_to_string(gmb_dt_today())"); return 1;
}
/* ALERT/CONFIRM: con schermo grafico il runtime apre una FINESTRA (gmb_console_alert/
 * confirm -> gmb_form_dialog); headless restano 0. Stesso messaggio, esito numerico. */
static int sys_alert(ctx_t* c, gmb_expr_t** a, int n) {
    sb_put(&c->out, "gmb_console_alert((");
    if (n >= 1 && a[0]) emit_expr(c, a[0]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ").data)"); return 1;
}
static int sys_confirm(ctx_t* c, gmb_expr_t** a, int n) {
    sb_put(&c->out, "gmb_console_confirm((");
    if (n >= 1 && a[0]) emit_expr(c, a[0]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ").data)"); return 1;
}
static int sys_commandcount(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_command_count()"); return 1; }
static int sys_command(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_command_arg((int)(");
    emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int sys_envget(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_env_get((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data))");
    return 1;
}
static int sys_envhas(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    /* Convenzione booleana BASIC: -1 se presente, 0 se assente (gmb_env_has è 1/0). */
    sb_put(&c->out, "((gmb_double_t)(gmb_env_has((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data) ? -1.0 : 0.0))");
    return 1;
}
static int sys_sys(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* SYSTEM.SYS(prop): runtime condiviso gmb_sys_info (OS/ARCH/UI/CPU/MACHINE/USER/CWD/MEM*),
     * parità con IlRuntime.Sys e coll'emitter C# (gmb_string_alloc(gmb_sys_info(...))). */
    sb_put(&c->out, "gmb_string_alloc(gmb_sys_info(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int sys_tab(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_str_tab("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int sys_ncpu(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_ncpu()"); return 1; }
static int sys_numgoroutine(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_c99_sched_active_count()"); return 1; }
static int sys_gomaxprocs(ctx_t* c, gmb_expr_t** a, int n) {
    /* GOMAXPROCS([n]): 0 arg = legge, 1 arg = imposta (0 = lettura nel runtime). */
    sb_put(&c->out, "(gmb_double_t)gmb_gomaxprocs(");
    if (n >= 1) { sb_put(&c->out, "(int)("); emit_expr(c, a[0]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
    return 1;
}
static int sys_varptr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)(uintptr_t)&("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int sys_strptr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)(uintptr_t)("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data");
    return 1;
}
/* ── canone SIZEOF / UDTPACKED / UDTALIGNED ─────────────────────────────────────
 * La dimensione è quella LOGICA del tipo dichiarato (byte del record / scambio FFI),
 * NON la taglia della struct C: qui ogni numerico è un gmb_double_t da 8 byte, ma
 * SIZEOF di un INTEGER risponde 2 come su IL, PCodeVM, VM-C. Unica
 * sorgente del canone per i backend gestiti è common/Ast/TypeLayout.cs; questo è
 * l'UNICO posto in cui la tabella è replicata, perché il compilatore C puro non può
 * chiamare il C# (regola dei due compilatori separati). I tipi senza layout statico
 * (stringa dinamica, array, collezione, CLASS, oggetto) rispondono -1. */
static long gmb_cls_layout_size(ctx_t* c, struct cls_info* ci, int depth);
/* Larghezze del canone (TypeLayout.cs). LE GRAFIE SONO QUELLE DEL LESSICO (Lexer.Keywords.cs):
 * un sinonimo dimenticato qui diventa -1 su questa gamba mentre le altre danno il byte corretto
 * (UINT era assente → SIZEOF(DIM u AS UINT) = -1 contro 2). Il set completo dei sinonimi è:
 * 1 = BIT/BOOL/BOOLEAN/BYTE/SBYTE · 2 = INTEGER/SHORT/INT/UINTEGER/USHORT/UINT/WORD ·
 * 4 = SINGLE/FLOAT · 8 = LONG/ULONG/QUAD/UQUAD/DOUBLE/NUMERIC/DATE/TIME/DATETIME/TIMESPAN ·
 * 16 = CURRENCY. */
static long gmb_scalar_bytes(const char* t) {
    if (!t) return 0;
    if (!strcasecmp(t, "BIT") || !strcasecmp(t, "BOOL") || !strcasecmp(t, "BOOLEAN")
        || !strcasecmp(t, "BYTE") || !strcasecmp(t, "SBYTE")) return 1;
    if (!strcasecmp(t, "INTEGER") || !strcasecmp(t, "SHORT") || !strcasecmp(t, "INT")
        || !strcasecmp(t, "UINTEGER") || !strcasecmp(t, "USHORT") || !strcasecmp(t, "UINT")
        || !strcasecmp(t, "WORD")) return 2;
    if (!strcasecmp(t, "SINGLE") || !strcasecmp(t, "FLOAT")) return 4;
    if (!strcasecmp(t, "CURRENCY")) return 16;
    if (!strcasecmp(t, "LONG") || !strcasecmp(t, "QUAD") || !strcasecmp(t, "ULONG")
        || !strcasecmp(t, "UQUAD") || !strcasecmp(t, "DOUBLE") || !strcasecmp(t, "NUMERIC")
        || !strcasecmp(t, "DATE") || !strcasecmp(t, "TIME") || !strcasecmp(t, "DATETIME")
        || !strcasecmp(t, "TIMESPAN")) return 8;
    return 0;
}
/* "STRING * 10" → 10 (0 se non a lunghezza fissa). */
static long gmb_str_fixed_len(const char* t) {
    const char* star = t ? strstr(t, "* ") : NULL;
    return star ? strtol(star + 2, NULL, 10) : 0;
}
/* Larghezza di un CAMPO di TYPE dal testo del tipo dichiarato: scalare, STRING*n = n+1,
 * UDT annidato = proprio layout, campo dimensionato ("T[]", marker del parser) o
 * collezione/oggetto = riferimento da 8 (TypeLayout.FieldByteSize). */
static long gmb_cls_field_bytes(ctx_t* c, const char* t, int depth) {
    if (!t) return 8;
    size_t n = strlen(t);
    /* Campo-array (`T[]` o `T[N]`/`T[N][M]`): RIFERIMENTO da 8 byte (gli elementi non sono
     * inline). Riconosce QUALSIASI suffisso `[...]` (il parser ora marca il bound reale). */
    if (n >= 2 && t[n - 1] == ']' && strchr(t, '[')) return 8;
    long sc = gmb_scalar_bytes(t);
    if (sc > 0) return sc;
    if (!strncasecmp(t, "STRING", 6)) { long fl = gmb_str_fixed_len(t); return fl > 0 ? fl + 1 : 8; }
    struct cls_info* nested = cls_find(c, t);
    if (nested && nested->is_udt) return gmb_cls_layout_size(c, nested, depth + 1);
    return 8;
}
static long gmb_cls_layout_size(ctx_t* c, struct cls_info* ci, int depth) {
    if (!ci || depth > 8) return 0;
    long offset = 0, maxAlign = 1;
    for (int i = 0; i < ci->nfields; i++) {
        long sz = gmb_cls_field_bytes(c, ci->ftype[i], depth);
        if (sz < 1) sz = 1;
        if (ci->is_packed) { offset += sz; continue; }
        offset = (offset + sz - 1) / sz * sz + sz;
        if (sz > maxAlign) maxAlign = sz;
    }
    return ci->is_packed ? offset : (offset + maxAlign - 1) / maxAlign * maxAlign;
}
/* SIZEOF di un testo di tipo: -1 se statico non c'è (stringa dinamica, collezione, CLASS). */
static long gmb_sizeof_type_text(ctx_t* c, const char* t) {
    if (!t) return -1;
    long sc = gmb_scalar_bytes(t);
    if (sc > 0) return sc;
    if (!strncasecmp(t, "STRING", 6)) { long fl = gmb_str_fixed_len(t); return fl > 0 ? fl + 1 : -1; }
    struct cls_info* ci = cls_find(c, t);
    if (ci && ci->is_udt) return gmb_cls_layout_size(c, ci, 0);
    return -1;
}
/* Nome di tipo SCALARE usato come identificatore nell'argomento di SIZEOF — SIZEOF(INTEGER)=2.
 * Le grafie sono ESATTAMENTE le keyword del lessico C# (Lexer.Keywords.cs), cioè il set di
 * TypeLayout.TrySizeOfTypeName: i sinonimi interni del solo C (FLOAT/WORD/NUMERIC, che il C#
 * non lessifica come tipo) QUI non compaiono, perché su un identificatore libero significherebbero
 * "nome sconosciuto" e le gambe gestite risponderebbero col default numerico. STRING → -1. */
static int gmb_sizeof_type_name(const char* t, long* out) {
    if (!t) return 0;
    if (!strcasecmp(t, "BIT") || !strcasecmp(t, "BOOL") || !strcasecmp(t, "BOOLEAN")
        || !strcasecmp(t, "BYTE") || !strcasecmp(t, "SBYTE")) { *out = 1; return 1; }
    if (!strcasecmp(t, "INTEGER") || !strcasecmp(t, "SHORT") || !strcasecmp(t, "INT")
        || !strcasecmp(t, "UINTEGER") || !strcasecmp(t, "USHORT") || !strcasecmp(t, "UINT")) { *out = 2; return 1; }
    if (!strcasecmp(t, "SINGLE")) { *out = 4; return 1; }
    if (!strcasecmp(t, "CURRENCY")) { *out = 16; return 1; }
    if (!strcasecmp(t, "LONG") || !strcasecmp(t, "QUAD") || !strcasecmp(t, "ULONG")
        || !strcasecmp(t, "UQUAD") || !strcasecmp(t, "DOUBLE")
        || !strcasecmp(t, "DATETIME")
        || !strcasecmp(t, "TIMESPAN")) { *out = 8; return 1; }
    /* DATE e TIME NON sono in questa tabella: nelle gambe gestite sono anche le funzioni a zero
     * argomenti DATE()/TIME(), quindi SIZEOF(DATE) arriva al parser come CHIAMATA e cade fuori
     * canone (-1). Tenere il nome qui avrebbe ridato 8 solo su questa gamba. Come TIPO
     * dichiarato (DIM v AS DATE) restano in gmb_scalar_bytes → 8 su tutte le gambe. */
    if (!strcasecmp(t, "STRING")) { *out = -1; return 1; }
    return 0;
}
/* Argomento di SIZEOF/UDTPACKED, canone TypeLayout (unico posto dove la tabella è replicata):
 *  1. letterale numerico = 8 (il double di default del linguaggio);
 *  2. variabile dichiarata (DIM/parametro) = dimensione del tipo dichiarato (UDT → layout),
 *     -1 se il tipo dichiarato non ha layout statico (STRING dinamica, collezione, CLASS);
 *  3. nome di TYPE = layout dello UDT; nome di tipo scalare = sua larghezza (STRING → -1);
 *  4. identificatore NON dichiarato = DOUBLE di default → 8, salvo che il tipo della variabile
 *     nota all'emitter non sia stringa/oggetto/collezione/array (→ -1);
 *  5. ogni ALTRA forma di espressione (aritmetica, chiamata, membro, indice) = -1: qui non c'è
 *     un tipo statico da misurare e l'emitter non inferisce dal flusso dati come IL. */
static long gmb_sizeof_arg(ctx_t* c, gmb_expr_t* e) {
    if (!e) return -1;
    if (e->kind == GMB_EXPR_NUM) return gmb_scalar_bytes("DOUBLE");
    if (e->kind != GMB_EXPR_VAR || !e->var) return -1;
    long byName = 0;
    if (gmb_sizeof_type_name(e->var, &byName)) return byName;
    char declared[32];
    if (gmb_var_declared_type(c, e->var, declared, sizeof declared))
        return gmb_sizeof_type_text(c, declared);
    struct cls_info* ci = cls_find(c, e->var);
    if (ci && ci->is_udt) return gmb_cls_layout_size(c, ci, 0);
    /* Parità col parser gestito (canone regola 5): un identificatore il cui token è una KEYWORD
     * che NON è un nome di tipo né un TYPE utente è una FUNZIONE built-in a zero argomenti
     * (DATE, TIME, EMPTY, ...) — nelle gambe gestite arriva a SIZEOF come CHIAMATA, senza tipo
     * statico, e risponde -1. Il lexer C risolve già il TokenKind sul nodo: il test è sulla chiave
     * canonica, mai sulla grafia (REGOLA-PRIMA del dispatch).
    /* ECCEZIONE GMB_TOK_RddFloat: nel solo mondo C il nome nudo FLOAT risolve sulla funzione RDD
     * FLOAT(). La tabella CONDIVISA (TypeLayout.TrySizeOfTypeName, sorgente unica) non contiene
     * FLOAT — il lessico C# non lo keyword-a affatto — e un token che non è nella tabella comune
     * NON può cambiare la risposta di questa gamba: nelle gambe gestite FLOAT arriva come
     * identificatore implicito → DOUBLE di default (8), il valore che WORD/NUMERIC (che il lexer C
     * non keyword-a) già producono qui. DATE/TIME restano -1: keyword in ENTRAMBI i lessichi. */
    if (e->tk != GMB_TOK_Identifier && e->tk != GMB_TOK_Unknown && e->tk != GMB_TOK_RddFloat) return -1;
    var_t* v = lookup_var(c, e->var);
    if (v && (v->type != 0 || v->vclass[0] || v->gkind || v->is_array || v->boxed_val)) return -1;
    return gmb_scalar_bytes("DOUBLE");
}
static int sys_sizeof(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_printf(&c->out, "(gmb_double_t)%ld", gmb_sizeof_arg(c, a[0]));
    return 1;
}
static int sys_udtpacked(ctx_t* c, gmb_expr_t** a, int n) {
    /* UDTPACKED(udt|NomeTipo) = NUMERO DI CAMPI (proprietà compile-time, canone dei
     * backend gestiti) — NON un serializzatore. */
    if (n < 1) return 0;
    long nf = 0;
    if (a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        struct cls_info* ci = cls_find(c, a[0]->var);
        if (!ci) {
            char declared[32];
            if (gmb_var_declared_type(c, a[0]->var, declared, sizeof declared)) ci = cls_find(c, declared);
        }
        if (ci && ci->is_udt) nf = ci->nfields;
    }
    sb_printf(&c->out, "(gmb_double_t)%ld", nf);
    return 1;
}
static int sys_udtaligned(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a;
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)1");
    return 1;
}
static int sys_empty(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (etype(c, a[0]) == TY_STR) {
        sb_put(&c->out, "(gmb_str_isblank("); emit_str_operand(c, a[0]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    } else {
        sb_put(&c->out, "(("); emit_expr(c, a[0]); sb_put(&c->out, ") == 0 ? -1.0 : 0.0)");
    }
    return 1;
}
static int sys_iif(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (etype(c, a[1]) == TY_STR || etype(c, a[2]) == TY_STR) {
        sb_put(&c->out, "(("); emit_expr(c, a[0]); sb_put(&c->out, ") ? ");
        emit_str_operand(c, a[1]); sb_put(&c->out, " : ");
        emit_str_operand(c, a[2]); sb_put(&c->out, ")");
    } else {
        sb_put(&c->out, "(("); emit_expr(c, a[0]); sb_put(&c->out, ") ? (");
        emit_expr(c, a[1]); sb_put(&c->out, ") : (");
        emit_expr(c, a[2]); sb_put(&c->out, "))");
    }
    return 1;
}
static int sys_shellout(ctx_t* c, gmb_expr_t** a, int n) {
    /* SYSTEM.SHELLOUT("cmd") → stdout+stderr (runtime condiviso gmb_shell.c) */
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_shell_out((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data))");
    return 1;
}
static int sys_shellcode(ctx_t* c, gmb_expr_t** a, int n) {
    /* SYSTEM.SHELLCODE("cmd") → exit code */
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_shell_code((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
static int sys_choose(ctx_t* c, gmb_expr_t** a, int n) {
    /* CHOOSE(idx, v1, v2, ...) — 1-based pick. Se il parser annida i ParamArray in un
     * call `_array_`, appiattisci. I valori sono costruiti come gmb_val_t compound literal;
     * ritorno: se qualche valore e' STRING → gmb_val_to_string, altrimenti gmb_val_to_double. */
    if (n < 2) return 0;
    int first = 1;
    int nvals = n - 1;
    if (n == 2 && a[1] && a[1]->kind == GMB_EXPR_CALL && a[1]->var
        && strcmp(a[1]->var, "_array_") == 0) {
        nvals = a[1]->nargs;
        first = -1; /* valori in a[1]->args */
    }
    int is_str = 0;
    for (int i = 0; i < nvals; i++) {
        gmb_expr_t* v = first < 0 ? a[1]->args[i] : a[i + 1];
        if (etype(c, v) == TY_STR) { is_str = 1; break; }
    }
    if (is_str) sb_put(&c->out, "gmb_val_to_string(gmb_choose((gmb_int_t)(");
    else sb_put(&c->out, "gmb_val_to_double(gmb_choose((gmb_int_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), (gmb_val_t[]){");
    for (int i = 0; i < nvals; i++) {
        gmb_expr_t* v = first < 0 ? a[1]->args[i] : a[i + 1];
        if (i) sb_put(&c->out, ", ");
        if (etype(c, v) == TY_STR) { sb_put(&c->out, "gmb_make_val_string("); emit_str_operand(c, v); sb_put(&c->out, ".data)"); }
        else { sb_put(&c->out, "gmb_make_val_double("); emit_expr(c, v); sb_put(&c->out, ")"); }
    }
    sb_printf(&c->out, "}, (gmb_int_t)(%d)))", nvals);
    return 1;
}
static const sys_row SYS_TBL[] = {
    { GMB_TOK_Ti, sys_ti }, { GMB_TOK_StrTi, sys_stri }, { GMB_TOK_DATEMONEY, sys_datemoney },
    { GMB_TOK_Alert, sys_alert }, { GMB_TOK_Confirm, sys_confirm },
    { GMB_TOK_CommandCount, sys_commandcount }, { GMB_TOK_Command, sys_command },
    { GMB_TOK_EnvGet, sys_envget }, { GMB_TOK_EnvHas, sys_envhas },
    { GMB_TOK_Sys, sys_sys }, { GMB_TOK_Tab, sys_tab },
    { GMB_TOK_NumCpu, sys_ncpu }, { GMB_TOK_NumGoroutine, sys_numgoroutine },
    { GMB_TOK_GoMaxProcs, sys_gomaxprocs }, { GMB_TOK_VarPtr, sys_varptr },
    { GMB_TOK_StrPtr, sys_strptr }, { GMB_TOK_SizeOf, sys_sizeof }, { GMB_TOK_Empty, sys_empty },
    { GMB_TOK_UdtPacked, sys_udtpacked }, { GMB_TOK_UdtAligned, sys_udtaligned },
    { GMB_TOK_Iif, sys_iif }, { GMB_TOK_ShellOut, sys_shellout }, { GMB_TOK_ShellCode, sys_shellcode },
    { GMB_TOK_Choose, sys_choose },
};
int emit_bf_system(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(SYS_TBL) / sizeof(SYS_TBL[0]); i++)
        if (SYS_TBL[i].tk == tk) return SYS_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: json (delegazione §0quater) ── */
/* JSON.EXTRACT(src, path): forma TESTUALE (default) o TIPIZZATA (`typed=1`, usata dal
 * PRINT: un valore numerico conserva la zona, parità IL/PCode/VM-C). */
int js_extract_emit(ctx_t* c, gmb_expr_t** a, int n, int typed) {   /* §0quater: usata anche dal PRINT in gmb_c99_emit.c */
    if (n < 2) return 0;
    sb_put(&c->out, typed ? "gmb_json_extract_val((" : "gmb_string_alloc(gmb_json_extract((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]);
    sb_put(&c->out, ").data)");
    if (!typed) sb_put(&c->out, ")");
    return 1;
}
static int js_extract(ctx_t* c, gmb_expr_t** a, int n) { return js_extract_emit(c, a, n, 0); }
/* JSON.READSECTION(file$, sezione$): sezione INI/JSON -> dict (runtime condiviso
 * gmb_ini_section_to_dict_val, stessa fonte della VM/C e dell'emitter C# §0bis).
 * Prima mancava del tutto ("simbolo non trovato"). */
/* JSON.KEYS(v) — chiavi di un oggetto JSON (val boxata o stringa JSON).
 * Prima mancava del tutto sul C99Native ("simbolo non trovato: JSON.KEYS").
 * KEYS è un TokenKind CONDIVISO (dict.Keys/collections): il handler si attiva solo per
 * una val boxata o una stringa JSON, altrimenti lascia il dispatch generico. */
static int js_keys(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || !a[0]) return 0;
    var_t* kv = (a[0]->kind == GMB_EXPR_VAR && a[0]->var) ? lookup_var(c, a[0]->var) : NULL;
    int is_val = (kv && kv->boxed_val) || a[0]->kind == GMB_EXPR_CALL;
    int is_str = (etype(c, a[0]) == TY_STR);
    if (!is_val && !is_str) return 0;
    sb_put(&c->out, "gmb_val_dict_keys(");
    if (is_str && !(kv && kv->boxed_val)) {
        /* stringa JSON (var/letterale, non una call): emit_expr, NON emit_str_operand che
         * ri-dispatcha la builtin (ricorsione: gmb_json_parse(gmb_val_dict_keys(...).data)). */
        sb_put(&c->out, "gmb_json_parse(");
        emit_expr(c, a[0]);
        sb_put(&c->out, ".data)");
    } else emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int js_readsection(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { c99_fail(c, 0, "JSON.READSECTION richiede (sezione) o (file, sezione) — backend C99Native"); return 1; }
    sb_put(&c->out, "gmb_ini_section_to_dict_val(");
    if (n >= 2) {
        /* (file, sezione) */
        emit_str_operand(c, a[0]);
        sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]);
    } else {
        /* (sezione) → file di default (canone IlRuntime.ReadSection: CWD/app.settings.json) */
        sb_put(&c->out, "gmb_string_alloc(\"app.settings.json\").data, ");
        emit_str_operand(c, a[0]);
    }
    sb_put(&c->out, ".data)");
    return 1;
}
/* JSON.READVALUE — arità canoniche (IlRuntime.ReadValue / PCodeProcessor.Builtins):
 *   (file, section, key[, def]) → dal file indicato
 *   (key)                       → app.settings.json, sezione "", def ""
 * Prima era uno STUB silenzioso (`gmb_string_alloc("")`, §0bis) → sempre vuoto
 * (minitests/json/readvalue.bas: def=|val=|kv=). */
static int js_readvalue(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_ini_read_value(");
    if (n >= 3) {
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
        if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data"); }
        else sb_put(&c->out, "gmb_string_alloc(\"\").data");
    } else {
        sb_put(&c->out, "gmb_string_alloc(\"app.settings.json\").data, gmb_string_alloc(\"\").data, ");
        emit_str_operand(c, a[0]);
        sb_put(&c->out, ".data, gmb_string_alloc(\"\").data");
    }
    sb_put(&c->out, "))");
    return 1;
}

/* JSON.TOOBJECT(Tipo, json): ricostruisce la struct UDT copiando i campi dal dict JSON
 * (parità emitter C# EmitJsonToObject / PCodeVM JsonElementToObject). Il 1o argomento è il
 * NOME DEL TIPO (identificatore, non un valore), il 2o la sorgente JSON (in C99Native è una
 * stringa: JSON.TOJSON/JSON.Load). Campi array restano a zero. Prima: "simbolo non trovato"
 * (nessuna emissione) — il caso UDT di JSON.TOOBJECT mancava sul C99Native. */
static int js_to_object(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) { c99_fail(c, 0, "JSON.TOOBJECT richiede (Tipo, json) — backend C99Native"); return 1; }
    const char* tname = (a[0] && a[0]->var) ? a[0]->var : NULL;
    struct cls_info* ci = NULL;
    for (int i = 0; tname && i < c->n_classes; i++)
        if (c->classes[i].is_udt && strcmp(c->classes[i].name, tname) == 0) { ci = &c->classes[i]; break; }
    /* sorgente JSON: in C99Native una variabile JSON è BOXATA (`gmb_val_t`, es.
     * `gmb_val_t j = ...`) → va estratta con gmb_val_json_text(...).data; una stringa
     * diretta (JSON.TOJSON(...) inline, JSON.Load) ha già `.data`. Il flag boxed_val della
     * var è l'unico affidabile (etype può dire TY_STR mentre il C type è gmb_val_t). */
    int src_is_str = 1;
    if (a[1]->kind == GMB_EXPR_VAR && a[1]->var) {
        var_t* sv = lookup_var(c, a[1]->var);
        if (sv && sv->boxed_val) src_is_str = 0;
    } else if (etype(c, a[1]) != TY_STR) {
        src_is_str = 0;
    }
    if (ci == NULL) {
        /* Non-UDT. Il parser C appiattisce "LIST OF <T>" al solo nome dell'elemento, quindi
         * il tipo-argomento NON distingue lista da boxato: decide il TARGET dell'assegnamento
         * (cur_assign_target, popolato da emit_assignment). Target LISTA (default, parità
         * PCodeVM/IL) -> gmb_val_as_list(...); target BOXED (gmb_val_t, es.
         * `DIM x AS JSON`) -> gmb_json_parse(...) diretto (un gmb_list_t non è assegnabile a
         * un gmb_val_t). */
        int tboxed = 0;
        if (c->cur_assign_target) {
            var_t* tv = lookup_var(c, c->cur_assign_target);
            if (tv && tv->boxed_val && !tv->gkind && !tv->vclass[0]) tboxed = 1;
        }
        if (!tboxed) sb_put(&c->out, "gmb_val_as_list(");
        sb_put(&c->out, "gmb_json_parse(");
        if (src_is_str) { sb_put(&c->out, "("); emit_expr(c, a[1]); sb_put(&c->out, ").data"); }
        else { sb_put(&c->out, "gmb_val_json_text("); emit_expr(c, a[1]); sb_put(&c->out, ").data"); }
        sb_put(&c->out, ")");
        if (!tboxed) sb_put(&c->out, ")");
        return 1;
    }
    if (ci->nfields <= 0) {
        c99_fail(c, 0, "JSON.TOOBJECT: UDT '%s' senza campi (backend C99Native)", ci->name);
        return 1;
    }
    sb_put(&c->out, "({ gmb_val_t _jv = gmb_json_parse(");
    if (src_is_str) { sb_put(&c->out, "("); emit_expr(c, a[1]); sb_put(&c->out, ").data"); }
    /* val boxato (JSON.PARSE → dict, TOJSON → stringa col testo JSON): gmb_val_json_text
     * ritorna il TESTO JSON in entrambi i casi (una stringa non va virgolettata). */
    else { sb_put(&c->out, "gmb_val_json_text("); emit_expr(c, a[1]); sb_put(&c->out, ").data"); }
    sb_printf(&c->out, "); gmb_obj_%s _u = (gmb_obj_%s){0}; ", ci->name, ci->name);
    for (int fi = 0; fi < ci->nfields; fi++) {
        const char* ft = ci->ftype[fi] ? ci->ftype[fi] : "NUMERIC";
        /* Campo LIST OF <Elem>: costruisce una lista di UDT elemento dal vettore JSON. */
        if (strncasecmp(ft, "LIST OF ", 8) == 0) {
            const char* elem = ft + 8;
            struct cls_info* ei = NULL;
            for (int k = 0; elem && elem[0] && k < c->n_classes; k++)
                if (c->classes[k].is_udt && strcmp(c->classes[k].name, elem) == 0) { ei = &c->classes[k]; break; }
            if (ei) {
                sb_printf(&c->out, "_u.%s = gmb_list_new(); ", ci->fname[fi]);
                sb_printf(&c->out, "{ gmb_list_t _fl_%s = gmb_val_as_list(gmb_val_dict_get(_jv, \"%s\")); ",
                          ci->fname[fi], ci->fname[fi]);
                sb_printf(&c->out, "for (gmb_int_t _fi_%s = 0; _fi_%s < (gmb_int_t)_fl_%s.count; _fi_%s++) { ",
                          ci->fname[fi], ci->fname[fi], ci->fname[fi], ci->fname[fi]);
                sb_printf(&c->out, "gmb_val_t _e_%s = _fl_%s.items[_fi_%s]; ", ci->fname[fi], ci->fname[fi], ci->fname[fi]);
                sb_printf(&c->out, "gmb_obj_%s _le_%s = (gmb_obj_%s){0}; ", elem, ci->fname[fi], elem);
                for (int j = 0; j < ei->nfields; j++) {
                    const char* eft = ei->ftype[j] ? ei->ftype[j] : "NUMERIC";
                    if (strncasecmp(eft, "LIST OF ", 8) == 0) continue;   /* liste annidate: non gestite qui */
                    sb_printf(&c->out, "_le_%s.%s = ", ci->fname[fi], ei->fname[j]);
                    if (strncasecmp(eft, "STRING", 6) == 0)
                        sb_printf(&c->out, "gmb_val_json_text(gmb_val_dict_get(_e_%s, \"%s\")); ", ci->fname[fi], ei->fname[j]);
                    else if (strncasecmp(eft, "BIT", 3) == 0 || strncasecmp(eft, "BOOL", 4) == 0)
                        sb_printf(&c->out, "(gmb_bool_t)gmb_val_to_double(gmb_val_dict_get(_e_%s, \"%s\")); ", ci->fname[fi], ei->fname[j]);
                    else if (strncasecmp(eft, "LONG", 4) == 0 || strncasecmp(eft, "INTEGER", 7) == 0)
                        sb_printf(&c->out, "(gmb_int_t)gmb_val_to_double(gmb_val_dict_get(_e_%s, \"%s\")); ", ci->fname[fi], ei->fname[j]);
                    else
                        sb_printf(&c->out, "gmb_val_to_double(gmb_val_dict_get(_e_%s, \"%s\")); ", ci->fname[fi], ei->fname[j]);
                }
                sb_printf(&c->out, "gmb_list_add(&_u.%s, gmb_make_val_obj(gmb_udt_box(&_le_%s, sizeof(_le_%s), \"%s\"))); } } ",
                          ci->fname[fi], ci->fname[fi], ci->fname[fi], elem);
            } else {
                /* LIST non-UDT: copia la lista boxata */
                sb_printf(&c->out, "_u.%s = gmb_val_as_list(gmb_val_dict_get(_jv, \"%s\")); ", ci->fname[fi], ci->fname[fi]);
            }
            continue;
        }
        /* Campo ARRAY (`INTEGER[1]`, `STRING[N]`): alloca il buffer e popola dal vettore JSON
         * (il campo è un puntatore `gmb_double_t*`/`gmb_string_t*`). Prima cadeva nel ramo
         * scalare → `_u.Ports = (gmb_int_t)gmb_val_to_double(...)` (int → double*, errore cc). */
        if (strchr(ft, '[') != NULL) {
            int _is_str = (strncasecmp(ft, "STRING", 6) == 0);
            const char* _ct = _is_str ? "gmb_string_t" : "gmb_double_t";
            sb_printf(&c->out, "{ gmb_list_t _fa_%s = gmb_val_as_list(gmb_val_dict_get(_jv, \"%s\")); ",
                      ci->fname[fi], ci->fname[fi]);
            sb_printf(&c->out, "_u.%s = (%s*)calloc(_fa_%s.count ? _fa_%s.count : 1, sizeof(%s)); ",
                      ci->fname[fi], _ct, ci->fname[fi], ci->fname[fi], _ct);
            sb_printf(&c->out, "for (gmb_int_t _fk_%s = 0; _fk_%s < (gmb_int_t)_fa_%s.count; _fk_%s++) ",
                      ci->fname[fi], ci->fname[fi], ci->fname[fi], ci->fname[fi]);
            if (_is_str)
                sb_printf(&c->out, "_u.%s[_fk_%s] = gmb_val_json_text(_fa_%s.items[_fk_%s]); } ",
                          ci->fname[fi], ci->fname[fi], ci->fname[fi], ci->fname[fi]);
            else
                sb_printf(&c->out, "_u.%s[_fk_%s] = gmb_val_to_double(_fa_%s.items[_fk_%s]); } ",
                          ci->fname[fi], ci->fname[fi], ci->fname[fi], ci->fname[fi]);
            continue;
        }
        sb_printf(&c->out, "_u.%s = ", ci->fname[fi]);
        if (strncasecmp(ft, "STRING", 6) == 0)
            sb_printf(&c->out, "gmb_val_json_text(gmb_val_dict_get(_jv, \"%s\")); ", ci->fname[fi]);
        else if (strncasecmp(ft, "BIT", 3) == 0 || strncasecmp(ft, "BOOL", 4) == 0)
            sb_printf(&c->out, "(gmb_bool_t)gmb_val_to_double(gmb_val_dict_get(_jv, \"%s\")); ", ci->fname[fi]);
        else if (strncasecmp(ft, "LONG", 4) == 0 || strncasecmp(ft, "INTEGER", 7) == 0)
            sb_printf(&c->out, "(gmb_int_t)gmb_val_to_double(gmb_val_dict_get(_jv, \"%s\")); ", ci->fname[fi]);
        else
            sb_printf(&c->out, "gmb_val_to_double(gmb_val_dict_get(_jv, \"%s\")); ", ci->fname[fi]);
    }
    /* Il valore UDT in C99Native è BOXATO (l'assegnazione usa gmb_obj_copy_typed(val,...)):
     * la struct va copiata su heap e avvolta come oggetto type_name (gmb_udt_box). */
    sb_printf(&c->out, "gmb_make_val_obj(gmb_udt_box(&_u, sizeof(_u), \"%s\")); })", ci->name);
    return 1;
}
/* JSON.Stringify(v) / JSON.TOJSON(v) — parità emitter C# (EmitJsonStringify) e PCodeVM:
 * v = array nativo (gmb_double_t / gmb_string_t elementi) → runtime gmb_json_double_array
 *     o gmb_json_string_array; v = stringa scalare → stringa JSON virgolettata (via
 *     gmb_val_to_json di un val string); v = numerico scalare → numero JSON nudo. */
int js_stringify(ctx_t* c, gmb_expr_t** a, int n) {   /* §0quater: usata anche da st_json_writesection (gmb_c99_emit.c) */
    if (n < 1) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    gmb_expr_t* arg = a[0];
    /* JSON.KEYS(v) (forma builtin, non membro): gmb_val_dict_keys ritorna gmb_list_t →
     * boxa in val per la serializzazione (come la forma membro X.Keys()). */
    /* NIL → "null" (parità IL/PCode: JSON.STRINGIFY(NIL) == "null"; prima usciva 0). */
    if (arg && arg->kind == GMB_EXPR_VAR && arg->var && strcasecmp(arg->var, "NIL") == 0) {
        sb_put(&c->out, "gmb_string_alloc(\"null\")");
        return 1;
    }
    /* Contenitore TIPIZZATO a handle (HEAP/MAXHEAP/BAG/TRIE): il valore lo produce il dispatch
     * canonico per TokenKind (runtime condiviso) e si serializza diretto. Senza, i rami per
     * NOME qui sotto lo trattavano come gmb_list_t (JSON.Stringify(B.UniqueItems()) →
     * gmb_list_unique_copy(B); TR.FindAll("ca") → gmb_list_find_all(TR, …)) = errori C. */
    if (arg && arg->kind == GMB_EXPR_CALL && arg->var) {
        const char* hd = strchr(arg->var, '.');
        if (hd && hd != arg->var) {
            char hp[160]; size_t hpl = (size_t)(hd - arg->var);
            if (hpl < sizeof(hp)) {
                memcpy(hp, arg->var, hpl); hp[hpl] = 0;
                if (gmb_c99_coll_handle_supports(c, hp, hd + 1, arg->nargs)) {
                    sb_put(&c->out, "gmb_val_to_json(");
                    gmb_c99_coll_handle_method(c, hp, hd + 1, arg->args, arg->nargs, arg->line);
                    sb_put(&c->out, ")");
                    return 1;
                }
            }
        }
    }
    /* letterale lista `[e1, ..., en]` (il parser lo rappresenta come CALL "_array_"): emit_expr
     * lo materializza come gmb_val_t (gmb_make_val_list) → serializza diretto. Prima cadeva nel
     * fallback (3) numerico → `gmb_double_to_string((gmb_double_t)(<val>))` = errore cc. */
    if (arg && arg->kind == GMB_EXPR_CALL && arg->var && strcmp(arg->var, "_array_") == 0) {
        sb_put(&c->out, "gmb_val_to_json(");
        emit_expr(c, arg);
        sb_put(&c->out, ")");
        return 1;
    }
    /* JSON.STRINGIFY(JSON.KEYS(<stringa>)): KEYS su una stringa non è applicabile → "null"
     * (parità IL/PCode; prima serializzava le chiavi del JSON parsato). */
    if (arg && arg->kind == GMB_EXPR_CALL && arg->tk == GMB_TOK_Keys && arg->nargs >= 1
        && arg->args[0] && etype(c, arg->args[0]) == TY_STR
        && !(arg->args[0]->kind == GMB_EXPR_VAR && arg->args[0]->var
             && lookup_var(c, arg->args[0]->var) && lookup_var(c, arg->args[0]->var)->boxed_val)) {
        sb_put(&c->out, "gmb_string_alloc(\"null\")");
        return 1;
    }
    if (arg && arg->kind == GMB_EXPR_CALL && arg->tk == GMB_TOK_Keys && arg->nargs >= 1) {
        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(");
        js_keys(c, arg->args, arg->nargs);   /* argomenti INTERNI della call, non la call */
        sb_put(&c->out, "))");
        return 1;
    }
    /* (0) X.Keys()/X.Values() (dict/sorted) e X.ToArray()/X.ToList() (list) /
     *     X.ToDict()/X.ToJson() (dict) → serializzazione del contenuto boxed. */
    if (arg && arg->kind == GMB_EXPR_CALL && arg->var && arg->nargs == 0) {
        const char* kd = strchr(arg->var, '.');
        if (kd && kd != arg->var) {
            size_t kl = (size_t)(kd - arg->var);
            char kp[160];
            if (kl < sizeof(kp)) {
                memcpy(kp, arg->var, kl); kp[kl] = 0;
                var_t* kv = lookup_var(c, kp);
                if (kv && kv->gkind) {
                    int ck = gmb_c99_var_coll_kind(c, kp);
                    int mk = (int)gmb_member_tok(kd + 1);
                    if (ck == 2 && (mk == GMB_TOK_Keys || mk == GMB_TOK_Values)) {
                        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(gmb_dict_");
                        if (mk == GMB_TOK_Values) {
                            if (gmb_c99_var_coll_sorted(c, kp)) sb_put(&c->out, "values_sorted(");
                            else sb_put(&c->out, "values(");
                        } else {
                            if (gmb_c99_var_coll_sorted(c, kp)) sb_put(&c->out, "keys_sorted(");
                            else sb_put(&c->out, "keys(");
                        }
                        emit_ident(c, kv->name);
                        sb_put(&c->out, ")))");
                        return 1;
                    }
                    if (ck == 1 && (mk == GMB_TOK_ToArray || mk == GMB_TOK_ToList
                                    || mk == GMB_TOK_UniqueItems)) {
                        if (mk == GMB_TOK_UniqueItems) {
                            sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(gmb_list_unique_copy(");
                            emit_ident(c, kv->name);
                            sb_put(&c->out, ")))");
                        } else {
                            sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(");
                            emit_ident(c, kv->name);
                            sb_put(&c->out, "))");
                        }
                        return 1;
                    }
                    /* NB: `ToJson()` NON entra qui. Ritorna il TESTO JSON (gmb_string_t,
                     * parità IL/PCode/VM/C: TYPENAME=STRING) → JSON.STRINGIFY(d.ToJson()) è la
                     * stringa RE-CODIFICATA (quotata), non la serializzazione dell'oggetto.
                     * Prima C99 trattava ToJson come ToDict e stampava l'oggetto nudo
                     * (Collections/CollectionsDictLists: "Original ToJson" senza virgolette). */
                    if (ck == 2 && mk == GMB_TOK_ToDict) {
                        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_dict(");
                        emit_ident(c, kv->name);
                        sb_put(&c->out, "))");
                        return 1;
                    }
                    if (ck == 2 && mk == GMB_TOK_ToArray) {
                        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(gmb_dict_to_kvp(");
                        emit_ident(c, kv->name);
                        sb_put(&c->out, ")))");
                        return 1;
                    }
                    if (ck == 2 && (mk == GMB_TOK_Min || mk == GMB_TOK_Max
                                    || mk == GMB_TOK_ExtractMin || mk == GMB_TOK_ExtractMax)) {
                        int ismax = mk == GMB_TOK_Max || mk == GMB_TOK_ExtractMax;
                        int rmv = mk == GMB_TOK_ExtractMin || mk == GMB_TOK_ExtractMax;
                        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(gmb_dict_extreme(&");
                        emit_ident(c, kv->name);
                        sb_printf(&c->out, ", %d, %d)))", ismax ? 1 : 0, rmv ? 1 : 0);
                        return 1;
                    }
                }
            }
        }
    }
    /* (0b) X.FindAll(...) su lista-stringhe → JSON dell'elenco filtrato */
    if (arg && arg->kind == GMB_EXPR_CALL && arg->var && arg->nargs == 1) {
        const char* fd = strchr(arg->var, '.');
        if (fd && fd != arg->var && gmb_member_tok(fd + 1) == GMB_TOK_FindAll) {
            size_t fl = (size_t)(fd - arg->var);
            char fp[160];
            if (fl < sizeof(fp)) {
                memcpy(fp, arg->var, fl); fp[fl] = 0;
                var_t* fv = lookup_var(c, fp);
                if (fv && fv->gkind && gmb_c99_var_coll_kind(c, fp) == 1) {
                    sb_put(&c->out, "gmb_val_to_json(gmb_make_val_list(gmb_list_find_all(");
                    emit_ident(c, fv->name);
                    sb_put(&c->out, ", (");
                    emit_expr(c, arg->args[0]);
                    sb_put(&c->out, ").data)))");
                    return 1;
                }
            }
        }
    }
    /* (0c) JSON.Stringify(var AS JSON) → gmb_val_to_json(boxed) */
    if (arg && arg->kind == GMB_EXPR_VAR && arg->var) {
        var_t* bv = lookup_var(c, arg->var);
        if (bv && bv->boxed_val && !bv->gkind && !bv->vclass[0]) {
            sb_put(&c->out, "gmb_val_to_json(");
            emit_ident(c, bv->name);
            sb_put(&c->out, ")");
            return 1;
        }
    }
    /* (0d) base.membro su var BOXED (AnalysisResult/JSON): il membro può essere un ARRAY
     * (KMeans.Centroids / HoltWinters.Forecast/Coefficients…): serializza il val GREZZO del
     * membro. Prima cadeva nel fallback (3): emit_expr lo coerciva a double (membro array →
     * 0) e JSON.Stringify stampava "0" invece dell'array. */
    if (arg && arg->kind == GMB_EXPR_VAR && arg->var) {
        const char* dot = strchr(arg->var, '.');
        if (dot && dot != arg->var && !strchr(dot + 1, '.')) {
            char pre[160]; size_t pl = (size_t)(dot - arg->var);
            if (pl < sizeof(pre)) {
                memcpy(pre, arg->var, pl); pre[pl] = 0;
                var_t* bbv = lookup_var(c, pre);
                if (bbv && bbv->boxed_val) {
                    sb_put(&c->out, "gmb_val_to_json(gmb_dict_get(gmb_val_as_dict(");
                    emit_ident(c, bbv->name);
                    sb_put(&c->out, "), \"");
                    sb_put(&c->out, dot + 1);
                    sb_put(&c->out, "\"))");
                    return 1;
                }
            }
        }
    }
    /* (1) variabile array nativa (o `arr()` CALL 0-arg) */
    const char* aname = NULL;
    if (arg->kind == GMB_EXPR_VAR) aname = arg->var;
    else if (arg->kind == GMB_EXPR_CALL && arg->var && arg->nargs == 0) aname = arg->var;
    if (aname) {
        var_t* av = lookup_var(c, aname);
        /* collections tipizzate (LIST/DICT/QUEUE/STACK/HASHSET/SORTED) → gmb_val_to_json
         * su val boxed: il writer gmb_json_append_val serializza lista/oggetto. */
        if (av && av->gkind && gmb_c99_emit_coll_json(c, aname)) return 1;
        if (av && av->is_array && !av->matrix) {
            int is_str = av->type == TY_STR || av->elem_str;
            sb_put(&c->out, is_str ? "gmb_json_string_array(" : "gmb_json_double_array(");
            emit_ident(c, av->name);
            sb_put(&c->out, ", (gmb_int_t)(");
            emit_arr_len_of(c, av);
            sb_put(&c->out, "))");
            return 1;
        }
    }
    /* (2) stringa scalare → JSON string virgolettata */
    if (etype(c, arg) == TY_STR) {
        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_string(");
        emit_str_operand(c, arg);
        sb_put(&c->out, ".data))");
        return 1;
    }
    /* (2b) variabile UDT (TYPE ... END TYPE): boxa la struct e serializza il val
     * (gmb_val_to_json usa il layout UDT registrato a boot). Prima cadeva nel fallback (3)
     * → gmb_double_to_string((gmb_double_t)(u)) = errore di compilazione C (bug reale).
     * NB: come il ramo (1), una variabile può arrivare come VAR o come CALL 0-arg. */
    {
        const char* uname = NULL;
        if (arg->kind == GMB_EXPR_VAR) uname = arg->var;
        else if (arg->kind == GMB_EXPR_CALL && arg->var && arg->nargs == 0) uname = arg->var;
        if (uname) {
            var_t* uv = lookup_var(c, uname);
            /* Il tipo può non essere su QUESTA entry (doppia registrazione di scope: la var
             * risolta qui può essere quella vuota) → fallback sulla mappa nome→tipo UDT
             * popolata al DIM (dove cls_find risolve il tipo). */
            const char* utype = (uv && uv->vclass[0]) ? uv->vclass : NULL;
            char utype_buf[40];
            if (!utype && uv && gmb_udt_var_type(c, uv->name, utype_buf, sizeof(utype_buf)))
                utype = utype_buf;
            if (utype && uv) {
                /* In C99Native una var UDT è GIÀ un gmb_val_t boxato (gmb_obj_<T> nel val):
                 * basta serializzarla — nessun gmb_udt_box (che copierebbe il val, non la
                 * struct → garbage/segfault). Ritorna una STRINGA JSON come gli altri rami. */
                sb_put(&c->out, "gmb_val_to_json(");
                emit_ident(c, uv->name);
                sb_put(&c->out, ")");
                return 1;
            }
        }
    }
    /* (2d) CALL a un metodo GRAPH che ritorna un VAL (BFS/DFS/Dijkstra/GetNeighbors):
     * emit_expr produce il val boxato → serializzalo (prima cadeva nel fallback (3)
     * gmb_double_to_string → errore di compilazione C). */
    if (arg->kind == GMB_EXPR_CALL && arg->var) {
        const char* gdot = strchr(arg->var, '.');
        if (gdot && gdot != arg->var) {
            gmb_tokenkind_t gmk = gmb_member_tok(gdot + 1);
            if (gmk == GMB_TOK_CollBFS || gmk == GMB_TOK_CollDFS
                || gmk == GMB_TOK_CollDijkstra || gmk == GMB_TOK_CollGetNeighbors) {
                sb_put(&c->out, "gmb_val_to_json(");
                emit_expr(c, arg);
                sb_put(&c->out, ")");
                return 1;
            }
        }
    }
    /* (2e) TRUE/FALSE (sentinelle `_true_`/`_false_`, o i nomi letterali): valore BOOL, non
     * numero — serializzato `true`/`false` come in IL/PCode. Il fallback (3) sotto emetteva
     * `gmb_double_to_string(-1)` = "-1" (e per `JSON.STRINGIFY(o("k"))` perfino un cast
     * double su un gmb_val_t = C invalido). Vedi
     * Bugs/(solved) - JSON-Stringify-Scalare-Diverge-Tra-Gambe.md. */
    if (arg && ((arg->kind == GMB_EXPR_VAR && arg->var
                 && (strcasecmp(arg->var, "TRUE") == 0 || strcasecmp(arg->var, "_true_") == 0
                     || strcasecmp(arg->var, "FALSE") == 0 || strcasecmp(arg->var, "_false_") == 0))
                || (arg->kind == GMB_EXPR_CALL && arg->var && arg->nargs == 0
                    && (strcmp(arg->var, "_true_") == 0 || strcmp(arg->var, "_false_") == 0)))) {
        sb_put(&c->out, "gmb_val_to_json(gmb_make_val_bool((");
        emit_expr(c, arg);
        sb_put(&c->out, ")))");
        return 1;
    }
    /* (2f) espressione che rende un gmb_val_t GREZZO (accesso dinamico `d("k")` su un oggetto
     * JSON, getter RDD/dinamico, call che ritorna val): si serializza il VAL. Il fallback (3)
     * sotto emetteva `gmb_double_to_string((gmb_double_t)(<val>))` = C INVALIDO, quindi
     * `JSON.STRINGIFY(d("k"))` non compilava (cast double su una struct gmb_val).
     * NB: `TY_VAL` non esiste in questo TU (l'enum condiviso ha solo TY_NUM/TY_STR): il valore 2
     * è esposto come `BF_TY_VAL` (vedi la testa del file). Scriverlo col nome dell'enum di
     * gmb_c99_emit.c faceva FALLIRE la compilazione dell'emitter (`use of undeclared identifier`),
     * fermando anche il driver e quindi tutta la gamba C99. */
    if (etype(c, arg) == BF_TY_VAL) {
        sb_put(&c->out, "gmb_val_to_json(");
        emit_expr(c, arg);
        sb_put(&c->out, ")");
        return 1;
    }
    /* (3) numerico scalare → numero JSON (niente virgolette) */
    sb_put(&c->out, "gmb_double_to_string((gmb_double_t)(");
    emit_expr(c, arg);
    sb_put(&c->out, "))");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } js_row;
static const js_row JSON_TBL[] = {
    { GMB_TOK_Extract, js_extract }, { GMB_TOK_ReadValue, js_readvalue },
    { GMB_TOK_Stringify, js_stringify }, { GMB_TOK_ObjectToJson, js_stringify },
    { GMB_TOK_JsonToObject, js_to_object },
    { GMB_TOK_ReadSection, js_readsection },
    { GMB_TOK_Keys, js_keys },
};
int emit_bf_json(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(JSON_TBL) / sizeof(JSON_TBL[0]); i++)
        if (JSON_TBL[i].tk == tk) return JSON_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: conversions (delegazione §0quater) ── */
static int cv_hex(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_hex((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int cv_oct(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_oct((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int cv_bin(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_str_bin((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int cv_cbyte(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_cbyte("); emit_expr(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int cv_cstr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* campo BIT/BOOL di un UDT (`rec.Attivo`): il bool BASIC -1/0 → "True"/"False"
     * (parità C# Conversions.CStr(bool); gmb_str_basic(-1) dava "-1"). */
    if (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        const char* dot = strchr(a[0]->var, '.');
        if (dot && dot != a[0]->var && !strchr(dot + 1, '.')) {
            char pre[160]; size_t pl = (size_t)(dot - a[0]->var);
            if (pl < sizeof pre) {
                memcpy(pre, a[0]->var, pl); pre[pl] = 0;
                var_t* bv = lookup_var(c, pre);
                struct cls_info* ci = (bv && bv->vclass[0]) ? cls_find(c, bv->vclass) : NULL;
                if (ci) {
                    for (int i = 0; i < ci->nfields; i++)
                        if (strcasecmp(ci->fname[i], dot + 1) == 0
                            && (strncasecmp(ci->ftype[i], "BIT", 3) == 0
                                || strncasecmp(ci->ftype[i], "BOOL", 4) == 0)) {
                            sb_put(&c->out, "(("); emit_expr(c, a[0]);
                            sb_put(&c->out, ") ? gmb_string_alloc(\"True\") : gmb_string_alloc(\"False\"))");
                            return 1;
                        }
                }
            }
        }
    }
    /* VARIABILE BIT/BOOL (`DIM isValid AS BIT`): CSTR → "True"/"False" (gmb_str_basic(-1)
     * dava "-1"/" 1"). Parità IL/PCode/VM-C (§ DbLedgerFullTests: LEDGER VALIDATE). */
    if (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        var_t* bv = lookup_var(c, a[0]->var);
        if (bv && bv->is_bit) {
            sb_put(&c->out, "("); emit_expr(c, a[0]);
            sb_put(&c->out, " ? gmb_string_alloc(\"True\") : gmb_string_alloc(\"False\"))");
            return 1;
        }
    }
    /* letterale booleano TRUE/FALSE (parsed come call "_true_"/"_false_"): CSTR → "True"/"False"
     * (gmb_str_basic(-1) dava "-1"). Parità IL/PCode/VM-C. */
    if (a[0] && a[0]->kind == GMB_EXPR_CALL && a[0]->var
        && (strcmp(a[0]->var, "_true_") == 0 || strcmp(a[0]->var, "_false_") == 0)) {
        sb_put(&c->out, a[0]->var[1] == 't' ? "gmb_string_alloc(\"True\")" : "gmb_string_alloc(\"False\")");
        return 1;
    }
    /* Campo di un elemento JSON (`arr(i).Amount`): il valore è un gmb_val_t di tipo
     * ignoto a compile-time; CSTR va reso col canone (numerico → spazio se >= 0, bool →
     * True/False) via gmb_cstr_val sul VALORE RAW (gmb_json_index_member_val). Prima il
     * testo (gmb_json_index_member_text) usciva grezzo, senza lo spazio del canone. */
    if (a[0] && is_json_index_member_call(c, a[0])) {
        const char* dot = strchr(a[0]->var, '.');
        char base[160]; size_t bl = (size_t)(dot - a[0]->var);
        if (bl < sizeof(base)) {
            memcpy(base, a[0]->var, bl); base[bl] = 0;
            var_t* bv = lookup_var(c, base);
            if (bv) {
                sb_put(&c->out, "gmb_cstr_val(gmb_json_index_member_val(");
                emit_ident(c, bv->name);
                sb_put(&c->out, ", (gmb_int_t)(");
                emit_expr(c, a[0]->args[0]);
                sb_put(&c->out, "), \"");
                for (const char* p = dot + 1; *p; p++) {
                    if (*p == '"' || *p == '\\') sb_put(&c->out, "\\");
                    sb_putn(&c->out, p, 1);
                }
                sb_put(&c->out, "\"))");
                return 1;
            }
        }
    }
    { int _t = etype(c, a[0]);
      if (_t == TY_STR) emit_expr(c, a[0]);
      /* TY_VAL (campo nudo DBF/RDD): CSTR del val → gmb_cstr_val (stringa → TESTO del campo). */
      else if (_t == BF_TY_VAL) { sb_put(&c->out, "gmb_cstr_val("); emit_expr(c, a[0]); sb_put(&c->out, ")"); }
      else { sb_put(&c->out, "gmb_str_basic("); emit_num_operand(c, a[0]); sb_put(&c->out, ")"); } }
    return 1;
}
/* CLNG/CINT: arrotondamento half-to-even (parity IL Convert.ToInt64/ToInt16 e canonical .NET),
 * NON il cast C (che tronca verso zero). gmb_round(x, 0) = MidpointRounding.ToEven. */
static int cv_tolong(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_double_t)(gmb_int_t)gmb_round((gmb_double_t)("); emit_expr(c, a[0]); sb_put(&c->out, "), 0)"); return 1; }
static int cv_cbool(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "("); emit_expr(c, a[0]); sb_put(&c->out, ") ? -1.0 : 0.0"); return 1; }
/* CCUR/CDBL/CSNG: prima emettevano l'argomento tal quale (identity), e con una STRINGA
 * lasciavano una `gmb_string_t` in contesto numerico → `CCUR("7") = VAL("7")` e
 * `CDBL("12.5") = 12.5` non compilavano ("invalid operands to binary expression
 * ('gmb_string_t' and 'double')"). In C99 i numerici sono già gmb_double_t, quindi il canone
 * di queste tre conversioni è emit_num_operand: identity per i numerici, `gmb_string_to_double`
 * per le stringhe, `gmb_val_to_double` per i val dinamici (campo DBF/RDD, membro JSON). */
static int cv_num_operand(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; emit_num_operand(c, a[0]); return 1; }
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } cv_row;
static const cv_row CV_TBL[] = {
    { GMB_TOK_Hex, cv_hex }, { GMB_TOK_Oct, cv_oct }, { GMB_TOK_Bin, cv_bin },
    { GMB_TOK_CByte, cv_cbyte }, { GMB_TOK_CStr, cv_cstr },
    { GMB_TOK_CLng, cv_tolong }, { GMB_TOK_CInt, cv_tolong }, { GMB_TOK_CSng, cv_num_operand },
    { GMB_TOK_CDbl, cv_num_operand }, { GMB_TOK_CCur, cv_num_operand }, { GMB_TOK_CBOOL, cv_cbool },
};
int emit_bf_conversions(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(CV_TBL) / sizeof(CV_TBL[0]); i++)
        if (CV_TBL[i].tk == tk) return CV_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: math (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; int n; const char* cf; ma_sp sp; } ma_row;
static int ma_emit(ctx_t* c, const ma_row* e, gmb_expr_t** args, int nargs) {
    if (e->sp) return e->sp(c, args, nargs);
    if (nargs < e->n) return 0;
    sb_put(&c->out, e->cf); sb_put(&c->out, "(");
    for (int i = 0; i < e->n; i++) { if (i) sb_put(&c->out, ", "); emit_num_operand(c, args[i]); }
    sb_put(&c->out, ")");
    return 1;
}
static int ma_pi(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "3.14159265358979323846"); return 1; }
static int ma_deg2rad(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "((gmb_double_t)("); emit_expr(c, a[0]); sb_put(&c->out, ") * 0.017453292519943295)"); return 1; }
static int ma_rad2deg(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "((gmb_double_t)("); emit_expr(c, a[0]); sb_put(&c->out, ") * 57.29577951308232)"); return 1; }
/* ATN(y) → atan(y); ATN(y, x) → atan2(y, x): il 2° argomento è OPZIONALE (un solo token
 * GMB_TOK_Atn, niente ATN2). Custom come ma_round: due righe dichiarative sullo stesso
 * token sarebbero irraggiungibili (emit_bf_math prende la PRIMA riga col token). */
static int ma_atn(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || n > 2) return 0;
    if (n == 2) {
        sb_put(&c->out, "atan2("); emit_num_operand(c, a[0]);
        sb_put(&c->out, ", "); emit_num_operand(c, a[1]); sb_put(&c->out, ")");
        return 1;
    }
    sb_put(&c->out, "atan("); emit_num_operand(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
/* RND non accetta argomenti: accettarne uno in silenzio (prima: n <= 1) rendeva RND(1)
 * indistinguibile da RND(). Ritornare 0 = "non gestito" -> errore in compilazione, come
 * su IL/PCode/VM-C. RND(n) -> 1..n e' semantica Clipper, non GMBasic. */
static int ma_rnd(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_rnd()"); return 1; }
static int ma_yield(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_cur() ? (gmb_yield(), 0.0) : 0.0)"); return 1; }
/* CONCURRENCY.SLEEPASYNC(ms): attesa asincrona (park-based). Token condiviso con lo
 * statement SLEEP; qui la forma funzione (usata con AWAIT) ritorna 0.0 dopo l'attesa. */
static int ma_sleepasync(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_sleep_ms((int)("); emit_expr(c, a[0]); sb_put(&c->out, ")), 0.0)");
    return 1;
}
static int ma_format(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_format((gmb_double_t)("); emit_num_operand(c, a[0]);
    sb_put(&c->out, "), "); emit_str_operand(c, a[1]); sb_put(&c->out, ")");
    return 1;
}
static int ma_round(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || n > 2) return 0;
    sb_put(&c->out, "gmb_round("); emit_num_operand(c, a[0]); sb_put(&c->out, ", ");
    if (n == 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
    return 1;
}
/* MIN/MAX/SUM con UN solo argomento VAR array (non matrice): riduzione runtime sull'array
 * (gmb_min_arr/gmb_max_arr/gmb_sum_arr) — parity IL (EmitDoubleParamsBuiltinCall). */
static int ma_arr_reduce(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1 || !a[0] || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!av || !av->is_array || av->matrix || av->vclass[0]) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "("); emit_var_ref(c, av);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "))");
    return 1;
}
/* MATH.MIN/MAX vararg (1..N): fold annidato da sinistra, es. N=3 → fmax(fmax(a0, a1), a2)
 * — un solo argomento è il valore stesso (parity C# Math.Min/Max params e del fold
 * gmb_max(gmb_max(a,b),c) dell'emitter C#). */
static int ma_minmax_fold(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    if (ma_arr_reduce(c, a, n, strcmp(fn, "fmin") == 0 ? "gmb_min_arr" : "gmb_max_arr")) return 1;
    for (int i = 0; i < n - 1; i++) sb_put(&c->out, fn), sb_put(&c->out, "(");
    for (int i = 0; i < n; i++) {
        if (i) sb_put(&c->out, ", ");
        emit_expr(c, a[i]);
        if (i) sb_put(&c->out, ")");
    }
    return 1;
}
static int ma_minmax(ctx_t* c, gmb_expr_t** a, int n) { return ma_minmax_fold(c, a, n, "fmin"); }
static int ma_maxmax(ctx_t* c, gmb_expr_t** a, int n) { return ma_minmax_fold(c, a, n, "fmax"); }
static int ma_polarx(ctx_t* c, gmb_expr_t** a, int n) { if (n != 2) return 0; sb_put(&c->out, "gmb_polarx("); emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ")"); return 1; }
static int ma_polary(ctx_t* c, gmb_expr_t** a, int n) { if (n != 2) return 0; sb_put(&c->out, "gmb_polary("); emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ")"); return 1; }
static int ma_isnum(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_is_numeric(");
    if (etype(c, a[0]) == TY_STR) { emit_str_operand(c, a[0]); sb_put(&c->out, ".data)) ? -1.0 : 0.0)"); }
    else { sb_put(&c->out, "gmb_double_to_string("); emit_expr(c, a[0]); sb_put(&c->out, "))) ? -1.0 : 0.0)"); }
    return 1;
}
static int ma_clamp(ctx_t* c, gmb_expr_t** a, int n) { if (n != 3) return 0; sb_put(&c->out, "gmb_clamp("); emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]); sb_put(&c->out, ")"); return 1; }
static int ma_sumv(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; if (ma_arr_reduce(c, a, n, "gmb_sum_arr")) return 1; sb_put(&c->out, "("); for (int i = 0; i < n; i++) { if (i) sb_put(&c->out, " + "); sb_put(&c->out, "("); emit_expr(c, a[i]); sb_put(&c->out, ")"); } sb_put(&c->out, ")"); return 1; }
/* RANGE(start[, end[, step]]) → lista (gmb_list_range). ParamArray: gli argomenti possono
 * arrivare come lista `_array_` (anche annidata) o sciolti — parity IlRuntime.Range. */
static int ma_range(ctx_t* c, gmb_expr_t** a, int n) {
    gmb_expr_t** vals = a; int nv = n;
    while (nv == 1 && vals[0] && vals[0]->kind == GMB_EXPR_CALL && vals[0]->var
           && strcmp(vals[0]->var, "_array_") == 0) { vals = vals[0]->args; nv = vals[0]->nargs; }
    sb_put(&c->out, "gmb_list_range(");
    if (nv > 0) emit_expr(c, vals[0]); else sb_put(&c->out, "0.0");
    sb_put(&c->out, ", ");
    if (nv > 1) emit_expr(c, vals[1]); else if (nv > 0) emit_expr(c, vals[0]); else sb_put(&c->out, "0.0");
    sb_put(&c->out, ", ");
    if (nv > 2) emit_expr(c, vals[2]); else sb_put(&c->out, "1.0");
    sb_put(&c->out, ")");
    return 1;
}
static const ma_row MATH_TBL[] = {
    { GMB_TOK_Range, 0, NULL, ma_range },
    { GMB_TOK_PolarX, 0, NULL, ma_polarx }, { GMB_TOK_PolarY, 0, NULL, ma_polary },
    { GMB_TOK_IsNumeric, 0, NULL, ma_isnum },
    { GMB_TOK_Clamp, 0, NULL, ma_clamp }, { GMB_TOK_Sum, 0, NULL, ma_sumv },
    { GMB_TOK_Hypot, 2, "gmb_hypot", NULL }, { GMB_TOK_Pythagoras, 2, "gmb_pythagoras", NULL },
    { GMB_TOK_Perm, 2, "gmb_perm", NULL }, { GMB_TOK_Comb, 2, "gmb_comb", NULL },
    { GMB_TOK_Dist2d, 4, "gmb_dist2d", NULL }, { GMB_TOK_Dist3d, 6, "gmb_dist3d", NULL },
    { GMB_TOK_Angle2d, 4, "gmb_angle2d", NULL }, { GMB_TOK_Clamp, 3, "gmb_clamp", NULL },
    { GMB_TOK_Round, 0, NULL, ma_round },
    { GMB_TOK_Sin, 1, "sin", NULL }, { GMB_TOK_Cos, 1, "cos", NULL }, { GMB_TOK_Tan, 1, "tan", NULL },
    { GMB_TOK_Atn, 0, NULL, ma_atn },
    { GMB_TOK_Exp, 1, "exp", NULL }, { GMB_TOK_Log, 1, "log", NULL },
    { GMB_TOK_Pi, 0, NULL, ma_pi },
    { GMB_TOK_Acos, 1, "acos", NULL }, { GMB_TOK_Asin, 1, "asin", NULL },
    { GMB_TOK_Sinh, 1, "sinh", NULL }, { GMB_TOK_Cosh, 1, "cosh", NULL }, { GMB_TOK_Tanh, 1, "tanh", NULL },
    { GMB_TOK_Log10, 1, "log10", NULL },
    { GMB_TOK_Floor, 1, "gmb_floor", NULL }, { GMB_TOK_Trunc, 1, "gmb_trunc", NULL },
    { GMB_TOK_Ceiling, 1, "gmb_ceiling", NULL },
    { GMB_TOK_Deg2Rad, 0, NULL, ma_deg2rad }, { GMB_TOK_Rad2Deg, 0, NULL, ma_rad2deg },
    { GMB_TOK_CByte, 1, "gmb_cbyte", NULL }, { GMB_TOK_Sgn, 1, "gmb_sgn", NULL },
    /* Min/Max sono vararg (MATH.MIN/MAX(1..N valori), parity C# params): emitter
     * dedicato — la riga fmin/fmax binaria scartava gli argomenti dal 3° in poi
     * (MATH.MAX(10,29,33) → 29, bug silenzioso). */
    { GMB_TOK_Min, 0, NULL, ma_minmax },
    { GMB_TOK_Max, 0, NULL, ma_maxmax },
    { GMB_TOK_Fix, 1, "trunc", NULL },
    { GMB_TOK_Rnd, 0, NULL, ma_rnd }, { GMB_TOK_Yield, 0, NULL, ma_yield },
    { GMB_TOK_Sleep, 0, NULL, ma_sleepasync }, { GMB_TOK_Format, 0, NULL, ma_format },
    { GMB_TOK_SpecialBetaInc, 3, "gmb_incbeta", NULL },
    { GMB_TOK_SpecialBetaIncInv, 3, "gmb_betaincinv", NULL }, { GMB_TOK_SpecialBetaLn, 2, "gmb_betaln", NULL },
    { GMB_TOK_SpecialErf, 1, "gmb_erf", NULL }, { GMB_TOK_SpecialErfc, 1, "gmb_erf_c", NULL },
    { GMB_TOK_SpecialErfcInv, 1, "gmb_erfcinv", NULL }, { GMB_TOK_SpecialErfInv, 1, "gmb_erfinv", NULL },
    { GMB_TOK_Fact, 1, "gmb_fact", NULL }, { GMB_TOK_Gamma, 1, "gmb_gamma", NULL },
    { GMB_TOK_SpecialGammaInc, 2, "gmb_gamma_p", NULL }, { GMB_TOK_SpecialGammaIncc, 2, "gmb_gammaincc", NULL },
    { GMB_TOK_SpecialGammaInccInv, 2, "gmb_gammainccinv", NULL }, { GMB_TOK_SpecialGammaIncInv, 2, "gmb_gammaincinv", NULL },
    /* scipy.special P1: LGamma/Digamma/Bessel (canone CommonLib.Numerics.Special; runtime condiviso). */
    { GMB_TOK_SpecialLGamma, 1, "gmb_lgamma", NULL },
    { GMB_TOK_SpecialDigamma, 1, "gmb_digamma", NULL },
    { GMB_TOK_SpecialBesselJ, 2, "gmb_besselj", NULL },
    { GMB_TOK_SpecialBesselY, 2, "gmb_bessely", NULL },
    { GMB_TOK_SpecialBesselI, 2, "gmb_besseli", NULL },
    { GMB_TOK_SpecialBesselK, 2, "gmb_besselk", NULL },
    /* SPECIAL.GAMMA/BETA: i token SpecialGamma/SpecialBeta non erano nella tabella
     * (solo Gamma/Beta di FINANCE) → "simbolo non trovato". */
    { GMB_TOK_SpecialGamma, 1, "gmb_gamma", NULL },
    { GMB_TOK_SpecialBeta, 2, "gmb_beta_fn", NULL },
};
int emit_bf_math(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(MATH_TBL) / sizeof(MATH_TBL[0]); i++)
        if (MATH_TBL[i].tk == tk) return ma_emit(c, &MATH_TBL[i], args, nargs);
    return 0;
}

/* ── dominio: sync (delegazione §0quater) ── */
typedef struct { const char* ns; gmb_tokenkind_t tk; ma_sp sp; } sy_row;
static int sy_atomicinc(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_atomic_inc("); emit_expr(c, a[0]);
    sb_put(&c->out, ", (long long)("); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int sy_atomicfetch(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_atomic_fetch("); emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int sy_atomicstore(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_c99_atomic_store("); emit_expr(c, a[0]);
    sb_put(&c->out, ", (long long)("); emit_expr(c, a[1]); sb_put(&c->out, ")), 0.0)");
    return 1;
}
static int sy_atomiccas(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 3) return 0;
    sb_put(&c->out, "(gmb_bool_t)gmb_c99_atomic_cas("); emit_expr(c, a[0]);
    sb_put(&c->out, ", (long long)("); emit_expr(c, a[1]);
    sb_put(&c->out, "), (long long)("); emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static int sy_atomicswap(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_c99_atomic_swap("); emit_expr(c, a[0]);
    sb_put(&c->out, ", (long long)("); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int sy_poolget(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_c99_pool_get("); emit_expr(c, a[0]);   /* gmb_val_t dinamico (TY_VAL): numero o stringa */
    sb_put(&c->out, "))");
    return 1;
}
static int sy_poolput(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_c99_pool_put("); emit_expr(c, a[0]);
    sb_put(&c->out, ", ");
    if (etype(c, a[1]) == TY_STR) {
        sb_put(&c->out, "gmb_make_val_string(("); emit_str_operand(c, a[1]);
        sb_put(&c->out, ").data)");
    } else {
        sb_put(&c->out, "gmb_make_val_double("); emit_expr(c, a[1]);
        sb_put(&c->out, ")");
    }
    sb_put(&c->out, "), 0.0)");
    return 1;
}
static int sy_smstore(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 3) return 0;
    sb_put(&c->out, "(gmb_syncmap_store("); emit_expr(c, a[0]);
    sb_put(&c->out, ", ("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data, ");
    if (etype(c, a[2]) == TY_STR) {
        sb_put(&c->out, "gmb_make_val_string(("); emit_str_operand(c, a[2]);
        sb_put(&c->out, ").data)");
    } else {
        sb_put(&c->out, "gmb_make_val_double("); emit_expr(c, a[2]);
        sb_put(&c->out, ")");
    }
    sb_put(&c->out, "), 0.0)");
    return 1;
}
static int sy_smloadorstore(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 3) return 0;
    sb_put(&c->out, "gmb_val_str_or_empty(gmb_syncmap_load_or_store("); emit_expr(c, a[0]);
    sb_put(&c->out, ", ("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data, ");
    if (etype(c, a[2]) == TY_STR) {
        sb_put(&c->out, "gmb_make_val_string(("); emit_str_operand(c, a[2]);
        sb_put(&c->out, ").data)");
    } else {
        sb_put(&c->out, "gmb_make_val_double("); emit_expr(c, a[2]);
        sb_put(&c->out, ")");
    }
    sb_put(&c->out, "))");
    return 1;
}
static int sy_smloaddel(ctx_t* c, gmb_expr_t** a, int n, const char* op) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_val_str_or_empty(gmb_syncmap_");
    sb_put(&c->out, op);
    sb_put(&c->out, "("); emit_expr(c, a[0]);
    sb_put(&c->out, ", ("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data))");
    return 1;
}
static int sy_smload(ctx_t* c, gmb_expr_t** a, int n) { return sy_smloaddel(c, a, n, "load"); }
static int sy_smdelete(ctx_t* c, gmb_expr_t** a, int n) { return sy_smloaddel(c, a, n, "delete"); }
static int sy_mtx_trylock(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    var_t* mv = (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var)
                ? lookup_var(c, a[0]->var) : NULL;
    sb_put(&c->out, "(");
    sb_put(&c->out, (mv && mv->gkind == GK_RWMUTEX)
             ? "gmb_grwmutex_trylock(" : "gmb_gmutex_trylock(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static const sy_row SY_TBL[] = {
    { "ATOMIC", GMB_TOK_AtomicIncrement, sy_atomicinc },
    { "ATOMIC", GMB_TOK_AtomicFetchValue, sy_atomicfetch },
    { "ATOMIC", GMB_TOK_AtomicStore, sy_atomicstore },
    { "ATOMIC", GMB_TOK_AtomicCompareSwap, sy_atomiccas },
    { "ATOMIC", GMB_TOK_AtomicSwap, sy_atomicswap },
    /* POOL.GET/PUT → i TokenKind DEDICATI (PoolGet/PoolPut) come li risolve gmb_ns_lookup
     * ((ns,lexeme di registro)); prima la tabella usava Get/Put → POOL.PUT non dispatchato
     * ("POOL.PUT non definita", GpmV2Pool/PCodeGpmPool). */
    { "POOL", GMB_TOK_PoolGet, sy_poolget },
    { "POOL", GMB_TOK_PoolPut, sy_poolput },
    { "SYNCMAP", GMB_TOK_SyncMapStore, sy_smstore },
    { "SYNCMAP", GMB_TOK_SyncMapLoadOrStore, sy_smloadorstore },
    { "SYNCMAP", GMB_TOK_SyncMapLoad, sy_smload },
    { "SYNCMAP", GMB_TOK_SyncMapDelete, sy_smdelete },
    { "MUTEX", GMB_TOK_MutexTryLock, sy_mtx_trylock },
};
int emit_bf_sync(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)line;
    for (size_t i = 0; i < sizeof(SY_TBL) / sizeof(SY_TBL[0]); i++)
        if (SY_TBL[i].ns && ns && !strcasecmp(ns, SY_TBL[i].ns) && SY_TBL[i].tk == tk)
            return SY_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: graphics (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } gf_row;
/* Parità IL/PCode/VMC: metriche testo e screens via runtime condiviso gmb_gfx_* (§0bis). */
static int gf_textwidth(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_gfx_text_width_logical("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int gf_textheight(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_gfx_text_height_logical())");
    return 1;
}
static int gf_width(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_console_buffer_width())");
    return 1;
}
static int gf_height(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_console_buffer_height())");
    return 1;
}
static int gf_screens(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    /* parità IL (Screens()=1), non il conteggio modalità display SDL */
    sb_put(&c->out, "((gmb_double_t)gmb_gfx_screens())");
    return 1;
}
static int gf_scale(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "((gmb_double_t)gmb_gfx_%s((double)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "), (double)(");
    if (n >= 2) emit_expr(c, a[1]);
    else {
        /* size omessa = dimensione del font corrente (canone sample/C2PX docs):
         * colonne → TEXTWIDTH(" "), righe → TEXTHEIGHT (entrambi LOGICI). Prima
         * cadeva a 1 → C2PX(5) = 5px (bug GraphicsCellPixel). */
        int is_col = !strcmp(fn, "c2px") || !strcmp(fn, "p2cx");
        sb_put(&c->out, is_col ? "(double)gmb_gfx_text_width_logical(\" \")" : "(double)gmb_gfx_text_height_logical()");
    }
    sb_put(&c->out, ")))");
    return 1;
}
static int gf_c2px(ctx_t* c, gmb_expr_t** a, int n) { return gf_scale(c, a, n, "c2px"); }
static int gf_c2py(ctx_t* c, gmb_expr_t** a, int n) { return gf_scale(c, a, n, "c2py"); }
static int gf_p2cx(ctx_t* c, gmb_expr_t** a, int n) { return gf_scale(c, a, n, "p2cx"); }
static int gf_p2cy(ctx_t* c, gmb_expr_t** a, int n) { return gf_scale(c, a, n, "p2cy"); }
/* POINT/CELL/CELLSTR/SCREEN/COLLIDE headless — parità IL/PCode/VMC. */
static int gf_point(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) { sb_put(&c->out, "((gmb_double_t)-1)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_gfx_point((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, ")))");
    return 1;
}
static int gf_cell(ctx_t* c, gmb_expr_t** a, int n) {
    sb_put(&c->out, "((gmb_double_t)((");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "0");
    sb_put(&c->out, ") >= 1 ? 0 : 7))");
    return 1;
}
static int gf_cellstr(ctx_t* c, gmb_expr_t** a, int n) { (void)a; (void)n; sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
static int gf_screen(ctx_t* c, gmb_expr_t** a, int n) { (void)a; (void)n; sb_put(&c->out, "((gmb_double_t)0)"); return 1; }
static int gf_collide(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_gfx_collide((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, ")))");
    return 1;
}
static int gf_screeninfo(ctx_t* c, gmb_expr_t** a, int n);
static int gf_collidebatch(ctx_t* c, gmb_expr_t** a, int n);
/* WINDOWS.WIDTH/HEIGHT: headless → -1 (parità IL IlRuntime.WindowWidth/Height). */
static int gf_window_size(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "(-1.0)");
    return 1;
}
/* KEY.MAKE(key, mod) → intero (parità IL IlRuntime.KeyMake / StdLib LongType). */
static int gf_key_make(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "((double)(int)("); emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static const gf_row GF_TBL[] = {
    { GMB_TOK_TextWidth, gf_textwidth }, { GMB_TOK_TextHeight, gf_textheight },
    { GMB_TOK_WindowWidth, gf_window_size }, { GMB_TOK_WindowHeight, gf_window_size },
    { GMB_TOK_Make, gf_key_make },
    { GMB_TOK_WIDTH, gf_width }, { GMB_TOK_HEIGHT, gf_height }, { GMB_TOK_Screens, gf_screens },
    { GMB_TOK_C2PX, gf_c2px }, { GMB_TOK_C2PY, gf_c2py },
    { GMB_TOK_P2CX, gf_p2cx }, { GMB_TOK_P2CY, gf_p2cy },
    { GMB_TOK_Point, gf_point }, { GMB_TOK_Cell, gf_cell }, { GMB_TOK_CellStr, gf_cellstr },
    { GMB_TOK_Screen, gf_screen }, { GMB_TOK_Collide, gf_collide },
    { GMB_TOK_ScreenInfo, gf_screeninfo }, { GMB_TOK_CollideBatch, gf_collidebatch },
};
static int gf_rgb(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "RGB richiede (r,g,b[,a])"); return 0; }
    /* RgbaColor dict {Red,Green,Blue,Alpha} via runtime gmb_rgb (parità IL/PCode/VM-C). */
    sb_put(&c->out, "gmb_rgb((double)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (double)("); emit_expr(c, a[1]);
    sb_put(&c->out, "), (double)("); emit_expr(c, a[2]);
    sb_put(&c->out, "), (double)(");
    if (n >= 4) emit_expr(c, a[3]); else sb_put(&c->out, "255.0");
    sb_put(&c->out, "))");
    return 1;
}
static int gf_screeninfo(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* DisplayMode dict {ModeID,Width,Height,Bpp,Colors,Active} (canone StdLib).
     * Colors/Active mancavano: SCREENINFO().Colors dava 0 e .Active veniva
     * risolto come il builtin AI ACTIVE (cc-error). */
    sb_put(&c->out, "({ gmb_dict_t _dm = gmb_dict_new(); gmb_dict_set(&_dm, \"ModeID\", gmb_make_val_double(0.0)); gmb_dict_set(&_dm, \"Width\", gmb_make_val_double(80.0)); gmb_dict_set(&_dm, \"Height\", gmb_make_val_double(25.0)); gmb_dict_set(&_dm, \"Bpp\", gmb_make_val_double(0.0)); gmb_dict_set(&_dm, \"Colors\", gmb_make_val_double(16.0)); gmb_dict_set(&_dm, \"Active\", gmb_make_val_double(-1.0)); gmb_make_val_dict(_dm); })");
    return 1;
}
static int gf_collidebatch(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* headless: nessuno sprite → nessuna collisione → lista vuota (parità IL/VMC). */
    sb_put(&c->out, "gmb_make_val_list(gmb_list_new())");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } gf2_row;
static const gf2_row GF2_TBL[] = { { GMB_TOK_Rgb, gf_rgb } };
int emit_bf_graphics(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    c99_mark_ui(c);
    (void)line;
    /* ACTIVE() a ZERO argomenti = SCREENINFO().Active. "Active" è keyword: il parser
     * perde la base SCREENINFO() e resta la call ACTIVE (0 arg) → gmb_provider_active()
     * senza ruolo (cc-error). L'ACTIVE del provider AI richiede il ruolo. */
    if (name && strcasecmp(name, "ACTIVE") == 0 && nargs == 0) {
        sb_put(&c->out, "(gmb_double_t)gmb_val_to_double(gmb_dict_get(gmb_val_as_dict(");
        gf_screeninfo(c, NULL, 0);
        sb_put(&c->out, "), \"Active\"))");
        return 1;
    }
    /* SCREENINFO().ModeID/Width/Height/Bpp: il parser produce una call qualificata
     * `SCREENINFO.<membro>` (per questo la forma con `()` non porta il base) → leggi il
     * membro dal dict di SCREENINFO() (parità emitter C#: DisplayMode val → dict member). */
    if ((name && strncasecmp(name, "SCREENINFO.", 11) == 0 && name[11])
        || (ns && strcasecmp(ns, "SCREENINFO") == 0 && name && *name)) {
        /* Qualsiasi membro di SCREENINFO() (ModeID/Width/Height/Bpp/Colors/Active):
         * leggi il campo dal dict. La forma `SCREENINFO().Active` arriva con
         * ns="SCREENINFO" e name="Active" (Active è keyword → dispatch per token,
         * prima diventava il builtin AI ACTIVE con 0 argomenti). */
        const char* m = (name && strncasecmp(name, "SCREENINFO.", 11) == 0 && name[11]) ? name + 11 : name;
        sb_put(&c->out, "(gmb_double_t)gmb_val_to_double(gmb_dict_get(gmb_val_as_dict(");
        gf_screeninfo(c, NULL, 0);
        sb_printf(&c->out, "), \"%s\"))", m);
        return 1;
    }
    /* CHART.CREATE(title$, xlabel$, ylabel$) -> handle (il runtime C ignora i titoli). */
    if (tk == GMB_TOK_ChartCreate ||
        (ns && name && strcasecmp(ns, "CHART") == 0 && strcasecmp(name, "CREATE") == 0)) {
        sb_put(&c->out, "gmb_gfx_chart_create()");
        return 1;
    }
    for (size_t i = 0; i < sizeof(GF_TBL) / sizeof(GF_TBL[0]); i++)
        if (GF_TBL[i].tk == tk && GF_TBL[i].sp(c, args, nargs)) return 1;
    for (size_t i = 0; i < sizeof(GF2_TBL) / sizeof(GF2_TBL[0]); i++)
        if (GF2_TBL[i].tk == tk) return GF2_TBL[i].sp(c, args, nargs, line);
    return 0;
}
/* ── dominio: kmeans (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } km_row;
static int km_kmeans(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) return 0;
    var_t* a0 = NULL;
    if (a[0]->kind != GMB_EXPR_VAR || !(a0 = lookup_var(c, a[0]->var)) || !a0->is_array) {
        c99_fail(c, line, "MULTIVARIATE.KMeans: il primo argomento deve essere un array");
        return 0;
    }
    sb_put(&c->out, "gmb_kmeans_result((const double*)");
    emit_var_ref(c, a0);
    sb_put(&c->out, ", (gmb_int_t)(");
    emit_arr_len_of(c, a0);
    sb_put(&c->out, "), (gmb_int_t)(");
    emit_expr(c, a[1]);
    sb_put(&c->out, "))");
    return 1;
}
static const km_row KM_TBL[] = { { GMB_TOK_KMeans, km_kmeans } };
int emit_bf_kmeans(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns;
    for (size_t i = 0; i < sizeof(KM_TBL) / sizeof(KM_TBL[0]); i++)
        if (KM_TBL[i].tk == tk) return KM_TBL[i].sp(c, args, nargs, line);
    return 0;
}
/* ── dominio: compression (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } cp_row;
static int cp_zstream(ctx_t* c, gmb_expr_t** a, int n, const char* algo, int compr) {
    if (n < 1) return 0;
    sb_printf(&c->out, "%s(\"%s\", ", compr ? "gmb_compress_string" : "gmb_decompress_string", algo);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int cp_gzip(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "gzip", 1); }
static int cp_ungzip(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "gzip", 0); }
static int cp_zlib(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "zlib", 1); }
static int cp_unzlib(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "zlib", 0); }
static int cp_deflate(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "deflate", 1); }
static int cp_undeflate(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "deflate", 0); }
static int cp_bzip2(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "bzip2", 1); }
static int cp_unbzip2(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "bzip2", 0); }
static int cp_brotli(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "brotli", 1); }
static int cp_unbrotli(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "brotli", 0); }
static int cp_zstd(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "zstd", 1); }
static int cp_unzstd(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "zstd", 0); }
static int cp_lz4(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "lz4", 1); }
static int cp_unlz4(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "lz4", 0); }
static int cp_xz(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "xz", 1); }
static int cp_unxz(ctx_t* c, gmb_expr_t** a, int n) { return cp_zstream(c, a, n, "xz", 0); }
static int cp_algorithms(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    /* canone IL/CommonLib.CompressionUtils.SupportedAlgorithms (maiuscolo, virgola). */
    sb_put(&c->out, "gmb_string_alloc(\"GZIP,DEFLATE,BROTLI,ZLIB,BZIP2,ZSTD,LZ4,XZ\")");
    return 1;
}
static int cp_zip(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    if (a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        var_t* zv = lookup_var(c, a[0]->var);
        if (zv && zv->is_array && zv->type == TY_STR && !zv->matrix) {
            static int zf_uid = 0;
            int uid = ++zf_uid;
            sb_printf(&c->out, "({ const char* _zf%d[32]; gmb_int_t _zn%d = (gmb_int_t)(", uid, uid);
            emit_arr_len_of(c, zv);
            sb_printf(&c->out, "); if (_zn%d > 32) _zn%d = 32; ", uid, uid);
            sb_printf(&c->out, "for (gmb_int_t _i%d = 0; _i%d < _zn%d; _i%d++) _zf%d[_i%d] = ", uid, uid, uid, uid, uid, uid);
            emit_var_ref(c, zv);
            sb_printf(&c->out, "[_i%d].data ? ", uid);
            emit_var_ref(c, zv);
            sb_printf(&c->out, "[_i%d].data : \"\"; ", uid);
            /* gmb_zip_* torna 0 = OK / -1 = errore: un BIT BASIC vero e' -1, quindi senza
             * questa conversione COMPRESSION.ZIP(...) valeva FALSO anche avendo archiviato
             * tutto (MiniTests/compression/zip_fn.bas: "zipfn-ko" invece di "zipfn-ok"). */
            sb_printf(&c->out, "(gmb_double_t)(gmb_zip_files(_zf%d, (int)_zn%d, (", uid, uid);
            emit_str_operand(c, a[1]);
            sb_put(&c->out, ").data) == 0 ? -1 : 0); })");
            return 1;
        }
    }
    sb_put(&c->out, "(gmb_double_t)(gmb_zip_files_semicolon((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]);
    /* stessa convenzione: 0 = OK -> BIT vero (-1) */
    sb_put(&c->out, ").data) == 0 ? -1 : 0)");
    return 1;
}
static int cp_unzip(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    /* gmb_unzip_files: stessa convenzione 0 = OK del resto della famiglia (normalizzata
     * in gmb_compress.c): 0 -> BIT vero (-1). */
    sb_put(&c->out, "(gmb_double_t)(gmb_unzip_files((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]);
    sb_put(&c->out, ").data) == 0 ? -1 : 0)");
    return 1;
}
static const cp_row CP_TBL[] = {
    { GMB_TOK_Gzip, cp_gzip }, { GMB_TOK_Ungzip, cp_ungzip },
    { GMB_TOK_Zlib, cp_zlib }, { GMB_TOK_Unzlib, cp_unzlib },
    { GMB_TOK_Deflate, cp_deflate }, { GMB_TOK_Undeflate, cp_undeflate },
    { GMB_TOK_Bzip2, cp_bzip2 },
    { GMB_TOK_Unbzip2, cp_unbzip2 },
    { GMB_TOK_Brotli, cp_brotli }, { GMB_TOK_Unbrotli, cp_unbrotli },
    { GMB_TOK_Zstd, cp_zstd }, { GMB_TOK_Unzstd, cp_unzstd },
    { GMB_TOK_Lz4, cp_lz4 }, { GMB_TOK_Unlz4, cp_unlz4 },
    { GMB_TOK_Xz, cp_xz }, { GMB_TOK_Unxz, cp_unxz },
    { GMB_TOK_Compress, cp_gzip }, { GMB_TOK_Decompress, cp_ungzip },
    { GMB_TOK_Algorithms, cp_algorithms }, { GMB_TOK_Zip, cp_zip }, { GMB_TOK_Unzip, cp_unzip },
};
int emit_bf_compression(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(CP_TBL) / sizeof(CP_TBL[0]); i++)
        if (CP_TBL[i].tk == tk) return CP_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: context (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } ct_row;
static int ct_background(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_ctx_background()");
    return 1;
}
static int ct_withcancel(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_ctx_withcancel(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int ct_done(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_ctx_done(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int ct_cancelled(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_ctx_cancelled(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "))");
    return 1;
}
static int ct_cancel(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_ctx_cancel(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), 0.0)");
    return 1;
}
static const ct_row CT_TBL[] = {
    { GMB_TOK_ContextBackground, ct_background }, { GMB_TOK_ContextWithCancel, ct_withcancel },
    { GMB_TOK_ContextDone, ct_done }, { GMB_TOK_ContextCancelled, ct_cancelled },
    { GMB_TOK_ContextCancel, ct_cancel },
};
int emit_bf_context(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)ns;
    for (size_t i = 0; i < sizeof(CT_TBL) / sizeof(CT_TBL[0]); i++)
        if (CT_TBL[i].tk == tk && CT_TBL[i].sp(c, args, nargs)) return 1;
    c99_fail(c, line, "CONTEXT: membro '%s' non supportato dall'emitter nativo", name);
    return 0;
}
/* ── dominio: fetch (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } fe_row;
static int fe_fetch(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1 && n != 2) return 0;
    c->uses_fetch = 1;
    c->uses_gpm = 1;    /* gmb_future_await parcheggia il task: serve lo scheduler GPM */
    c->uses_net = 1;    /* gmb_net_init per curl */
    sb_put(&c->out, "({ gmb_fetch_expr_th_args* _fa = (gmb_fetch_expr_th_args*)calloc(1, sizeof(gmb_fetch_expr_th_args)); _fa->url = strdup((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data); ");
    if (n == 2) {
        sb_put(&c->out, "_fa->dict = ");
        emit_expr(c, a[1]);
        sb_put(&c->out, "; _fa->has_opts = 1; ");
    } else {
        sb_put(&c->out, "_fa->has_opts = 0; ");
    }
    sb_put(&c->out, "gmb_future_t* _fut = gmb_future_new(); _fa->fut = _fut; gmb_thread_t _t; GMB_THREAD_CREATE(&_t, gmb_fetch_expr_th, _fa); GMB_THREAD_DETACH(&_t); gmb_future_await(_fut); gmb_val_t _res = _fut->value; free(_fa->url); free(_fa); _res; })");
    return 1;
}
static const fe_row FE_TBL[] = { { GMB_TOK_Fetch, fe_fetch } };
int emit_bf_fetch(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(FE_TBL) / sizeof(FE_TBL[0]); i++)
        if (FE_TBL[i].tk == tk) return FE_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: HTTP client (GET/POST/PUT/DELETE) → runtime gmb_net_http_body (§0bis) ── */
static int ht_method(ctx_t* c, gmb_expr_t** a, int n, const char* method) {
    if (n < 1) return 0;
    c->uses_net = 1;    /* gmb_net_init per curl */
    sb_printf(&c->out, "gmb_string_alloc(gmb_net_http_body(\"%s\", ", method);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "NULL");
    sb_put(&c->out, "))");
    return 1;
}
static int ht_get(ctx_t* c, gmb_expr_t** a, int n)    { return ht_method(c, a, n, "GET"); }
static int ht_post(ctx_t* c, gmb_expr_t** a, int n)   { return ht_method(c, a, n, "POST"); }
static int ht_put(ctx_t* c, gmb_expr_t** a, int n)    { return ht_method(c, a, n, "PUT"); }
static int ht_delete(ctx_t* c, gmb_expr_t** a, int n) { return ht_method(c, a, n, "DELETE"); }
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int); } ht_row;
static const ht_row HT_TBL[] = {
    { GMB_TOK_HttpGet, ht_get }, { GMB_TOK_HttpPost, ht_post },
    { GMB_TOK_HttpPut, ht_put }, { GMB_TOK_HttpDelete, ht_delete },
};
int emit_bf_http_client(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(HT_TBL) / sizeof(HT_TBL[0]); i++)
        if (HT_TBL[i].tk == tk) return HT_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: bounds (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } bd_row;
static const char* bd_arg_name(gmb_expr_t* e) {
    if (e->kind == GMB_EXPR_VAR && e->var) return e->var;
    if (e->kind == GMB_EXPR_CALL && e->var && e->nargs == 0) return e->var;  /* arr() */
    return NULL;
}
static int bd_lbound(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    if (n != 1 && n != 2) return 0;
    const char* an0 = bd_arg_name(a[0]);
    if (!an0) return 0;
    var_t* av = lookup_var(c, an0);
    if (av && av->is_array) {
        /* Canone 1-based 2026-09-30: lower dal DIM con TO (__blo), default 0. */
        if (av->has_blo) {
            sb_put(&c->out, "(gmb_double_t)(");
            if (av->arr_byref) sb_put(&c->out, "_");
            emit_ident(c, av->name);
            sb_put(&c->out, "__blo)");
        } else sb_put(&c->out, "(gmb_double_t)0");
        return 1;
    }
    return 0;
}
static int bd_ubound(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1 && n != 2) return 0;
    /* UBOUND(A(i)) con A JAGGED (`DIM A(n) AS ARRAY OF DOUBLE` / parametro jagged):
     * le righe sono rettangolari → il numero di colonne è `A__cols` (2026-11-03). Senza,
     * la chiamata cadeva in "UBOUND è una funzione di libreria (SYSTEM)". */
    if (n == 1 && a[0] && a[0]->kind == GMB_EXPR_CALL && a[0]->nargs == 1 && a[0]->var) {
        var_t* jv = lookup_var(c, a[0]->var);
        if (jv && jv->jagged && !jv->matrix && jv->type != TY_STR) {
            sb_put(&c->out, "(gmb_double_t)(");
            emit_ident(c, jv->name);
            sb_put(&c->out, "__cols - 1)");
            return 1;
        }
    }
    /* CONVERT.* Markdown AST: UBOUND(HEADINGS(doc[,level])) o UBOUND(var boxed_md) →
     * gmb_list_count(gmb_val_as_list(...)) - 1 (parity emitter C# gmb_list_count). */
    if (n == 1 && a[0]) {
        if (a[0]->kind == GMB_EXPR_CALL && cv_is_md_list(a[0]->tk)) {
            sb_put(&c->out, "(gmb_double_t)(gmb_list_count(");
            emit_expr(c, a[0]);
            sb_put(&c->out, ") - 1)");
            return 1;
        }
        /* UBOUND(<call che ritorna gmb_list_t>) — AI.AGENTLIST/AGENTEVENTS/AI.LIST:
         * gmb_list_count(<call>) - 1. Prima il call non era un nome di variabile →
         * "simbolo non trovato: UBOUND" (AgentOperatingCenter). */
        extern int gmb_c99_tk_returns_list(gmb_tokenkind_t tk);   /* gmb_c99_emit.c */
        if (a[0]->kind == GMB_EXPR_CALL && gmb_c99_tk_returns_list(a[0]->tk)) {
            sb_put(&c->out, "(gmb_double_t)(gmb_list_count(");
            emit_expr(c, a[0]);
            sb_put(&c->out, ") - 1)");
            return 1;
        }
        if (a[0]->kind == GMB_EXPR_CALL
            && (a[0]->tk == GMB_TOK_AiAgentList || a[0]->tk == GMB_TOK_AiAgentEvents
                || a[0]->tk == GMB_TOK_AiList)) {
            sb_put(&c->out, "(gmb_double_t)(gmb_list_count(");
            emit_expr(c, a[0]);
            sb_put(&c->out, ") - 1)");
            return 1;
        }
        if (a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
            var_t* mbv = lookup_var(c, a[0]->var);
            if (mbv && mbv->boxed_md) {
                sb_put(&c->out, "(gmb_double_t)(gmb_list_count(gmb_val_as_list(");
                emit_ident(c, mbv->name);
                sb_put(&c->out, ")) - 1)");
                return 1;
            }
            /* UBOUND(var JSON boxed: DIM x() AS JSON): conteggio dell'array JSON. */
            if (mbv && !mbv->gkind && mbv->boxed_val && !mbv->vclass[0] && !mbv->stats_val) {
                sb_put(&c->out, "(gmb_double_t)(gmb_list_count(gmb_val_as_list(");
                emit_ident(c, mbv->name);
                sb_put(&c->out, ")) - 1)");
                return 1;
            }
            /* UBOUND(<postedVar>.<membro>) / UBOUND(<mdVar>.<membro>) /
             * UBOUND(req.BodyJson|req.Params): il membro-read è un valore GREZZO
             * gmb_val_t (lista per HttpMultiPart FORMFIELDS, md-list, dict JSON):
             * gmb_list_count(gmb_val_as_list(<expr>)) - 1 — la lettura grezza è già
             * emessa dal dispatch VAR (posted_val / boxed_md / whitelist GK_HTTPREQ). */
            {
                const char* pdot = strchr(a[0]->var, '.');
                if (pdot && pdot != a[0]->var) {
                    char pbase[160]; size_t pbl = (size_t)(pdot - a[0]->var);
                    if (pbl < sizeof pbase) {
                        memcpy(pbase, a[0]->var, pbl); pbase[pbl] = 0;
                        var_t* pb = lookup_var(c, pbase);
                        if (pb && !pb->vclass[0] && (pb->posted_val || pb->boxed_md
                            || (pb->gkind == GK_HTTPREQ
                                && (strcasecmp(pdot + 1, "BodyJson") == 0
                                    || strcasecmp(pdot + 1, "Params") == 0)))) {
                            sb_put(&c->out, "(gmb_double_t)(gmb_list_count(gmb_val_as_list(");
                            emit_expr(c, a[0]);
                            sb_put(&c->out, ")) - 1)");
                            return 1;
                        }
                    }
                }
            }
            /* UBOUND(<ist>.<campo-ARRAY>) — campo-array di UDT/CLASSE (es. `Res.Weights`,
             * `r.W`): il bound è un COSTANTE dichiarata → emetti il limite SUPERIORE
             * (cls_field_arr_dim). Prima cadeva nel fallback "UBOUND non definita"
             * (TradingPortfolioOptimization). */
            {
                const char* fdot = strchr(a[0]->var, '.');
                if (fdot && fdot != a[0]->var) {
                    char fb[160]; size_t fbl = (size_t)(fdot - a[0]->var);
                    if (fbl < sizeof fb) {
                        memcpy(fb, a[0]->var, fbl); fb[fbl] = 0;
                        var_t* fbv = lookup_var(c, fb);
                        const char* fcls = (fbv && fbv->vclass[0]) ? fbv->vclass : NULL;
                        char futy[64]; futy[0] = 0;
                        if (!fcls && gmb_udt_var_type(c, fb, futy, (int)sizeof futy) && futy[0]) fcls = futy;
                        struct cls_info* fci = fcls ? cls_find(c, fcls) : NULL;
                        const char* fty = fci ? cls_field_type_of(fci, fdot + 1) : NULL;
                        if (fty && cls_field_arr_size(fty) >= 0) {
                            sb_printf(&c->out, "(gmb_double_t)(%ld)", cls_field_arr_dim(fty, 0));
                            return 1;
                        }
                    }
                }
            }
        }
    }
    const char* an0 = bd_arg_name(a[0]);
    if (!an0) return 0;
    var_t* av = lookup_var(c, an0);
    /* UBOUND(<lista/dict gmb_list_t>) — es. `DIM ids() = SQL.COL(...)` (la DIM lo classifica
     * gkind=GK_LIST, NON is_array): gmb_list_count - 1. */
    if (av && av->gkind == GK_LIST && !av->vclass[0]) {
        sb_put(&c->out, "(gmb_double_t)(gmb_list_count(");
        emit_var_ref(c, av);
        sb_put(&c->out, ") - 1)");
        return 1;
    }
    if (av && av->is_array) {
        if (n == 2) {
            int nd = av->ndims > 0 ? av->ndims : 1;
            if (av->dyn_len || av->matrix) {
                c99_fail(c, line, "UBOUND(arr, dim): array dinamico non supportato");
                return 0;
            }
            if (a[1]->kind == GMB_EXPR_NUM) {
                int k = (int)a[1]->num - 1;
                if (k < 0 || k >= nd) {
                    c99_fail(c, line, "UBOUND(arr, dim): dimensione %d fuori range (ndims=%d)",
                             (int)a[1]->num, nd);
                    return 0;
                }
                sb_put(&c->out, "(gmb_double_t)(");
                sb_bound_var(&c->out, av, k);
                sb_put(&c->out, ")");
                return 1;
            }
            /* dimensione non costante: selezione a runtime (ternari annidati) */
            sb_put(&c->out, "(gmb_double_t)(");
            for (int k = 0; k < nd; k++) {
                sb_put(&c->out, "((gmb_int_t)(");
                emit_expr(c, a[1]);
                sb_printf(&c->out, ") == %d ? (gmb_int_t)(", k + 1);
                sb_bound_var(&c->out, av, k);
                sb_put(&c->out, ") : ");
            }
            sb_put(&c->out, "(gmb_int_t)0");
            for (int k = 0; k < nd; k++) sb_put(&c->out, ")");
            sb_put(&c->out, ")");
            return 1;
        }
        sb_put(&c->out, "(gmb_double_t)(");
        emit_arr_len_of(c, av);
        sb_put(&c->out, ") - 1");
        return 1;
    }
    /* UBOUND(<base>.<membro>) con base JSON/boxed/dict e membro lista/array (es.
     * UBOUND(Res.Weights), UBOUND(formData.FormFields)): gmb_list_count(gmb_val_as_list(
     * <lettura membro>)) - 1. Prima cadeva in "UBOUND è una funzione di libreria (SYSTEM)". */
    if (n == 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        const char* dot = strchr(a[0]->var, '.');
        if (dot && dot != a[0]->var) {
            char base[160]; size_t bl = (size_t)(dot - a[0]->var);
            if (bl < sizeof base) {
                memcpy(base, a[0]->var, bl); base[bl] = 0;
                var_t* bv = lookup_var(c, base);
                if (bv && !bv->vclass[0] && bv->boxed_val && !strchr(dot + 1, '.')) {
                    /* Membro di DICT boxed: lettura GREZZA gmb_val_t — gmb_json_path_text
                     * ritorna una STRINGA e gmb_val_as_list non l'accetta (TradingPortfolio). */
                    sb_put(&c->out, "(gmb_double_t)(gmb_list_count(gmb_val_as_list(gmb_dict_get(gmb_val_as_dict(");
                    emit_ident(c, bv->name);
                    sb_put(&c->out, "), \"");
                    for (const char* p = dot + 1; *p; p++) { if (*p == '"' || *p == '\\') sb_put(&c->out, "\\"); sb_putn(&c->out, p, 1); }
                    sb_put(&c->out, "\"))) - 1)");
                    return 1;
                }
                if (bv && !bv->vclass[0] && bv->gkind) {
                    sb_put(&c->out, "(gmb_double_t)(gmb_list_count(gmb_val_as_list(");
                    emit_expr(c, a[0]);
                    sb_put(&c->out, ")) - 1)");
                    return 1;
                }
            }
        }
    }
    return 0;
}
static int bd_join(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* JOIN(arr, sep) → gmb_array_join / gmb_array_join_doubles (parity oracolo Collections.cs) */
    (void)line;
    if (n != 2) return 0;
    /* JOIN(<call che ritorna gmb_list_t>, sep) — AI.AGENTEVENTS/AGENTLIST/AI.LIST:
     * gmb_list_join(<call>, sep). Prima il call non era un nome di variabile →
     * "simbolo non trovato: JOIN" (AgentPermissions). */
    if (a[0] && a[0]->kind == GMB_EXPR_CALL
        && (a[0]->tk == GMB_TOK_AiAgentList || a[0]->tk == GMB_TOK_AiAgentEvents
            || a[0]->tk == GMB_TOK_AiList)) {
        sb_put(&c->out, "gmb_list_join(");
        emit_expr(c, a[0]);
        sb_put(&c->out, ", (");
        emit_str_operand(c, a[1]);
        sb_put(&c->out, ").data)");
        return 1;
    }
    const char* an0 = bd_arg_name(a[0]);
    if (!an0) return 0;
    var_t* av = lookup_var(c, an0);
    if (av && av->is_array && !av->matrix && !av->vclass[0]) {
        sb_put(&c->out, av->type == TY_STR ? "gmb_array_join(" : "gmb_array_join_doubles(");
        emit_var_ref(c, av);
        sb_put(&c->out, ", (gmb_int_t)(");
        emit_arr_len_of(c, av);
        sb_put(&c->out, "), (");
        emit_str_operand(c, a[1]);
        sb_put(&c->out, ").data)");
        return 1;
    }
    /* JOIN(<lista gmb_list_t>, sep) — es. `DIM chiavi() AS STRING = JSON.KEYS(...)` (la DIM
     * classifica `chiavi` come gkind=GK_LIST, NON is_array): gmb_list_join. Prima il build
     * falliva con "simbolo non trovato: JOIN" (JsonFullTests/JsonFiles). */
    if (av && av->gkind == GK_LIST && !av->vclass[0]) {
        sb_put(&c->out, "gmb_list_join(");
        emit_var_ref(c, av);
        sb_put(&c->out, ", (");
        emit_str_operand(c, a[1]);
        sb_put(&c->out, ").data)");
        return 1;
    }
    return 0;
}
static const bd_row BD_TBL[] = {
    { GMB_TOK_UBound, bd_ubound }, { GMB_TOK_LBound, bd_lbound }, { GMB_TOK_Join, bd_join },
};
int emit_bf_bounds(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns;
    for (size_t i = 0; i < sizeof(BD_TBL) / sizeof(BD_TBL[0]); i++)
        if (BD_TBL[i].tk == tk) return BD_TBL[i].sp(c, args, nargs, line);
    return 0;
}
/* ── dominio: stats_arr (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } sa_row;
static int sa_stat(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix && !av->vclass[0]) {
        sb_put(&c->out, fn); sb_put(&c->out, "("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int sa_mean(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_mean"); }
static int sa_median(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_median"); }
static int sa_stddev(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_stddev"); }
static int sa_variance(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_variance"); }
static int sa_kurtosis(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_kurtosis"); }
static int sa_mad(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_mad"); }
static int sa_skewness(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_skewness"); }
static int sa_correlation(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    if (!a[0] || a[0]->kind != GMB_EXPR_VAR || !a[1] || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* xv = lookup_var(c, a[0]->var);
    var_t* yv = lookup_var(c, a[1]->var);
    if (!xv || !yv || !xv->is_array || !yv->is_array || xv->matrix || yv->matrix) return 0;
    sb_put(&c->out, "gmb_correlation("); emit_var_ref(c, xv);
    sb_put(&c->out, ", "); emit_var_ref(c, yv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, xv); sb_put(&c->out, "))");
    return 1;
}
static int sa_covariance(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    if (!a[0] || a[0]->kind != GMB_EXPR_VAR || !a[1] || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* xv = lookup_var(c, a[0]->var);
    var_t* yv = lookup_var(c, a[1]->var);
    if (!xv || !yv || !xv->is_array || !yv->is_array || xv->matrix || yv->matrix) return 0;
    sb_put(&c->out, "gmb_covariance("); emit_var_ref(c, xv);
    sb_put(&c->out, ", "); emit_var_ref(c, yv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, xv); sb_put(&c->out, "))");
    return 1;
}
static int sa_range(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_range_arr"); }
static int sa_amax(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_max_arr"); }
static int sa_amin(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_min_arr"); }
static int sa_asum(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_sum_arr"); }
static int sa_mode(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_mode"); }
static int sa_iqr(ctx_t* c, gmb_expr_t** a, int n) { return sa_stat(c, a, n, "gmb_iqr"); }
static int sa_moment(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    if (!a[0] || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix && !av->vclass[0]) {
        sb_put(&c->out, "gmb_moment("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int sa_percentile(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (!a[0] || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix && !av->vclass[0]) {
        sb_put(&c->out, "gmb_percentile("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), ");
        if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "50");
        sb_put(&c->out, ")"); return 1;
    }
    return 0;
}
/* AVG: media di un array (delega a sa_stat) O variadic di scalari
 * (canone: AVG(2,4)=3) — compound literal per la runtime gmb_mean. */
static int sa_avg(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (n == 1 && a[0]->kind == GMB_EXPR_VAR) {
        var_t* av = lookup_var(c, a[0]->var);
        if (av && av->is_array && !av->matrix && !av->vclass[0])
            return sa_stat(c, a, n, "gmb_mean");
    }
    sb_put(&c->out, "gmb_mean((gmb_double_t[]){");
    for (int i = 0; i < n; i++) { if (i) sb_put(&c->out, ", "); sb_put(&c->out, "("); emit_expr(c, a[i]); sb_put(&c->out, ")"); }
    sb_printf(&c->out, "}, %d)", n);
    return 1;
}
static const sa_row SA_TBL[] = {
    { GMB_TOK_Mean, sa_mean }, { GMB_TOK_Avg, sa_avg },
    { GMB_TOK_Median, sa_median }, { GMB_TOK_StdDev, sa_stddev },
    { GMB_TOK_Variance, sa_variance }, { GMB_TOK_AMax, sa_amax },
    { GMB_TOK_AMin, sa_amin }, { GMB_TOK_ASum, sa_asum },
    { GMB_TOK_Kurtosis, sa_kurtosis }, { GMB_TOK_Mad, sa_mad },
    { GMB_TOK_Skewness, sa_skewness }, { GMB_TOK_Correlation, sa_correlation },
    { GMB_TOK_Percentile, sa_percentile },
    { GMB_TOK_StatsMode, sa_mode }, { GMB_TOK_StatsIqr, sa_iqr }, { GMB_TOK_StatsMoment, sa_moment },
    { GMB_TOK_StatsCovariance, sa_covariance },
    { GMB_TOK_StatsRange, sa_range },
    { GMB_TOK_StatsMax, sa_amax }, { GMB_TOK_StatsMin, sa_amin },
};
int emit_bf_stats_arr(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(SA_TBL) / sizeof(SA_TBL[0]); i++)
        if (SA_TBL[i].tk == tk) return SA_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: trading metrics (delegazione §0quater) ──
 * TRADING.METRICS array→scalar: la runtime condivisa gmb_tm_... (mai inline, §0bis).
 * Il primo argomento è una variabile array; gli extra sono scalari passati in ordine. */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } tm_row;
static int tm_ret(ctx_t* c, gmb_expr_t** a, int n, const char* fn, int min_extra) {
    if (n < 1 + min_extra) return 0;
    if (!a[0]) return 0;
    if (a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        var_t* av = lookup_var(c, a[0]->var);
        if (!av || !av->is_array || av->matrix || av->vclass[0]) return 0;
        sb_put(&c->out, fn); sb_put(&c->out, "(");
        emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, ")");
        for (int i = 1; i < n; i++) { sb_put(&c->out, ", "); emit_expr(c, a[i]); }
        sb_put(&c->out, ")"); return 1;
    }
    /* Argomento SCALARE dove la StdLib attende una serie: il canone IL (ToDoubleArray)
     * lo coercisce a [valore] — stessa semantica del helper VM gmb_vm_list_to_dbl
     * (type==1 → [num]). Serve ai builtin TRADING.GEOMETRICMEANRETURN/HOLDINGPERIODRETURN
     * chiamati con un letterale (trading/gen_TRADING_*.bas). */
    if (etype(c, a[0]) == TY_STR) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "((const double[]){ (double)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ") }, 1");
    for (int i = 1; i < n; i++) { sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ")"); return 1;
}
/* funzioni con due array di input (asset, benchmark) */
static int tm_ab(ctx_t* c, gmb_expr_t** a, int n, const char* fn, int min_extra) {
    if (n < 2 + min_extra) return 0;
    if (!a[0] || a[0]->kind != GMB_EXPR_VAR || !a[1] || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    var_t* bv = lookup_var(c, a[1]->var);
    if (!av || !bv || !av->is_array || !bv->is_array || av->matrix || bv->matrix || av->vclass[0] || bv->vclass[0]) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_var_ref(c, av); sb_put(&c->out, ", "); emit_var_ref(c, bv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, ")");
    for (int i = 2; i < n; i++) { sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ")"); return 1;
}
static int tm_maxdd(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_max_drawdown", 0); }
static int tm_beta(ctx_t* c, gmb_expr_t** a, int n) { return tm_ab(c, a, n, "gmb_trd_beta", 0); }
static int tm_annret(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_annualized_return", 1); }
static int tm_annvol(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_annualized_volatility", 1); }
static int tm_calmar(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_calmar_ratio", 1); }
/* SHARPERATIO canone BASIC: IlRuntime.SharpeRatio(arr) = mean(rf=0)/sample_sd * sqrt(252).
 * Il riskfree opzionale del registro NON è applicato dal canone IL — emetto rf=0 e ppy=252. */
static int tm_sharpe(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (!a[0] || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!av || !av->is_array || av->matrix || av->vclass[0]) return 0;
    sb_put(&c->out, "gmb_tm_sharpe_ratio("); emit_var_ref(c, av);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av);
    sb_put(&c->out, "), 0, 252)");
    return 1;
}
static int tm_sortino(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_sortino_ratio", 2); }
static int tm_ulcer(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_trd_ulcer", 0); }
static int tm_downside(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_downside_deviation", 1); }
static int tm_var(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_value_at_risk", 1); }
static int tm_cvar(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_trd_cvar", 1); }
static int tm_rskew(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_returns_skewness", 0); }
static int tm_omega(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_omega_ratio", 1); }
static int tm_aterr(ctx_t* c, gmb_expr_t** a, int n) { return tm_ab(c, a, n, "gmb_tm_annualized_tracking_error", 1); }
static int tm_geomret(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_geometric_mean_return", 0); }
static int tm_holdret(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_holding_period_return", 0); }
static int tm_addd(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_annualized_downside_dev", 2); }
static int tm_totret(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_trd_total_return", 0); }
static int tm_rkurt(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_kurtosis", 0); }
static int tm_alpha(ctx_t* c, gmb_expr_t** a, int n) { return tm_ab(c, a, n, "gmb_trd_alpha", 2); }
static int pf_expret(ctx_t* c, gmb_expr_t** a, int n) { return tm_ab(c, a, n, "gmb_tm_expected_return", 0); }
static int pf_long(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_is_long_only", 0); }
static int pf_sharpe3(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 3) return 0;
    sb_put(&c->out, "gmb_fin_portfolio_sharpe(");
    for (int i = 0; i < 3; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ")"); return 1;
}
/* PortfolioResult come dict boxed (parity emitter C# EmitPortfolioResultCall):
 * gmb_tm_portfolio_result(rets, n, kind) → gmb_val_t. kind: 0=MAXRETURN,
 * 1=KELLYCRITERION, 2=MINCVAR, 3=MINCDAR. */
static int pf_result(ctx_t* c, gmb_expr_t** a, int n, int kind) {
    if (n < 1 || !a[0] || a[0]->kind != GMB_EXPR_VAR) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!av || !av->is_array || av->matrix || av->vclass[0]) return 0;
    sb_put(&c->out, "gmb_tm_portfolio_result((gmb_double_t*)");
    emit_var_ref(c, av);
    sb_put(&c->out, ", (int)(");
    /* jagged (DIM x(n) AS ARRAY OF DOUBLE): niente _len, il bound __b0 è l'indice max →
     * n = __b0 + 1 (parity emitter C# _arraySizes = BoundValue + 1). */
    if (av->jagged) { sb_bound_var(&c->out, av, 0); sb_put(&c->out, " + 1"); }
    else emit_arr_len_of(c, av);
    sb_printf(&c->out, "), %d)", kind);
    return 1;
}
static int pf_maxret(ctx_t* c, gmb_expr_t** a, int n) { return pf_result(c, a, n, 0); }
static int pf_kelly(ctx_t* c, gmb_expr_t** a, int n) { return pf_result(c, a, n, 1); }
static int pf_mincvar(ctx_t* c, gmb_expr_t** a, int n) { return pf_result(c, a, n, 2); }
static int pf_mincdar(ctx_t* c, gmb_expr_t** a, int n) { return pf_result(c, a, n, 3); }
/* Portfolio optimization: gmb_tm_portfolio_optimize(cov, n, rets, rets_n, rf, kind).
 * kind: 0=MINVOLATILITY, 1=MAXSHARPE, 2=RISKPARITY, 3=EQUALLYWEIGHTED, 4=MEANVARIANCE. */
static int pf_optimize(ctx_t* c, gmb_expr_t** a, int n, int kind) {
    if (kind == 3) {
        if (n < 1) return 0;
        sb_put(&c->out, "gmb_tm_portfolio_optimize(NULL, (int)(");
        emit_expr(c, a[0]);
        sb_put(&c->out, "), NULL, 0, 0, 3)");
        return 1;
    }
    int covIdx = (kind == 1 || kind == 4) ? 1 : 0;
    if (n <= covIdx || !a[covIdx] || a[covIdx]->kind != GMB_EXPR_VAR) return 0;
    var_t* cv = lookup_var(c, a[covIdx]->var);
    if (!cv || !cv->jagged) return 0;
    sb_put(&c->out, "gmb_tm_portfolio_optimize((gmb_double_t**)");
    emit_var_ref(c, cv);
    sb_put(&c->out, ", (int)(");
    sb_bound_var(&c->out, cv, 0);
    /* il bound __b0 è l'INDICE massimo (DIM cov(2) → 2): n = __b0 + 1 (parity emitter C#
     * _arraySizes = BoundValue + 1). */
    sb_put(&c->out, " + 1), ");
    if ((kind == 1 || kind == 4) && n >= 1 && a[0] && a[0]->kind == GMB_EXPR_VAR) {
        var_t* rv = lookup_var(c, a[0]->var);
        if (rv && rv->is_array && !rv->jagged) {
            sb_put(&c->out, "(const double*)");
            emit_var_ref(c, rv);
            sb_put(&c->out, ", (int)(");
            emit_arr_len_of(c, rv);
            sb_put(&c->out, "), ");
        } else sb_put(&c->out, "NULL, 0, ");
    } else sb_put(&c->out, "NULL, 0, ");
    if (kind == 1 && n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "0");
    sb_printf(&c->out, ", %d)", kind);
    return 1;
}
static int pf_maxsharpe(ctx_t* c, gmb_expr_t** a, int n) { return pf_optimize(c, a, n, 1); }
static int pf_meanvar(ctx_t* c, gmb_expr_t** a, int n) { return pf_optimize(c, a, n, 4); }
static int pf_minvol(ctx_t* c, gmb_expr_t** a, int n) { return pf_optimize(c, a, n, 0); }
static int pf_riskparity(ctx_t* c, gmb_expr_t** a, int n) { return pf_optimize(c, a, n, 2); }
static int pf_equally(ctx_t* c, gmb_expr_t** a, int n) { return pf_optimize(c, a, n, 3); }
/* PVAR/CVAR(storici[][], pesi, conf): i pesi sono IGNORATI (parità IL/VM/C); lo storico
 * jagged (righe uniformi) viene appiattito e passato a gmb_trd_{var,cvar} (CVaR assoluto). */
static int pf_risk_jagged(ctx_t* c, gmb_expr_t** a, int n, int is_cvar) {
    if (n < 1 || !a[0] || a[0]->kind != GMB_EXPR_VAR) return 0;
    var_t* hv = lookup_var(c, a[0]->var);
    if (!hv || !hv->jagged) return 0;
    sb_printf(&c->out, "gmb_trd_portfolio_%s_jagged((double**)", is_cvar ? "cvar" : "var");
    emit_var_ref(c, hv);
    sb_put(&c->out, ", (int)(");
    sb_bound_var(&c->out, hv, 0); sb_put(&c->out, " + 1), (int)(");
    emit_ident(c, hv->name); sb_put(&c->out, "__cols), ");
    if (n >= 3) emit_expr(c, a[2]);
    else if (n >= 2) emit_expr(c, a[1]);
    else sb_put(&c->out, "0.95");
    sb_put(&c->out, ")");
    return 1;
}
static int pf_pvar(ctx_t* c, gmb_expr_t** a, int n) { return pf_risk_jagged(c, a, n, 0); }
static int pf_pcvar(ctx_t* c, gmb_expr_t** a, int n) { return pf_risk_jagged(c, a, n, 1); }
static int pf_mad(ctx_t* c, gmb_expr_t** a, int n) { return tm_ret(c, a, n, "gmb_tm_portfolio_mad", 0); }
static int pf_semimad(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || !a[0] || a[0]->kind != GMB_EXPR_VAR) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!av || !av->is_array || av->matrix || av->vclass[0]) return 0;
    sb_put(&c->out, "gmb_tm_portfolio_semimad(");
    emit_var_ref(c, av);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), ");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
    return 1;
}
/* STDEV(pesi[], covarianza[][]) = sqrt(wᵀ·cov·w) — cov jagged, n = __b0 + 1. */
static int pf_stdev(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2 || !a[0] || a[0]->kind != GMB_EXPR_VAR || !a[1] || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* wv = lookup_var(c, a[0]->var);
    var_t* cv = lookup_var(c, a[1]->var);
    if (!wv || !wv->is_array || !cv || !cv->jagged) return 0;
    sb_put(&c->out, "({ const double* _sw = (const double*)");
    emit_var_ref(c, wv);
    sb_put(&c->out, "; const double* const* _sm = (const double* const*)");
    emit_var_ref(c, cv);
    sb_put(&c->out, "; int _sn = (int)(");
    sb_bound_var(&c->out, cv, 0); sb_put(&c->out, " + 1");
    sb_put(&c->out, "); double _ss = 0; for (int _i = 0; _i < _sn; _i++) for (int _j = 0; _j < _sn; _j++) _ss += _sw[_i] * _sm[_i][_j] * _sw[_j]; sqrt(_ss); })");
    return 1;
}
static int tm_treynor(ctx_t* c, gmb_expr_t** a, int n) { return tm_ab(c, a, n, "gmb_tm_treynor_ratio", 2); }
static int tm_infratio(ctx_t* c, gmb_expr_t** a, int n) { return tm_ab(c, a, n, "gmb_tm_information_ratio", 1); }
static const tm_row TM_TBL[] = {
    { GMB_TOK_MaxDrawdown, tm_maxdd }, { GMB_TOK_AnnualizedReturn, tm_annret },
    { GMB_TOK_AnnualizedVolatility, tm_annvol }, { GMB_TOK_CalmarRatio, tm_calmar },
    { GMB_TOK_SharpeRatio, tm_sharpe }, { GMB_TOK_SortinoRatio, tm_sortino },
    { GMB_TOK_UlcerIndex, tm_ulcer }, { GMB_TOK_DownsideDeviation, tm_downside },
    { GMB_TOK_ValueAtRisk, tm_var },
    { GMB_TOK_ConditionalValueAtRisk, tm_cvar },
    { GMB_TOK_AnnualizedTrackingError, tm_aterr },
    { GMB_TOK_AnnualizedDownsideDeviation, tm_addd },
    { GMB_TOK_ReturnsSkewness, tm_rskew }, { GMB_TOK_OmegaRatio, tm_omega },
    { GMB_TOK_TotalReturn, tm_totret },
    { GMB_TOK_ReturnsKurtosis, tm_rkurt },
    { GMB_TOK_Alpha, tm_alpha },
    { GMB_TOK_TreynorRatio, tm_treynor }, { GMB_TOK_InformationRatio, tm_infratio },
    { GMB_TOK_Beta, tm_beta },
    /* TRADING array→scalar mancanti: le funzioni esistono già nel runtime condiviso
     * (gmb_tm_geometric_mean_return / gmb_tm_holding_period_return, §0bis). */
    { GMB_TOK_GeometricMeanReturn, tm_geomret },
    { GMB_TOK_HoldingPeriodReturn, tm_holdret },
    /* TRADING.PORTFOLIO scalar (array→scalar, runtime condivisa) */
    { GMB_TOK_PortfolioExpectedReturn, pf_expret },
    { GMB_TOK_PortfolioSharpeRatio, pf_sharpe3 },
    { GMB_TOK_MaxReturn, pf_maxret },
    { GMB_TOK_KellyCriterion, pf_kelly },
    { GMB_TOK_MinCvar, pf_mincvar },
    { GMB_TOK_MinCdar, pf_mincdar },
    { GMB_TOK_MaxSharpe, pf_maxsharpe },
    { GMB_TOK_MeanVariance, pf_meanvar },
    { GMB_TOK_MinVolatility, pf_minvol },
    { GMB_TOK_RiskParity, pf_riskparity },
    { GMB_TOK_EquallyWeighted, pf_equally },
    { GMB_TOK_PortfolioMad, pf_mad },
    { GMB_TOK_PortfolioSemiMad, pf_semimad },
    { GMB_TOK_PortfolioStDev, pf_stdev },
    { GMB_TOK_PortfolioVar, pf_pvar },
    { GMB_TOK_PortfolioCVar, pf_pcvar },
    { GMB_TOK_IsLongOnly, pf_long },
};
int emit_bf_metrics(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(TM_TBL) / sizeof(TM_TBL[0]); i++)
        if (TM_TBL[i].tk == tk) return TM_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: signal (design IIR handle-based + welch) ──
 * Design: gmb_filter_{butter,cheby1,cheby2,ellip}(...) → handle scalare (parità IL
 * IlRuntime.Signal*). WELCH: gmb_welch_c99 → handle matrice (parità emitter C#). */
static int sg_wn(ctx_t* c, gmb_expr_t* e, var_t** out) {
    if (!e || e->kind != GMB_EXPR_VAR || !e->var) return 0;
    var_t* v = lookup_var(c, e->var);
    if (!v || !v->is_array) return 0;
    *out = v; return 1;
}
static int sg_butter(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    var_t* wv = NULL; if (!sg_wn(c, a[1], &wv)) return 0;
    sb_put(&c->out, "gmb_filter_butter((gmb_int_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (const double*)"); emit_var_ref(c, wv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, wv); sb_put(&c->out, "), ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data)");
    return 1;
}
static int sg_cheby1(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    var_t* wv = NULL; if (!sg_wn(c, a[2], &wv)) return 0;
    sb_put(&c->out, "gmb_filter_cheby1((gmb_int_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), "); emit_expr(c, a[1]);
    sb_put(&c->out, ", (const double*)"); emit_var_ref(c, wv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, wv); sb_put(&c->out, "), ");
    emit_str_operand(c, a[3]); sb_put(&c->out, ".data)");
    return 1;
}
static int sg_cheby2(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    var_t* wv = NULL; if (!sg_wn(c, a[2], &wv)) return 0;
    sb_put(&c->out, "gmb_filter_cheby2((gmb_int_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), "); emit_expr(c, a[1]);
    sb_put(&c->out, ", (const double*)"); emit_var_ref(c, wv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, wv); sb_put(&c->out, "), ");
    emit_str_operand(c, a[3]); sb_put(&c->out, ".data)");
    return 1;
}
static int sg_ellip(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 5) return 0;
    var_t* wv = NULL; if (!sg_wn(c, a[3], &wv)) return 0;
    sb_put(&c->out, "gmb_filter_ellip((gmb_int_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), "); emit_expr(c, a[1]);
    sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", (const double*)"); emit_var_ref(c, wv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, wv); sb_put(&c->out, "), ");
    emit_str_operand(c, a[4]); sb_put(&c->out, ".data)");
    return 1;
}
static int sg_welch(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    var_t* xv = NULL; if (!sg_wn(c, a[0], &xv)) return 0;
    sb_put(&c->out, "gmb_welch_c99((const double*)"); emit_var_ref(c, xv);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, xv); sb_put(&c->out, "), (int)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), "); emit_expr(c, a[2]); sb_put(&c->out, ")");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int); } sg_row;
static const sg_row SG_TBL[] = {
    { GMB_TOK_SignalButter, sg_butter },
    { GMB_TOK_SignalCheby1, sg_cheby1 },
    { GMB_TOK_SignalCheby2, sg_cheby2 },
    { GMB_TOK_SignalEllip, sg_ellip },
    { GMB_TOK_SignalWelch, sg_welch },
};
/* ── dominio: torch (tensor ops + model, delegazione §0quater) ── */
#define TG_UN(nm, fn) static int nm(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out, fn "("); emit_expr(c,a[0]); sb_put(&c->out, ")"); return 1; }
#define TG_BIN(nm, fn) static int nm(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out, fn "("); emit_expr(c,a[0]); sb_put(&c->out, ", "); emit_expr(c,a[1]); sb_put(&c->out, ")"); return 1; }
TG_UN(tg_relu, "gmb_ml_tensor_relu") TG_UN(tg_sigmoid, "gmb_ml_tensor_sigmoid") TG_UN(tg_tanh, "gmb_ml_tensor_tanh")
TG_UN(tg_softmax, "gmb_ml_tensor_softmax") TG_UN(tg_exp, "gmb_ml_tensor_exp") TG_UN(tg_log, "gmb_ml_tensor_log")
TG_UN(tg_sqrt, "gmb_ml_tensor_sqrt") TG_UN(tg_abs, "gmb_ml_tensor_abs") TG_UN(tg_transpose, "gmb_ml_tensor_transpose")
TG_UN(tg_gelu, "gmb_ml_tensor_gelu") TG_UN(tg_sum, "gmb_ml_tensor_sum") TG_UN(tg_mean, "gmb_ml_tensor_mean")
TG_UN(tg_max, "gmb_ml_tensor_max") TG_UN(tg_min, "gmb_ml_tensor_min")
TG_BIN(tg_add, "gmb_ml_tensor_add") TG_BIN(tg_sub, "gmb_ml_tensor_sub") TG_BIN(tg_mul, "gmb_ml_tensor_mul")
TG_BIN(tg_div, "gmb_ml_tensor_div") TG_BIN(tg_matmul, "gmb_ml_tensor_matmul") TG_BIN(tg_concat, "gmb_ml_tensor_concat")
TG_BIN(tg_stack, "gmb_ml_tensor_stack")
static int tg_device(ctx_t* c, gmb_expr_t** a, int n){ (void)a;(void)n; sb_put(&c->out, "gmb_ml_device_name_s()"); return 1; }
static int tg_libpath(ctx_t* c, gmb_expr_t** a, int n){ (void)a;(void)n; sb_put(&c->out, "gmb_ml_lib_path_s()"); return 1; }
static int tg_loaded(ctx_t* c, gmb_expr_t** a, int n){ (void)a;(void)n; sb_put(&c->out, "((gmb_double_t)gmb_ml_loaded())"); return 1; }
static int tg_available(ctx_t* c, gmb_expr_t** a, int n){ (void)a;(void)n; sb_put(&c->out, "gmb_ml_available_s()"); return 1; }
static int tg_create(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_tensor_create((int)("); emit_expr(c,a[0]); sb_put(&c->out,"), (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), gmb_ml_active_device())"); return 1; }
static int tg_zeros(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_tensor_zeros((int)("); emit_expr(c,a[0]); sb_put(&c->out,"), (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), gmb_ml_active_device())"); return 1; }
static int tg_ones(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_tensor_ones((int)("); emit_expr(c,a[0]); sb_put(&c->out,"), (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), gmb_ml_active_device())"); return 1; }
static int tg_eye(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_tensor_eye((int)("); emit_expr(c,a[0]); sb_put(&c->out,"), gmb_ml_active_device())"); return 1; }
static int tg_randn(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_tensor_randn((int)("); emit_expr(c,a[0]); sb_put(&c->out,"), (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), gmb_ml_active_device())"); return 1; }
static int tg_from(ctx_t* c, gmb_expr_t** a, int n){ if(n<1||a[0]->kind!=GMB_EXPR_VAR)return 0; var_t* v=lookup_var(c,a[0]->var); if(!v||!v->is_array)return 0; sb_put(&c->out,"gmb_ml_tensor_from(1, (int)("); emit_arr_len_of(c,v); sb_put(&c->out,"), (gmb_double_t*)"); emit_var_ref(c,v); sb_put(&c->out,", gmb_ml_active_device())"); return 1; }
static int tg_get(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_tensor_get("); emit_expr(c,a[0]); sb_put(&c->out,", (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"))"); return 1; }
static int tg_set(ctx_t* c, gmb_expr_t** a, int n){ if(n<4)return 0; sb_put(&c->out,"(gmb_ml_tensor_set("); emit_expr(c,a[0]); sb_put(&c->out,", (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"), "); emit_expr(c,a[3]); sb_put(&c->out,"), "); emit_expr(c,a[3]); sb_put(&c->out,")"); return 1; }
static int tg_free(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"(gmb_ml_tensor_free("); emit_expr(c,a[0]); sb_put(&c->out,"), 0)"); return 1; }
static int tg_allclose(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"(gmb_ml_tensor_allclose("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,", "); emit_expr(c,a[2]); sb_put(&c->out,") != 0 ? -1 : 0)"); return 1; }
static int tg_pow(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_tensor_pow("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,")"); return 1; }
static int tg_reshape(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_tensor_reshape("); emit_expr(c,a[0]); sb_put(&c->out,", (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"))"); return 1; }
static int tg_permute(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_tensor_permute("); emit_expr(c,a[0]); sb_put(&c->out,", (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"))"); return 1; }
static int tg_slice(ctx_t* c, gmb_expr_t** a, int n){ if(n<4)return 0; sb_put(&c->out,"gmb_ml_tensor_slice("); emit_expr(c,a[0]); sb_put(&c->out,", (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"), (int)("); emit_expr(c,a[3]); sb_put(&c->out,"))"); return 1; }
static int tg_split(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_tensor_split("); emit_expr(c,a[0]); sb_put(&c->out,", (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"))"); return 1; }
static int tg_similarity(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_similarity("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,", "); emit_str_operand(c,a[2]); sb_put(&c->out,".data)"); return 1; }
/* L7 Training: BLEU-4 + ROUGE-L dal runtime condiviso (gmb_rag_eval.c). Stringhe come .data. */
static int tg_bleu(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_bleu_score("); emit_str_operand(c,a[0]); sb_put(&c->out,".data, "); emit_str_operand(c,a[1]); sb_put(&c->out,".data)"); return 1; }
/* L5 DPO/ORPO: ms, mr, x, c, r + opts$ opzionale (5 o 6 argomenti). */
/* L6 pipeline dati testuale toy: TEXTENCODE/DECODE/DATASETLOAD. */
static int tg_imatrixload(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_imatrix_load_t("); emit_str_operand(c,a[0]); sb_put(&c->out,".data)"); return 1; }
static int tg_imatrixranks(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_imatrix_ranks_t("); emit_str_operand(c,a[0]); sb_put(&c->out,".data, (int)("); emit_expr(c,a[1]); sb_put(&c->out,"), (int)("); emit_expr(c,a[2]); sb_put(&c->out,"))"); return 1; }
static int tg_modelexport(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_model_export("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_str_operand(c,a[1]); sb_put(&c->out,".data, "); if(n>=3){ emit_str_operand(c,a[2]); sb_put(&c->out,".data"); } else sb_put(&c->out,"NULL"); sb_put(&c->out,")"); return 1; }
static int tg_textencode(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_text_encode("); emit_str_operand(c,a[0]); sb_put(&c->out,".data)"); return 1; }
static int tg_textdecode(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_text_decode_s("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_datasetload(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_dataset_load_text("); emit_str_operand(c,a[0]); sb_put(&c->out,".data"); if(n>=2){ sb_put(&c->out,", "); emit_str_operand(c,a[1]); sb_put(&c->out,".data"); } else sb_put(&c->out,", NULL"); sb_put(&c->out,")"); return 1; }
static int tg_dpostep(ctx_t* c, gmb_expr_t** a, int n){ if(n<5)return 0; sb_put(&c->out,"gmb_ml_model_dpo_step("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,", "); emit_expr(c,a[2]); sb_put(&c->out,", "); emit_expr(c,a[3]); sb_put(&c->out,", "); emit_expr(c,a[4]); if(n>=6){ sb_put(&c->out,", "); emit_str_operand(c,a[5]); sb_put(&c->out,".data"); } else { sb_put(&c->out,", (void*)0"); } sb_put(&c->out,")"); return 1; }
static int tg_rouge(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_rouge_l("); emit_str_operand(c,a[0]); sb_put(&c->out,".data, "); emit_str_operand(c,a[1]); sb_put(&c->out,".data)"); return 1; }
static int tg_dtype(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_tensor_dtype_s("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_deviceof(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_tensor_device_s("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_toarray(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_tensor_toarray_alloc("); emit_expr(c,a[0]); sb_put(&c->out,", NULL)"); return 1; }
static int tg_shape(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_tensor_shape_alloc("); emit_expr(c,a[0]); sb_put(&c->out,", NULL)"); return 1; }
static int tg_argmax(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_tensor_argmax("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_loss(ctx_t* c, gmb_expr_t** a, int n, const char* k){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_loss("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_printf(&c->out,", \"%s\")", k); return 1; }
static int tg_lossmse(ctx_t* c, gmb_expr_t** a, int n){ return tg_loss(c,a,n,"mse"); }
static int tg_lossce(ctx_t* c, gmb_expr_t** a, int n){ return tg_loss(c,a,n,"crossentropy"); }
static int tg_lossbin(ctx_t* c, gmb_expr_t** a, int n){ return tg_loss(c,a,n,"binary"); }
static int tg_accuracy(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_accuracy("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,")"); return 1; }
static int tg_modelcreate(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_model_create("); emit_str_operand(c,a[0]); sb_put(&c->out,".data, "); emit_str_operand(c,a[1]); sb_put(&c->out,".data)"); return 1; }
static int tg_modelfree(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"(gmb_ml_model_free("); emit_expr(c,a[0]); sb_put(&c->out,"), 0)"); return 1; }
static int tg_modelreset(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"((gmb_double_t)gmb_ml_model_reset("); emit_expr(c,a[0]); sb_put(&c->out,"))"); return 1; }
static int tg_modelsave(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"((gmb_double_t)gmb_ml_model_save("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_str_operand(c,a[1]); sb_put(&c->out,".data))"); return 1; }
static int tg_modelinput(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_model_input("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_modeloutput(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_model_output("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_modelforward(ctx_t* c, gmb_expr_t** a, int n){ if(n<2)return 0; sb_put(&c->out,"gmb_ml_model_forward("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,")"); return 1; }
static int tg_modelparams(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_model_parameters("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_modelloss(ctx_t* c, gmb_expr_t** a, int n){ if(n<3)return 0; sb_put(&c->out,"gmb_ml_model_loss("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,", "); emit_expr(c,a[2]); sb_put(&c->out,")"); return 1; }
static int tg_modelstate(ctx_t* c, gmb_expr_t** a, int n){ if(n<1)return 0; sb_put(&c->out,"gmb_ml_model_state_s("); emit_expr(c,a[0]); sb_put(&c->out,")"); return 1; }
static int tg_modeltrainstep(ctx_t* c, gmb_expr_t** a, int n){ if(n<6)return 0; if(n>=7){ sb_put(&c->out,"gmb_ml_model_train_step_ex("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,", "); emit_expr(c,a[2]); sb_put(&c->out,", "); emit_str_operand(c,a[3]); sb_put(&c->out,".data, "); emit_str_operand(c,a[4]); sb_put(&c->out,".data, "); emit_expr(c,a[5]); sb_put(&c->out,", "); emit_str_operand(c,a[6]); sb_put(&c->out,".data)"); return 1; } sb_put(&c->out,"gmb_ml_model_train_step("); emit_expr(c,a[0]); sb_put(&c->out,", "); emit_expr(c,a[1]); sb_put(&c->out,", "); emit_expr(c,a[2]); sb_put(&c->out,", "); emit_str_operand(c,a[3]); sb_put(&c->out,".data, "); emit_str_operand(c,a[4]); sb_put(&c->out,".data, "); emit_expr(c,a[5]); sb_put(&c->out,")"); return 1; }
typedef struct { gmb_tokenkind_t tk; int (*fn)(ctx_t*, gmb_expr_t**, int); } tg_row;
static const tg_row TORCH_TBL[] = {
    { GMB_TOK_TorchDevice, tg_device }, { GMB_TOK_TorchLibPath, tg_libpath }, { GMB_TOK_TorchLoaded, tg_loaded },
    { GMB_TOK_TorchAvailable, tg_available }, { GMB_TOK_TorchCreate, tg_create }, { GMB_TOK_TorchFrom, tg_from },
    { GMB_TOK_TorchAdd, tg_add }, { GMB_TOK_TorchSub, tg_sub }, { GMB_TOK_TorchMul, tg_mul }, { GMB_TOK_TorchDiv, tg_div },
    { GMB_TOK_TorchMatmul, tg_matmul }, { GMB_TOK_TorchRelu, tg_relu }, { GMB_TOK_TorchGet, tg_get }, { GMB_TOK_TorchSet, tg_set },
    { GMB_TOK_TorchToArray, tg_toarray }, { GMB_TOK_TorchShape, tg_shape }, { GMB_TOK_TorchFree, tg_free },
    { GMB_TOK_TorchAllclose, tg_allclose }, { GMB_TOK_TorchSum, tg_sum }, { GMB_TOK_TorchMean, tg_mean },
    { GMB_TOK_TorchZeros, tg_zeros }, { GMB_TOK_TorchOnes, tg_ones }, { GMB_TOK_TorchEye, tg_eye }, { GMB_TOK_TorchRandn, tg_randn },
    { GMB_TOK_TorchSigmoid, tg_sigmoid }, { GMB_TOK_TorchTanh, tg_tanh }, { GMB_TOK_TorchSoftmax, tg_softmax },
    { GMB_TOK_TorchExp, tg_exp }, { GMB_TOK_TorchLog, tg_log }, { GMB_TOK_TorchSqrt, tg_sqrt }, { GMB_TOK_TorchAbs, tg_abs },
    { GMB_TOK_TorchPow, tg_pow }, { GMB_TOK_TorchTranspose, tg_transpose }, { GMB_TOK_TorchReshape, tg_reshape },
    { GMB_TOK_TorchMax, tg_max }, { GMB_TOK_TorchMin, tg_min }, { GMB_TOK_TorchGelu, tg_gelu }, { GMB_TOK_TorchPermute, tg_permute },
    { GMB_TOK_TorchSlice, tg_slice }, { GMB_TOK_TorchConcat, tg_concat }, { GMB_TOK_TorchStack, tg_stack }, { GMB_TOK_TorchSplit, tg_split },
    { GMB_TOK_TorchSimilarity, tg_similarity }, { GMB_TOK_TorchBleu, tg_bleu }, { GMB_TOK_TorchRouge, tg_rouge }, { GMB_TOK_TorchDpoStep, tg_dpostep }, { GMB_TOK_TorchTextEncode, tg_textencode }, { GMB_TOK_TorchTextDecode, tg_textdecode }, { GMB_TOK_TorchDatasetLoad, tg_datasetload }, { GMB_TOK_TorchModelExport, tg_modelexport }, { GMB_TOK_TorchImatrixLoad, tg_imatrixload }, { GMB_TOK_TorchImatrixRanks, tg_imatrixranks }, { GMB_TOK_TorchDtype, tg_dtype }, { GMB_TOK_TorchDeviceOf, tg_deviceof },
    { GMB_TOK_TorchArgmax, tg_argmax },
    { GMB_TOK_TorchLossMse, tg_lossmse }, { GMB_TOK_TorchLossCrossEntropy, tg_lossce }, { GMB_TOK_TorchLossBinary, tg_lossbin },
    { GMB_TOK_TorchAccuracy, tg_accuracy }, { GMB_TOK_TorchModelCreate, tg_modelcreate }, { GMB_TOK_TorchModelFree, tg_modelfree },
    { GMB_TOK_TorchModelReset, tg_modelreset }, { GMB_TOK_TorchModelSave, tg_modelsave }, { GMB_TOK_TorchModelInput, tg_modelinput },
    { GMB_TOK_TorchModelOutput, tg_modeloutput }, { GMB_TOK_TorchModelForward, tg_modelforward },
    { GMB_TOK_TorchModelParameters, tg_modelparams }, { GMB_TOK_TorchModelLoss, tg_modelloss },
    { GMB_TOK_TorchModelState, tg_modelstate }, { GMB_TOK_TorchModelTrainStep, tg_modeltrainstep },
};
int emit_bf_torch(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(TORCH_TBL) / sizeof(TORCH_TBL[0]); i++)
        if (TORCH_TBL[i].tk == tk) return TORCH_TBL[i].fn(c, args, nargs);
    return 0;
}

/* SPATIAL — matrici 2D C99Native = flat gmb_double_t* con bound <v>__b0/__b1:
 * si usano le runtime *_flat (stesso risultato delle *_c99 jagged). */
static var_t* sp_matrix_var(ctx_t* c, gmb_expr_t* e) {
    if (!e || e->kind != GMB_EXPR_VAR) return NULL;
    var_t* v = lookup_var(c, e->var);
    return (v && v->is_array) ? v : NULL;
}
static void sp_flat_args(ctx_t* c, var_t* v, int rows, int cols) {
    emit_var_ref(c, v);
    if (rows) { sb_put(&c->out, ", (int)("); sb_bound_var(&c->out, v, 0); sb_put(&c->out, " + 1)"); }
    if (cols) { sb_put(&c->out, ", (int)("); sb_bound_var(&c->out, v, 1); sb_put(&c->out, " + 1)"); }
}
static int sp_cdist_common(ctx_t* c, gmb_expr_t** a, int n, int euclid_only) {
    if (n < 2) return 0;
    var_t* xv = sp_matrix_var(c, a[0]);
    var_t* yv = sp_matrix_var(c, a[1]);
    if (!xv || !yv) return 0;
    if (xv->jagged && yv->jagged) {
        /* DIM X(n) AS ARRAY OF DOUBLE: jagged gmb_double_t** (2 colonne) → runtime *_c99. */
        sb_put(&c->out, euclid_only ? "gmb_distance_matrix_c99(" : "gmb_cdist_c99(");
        emit_var_ref(c, xv);
        sb_put(&c->out, ", (int)("); sb_bound_var(&c->out, xv, 0); sb_put(&c->out, " + 1), 2, ");
        emit_var_ref(c, yv);
        sb_put(&c->out, ", (int)("); sb_bound_var(&c->out, yv, 0); sb_put(&c->out, " + 1), 2");
        if (!euclid_only) {
            sb_put(&c->out, ", ");
            if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data"); }
            else sb_put(&c->out, "\"euclidean\"");
        }
        sb_put(&c->out, ")");
        return 1;
    }
    sb_put(&c->out, "({ gmb_int_t _r = 0, _cc = 0; double* _m = ");
    sb_put(&c->out, euclid_only ? "gmb_distance_matrix_flat((const double*)" : "gmb_cdist_flat((const double*)");
    sp_flat_args(c, xv, 1, 1);
    sb_put(&c->out, ", (const double*)");
    sp_flat_args(c, yv, 1, 1);
    sb_put(&c->out, ", ");
    if (!euclid_only) {
        if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); }
        else sb_put(&c->out, "\"euclidean\", ");
    }
    sb_put(&c->out, "&_r, &_cc); gmb_int_t _h = gmb_matrix_create_from(_r, _cc, _m); free(_m); _h; })");
    return 1;
}
static int sp_cdist(ctx_t* c, gmb_expr_t** a, int n) { return sp_cdist_common(c, a, n, 0); }
static int sp_distancematrix(ctx_t* c, gmb_expr_t** a, int n) { return sp_cdist_common(c, a, n, 1); }
int emit_bf_spatial(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    switch (tk) {
        case GMB_TOK_SpatialCdist: return sp_cdist(c, args, nargs);
        case GMB_TOK_SpatialDistanceMatrix: return sp_distancematrix(c, args, nargs);
        default: return 0;
    }
}

int emit_bf_signal(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(SG_TBL) / sizeof(SG_TBL[0]); i++)
        if (SG_TBL[i].tk == tk) return SG_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: analysis (delegazione §0quater) ──
 * Nuclei ANALYSIS (scipy integrate/interpolate/fft): le funzioni emettono chiamate
 * alle gmb_* del runtime CONdiviso (mai inline, §0bis). I TokenKind ANALYSIS-specifici
 * (Trapezoid, CubicSpline, Interpolate*, EuclideanDistanceSquared) non erano raggiunti:
 * il driver gmb_bf_composite li mappa qui. Gli argomenti array sono variabili native
 * (pattern sa_*): ptr via emit_var_ref + (int)len via emit_arr_len_of. */
static int an_arr_is(ctx_t* c, gmb_expr_t* a, var_t** out) {
    if (!a || a->kind != GMB_EXPR_VAR || !a->var) return 0;
    var_t* av = lookup_var(c, a->var);
    if (!av || !av->is_array || av->matrix || av->vclass[0]) return 0;
    *out = av; return 1;
}
static int an_2arr1scalar(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 3) return 0;
    var_t* x; var_t* y;
    if (!an_arr_is(c, a[0], &x) || !an_arr_is(c, a[1], &y)) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_var_ref(c, x); sb_put(&c->out, ", "); emit_var_ref(c, y);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, "), ");
    emit_expr(c, a[2]); sb_put(&c->out, ")"); return 1;
}
static int an_2arr(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 2) return 0;
    var_t* x; var_t* y;
    if (!an_arr_is(c, a[0], &x) || !an_arr_is(c, a[1], &y)) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_var_ref(c, x); sb_put(&c->out, ", "); emit_var_ref(c, y);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, "))");
    return 1;
}
static int an_trapezoid(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr(c, a, n, "gmb_trapezoid"); }
static int an_euclid(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr(c, a, n, "gmb_euclidean_distance_squared"); }
static int an_cubicspline(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr1scalar(c, a, n, "gmb_cubic_spline"); }
static int an_interp_linear(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr1scalar(c, a, n, "gmb_interp_linear"); }
static int an_interp_step(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr1scalar(c, a, n, "gmb_interp_step"); }
static int an_interp_polynomial(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr1scalar(c, a, n, "gmb_interp_polynomial"); }
static int an_interp_pchip(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr1scalar(c, a, n, "gmb_interp_pchip"); }
static int an_interp_spline(ctx_t* c, gmb_expr_t** a, int n) { return an_2arr1scalar(c, a, n, "gmb_interp_akima"); }
/* INTERPOLATECLAMP(x, y, tx) — clamp ai bordi + interp lineare (canone emitter C#). */
static int an_interp_clamp(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 3) return 0;
    var_t* x; var_t* y;
    if (!an_arr_is(c, a[0], &x) || !an_arr_is(c, a[1], &y)) return 0;
    sb_put(&c->out, "gmb_interp_clamp("); emit_var_ref(c, x); sb_put(&c->out, ", "); emit_var_ref(c, y);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, "), ");
    emit_expr(c, a[2]); sb_put(&c->out, ", gmb_interp_akima)"); return 1;
}
/* INTERPOLATEGENERIC(method, x, y, tx) — dispatcher per metodo (parity IL/PCode:
 * "linear"/"spline"/"poly"/"polynomial"/"pchip"/"step"). emitter C# mappa solo "linear";
 * qui supportiamo linear/step/pchip/akima/polynomial col metodo richiesto. */
static int an_interp_generic(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 4) return 0;
    var_t* x; var_t* y;
    if (a[0]->kind != GMB_EXPR_STR || !an_arr_is(c, a[1], &x) || !an_arr_is(c, a[2], &y)) return 0;
    const char* meth = NULL;
    if (a[0]->str) {
        if (!strcasecmp(a[0]->str, "LINEAR")) meth = "gmb_interp_linear";
        else if (!strcasecmp(a[0]->str, "STEP")) meth = "gmb_interp_step";
        else if (!strcasecmp(a[0]->str, "POLY") || !strcasecmp(a[0]->str, "POLYNOMIAL")) meth = "gmb_interp_polynomial";
        else if (!strcasecmp(a[0]->str, "PCHIP")) meth = "gmb_interp_pchip";
        else if (!strcasecmp(a[0]->str, "SPLINE")) meth = "gmb_interp_akima";
    }
    if (!meth) return 0;
    sb_put(&c->out, meth); sb_put(&c->out, "("); emit_var_ref(c, x); sb_put(&c->out, ", "); emit_var_ref(c, y);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, "), ");
    emit_expr(c, a[3]); sb_put(&c->out, ")"); return 1;
}
/* λ come valore gmb_fn_t (il pre-pass l'ha lowerizzata a VAR di funzione sintetica;
 * emit_expr sul VAR funzione produce ((gmb_fn_t){ .fn = ..., .ctx = ... })). */
static int an_fna(ctx_t* c, gmb_expr_t** a, int n, const char* fn, int nreq) {
    if (n < nreq) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[0]);
    for (int i = 1; i < nreq; i++) { sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ")"); return 1;
}
static int an_integrate(ctx_t* c, gmb_expr_t** a, int n) { return an_fna(c, a, n, "gmb_quad_gk_c99", 3); }
static int an_simpson(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    sb_put(&c->out, "gmb_simpson_c99("); emit_expr(c, a[0]);
    sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", (int)("); emit_expr(c, a[3]); sb_put(&c->out, "))"); return 1;
}
static int an_differentiate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "({ gmb_fn_t _df = "); emit_expr(c, a[0]);
    sb_put(&c->out, "; gmb_double_t _dx = "); emit_expr(c, a[1]);
    sb_put(&c->out, "; gmb_double_t _dh = 1e-6; (((gmb_double_t(*)(void*, gmb_double_t))_df.fn)(_df.ctx, _dx + _dh) - ((gmb_double_t(*)(void*, gmb_double_t))_df.fn)(_df.ctx, _dx - _dh)) / (2 * _dh); })");
    return 1;
}
/* RFFT: gmb_rfft_c99(x, n) → gmb_int_t (handle/indice del risultato; parity emitter C#). */
static int an_rfft(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    var_t* x = NULL;
    if (!an_arr_is(c, a[0], &x)) return 0;
    sb_put(&c->out, "gmb_rfft_c99("); emit_var_ref(c, x);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, "))");
    return 1;
}
/* SOLVEIVP: gmb_solve_ivp_c99(fn, y0, n, t0, t1) → gmb_int_t (n° punti; parity emitter C#). */
static int an_solveivp(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    var_t* y0 = NULL;
    if (!an_arr_is(c, a[1], &y0)) return 0;
    sb_put(&c->out, "gmb_solve_ivp_c99("); emit_expr(c, a[0]);
    sb_put(&c->out, ", (double*)"); emit_var_ref(c, y0);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, y0); sb_put(&c->out, "), ");
    emit_expr(c, a[2]); sb_put(&c->out, ", "); emit_expr(c, a[3]); sb_put(&c->out, ")");
    return 1;
}
/* ADFTEST/HOLTWINTERS/ARIMA: risultato UDT boxed (gmb_val_t dict), parity col ramo
 * assegnazione (gmb_*_result). Servono al DIM: `DIM x = TIMESERIES.AdfTest(...)`. */
static int an_adftest(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    var_t* x = NULL;
    if (!an_arr_is(c, a[0], &x)) return 0;
    sb_put(&c->out, "gmb_adf_test_result((const double*)");
    emit_var_ref(c, x); sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, "))");
    return 1;
}
static int an_arima(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    var_t* x = NULL;
    if (!an_arr_is(c, a[0], &x)) return 0;
    sb_put(&c->out, "gmb_arima_result((const double*)");
    emit_var_ref(c, x); sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, ")");
    for (int i = 1; i <= 3; i++) { sb_put(&c->out, ", (int)("); emit_expr(c, a[i]); sb_put(&c->out, ")"); }
    sb_put(&c->out, ")"); return 1;
}
static int an_holtwinters(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 5) return 0;
    var_t* x = NULL;
    if (!an_arr_is(c, a[0], &x)) return 0;
    sb_put(&c->out, "gmb_holt_winters_result((const double*)");
    emit_var_ref(c, x); sb_put(&c->out, ", (int)("); emit_arr_len_of(c, x); sb_put(&c->out, ")");
    for (int i = 1; i <= 4; i++) { sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ")"); return 1;
}
static const sa_row AN_TBL[] = {
    { GMB_TOK_AdfTest, an_adftest },
    { GMB_TOK_Arima, an_arima },
    { GMB_TOK_HoltWinters, an_holtwinters },
    { GMB_TOK_Trapezoid, an_trapezoid },
    { GMB_TOK_FftRfft, an_rfft },
    { GMB_TOK_SolveIvp, an_solveivp },
    { GMB_TOK_CubicSpline, an_cubicspline },
    { GMB_TOK_InterpolateLinear, an_interp_linear },
    { GMB_TOK_InterpolateStep, an_interp_step },
    { GMB_TOK_InterpolatePolynomial, an_interp_polynomial },
    { GMB_TOK_InterpolatePchip, an_interp_pchip },
    { GMB_TOK_InterpolateSpline, an_interp_spline },
    { GMB_TOK_InterpolateClamp, an_interp_clamp },
    { GMB_TOK_InterpolateGeneric, an_interp_generic },
    { GMB_TOK_EuclideanDistanceSquared, an_euclid },
    { GMB_TOK_Integrate, an_integrate },
    { GMB_TOK_Simpson, an_simpson },
    { GMB_TOK_Differentiate, an_differentiate },
};
int emit_bf_analysis(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(AN_TBL) / sizeof(AN_TBL[0]); i++)
        if (AN_TBL[i].tk == tk) return AN_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: finance (delegazione §0quater) ── */
static int fd_duration(ctx_t* c, gmb_expr_t** a, int n);
static int fd_yearfrac(ctx_t* c, gmb_expr_t** a, int n);
/* TRADING su array di rendimenti: gmb_tm_holding_period_return/
 * gmb_tm_geometric_mean_return richiedono (const double*, int). */
static int fd_ret_stat(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1 || !a[0] || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!(av && av->is_array && !av->matrix && !av->vclass[0])) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "("); emit_var_ref(c, av);
    sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "))");
    return 1;
}
static int fd_holding(ctx_t* c, gmb_expr_t** a, int n) { return fd_ret_stat(c, a, n, "gmb_tm_holding_period_return"); }
static int fd_geomean(ctx_t* c, gmb_expr_t** a, int n) { return fd_ret_stat(c, a, n, "gmb_tm_geometric_mean_return"); }
/* Merton: il registro BASIC ordina (S,K,T,r,v,q) — 5°=vol, 6°=dividend yield — mentre la
 * firma gmb_fin_merton_* è (s,k,t,r,q,v). Gli handler scambiano i due argomenti. */
static int ma_merton_call(ctx_t* c, gmb_expr_t** a, int n) { if (n != 6) return 0; sb_put(&c->out, "gmb_fin_merton_call("); for (int i = 0; i < 6; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i == 4 ? 5 : i == 5 ? 4 : i]); } sb_put(&c->out, ")"); return 1; }
static int ma_merton_put(ctx_t* c, gmb_expr_t** a, int n) { if (n != 6) return 0; sb_put(&c->out, "gmb_fin_merton_put("); for (int i = 0; i < 6; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i == 4 ? 5 : i == 5 ? 4 : i]); } sb_put(&c->out, ")"); return 1; }
/* BINOMIALOPTION: canone IL BinomialOption(...,steps,string type) → isPut = type=="PUT"
 * (CommonLib europeo, isAmerican=false). Il BASIC converte i numerici a stringa → mai PUT:
 * is_put=1 SOLO se il 7° argomento è il letterale "PUT" (rilevato a compile-time). */
static int ma_binopt(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 7) return 0;
    int is_put = 0;
    if (a[6] && a[6]->kind == GMB_EXPR_STR && a[6]->str && !strcasecmp(a[6]->str, "PUT")) is_put = 1;
    sb_put(&c->out, "gmb_fin_binomial(");
    for (int i = 0; i < 5; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", (int)("); emit_expr(c, a[5]);     sb_printf(&c->out, "), %d, 0)", is_put);
    return 1;
}
/* DELTA/RHO/THETA/CHARM: canone IL (...,string type) → isPut = type=="PUT". Il BASIC
 * converte i numerici a stringa (es. 1 → "1") → mai PUT: is_put=1 SOLO per il letterale
 * "PUT" (rilevato a compile-time), come ma_binopt. */
static int ma_isput_last(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 6) return 0;
    int is_put = (a[5] && a[5]->kind == GMB_EXPR_STR && a[5]->str && !strcasecmp(a[5]->str, "PUT")) ? 1 : 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    for (int i = 0; i < 5; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_printf(&c->out, ", %d)", is_put);
    return 1;
}
static int ma_delta(ctx_t* c, gmb_expr_t** a, int n) { return ma_isput_last(c, a, n, "gmb_fin_delta"); }
static int ma_rho(ctx_t* c, gmb_expr_t** a, int n) { return ma_isput_last(c, a, n, "gmb_fin_rho"); }
static int ma_theta(ctx_t* c, gmb_expr_t** a, int n) { return ma_isput_last(c, a, n, "gmb_fin_theta"); }
static int ma_charm(ctx_t* c, gmb_expr_t** a, int n) { return ma_isput_last(c, a, n, "gmb_fin_charm"); }
static const ma_row FN_TBL[] = {
    { GMB_TOK_BookValuePerShare, 2, "gmb_fin_bvps", NULL },
    { GMB_TOK_BsCall, 5, "gmb_fin_bs_call", NULL },
    { GMB_TOK_BsPut, 5, "gmb_fin_bs_put", NULL },
    { GMB_TOK_BinomialOption, 7, NULL, ma_binopt },
    { GMB_TOK_Charm, 6, NULL, ma_charm },
    { GMB_TOK_DeltaSpeed, 5, "gmb_fin_speed", NULL },
    { GMB_TOK_DPS, 2, "gmb_fin_dps", NULL },
    { GMB_TOK_Duration, 0, NULL, fd_duration },
    { GMB_TOK_EquityMultiplier, 2, "gmb_fin_equity_multiplier", NULL },
    { GMB_TOK_Gamma, 5, "gmb_fin_gamma", NULL },
    { GMB_TOK_GammaColor, 5, "gmb_fin_color", NULL },
    { GMB_TOK_MertonCall, 6, "gmb_fin_merton_call", ma_merton_call },
    { GMB_TOK_MertonPut, 6, "gmb_fin_merton_put", ma_merton_put },
    { GMB_TOK_NAV, 3, "gmb_fin_nav", NULL },
    { GMB_TOK_PreferredStockValue, 2, "gmb_fin_preferred_value", NULL },
    { GMB_TOK_Sln, 3, "gmb_fin_sln", NULL },
    { GMB_TOK_StockValueZeroGrowth, 2, "gmb_fin_stock_zero_growth", NULL },
    { GMB_TOK_Syd, 4, "gmb_fin_syd", NULL },
    { GMB_TOK_Vanna, 5, "gmb_fin_vanna", NULL },
    { GMB_TOK_Vega, 5, "gmb_fin_vega", NULL },
    { GMB_TOK_Volga, 5, "gmb_fin_volga", NULL },
    { GMB_TOK_YieldToMaturity, 4, "gmb_fin_ytm", NULL },
    { GMB_TOK_ZeroCouponBondValue, 3, "gmb_fin_zcb_value", NULL },
    { GMB_TOK_ZeroCouponBondYield, 3, "gmb_fin_zcb_yield", NULL },
    { GMB_TOK_Zomma, 5, "gmb_fin_zomma", NULL },
    /* TRADING azioni/rendimenti (2026-09): equity + metriche scalari */
    { GMB_TOK_StockValueConstGrowth, 3, "gmb_trd_stock_const_growth", NULL },
    { GMB_TOK_CAPM, 3, "gmb_trd_capm", NULL },
    { GMB_TOK_TaxEquivalentYield, 2, "gmb_tm_tax_equivalent_yield", NULL },
    { GMB_TOK_RiskPremium, 2, "gmb_tm_risk_premium", NULL },
    { GMB_TOK_TotalStockReturn, 3, "gmb_tm_total_stock_return", NULL },
    { GMB_TOK_CapitalGainsYield, 2, "gmb_tm_capital_gains_yield", NULL },
    { GMB_TOK_GrowthRate, 2, "gmb_tm_growth_rate", NULL },
    { GMB_TOK_BidAskSpread, 2, "gmb_tm_bid_ask_spread", NULL },
    { GMB_TOK_HoldingPeriodReturn, 0, NULL, fd_holding },
    { GMB_TOK_GeometricMeanReturn, 0, NULL, fd_geomean },
    { GMB_TOK_BondEquivalentYield, 3, "gmb_trd_bond_equiv_yield", NULL },
    { GMB_TOK_CurrentYield, 2, "gmb_trd_current_yield", NULL },
    { GMB_TOK_Delta, 6, NULL, ma_delta },
    { GMB_TOK_Rho, 6, NULL, ma_rho },
    { GMB_TOK_Theta, 6, NULL, ma_theta },
    /* TRADING.METRICS scalari (2026-09) */
    { GMB_TOK_EPS, 2, "gmb_fin_eps", NULL },
    { GMB_TOK_PERatio, 2, "gmb_trd_pe_ratio", NULL },
    { GMB_TOK_DilutedEPS, 3, "gmb_fin_diluted_eps", NULL },
    { GMB_TOK_DividendPayoutRatio, 2, "gmb_fin_dividend_payout_ratio", NULL },
    { GMB_TOK_DividendYield, 2, "gmb_trd_dividend_yield", NULL },
    { GMB_TOK_PSRatio, 2, "gmb_fin_ps_ratio", NULL },
    { GMB_TOK_PBRatio, 2, "gmb_fin_pb_ratio", NULL },
};
int emit_bf_finance(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(FN_TBL) / sizeof(FN_TBL[0]); i++)
        if (FN_TBL[i].tk == tk) return ma_emit(c, &FN_TBL[i], args, nargs);
    return 0;
}
/* ── dominio: finance_defaults (delegazione §0quater) ── */
typedef struct { const char* ns; gmb_tokenkind_t tk; ma_sp sp; } fd_row;
static int fd_days360(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_fin_days360("); emit_expr(c, a[0]);
    sb_put(&c->out, ", "); emit_expr(c, a[1]);
    sb_put(&c->out, ", ");
    if (n >= 3) { sb_put(&c->out, "(int)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ")"); return 1;
}
static int fd_pricesimple(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 5) return 0;
    sb_put(&c->out, "gmb_fin_price_simple(");
    for (int i = 0; i < 5; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", ");
    if (n >= 6) { sb_put(&c->out, "(int)("); emit_expr(c, a[5]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "1");
    sb_put(&c->out, ")"); return 1;
}
static int fd_bondprice(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 6) return 0;
    /* BondPrice(settle,mat,rate,yld,redemp,freq[,model]): model 0=trading, 1=numerics */
    sb_put(&c->out, "((int)(");
    if (n > 6) emit_expr(c, a[6]); else sb_put(&c->out, "0");
    sb_put(&c->out, ") == 1 ? gmb_fin_bond_price(");
    for (int i = 0; i < 5; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", (int)("); emit_expr(c, a[5]); sb_put(&c->out, "), 0) : gmb_trd_bond_price(");
    for (int i = 0; i < 5; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", (int)("); emit_expr(c, a[5]); sb_put(&c->out, ")))"); return 1;
}
static int fd_bondyield(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 6) return 0;
    /* BondYield(settle,mat,rate,price,redemp,freq[,model]): numerics ordina
     * gmb_fin_bond_yield(settle,mat,price,redemption,rate,freq,basis); trading diretto. */
    sb_put(&c->out, "((int)(");
    if (n > 6) emit_expr(c, a[6]); else sb_put(&c->out, "0");
    sb_put(&c->out, ") == 1 ? gmb_fin_bond_yield(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[3]); sb_put(&c->out, ", "); emit_expr(c, a[4]); sb_put(&c->out, ", ");
    emit_expr(c, a[2]); sb_put(&c->out, ", (int)("); emit_expr(c, a[5]); sb_put(&c->out, "), 0) : gmb_trd_bond_yield(");
    for (int i = 0; i < 5; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", (int)("); emit_expr(c, a[5]); sb_put(&c->out, ")))"); return 1;
}
static int fd_bondequivyield(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_fin_bond_yield("); emit_expr(c, a[0]);
    sb_put(&c->out, ", "); emit_expr(c, a[1]);
    sb_put(&c->out, ", 100, 100, 0, 1, 0)"); return 1;
}
static int fd_price(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 7) return 0;
    sb_put(&c->out, "gmb_fin_bond_price(");
    for (int i = 0; i < 7; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ")"); return 1;
}
static int fd_pvext(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_fin_pvext(");
    for (int i = 0; i < 3; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", ");
    if (n > 3) emit_expr(c, a[3]);
    else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n > 4) { sb_put(&c->out, "(int)("); emit_expr(c, a[4]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ")"); return 1;
}
static int fd_nperext(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_fin_nper_ext(");
    for (int i = 0; i < 3; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", ");
    if (n > 3) emit_expr(c, a[3]);
    else sb_put(&c->out, "0");
    sb_put(&c->out, ", (int)(");
    if (n > 4) emit_expr(c, a[4]);
    else sb_put(&c->out, "0");
    sb_put(&c->out, "))"); return 1;
}
static int fd_yearfrac(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_fin_yearfrac("); emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]);
    sb_put(&c->out, ", (int)(");
    if (n > 2) emit_expr(c, a[2]); else sb_put(&c->out, "0");
    sb_put(&c->out, "))"); return 1;
}
static int fd_duration(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    sb_put(&c->out, "gmb_fin_duration(");
    for (int i = 0; i < 4; i++) { if (i) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_put(&c->out, ", (int)("); if (n > 4) emit_expr(c, a[4]); else sb_put(&c->out, "1"); sb_put(&c->out, ")");
    sb_put(&c->out, ", (int)("); if (n > 5) emit_expr(c, a[5]); else sb_put(&c->out, "0"); sb_put(&c->out, ")");
    sb_put(&c->out, ", 0)"); return 1;
}
static int fd_ensurecollection(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_rag_ensure_collection_fmt(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { sb_put(&c->out, "(int)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "8");
    sb_put(&c->out, ", \"cosine\", 0, ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ")"); return 1;
}
static int fd_index(ctx_t* c, gmb_expr_t** a, int n) {
    /* AI RAG INDEX TEXT|META db, coll, source, text [, metadata] →
     * gmb_rag_index_source(db, coll, source, text, metadata, 0, 0) */
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_rag_index_source(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 4) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[3]); sb_put(&c->out, ".data, "); }
    else { sb_put(&c->out, "\"doc\""); sb_put(&c->out, ", "); emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); }
    if (n >= 5) { sb_put(&c->out, "("); emit_str_operand(c, a[4]); sb_put(&c->out, ".data)"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ", 0, 0)"); return 1;
}
static int fd_deletetext(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_rag_delete_source(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_listcollections(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rag_list_collections(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_gentest(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_vectordb_gentest(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))"); return 1;
}
/* ── VECTORDB eval advanced (diagnose, calibrate, gate, diff, baseline, drift, abtest, compare) ── */
static int fd_diagnose(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_diagnose(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_diagnose_report(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_diagnose_report())"); return 1;
}
static int fd_calibrate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_calibrate(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_evalgate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_eval_gate(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_evaldiff(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_eval_diff(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_baseline(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_baseline(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_drift(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_rageval_psi(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_abtest(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    /* gmb_rageval_ab_test ritorna il JSON {z_score,p_value,effect_size} (gmb_rageval_z_test
     * ritorna double: passarlo a gmb_string_alloc era C non compilabile). */
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_ab_test(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
static int fd_compare(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rageval_compare(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
/* ── AI OBSERVE (file, log, screen) + AiObservation* functions ── */
static int fd_observe_file(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_cua_observe_file("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_observe_log(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_cua_observe_log("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (double)(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "200"); sb_put(&c->out, "))"); return 1;
}
static int fd_observe_test(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_observe_test("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_observe_database(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_observe_database("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) emit_str_operand(c, a[1]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ".data)"); return 1;
}
static int fd_observe_process(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_observe_process("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_observe_browser(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_observe_browser("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_observe_screen(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_cua_observe_screen("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_obs_latest(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n > 0) return 0;
    /* gmb_obs_latest_id ritorna gmb_string_t (gmb_observation.h): NON avvolgerlo in
     * gmb_string_alloc (passava gmb_string_t a un parametro const char* → C non compilante,
     * come gmb_ctx_view). Idem per json/hash/list/file e gmb_state_load. */
    sb_put(&c->out, "gmb_obs_latest_id()"); return 1;
}
static int fd_obs_json(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_obs_json((int)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1;
}
static int fd_obs_hash(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_obs_hash((int)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1;
}
static int fd_obs_count(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n > 0) return 0;
    sb_put(&c->out, "gmb_obs_count()"); return 1;
}
static int fd_obs_list(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n > 0) return 0;
    sb_put(&c->out, "gmb_obs_list()"); return 1;
}
static int fd_obs_file(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_obs_file((int)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1;
}
static int fd_count(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_rag_count(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_search(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rag_search(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
    if (n >= 4) { sb_put(&c->out, "(int)("); emit_expr(c, a[3]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "5");
    sb_put(&c->out, ", 0))"); return 1;
}
static int fd_ask(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_rag_ask(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
    if (n >= 4) { sb_put(&c->out, "(int)("); emit_expr(c, a[3]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "5");
    sb_put(&c->out, ", 0))"); return 1;
}
/* ── AI CONTEXT (P2, 2026-09-15): budget/view engine — gmb_context.h ── */
static int fd_ctx_used(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_used("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_budget(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_budget("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_overflow(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_overflow("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_view(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* gmb_ctx_view ritorna gmb_string_t (non const char*): avvolgerlo in gmb_string_alloc
     * passava un gmb_string_t a un parametro const char* → C non compilante
     * ("incompatible integer to pointer conversion", 2026-09-16). */
    sb_put(&c->out, "gmb_ctx_view("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_set(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_ctx_set_budget("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (double)(");
    emit_expr(c, a[1]); sb_put(&c->out, "))"); return 1;
}
static int fd_ctx_viewcmd(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_ctx_set_view("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
/* P2 residuo (2026-09-16): AI CONTEXT SET POLICY|SELECTOR — completano la superficie §2. */
static int fd_ctx_setpolicy(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_ctx_set_policy("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_setselector(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_ctx_set_selector("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
/* §3.6 PARSEC (2026-10-04): AI CONTEXT builder a SEGMENTI (agente) — gmb_context.h */
static int fd_ctx_create(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctxseg_create("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_add(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_ctx_add("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_map(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_map("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_trim(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_trim("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_gc(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_gc("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_commit(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_commit("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_hash(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_ctx_hash("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_ctx_valid(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_ctx_valid("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_task_metrics(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* gmb_task_metrics ritorna char* (malloc) — lo si avvolge in una stringa BASIC. */
    sb_put(&c->out, "gmb_string_alloc(gmb_task_metrics("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
/* ── AI WORKER + AI STATE (P3, 2026-09-15): gmb_worker.h ── */
static int fd_worker_new(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_worker_new("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_replay_auto(ctx_t* c, gmb_expr_t** a, int n) {
    /* (session) → double (numero eventi rieseguiti OK). */
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_replay_auto("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_worker_manage(ctx_t* c, gmb_expr_t** a, int n) {
    /* (cycles, require, plan) — require "" = nessun auditor, plan "" = intero task graph. */
    sb_put(&c->out, "gmb_worker_manage((double)(");
    if (n >= 1) emit_expr(c, a[0]); else sb_put(&c->out, "0");
    sb_put(&c->out, "), ");
    if (n >= 2) emit_str_operand(c, a[1]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ".data, ");
    if (n >= 3) emit_str_operand(c, a[2]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ".data)"); return 1;
}
static int fd_worker_loop(ctx_t* c, gmb_expr_t** a, int n) {
    /* (task, rounds, require) — require "" = nessun audit (loop verificato assente). */
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_worker_loop("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (double)(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, "), ");
    if (n >= 3) emit_str_operand(c, a[2]); else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ".data)"); return 1;
}
static int fd_worker_invoke(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_worker_invoke("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_worker_run(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_worker_run("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_state_save(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_state_save("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_state_load(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_state_load("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int fd_worker_rounds(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_worker_rounds()"); return 1;
}
static int fd_worker_rescued(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_worker_rescued()"); return 1;
}
static int fd_worker_spend(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_worker_spend()"); return 1;
}
static const fd_row FD_TBL[] = {
    { NULL, GMB_TOK_Days360, fd_days360 },
    { NULL, GMB_TOK_PriceSimple, fd_pricesimple },
    { NULL, GMB_TOK_BondPrice, fd_bondprice },
    { NULL, GMB_TOK_BondYield, fd_bondyield },
    { NULL, GMB_TOK_BondEquivalentYield, fd_bondequivyield },
    { NULL, GMB_TOK_Price, fd_price },
    { NULL, GMB_TOK_PvExt, fd_pvext },
    { NULL, GMB_TOK_NperExt, fd_nperext },
    { NULL, GMB_TOK_YearFrac, fd_yearfrac },
    { NULL, GMB_TOK_AiVectorEnsureCollection, fd_ensurecollection },
    { NULL, GMB_TOK_AiVectorIndexText, fd_index },
    { NULL, GMB_TOK_AiVectorIndexMeta, fd_index },
    { NULL, GMB_TOK_AiVectorRemoveSource, fd_deletetext },
    { NULL, GMB_TOK_AiVectorListCollections, fd_listcollections },
    { NULL, GMB_TOK_AiVectorGenTest, fd_gentest },
    { NULL, GMB_TOK_AiVectorCountEmbeddings, fd_count },
    { NULL, GMB_TOK_AiVectorSearch, fd_search },
    { NULL, GMB_TOK_AiVectorAsk, fd_ask },
    { NULL, GMB_TOK_VectorDbDiagnose, fd_diagnose },
    { NULL, GMB_TOK_VectorDbDiagnoseReport, fd_diagnose_report },
    { NULL, GMB_TOK_VectorDbCalibrate, fd_calibrate },
    { NULL, GMB_TOK_VectorDbEvalGate, fd_evalgate },
    { NULL, GMB_TOK_VectorDbEvalDiff, fd_evaldiff },
    { NULL, GMB_TOK_VectorDbBaseline, fd_baseline },
    { NULL, GMB_TOK_VectorDbDrift, fd_drift },
    { NULL, GMB_TOK_VectorDbAbTest, fd_abtest },
    { NULL, GMB_TOK_VectorDbCompare, fd_compare },
    /* AI OBSERVE / AI.OBSERVATION* (2026-09-15) */
    { NULL, GMB_TOK_AiObserveFile, fd_observe_file },
    { NULL, GMB_TOK_AiObserveLog, fd_observe_log },
    { NULL, GMB_TOK_AiObserveScreen, fd_observe_screen },
    { NULL, GMB_TOK_AiObserveTest, fd_observe_test },
    { NULL, GMB_TOK_AiObserveDatabase, fd_observe_database },
    { NULL, GMB_TOK_AiObserveProcess, fd_observe_process },
    { NULL, GMB_TOK_AiObserveBrowser, fd_observe_browser },
    { NULL, GMB_TOK_AiObservationLatest, fd_obs_latest },
    { NULL, GMB_TOK_AiObservationGet, fd_obs_json },
    { NULL, GMB_TOK_AiObservationHash, fd_obs_hash },
    { NULL, GMB_TOK_AiObservationCount, fd_obs_count },
    { NULL, GMB_TOK_AiObservationList, fd_obs_list },
    { NULL, GMB_TOK_AiObservationFile, fd_obs_file },
    /* AI CONTEXT P2 (2026-09-15) */
    { NULL, GMB_TOK_AiContextUsed, fd_ctx_used },
    { NULL, GMB_TOK_AiContextBudget, fd_ctx_budget },
    { NULL, GMB_TOK_AiContextOverflow, fd_ctx_overflow },
    { NULL, GMB_TOK_AiContextView, fd_ctx_view },
    { NULL, GMB_TOK_AiContextViewDebug, fd_ctx_view },
    { NULL, GMB_TOK_AiContextSet, fd_ctx_set },
    { NULL, GMB_TOK_AiContextViewCmd, fd_ctx_viewcmd },
    { NULL, GMB_TOK_AiContextSetPolicy, fd_ctx_setpolicy },
    { NULL, GMB_TOK_AiContextSetSelector, fd_ctx_setselector },
    /* §3.6 PARSEC: context builder a segmenti (agente) */
    { NULL, GMB_TOK_AiContextCreate, fd_ctx_create },
    { NULL, GMB_TOK_AiContextAdd, fd_ctx_add },
    { NULL, GMB_TOK_AiContextMap, fd_ctx_map },
    { NULL, GMB_TOK_AiContextTrim, fd_ctx_trim },
    { NULL, GMB_TOK_AiContextGc, fd_ctx_gc },
    { NULL, GMB_TOK_AiContextCommit, fd_ctx_commit },
    { NULL, GMB_TOK_AiContextHash, fd_ctx_hash },
    { NULL, GMB_TOK_AiContextValid, fd_ctx_valid },
    { NULL, GMB_TOK_AiTaskMetrics, fd_task_metrics },
    /* AI WORKER + AI STATE P3 (2026-09-15) */
    { NULL, GMB_TOK_AiWorkerNew, fd_worker_new },
    { NULL, GMB_TOK_AiWorkerRun, fd_worker_run },
    { NULL, GMB_TOK_AiWorkerInvoke, fd_worker_invoke },
    { NULL, GMB_TOK_AiWorkerLoop, fd_worker_loop },
    { NULL, GMB_TOK_AiWorkerManage, fd_worker_manage },
    { NULL, GMB_TOK_AiReplayAuto, fd_replay_auto },
    { NULL, GMB_TOK_AiStateSave, fd_state_save },
    { NULL, GMB_TOK_AiStateLoad, fd_state_load },
    { NULL, GMB_TOK_AiWorkerRounds, fd_worker_rounds },
    { NULL, GMB_TOK_AiWorkerRescued, fd_worker_rescued },
    { NULL, GMB_TOK_AiWorkerSpend, fd_worker_spend },
};
int emit_bf_finance_defaults(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)line;
    for (size_t i = 0; i < sizeof(FD_TBL) / sizeof(FD_TBL[0]); i++)
        if (FD_TBL[i].tk == tk && (!FD_TBL[i].ns || (ns && !strcasecmp(ns, FD_TBL[i].ns))))
            return FD_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: matrix (delegazione §0quater) ── */
/* VETTORI: helper 2-arg int64_t handle → double */
static int vc_vec(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) { sb_put(&c->out, "0.0"); return 1; }
    sb_printf(&c->out, "%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "), (int64_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int vc_manhattan(ctx_t* c, gmb_expr_t** a, int n) { return vc_vec(c, a, n, "gmb_vector_manhattan"); }
static int vc_chebyshev(ctx_t* c, gmb_expr_t** a, int n) { return vc_vec(c, a, n, "gmb_vector_chebyshev"); }
static int vc_cosine(ctx_t* c, gmb_expr_t** a, int n)   { return vc_vec(c, a, n, "gmb_vector_cosine"); }
static int vc_dot(ctx_t* c, gmb_expr_t** a, int n)      { return vc_vec(c, a, n, "gmb_vector_dot"); }
static int vc_minkowski(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "gmb_vector_minkowski((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (int64_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), (double)(");
    emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static const st_row VEC_TBL[] = {
    { GMB_TOK_VectorManhattan, vc_manhattan },
    { GMB_TOK_VectorChebyshev, vc_chebyshev },
    { GMB_TOK_VectorCosineDistance, vc_cosine },
    { GMB_TOK_VectorMinkowski, vc_minkowski },
    { GMB_TOK_DotProduct, vc_dot },
};
int emit_bf_vector(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(VEC_TBL) / sizeof(VEC_TBL[0]); i++)
        if (VEC_TBL[i].tk == tk) return VEC_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: matrix (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } mx_row;
static int mx_one(ctx_t* c, gmb_expr_t** a, int n, const char* fn, int cast) {
    if (n != 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    /* MATRIX handle (DIM h = MATRIX.CREATE(...)): gmb_matrix_*(h) — cast a double per
     * gli scalari, a int per gli handle (parity emitter C#). */
    if (av && av->matrix_handle) {
        sb_put(&c->out, cast ? "(gmb_int_t)" : "(gmb_double_t)");
        sb_printf(&c->out, "%s((int64_t)(", fn);
        emit_expr(c, a[0]);
        sb_put(&c->out, "))");
        return 1;
    }
    if (!(av && av->matrix)) {
        /* Fallback: variabile SCALARE non tipizzata che CONTIENE un handle MATRIX (es.
         * `h = MATRIX.CREATE(...)` senza DIM non marca matrix_handle). Il dispatch qui è
         * già per TokenKind MATRIX.* → l'argomento è un handle: emetti comunque. */
        sb_put(&c->out, cast ? "(gmb_int_t)" : "(gmb_double_t)");
        sb_printf(&c->out, "%s((int64_t)(", fn);
        emit_expr(c, a[0]);
        sb_put(&c->out, "))");
        return 1;
    }
    if (cast) sb_put(&c->out, "(gmb_double_t)");
    sb_printf(&c->out, "%s(", fn);
    emit_matrix_handle(c, av);
    sb_put(&c->out, ")");
    return 1;
}
static int mx_multiply(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* a0 = lookup_var(c, a[0]->var);
    var_t* a1 = lookup_var(c, a[1]->var);
    if (a0 && a1 && a0->matrix_handle && a1->matrix_handle) {
        sb_put(&c->out, "(gmb_int_t)gmb_matrix_multiply((int64_t)(");
        emit_expr(c, a[0]); sb_put(&c->out, "), (int64_t)(");
        emit_expr(c, a[1]); sb_put(&c->out, "))");
        return 1;
    }
    if (!(a0 && a0->matrix && a1 && a1->matrix)) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_matrix_multiply(");
    emit_matrix_handle(c, a0); sb_put(&c->out, ", ");
    emit_matrix_handle(c, a1); sb_put(&c->out, ")");
    return 1;
}
/* MATRIX handle: CREATE/GETVALUE/SETVALUE */
static int mxh_vcreate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "0.0"); return 1; }
    sb_put(&c->out, "(gmb_int_t)gmb_vector_create(");
    for (int i = 0; i < 4; i++) {
        if (i > 0) sb_put(&c->out, ", ");
        if (i < n) emit_expr(c, a[i]); else sb_put(&c->out, "0");
    }
    sb_put(&c->out, ")");
    return 1;
}
static int mxh_create(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_create((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int mxh_get(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_matrix_get((int64_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
    emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static int mxh_set(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    sb_put(&c->out, "(gmb_matrix_set((int64_t)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
    emit_expr(c, a[2]); sb_put(&c->out, "), "); emit_expr(c, a[3]);
    sb_put(&c->out, "), (gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int mx_rowcol(ctx_t* c, gmb_expr_t** a, int n, const char* rc) {
    if (n != 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!(av && av->matrix)) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_matrix_");
    sb_put(&c->out, rc);
    sb_put(&c->out, "((gmb_int_t)(");
    emit_matrix_handle(c, av);
    sb_put(&c->out, "))");
    return 1;
}
static int mx_det(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_determinant", 0); }
static int mx_rank(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_rank", 0); }
static int mx_trace(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_trace", 0); }
static int mx_norm(ctx_t* c, gmb_expr_t** a, int n) {
    /* VECTOR.EUCLIDEA (token condiviso GMB_TOK_Norm) su array 1D → gmb_vector_norm_flat. */
    if (n == 1 && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        var_t* av = lookup_var(c, a[0]->var);
        if (av && av->is_array && !av->matrix) {
            sb_put(&c->out, "(gmb_double_t)gmb_vector_norm_flat((const double*)");
            emit_var_ref(c, av);
            sb_put(&c->out, ", (int)(");
            emit_arr_len_of(c, av);
            sb_put(&c->out, "))");
            return 1;
        }
    }
    return mx_one(c, a, n, "gmb_matrix_norm", 0);
}
static int mx_inverse(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_inverse", 1); }
static int mx_eig(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_eig", 1); }
static int mx_rowcount(ctx_t* c, gmb_expr_t** a, int n) { return mx_rowcol(c, a, n, "rows"); }
static int mx_colcount(ctx_t* c, gmb_expr_t** a, int n) { return mx_rowcol(c, a, n, "cols"); }
static int mx_frobenius(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_frobenius_norm", 0); }
static int mx_infnorm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_infinity_norm", 0); }
static int mx_onenorm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_one_norm", 0); }
static int mx_maxval(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_max_value", 0); }
static int mx_minval(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_min_value", 0); }
static int mx_cond(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_cond", 0); }
static int mx_expm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_expm", 1); }
static int mx_sqrtm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_sqrtm", 1); }
static int mx_logm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_logm", 1); }
static int mx_hessenberg(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_hessenberg", 1); }
static int mx_orth(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_orth", 1); }
static int mx_polar(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_polar", 1); }
static int mx_schur(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_schur", 1); }
static int mx_pinverse(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_pseudo_inverse", 1); }
static int mx_transpose(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_transpose", 1); }
static int mx_sinm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_sinm", 1); }
static int mx_cosm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_cosm", 1); }
static int mx_tanm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_tanm", 1); }
static int mx_signm(ctx_t* c, gmb_expr_t** a, int n) { return mx_one(c, a, n, "gmb_matrix_signm", 1); }
/* LINALG.FACTOR(decomp, name): estrae un fattore da una decomposizione (EIG/SCHUR/POLAR/…).
 * Condivide GMB_TOK_Get con POOL.Get/SyncMap.Get: n==2 con 1° arg handle decomp. */
static int mx_factor(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_matrix_decomp_get_factor_named((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), (const char*)(");
    emit_str_operand(c, a[1]);
    sb_put(&c->out, ".data))");
    return 1;
}
/* MATRIX nuovi gap (2026-09-12): 18 funzioni con runtime C già in gmb_matrix.h */
static int mxh_one_arg(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "(gmb_int_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1;
}
static int mxh_one_double(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "(gmb_double_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1;
}
static int mxh_two_handles(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 2) return 0;
    sb_printf(&c->out, "(gmb_int_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "), (int64_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "))"); return 1;
}
static int mxh_cholesky(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_cholesky"); }
static int mxh_evd(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_evd"); }
static int mxh_lu(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_lu"); }
static int mxh_qr(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_qr"); }
static int mxh_svd(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_svd"); }
static int mxh_nullspace(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_null_space"); }
static int mxh_nuclearnorm(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_double(c, a, n, "gmb_matrix_nuclear_norm"); }
static int mxh_kronecker(ctx_t* c, gmb_expr_t** a, int n) { return mxh_two_handles(c, a, n, "gmb_matrix_kronecker"); }
static int mxh_lu_solve(ctx_t* c, gmb_expr_t** a, int n) { return mxh_two_handles(c, a, n, "gmb_matrix_lu_solve"); }
static int mxh_fractional(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_fractional_power((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (double)(");
    emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int mxh_blockdiag(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_block_diag((const int64_t[]){ (int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (int64_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, ") }, 2)");
    return 1;
}
static int mxh_loadcsv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_load_csv((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
static int mxh_loadjson(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_load_json((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data)");
    return 1;
}
static int mxh_circulant(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (!(av && av->is_array && !av->matrix)) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_circulant((const double*)");
    emit_var_ref(c, av); sb_put(&c->out, ", (int)(");
    if (n >= 2) emit_expr(c, a[1]); else emit_arr_len_of(c, av);
    sb_put(&c->out, "))");
    return 1;
}
static int mxh_hankel(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2 || a[0]->kind != GMB_EXPR_VAR || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* ac = lookup_var(c, a[0]->var);
    var_t* ar = lookup_var(c, a[1]->var);
    if (!(ac && ar && ac->is_array && ar->is_array)) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_hankel((const double*)");
    emit_var_ref(c, ac); sb_put(&c->out, ", (const double*)");
    emit_var_ref(c, ar); sb_put(&c->out, ", (int)(");
    emit_arr_len_of(c, ac); sb_put(&c->out, "), (int)(");
    emit_arr_len_of(c, ar); sb_put(&c->out, "))");
    return 1;
}
static int mxh_toeplitz(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2 || a[0]->kind != GMB_EXPR_VAR || a[1]->kind != GMB_EXPR_VAR) return 0;
    var_t* ac = lookup_var(c, a[0]->var);
    var_t* ar = lookup_var(c, a[1]->var);
    if (!(ac && ar && ac->is_array && ar->is_array)) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_toeplitz((const double*)");
    emit_var_ref(c, ac); sb_put(&c->out, ", (const double*)");
    emit_var_ref(c, ar); sb_put(&c->out, ", (int)(");
    emit_arr_len_of(c, ac); sb_put(&c->out, "), (int)(");
    emit_arr_len_of(c, ar); sb_put(&c->out, "))");
    return 1;
}
static int mxh_solvetri(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (a[1]->kind != GMB_EXPR_VAR || !a[1]->var) return 0;
    var_t* bv = lookup_var(c, a[1]->var);
    if (!(bv && bv->is_array && !bv->matrix)) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_solve_triangular((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (const double*)");
    emit_var_ref(c, bv); sb_put(&c->out, ", (int)(");
    if (n >= 4) emit_expr(c, a[3]); else emit_arr_len_of(c, bv);
    sb_put(&c->out, "), (int)(");
    emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static int mxh_subspace(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_int_t)(int64_t)(uintptr_t)gmb_matrix_subspace_angles_alloc((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (int64_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), &(int64_t){0})");
    return 1;
}
/* MATRIX gap 2026-09-12 (32 funzioni): HANDLE→HANDLE / →DOUBLE / →INT / →ARRAY /
 * →STRING + vettori. Runtime condiviso gmb_matrix_*. Stesso pattern delle 18 già
 * colmate (mxh_one_arg/mxh_one_double/mxh_two_handles/mxh_subspace). */
static int mxh_reshape(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_reshape((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
    emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static int mxh_slice(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 5) return 0;
    sb_put(&c->out, "(gmb_int_t)gmb_matrix_slice((int64_t)(");
    emit_expr(c, a[0]);
    for (int i = 1; i < 5; i++) { sb_put(&c->out, "), (int)("); emit_expr(c, a[i]); }
    sb_put(&c->out, "))");
    return 1;
}
/* ADDCOLVECTOR/ADDROWVECTOR(handle, array) → nuovo handle. */
static int mxh_add_vector(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2 || a[1]->kind != GMB_EXPR_VAR || !a[1]->var) return 0;
    var_t* bv = lookup_var(c, a[1]->var);
    if (!(bv && bv->is_array && !bv->matrix)) return 0;
    sb_printf(&c->out, "(gmb_int_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "), (const double*)");
    emit_var_ref(c, bv); sb_put(&c->out, ", (int)(");
    emit_arr_len_of(c, bv); sb_put(&c->out, "))");
    return 1;
}
/* HANDLE→INT (rows/cols/flag) reso come double (contesto espressione). */
static int mxh_one_int(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "(gmb_double_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
/* Predicati booleani: convenzione Bit canonica -1/0 (parità IL/PCode), non 1/0 C. */
static int mxh_one_bit(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "(gmb_double_t)(%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, ")) ? -1 : 0)");
    return 1;
}
/* HANDLE→ARRAY in contesto espressione: come mxh_subspace, il puntatore è reso come
 * int64_t (la forma `DIM x() = …` passa da dim_arr_init_kind/emit_dim_arr_init). */
static int mxh_arr1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "(gmb_int_t)(int64_t)(uintptr_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "), &(int64_t){0})");
    return 1;
}
/* ABOVEMASK/ABOVEMATRIX(handle, threshold) → bool* (reso come int64_t in espressione). */
static int mxh_above(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) return 0;
    sb_printf(&c->out, "(gmb_int_t)(int64_t)(uintptr_t)%s((int64_t)(", fn);
    emit_expr(c, a[0]); sb_put(&c->out, "), (double)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), &(int64_t){0})");
    return 1;
}
/* MULTIPLYVECTOR(handle, array) → double* (reso come int64_t in espressione). */
static int mxh_mulvec(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2 || a[1]->kind != GMB_EXPR_VAR || !a[1]->var) return 0;
    var_t* bv = lookup_var(c, a[1]->var);
    if (!(bv && bv->is_array)) return 0;
    sb_put(&c->out, "(gmb_int_t)(int64_t)(uintptr_t)gmb_matrix_multiply_vector_alloc((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), (const double*)");
    emit_var_ref(c, bv); sb_put(&c->out, ", (int)(");
    emit_arr_len_of(c, bv); sb_put(&c->out, "), &(int64_t){0})");
    return 1;
}
/* MULTIPLYLEFTVECTOR(handle, array) → double* (reso come int64_t in espressione).
 * La firma StdLib è (data handle, values); il runtime vuole (v, vlen, h, outn). */
static int mxh_mulleftvec(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2 || a[1]->kind != GMB_EXPR_VAR || !a[1]->var) return 0;
    var_t* bv = lookup_var(c, a[1]->var);
    if (!(bv && bv->is_array)) return 0;
    sb_put(&c->out, "(gmb_int_t)(int64_t)(uintptr_t)gmb_matrix_multiply_left_vector_alloc((const double*)");
    emit_var_ref(c, bv); sb_put(&c->out, ", (int)(");
    emit_arr_len_of(c, bv); sb_put(&c->out, "), (int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), &(int64_t){0})");
    return 1;
}
/* ASCSV/ASJSON(handle) → gmb_string_t (char* runtime → gmb_string_alloc). */
static int mxh_as_csv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_matrix_as_csv((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int mxh_as_json(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_matrix_as_json((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int mxh_clone(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_arg(c, a, n, "gmb_matrix_clone"); }
static int mxh_concat_rows(ctx_t* c, gmb_expr_t** a, int n) { return mxh_two_handles(c, a, n, "gmb_matrix_concat_rows"); }
static int mxh_concat_cols(ctx_t* c, gmb_expr_t** a, int n) { return mxh_two_handles(c, a, n, "gmb_matrix_concat_cols"); }
static int mxh_add_colvec(ctx_t* c, gmb_expr_t** a, int n) { return mxh_add_vector(c, a, n, "gmb_matrix_add_col_vector"); }
static int mxh_add_rowvec(ctx_t* c, gmb_expr_t** a, int n) { return mxh_add_vector(c, a, n, "gmb_matrix_add_row_vector"); }
static int mxh_frobenius(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_double(c, a, n, "gmb_matrix_frobenius_norm"); }
static int mxh_infnorm(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_double(c, a, n, "gmb_matrix_infinity_norm"); }
static int mxh_onenorm(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_double(c, a, n, "gmb_matrix_one_norm"); }
static int mxh_maxval(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_double(c, a, n, "gmb_matrix_max_value"); }
static int mxh_minval(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_double(c, a, n, "gmb_matrix_min_value"); }
static int mxh_rowcount(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_int(c, a, n, "gmb_matrix_rows"); }
static int mxh_colcount(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_int(c, a, n, "gmb_matrix_cols"); }
static int mxh_islower(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_bit(c, a, n, "gmb_matrix_is_lower_triangular"); }
static int mxh_isupper(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_bit(c, a, n, "gmb_matrix_is_upper_triangular"); }
static int mxh_issym(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_bit(c, a, n, "gmb_matrix_is_symmetric"); }
static int mxh_isorth(ctx_t* c, gmb_expr_t** a, int n) { return mxh_one_bit(c, a, n, "gmb_matrix_is_orthogonal"); }
static int mxh_rowsums(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_row_sums_alloc"); }
static int mxh_rowmeans(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_row_means_alloc"); }
static int mxh_colsums(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_col_sums_alloc"); }
static int mxh_colmeans(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_col_means_alloc"); }
static int mxh_rowvars(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_row_variances_alloc"); }
static int mxh_colvars(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_col_variances_alloc"); }
static int mxh_argmax(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_argmax_alloc"); }
static int mxh_argmin(ctx_t* c, gmb_expr_t** a, int n) { return mxh_arr1(c, a, n, "gmb_matrix_argmin_alloc"); }
static int mxh_abovemask(ctx_t* c, gmb_expr_t** a, int n) { return mxh_above(c, a, n, "gmb_matrix_above_mask_alloc"); }
static int mxh_abovematrix(ctx_t* c, gmb_expr_t** a, int n) { return mxh_above(c, a, n, "gmb_matrix_above_matrix_alloc"); }

static const mx_row MX_TBL[] = {
    { GMB_TOK_Determinant, mx_det }, { GMB_TOK_RANK, mx_rank }, { GMB_TOK_TRACE, mx_trace },
    { GMB_TOK_Norm, mx_norm }, { GMB_TOK_Inverse, mx_inverse }, { GMB_TOK_EIG, mx_eig },
    { GMB_TOK_Multiply, mx_multiply },
    { GMB_TOK_ROWCOUNT, mx_rowcount }, { GMB_TOK_COLUMNCOUNT, mx_colcount },
    { GMB_TOK_FROBENIUSNORM, mx_frobenius }, { GMB_TOK_INFINITYNORM, mx_infnorm },
    { GMB_TOK_ONENORM, mx_onenorm }, { GMB_TOK_MAXVALUE, mx_maxval },
    { GMB_TOK_MINVALUE, mx_minval }, { GMB_TOK_Cond, mx_cond },
    { GMB_TOK_EXPM, mx_expm }, { GMB_TOK_SQRTM, mx_sqrtm }, { GMB_TOK_LOGM, mx_logm },
    { GMB_TOK_HESSENBERG, mx_hessenberg }, { GMB_TOK_ORTH, mx_orth }, { GMB_TOK_POLAR, mx_polar },
    { GMB_TOK_SCHUR, mx_schur }, { GMB_TOK_PINVERSE, mx_pinverse }, { GMB_TOK_Transpose, mx_transpose },
    { GMB_TOK_SINM, mx_sinm }, { GMB_TOK_COSM, mx_cosm }, { GMB_TOK_TANM, mx_tanm },
    { GMB_TOK_SIGNM, mx_signm },
    /* MATRIX handle API (token risolti dal parser per MATRIX.*) */
    { GMB_TOK_LinAlgMatrixCreateFunc, mxh_create },
    { GMB_TOK_LinAlgVectorCreateFunc, mxh_vcreate },
    { GMB_TOK_MatrixGetValue, mxh_get }, { GMB_TOK_MatrixSetValue, mxh_set },
    { GMB_TOK_MatrixCond, mx_cond }, { GMB_TOK_MatrixCosm, mx_cosm },
    { GMB_TOK_MatrixEig, mx_eig }, { GMB_TOK_MatrixExp, mx_expm },
    { GMB_TOK_MatrixHessenberg, mx_hessenberg }, { GMB_TOK_MatrixLogm, mx_logm },
    { GMB_TOK_MatrixNorm, mx_norm }, { GMB_TOK_MatrixOrth, mx_orth },
    { GMB_TOK_MatrixPseudoInverse, mx_pinverse }, { GMB_TOK_MatrixPolar, mx_polar },
    { GMB_TOK_MatrixRank, mx_rank }, { GMB_TOK_MatrixSchur, mx_schur },
    { GMB_TOK_MatrixSignm, mx_signm }, { GMB_TOK_MatrixSinm, mx_sinm },
    { GMB_TOK_MatrixSqrt, mx_sqrtm }, { GMB_TOK_MatrixTanm, mx_tanm },
    { GMB_TOK_MatrixTrace, mx_trace },
    /* GAP colmati 2026-09-12: 18 funzioni MATRIX con runtime C già in gmb_matrix.h */
    { GMB_TOK_Cholesky, mxh_cholesky }, { GMB_TOK_Evd, mxh_evd },
    { GMB_TOK_Lu, mxh_lu }, { GMB_TOK_Qr, mxh_qr }, { GMB_TOK_Svd, mxh_svd },
    { GMB_TOK_MatrixBlockDiag, mxh_blockdiag },
    { GMB_TOK_MatrixCirculant, mxh_circulant },
    { GMB_TOK_MatrixFractionalPower, mxh_fractional },
    { GMB_TOK_MatrixHankel, mxh_hankel },
    { GMB_TOK_MatrixKron, mxh_kronecker },
    { GMB_TOK_MatrixLoadCsv, mxh_loadcsv },
    { GMB_TOK_MatrixLoadJson, mxh_loadjson },
    { GMB_TOK_MatrixNuclearNorm, mxh_nuclearnorm },
    { GMB_TOK_MatrixNullSpace, mxh_nullspace },
    { GMB_TOK_MatrixSolveTriangular, mxh_solvetri },
    { GMB_TOK_MatrixSubspaceAngles, mxh_subspace },
    { GMB_TOK_MatrixToeplitz, mxh_toeplitz },
    { GMB_TOK_Solve, mxh_lu_solve },
    /* MATRIX gap 2026-09-12 (32 funzioni) */
    { GMB_TOK_MatrixClone, mxh_clone },
    { GMB_TOK_MatrixReshape, mxh_reshape },
    { GMB_TOK_MatrixConcatRows, mxh_concat_rows },
    { GMB_TOK_MatrixConcatCols, mxh_concat_cols },
    { GMB_TOK_MatrixSlice, mxh_slice },
    { GMB_TOK_MatrixAddColVector, mxh_add_colvec },
    { GMB_TOK_MatrixAddRowVector, mxh_add_rowvec },
    { GMB_TOK_MatrixFrobeniusNorm, mxh_frobenius },
    { GMB_TOK_MatrixInfinityNorm, mxh_infnorm },
    { GMB_TOK_MatrixOneNorm, mxh_onenorm },
    { GMB_TOK_MatrixMaxValue, mxh_maxval },
    { GMB_TOK_MatrixMinValue, mxh_minval },
    { GMB_TOK_MatrixRowCount, mxh_rowcount },
    { GMB_TOK_MatrixColumnCount, mxh_colcount },
    { GMB_TOK_MatrixIsLowerTriangular, mxh_islower },
    { GMB_TOK_MatrixIsUpperTriangular, mxh_isupper },
    { GMB_TOK_MatrixIsSymmetric, mxh_issym },
    { GMB_TOK_MatrixIsOrthogonal, mxh_isorth },
    { GMB_TOK_MatrixRowSums, mxh_rowsums },
    { GMB_TOK_MatrixRowMeans, mxh_rowmeans },
    { GMB_TOK_MatrixColSums, mxh_colsums },
    { GMB_TOK_MatrixColMeans, mxh_colmeans },
    { GMB_TOK_MatrixArgMax, mxh_argmax },
    { GMB_TOK_MatrixArgMin, mxh_argmin },
    { GMB_TOK_MatrixAboveMask, mxh_abovemask },
    { GMB_TOK_MatrixAboveMatrix, mxh_abovematrix },
    { GMB_TOK_MatrixAsCsv, mxh_as_csv },
    { GMB_TOK_MatrixAsJson, mxh_as_json },
    { GMB_TOK_MatrixMultiplyVector, mxh_mulvec },
    { GMB_TOK_MatrixMultiplyLeftVector, mxh_mulleftvec },
    { GMB_TOK_MatrixRowVariances, mxh_rowvars },
    { GMB_TOK_MatrixColVariances, mxh_colvars },
    { GMB_TOK_Get, mx_factor },
    { GMB_TOK_LinAlgDecompGetFactorFunc, mx_factor },
};
int emit_bf_matrix(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(MX_TBL) / sizeof(MX_TBL[0]); i++)
        if (MX_TBL[i].tk == tk) return MX_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: rdd (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } rd_row;
static int rd_copyto(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_rdd_copy_to(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int rd_setorder(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_rdd_set_order(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int rd_setdeleted(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    /* statement-expression (come rd_getfield): il `,` in una CAST non è un operatore comma
     * valido → il C generato non compilava ("expected ')'"). Prima l'area va selezionata,
     * poi si applica SETDELETED. */
    sb_put(&c->out, "({ gmb_rdd_select((int)("); emit_expr(c, a[0]);
    /* Ritorno BIT BASIC (-1 vero / 0 falso), come IL/VM-C: prima emetteva 1 secco
     * (`setdeleted= 1` contro `-1` delle altre gambe). */
    sb_put(&c->out, ")); (gmb_double_t)(gmb_rdd_set_deleted((int)("); emit_expr(c, a[1]);
    sb_put(&c->out, ")) == 0 ? -1.0 : 0.0); })");
    return 1;
}
static int rd_delete(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_rdd_delete()"); return 1; }
static int rd_recall(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_rdd_recall()"); return 1; }
static int rd_eof(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)(gmb_rdd_eof() ? -1 : 0)"); return 1; }
static int rd_bof(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)(gmb_rdd_bof() ? -1 : 0)"); return 1; }
static int rd_found(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)(gmb_rdd_found() ? -1 : 0)"); return 1; }
static int rd_getfield(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    /* Getter TIPIZZATO (gmb_val_t): etype TY_VAL; le coercizioni str/num lo risolvono. */
    sb_put(&c->out, "({ gmb_rdd_select((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, ")); ");
    if (a[1]->kind == GMB_EXPR_STR) {
        sb_put(&c->out, "gmb_rdd_get_field_val_by_name(("); emit_str_operand(c, a[1]);
        sb_put(&c->out, ").data)");
    } else {
        sb_put(&c->out, "gmb_rdd_get_field_val((int)("); emit_expr(c, a[1]);
        sb_put(&c->out, ") - 1)");
    }
    sb_put(&c->out, "; })");
    return 1;
}
static int rd_setfield(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    /* `gmb_rdd_set_field` NON esiste nel runtime condiviso: l'emissione chiamava una funzione
     * inesistente (link error latente per RDD REPLACE in C99Native). La funzione canonica è
     * `gmb_rdd_replace_field(idx, value)` — la STESSA che usano VM/C (syscall RDD_SET_FIELD,
     * codice 40) e il percorso dei campi impliciti (§0bis: una sola implementazione). */
    sb_put(&c->out, "((gmb_double_t)(gmb_rdd_select((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, ")), gmb_rdd_replace_field((int)("); emit_expr(c, a[1]);
    sb_put(&c->out, ") - 1, "); emit_str_operand(c, a[2]); sb_put(&c->out, ".data)))");
    return 1;
}
static int rd_getfldstate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)(gmb_rdd_select((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, ")), gmb_rdd_get_field_state(");
    if (a[1]->kind == GMB_EXPR_STR) { sb_put(&c->out, "gmb_rdd_field_index(("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data)"); }
    else { sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ") - 1"); }
    sb_put(&c->out, ")))");
    return 1;
}
static int rd_select(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_rdd_select((int)("); emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int rd_valtype(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_rdd_valtype_str()");
    return 1;
}
static int rd_float(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_rdd_float((gmb_double_t)("); emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int rd_curval(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_rdd_curval((gmb_double_t)("); emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int rd_tableupdate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_rdd_tableupdate((int)("); emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int rd_tablerevert(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_rdd_tablerevert((int)("); emit_expr(c, a[0]); sb_put(&c->out, ")))");
    return 1;
}
static int rd_buffer(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_rdd_buffer((int)("); emit_expr(c, a[0]);
    sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, ")))");
    return 1;
}
static int rd_today(ctx_t* c, gmb_expr_t** a, int n) {
    /* TODAY() è NUMERICO (serial date), parity IlRuntime/gmb_dt_today della VM/C. */
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)gmb_dt_today())");
    return 1;
}
/* RDD.RECALL(scope [, expr]) → NUMERO di record recallati (parity IlRuntime/VM: il motore C
 * non valuta la condizione, la convenzione è lo scope "ALL"). */
static int rd_recall_scoped(ctx_t* c, gmb_expr_t** a, int n) {
    sb_put(&c->out, "(gmb_double_t)gmb_rdd_recall_scoped(");
    if (n >= 1) { sb_put(&c->out, "("); emit_str_operand(c, a[0]); sb_put(&c->out, ").data"); }
    else sb_put(&c->out, "\"ALL\"");
    sb_put(&c->out, ")");
    return 1;
}
static int rd_oldval(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_rdd_oldval_val(");
    if (a[1]->kind == GMB_EXPR_STR) { sb_put(&c->out, "gmb_rdd_field_index(("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data)"); }
    else { sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ") - 1"); }
    sb_put(&c->out, ")");
    return 1;
}
/* RDD.RECNO/RLOCK/FLOCK/CTOD/DTOC/DELETE/REPLACE: presenti nel ns_lookup ma assenti dalla
 * tabella → "simbolo non trovato" su C99Native. Parity VM/C (gmb_bf_741_c755/c757). */
static int rd_recno0(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_rdd_recno()"); return 1; }
static int rd_rlock(ctx_t* c, gmb_expr_t** a, int n) { sb_put(&c->out, "((gmb_double_t)({ "); if (n >= 1) { sb_put(&c->out, "gmb_rdd_select((int)("); emit_expr(c, a[0]); sb_put(&c->out, ")); "); } sb_put(&c->out, "gmb_rdd_used() ? gmb_rdd_rlock() : (gmb_db_current_isam() > 0 ? 1 : gmb_rdd_rlock()); }))"); return 1; }
static int rd_flock(ctx_t* c, gmb_expr_t** a, int n) { sb_put(&c->out, "((gmb_double_t)({ "); if (n >= 1) { sb_put(&c->out, "gmb_rdd_select((int)("); emit_expr(c, a[0]); sb_put(&c->out, ")); "); } sb_put(&c->out, "gmb_rdd_used() ? gmb_rdd_flock() : (gmb_db_current_isam() > 0 ? 1 : gmb_rdd_flock()); }))"); return 1; }
static int rd_ctod(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_dt_ctod(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int rd_dtoc(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* RDD.DTOC → ISO yyyy-MM-dd (il DTOC standard usa gmb_dt_dtoc US "mm-dd-yyyy"). */
    sb_put(&c->out, "gmb_dt_dtoc_iso((gmb_double_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "))");
    return 1;
}
static int rd_delete_scoped(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "(gmb_double_t)(gmb_rdd_delete() == 0 ? 1.0 : 0.0)");
    return 1;
}
static int rd_replace_scoped(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "({ int _rfc = ");
    if (a[1]->kind == GMB_EXPR_STR) {
        sb_put(&c->out, "gmb_rdd_field_index((");
        emit_str_operand(c, a[1]);
        sb_put(&c->out, ").data)");
    } else {
        sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ") - 1");
    }
    sb_put(&c->out, "; (gmb_double_t)((gmb_rdd_recno() > 0 && gmb_rdd_replace_field(_rfc, (");
    if (n >= 3) emit_str_operand(c, a[2]);
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, ").data) == 0) ? 1.0 : 0.0); })");
    return 1;
}
static const rd_row RD_TBL[] = {
    { GMB_TOK_RddValType, rd_valtype }, { GMB_TOK_RddCurval, rd_curval }, { GMB_TOK_RddFloat, rd_float },
    { GMB_TOK_RddTableUpdate, rd_tableupdate }, { GMB_TOK_RddTableRevert, rd_tablerevert },
    { GMB_TOK_RddBuffer, rd_buffer }, { GMB_TOK_RddToday, rd_today },
    /* voci QUALIFICATE dal namespace RDD (gmb_ns_lookup.h): i token nudi non le coprono. */
    { GMB_TOK_RddOldVal, rd_oldval }, { GMB_TOK_RddSetDeleted, rd_setdeleted },
    { GMB_TOK_RddRecallScoped, rd_recall_scoped },
    { GMB_TOK_RddCtod, rd_ctod }, { GMB_TOK_RddDtoc, rd_dtoc }, { GMB_TOK_Recno, rd_recno0 },
    { GMB_TOK_RddRLock, rd_rlock }, { GMB_TOK_RddFLock, rd_flock },
    { GMB_TOK_RddDeleteScoped, rd_delete_scoped }, { GMB_TOK_RddReplaceScoped, rd_replace_scoped },
    { GMB_TOK_COPYTO, rd_copyto }, { GMB_TOK_SETORDER, rd_setorder },
    { GMB_TOK_SETDELETED, rd_setdeleted }, { GMB_TOK_Delete, rd_delete },
    { GMB_TOK_Recall, rd_recall }, { GMB_TOK_Eof, rd_eof }, { GMB_TOK_EofFunc, rd_eof }, { GMB_TOK_Bof, rd_bof },
    { GMB_TOK_Found, rd_found }, { GMB_TOK_RddGetField, rd_getfield },
    { GMB_TOK_SETFIELD, rd_setfield }, { GMB_TOK_RddGetFieldState, rd_getfldstate },
    { GMB_TOK_Select, rd_select },
};
int emit_bf_rdd(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name;
    /* `SELECT` è la funzione RDD.SELECT; nella namespace DBF NON esiste: l'area si cambia
     * col comando `DBF SELECT <alias>` e si legge con `DBF.SELECTED()`. Prima il C99
     * accettava `DBF.SELECT(1)` come alias silenzioso di RDD.SELECT (divergenza C#/IL/PCode). */
    if (tk == GMB_TOK_Select && ns && strcasecmp(ns, "DBF") == 0) {
        c99_fail(c, line, "DBF.SELECT non è una funzione: usa il comando DBF SELECT o DBF.SELECTED()");
        return 1;
    }
    for (size_t i = 0; i < sizeof(RD_TBL) / sizeof(RD_TBL[0]); i++)
        if (RD_TBL[i].tk == tk) return RD_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: ledger (funzioni) ───────────────────────────────────────────────
 * Parity con IL/PCode: gli array del ledger si restituiscono come LISTE
 * (gmb_db_ledger_*_val), i bit come -1/0, PLOT come numero di punti. */
static int ld_num1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)"); sb_put(&c->out, fn);
    sb_put(&c->out, "(gmb_db_current_ledger()))");
    return 1;
}
static int ld_bit1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)("); sb_put(&c->out, fn);
    sb_put(&c->out, "(gmb_db_current_ledger()) ? -1.0 : 0.0))");
    return 1;
}
static int ld_open(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_ledger_open(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data))");
    return 1;
}
static int ld_list(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    (void)a; (void)n;
    /* Le funzioni _val del runtime restituiscono un gmb_val_t (lista); l'emitter tratta le liste
     * come gmb_list_t* (accesso .length). Conversione con le API pubbliche del runtime. */
    sb_put(&c->out, "gmb_val_as_list(gmb_db_ledger_");
    sb_put(&c->out, fn);
    sb_put(&c->out, "_val(gmb_db_current_ledger(), \"\"))");
    return 1;
}
static int ld_accounts(ctx_t* c, gmb_expr_t** a, int n) { return ld_list(c, a, n, "accounts"); }
static int ld_balance(ctx_t* c, gmb_expr_t** a, int n) { return ld_list(c, a, n, "balance"); }
static int ld_register(ctx_t* c, gmb_expr_t** a, int n) { return ld_list(c, a, n, "register"); }
/* LEDGER.GET(h, id) → LISTA dei movimenti (parità IL object?[]; tipi definiti, non JSON). */
static int ld_get(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_val_as_list(gmb_db_ledger_get_val(gmb_db_current_ledger(), (int64_t)(");
    emit_expr(c, n >= 2 ? a[1] : a[0]);
    sb_put(&c->out, ")))");
    return 1;
}
/* LEDGER.VARIANCE(h, account, period) → DOUBLE (parità IL). */
static int ld_variance(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "({ double _lv = 0; (void)gmb_db_ledger_variance(gmb_db_current_ledger(), (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[2]); sb_put(&c->out, ").data, &_lv); _lv; })");
    return 1;
}
static int ld_count(ctx_t* c, gmb_expr_t** a, int n) { return ld_num1(c, a, n, "gmb_db_ledger_count"); }static int ld_lastrec(ctx_t* c, gmb_expr_t** a, int n) { return ld_num1(c, a, n, "gmb_db_ledger_count"); }
static int ld_recno(ctx_t* c, gmb_expr_t** a, int n) { return ld_num1(c, a, n, "gmb_db_ledger_recno"); }
static int ld_found(ctx_t* c, gmb_expr_t** a, int n) { return ld_bit1(c, a, n, "gmb_db_ledger_found"); }
static int ld_eof(ctx_t* c, gmb_expr_t** a, int n) { return ld_bit1(c, a, n, "gmb_db_ledger_eof"); }
static int ld_bof(ctx_t* c, gmb_expr_t** a, int n) { return ld_bit1(c, a, n, "gmb_db_ledger_bof"); }
static int ld_validate(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* gmb_db_ledger_validate ritorna 1 = valido → -1 (BIT BASIC), come IL/VM/C. */
    sb_put(&c->out, "((gmb_double_t)(gmb_db_ledger_validate(gmb_db_current_ledger()) ? -1.0 : 0.0))");
    return 1;
}
static int ld_import(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_ledger_import(gmb_db_current_ledger(), (");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data))");
    return 1;
}
static int ld_plot(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "({ char _lp[64][" "GMB_DB_LEDGER_NAME" "]; double _la[64]; int _ln = 0;"
                    " (void)gmb_db_ledger_plot(gmb_db_current_ledger(), \"\", _lp, _la, 64, &_ln);"
                    " (gmb_double_t)_ln; })");
    return 1;
}
/* LEDGER.EDIT(h, id, date, desc) → gmb_db_ledger_edit (parità emitter C# EmitBuiltinLEDGER:
 * id + date + desc, accounts/amounts NULL). La forma statement (LEDGER EDIT ...) resta in
 * gmb_c99_emit.c. */
static int ld_edit(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "({ double _le = (double)gmb_db_ledger_edit(gmb_db_current_ledger(), (int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), (");
    emit_str_operand(c, a[1]);
    sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[2]);
    sb_put(&c->out, ").data, NULL, NULL, 0); _le; })");
    return 1;
}
static const rd_row LD_TBL[] = {
    { GMB_TOK_Open, ld_open }, { GMB_TOK_Count, ld_count }, { GMB_TOK_Lastrec, ld_lastrec },
    { GMB_TOK_Edit, ld_edit },
    { GMB_TOK_Recno, ld_recno }, { GMB_TOK_Found, ld_found }, { GMB_TOK_Eof, ld_eof },
    { GMB_TOK_EofFunc, ld_eof }, { GMB_TOK_Bof, ld_bof }, { GMB_TOK_Validate, ld_validate },
    { GMB_TOK_Import, ld_import }, { GMB_TOK_Plot, ld_plot },
    { GMB_TOK_Accounts, ld_accounts }, { GMB_TOK_Balance, ld_balance }, { GMB_TOK_Register, ld_register },
    { GMB_TOK_Get, ld_get }, { GMB_TOK_Variance, ld_variance },
};
int emit_bf_ledger(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(LD_TBL) / sizeof(LD_TBL[0]); i++)
        if (LD_TBL[i].tk == tk) return LD_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: dbf / isam (funzioni) ──────────────────────────────────────────
 * Parity con l'oracolo C# (C99CodeEmitter.Builtin*.cs): le funzioni
 * DBF/ISAM chiamano lo STESSO runtime C condiviso (native/gmb_db_c). Semantica copiata dai
 * handler VM/C (gmb_bf_466/473) — unica fonte di verità del comportamento.
 * Convenzione: -1 = vero (BASIC), 0 = falso, come i bit delle funzioni DBF. */
static int db_used(ctx_t* c, gmb_expr_t** a, int n) {
    /* USED([alias]): un'area è "usata" se l'alias (o l'area corrente) è un DBF con campi
     * oppure un'area ISAM aperta. Prima guardava SOLO il DBF: ISAM.USED("X") dava 0 su
     * C99Native mentre IL/VM-C davano -1 (divergenza misurata dal MiniTest ISAM). */
    if (n >= 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        sb_put(&c->out, "((gmb_double_t)((gmb_db_dbf_find_handle_by_alias(\"");
        sb_put(&c->out, a[0]->var);
        sb_put(&c->out, "\") > 0 || gmb_db_isam_handle_for(\"");
        sb_put(&c->out, a[0]->var);
        sb_put(&c->out, "\") > 0) ? -1.0 : 0.0))");
        return 1;
    }
    sb_put(&c->out, "((gmb_double_t)((gmb_db_current_isam() > 0 || (gmb_db_current_dbf() > 0 && gmb_db_dbf_field_count(gmb_db_current_dbf()) > 0)) ? -1.0 : 0.0))");
    return 1;
}
static int db_bit(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    /* ISAM.FOUND/EOF/BOF: se c'e' un'area ISAM corrente, il handle e' quello ISAM. */
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)("); sb_put(&c->out, fn);
    sb_put(&c->out, "(gmb_db_current_isam() > 0 ? gmb_db_current_isam() : gmb_db_current_dbf()) ? -1.0 : 0.0))");
    return 1;
}
static int db_bof(ctx_t* c, gmb_expr_t** a, int n) { return db_bit(c, a, n, "gmb_db_dbf_bof"); }
static int db_eof(ctx_t* c, gmb_expr_t** a, int n) {
    /* AREA-AWARE: se l'area corrente è un cell-store RDD (RDD USE ...) usa l'EOF RDD,
     * altrimenti il DBF/ISAM. Prima andava sempre al DBF → un'area CSV RDD risultava
     * sempre EOF (bug CustomerOrdersReport.bas). */
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)((gmb_rdd_used() ? gmb_rdd_eof() : gmb_db_dbf_eof(gmb_db_current_isam() > 0 ? gmb_db_current_isam() : gmb_db_current_dbf())) ? -1.0 : 0.0))");
    return 1;
}
static int db_found(ctx_t* c, gmb_expr_t** a, int n) { return db_bit(c, a, n, "gmb_db_dbf_found"); }
static int db_deleted(ctx_t* c, gmb_expr_t** a, int n) {
    /* AREA-AWARE (come db_eof): con un'area RDD in uso DELETED() legge il marcatore RDD
     * (`RDD DELETE` lo scrive in gmb_rdd), non l'handle DBF — prima restava sempre 0. */
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)((gmb_rdd_used() ? gmb_rdd_deleted() : gmb_db_dbf_deleted(gmb_db_current_isam() > 0 ? gmb_db_current_isam() : gmb_db_current_dbf())) ? -1.0 : 0.0))");
    return 1;
}
static int db_num1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)"); sb_put(&c->out, fn);
    sb_put(&c->out, "(gmb_db_current_dbf()))");
    return 1;
}
static int db_lastrec(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* ISAM.LASTREC(alias): se il primo argomento è un identificatore (alias area ISAM),
     * seleziona l'area (gmb_db_isam_select) e conta lì — altrimenti l'area corrente. */
    sb_put(&c->out, "((gmb_double_t)(");
    if (n >= 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        sb_put(&c->out, "gmb_db_isam_select(\"");
        sb_put(&c->out, a[0]->var);
        sb_put(&c->out, "\") > 0 ? gmb_db_isam_count(gmb_db_current_isam()) : (gmb_db_current_dbf() > 0 ? gmb_db_dbf_lastrec(gmb_db_current_dbf()) : gmb_rdd_lastrec())");
    } else {
        sb_put(&c->out, "gmb_db_current_isam() > 0 ? gmb_db_isam_count(gmb_db_current_isam()) : (gmb_db_current_dbf() > 0 ? gmb_db_dbf_lastrec(gmb_db_current_dbf()) : gmb_rdd_lastrec())");
    }
    sb_put(&c->out, "))");
    return 1;
}
static int db_recno(ctx_t* c, gmb_expr_t** a, int n) {
    /* RECNO condiviso DBF/ISAM: se c'e' un'area ISAM corrente usa gmb_db_isam_recno
     * (parita' di db_bit per EOF/BOF/FOUND). Prima usava solo gmb_db_dbf_recno →
     * ISAM.RECNO dava 0 (gamba C99Native di DbIsamFullTests.bas). */
    (void)a; (void)n;
    /* NON area-aware per l'area RDD: parità IL — DbfRecno con area RDD torna 0
     * (nessun handle DBF; il fallback IL è CurrentKeyNo()=ordine indice, non recno).
     * RDD.RECNO() usa invece gmb_rdd_recno() (funzione RDD). */
    sb_put(&c->out, "((gmb_double_t)(gmb_db_current_isam() > 0 ? gmb_db_isam_recno(gmb_db_current_isam()) : gmb_db_dbf_recno(gmb_db_current_dbf())))");
    return 1;
}
static int db_recsize(ctx_t* c, gmb_expr_t** a, int n) {
    /* RECSIZE condiviso DBF/ISAM (come db_recno/db_bit): se c'e' un'area ISAM corrente usa
     * gmb_db_isam_recsize (byte del record, incl. metadata), altrimenti il DBF. Prima usava
     * solo gmb_db_dbf_record_size -> ISAM.RECSIZE=-1. */
    /* RECSIZE([alias]): se c'e' un alias esplicito risolvilo (read-only) all'area ISAM;
     * altrimenti usa l'area corrente. Prima ignorava l'argomento -> RECSIZE(Ordini) leggeva
     * l'area corrente (Clienti). */
    if (n >= 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        sb_put(&c->out, "((gmb_double_t)gmb_db_isam_recsize(gmb_db_isam_handle_for(\"");
        sb_put(&c->out, a[0]->var);
        sb_put(&c->out, "\")))");
    } else {
        sb_put(&c->out, "((gmb_double_t)(gmb_db_current_isam() > 0 ? gmb_db_isam_recsize(gmb_db_current_isam()) : gmb_db_dbf_record_size(gmb_db_current_dbf())))");
    }
    return 1;
}
static int db_isam_info(ctx_t* c, gmb_expr_t** a, int n) {
    /* ISAM.INFO([alias]) → JSON header+schema del file ISAM (gmb_db_isam_info_str, gmb_string_t). */
    sb_put(&c->out, "gmb_db_isam_info_str(");
    if (n >= 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        sb_put(&c->out, "(gmb_db_isam_select(\"");
        sb_put(&c->out, a[0]->var);
        sb_put(&c->out, "\"), gmb_db_current_isam())");
    } else {
        sb_put(&c->out, "gmb_db_current_isam()");
    }
    sb_put(&c->out, ")");
    return 1;
}
static int db_lupdate(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_db_dbf_lupdate_str(gmb_db_current_dbf())");
    return 1;
}
static int db_filter(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_db_dbf_get_filter_str(gmb_db_current_dbf())");
    return 1;
}
static int db_relation(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_db_dbf_relation_key_str(gmb_db_current_dbf())");
    return 1;
}
static int db_fieldname(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* FIELDNAME(n) è 1-BASED (parity IL/IlRuntime.DbfFieldName e StdLib); la API del runtime
     * prende l'indice 0-based → -1. Senza, FIELDNAME(1) dava il SECONDO campo
     * (§ MiniTests/dbf/dbf_state.bas: field1 [NOME] invece di [ID]). */
    sb_put(&c->out, "gmb_db_dbf_field_name_str(gmb_db_current_dbf(), (int)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, ") - 1)");
    return 1;
}
static int db_setdur(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)(gmb_db_dbf_set_durability(gmb_db_current_dbf(), (");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data) == 0 ? -1.0 : 0.0))");
    return 1;
}
static int db_flushdur(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)(gmb_db_dbf_flush_durability(gmb_db_current_dbf()) == 0 ? -1.0 : 0.0))");
    return 1;
}
static int db_begingc(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)(gmb_db_dbf_begin_group_commit(gmb_db_current_dbf()) == 0 ? -1.0 : 0.0))");
    return 1;
}
static int db_endgc(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)(gmb_db_dbf_end_group_commit(gmb_db_current_dbf()) == 0 ? -1.0 : 0.0))");
    return 1;
}
static int db_rlock(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* Area ISAM (B+Tree): lock del record corrente dal motore ISAM; valore 1 (parità IL,
     * non -1). Prima usava solo il DBF → 0 su un'area ISAM. */
    sb_put(&c->out, "((gmb_double_t)(gmb_db_current_isam() > 0 ? (gmb_db_isam_lock(gmb_db_current_isam()) >= 0 ? 1.0 : 0.0) : (gmb_db_dbf_rlock(gmb_db_current_dbf()) == 0 ? 1.0 : 0.0)))");
    return 1;
}
static int db_flock(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* Area ISAM: lock del file dal motore ISAM; valore 1 (parità IL). */
    sb_put(&c->out, "((gmb_double_t)(gmb_db_current_isam() > 0 ? (gmb_db_isam_flock(gmb_db_current_isam()) >= 0 ? 1.0 : 0.0) : (gmb_db_dbf_flock(gmb_db_current_dbf()) == 0 ? 1.0 : 0.0)))");
    return 1;
}
static int db_setrepl(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_db_dbf_set_replication_str(gmb_db_current_dbf(), (");
    emit_str_operand(c, a[n - 1]);
    sb_put(&c->out, ").data)");
    return 1;
}
static int db_applyrepl(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_dbf_apply_replication(gmb_db_current_dbf(), (");
    emit_str_operand(c, a[n - 1]);
    sb_put(&c->out, ").data))");
    return 1;
}
static int db_followrepl(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_dbf_follow_replication(gmb_db_current_dbf(), (");
    emit_str_operand(c, a[n - 1]);
    sb_put(&c->out, ").data, NULL))");
    return 1;
}
static int db_ship(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_dbf_replication_ship(gmb_db_current_dbf(), (");
    emit_str_operand(c, a[n - 2]);
    sb_put(&c->out, ").data, (int)(");
    emit_expr(c, a[n - 1]);
    sb_put(&c->out, ")))");
    return 1;
}
static int db_recv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_dbf_replication_recv(gmb_db_current_dbf(), (int)(");
    emit_expr(c, a[n - 1]);
    sb_put(&c->out, ")))");
    return 1;
}
static int db_lookup(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_db_dbf_lookup_str(gmb_db_current_dbf(), (");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[2]); sb_put(&c->out, ").data)");
    return 1;
}
static int db_alias(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_db_dbf_get_alias_str(gmb_db_current_dbf())");
    return 1;
}
static int db_selected(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) {
        /* SELECTED(): numero d'area 1-based corrente (DBF, altrimenti ISAM), 0 se nessuna.
         * Parità IL/PCode/VM-C. */
        sb_put(&c->out, "((gmb_double_t)(gmb_db_current_area() > 0 ? gmb_db_current_area() : (gmb_db_current_isam() > 0 ? 1.0 : 0.0)))");
        return 1;
    }
    /* SELECTED(alias) / SELECTED("alias"): area dell'alias. Ordine: ISAM, area DBF
     * (gmb_db_area_of_alias), RDD. Un identificatore nudo è il NOME dell'alias (non il
     * valore di una variabile), come nella forma legacy DBF; una stringa/espressione è
     * valutata. Una sola valutazione dell'argomento via statement-expression. */
    sb_put(&c->out, "({ gmb_string_t __sela = ");
    if (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        sb_printf(&c->out, "gmb_string_alloc(\"%s\")", a[0]->var);
    } else {
        sb_put(&c->out, "(");
        emit_str_operand(c, a[0]);
        sb_put(&c->out, ")");
    }
    sb_put(&c->out, "; (gmb_double_t)(gmb_db_isam_selected(__sela.data) > 0 ? gmb_db_isam_selected(__sela.data) : (gmb_db_area_of_alias(__sela.data) > 0 ? gmb_db_area_of_alias(__sela.data) : gmb_selected(__sela.data))); })");
    return 1;
}
static int db_indexord(ctx_t* c, gmb_expr_t** a, int n) {
    /* INDEXORD è LongType in StdLib (numero dell'ordine attivo, 0 = fisico): NON una stringa. */
    (void)a; (void)n;
    sb_put(&c->out, "gmb_indexord_dbl(0)");
    return 1;
}
static int db_indexkey(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_indexkey(0)");
    return 1;
}
static int db_neterr(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_neterr(0)");
    return 1;
}
int emit_udt_array_csv(ctx_t* c, const char* handleExpr, const char* varName, const char* fnAbi);  /* gmb_c99_emit.c */
const char* emit_dim_count_expr(ctx_t* c, const char* varName);                                     /* gmb_c99_emit.c */
int emit_append_dict_abi(ctx_t* c, gmb_expr_t* dict, const char* fieldsAbi,
                         const char* writeAbi, const char* handleAbi);                              /* gmb_c99_emit.c */
/* APPENDMANY su una LIST dinamica (`DIM v AS LIST OF X`, gkind GK_LIST): valori = .items,
 * conteggio = .count (NON sizeof: `gmb_list_t` è una struct, non un array C — il sizeof
 * produceva un errore di compilazione su c99n, DbIsamAppendMany). */
static int appendmany_is_list(ctx_t* c, const char* vn) {
    var_t* v = c->cur_fn ? vars_find(&c->cur_fn->locals, vn) : NULL;
    if (!v) v = vars_find(&c->globals, vn);
    return v && v->gkind == GK_LIST;
}
static int db_appendmany(ctx_t* c, gmb_expr_t** a, int n) {
    const char* vn = NULL; int i;
    for (i = n - 1; i >= 0 && !vn; i--)
        if (a[i] && a[i]->kind == GMB_EXPR_VAR && a[i]->var && !strchr(a[i]->var, '.'))
            vn = a[i]->var;
    if (!vn) return 0;
    /* Conteggio: dal contatore a scope di funzione emesso dal DIM (`<var>_count`); se il
     * bound non è noto si ricade su sizeof (array C veri). */
    {
        const char* cnt = emit_dim_count_expr(c, vn);
        sb_put(&c->out, "((gmb_double_t)gmb_db_dbf_append_many_boxed(gmb_db_current_dbf(), ");
        if (appendmany_is_list(c, vn)) { sb_put(&c->out, vn); sb_put(&c->out, ".items"); }
        else sb_put(&c->out, vn);
        if (cnt) { sb_put(&c->out, ", "); sb_put(&c->out, cnt); }
        else if (appendmany_is_list(c, vn)) { sb_put(&c->out, ", (int)"); sb_put(&c->out, vn); sb_put(&c->out, ".count"); }
        else {
            sb_put(&c->out, ", (int)(sizeof("); sb_put(&c->out, vn);
            sb_put(&c->out, ")/sizeof("); sb_put(&c->out, vn); sb_put(&c->out, "[0]))");
        }
        sb_put(&c->out, "))");
    }
    return 1;
}
static const rd_row DB_TBL[] = {
    { GMB_TOK_DbfAppendMany, db_appendmany },
    { GMB_TOK_Used, db_used },
    { GMB_TOK_IndexOrd, db_indexord }, { GMB_TOK_IndexKey, db_indexkey },
    { GMB_TOK_NetErr, db_neterr },
    { GMB_TOK_Alias, db_alias }, { GMB_TOK_Selected, db_selected },
    { GMB_TOK_Bof, db_bof }, { GMB_TOK_Eof, db_eof }, { GMB_TOK_EofFunc, db_eof },
    { GMB_TOK_Found, db_found }, { GMB_TOK_Deleted, db_deleted },
    { GMB_TOK_Lastrec, db_lastrec }, { GMB_TOK_Recno, db_recno },
    { GMB_TOK_RecSize, db_recsize }, { GMB_TOK_LUpdate, db_lupdate },
    { GMB_TOK_IsamInfo, db_isam_info },
    { GMB_TOK_DbFilter, db_filter }, { GMB_TOK_DbRelation, db_relation },
    { GMB_TOK_FieldName, db_fieldname },
    { GMB_TOK_DbfSetDurability, db_setdur }, { GMB_TOK_DbfFlushDurability, db_flushdur },
    { GMB_TOK_DbfBeginGroupCommit, db_begingc }, { GMB_TOK_DbfEndGroupCommit, db_endgc },
    { GMB_TOK_RLock, db_rlock }, { GMB_TOK_FLock, db_flock },
    { GMB_TOK_DbfSetReplication, db_setrepl },
    { GMB_TOK_DbfApplyReplication, db_applyrepl },
    { GMB_TOK_DbfFollowReplication, db_followrepl },
    { GMB_TOK_DbfReplicationShipTcp, db_ship },
    { GMB_TOK_DbfReplicationRecvTcp, db_recv },
    { GMB_TOK_DbfLookup, db_lookup },
};
int emit_bf_dbf(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(DB_TBL) / sizeof(DB_TBL[0]); i++)
        if (DB_TBL[i].tk == tk) return DB_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: isam (funzioni) ── */
static int is_integrity(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_db_isam_integrity_str(gmb_db_current_isam())");
    return 1;
}
/* ISAM.LOG([area]) → riepilogo WAL (gmb_db_isam_log_str; unica fonte:
 * WriteAheadLog_Inspect + WalLogSummary_Format). Prima il simbolo non esisteva. */
static int is_log(ctx_t* c, gmb_expr_t** a, int n) {
    if (n >= 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && !strchr(a[0]->var, '.')) {
        sb_put(&c->out, "gmb_db_isam_log_str(gmb_db_isam_handle_for(\"");
        sb_put(&c->out, a[0]->var);
        sb_put(&c->out, "\"))");
    } else if (n >= 1 && etype(c, a[0]) == TY_STR) {
        /* alias come STRINGA (`ISAM.LOG("X")`, o una variabile STRING): handle dall'alias — prima il
         * gmb_string_t veniva castato a int64 → errore di compilazione C. */
        sb_put(&c->out, "gmb_db_isam_log_str(gmb_db_isam_handle_for((");
        emit_str_operand(c, a[0]); sb_put(&c->out, ").data))");
    } else if (n >= 1) {
        sb_put(&c->out, "gmb_db_isam_log_str((int64_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))");
    } else {
        sb_put(&c->out, "gmb_db_isam_log_str(gmb_db_current_isam())");
    }
    return 1;
}
static int is_setrepl(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_db_isam_set_replication_str(gmb_db_current_isam(), (");
    emit_str_operand(c, a[n - 1]);
    sb_put(&c->out, ").data)");
    return 1;
}
static int is_applyrepl(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_isam_apply_replication(gmb_db_current_isam(), (");
    emit_str_operand(c, a[n - 1]);
    sb_put(&c->out, ").data))");
    return 1;
}
static int is_followrepl(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_isam_follow_replication(gmb_db_current_isam(), (");
    emit_str_operand(c, a[n - 1]);
    sb_put(&c->out, ").data, NULL))");
    return 1;
}
static int is_ship(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_isam_replication_ship(gmb_db_current_isam(), (");
    emit_str_operand(c, a[n - 2]);
    sb_put(&c->out, ").data, (int)(");
    emit_expr(c, a[n - 1]);
    sb_put(&c->out, ")))");
    return 1;
}
static int is_recv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_db_isam_replication_recv(gmb_db_current_isam(), (int)(");
    emit_expr(c, a[n - 1]);
    sb_put(&c->out, ")))");
    return 1;
}
static int is_bof(ctx_t* c, gmb_expr_t** a, int n) { return db_bit(c, a, n, "gmb_db_isam_bof"); }
static int is_eof(ctx_t* c, gmb_expr_t** a, int n) { return db_bit(c, a, n, "gmb_db_isam_eof"); }
static int is_found(ctx_t* c, gmb_expr_t** a, int n) { return db_bit(c, a, n, "gmb_db_isam_found"); }
static int is_recno(ctx_t* c, gmb_expr_t** a, int n) { (void)a; (void)n; sb_put(&c->out, "((gmb_double_t)gmb_db_isam_recno(gmb_db_current_isam() > 0 ? gmb_db_current_isam() : gmb_db_current_dbf()))"); return 1; }
/* Cerca un record LETTERALE {Campo: valore} dentro l'espressione, anche annidato:
 * `{{ID: "R003"}}` arriva come lista/call che CONTIENE il dict (un livello sotto). */
static gmb_expr_t* find_dict_lit(gmb_expr_t* e, int depth) {
    int i;
    if (!e || depth > 3) return NULL;
    if (e->kind == GMB_EXPR_CALL && e->var && strcmp(e->var, "_dict_") == 0 && e->nargs >= 2) return e;
    if (e->kind == GMB_EXPR_CALL)
        for (i = 0; i < e->nargs; i++) {
            gmb_expr_t* r = find_dict_lit(e->args[i], depth + 1);
            if (r) return r;
        }
    return NULL;
}
static int is_appendmany(ctx_t* c, gmb_expr_t** a, int n) {
    const char* vn = NULL; int i;
    /* LISTA LETTERALE di record (`[{ID: "A"}, {ID: "B"}]`): UN append per ogni dict (la ricerca del
     * primo dict qui sotto ne inseriva UNO solo → APPENDMANY ritornava 1). Somma dei conteggi. */
    for (i = n - 1; i >= 0; i--) {
        gmb_expr_t* arr = a[i];
        int all_dict = arr && arr->kind == GMB_EXPR_CALL && arr->var && arr->nargs > 0
                       && (strcmp(arr->var, "_array_") == 0 || strcmp(arr->var, "_list_") == 0);
        for (int j = 0; all_dict && j < arr->nargs; j++)
            if (!arr->args[j] || arr->args[j]->kind != GMB_EXPR_CALL || !arr->args[j]->var
                || strcmp(arr->args[j]->var, "_dict_") != 0) all_dict = 0;
        if (!all_dict) continue;
        sb_put(&c->out, "((gmb_double_t)(");
        for (int j = 0; j < arr->nargs; j++) {
            if (j) sb_put(&c->out, " + ");
            if (!emit_append_dict_abi(c, arr->args[j], "gmb_db_isam_fields", "gmb_db_isam_insert",
                                      "gmb_db_current_isam")) sb_put(&c->out, "0");
        }
        sb_put(&c->out, "))");
        return 1;
    }
    /* Forma con RECORD LETTERALE ({…} o {{…}}): CSV costruito sullo schema reale della
     * tabella (gmb_db_isam_fields), come per DBF APPEND RECORD. */
    for (i = n - 1; i >= 0; i--) {
        gmb_expr_t* d = find_dict_lit(a[i], 0);
        if (d) return emit_append_dict_abi(c, d, "gmb_db_isam_fields", "gmb_db_isam_insert",
                                           "gmb_db_current_isam");
    }
    for (i = n - 1; i >= 0 && !vn; i--)
        if (a[i] && a[i]->kind == GMB_EXPR_VAR && a[i]->var && !strchr(a[i]->var, '.'))
            vn = a[i]->var;
    if (!vn) return 0;
    /* Conteggio: dal contatore a scope di funzione emesso dal DIM (`<var>_count`); se il
     * bound non è noto si ricade su sizeof (array C veri). */
    {
        const char* cnt = emit_dim_count_expr(c, vn);
        sb_put(&c->out, "((gmb_double_t)gmb_db_isam_append_many_boxed(gmb_db_current_isam(), ");
        if (appendmany_is_list(c, vn)) { sb_put(&c->out, vn); sb_put(&c->out, ".items"); }
        else sb_put(&c->out, vn);
        if (cnt) { sb_put(&c->out, ", "); sb_put(&c->out, cnt); }
        else if (appendmany_is_list(c, vn)) { sb_put(&c->out, ", (int)"); sb_put(&c->out, vn); sb_put(&c->out, ".count"); }
        else {
            sb_put(&c->out, ", (int)(sizeof("); sb_put(&c->out, vn);
            sb_put(&c->out, ")/sizeof("); sb_put(&c->out, vn); sb_put(&c->out, "[0]))");
        }
        sb_put(&c->out, "))");
    }
    return 1;
}
static const rd_row IS_TBL[] = {
    { GMB_TOK_IsamAppendMany, is_appendmany },
    { GMB_TOK_IsamLog, is_log },
    { GMB_TOK_IsamInfo, db_isam_info },
    { GMB_TOK_Bof, is_bof }, { GMB_TOK_Eof, is_eof }, { GMB_TOK_EofFunc, is_eof },
    { GMB_TOK_Found, is_found }, { GMB_TOK_Recno, is_recno },
    { GMB_TOK_IsamIntegrity, is_integrity },
    { GMB_TOK_IsamSetReplication, is_setrepl },
    { GMB_TOK_IsamApplyReplication, is_applyrepl },
    { GMB_TOK_IsamFollowReplication, is_followrepl },
    { GMB_TOK_IsamReplicationShipTcp, is_ship },
    { GMB_TOK_IsamReplicationRecvTcp, is_recv },
};
int emit_bf_isam(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(IS_TBL) / sizeof(IS_TBL[0]); i++)
        if (IS_TBL[i].tk == tk) return IS_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: sql (delegazione §0quater) ── */
static int sq_fetch(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_sql_fetch_next((gmb_int_t)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int sq_exec(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(");
    if (n == 2) {
        sb_put(&c->out, "gmb_sql_exec("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data) == 0 ? (gmb_double_t)gmb_sql_changes(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data) : 0.0)");
    } else {
        sb_put(&c->out, "gmb_sql_exec_params("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, (const char*[]){ ");
        for (int i = 2; i < n; i++) { if (i > 2) sb_put(&c->out, ", "); emit_str_operand(c, a[i]); sb_put(&c->out, ".data"); }
        sb_printf(&c->out, " }, %d) == 0 ? (gmb_double_t)gmb_sql_changes(", n - 2);
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data) : 0.0)");
    }
    return 1;
}
static int sq_last(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_sql_last_rowid("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1; }
static int sq_chg(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_sql_changes("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1; }
/* G10 (2026-10-09): ergonomia DataFrame (DESCRIBE/TOCSV/FROMCSV). */
static int sq_describe(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ char __sqb[8192]; gmb_sql_describe((gmb_int_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), __sqb, sizeof(__sqb)); gmb_string_alloc(__sqb); })");
    return 1;
}
static int sq_tocsv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_sql_to_csv((gmb_int_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), "); emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int sq_fromcsv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_sql_from_csv(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data)");
    return 1;
}
/* G10: SQL.COL(rs, col) → gmb_list_t (colonna intera, valori tipizzati). */
static int sq_col(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_sql_col_list((gmb_int_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), "); emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } sq_row;
static const sq_row SQL_TBL[] = {
    { GMB_TOK_Fetch, sq_fetch }, { GMB_TOK_EXEC, sq_exec },
    { GMB_TOK_LASTINSERTROWID, sq_last }, { GMB_TOK_CHANGES, sq_chg },
    /* Token risolti dal parser per SQL.EXEC/LASTINSERTROWID/CHANGES. */
    { GMB_TOK_SqlExec, sq_exec }, { GMB_TOK_SqlLastInsertRowId, sq_last },
    { GMB_TOK_SqlChanges, sq_chg }, { GMB_TOK_SqlFetch, sq_fetch },
    { GMB_TOK_SqlDfSchema, sq_describe }, { GMB_TOK_SqlCol, sq_col }, { GMB_TOK_SqlToCsv, sq_tocsv }, { GMB_TOK_SqlFromCsv, sq_fromcsv },
};
int emit_bf_sql(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(SQL_TBL) / sizeof(SQL_TBL[0]); i++)
        if (SQL_TBL[i].tk == tk) return SQL_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: joy (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } jy_row;
static int jy_ports(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_joy_count()"); return 1; }
static int jy_avail(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_joy_is_present((int)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static int jy_name(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_string_alloc(gmb_joy_get_name((int)("); emit_expr(c, a[0]); sb_put(&c->out, ")))"); return 1; }
static int jy_mask(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_joy_get_state((int)("); emit_expr(c, a[0]); sb_put(&c->out, "))"); return 1; }
static const jy_row JY_TBL[] = {
    { GMB_TOK_JoyCount, jy_ports }, { GMB_TOK_JoyPresent, jy_avail },
    { GMB_TOK_JoyName, jy_name },   { GMB_TOK_Joy, jy_mask },
};
int emit_bf_joy(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(JY_TBL) / sizeof(JY_TBL[0]); i++)
        if (JY_TBL[i].tk == tk) return JY_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: dist (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } ds_row;
static int ds_sh2(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int ds_sh2_sf(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) return 0;
    sb_put(&c->out, "(1.0 - "); sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, "))");
    return 1;
}
static int ds_sh3_sf(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) return 0;
    sb_put(&c->out, "(1.0 - "); sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[2]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, "))");
    return 1;
}
static int ds_weibull_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3_sf(c, a, n, "gmb_dist_weibull_cdf"); }
static int ds_f_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3_sf(c, a, n, "gmb_dist_f_cdf"); }
static int ds_sh3(ctx_t* c, gmb_expr_t** a, int n, const char* fn);
static int ds_pareto_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_pareto_cdf"); }
static int ds_pareto_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3_sf(c, a, n, "gmb_dist_pareto_cdf"); }
static int ds_uniform_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_uniform_cdf"); }
static int ds_uniform_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3_sf(c, a, n, "gmb_dist_uniform_cdf"); }
static int ds_beta_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3_sf(c, a, n, "gmb_dist_beta_cdf"); }
static int ds_chi_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2_sf(c, a, n, "gmb_dist_chi_cdf"); }
static int ds_logistic_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_logistic_cdf"); }
static int ds_logistic_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3_sf(c, a, n, "gmb_dist_logistic_cdf"); }
static int ds_shtri(ctx_t* c, gmb_expr_t** a, int n, const char* fn);
static int ds_triangular_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_shtri(c, a, n, "gmb_dist_triangular_cdf"); }
static int ds_shtri_sf(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_put(&c->out, "(1.0 - "); sb_put(&c->out, fn); sb_put(&c->out, "(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 4) emit_expr(c, a[3]); else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "0.5");
    sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, "))");
    return 1;
}
static int ds_triangular_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_shtri_sf(c, a, n, "gmb_dist_triangular_cdf"); }
static int ds_weibull_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_weibull_isf"); }
static int ds_uniform_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_uniform_isf"); }
static int ds_pareto_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_pareto_isf"); }
static int ds_beta_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_beta_isf"); }
static int ds_chi_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi_isf"); }
static int ds_logistic_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_logistic_isf"); }
static int ds_lognormal_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_lognormal_isf"); }
static int ds_triangular_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_shtri(c, a, n, "gmb_dist_triangular_isf"); }
static int ds_exp_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2_sf(c, a, n, "gmb_dist_exp_cdf"); }
static int ds_student_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2_sf(c, a, n, "gmb_dist_student_t_cdf"); }
static int ds_chisq_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2_sf(c, a, n, "gmb_dist_chi2_cdf"); }
static int ds_sh2i(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    sb_put(&c->out, ")");
    return 1;
}
static int ds_sh3(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[2]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
/* Normal/Lognormal: mu e sigma sono OPTIONAL (default 0 e 1 — gli stessi default del
 * metodo CommonLib Numerics.*). Registro: 1° argomento = x (o p per ppf/isf); il runtime
 * vuole fn(mu, sigma, x). Con 1 argomento → (0, 1, x); con 2 → (mu, 1, x). Senza, i call
 * "corti" come `STATS.NORMALPDF(0)` fallivano ("non definita"). */
static int ds_sh3nm(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int ds_sh3nm_sf(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_put(&c->out, "(1.0 - "); sb_put(&c->out, fn); sb_put(&c->out, "(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, "))");
    return 1;
}
static int ds_normal_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3nm_sf(c, a, n, "gmb_dist_normal_cdf"); }
static int ds_lognormal_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3nm_sf(c, a, n, "gmb_dist_lognormal_cdf"); }
static int ds_sh3i(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[2]); sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    sb_put(&c->out, ")");
    return 1;
}
/* Gamma (registro STATS ordina (shape, scale, x/p) come la runtime gmb_gamma_*). */
static int ds_dir3(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[0]); sb_put(&c->out, ", ");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[2]);
    sb_put(&c->out, ")");
    return 1;
}
static int ds_gamma_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_dir3(c, a, n, "gmb_gamma_cdf"); }
static int ds_gamma_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_dir3(c, a, n, "gmb_gamma_sf"); }
static int ds_gamma_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_dir3(c, a, n, "gmb_gamma_pdf"); }
static int ds_gamma_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_dir3(c, a, n, "gmb_gamma_ppf"); }
static int ds_gamma_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_dir3(c, a, n, "gmb_gamma_isf"); }
static int ds_shbinom(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 3) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[2]); sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ")");
    sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    sb_put(&c->out, ")");
    return 1;
}
static int ds_shhyper(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 4) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ")");
    sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[2]); sb_put(&c->out, ")");
    sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[3]); sb_put(&c->out, ")");
    sb_put(&c->out, ", ");
    sb_put(&c->out, "(int)("); emit_expr(c, a[0]); sb_put(&c->out, ")");
    sb_put(&c->out, ")");
    return 1;
}
/* SKEWEDNORMALPDF(x, location=0, scale=1, alpha=0) — 2°/3°/4° argomento OPTIONAL. */
static int ds_shskew(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[0]); sb_put(&c->out, ", ");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    if (n >= 4) emit_expr(c, a[3]); else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
    return 1;
}
/* TRIANGULARPDF(x, min=0, mode=0.5, max=1) — default come EmitOptionalParamsCall del IL.
 * Mapping posizionale invariato (a[1],a[3],a[2],a[0] = min,max,mode,x). */
static int ds_shtri(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 4) emit_expr(c, a[3]); else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "0.5");
    sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
/* UNIFORMPDF(x, min=0, max=1). */
static int ds_sh3u(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    emit_expr(c, a[0]);
    sb_put(&c->out, ")");
    return 1;
}
static int ds_bernoulli(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2i(c, a, n, "gmb_dist_bernoulli_pmf"); }
static int ds_beta_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_beta_cdf"); }
static int ds_beta_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_beta_pdf"); }
static int ds_beta_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_beta_ppf"); }
static int ds_binomial(ctx_t* c, gmb_expr_t** a, int n) { return ds_shbinom(c, a, n, "gmb_dist_binomial_pmf"); }
static int ds_chi_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi_ppf"); }
static int ds_chi_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi_cdf"); }
static int ds_chi_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi_pdf"); }
static int ds_chisq_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi2_cdf"); }
static int ds_ncchi2_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_nc_chi2_pdf"); }
static int ds_ncchi2_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_nc_chi2_cdf"); }
static int ds_ncchi2_sf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_nc_chi2_sf"); }
static int ds_sh4(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 4) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[2]); sb_put(&c->out, ", ");
    emit_expr(c, a[3]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int ds_ncbetapdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh4(c, a, n, "gmb_dist_nc_beta_pdf"); }
static int ds_ncbetacdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh4(c, a, n, "gmb_dist_nc_beta_cdf"); }
static int ds_ncbetasf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh4(c, a, n, "gmb_dist_nc_beta_sf"); }
static int ds_ncgammapdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_nc_gamma_pdf"); }
static int ds_ncgammacdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_nc_gamma_cdf"); }
static int ds_ncgammasf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_nc_gamma_sf"); }
static int ds_chisq_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi2_isf"); }
static int ds_chisq_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi2_pdf"); }
static int ds_chisq_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_chi2_ppf"); }
static int ds_exp_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_exp_cdf"); }
static int ds_exp_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_exp_isf"); }
static int ds_exp_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_exp_pdf"); }
static int ds_exp_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_exp_ppf"); }
static int ds_f_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_f_cdf"); }
static int ds_f_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_f_isf"); }
static int ds_f_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_f_pdf"); }
static int ds_f_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_f_ppf"); }
static int ds_geometric(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2i(c, a, n, "gmb_dist_geometric_pmf"); }
static int ds_hypergeom(ctx_t* c, gmb_expr_t** a, int n) { return ds_shhyper(c, a, n, "gmb_dist_hypergeom_pmf"); }
static int ds_logistic_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_logistic_pdf"); }
static int ds_logistic_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_logistic_ppf"); }
static int ds_lognormal_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3nm(c, a, n, "gmb_dist_lognormal_cdf"); }
static int ds_lognormal_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3nm(c, a, n, "gmb_dist_lognormal_pdf"); }
static int ds_lognormal_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_lognormal_ppf"); }
static int ds_negbinom(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3i(c, a, n, "gmb_dist_negbinom_pmf"); }
static int ds_normal_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3nm(c, a, n, "gmb_dist_normal_cdf"); }
static int ds_normal_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_normal_isf"); }
static int ds_normal_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3nm(c, a, n, "gmb_dist_normal_pdf"); }
static int ds_normal_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_normal_ppf"); }
static int ds_pareto_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_pareto_pdf"); }
static int ds_pareto_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_pareto_ppf"); }
static int ds_poisson(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2i(c, a, n, "gmb_dist_poisson_pmf"); }
static int ds_skewnormal(ctx_t* c, gmb_expr_t** a, int n) { return ds_shskew(c, a, n, "gmb_dist_skewnormal_pdf"); }
static int ds_studentt_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_student_t_cdf"); }
static int ds_studentt_isf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_student_t_isf"); }
static int ds_studentt_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_student_t_pdf"); }
static int ds_studentt_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh2(c, a, n, "gmb_dist_student_t_ppf"); }
static int ds_triangular_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_shtri(c, a, n, "gmb_dist_triangular_pdf"); }
static int ds_triangular_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_shtri(c, a, n, "gmb_dist_triangular_ppf"); }
static int ds_uniform_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3u(c, a, n, "gmb_dist_uniform_pdf"); }
static int ds_uniform_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_uniform_ppf"); }
static int ds_weibull_cdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_weibull_cdf"); }
static int ds_weibull_pdf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_weibull_pdf"); }
static int ds_weibull_ppf(ctx_t* c, gmb_expr_t** a, int n) { return ds_sh3(c, a, n, "gmb_dist_weibull_ppf"); }
static const ds_row DS_TBL[] = {
    { GMB_TOK_BernoulliPmf, ds_bernoulli },
    { GMB_TOK_BetaCdf, ds_beta_cdf },
    { GMB_TOK_StatsBetaSf, ds_beta_sf },
    { GMB_TOK_BetaPdf, ds_beta_pdf },
    { GMB_TOK_BetaPpf, ds_beta_ppf },
    { GMB_TOK_BinomialPmf, ds_binomial },
    { GMB_TOK_ChiPpf, ds_chi_ppf },
    { GMB_TOK_ChiCdf, ds_chi_cdf },
    { GMB_TOK_StatsChiSf, ds_chi_sf },
    { GMB_TOK_ChiPdf, ds_chi_pdf },
    { GMB_TOK_ChiSquaredCdf, ds_chisq_cdf },
    { GMB_TOK_StatsNcChi2Pdf, ds_ncchi2_pdf },
    { GMB_TOK_StatsNcChi2Cdf, ds_ncchi2_cdf },
    { GMB_TOK_StatsNcChi2Sf, ds_ncchi2_sf },
    { GMB_TOK_StatsNcBetaPdf, ds_ncbetapdf },
    { GMB_TOK_StatsNcBetaCdf, ds_ncbetacdf },
    { GMB_TOK_StatsNcBetaSf, ds_ncbetasf },
    { GMB_TOK_StatsNcGammaPdf, ds_ncgammapdf },
    { GMB_TOK_StatsNcGammaCdf, ds_ncgammacdf },
    { GMB_TOK_StatsNcGammaSf, ds_ncgammasf },
    { GMB_TOK_StatsChiSquaredSf, ds_chisq_sf }, 
    { GMB_TOK_ChiSquaredIsf, ds_chisq_isf },
    { GMB_TOK_ChiSquaredPdf, ds_chisq_pdf },
    { GMB_TOK_ChiSquaredPpf, ds_chisq_ppf },
    { GMB_TOK_ExponentialCdf, ds_exp_cdf },
    { GMB_TOK_StatsExpSf, ds_exp_sf },
    { GMB_TOK_ExponentialIsf, ds_exp_isf },
    { GMB_TOK_ExponentialPdf, ds_exp_pdf },
    { GMB_TOK_ExponentialPpf, ds_exp_ppf },
    { GMB_TOK_FCdf, ds_f_cdf },
    { GMB_TOK_StatsFSf, ds_f_sf },
    { GMB_TOK_FIsf, ds_f_isf },
    { GMB_TOK_FPdf, ds_f_pdf },
    { GMB_TOK_FPpf, ds_f_ppf },
    { GMB_TOK_GeometricPmf, ds_geometric },
    { GMB_TOK_HypergeometricPmf, ds_hypergeom },
    { GMB_TOK_LogisticPdf, ds_logistic_pdf },
    { GMB_TOK_StatsLogisticCdf, ds_logistic_cdf },
    { GMB_TOK_StatsLogisticSf, ds_logistic_sf },
    { GMB_TOK_LogisticPpf, ds_logistic_ppf },
    { GMB_TOK_LognormalCdf, ds_lognormal_cdf },
    { GMB_TOK_StatsLognormalSf, ds_lognormal_sf },
    { GMB_TOK_LognormalPdf, ds_lognormal_pdf },
    { GMB_TOK_LognormalPpf, ds_lognormal_ppf },
    { GMB_TOK_NegativeBinomialPmf, ds_negbinom },
    { GMB_TOK_NormalCdf, ds_normal_cdf },
    { GMB_TOK_StatsNormalSf, ds_normal_sf },
    { GMB_TOK_NormalIsf, ds_normal_isf },
    { GMB_TOK_NormalPdf, ds_normal_pdf },
    { GMB_TOK_NormalPpf, ds_normal_ppf },
    { GMB_TOK_ParetoPdf, ds_pareto_pdf },
    { GMB_TOK_StatsParetoCdf, ds_pareto_cdf },
    { GMB_TOK_StatsParetoSf, ds_pareto_sf },
    { GMB_TOK_ParetoPpf, ds_pareto_ppf },
    { GMB_TOK_PoissonPmf, ds_poisson },
    { GMB_TOK_SkewedNormalPdf, ds_skewnormal },
    { GMB_TOK_StudentTCdf, ds_studentt_cdf },
    { GMB_TOK_StatsStudentTSf, ds_student_sf },
    { GMB_TOK_StudentTIsf, ds_studentt_isf },
    { GMB_TOK_StudentTPdf, ds_studentt_pdf },
    { GMB_TOK_StudentTPpf, ds_studentt_ppf },
    { GMB_TOK_TriangularPdf, ds_triangular_pdf },
    { GMB_TOK_TriangularPpf, ds_triangular_ppf },
    { GMB_TOK_StatsTriangularCdf, ds_triangular_cdf },
    { GMB_TOK_StatsTriangularSf, ds_triangular_sf },
    { GMB_TOK_StatsWeibullIsf, ds_weibull_isf },
    { GMB_TOK_StatsUniformIsf, ds_uniform_isf },
    { GMB_TOK_StatsParetoIsf, ds_pareto_isf },
    { GMB_TOK_StatsBetaIsf, ds_beta_isf },
    { GMB_TOK_StatsChiIsf, ds_chi_isf },
    { GMB_TOK_StatsLogisticIsf, ds_logistic_isf },
    { GMB_TOK_StatsLognormalIsf, ds_lognormal_isf },
    { GMB_TOK_StatsTriangularIsf, ds_triangular_isf },
    { GMB_TOK_UniformPdf, ds_uniform_pdf },
    { GMB_TOK_UniformPpf, ds_uniform_ppf },
    { GMB_TOK_StatsUniformCdf, ds_uniform_cdf },
    { GMB_TOK_StatsUniformSf, ds_uniform_sf },
    { GMB_TOK_WeibullCdf, ds_weibull_cdf },
    { GMB_TOK_StatsWeibullSf, ds_weibull_sf },
    { GMB_TOK_WeibullPdf, ds_weibull_pdf },
    { GMB_TOK_WeibullPpf, ds_weibull_ppf },
    { GMB_TOK_StatsGammaCdf, ds_gamma_cdf },
    { GMB_TOK_StatsGammaSf, ds_gamma_sf },
    { GMB_TOK_StatsGammaPdf, ds_gamma_pdf },
    { GMB_TOK_StatsGammaPpf, ds_gamma_ppf },
    { GMB_TOK_StatsGammaIsf, ds_gamma_isf },
};
int emit_bf_dist(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(DS_TBL) / sizeof(DS_TBL[0]); i++)
        if (DS_TBL[i].tk == tk) return DS_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: hash (delegazione §0quater) ── */
static int hs_h(ctx_t* c, gmb_expr_t** args, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s((const uint8_t*)", fn);
    emit_str_operand(c, args[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, args[0]); sb_put(&c->out, ".length))");
    return 1;
}
/* (2026-09-07) digest BYTE (MD5/SHA*, ritorno byte[]) → gmb_string_t di BYTE GREZZI
 * (parity IL/PCodeVM/VM-C: LEN(MD5("abc"))=16; PRINT/TOHEX lo esadecimalizzano). */
static int hs_raw(ctx_t* c, gmb_expr_t** a, int n, int algo) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_digest_raw(%d, (const uint8_t*)(", algo);
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (size_t)(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").length)");
    return 1;
}
static int hs_md5(ctx_t* c, gmb_expr_t** a, int n) { return hs_raw(c, a, n, 0); }
static int hs_sha1(ctx_t* c, gmb_expr_t** a, int n) { return hs_raw(c, a, n, 1); }
static int hs_sha256(ctx_t* c, gmb_expr_t** a, int n) { return hs_raw(c, a, n, 2); }
static int hs_sha512(ctx_t* c, gmb_expr_t** a, int n) { return hs_raw(c, a, n, 3); }
static int hs_strmd5(ctx_t* c, gmb_expr_t** a, int n) { return hs_h(c, a, n, "gmb_md5_hex"); }
static int hs_strsha256(ctx_t* c, gmb_expr_t** a, int n) { return hs_h(c, a, n, "gmb_sha256_hex"); }
static int hs_strsha1(ctx_t* c, gmb_expr_t** a, int n) { return hs_h(c, a, n, "gmb_sha1_hex"); }
static int hs_strsha512(ctx_t* c, gmb_expr_t** a, int n) { return hs_h(c, a, n, "gmb_sha512_hex"); }
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } hs_row;
static const hs_row HASH_TBL[] = {
    { GMB_TOK_Md5, hs_md5 }, { GMB_TOK_Sha1, hs_sha1 },
    { GMB_TOK_Sha256, hs_sha256 }, { GMB_TOK_Sha512, hs_sha512 },
    { GMB_TOK_StrMd5, hs_strmd5 }, { GMB_TOK_StrSha256, hs_strsha256 },
    { GMB_TOK_StrSha1, hs_strsha1 }, { GMB_TOK_StrSha512, hs_strsha512 },
};
int emit_bf_hash(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(HASH_TBL) / sizeof(HASH_TBL[0]); i++)
        if (HASH_TBL[i].tk == tk) return HASH_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: ts (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } ts_row;
static int ts_autocorr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix && n >= 2) {
        sb_put(&c->out, "gmb_ts_autocorrelation(");
        emit_var_ref(c, av); sb_put(&c->out, ", (gmb_int_t)(");
        emit_arr_len_of(c, av); sb_put(&c->out, "), (gmb_int_t)(");
        emit_expr(c, a[1]); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int ts_lyapunov(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_lyapunov("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int ts_bma_weights(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_bma_weights("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int ts_lz(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_lz_complexity("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int ts_perm_entropy(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_perm_entropy("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int ts_approxent(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_approx_entropy("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "), "); emit_expr(c, a[2]); sb_put(&c->out, ")");
        return 1;
    }
    return 0;
}
static int ts_sampen(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_sample_entropy("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "), "); emit_expr(c, a[2]); sb_put(&c->out, ")");
        return 1;
    }
    return 0;
}
static int ts_stationary_bootstrap(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_stationary_bootstrap("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "))");
        return 1;
    }
    return 0;
}
static int ts_kalman1d(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 5) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_kalman1d("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), ");
        emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]); sb_put(&c->out, ", ");
        emit_expr(c, a[3]); sb_put(&c->out, ", "); emit_expr(c, a[4]); sb_put(&c->out, ")");
        return 1;
    }
    return 0;
}
static int ts_fracdiff(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_fracdiff("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), ");
        emit_expr(c, a[1]); sb_put(&c->out, ")"); return 1;
    }
    return 0;
}
static int ts_hurst(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_hurst_dfa("); emit_var_ref(c, av);
        sb_put(&c->out, ", (int)("); emit_arr_len_of(c, av); sb_put(&c->out, "), 4)");
        return 1;
    }
    return 0;
}
static int ts_volatility(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
    var_t* av = lookup_var(c, a[0]->var);
    if (av && av->is_array && !av->matrix) {
        sb_put(&c->out, "gmb_ts_volatility(");
        emit_var_ref(c, av); sb_put(&c->out, ", (gmb_int_t)(");
        emit_arr_len_of(c, av); sb_put(&c->out, "), 0)");
        return 1;
    }
    return 0;
}
static const ts_row TS_TBL[] = {
    { GMB_TOK_Autocorrelation, ts_autocorr }, { GMB_TOK_Volatility, ts_volatility },
    { GMB_TOK_TsHurst, ts_hurst },
    { GMB_TOK_TsFracDiff, ts_fracdiff },
    { GMB_TOK_TsKalman1D, ts_kalman1d },
    { GMB_TOK_TsStationaryBootstrap, ts_stationary_bootstrap },
    { GMB_TOK_TsSampleEntropy, ts_sampen },
    { GMB_TOK_TsApproxEntropy, ts_approxent },
    { GMB_TOK_TsPermEntropy, ts_perm_entropy },
    { GMB_TOK_TsLzComplexity, ts_lz },
    { GMB_TOK_TsBmaWeights, ts_bma_weights },
    { GMB_TOK_TsLyapunov, ts_lyapunov },
};
int emit_bf_ts(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(TS_TBL) / sizeof(TS_TBL[0]); i++)
        if (TS_TBL[i].tk == tk) return TS_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: tool (CUA tool registry, gmb_agent_runtime/tool_registry.h) ── */
static int tool_register(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2 || !a[0] || !a[1]) return 0;
    /* TOOL.REGISTER(nome, descrizione[, provider]) → 1 se registrato, 0 altrimenti
     * (nome vuoto → 0). Parità IlRuntime.ToolRegister e VM/C gmb_bf_cua_toolregister
     * (che delega a gmb_tool_registry_register_simple: 0 = successo). */
    sb_put(&c->out, "({ gmb_string_t _trn = ");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, "; gmb_string_t _trd = ");
    emit_str_operand(c, a[1]);
    sb_put(&c->out, "; double _trr = (_trn.data && _trn.data[0]) ? "
                    "(gmb_tool_registry_register_simple(_trn.data, _trd.data ? _trd.data : \"\", ");
    if (n >= 3) { sb_put(&c->out, "("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data"); }
    else sb_put(&c->out, "\"native\"");
    sb_put(&c->out, ") == 0 ? 1.0 : 0.0) : 0.0; _trr; })");
    return 1;
}
int emit_bf_tool(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    if (tk == GMB_TOK_ToolRegister) return tool_register(c, args, nargs);
    return 0;
}
/* ── dominio: encoding (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } en_row;
/* (2026-09-07) operando "data" per encoder (TOHEX/TOBASE64/…): un digest MD5/SHA*
 * generico produce hex-text; qui va consumato come BYTE GREZZI (parity IL/PCodeVM/VM-C:
 * MD5 ritorna byte[], quindi TOHEX(MD5(x)) = hex del digest, non doppio hex). */
static void emit_bytes_operand(ctx_t* c, gmb_expr_t* a) {
    if (a && a->kind == GMB_EXPR_CALL && a->var) {
        /* RANDOMBYTES/RANDOMCRYPTOBYTES ritornano già gmb_string_t binaria: emetti la
         * chiamata così com'è (il chiamante appende .data/.length). */
        if (a->tk == GMB_TOK_RandomBytes || a->tk == GMB_TOK_RandomCryptoBytes) {
            emit_expr(c, a);
            return;
        }
        int algo = a->tk == GMB_TOK_Md5 ? 0 : a->tk == GMB_TOK_Sha1 ? 1
                 : a->tk == GMB_TOK_Sha256 ? 2 : a->tk == GMB_TOK_Sha512 ? 3 : -1;
        if (algo >= 0 && a->nargs >= 1 && a->args[0]) {
            sb_printf(&c->out, "gmb_digest_raw(%d, (const uint8_t*)(", algo);
            emit_bytes_operand(c, a->args[0]);
            sb_put(&c->out, ").data, (size_t)(");
            emit_bytes_operand(c, a->args[0]);
            sb_put(&c->out, ").length)");
            return;
        }
    }
    emit_str_operand(c, a);
}
static int en_encode(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s((const uint8_t*)", fn);
    emit_bytes_operand(c, a[0]);
    sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]);
    sb_put(&c->out, ".length))");
    return 1;
}
static int en_decode(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "({ size_t _dec_n = 0; uint8_t* _dec = %s(", fn);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, &_dec_n); gmb_string_alloc(_dec ? (char*)_dec : \"\"); })");
    return 1;
}
static int en_base58encode(ctx_t* c, gmb_expr_t** a, int n) { return en_encode(c, a, n, "gmb_base58_encode"); }
static int en_base32encode(ctx_t* c, gmb_expr_t** a, int n) { return en_encode(c, a, n, "gmb_base32_encode"); }
static int en_tobase64(ctx_t* c, gmb_expr_t** a, int n) { return en_encode(c, a, n, "gmb_base64_encode"); }
static int en_base58decode(ctx_t* c, gmb_expr_t** a, int n) { return en_decode(c, a, n, "gmb_base58_decode"); }
static int en_base32decode(ctx_t* c, gmb_expr_t** a, int n) { return en_decode(c, a, n, "gmb_base32_decode"); }
static int en_frombase64(ctx_t* c, gmb_expr_t** a, int n) { return en_decode(c, a, n, "gmb_base64_decode"); }
static int en_frombase64url(ctx_t* c, gmb_expr_t** a, int n) { return en_decode(c, a, n, "gmb_base64url_decode"); }
static int en_tohex(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_hex_encode_string((const uint8_t*)");
    emit_bytes_operand(c, a[0]);
    sb_put(&c->out, ".data, ");
    emit_bytes_operand(c, a[0]);
    sb_put(&c->out, ".length))");
    return 1;
}
static int en_fromhex(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1) return 0; sb_put(&c->out, "gmb_bytes_fromhex("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
static int en_keccak(ctx_t* c, gmb_expr_t** a, int n) { return en_encode(c, a, n, "gmb_keccak256_hex"); }
static int en_ripemd(ctx_t* c, gmb_expr_t** a, int n) { return en_encode(c, a, n, "gmb_ripemd160_hex"); }
static const en_row EN_TBL[] = {
    { GMB_TOK_Base58Encode, en_base58encode }, { GMB_TOK_Base32Encode, en_base32encode },
    { GMB_TOK_ToBase64, en_tobase64 },
    { GMB_TOK_Base58Decode, en_base58decode }, { GMB_TOK_Base32Decode, en_base32decode },
    { GMB_TOK_FromBase64, en_frombase64 }, { GMB_TOK_ToHex, en_tohex }, { GMB_TOK_FromHex, en_fromhex },
    { GMB_TOK_FromBase64Url, en_frombase64url },
    { GMB_TOK_StrKeccak256, en_keccak }, { GMB_TOK_StrRipeMd160, en_ripemd },
};
int emit_bf_encoding(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(EN_TBL) / sizeof(EN_TBL[0]); i++)
        if (EN_TBL[i].tk == tk) return EN_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: blockchain (delegazione §0quater) ──
 * 18 funzioni BLOCKCHAIN: hashing byte[] (KECCAK256/RIPEMD160/BLAKE2B256), encoding
 * (RLP/VarInt/ABI), indirizzi ETH/BTC, derivazione chiavi (BIP32/BIP39), contract proxy.
 * Tutto dal runtime condiviso gmb_runtime_c (§0bis): nessuno stub, nessuna logica inline. */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } bc_row;
/* Hash byte[] → gmb_string_t binaria (algo 4/5/6 di gmb_digest_raw, come MD5/SHA 0-3). */
static int bc_hash(ctx_t* c, gmb_expr_t** a, int n, int algo) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_digest_raw(%d, (const uint8_t*)", algo);
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length)");
    return 1;
}
static int bc_keccak(ctx_t* c, gmb_expr_t** a, int n) { return bc_hash(c, a, n, 4); }
static int bc_ripemd(ctx_t* c, gmb_expr_t** a, int n) { return bc_hash(c, a, n, 5); }
static int bc_blake(ctx_t* c, gmb_expr_t** a, int n) { return bc_hash(c, a, n, 6); }
/* RLPENCODE(data) → byte[] (uint8_t* + out-len → gmb_string_t binaria). */
static int bc_rlp(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ size_t _bc_n = 0; uint8_t* _bc_p = gmb_rlp_encode((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, &_bc_n); gmb_string_take_bytes(_bc_p, _bc_n); })");
    return 1;
}
/* VARINTENCODE(value) → byte[] LEB128. */
static int bc_varint_encode(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ size_t _bc_n = 0; uint8_t* _bc_p = gmb_varint_encode((int64_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "), &_bc_n); gmb_string_take_bytes(_bc_p, _bc_n); })");
    return 1;
}
/* VARINTDECODE(data[]) → int64 (double in contesto espressione). */
static int bc_varint_decode(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_varint_decode((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, NULL)");
    return 1;
}
/* ETHADDRFROMPUB(pub[]) → stringa "0x...". */
static int bc_eth_addr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_eth_address_from_pub_hex((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length))");
    return 1;
}
/* ETHADDRCHECKSUM(addr$) → stringa EIP-55. */
static int bc_eth_checksum(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_eth_address_checksum((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data))");
    return 1;
}
/* BTCADDRFROMPUB(pub[], testnet) → stringa base58check. */
static int bc_btc_addr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_btc_address_from_pub((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, (int)(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "0");
    sb_put(&c->out, ")))");
    return 1;
}
/* BLOCKDIFFICULTY(header[], bits) → double. */
static int bc_difficulty(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_block_difficulty((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, (int32_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
/* SECP256K1SIGN(msg[], priv[]) → byte[] DER. */
static int bc_sign(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "({ size_t _bc_n = 0; uint8_t* _bc_p = gmb_ecdsa_sign_raw((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, (const uint8_t*)");
    emit_bytes_operand(c, a[1]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[1]); sb_put(&c->out, ".length, &_bc_n); gmb_string_take_bytes(_bc_p, _bc_n); })");
    return 1;
}
/* GENERATEMNEMONIC(n) → stringa (default 12 parole). */
static int bc_mnemonic(ctx_t* c, gmb_expr_t** a, int n) {
    sb_put(&c->out, "gmb_string_alloc(gmb_bip39_generate_mnemonic((unsigned)(");
    if (n >= 1) emit_expr(c, a[0]); else sb_put(&c->out, "12");
    sb_put(&c->out, ")))");
    return 1;
}
/* MNEMONICTOSEED(mnemonic$[, passphrase$]) → byte[] (64). */
static int bc_mnemonic_to_seed(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ size_t _bc_n = 0; uint8_t* _bc_p = gmb_bip39_mnemonic_to_seed_alloc((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, ");
    if (n >= 2) { sb_put(&c->out, "("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data"); }
    else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", &_bc_n); gmb_string_take_bytes(_bc_p, _bc_n); })");
    return 1;
}
/* DERIVEKEY(seed[], path$) → dict boxed (privateKey/publicKey/chainCode). */
static int bc_derive_key(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_bip32_derive_key_dict_val((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, ");
    if (n >= 2) { sb_put(&c->out, "("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data"); }
    else sb_put(&c->out, "\"m\"");
    sb_put(&c->out, ")");
    return 1;
}
/* GETPUBLICKEY(priv[]) → byte[] (33 compressed). */
static int bc_get_pubkey(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ size_t _bc_n = 0; uint8_t* _bc_p = gmb_get_public_key((const uint8_t*)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".data, (size_t)");
    emit_bytes_operand(c, a[0]); sb_put(&c->out, ".length, &_bc_n); gmb_string_take_bytes(_bc_p, _bc_n); })");
    return 1;
}
/* CONTRACT(addr$, iface$[, rpc$]) → dict boxed {address, interface, rpcUrl}. */
static int bc_contract(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_contract_proxy_val((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data, ");
    if (n >= 3) { sb_put(&c->out, "("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data"); }
    else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ")");
    return 1;
}
/* ABIENCODE(sig$, args...) → stringa hex (gmb_abi_encode_vals). Gli argomenti variadici
 * sono impacchettati in un array gmb_val_t (stringa o double). */
static int bc_abi_encode(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    int argc = n - 1;
    sb_put(&c->out, "gmb_string_alloc(gmb_abi_encode_vals((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, ");
    if (argc > 0) {
        sb_put(&c->out, "(const gmb_val_t[]){ ");
        for (int i = 1; i < n; i++) {
            if (i > 1) sb_put(&c->out, ", ");
            if (etype(c, a[i]) == TY_STR) {
                sb_put(&c->out, "gmb_make_val_string((");
                emit_str_operand(c, a[i]); sb_put(&c->out, ").data)");
            } else {
                sb_put(&c->out, "gmb_make_val_double((");
                emit_expr(c, a[i]); sb_put(&c->out, "))");
            }
        }
        sb_put(&c->out, " }, ");
    } else {
        sb_put(&c->out, "NULL, ");
    }
    sb_printf(&c->out, "%d))", argc);
    return 1;
}
/* ABIDECODE(hex$, types[]) → double* (la forma `DIM r = ...` passa da dim_arr_init_kind). */
static int bc_abi_decode(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_abi_decode_doubles((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, &(int){0})");
    return 1;
}
static const bc_row BC_TBL[] = {
    { GMB_TOK_Keccak256, bc_keccak }, { GMB_TOK_RipeMd160, bc_ripemd },
    { GMB_TOK_Blake2b256, bc_blake },
    { GMB_TOK_RlpEncode, bc_rlp }, { GMB_TOK_VarIntEncode, bc_varint_encode },
    { GMB_TOK_VarIntDecode, bc_varint_decode },
    { GMB_TOK_EthAddressFromPub, bc_eth_addr }, { GMB_TOK_EthAddressChecksum, bc_eth_checksum },
    { GMB_TOK_BtcAddressFromPub, bc_btc_addr }, { GMB_TOK_BlockDifficulty, bc_difficulty },
    { GMB_TOK_EcdsaSecp256k1Sign, bc_sign },
    { GMB_TOK_GenerateMnemonic, bc_mnemonic }, { GMB_TOK_MnemonicToSeed, bc_mnemonic_to_seed },
    { GMB_TOK_DeriveKey, bc_derive_key }, { GMB_TOK_GetPublicKey, bc_get_pubkey },
    { GMB_TOK_Contract, bc_contract },
    { GMB_TOK_AbiEncode, bc_abi_encode }, { GMB_TOK_AbiDecode, bc_abi_decode },
};
int emit_bf_blockchain(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(BC_TBL) / sizeof(BC_TBL[0]); i++)
        if (BC_TBL[i].tk == tk) return BC_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: finance_scalar (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } fs_row;
static int fs_base(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    if (n > 3) { sb_put(&c->out, ", "); emit_expr(c, a[3]); } else sb_put(&c->out, ", 0");
    if (n > 4) { sb_put(&c->out, ", ((int)("); emit_expr(c, a[4]); sb_put(&c->out, "))"); } else sb_put(&c->out, ", 0");
    sb_put(&c->out, ")");
    return 1;
}
static int fs_pmt(ctx_t* c, gmb_expr_t** a, int n) { return fs_base(c, a, n, "gmb_fin_pmt"); }
static int fs_fv(ctx_t* c, gmb_expr_t** a, int n) { return fs_base(c, a, n, "gmb_fin_fv"); }
static int fs_pvext(ctx_t* c, gmb_expr_t** a, int n) { return fs_base(c, a, n, "gmb_fin_pvext"); }
static int fs_nper(ctx_t* c, gmb_expr_t** a, int n) { return fs_base(c, a, n, "gmb_fin_nper_ext"); }
static int fs_fvext(ctx_t* c, gmb_expr_t** a, int n) { return fs_base(c, a, n, "gmb_fin_fvext"); }
static int fs_rate(ctx_t* c, gmb_expr_t** a, int n) {
    /* RATE(nper, pmt, pv[, fv=0][, type=0][, guess=0.1]) */
    sb_put(&c->out, "gmb_fin_rate(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    if (n > 3) { sb_put(&c->out, ", "); emit_expr(c, a[3]); } else sb_put(&c->out, ", 0");
    if (n > 4) { sb_put(&c->out, ", ((int)("); emit_expr(c, a[4]); sb_put(&c->out, "))"); } else sb_put(&c->out, ", 0");
    if (n > 5) { sb_put(&c->out, ", "); emit_expr(c, a[5]); } else sb_put(&c->out, ", 0.1");
    sb_put(&c->out, ")");
    return 1;
}
static int fs_cumipmt(ctx_t* c, gmb_expr_t** a, int n) {
    /* CUMIPMT(rate, nper, pv, start, end[, type=0]) */
    sb_put(&c->out, "gmb_fin_cumipmt(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", (int)("); emit_expr(c, a[3]); sb_put(&c->out, "), (int)("); emit_expr(c, a[4]); sb_put(&c->out, ")");
    if (n > 5) { sb_put(&c->out, ", (int)("); emit_expr(c, a[5]); sb_put(&c->out, ")"); } else sb_put(&c->out, ", 0");
    sb_put(&c->out, ")");
    return 1;
}
static int fs_cumprinc(ctx_t* c, gmb_expr_t** a, int n) {
    /* CUMPRINC(rate, nper, pv, start, end[, type=0]) */
    sb_put(&c->out, "gmb_fin_cumprinc(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", (int)("); emit_expr(c, a[3]); sb_put(&c->out, "), (int)("); emit_expr(c, a[4]); sb_put(&c->out, ")");
    if (n > 5) { sb_put(&c->out, ", (int)("); emit_expr(c, a[5]); sb_put(&c->out, ")"); } else sb_put(&c->out, ", 0");
    sb_put(&c->out, ")");
    return 1;
}
static int fs_db(ctx_t* c, gmb_expr_t** a, int n) {
    /* DB(cost, salvage, life, period[, month=12]) */
    sb_put(&c->out, "gmb_fin_db(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", "); emit_expr(c, a[3]);
    if (n > 4) { sb_put(&c->out, ", ((int)("); emit_expr(c, a[4]); sb_put(&c->out, "))"); } else sb_put(&c->out, ", 12");
    sb_put(&c->out, ")");
    return 1;
}
static int fs_ddb(ctx_t* c, gmb_expr_t** a, int n) {
    /* DDB(cost, salvage, life, period[, factor=2]) */
    sb_put(&c->out, "gmb_fin_ddb(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", "); emit_expr(c, a[3]);
    if (n > 4) { sb_put(&c->out, ", "); emit_expr(c, a[4]); } else sb_put(&c->out, ", 2");
    sb_put(&c->out, ")");
    return 1;
}
static const fs_row FS_TBL[] = {
    { GMB_TOK_Pmt, fs_pmt }, { GMB_TOK_PmtExt, fs_pmt },
    { GMB_TOK_Fv, fs_fv }, { GMB_TOK_Pv, fs_pvext }, { GMB_TOK_PvExt, fs_pvext },
    { GMB_TOK_Nper, fs_nper }, { GMB_TOK_Rate, fs_rate },
    { GMB_TOK_Cumipmt, fs_cumipmt }, { GMB_TOK_Cumprinc, fs_cumprinc },
    { GMB_TOK_Db, fs_db }, { GMB_TOK_Ddb, fs_ddb }, { GMB_TOK_FvExt, fs_fvext },
    { GMB_TOK_YearFrac, fd_yearfrac }, { GMB_TOK_Days360, fd_days360 }, { GMB_TOK_NperExt, fd_nperext },
    /* TRADING.BONDS scalari con default (promossi da FD_TBL ns-first → token-first) */
    { GMB_TOK_BondPrice, fd_bondprice }, { GMB_TOK_BondYield, fd_bondyield },
    { GMB_TOK_PriceSimple, fd_pricesimple }, { GMB_TOK_BondEquivalentYield, fd_bondequivyield },
    { GMB_TOK_Price, fd_price },
};
int emit_bf_finance_scalar(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(FS_TBL) / sizeof(FS_TBL[0]); i++)
        if (FS_TBL[i].tk == tk) return FS_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: finance_cashflow (delegazione §0quater) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } fc_row;
static int fc_npv_gen(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 2) return 0;
    sb_put(&c->out, fn); sb_put(&c->out, "("); emit_expr(c, a[0]); sb_put(&c->out, ", ");
    if (n == 2 && a[1] && a[1]->kind == GMB_EXPR_VAR)
    {
        var_t* av = lookup_var(c, a[1]->var);
        if (av && av->is_array) { emit_arr_arg_len(c, a[1]); sb_put(&c->out, ")"); return 1; }
    }
    sb_put(&c->out, "(gmb_double_t[]){");
    for (int i = 1; i < n; i++) { if (i > 1) sb_put(&c->out, ", "); emit_expr(c, a[i]); }
    sb_printf(&c->out, "}, %d)", n - 1);
    return 1;
}
static int fc_npv(ctx_t* c, gmb_expr_t** a, int n) { return fc_npv_gen(c, a, n, "gmb_fin_npv"); }
static int fc_mnpv(ctx_t* c, gmb_expr_t** a, int n) { return fc_npv_gen(c, a, n, "gmb_fin_mnpv"); }
static int fc_irr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_fin_irr(");
    emit_arr_arg_len(c, a[0]);
    if (n >= 2) { sb_put(&c->out, ", "); emit_expr(c, a[1]); } else sb_put(&c->out, ", 0.1");
    sb_put(&c->out, ")");
    return 1;
}
static int fc_mirr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_fin_mirr(");
    emit_arr_arg_len(c, a[0]);
    sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ")");
    return 1;
}
static void fc_emit_len1(ctx_t* c, gmb_expr_t* a) {
    if (a && a->kind == GMB_EXPR_VAR)
    {
        var_t* av = lookup_var(c, a->var);
        if (av && av->is_array) { sb_put(&c->out, "(int)(("); emit_arr_len_of(c, av); sb_put(&c->out, ")+1)"); return; }
    }
    sb_put(&c->out, "1");
}
static int fc_xirr(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_fin_xirr(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", ");
    fc_emit_len1(c, a[0]);
    sb_put(&c->out, ", ");
    if (n >= 3) emit_expr(c, a[2]); else sb_put(&c->out, "0.1");
    sb_put(&c->out, ")");
    return 1;
}
static int fc_xnpv(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_fin_xnpv(");
    emit_expr(c, a[0]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]);
    sb_put(&c->out, ", "); fc_emit_len1(c, a[1]);
    sb_put(&c->out, ")");
    return 1;
}
static const fc_row FC_TBL[] = {
    { GMB_TOK_Npv, fc_npv }, { GMB_TOK_Xnpv, fc_xnpv }, { GMB_TOK_Mnpv, fc_mnpv },
    { GMB_TOK_Irr, fc_irr }, { GMB_TOK_Mirr, fc_mirr }, { GMB_TOK_Xirr, fc_xirr },
};
int emit_bf_finance_cashflow(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(FC_TBL) / sizeof(FC_TBL[0]); i++)
        if (FC_TBL[i].tk == tk) return FC_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: security (2026-09-02) — CRYPTOGRAPHY/STRINGS/AUTH/HTTP builtin 2026-09-02.
 * I nomi NON hanno GMB_TOK_ dedicato nel C (regola d'oro §12.2: dispatch per nome
 * dichiarato in tabella, MAI il TokenKind → il dispatcher li chiama via emit_builtin
 * quando tk/composite non risolve). Runtime = funzioni condivise del gmb_runtime_c. */
typedef int (*sec_fn)(ctx_t*, gmb_expr_t**, int);
typedef struct { const char* ns; const char* name; sec_fn fn; } sec_row;
/* stringa (1 arg stringa) → char* runtime → gmb_string_t */
static int se_cstr1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data))");
    return 1;
}
/* stringa (1 arg stringa) → encode di BYTE (uint8_t*, len) → gmb_string_t */
static int se_bytes1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s((const uint8_t*)", fn);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".length))");
    return 1;
}
/* HMACSHA256/384/512: gmb_hmac_sha*_hex(key, key_len, msg, msg_len) → hex (malloc) */
static int se_hmac(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 2) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s((const uint8_t*)", fn);
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[1]);
    sb_put(&c->out, ".length, (const uint8_t*)");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[0]);
    sb_put(&c->out, ".length))");
    return 1;
}
static int se_hmac256(ctx_t* c, gmb_expr_t** a, int n) { return se_hmac(c, a, n, "gmb_hmac_sha256_hex"); }
static int se_hmac384(ctx_t* c, gmb_expr_t** a, int n) { return se_hmac(c, a, n, "gmb_hmac_sha384_hex"); }
static int se_hmac512(ctx_t* c, gmb_expr_t** a, int n) { return se_hmac(c, a, n, "gmb_hmac_sha512_hex"); }
static int se_jwtcreate(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_jwt_create(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[1]);
    sb_put(&c->out, ".data))");
    return 1;
}
static int se_jwtpayload(ctx_t* c, gmb_expr_t** a, int n) { return se_cstr1(c, a, n, "gmb_jwt_payload"); }
static int se_jwtverify(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_jwt_verify(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[1]);
    sb_put(&c->out, ".data) ? -1 : 0)");
    return 1;
}
static int se_randint(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1 || n > 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_rand_int(");
    if (n == 2) emit_expr(c, a[0]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", "); emit_expr(c, a[n - 1]); sb_put(&c->out, ")");
    return 1;
}
static int se_randdouble(ctx_t* c, gmb_expr_t** a, int n) {
    if (n > 2) return 0;
    sb_put(&c->out, "gmb_rand_double(");
    if (n > 0) emit_expr(c, a[0]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n > 1) emit_expr(c, a[1]); else sb_put(&c->out, "1");
    sb_put(&c->out, ")");
    return 1;
}
static int se_randnormal(ctx_t* c, gmb_expr_t** a, int n) {
    if (n > 2) return 0;
    sb_put(&c->out, "gmb_rand_normal(");
    if (n > 0) emit_expr(c, a[0]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n > 1) emit_expr(c, a[1]); else sb_put(&c->out, "1");
    sb_put(&c->out, ")");
    return 1;
}
static int se_randbytes(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_rand_bytes_val((int)(");
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int se_randcryptobytes(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "gmb_rand_crypto_bytes_val((int)(");
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int se_pwhash(ctx_t* c, gmb_expr_t** a, int n) { return se_cstr1(c, a, n, "gmb_password_hash"); }
static int se_pwverify(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "(gmb_password_verify(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[1]);
    sb_put(&c->out, ".data) ? -1 : 0)");
    return 1;
}
static int se_safe_name(ctx_t* c, gmb_expr_t** a, int n) { return se_cstr1(c, a, n, "gmb_upload_safe_name"); }
/* DETECTCONTENTTYPE: sniff magic-number (uint8_t*, len) → const char* runtime → gmb_string_t
 * (stessa firma di gmb_upload_detect_type(data, len) in gmb_encoding.c:519). */
static int se_detect_type(ctx_t* c, gmb_expr_t** a, int n) { return se_bytes1(c, a, n, "gmb_upload_detect_type"); }
static int se_session(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_session_create(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_str_operand(c, a[1]);
    sb_put(&c->out, ".data))");
    return 1;
}
/* validazione: runtime int (0/1) → bool BASIC (-1/0) */
static int se_val1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "(%s(", fn);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data) ? -1 : 0)");
    return 1;
}
static int se_val_length(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 3) return 0;
    sb_put(&c->out, "(gmb_validation_length(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (int)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]);
    sb_put(&c->out, ")) ? -1 : 0)");
    return 1;
}
static int se_required(ctx_t* c, gmb_expr_t** a, int n) { return se_val1(c, a, n, "gmb_validation_required"); }
static int se_email(ctx_t* c, gmb_expr_t** a, int n) { return se_val1(c, a, n, "gmb_validation_email"); }
static int se_url(ctx_t* c, gmb_expr_t** a, int n) { return se_val1(c, a, n, "gmb_validation_url"); }
static int se_cc(ctx_t* c, gmb_expr_t** a, int n) { return se_val1(c, a, n, "gmb_validation_credit_card"); }
static int se_tob64url(ctx_t* c, gmb_expr_t** a, int n) { return se_bytes1(c, a, n, "gmb_base64url_encode"); }
static int se_html(ctx_t* c, gmb_expr_t** a, int n) { return se_cstr1(c, a, n, "gmb_html_encode"); }
static int se_urlenc(ctx_t* c, gmb_expr_t** a, int n) { return se_cstr1(c, a, n, "gmb_url_encode"); }
static int se_js(ctx_t* c, gmb_expr_t** a, int n) { return se_cstr1(c, a, n, "gmb_js_encode"); }
/* Voci token-first (chiave canonica GMB_TOK — regola d'oro) per le funzioni security
 * con emissione reale per nome in SEC_TBL: le rende contate dall'audit. Gli handler
 * se_* sono ma_sp-shaped e riusati. */
typedef struct { gmb_tokenkind_t tk; sec_fn fn; } sec_tok_row;
/* AESENCRYPT(data$, key$) / AESDECRYPT(enc$, key$) → byte[] (gmb_string_t). */
static int se_aesenc(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_aes_encrypt_val(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ")");
    return 1;
}
static int se_aesdec(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_aes_decrypt_val(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ")");
    return 1;
}
/* CHACHA20ENCRYPT/DECRYPT(data$, key, nonce) → byte[] (XOR simmetrico). */
static int se_chacha(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "gmb_chacha20_xor_val(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ")");
    return 1;
}
/* CHACHA20POLY1305ENCRYPT(pt, key, nonce, aad) → dict val; DECRYPT(ct, tag, key, nonce, aad)
 * → byte[] (ct/tag sono gmb_val_t liste, key/nonce/aad gmb_string_t). */
static int se_ccp1305enc(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 4) return 0;
    sb_put(&c->out, "gmb_chacha20poly1305_encrypt_sval(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[3]); sb_put(&c->out, ")");
    return 1;
}
/* Argomento Chacha20Poly1305: ct/tag sono gmb_val_t, key/nonce/aad gmb_string_t. Un
 * argomento di tipo ARRAY (double*, es. `DIM x = {0,0}`) va convertito: val → lista
 * di double; stringa → bytes (vuoto se non è una stringa). Senza, il C generato passava
 * un gmb_double_t* dove serve gmb_val_t/gmb_string_t (compile-error, gen_CHACHA*). */
static void ccp1305_arg(ctx_t* c, gmb_expr_t* x, int as_val) {
    var_t* av = (x && x->kind == GMB_EXPR_VAR && x->var) ? lookup_var(c, x->var) : NULL;
    if (av && (av->is_array || av->matrix || av->jagged || av->dyn_len)) {
        if (as_val) {
            sb_put(&c->out, "gmb_make_val_list(gmb_list_from_doubles(");
            emit_ident(c, av->name); sb_put(&c->out, ", (gmb_int_t)");
            emit_ident(c, av->name); sb_put(&c->out, "_len))");
        } else {
            sb_put(&c->out, "gmb_string_alloc(\"\")");
        }
    } else if (as_val) emit_expr(c, x);
    else emit_str_operand(c, x);
}
static int se_ccp1305dec(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 5) return 0;
    sb_put(&c->out, "gmb_chacha20poly1305_decrypt_sval(");
    ccp1305_arg(c, a[0], 1); sb_put(&c->out, ", ");
    ccp1305_arg(c, a[1], 1); sb_put(&c->out, ", ");
    ccp1305_arg(c, a[2], 0); sb_put(&c->out, ", ");
    ccp1305_arg(c, a[3], 0); sb_put(&c->out, ", ");
    ccp1305_arg(c, a[4], 0); sb_put(&c->out, ")");
    return 1;
}
/* HASHFILE(path, algo) → byte[]; STRHASHFILE(path, algo) → hex. */
static int se_hashfile(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_hashfile_val(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"SHA256\"");
    sb_put(&c->out, ")");
    return 1;
}
static int se_strhashfile(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_strhashfile_val(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"SHA256\"");
    sb_put(&c->out, ")");
    return 1;
}
/* ── asimmetrico (handle OpenSSL) ── */
static void sg_bytes(ctx_t* c, gmb_expr_t* e) {
    sb_put(&c->out, "(const uint8_t*)");
    emit_str_operand(c, e); sb_put(&c->out, ".data, (size_t)");
    emit_str_operand(c, e); sb_put(&c->out, ".length");
}
static int se_keygen(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_keys_generate_val(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (gmb_int_t)(");
    if (n >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "2048");
    sb_put(&c->out, "))");
    return 1;
}
static int se_keyexport(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_keys_export_val((gmb_int_t)(");
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int se_keyimport(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_keys_import_pem(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int se_sign(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_keys_sign_val((gmb_int_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), ");
    sg_bytes(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int se_verify(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_keys_verify_val((gmb_int_t)(");
    emit_expr(c, a[2]); sb_put(&c->out, "), ");
    sg_bytes(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]); sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static int se_verifyraw(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_keys_verify_raw_val(");
    sg_bytes(c, a[1]); sb_put(&c->out, ", ");
    emit_expr(c, a[0]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data) ? -1.0 : 0.0)");
    return 1;
}
static int se_encryptasym(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_keys_encrypt_val((gmb_int_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), ");
    sg_bytes(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int se_decryptasym(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_keys_decrypt_val((gmb_int_t)(");
    emit_expr(c, a[1]); sb_put(&c->out, "), ");
    sg_bytes(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static const sec_tok_row SEC_TOK_TBL[] = {
    { GMB_TOK_HmacSha256, se_hmac256 }, { GMB_TOK_HmacSha384, se_hmac384 },
    { GMB_TOK_HmacSha512, se_hmac512 }, { GMB_TOK_ToBase64Url, se_tob64url },
    { GMB_TOK_AesEncrypt, se_aesenc }, { GMB_TOK_AesDecrypt, se_aesdec },
    { GMB_TOK_ChaCha20Encrypt, se_chacha }, { GMB_TOK_ChaCha20Decrypt, se_chacha },
    { GMB_TOK_ChaCha20Poly1305Encrypt, se_ccp1305enc },
    { GMB_TOK_ChaCha20Poly1305Decrypt, se_ccp1305dec },
    { GMB_TOK_HashFile, se_hashfile }, { GMB_TOK_StrHashFile, se_strhashfile },
    { GMB_TOK_KeyGen, se_keygen }, { GMB_TOK_KeyExport, se_keyexport },
    { GMB_TOK_KeyImport, se_keyimport }, { GMB_TOK_Sign, se_sign },
    { GMB_TOK_Verify, se_verify }, { GMB_TOK_VerifyRaw, se_verifyraw },
    { GMB_TOK_EncryptAsym, se_encryptasym }, { GMB_TOK_DecryptAsym, se_decryptasym },
    { GMB_TOK_PasswordHash, se_pwhash }, { GMB_TOK_PasswordVerify, se_pwverify },
    { GMB_TOK_JwtCreate, se_jwtcreate }, { GMB_TOK_JwtVerify, se_jwtverify },
    { GMB_TOK_JwtPayload, se_jwtpayload },
    { GMB_TOK_RandomInt, se_randint }, { GMB_TOK_RandomDouble, se_randdouble },
    { GMB_TOK_RandomNormal, se_randnormal }, { GMB_TOK_RandomBytes, se_randbytes },
    { GMB_TOK_RandomCryptoBytes, se_randcryptobytes },
    { GMB_TOK_HtmlEncode, se_html }, { GMB_TOK_UrlEncode, se_urlenc },
    { GMB_TOK_JsEncode, se_js }, { GMB_TOK_IsRequired, se_required },
    { GMB_TOK_IsEmail, se_email }, { GMB_TOK_IsUrl, se_url },
    { GMB_TOK_IsCreditCard, se_cc }, { GMB_TOK_IsLength, se_val_length },
    { GMB_TOK_SafeFileName, se_safe_name }, { GMB_TOK_SessionCreate, se_session },
    { GMB_TOK_DetectContentType, se_detect_type },
};
int emit_bf_security_tok(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(SEC_TOK_TBL) / sizeof(SEC_TOK_TBL[0]); i++)
        if (SEC_TOK_TBL[i].tk == tk) return SEC_TOK_TBL[i].fn(c, args, nargs);
    return 0;
}
/* ── dominio: memory (2026-09-02) — MEMORY.STORE/LOAD/DELETE/LIST/SEARCH (gmb_ai.c engine) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } mem_row;
static int me_str1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n != 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data))");
    return 1;
}
static int me_store(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "((gmb_double_t)(gmb_mem_store(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { sb_put(&c->out, "(int)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ") ? -1.0 : 0.0))");
    return 1;
}
static int me_delete(ctx_t* c, gmb_expr_t** a, int n) {
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_double_t)(gmb_mem_delete(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data) ? -1.0 : 0.0))");
    return 1;
}
static int me_load(ctx_t* c, gmb_expr_t** a, int n) { return me_str1(c, a, n, "gmb_mem_load"); }
static int me_search(ctx_t* c, gmb_expr_t** a, int n) { return me_str1(c, a, n, "gmb_mem_search"); }
static int me_list(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_mem_list())");
    return 1;
}
static const mem_row MEM_TBL[] = {
    { GMB_TOK_MemoryStore, me_store }, { GMB_TOK_MemoryLoad, me_load },
    { GMB_TOK_MemoryDelete, me_delete }, { GMB_TOK_MemoryList, me_list },
    { GMB_TOK_MemorySearch, me_search },
};
int emit_bf_memory(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(MEM_TBL) / sizeof(MEM_TBL[0]); i++)
        if (MEM_TBL[i].tk == tk) return MEM_TBL[i].sp(c, args, nargs);
    return 0;
}

/* ============================================================================
 * dominio: convert (CONVERT/DOCUMENT — gmb_doc_c + gmb_runtime_c/src/gmb_md.c).
 * Riferimento semantico: C99CodeEmitter.Builtin.Convert.cs.
 * Stringhe (gmb_ns_types.h is_str=1) → gmb_string_alloc(char*); handle/int → (int);
 * AST markdown boxed → gmb_val_t (PARSE) oppure gmb_list_t via gmb_val_as_list
 * (HEADINGS/LINKS/MDIMAGES/TABLES/CODEBLOCKS/QUERY). Come la VM/C (gmb_bf_1250), se
 * l'argomento "doc" è una stringa viene parsato prima (gmb_md_parse).
 * ============================================================================ */
static int cv_is_md_list(gmb_tokenkind_t tk) {
    return tk == GMB_TOK_MdHeadings || tk == GMB_TOK_MdLinks || tk == GMB_TOK_MdImages
        || tk == GMB_TOK_MdTables || tk == GMB_TOK_MdCodeBlocks || tk == GMB_TOK_MdQuery;
}
/* argomento "doc" dell'AST markdown: se è una STRINGA la parsa (parity VM/C gmb_bf_1250). */
static void cv_md_doc(ctx_t* c, gmb_expr_t* a) {
    if (a && etype(c, a) == TY_STR) {
        sb_put(&c->out, "gmb_md_parse(");
        emit_str_operand(c, a);
        sb_put(&c->out, ".data)");
    } else {
        emit_expr(c, a);
    }
}
/* slot intero: i letterali stringa (probe) valgono 0; le variabili stringa sono convertite. */
static void cv_int(ctx_t* c, gmb_expr_t* a, const char* dflt) {
    if (!a) { sb_put(&c->out, dflt); return; }
    if (a->kind == GMB_EXPR_STR) { sb_put(&c->out, "0"); return; }
    if (etype(c, a) == TY_STR) {
        sb_put(&c->out, "(int)(gmb_string_to_double(");
        emit_str_operand(c, a);
        sb_put(&c->out, "))");
        return;
    }
    sb_put(&c->out, "(int)("); emit_expr(c, a); sb_put(&c->out, ")");
}
static void cv_str_arg(ctx_t* c, gmb_expr_t* a) {
    emit_str_operand(c, a); sb_put(&c->out, ".data");
}
/* EXTRACT/EXTRACTENGINE: >=2 arg = scrittura su file (extract_to), 1 arg = testo. */
static int cv_extract(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (n >= 2) {
        sb_put(&c->out, "(gmb_doc_extract_to(");
        cv_str_arg(c, a[0]); sb_put(&c->out, ", "); cv_str_arg(c, a[1]); sb_put(&c->out, "), 0.0)");
    } else {
        sb_put(&c->out, "gmb_string_alloc(gmb_doc_extract(");
        cv_str_arg(c, a[0]); sb_put(&c->out, "))");
    }
    return 1;
}
static int cv_extract_engine(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    if (n >= 2) {
        sb_put(&c->out, "(gmb_doc_extract_to(");
        cv_str_arg(c, a[0]); sb_put(&c->out, ", "); cv_str_arg(c, a[1]); sb_put(&c->out, "), 0.0)");
    } else {
        sb_put(&c->out, "gmb_string_alloc(gmb_doc_extract_engine(");
        cv_str_arg(c, a[0]); sb_put(&c->out, "))");
    }
    return 1;
}
/* RENDER(path, dest): scrittura su file. */
static int cv_render(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_doc_render_to(");
    cv_str_arg(c, a[0]); sb_put(&c->out, ", "); cv_str_arg(c, a[1]); sb_put(&c->out, "), 0.0)");
    return 1;
}
/* PARSE(md$): AST boxed gmb_val_t. */
static int cv_parse(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_md_parse(");
    cv_str_arg(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int cv_headings(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_val_as_list(gmb_md_headings(");
    cv_md_doc(c, a[0]); sb_put(&c->out, ", ");
    cv_int(c, n >= 2 ? a[1] : NULL, "0");
    sb_put(&c->out, "))");
    return 1;
}
static int cv_md_list1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_val_as_list(%s(", fn);
    cv_md_doc(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int cv_links(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_list1(c, a, n, "gmb_md_links"); }
static int cv_mdimages(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_list1(c, a, n, "gmb_md_images"); }
static int cv_tables(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_list1(c, a, n, "gmb_md_tables"); }
static int cv_codeblocks(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_list1(c, a, n, "gmb_md_codeblocks"); }
static int cv_query(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_val_as_list(gmb_md_query(");
    cv_md_doc(c, a[0]); sb_put(&c->out, ", "); cv_str_arg(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
/* TREE/TOHTML/TOTEXT/FORMAT(doc): doc boxed → stringa allocata. */
static int cv_md_str1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    cv_md_doc(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int cv_tree(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_str1(c, a, n, "gmb_md_tree"); }
static int cvd_md_tohtml(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_str1(c, a, n, "gmb_md_to_html"); }
static int cvd_md_totext(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_str1(c, a, n, "gmb_md_to_text"); }
static int cv_format(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_str1(c, a, n, "gmb_md_format"); }
/* VALIDATE/STRIP prendono direttamente il testo (const char*), non un AST doc. */
static int cv_md_text1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    cv_str_arg(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int cv_validate(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_text1(c, a, n, "gmb_md_validate"); }
static int cv_strip(ctx_t* c, gmb_expr_t** a, int n) { return cv_md_text1(c, a, n, "gmb_md_strip"); }
/* OPEN(path) → handle int; CLOSE/PAGECOUNT handle int. */
static int cv_open(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_doc_open("); cv_str_arg(c, a[0]); sb_put(&c->out, ")");
    return 1;
}
static int cv_close(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_doc_close("); cv_int(c, a[0], "0"); sb_put(&c->out, "), 0.0)");
    return 1;
}
static int cv_pagecount(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_doc_page_count("); cv_int(c, a[0], "0"); sb_put(&c->out, ")");
    return 1;
}
/* TEXT/MARKDOWN/HTML(handle[, page]): estrazione formato da PDF aperto. */
static int cv_doc_extract(ctx_t* c, gmb_expr_t** a, int n, const char* fn) {
    if (n < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    cv_int(c, a[0], "0"); sb_put(&c->out, ", ");
    cv_int(c, n >= 2 ? a[1] : NULL, "-1");
    sb_put(&c->out, "))");
    return 1;
}
static int cv_text(ctx_t* c, gmb_expr_t** a, int n) { return cv_doc_extract(c, a, n, "gmb_doc_extract_text"); }
static int cv_markdown(ctx_t* c, gmb_expr_t** a, int n) { return cv_doc_extract(c, a, n, "gmb_doc_extract_markdown"); }
static int cv_html(ctx_t* c, gmb_expr_t** a, int n) { return cv_doc_extract(c, a, n, "gmb_doc_extract_html"); }
static int cv_metadata(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_doc_metadata("); cv_int(c, a[0], "0"); sb_put(&c->out, "))");
    return 1;
}
static int cv_search(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_doc_search(");
    cv_int(c, a[0], "0"); sb_put(&c->out, ", "); cv_str_arg(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
/* IMAGES(handle, page, out_dir): estrae le immagini → numero file. */
static int cv_images(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_doc_extract_images(");
    cv_int(c, a[0], "0"); sb_put(&c->out, ", ");
    cv_int(c, n >= 3 ? a[1] : NULL, "0"); sb_put(&c->out, ", ");
    if (n >= 2) cv_str_arg(c, a[n - 1]); else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ")");
    return 1;
}
/* TOMD/TOHTML/TOTEXT(path): conversione di formato → stringa. */
static int cv_convert(ctx_t* c, gmb_expr_t** a, int n, const char* fmt) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_doc_convert(");
    cv_str_arg(c, a[0]);
    sb_printf(&c->out, ", \"%s\"))", fmt);
    return 1;
}
static int cv_tomd(ctx_t* c, gmb_expr_t** a, int n) { return cv_convert(c, a, n, "md"); }
static int cvd_conv_html(ctx_t* c, gmb_expr_t** a, int n) { return cv_convert(c, a, n, "html"); }
static int cvd_conv_text(ctx_t* c, gmb_expr_t** a, int n) { return cv_convert(c, a, n, "text"); }
static int cv_web(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_doc_web(");
    cv_str_arg(c, a[0]); sb_put(&c->out, ", \"md\"))");
    return 1;
}
/* BATCH(dir[, fmt][, recursive]): converte la cartella. */
static int cv_batch(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_doc_batch(");
    cv_str_arg(c, a[0]); sb_put(&c->out, ", ");
    if (n >= 2) cv_str_arg(c, a[1]); else sb_put(&c->out, "\"md\"");
    sb_put(&c->out, ", ");
    cv_int(c, n >= 3 ? a[2] : NULL, "0");
    sb_put(&c->out, "))");
    return 1;
}
static int cv_formats(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_string_alloc(gmb_doc_formats_s())");
    return 1;
}
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } cvd_row;
static const cvd_row CVD_TBL[] = {
    { GMB_TOK_CvtBatch, cv_batch },
    { GMB_TOK_DocClose, cv_close },
    { GMB_TOK_MdCodeBlocks, cv_codeblocks },
    { GMB_TOK_DocExtract, cv_extract },
    { GMB_TOK_DocExtractEngine, cv_extract_engine },
    { GMB_TOK_MdFormat, cv_format },
    { GMB_TOK_ConvFormats, cv_formats },
    { GMB_TOK_MdHeadings, cv_headings },
    { GMB_TOK_DocHtml, cv_html },
    { GMB_TOK_DocImages, cv_images },
    { GMB_TOK_MdLinks, cv_links },
    { GMB_TOK_DocMarkdown, cv_markdown },
    { GMB_TOK_MdImages, cv_mdimages },
    { GMB_TOK_DocMetadata, cv_metadata },
    { GMB_TOK_DocOpen, cv_open },
    { GMB_TOK_DocPageCount, cv_pagecount },
    { GMB_TOK_MdParse, cv_parse },
    { GMB_TOK_MdQuery, cv_query },
    { GMB_TOK_DocRender, cv_render },
    { GMB_TOK_MdToHtml, cvd_md_tohtml },
    { GMB_TOK_MdToText, cvd_md_totext },
    { GMB_TOK_DocSearch, cv_search },
    { GMB_TOK_MdStrip, cv_strip },
    { GMB_TOK_MdTables, cv_tables },
    { GMB_TOK_DocText, cv_text },
    { GMB_TOK_ConvHtml, cvd_conv_html },
    { GMB_TOK_ConvDoc, cv_tomd },
    { GMB_TOK_ConvText, cvd_conv_text },
    { GMB_TOK_MdTree, cv_tree },
    { GMB_TOK_MdValidate, cv_validate },
    { GMB_TOK_WebToMd, cv_web },
};
int emit_bf_convert(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    c->uses_doc = 1; c->uses_md = 1;
    for (size_t i = 0; i < sizeof(CVD_TBL) / sizeof(CVD_TBL[0]); i++)
        if (CVD_TBL[i].tk == tk) return CVD_TBL[i].sp(c, args, nargs);
    return 0;
}


/* ============================================================================
 * dominio: arrays (AREDUCE/ACOUNT/ASCAN/ABINARYSEARCH — forme SCALARI; le forme
 * array-returning AMAP/AFILTER/AREVERSE/ACLONE/AUNIQUE sono gestite nel percorso
 * statement DIM/assegnazione, gmb_c99_emit.c emit_dim_arr_init). Parita IL
 * (common/IlBackendRuntime/ArrayHelpers.cs): 0-based, -1 not-found, callback ABI
 * come gmb_array.c. Source array PRIMITIVI (gmb_double_t* oppure gmb_string_t*).
 * ============================================================================ */
static int ar_src_var(ctx_t* c, gmb_expr_t* e, int* is_str, int line) {
    *is_str = 0;
    if (!e || e->kind != GMB_EXPR_VAR || !e->var) {
        c99_fail(c, line, "ARRAYS: il primo argomento deve essere una variabile array");
        return -1;
    }
    var_t* av = lookup_var(c, e->var);
    if (!av || !av->is_array || av->matrix) {
        c99_fail(c, line, "ARRAYS: '%s' non e' un array 1-D", e->var ? e->var : "?");
        return -1;
    }
    if (av->vclass[0]) {
        c99_fail(c, line, "ARRAYS su array di %s non supportato in questo contesto (UDT richiede il percorso dedicato)", av->vclass);
        return -1;
    }
    *is_str = (av->type == TY_STR);
    return 0;
}
/* emette l'argomento sorgente: (const gmb_<t>*)arr, (gmb_int_t)(len) */
static void ar_emit_src(ctx_t* c, var_t* av, int is_str) {
    sb_put(&c->out, is_str ? "(const gmb_string_t*)" : "(const gmb_double_t*)");
    emit_var_ref(c, av);
    sb_put(&c->out, ", (gmb_int_t)(");
    emit_arr_len_of(c, av);
    sb_put(&c->out, ")");
}
static int ar_reduce(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2 || n > 3) return 0;
    /* AREDUCE su array di UDT (gmb_val_t* boxed): accumulatore numerico + elemento boxed
     * — variante del runtime condiviso. ABI lambda gmb_double_t(*)(void*, gmb_double_t,
     * gmb_val_t) (parità IL/PCode/VM-C, accumulatore double). */
    if (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        var_t* uv = lookup_var(c, a[0]->var);
        if (uv && uv->is_array && !uv->matrix && uv->vclass[0]) {
            sb_put(&c->out, "gmb_array_reduce_val((const gmb_val_t*)");
            emit_var_ref(c, uv);
            sb_put(&c->out, ", (gmb_int_t)(");
            emit_arr_len_of(c, uv);
            sb_put(&c->out, "), ");
            emit_expr(c, a[1]);
            if (n >= 3) { sb_put(&c->out, ", 1, "); emit_expr(c, a[2]); }
            else { sb_put(&c->out, ", 0, 0.0"); }
            sb_put(&c->out, ")");
            return 1;
        }
    }
    int is_str = 0;
    if (ar_src_var(c, a[0], &is_str, line)) return 1;
    if (is_str) { c99_fail(c, line, "AREDUCE su array STRING non supportato"); return 1; }
    var_t* av = lookup_var(c, a[0]->var);
    sb_put(&c->out, "gmb_array_reduce(");
    ar_emit_src(c, av, 0);
    sb_put(&c->out, ", ");
    emit_expr(c, a[1]);                    /* callback (gmb_fn_t) */
    if (n >= 3) { sb_put(&c->out, ", 1, "); emit_expr(c, a[2]); }
    else { sb_put(&c->out, ", 0, 0.0"); }
    sb_put(&c->out, ")");
    return 1;
}
static int ar_count(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 2) return 0;
    /* ACOUNT su array di UDT (gmb_val_t* boxed): predicato val → variante boxed. */
    if (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        var_t* uv = lookup_var(c, a[0]->var);
        if (uv && uv->is_array && !uv->matrix && uv->vclass[0]) {
            sb_put(&c->out, "((gmb_double_t)gmb_array_count_val((const gmb_val_t*)");
            emit_var_ref(c, uv);
            sb_put(&c->out, ", (gmb_int_t)(");
            emit_arr_len_of(c, uv);
            sb_put(&c->out, "), ");
            emit_expr(c, a[1]);
            sb_put(&c->out, "))");
            return 1;
        }
    }
    int is_str = 0;
    if (ar_src_var(c, a[0], &is_str, line)) return 1;
    if (is_str) { c99_fail(c, line, "ACOUNT su array STRING non supportato"); return 1; }
    var_t* av = lookup_var(c, a[0]->var);
    sb_put(&c->out, "((gmb_double_t)gmb_array_count(");
    ar_emit_src(c, av, 0);
    sb_put(&c->out, ", ");
    emit_expr(c, a[1]);
    sb_put(&c->out, "))");
    return 1;
}
static int ar_binary_search(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 2) return 0;
    int is_str = 0;
    if (ar_src_var(c, a[0], &is_str, line)) return 1;
    if (is_str) { c99_fail(c, line, "ABINARYSEARCH su array STRING non supportato"); return 1; }
    var_t* av = lookup_var(c, a[0]->var);
    sb_put(&c->out, "((gmb_double_t)gmb_array_binary_search(");
    ar_emit_src(c, av, 0);
    sb_put(&c->out, ", ");
    emit_expr(c, a[1]);
    sb_put(&c->out, "))");
    return 1;
}
/* ASCAN: 2-3 argomenti. Forma valore (array, value[, start]) o predicato
 * (array, function[, start]) — scelta a compile-time come l'IL (IsCallbackArg). */
static int ar_scan(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2 || n > 3) return 0;
    /* ASCAN su array di UDT (gmb_val_t* boxed): ar_src_var rifiuterebbe la sorgente
     * ("UDT richiede il percorso dedicato") → variante boxed del runtime condiviso.
     * ABI lambda gmb_double_t(*)(void*, gmb_val_t), come ASORT con comparatore su UDT. */
    if (a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
        var_t* uv = lookup_var(c, a[0]->var);
        if (uv && uv->is_array && !uv->matrix && uv->vclass[0]) {
            if (!gmb_expr_is_fn_value(c, a[1])) {
                c99_fail(c, line, "ASCAN su array di %s richiede un predicato", uv->vclass);
                return 1;
            }
            sb_put(&c->out, "((gmb_double_t)gmb_array_scan_pred_val((const gmb_val_t*)");
            emit_var_ref(c, uv);
            sb_put(&c->out, ", (gmb_int_t)(");
            emit_arr_len_of(c, uv);
            sb_put(&c->out, "), ");
            emit_expr(c, a[1]);
            sb_put(&c->out, ", ");
            if (n >= 3) { sb_put(&c->out, "(gmb_int_t)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
            else sb_put(&c->out, "0");
            sb_put(&c->out, "))");
            return 1;
        }
    }
    int is_str = 0;
    if (ar_src_var(c, a[0], &is_str, line)) return 1;
    gmb_expr_t* a1 = a[1];
    int predicate = gmb_expr_is_fn_value(c, a1);
    var_t* av = lookup_var(c, a[0]->var);
    if (predicate) {
        sb_put(&c->out, "((gmb_double_t)");
        sb_put(&c->out, is_str ? "gmb_array_scan_pred_str(" : "gmb_array_scan_pred(");
        ar_emit_src(c, av, is_str ? 1 : 0);
        sb_put(&c->out, ", ");
        emit_expr(c, a1);
        sb_put(&c->out, ", ");
        if (n >= 3) { sb_put(&c->out, "(gmb_int_t)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "0");
        sb_put(&c->out, "))");
    } else if (is_str) {
        sb_put(&c->out, "((gmb_double_t)gmb_array_scan_value_str(");
        ar_emit_src(c, av, 1);
        sb_put(&c->out, ", ");
        emit_str_operand(c, a1);
        sb_put(&c->out, ".data, ");
        if (n >= 3) { sb_put(&c->out, "(gmb_int_t)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "0");
        sb_put(&c->out, "))");
    } else {
        sb_put(&c->out, "((gmb_double_t)gmb_array_scan_value(");
        ar_emit_src(c, av, 0);
        sb_put(&c->out, ", ");
        emit_expr(c, a1);
        sb_put(&c->out, ", ");
        if (n >= 3) { sb_put(&c->out, "(gmb_int_t)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "0");
        sb_put(&c->out, "))");
    }
    return 1;
}
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } ar_scan_row;
static const ar_scan_row AR_SCAN_TBL[] = {
    { GMB_TOK_AReduce,       ar_reduce },
    { GMB_TOK_ACount,        ar_count },
    { GMB_TOK_AScan,         ar_scan },
    { GMB_TOK_ABinarySearch, ar_binary_search },
};
int emit_bf_arrays(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns;
    for (size_t i = 0; i < sizeof(AR_SCAN_TBL) / sizeof(AR_SCAN_TBL[0]); i++)
        if (AR_SCAN_TBL[i].tk == tk) return AR_SCAN_TBL[i].sp(c, args, nargs, line);
    return 0;
}

/* ── dominio: AI AGENT (funzioni AI.AGENTGET/AGENTASKSYNC) ──
 * Tabella lookup per TokenKind (regola d'oro §6.6): i builtin agent che ritornano stringa
 * emettono gmb_agent_get/gmb_agent_ask del runtime condiviso. Parita:
 *   IlRuntime.AiAgentGet/AiAgentAsk → AgentServer.Get/Ask. */
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } aiag_row;
static int aiag_get(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.AGENTGET: attesi (name, key)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_get(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.AGENTSTATUS: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_status("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aiag_events(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    /* Parità IL: AI.AGENTEVENTS ritorna un ARRAY di stringhe → reso `[]` / `[a, b]`
     * (non la stringa vuota di gmb_agent_events_str per lista vuota). §0bis: gmb_list_join_bracketed
     * è l'helper condiviso del formato. */
    /* string[] (C#) → gmb_list_t (gmb_agent_events_names); il display lo fa PRINT
     * (gmb_print_val(gmb_make_val_list(...)) → "[a, b]"/"[]"). Prima forzava la stringa
     * bracketed, che però impediva `DIM ev() = AI.AGENTEVENTS()` + UBOUND (AgentOperatingCenter). */
    sb_put(&c->out, "gmb_agent_events_names()"); return 1;
}
static int aiag_ask(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.AGENTASKSYNC: attesi (name, prompt)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_ask(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
/* AI.AGENTRUN(name, goal[, rounds]) — bug 2026-09-08: mancava del tutto nel mondo C (l'emitter
 * non conosceva GMB_TOK_AgentRunCua, il chiamante cadeva sul fallback numerico -> type mismatch
 * su R$ = AI.AGENTRUN(...)). gmb_agent_run_named (gmb_agent_loop.c) fa il loop Brain->Azione
 * sull'agente named, stesso runtime di AGENT.RUN (che non ha nome). */
static int aiag_run(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.AGENTRUN: attesi (name, goal[, rounds])"); return 1; }
    /* gmb_agent_run_named è dichiarata in gmb_agent_loop.h (gmb_agent_runtime): senza l'include
     * il C generato non compila ("call to undeclared function"). Stesso pattern di uses_doc. */
    c->uses_agent_loop = 1;
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_run_named(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { sb_put(&c->out, "(int)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "5");   /* parità IL: AI.AGENTRUN default rounds=5 */
    sb_put(&c->out, "))");
    return 1;
}
/* AI.AGENTCODE/FIX: CodeGen loop nativo (Fase 2/3) — emette il wrapper gmb_agent_code/fix.
   Args: task[, outPath]. Ritorno gmb_string_t (output del run o "Error:"). */
static void aiag_cg_emit(ctx_t* c, const char* fn, gmb_expr_t** a, int n, int line, int min) {
    if (n < min) { c99_fail(c, line, "AI.AGENTCODE: atteso (task[, outPath])"); return; }
    sb_put(&c->out, fn);
    sb_put(&c->out, "(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data");
    if (n >= 2) { sb_put(&c->out, ", "); emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, ", NULL");
    sb_put(&c->out, ")");
}
static int aiag_code(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_cg_emit(c, "gmb_agent_code", a, n, line, 1); return 1; }
static int aiag_fix(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_cg_emit(c, "gmb_agent_fix", a, n, line, 1); return 1; }
/* AI.BLACKBOARDWRITE(key, value) → 1/0 (parita' Basic.Agents.Blackboard.Write C#). */
static int aiag_bb_write(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.BLACKBOARDWRITE: attesi (key, value)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_blackboard_write(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
/* AI.BLACKBOARDREAD(key) → stringa ("" se assente). */
static int aiag_bb_read(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.BLACKBOARDREAD: attesa (key)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_blackboard_read(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
/* AI.BLACKBOARDWAIT(key[, timeoutMs]) → stringa (valore o "" su timeout).
 * FASE 2.6(w): async park-based (parity FETCH, FASE 2.6p, e gmb_key_wait_park in
 * gmb_c99gpm.h) — gmb_blackboard_wait e' bloccante e documentata "non chiamare da un
 * worker GPM"; qui la wait vera gira su un pthread dedicato mentre la goroutine chiamante
 * si parcheggia in poll con gmb_yield_ticks(20) sullo stato del future (mai gmb_future_await
 * diretto: quella funzione arma un blocco SENZA sleep_until_ms, e per attese >~100ms senza
 * altri task runnable lo scheduler la scambia per un deadlock reale — falso positivo
 * "all goroutines are asleep", osservato con timeout 200ms nel MiniTest dedicato. Il poll
 * con gmb_yield_ticks arma invece sleep_until_ms ad ogni giro: lo scheduler vede sempre un
 * timer futuro e non dichiara mai stuck. Il campo gmb_future.state e' intenzionalmente non
 * opaco (vedi gmb_c99gpm.h) proprio per questo tipo di lettura dal codice emesso). */
static int aiag_bb_wait(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.BLACKBOARDWAIT: attesa (key[, timeoutMs])"); return 1; }
    c->uses_bb_wait = 1;
    /* NB: non forza c->uses_gpm=1 qui (a differenza di FETCH): se il programma non ha ALTRA
     * concorrenza (nessun GO SUB/SEND/RECEIVE/...), gpm_scan() non attiva lo scheduler GPM e
     * gmb_cur() sara' sempre NULL a runtime → il poll sotto usa il ramo gmb_sleep_ms (blocco
     * sincrono innocuo, non essendoci altri task da congelare — identico al comportamento
     * pre-fix). Forzare uses_gpm=1 qui, come FETCH, riattiva gmb_gpm_start anche per programmi
     * senza GO SUB e in casi con una SUB Main sia auto-invocata sia richiamata esplicitamente
     * a programma (es. minitests/misc/swarm_domain.bas) fa GIRARE gmb_user_Main() DUE VOLTE
     * (una sincrona pre-scheduler, una come task): con AI.AGENTSWARM/BLACKBOARDWAIT che hanno
     * side-effect osservabili (stato agenti) questo rompe l'EXPECT del MiniTest. Se il
     * programma ha davvero GO SUB altrove, gpm_scan() mette gia' uses_gpm=1 da solo e il ramo
     * gmb_yield_ticks (park cooperativo) scatta correttamente dentro quel task. */
    sb_put(&c->out, "({ gmb_bb_wait_th_args* _ba = (gmb_bb_wait_th_args*)calloc(1, sizeof(gmb_bb_wait_th_args)); _ba->key = strdup((");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ").data); _ba->timeout_ms = ");
    if (n >= 2) { sb_put(&c->out, "(int)("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "10000");
    /* gmb_cur() != NULL SOLO dentro un task GPM: alcuni programmi con uses_gpm=1 eseguono
     * comunque tratti di codice top-level FUORI da un task (es. una SUB Main richiamata sia
     * esplicitamente a livello di programma sia come entry-point automatico: il primo giro
     * gira prima di gmb_gpm_start, senza scheduler attivo). gmb_yield_ticks/gmb_cur() li
     * dentro andrebbero in NULL-deref. In quel caso fuori-task non c'e' nessun altro task da
     * congelare: un poll con sleep OS piatto e' innocuo ed equivalente al comportamento
     * bloccante di prima del fix (osservato con minitests/misc/swarm_domain.bas). */
    sb_put(&c->out, "; gmb_future_t* _bf = gmb_future_new(); _ba->fut = _bf; gmb_thread_t _bt; GMB_THREAD_CREATE(&_bt, gmb_bb_wait_th, _ba); GMB_THREAD_DETACH(&_bt); while (_bf->state == 0) { if (gmb_cur()) gmb_yield_ticks(20); else gmb_sleep_ms(5); } gmb_val_t _bres = _bf->value; free(_ba->key); free(_ba); _bres.v_string; })");
    return 1;
}
/* AI.AGENTSWARM(n, prefix, task[, maxConcurrency]) → stringa join '\n'.
 * FASE 2.6(w): async park-based (parity FETCH, FASE 2.6p) — N gmb_agent_ask sequenziali sono
 * bloccanti; qui girano su un pthread dedicato mentre la goroutine chiamante si parcheggia in
 * poll (gmb_yield_ticks(20) sullo stato del future — vedi motivazione sopra su aiag_bb_wait:
 * gmb_future_await diretto rischia il falso deadlock per attese lunghe senza altri task). */
static int aiag_swarm(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.AGENTSWARM: attesi (n, prefix, task[, maxConcurrency])"); return 1; }
    c->uses_agent_swarm = 1;
    /* NB: non forza c->uses_gpm=1 (vedi motivazione in aiag_bb_wait — swarm_domain.bas). */
    sb_put(&c->out, "({ gmb_agent_swarm_th_args* _sa = (gmb_agent_swarm_th_args*)calloc(1, sizeof(gmb_agent_swarm_th_args)); _sa->n = (double)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "); _sa->prefix = strdup((");
    emit_str_operand(c, a[1]);
    sb_put(&c->out, ").data); _sa->task = strdup((");
    emit_str_operand(c, a[2]);
    sb_put(&c->out, ").data); _sa->cc = ");
    if (n >= 4) { sb_put(&c->out, "(int)("); emit_expr(c, a[3]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "8");
    /* gmb_cur()==NULL fuori da un task GPM (vedi motivazione in aiag_bb_wait): fallback a
     * poll con sleep OS piatto, innocuo perche' fuori-task non c'e' nessun altro task GPM
     * da congelare (osservato con minitests/misc/swarm_domain.bas: SUB Main richiamata sia
     * esplicitamente sia come entry-point automatico gira anche prima di gmb_gpm_start). */
    sb_put(&c->out, "; gmb_future_t* _sf = gmb_future_new(); _sa->fut = _sf; gmb_thread_t _st; GMB_THREAD_CREATE(&_st, gmb_agent_swarm_th, _sa); GMB_THREAD_DETACH(&_st); while (_sf->state == 0) { if (gmb_cur()) gmb_yield_ticks(20); else gmb_sleep_ms(5); } gmb_val_t _sres = _sf->value; free(_sa->prefix); free(_sa->task); free(_sa); _sres.v_string; })");
    return 1;
}
/* Orchestrazione agenti (A1): categorie di task → provider/modello (gmb_category.c). */
static int aiag_cat_set(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.CATEGORYSET: attesi (category, provider)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_category_set(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_cat_fallback(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.CATEGORYFALLBACK: attesi (category, provider)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_category_fallback(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_cat_auto(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.CATEGORYAUTO: atteso (flag)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_category_auto((int)(");
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
static int aiag_cat_pick(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.CATEGORYPICK: attesa (category)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_category_pick(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_cat_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_category_list())");
    return 1;
}
/* Orchestrazione agenti (B1): Task Graph con dipendenze (gmb_taskgraph.c). */
static void aiag_task_2str(ctx_t* c, gmb_expr_t** a, int n, int line, const char* fn) {
    if (n < 2) { c99_fail(c, line, "AI.TASK*: attesi (id, valore)"); return; }
    sb_put(&c->out, "(gmb_double_t)"); sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
}
static int aiag_task_define(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_task_2str(c, a, n, line, "gmb_task_define"); return 1; }
static int aiag_task_depends(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_task_2str(c, a, n, line, "gmb_task_depends"); return 1; }
static int aiag_task_assign(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_task_2str(c, a, n, line, "gmb_task_assign"); return 1; }
static int aiag_task_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.TASKSTATUS: atteso (id)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_task_status(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_task_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_task_list())");
    return 1;
}
static int aiag_task_run(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_task_run())");
    return 1;
}
/* Orchestrazione agenti (A2): Planner — piani con approvazione (gmb_plan.c). */
static int aiag_plan_define(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.PLANDEFINE: attesi (name, goal)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_plan_define(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_plan_add(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.PLANADD: attesi (name, taskId)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_plan_add(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_plan_approve(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.PLANAPPROVE: atteso (name)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_plan_approve(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_plan_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.PLANSTATUS: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_plan_status(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_plan_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_plan_list())");
    return 1;
}
static int aiag_plan_run(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.PLANRUN: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_plan_run(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
/* Orchestrazione agenti (A3): lifecycle hooks (gmb_hook.c). */
static void aiag_hook_2str(ctx_t* c, gmb_expr_t** a, int n, int line, const char* fn) {
    if (n < 2) { c99_fail(c, line, "AI.HOOK*: attesi (event, handler)"); return; }
    sb_put(&c->out, "(gmb_double_t)"); sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
}
static int aiag_hook_register(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_hook_2str(c, a, n, line, "gmb_hook_register"); return 1; }
static int aiag_hook_unregister(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_hook_2str(c, a, n, line, "gmb_hook_unregister"); return 1; }
static int aiag_hook_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_hook_list())");
    return 1;
}
static int aiag_hook_events(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_hook_events())");
    return 1;
}
/* Orchestrazione agenti (A6): Artifact Store (gmb_artifact.c). */
static void aiag_art_2str(ctx_t* c, gmb_expr_t** a, int n, int line, const char* fn) {
    if (n < 2) { c99_fail(c, line, "AI.ARTIFACT*: attesi (name, ref)"); return; }
    sb_put(&c->out, "(gmb_double_t)"); sb_put(&c->out, fn); sb_put(&c->out, "(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
}
static int aiag_art_put(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_art_2str(c, a, n, line, "gmb_artifact_put"); return 1; }
static int aiag_art_link(ctx_t* c, gmb_expr_t** a, int n, int line) { aiag_art_2str(c, a, n, line, "gmb_artifact_link"); return 1; }
static int aiag_art_get(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.ARTIFACTGET: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_artifact_get(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_art_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_artifact_list())");
    return 1;
}
static int aiag_art_remove(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.ARTIFACTREMOVE: atteso (name)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_artifact_remove(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
/* Orchestrazione agenti (A7): Resource Manager (gmb_resource.c). */
static int aiag_res_set(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.RESOURCESET: attesi (key, value)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_resource_set(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_res_get(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RESOURCEGET: atteso (key)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_resource_get(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_res_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_resource_list())");
    return 1;
}
static int aiag_res_available(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RESOURCEAVAILABLE: atteso (key)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_resource_available(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_res_swarmsize(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RESOURCESWARMSIZE: atteso (n)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_resource_swarm_size((double)(");
    emit_expr(c, a[0]); sb_put(&c->out, "))");
    return 1;
}
/* Orchestrazione agenti (A9): Handoff (gmb_handoff.c). */
static int aiag_handoff(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.HANDOFF: attesi (taskId, toAgent)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_handoff_set(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_handoff_complete(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.HANDOFFCOMPLETE: attesi (taskId, result)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_handoff_complete(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_handoff_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.HANDOFFSTATUS: atteso (taskId)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_handoff_status(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_handoff_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_handoff_list())");
    return 1;
}
static int aiag_handoff_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_handoff_clear()");
    return 1;
}
/* Orchestrazione agenti (A5): Rules injection gerarchica (gmb_rules.c). */
static int aiag_rules_add(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.RULESADD: attesi (scope, content)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_rules_add(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_rules_for(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RULESFOR: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_rules_for(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_rules_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_rules_list())");
    return 1;
}
static int aiag_rules_remove(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RULESREMOVE: atteso (scope)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_rules_remove(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_rules_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_rules_clear()");
    return 1;
}
/* Orchestrazione agenti (A4): Goal + idle continuation (gmb_goal.c). */
static int aiag_goal_set(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.GOALSET: atteso (objective)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_goal_set(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_goal_get(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_goal_get())");
    return 1;
}
static int aiag_goal_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_goal_status())");
    return 1;
}
static int aiag_goal_tick(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_goal_tick()");
    return 1;
}
static int aiag_goal_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_goal_clear()");
    return 1;
}
/* Orchestrazione agenti (A8): Session Replay (gmb_replay.c). */
static int aiag_replay_start(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.REPLAYSTART: atteso (sessionId)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_replay_start(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_replay_record(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.REPLAYRECORD: atteso (event)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_replay_record(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_replay_stop(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_replay_stop()");
    return 1;
}
static int aiag_replay_get(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.REPLAYGET: atteso (sessionId)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_replay_get(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_replay_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_replay_list())");
    return 1;
}
static int aiag_replay_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_replay_clear()");
    return 1;
}
static int aiag_replay_run(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.REPLAYRUN: attesi (sessionId, targetId)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_replay_run(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_goal_agent(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.GOALAGENT: atteso (name)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_goal_agent(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
/* Orchestrazione agenti (B6): metriche fleet-level (gmb_metrics.c). */
static int aiag_metric_record(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.METRICRECORD: attesi (agent, key, value)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_metric_record(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, (double)(");
    emit_expr(c, a[2]); sb_put(&c->out, "))");
    return 1;
}
static int aiag_metric_get(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.METRICGET: attesi (agent, key)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_metric_get(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_metric_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_metric_list())");
    return 1;
}
static int aiag_fleet_stats(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_fleet_stats())");
    return 1;
}
static int aiag_metric_reset(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_metric_reset()");
    return 1;
}
/* Orchestrazione agenti (A10): Hashline (gmb_hashline.c). */
static int aiag_hashline(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.HASHLINE: atteso (content)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_hashline_tag(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_hash_edit(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.HASHEDIT: attesi (content, lineRef, newText)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_hashline_edit(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_hash_verify(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.HASHVERIFY: attesi (content, lineRef)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_hashline_verify(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
/* Orchestrazione agenti (B7): checkpoint/recovery (gmb_checkpoint.c). */
static int aiag_checkpoint(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.CHECKPOINT: attesi (sessionId, state)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_checkpoint_save(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_resume(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RESUME: atteso (sessionId)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_checkpoint_resume(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_checkpoint_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_checkpoint_list())");
    return 1;
}
static int aiag_checkpoint_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_checkpoint_clear()");
    return 1;
}
/* Persistenza orchestrazione su disco (gmb_orchestration.c). */
static int aiag_orch_save(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.ORCHSAVE: atteso (path)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_orch_save(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_orch_load(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.ORCHLOAD: atteso (path)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_orch_load(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aiag_task_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_task_clear(), (gmb_double_t)0)");
    return 1;
}
static int aiag_plan_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_plan_clear(), (gmb_double_t)0)");
    return 1;
}
/* AI VERIFY <azione> REQUIRE <require> (B2) → gmb_verify_run_named(azione, require, 3, …). */
static int aiag_verify(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI VERIFY: attesi (azione, REQUIRE)"); return 1; }
    /* Runtime condiviso `gmb_verify_run_*` (la stessa logica della VM/C, builtin 1992):
     * l'azione "COMPILE <path>" passa da gmb_compile_bas_file (weak symbol del compilatore C)
     * e il REQUIRE da gmb_expr_eval_ex.
     * LOCALI DEL CHIAMANTE (parità C# MacroEvalExprWithLocals / VM/C): si passano come due
     * compound-literal paralleli (nomi, valori) a `gmb_verify_run_named`, così
     * `REQUIRE "LEN(RESULT$) > n"` vede `n`. Solo SCALARI (numeri/stringhe): array,
     * collezioni, UDT e valori boxed non hanno una conversione gmb_val_t banale, e la VM/C
     * fa lo stesso. */
    int nloc = (c && c->cur_fn) ? c->cur_fn->locals.n : 0;
    char (*lnames)[64] = NULL;
    int* ltype = NULL;
    int* lflag = NULL;   /* bit0 byref, bit1 captured */
    if (nloc > 0) {
        lnames = (char(*)[64])calloc((size_t)nloc, sizeof *lnames);
        ltype = (int*)calloc((size_t)nloc, sizeof *ltype);
        lflag = (int*)calloc((size_t)nloc, sizeof *lflag);
        if (!lnames || !ltype || !lflag) { free(lnames); free(ltype); free(lflag); lnames = NULL; nloc = 0; }
    }
    if (nloc > 0) {
        for (int i = 0; i < nloc; i++) {
            var_t* v = &c->cur_fn->locals.v[i];
            ltype[i] = -1;
            if (!v->name || !*v->name) continue;
            if (v->is_array || v->gkind || v->vclass[0] || v->boxed_val || v->sb_builder
                || v->regex_obj || v->matrix || v->jagged || v->matrix_handle || v->nullable
                || v->is_bytes || v->stats_val || v->kvp_entry || v->posted_val) continue;
            snprintf(lnames[i], sizeof lnames[i], "%s", v->name);
            ltype[i] = v->type;
            lflag[i] = (v->byref ? 1 : 0) | (v->captured ? 2 : 0);
        }
    }
    sb_put(&c->out, "gmb_string_alloc(gmb_verify_run_named(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, 3, ");
    sb_printf(&c->out, "%d, (const char*[]){", nloc);
    for (int i = 0; i < nloc; i++)
        sb_printf(&c->out, "%s\"%s\"", i ? ", " : "", ltype[i] < 0 ? "" : lnames[i]);
    sb_put(&c->out, "}, (gmb_val_t[]){");
    for (int i = 0; i < nloc; i++) {
        if (i) sb_put(&c->out, ", ");
        if (ltype[i] < 0) { sb_put(&c->out, "gmb_val_nil()"); continue; }
        int deref = (lflag[i] & 1) && !(lflag[i] & 2);
        if (ltype[i] == TY_STR) {
            sb_put(&c->out, deref ? "gmb_make_val_string(((*" : "gmb_make_val_string((");
            emit_ident(c, lnames[i]);
            sb_put(&c->out, deref ? ")).data)" : ").data)");
        } else {
            sb_put(&c->out, deref ? "gmb_make_val_double((double)((*" : "gmb_make_val_double((double)(");
            emit_ident(c, lnames[i]);
            sb_put(&c->out, deref ? ")))" : "))");
        }
    }
    sb_put(&c->out, "}))");
    free(lnames); free(ltype); free(lflag);
    return 1;
}
static int aiag_eval_validate(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.EVALVALIDATE: attesi (result, folder)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_eval_validate(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int aiag_decide(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI DECIDE: attesi (state, kind)"); return 1; }
    if (n >= 6) {
        /* Diagnostica: (state, kind, providers_csv, "", "", ""). */
        sb_put(&c->out, "gmb_string_take(gmb_ai_decide_verify_json(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[2]); sb_put(&c->out, ".data))");
        return 1;
    }
    if (n >= 5) {
        /* Forma batch interna: (state, names_json, kinds_json, provider, fallback). */
        sb_put(&c->out, "gmb_string_take(gmb_ai_decide_group_json(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[3]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[4]); sb_put(&c->out, ".data))");
        return 1;
    }
    /* 4° arg = catena FALLBACK (CSV `|`): stessa funzione del runtime condiviso. */
    sb_put(&c->out, n >= 4 ? "gmb_string_take(gmb_ai_decide_run_json_chain("
                           : "gmb_string_take(gmb_ai_decide_run_json(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "\"\"");
    if (n >= 4) { sb_put(&c->out, ", "); emit_str_operand(c, a[3]); sb_put(&c->out, ".data"); }
    sb_put(&c->out, "))");
    return 1;
}
static int aiag_hook_fire(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.HOOKFIRE: atteso (event[, payload])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_hook_fire(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "\"\"");
    sb_put(&c->out, "))");
    return 1;
}
static const aiag_row AIAG_TBL[] = {
    { GMB_TOK_AiAgentGet, aiag_get },
    { GMB_TOK_AiAgentAsk, aiag_ask },
    { GMB_TOK_AiAgentStatus, aiag_status }, { GMB_TOK_AiAgentEvents, aiag_events },
    { GMB_TOK_AiAgentCode, aiag_code }, { GMB_TOK_AiAgentFix, aiag_fix },
    { GMB_TOK_AgentRunCua, aiag_run },
    { GMB_TOK_BlackboardWrite, aiag_bb_write },
    { GMB_TOK_BlackboardRead, aiag_bb_read },
    { GMB_TOK_BlackboardWait, aiag_bb_wait },
    { GMB_TOK_AgentSwarm, aiag_swarm },
    { GMB_TOK_AiCategorySet, aiag_cat_set },
    { GMB_TOK_AiCategoryFallback, aiag_cat_fallback },
    { GMB_TOK_AiCategoryAuto, aiag_cat_auto },
    { GMB_TOK_AiCategoryPick, aiag_cat_pick },
    { GMB_TOK_AiCategoryList, aiag_cat_list },
    { GMB_TOK_AiTaskDefine, aiag_task_define },
    { GMB_TOK_AiTaskDepends, aiag_task_depends },
    { GMB_TOK_AiTaskAssign, aiag_task_assign },
    { GMB_TOK_AiTaskStatus, aiag_task_status },
    { GMB_TOK_AiTaskList, aiag_task_list },
    { GMB_TOK_AiTaskRun, aiag_task_run },
    { GMB_TOK_AiPlanDefine, aiag_plan_define },
    { GMB_TOK_AiPlanAdd, aiag_plan_add },
    { GMB_TOK_AiPlanApprove, aiag_plan_approve },
    { GMB_TOK_AiPlanStatus, aiag_plan_status },
    { GMB_TOK_AiPlanList, aiag_plan_list },
    { GMB_TOK_AiPlanRun, aiag_plan_run },
    { GMB_TOK_AiHookRegister, aiag_hook_register },
    { GMB_TOK_AiHookUnregister, aiag_hook_unregister },
    { GMB_TOK_AiHookList, aiag_hook_list },
    { GMB_TOK_AiHookEvents, aiag_hook_events },
    { GMB_TOK_AiHookFire, aiag_hook_fire },
    { GMB_TOK_AiVerify, aiag_verify },
    { GMB_TOK_AiArtifactPut, aiag_art_put },
    { GMB_TOK_AiArtifactLink, aiag_art_link },
    { GMB_TOK_AiArtifactGet, aiag_art_get },
    { GMB_TOK_AiArtifactList, aiag_art_list },
    { GMB_TOK_AiArtifactRemove, aiag_art_remove },
    { GMB_TOK_AiResourceSet, aiag_res_set },
    { GMB_TOK_AiResourceGet, aiag_res_get },
    { GMB_TOK_AiResourceList, aiag_res_list },
    { GMB_TOK_AiResourceAvailable, aiag_res_available },
    { GMB_TOK_AiResourceSwarmSize, aiag_res_swarmsize },
    { GMB_TOK_AiHandoff, aiag_handoff },
    { GMB_TOK_AiHandoffComplete, aiag_handoff_complete },
    { GMB_TOK_AiHandoffStatus, aiag_handoff_status },
    { GMB_TOK_AiHandoffList, aiag_handoff_list },
    { GMB_TOK_AiHandoffClear, aiag_handoff_clear },
    { GMB_TOK_AiRulesAdd, aiag_rules_add },
    { GMB_TOK_AiRulesFor, aiag_rules_for },
    { GMB_TOK_AiRulesList, aiag_rules_list },
    { GMB_TOK_AiRulesRemove, aiag_rules_remove },
    { GMB_TOK_AiRulesClear, aiag_rules_clear },
    { GMB_TOK_AiGoalSet, aiag_goal_set },
    { GMB_TOK_AiGoalGet, aiag_goal_get },
    { GMB_TOK_AiGoalStatus, aiag_goal_status },
    { GMB_TOK_AiGoalTick, aiag_goal_tick },
    { GMB_TOK_AiGoalClear, aiag_goal_clear },
    { GMB_TOK_AiReplayStart, aiag_replay_start },
    { GMB_TOK_AiReplayRecord, aiag_replay_record },
    { GMB_TOK_AiReplayStop, aiag_replay_stop },
    { GMB_TOK_AiReplayGet, aiag_replay_get },
    { GMB_TOK_AiReplayList, aiag_replay_list },
    { GMB_TOK_AiReplayClear, aiag_replay_clear },
    { GMB_TOK_AiReplayRun, aiag_replay_run },
    { GMB_TOK_AiGoalAgent, aiag_goal_agent },
    { GMB_TOK_AiEvalValidate, aiag_eval_validate },
    { GMB_TOK_AiDecide, aiag_decide },
    { GMB_TOK_AiMetricRecord, aiag_metric_record },
    { GMB_TOK_AiMetricGet, aiag_metric_get },
    { GMB_TOK_AiMetricList, aiag_metric_list },
    { GMB_TOK_AiFleetStats, aiag_fleet_stats },
    { GMB_TOK_AiMetricReset, aiag_metric_reset },
    { GMB_TOK_AiHashLine, aiag_hashline },
    { GMB_TOK_AiHashEdit, aiag_hash_edit },
    { GMB_TOK_AiHashVerify, aiag_hash_verify },
    { GMB_TOK_AiCheckpoint, aiag_checkpoint },
    { GMB_TOK_AiResume, aiag_resume },
    { GMB_TOK_AiCheckpointList, aiag_checkpoint_list },
    { GMB_TOK_AiCheckpointClear, aiag_checkpoint_clear },
    { GMB_TOK_AiOrchSave, aiag_orch_save },
    { GMB_TOK_AiOrchLoad, aiag_orch_load },
    { GMB_TOK_AiTaskClear, aiag_task_clear },
    { GMB_TOK_AiPlanClear, aiag_plan_clear },
};
int emit_bf_ai_agent(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns;
    for (size_t i = 0; i < sizeof(AIAG_TBL) / sizeof(AIAG_TBL[0]); i++)
        if (AIAG_TBL[i].tk == tk) return AIAG_TBL[i].sp(c, args, nargs, line);
    return 0;
}

/* ── dominio: AI provider registry (namespace PROVIDER fuso in AI, 2026-09-03) ──
 * Funzioni AI.REGISTER/SETACTIVE/ACTIVE/TEST/... → runtime condiviso gmb_provider_*.
 * Dispatch per NOME dichiarato (regola d'oro §12.2): il parser C non risolve i TokenKind
 * Provider* in GMB_TOK_ dedicati. Guardia ns==AI. Emette `.data` per i gmb_string_t. */
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } aip_row;

/* Argomento "role/capability": accetta la costante %CHAT/%OCR/%AUDIO/%EMBEDDING/%RERANKER
 * (foldata dal parser come variabile "%NOME") OPPURE la stringa di ruolo storico. Emette
 * il ruolo canonico come stringa const char* (.data di un gmb_string_alloc). */
static void emit_role_operand(ctx_t* c, gmb_expr_t* e)
{
    if (e && e->kind == GMB_EXPR_VAR && e->var && e->var[0] == '%') {
        static const struct { const char* n; const char* role; } capr[] = {
            { "%CHAT", "completion" }, { "%OCR", "ocr" }, { "%AUDIO", "audio" },
            { "%EMBEDDING", "embedding" }, { "%RERANKER", "reranker" },
        };
        for (size_t i = 0; i < sizeof(capr) / sizeof(capr[0]); i++)
            if (strcasecmp(e->var, capr[i].n) == 0) {
                sb_put(&c->out, "gmb_string_alloc(\"");
                sb_put(&c->out, capr[i].role);
                sb_put(&c->out, "\")");
                return;
            }
    }
    emit_str_operand(c, e);
}

static int aip_register(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 4) { c99_fail(c, line, "AI.REGISTER: attesi (role, name, kind, config)"); return 1; }
    /* Forma funzione → gmb_provider_register_kind (qualunque kind, parità C# Register).
     * La forma statement usa gmb_provider_register (strict, RegisterSource) in gmb_c99_emit.c. */
    sb_put(&c->out, "((gmb_double_t)gmb_provider_register_kind(");
    emit_role_operand(c, a[0]); sb_put(&c->out, ".data, ");
    for (int i = 1; i < 4; i++) { emit_str_operand(c, a[i]); sb_put(&c->out, ".data"); if (i < 3) sb_put(&c->out, ", "); }
    sb_put(&c->out, "))");
    return 1;
}
static int aip_set_active(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SETACTIVE: attesi (role, name)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_set_active_explicit(");
    emit_role_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_active(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    /* AI.ACTIVE (senza role): provider attivo del ruolo di default (%CHAT). Prima errore
     * "atteso (role)" → sample `AI.ACTIVE()` falliva. */
    if (n < 1) {
        /* Collisione: `SCREENINFO().Active` arriva qui come ACTIVE a 0 arg (Active è
         * keyword) → leggiamo il campo Active del DisplayMode come stringa. */
        sb_put(&c->out, "gmb_double_to_string((gmb_double_t)gmb_val_to_double(gmb_dict_get(gmb_val_as_dict(");
        gf_screeninfo(c, NULL, 0);
        sb_put(&c->out, "), \"Active\")))");
        return 1;
    }
    sb_put(&c->out, "gmb_string_alloc(gmb_provider_active(");
    emit_role_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_rotate(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.ROTATE: atteso (role)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_provider_rotate(");
    emit_role_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_test(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.TEST: attesi (role, name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_provider_test(");
    emit_role_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_fail(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.FAIL: atteso (name)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_fail_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_reset_failed(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.RESETFAILED: atteso (name)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_reset_failed_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_set_priority(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SETPRIORITY: attesi (name, priority)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_set_priority_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (int)("); emit_expr(c, a[1]); sb_put(&c->out, ")))");
    return 1;
}
static int aip_priority(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.PRIORITY: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_provider_priority_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aip_set_weight(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SETWEIGHT: attesi (name, weight)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_set_weight_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, (int)("); emit_expr(c, a[1]); sb_put(&c->out, ")))");
    return 1;
}
static int aip_weight(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.WEIGHT: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_provider_weight_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    return 1;
}
static int aip_unregister(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.UNREGISTER: atteso (name)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_unregister_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_set_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SETSTATUS: attesi (name, status)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_set_status_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.STATUS: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_provider_status_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_configure(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.CONFIGURE: attesi (name, key[, value])"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_configure_name(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, "))");
    return 1;
}
static int aip_save(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.SAVE: atteso (path)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_save_registry(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_load(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.LOAD: atteso (path)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_provider_load_registry(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_set_credits(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SETCREDITS: attesi (name, credits)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_ai_set_credits(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_expr(c, a[1]); sb_put(&c->out, "))");
    return 1;
}
static int aip_credits(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.CREDITS: atteso (name)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_ai_get_credits(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_start_hc(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.STARTHEALTHCHECK: atteso (intervalMinutes)"); return 1; }
    /* parità ProviderHealthMonitor.Start: imposta l'intervallo e ritorna 1.0 */
    sb_put(&c->out, "((gmb_double_t)(gmb_provider_set_healthcheck_interval(\"\", \"\", (int)(");
    emit_expr(c, a[0]); sb_put(&c->out, ")), 1.0))");
    return 1;
}
static int aip_stop_hc(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)(gmb_provider_set_healthcheck_interval(\"\", \"\", 0), 1.0))");
    return 1;
}
static int aip_hc_interval(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)gmb_provider_healthcheck_interval(\"\", \"\"))");
    return 1;
}
static int aip_run_hc(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n;
    sb_put(&c->out, "((gmb_double_t)gmb_provider_run_healthcheck())");
    return 1;
}
/* ── provider-config: getter/setter AI.GETMODEL/GETKEY/... e statement AI SET ... →
 * gmb_ai_get_config/set_config del runtime C condiviso (port 2026-09-07). I getter
 * ritornano gmb_string_t (gmb_ai_get_config: const char*), i setter gmb_double_t. */
#define AIP_GC(NAME, KEY) static int aip_##NAME(ctx_t* c, gmb_expr_t** a, int n, int line) { \
    if (n < 1) { c99_fail(c, line, "AI." #NAME ": atteso (provider)"); return 1; } \
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_get_config("); \
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, \"" KEY "\"))"); return 1; }
AIP_GC(getkey, "apiKey")
AIP_GC(getmodel, "model")
AIP_GC(gettemp, "temperature")
AIP_GC(getbase, "baseUrl")
#define AIP_SC(NAME, KEY) static int aip_##NAME(ctx_t* c, gmb_expr_t** a, int n, int line) { \
    if (n < 2) { c99_fail(c, line, "AI." #NAME ": attesi (provider, value)"); return 1; } \
    sb_put(&c->out, "((gmb_double_t)gmb_ai_set_config("); \
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, \"" KEY "\", "); \
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1; }
AIP_SC(setkey, "apiKey")
AIP_SC(setmodel, "model")
AIP_SC(settemp, "temperature")
AIP_SC(setbase, "baseUrl")
static int aip_setoption(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SETOPTION: attesi (name, value)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_opt_set_persist(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_option(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.OPTION: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_opt_get_str(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_info(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.INFO: atteso (provider)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_info(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_providers(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    /* AI.PROVIDERS(): parità C# AiProviders = File.ReadAllText(providers.json). Prima:
     * gmb_provider_list("") (AI.LIST) → shape sbagliata (gen_AI_Providers). */
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_providers_json())"); return 1;
}
static int aip_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* AI.LIST(role): LISTA vera di nomi (gmb_list_t, parità string[] C# e GMB_OBJ_LIST VMC).
     * Prima: stringa "[a,b]" → `DIM x() = AI.LIST(..)` + `x(0)` leggeva "["/91 (AiDbAdmin). */
    if (n < 1) { c99_fail(c, line, "AI.LIST: atteso (role)"); return 1; }
    sb_put(&c->out, "gmb_provider_list_names(");
    emit_role_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int aip_pluginsinfo(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.PLUGINSINFO: atteso (plugin)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(llmengine_plugin_info(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_plugincall(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.PLUGINCALL: attesi (plugin, arg)"); return 1; }
    /* Il plugin non porta codice: `PLUGIN.md` dichiara la procedura, che deve esistere NEL
     * PROGRAMMA (parità C# PluginRegistry.Call → CallHandler → _basicProcedures). Il runtime C
     * non ha una VM BASIC, quindi il transpiler emette un dispatcher generato
     * (`__gmb_plugin_call`) verso le gmb_user_* — prima rispondeva la stringa dichiarativa
     * "PLUGIN nome(arg) OK" e la procedura non veniva MAI eseguita (divergenza chiusa). */
    c->uses_plugincall = 1;
    sb_put(&c->out, "gmb_string_alloc(__gmb_plugin_call(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
/* AI.PLUGINSLIST() come FUNZIONE: `[a, b]` (parità array IL). Prima esisteva solo il percorso
 * AJOIN(AI.PLUGINSLIST(), sep) → "simbolo non trovato" su C99Native. */
static int aip_pluginslist(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_list_join_bracketed(llmengine_plugin_list_names())"); return 1;
}
/* AI.PLUGINLIST(): il REGISTRO in-process (parità PluginRegistry.List C#) — non il disco. */
static int aip_pluginlist(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_list_join_bracketed(llmengine_plugin_list_registry())"); return 1;
}
/* AI.DISCOVER(prefix, capability, baseUrl, apiKey, soloFree) come FUNZIONE: `char*` →
 * gmb_string_alloc (parità ProviderDiscover C#). Prima solo la forma statement `AI DISCOVER`. */
static int aip_discover(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* Arity canonica 4..5 (registry: {"AI","Discover",79,4,5}): soloFree è OPZIONALE.
     * Richiedere 5 rifiutava `AI.DISCOVER(prefix, capability, baseUrl, apiKey)`
     * (ai/gen_AI_Discover.bas). Default soloFree = 0 come il canone IL. */
    if (n < 4) { c99_fail(c, line, "AI.DISCOVER: attesi (prefix, capability, baseUrl, apiKey [, soloFree])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_provider_discover(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[3]); sb_put(&c->out, ".data, (int)(");
    if (n >= 5) emit_expr(c, a[4]); else sb_put(&c->out, "0");
    sb_put(&c->out, ")))"); return 1;
}
static int aip_listcollections(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.LISTCOLLECTIONS: atteso (db)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_aiv_list_collections("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
/* F1-followup (2026-09-11): emissioni mancanti del cluster AiVector* (EnsureCollection/
 * IndexText/IndexMeta/RemoveSource/LoadDocuments): senza, il driver C99Native falliva con
 * "simbolo non trovato". Stesse funzioni runtime di IL/PCode/VMC. */
static int aip_ensurecollection(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.ENSURECOLLECTION: attesi (db, name [, dims [, format]])"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_aiv_ensure_collection_fmt(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) emit_num_or_zero(c, a[2]); else sb_put(&c->out, "0");
    sb_put(&c->out, ", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, "))"); return 1;
}
static int aip_indextext(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 4) { c99_fail(c, line, "AI.INDEXTEXT: attesi (db, coll, sourceId, text)"); return 1; }
    /* P0c (todo Tokenizer-WordPiece §9): source_id ESPLICITO (prima perso — la superficie
     * gmb_aiv_index_text genera un source hash da solo). */
    sb_put(&c->out, "((gmb_double_t)gmb_aiv_index_source(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[3]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_indexmeta(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 4) { c99_fail(c, line, "AI.INDEXMETA: attesi (db, coll, sourceId, text [, meta])"); return 1; }
    /* P0c (§9): source_id E metadata passati davvero (prima il metadata era scartato e il
     * source generato d'ufficio). */
    if (n >= 5)
    {
        sb_put(&c->out, "((gmb_double_t)gmb_aiv_index_meta(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[3]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[4]); sb_put(&c->out, ".data))"); return 1;
    }
    sb_put(&c->out, "((gmb_double_t)gmb_aiv_index_source(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[3]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_removesource(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.REMOVESOURCE: attesi (db, coll, sourceId)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_aiv_remove_source(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_loaddocuments(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.LOADDOCUMENTS: attesi (db, coll, path [, pattern])"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_aiv_load_documents(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, "))"); return 1;
}
static int aip_count(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.COUNT: attesi (db, collection)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_aiv_count(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int aip_purgedeprecated(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.PURGEDEPRECATED: attesi (db, collection)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_aiv_purge_deprecated(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)"); return 1;
}
static int aip_embed(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.EMBED: attesi (text, dims)"); return 1; }
    /* AI.EMBED ritorna una LISTA BASIC (parità VM-C/IL/PCode: gmb_list_from_double_array).
     * Prima ritornava un `double*` nudo: `DIM v = AI.EMBED(...)` classificava v come LISTA
     * (tk_returns_list) ma il RHS era double* → "assigning to 'gmb_list_t' from 'double *'"
     * (embed_dims FAIL su C99Native). NULL = nessun provider → errore ESPLICITO (parità IL). */
    /* NB: la statement-expression `({...})` deve TERMINARE con un'espressione (non un if/else,
     * altrimenti è void). Quindi: `if (!_ee) gmb_raise_error(...);` come statement, poi la
     * chiamata `gmb_list_from_double_array(...);` come ULTIMA (valore della statement-expr).
     * NULL → gmb_list_from_double_array(NULL,...) ritorna lista vuota (safe). */
    sb_put(&c->out, "({ gmb_int_t _en = 0; double* _ee = gmb_aiv_embed(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_num_or_zero(c, a[1]);
    sb_put(&c->out, ", &_en); if (!_ee) gmb_raise_error(0, \"Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)\", 0L); gmb_list_from_double_array(_ee, _en); })");
    return 1;
}
static int aip_workflowsteps(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    sb_put(&c->out, "gmb_workflow_steps(");
    if (n >= 1) { emit_str_operand(c, a[0]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "gmb_string_alloc(\"\").data");
    sb_put(&c->out, ")"); return 1;
}
static int aip_workflowlist(ctx_t* c, gmb_expr_t** a, int n, int line) { (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_workflow_list()"); return 1; }
static int aip_workflowstatus(ctx_t* c, gmb_expr_t** a, int n, int line) { (void)line;
    sb_put(&c->out, "gmb_workflow_status(");
    if (n >= 1) { emit_str_operand(c, a[0]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "gmb_string_alloc(\"\").data");
    sb_put(&c->out, ")"); return 1; }
static void emit_num_k(ctx_t* c, gmb_expr_t* e, const char* dflt) {
    if (!e) { sb_put(&c->out, dflt); return; }
    if (e->var && strcasecmp(e->var, "_true_") == 0) { sb_put(&c->out, "-1"); return; }
    if (e->var && strcasecmp(e->var, "_false_") == 0) { sb_put(&c->out, "0"); return; }
    emit_expr(c, e);
}
static int aip_ask(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.ASK: attesi (db, collection, question [, topK] [, reranker] [, minEvidence])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_rag_ask_sel(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, (int)("); emit_num_k(c, n >= 4 ? a[3] : NULL, "5"); sb_put(&c->out, "), (int)("); emit_num_k(c, n >= 5 ? a[4] : NULL, "1"); sb_put(&c->out, "), "); emit_num_k(c, n >= 6 ? a[5] : NULL, "-1.0"); sb_put(&c->out, "))"); return 1;
}
static int aip_rerank(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.RERANK: attesi (query, jsonResults, topK)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_rerank(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, (int)("); emit_expr(c, a[2]); sb_put(&c->out, ")))"); return 1;
}
static int is_null_arg(gmb_expr_t* e) {
    return e && e->kind == GMB_EXPR_VAR && e->var
        && (strcasecmp(e->var, "NULL") == 0 || strcasecmp(e->var, "Nothing") == 0
            || strcasecmp(e->var, "NIL") == 0);
}
static int aip_search(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.SEARCH: attesi (db, collection, query [, topK] [, hybrid] [, filter] [, reranker] [, minEvidence] [, explain])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_rag_search_sel(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, (int)("); emit_num_k(c, n >= 4 ? a[3] : NULL, "5"); sb_put(&c->out, "), (int)("); emit_num_k(c, n >= 5 ? a[4] : NULL, "1"); sb_put(&c->out, "), ");
    if (n >= 6 && !is_null_arg(a[5])) { emit_str_operand(c, a[5]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, ", (int)("); emit_num_k(c, n >= 7 ? a[6] : NULL, "1");     sb_put(&c->out, "), "); emit_num_k(c, n >= 8 ? a[7] : NULL, "-1.0"); sb_put(&c->out, ", "); if (n >= 9) { sb_put(&c->out, "(int)("); emit_num_k(c, a[8], "0"); sb_put(&c->out, ")"); } else sb_put(&c->out, "0");
    sb_put(&c->out, "))"); return 1;
}
static int aip_ragcontext(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.RAGCONTEXT: attesi (db, collection, query [, topK] [, reranker] [, minEvidence])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_rag_context_sel(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data, (int)("); emit_num_k(c, n >= 4 ? a[3] : NULL, "5"); sb_put(&c->out, "), (int)("); emit_num_k(c, n >= 5 ? a[4] : NULL, "1"); sb_put(&c->out, "), "); emit_num_k(c, n >= 6 ? a[5] : NULL, "-1.0"); sb_put(&c->out, "))"); return 1;
}
static int aip_mcplisttools(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    sb_put(&c->out, "gmb_string_alloc(");
    if (n >= 1) { sb_put(&c->out, "gmb_mcp_list_tools_server("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); }
    else sb_put(&c->out, "gmb_mcp_list_tools())");
    return 1;
}
/* AI.MCPPROMPTS() / AI.MCPRESOURCES(): mancavano le righe di tabella — il lookup
 * namespace le risolveva ma nessun emitter le copriva, quindi su C99Native il programma
 * NON compilava ("simbolo non trovato: AI.MCPRESOURCES") mentre girava su IL/PCode/VM-C.
 * Runtime condiviso: gmb_mcp_prompts/gmb_mcp_resources (stessa resa degli altri backend). */
static int aip_mcpprompts(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_prompts())"); return 1;
}
static int aip_mcpresources(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_resources())"); return 1;
}
static int aip_mcpserverstatus(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.MCPSERVERSTATUS: atteso (name)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_server_status("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_mcpserverlist(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_server_list())"); return 1;
}
static int aip_mcpmarketlist(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_server_marketplace_list())"); return 1;
}
static int aip_mcpmarketget(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.MCPSERVERMARKETPLACEGET: atteso (nome)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_server_marketplace_get("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_mcplisttoolsserver(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.MCPLISTTOOLSSERVER: atteso (server)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_list_tools_server("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_mcpcall(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.MCPCALL: attesi (server, tool, args)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_mcp_call(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))"); return 1;
}
/* PROCESS <file> [PROMPT <istruzioni>] (E3): statement-espressione → runtime C condiviso
 * gmb_process_document (la stessa funzione usata da VM/C `gmb_bf_1227` e dal backend C99). */
static int aip_process(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "PROCESS: atteso (file [, prompt])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_process_document(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "gmb_string_alloc(\"Riassumi il documento.\").data");
    sb_put(&c->out, "))"); return 1;
}
static int aip_ocr(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.OCR: atteso (imagePath)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_ocr_read(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    sb_put(&c->out, n >= 2 ? "gmb_string_alloc(\"Extract all text from this image.\").data" : "gmb_string_alloc(\"Extract all text from this image.\").data");
    sb_put(&c->out, ", 0))"); return 1;
}
static int aip_transcribe(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.TRANSCRIBE: atteso (audioPath)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_audio_transcribe(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    sb_put(&c->out, "gmb_string_alloc(\"\").data, 0))"); return 1;
}
/* F1-followup (2026-09-11): AI.ANALYZEIMAGE mancava del tutto nel dispatch C99Native
 * (gap preesistente: VMC la emettono, qui "non definita"). Stessa emissione
 * dell'arm emitter C# (runtime gmb_ai_analyze_image). */
static int aip_analyzeimage(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.ANALYZEIMAGE: atteso (imagePath)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_analyze_image(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\").data");
    sb_put(&c->out, "))"); return 1;
}
/* F3 multimodale (2026-09-11): primitive immagini PNG (runtime condiviso gmb_image.c). */
/* Numero da espressione per gli slot numerici: i letterali stringa (es. dimensioni "x"
 * nei test) diventano 0 — parità con PCode-SafeDouble (mai InvalidProgram,
 * mai errori cc su struct→int). Le variabili stringa restano (int)(expr) come altrove. */
static void emit_num_or_zero(ctx_t* c, gmb_expr_t* e) {
    if (e && e->kind == GMB_EXPR_STR) { sb_put(&c->out, "0"); return; }
    sb_put(&c->out, "(int)("); emit_expr(c, e); sb_put(&c->out, ")");
}
static int aip_imageinfo(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.IMAGEINFO: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_image_info_str(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_imageresize(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 4) { c99_fail(c, line, "AI.IMAGERESIZE: attesi (input, output, width, height)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_image_resize(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); emit_num_or_zero(c, a[2]); sb_put(&c->out, ", "); emit_num_or_zero(c, a[3]); sb_put(&c->out, "))"); return 1;
}
static int aip_imagecrop(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 6) { c99_fail(c, line, "AI.IMAGECROP: attesi (input, output, x, y, width, height)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_image_crop(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); emit_num_or_zero(c, a[2]); sb_put(&c->out, ", "); emit_num_or_zero(c, a[3]); sb_put(&c->out, ", "); emit_num_or_zero(c, a[4]); sb_put(&c->out, ", "); emit_num_or_zero(c, a[5]); sb_put(&c->out, "))"); return 1;
}
static int aip_imageconvert(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.IMAGECONVERT: attesi (input, output)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_image_convert(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
/* F2 multimodale (2026-09-11): estrazione strutturata documenti (gmb_docfields.c). */
static int aip_docfields(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.DOCFIELDS: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_docfields_json(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_docextract(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.DOCEXTRACT: attesi (path, schema)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_docextract_json(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
/* F4 multimodale (2026-09-11): TTS via motore di sistema (gmb_tts.c). */
static int aip_synthesize(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.SYNTHESIZE: attesi (text, output)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_tts_synthesize(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
/* F5 multimodale (2026-09-11): embedding percettivo, compare, detect, classify. */
static int aip_imageembed(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.IMAGEEMBED: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_image_embed_list("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)"); return 1;
}
static int aip_imagecompare(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.IMAGECOMPARE: attesi (path1, path2)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_image_compare(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_imagedetect(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.IMAGEDETECT: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_image_detect("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_imageclassify(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.IMAGECLASSIFY: attesi (path, labels)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_image_classify(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
/* F6 multimodale (2026-09-11): agent ask con input multimodale. */
static int aip_agentaskfile(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.AGENTASKFILE: attesi (name, path [, question])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_ask_file(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data"); } else sb_put(&c->out, "gmb_string_alloc(\"\").data");
    sb_put(&c->out, "))"); return 1;
}
/* F7 multimodale (2026-09-11): video (Y4M nativo). */
static int aip_videoinfo(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.VIDEOINFO: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_video_info_str("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_videoframe(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 3) { c99_fail(c, line, "AI.VIDEOFRAME: attesi (path, index, output)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_video_frame(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_num_or_zero(c, a[1]); sb_put(&c->out, ", ");
    emit_str_operand(c, a[2]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_videosnapshot(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "AI.VIDEOSNAPSHOT: attesi (path, output)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_video_frame(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, 0, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))"); return 1;
}
static int aip_getdefault(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n;
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_get_default(\"\"))"); return 1;
}
static int aip_setdefault(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "AI.SETDEFAULT: atteso (provider)"); return 1; }
    sb_put(&c->out, "((gmb_double_t)gmb_ai_set_default(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}

/* ── AI FINETUNE / AI LORA APPLY / stato (superficie MLX LoRA, runtime gmb_finetune) ── */
/* Stream D — AI DISTILL: BUILD (jsonl [, opts]) + STATUS. */
static int aip_distill_build(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ char __dsb[4096]; gmb_string_t __dsj = ");
    if (n >= 2) { sb_put(&c->out, "gmb_val_to_json("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, "; if (gmb_distill_build_opts(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __dsj.data, __dsb, sizeof __dsb) != 0) gmb_raise_error(5, __dsb, ");
    sb_printf(&c->out, "%d); free(__dsj.data); gmb_string_alloc(__dsb); })", line);
    return 1;
}
static int aip_distill_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc((char*)gmb_distill_status())"); return 1;
}
/* Stream D — AI DISTILL SAMPLE (file [, opts]) + SAMPLEN + SAMPLEFAIL. */
static int aip_distill_sample(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ char __dsb[8192]; gmb_string_t __dsj = ");
    if (n >= 2) { sb_put(&c->out, "gmb_val_to_json("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, "; if (gmb_distill_sample_opts(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __dsj.data, __dsb, sizeof __dsb) != 0) gmb_raise_error(5, __dsb, ");
    sb_printf(&c->out, "%d); free(__dsj.data); gmb_string_alloc(__dsb); })", line);
    return 1;
}
static int aip_distill_samplen(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_samplen())"); return 1;
}
static int aip_distill_samplefail(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_samplefail())"); return 1;
}
static int aip_distill_sampledone(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_sampledone())"); return 1;
}
/* Stream D — AI DISTILL EVAL (file [, opts]) + EVALTOK(idx) + EVALPASS(idx). */
static int aip_distill_eval(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ char __dsb[8192]; gmb_string_t __dsj = ");
    if (n >= 2) { sb_put(&c->out, "gmb_val_to_json("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, "; if (gmb_distill_eval_opts(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __dsj.data, __dsb, sizeof __dsb) != 0) gmb_raise_error(5, __dsb, ");
    sb_printf(&c->out, "%d); free(__dsj.data); gmb_string_alloc(__dsb); })", line);
    return 1;
}
static int aip_distill_evaltok(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_evaltok((int)(");
    emit_num_or_zero(c, a[0]); sb_put(&c->out, ")))"); return 1;
}
static int aip_distill_evalpass(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_evalpass((int)(");
    emit_num_or_zero(c, a[0]); sb_put(&c->out, ")))"); return 1;
}
static int aip_distill_evalacc(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)line;
    if (n != 1) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_evalacc((int)(");
    emit_num_or_zero(c, a[0]); sb_put(&c->out, ")))"); return 1;
}
/* Stream D — AI DISTILL APPLY (ckpt [, opts]) + RESTORE (backup). */
static int aip_distill_apply(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ char __dsb[8192]; gmb_string_t __dsj = ");
    if (n >= 2) { sb_put(&c->out, "gmb_val_to_json("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, "; if (gmb_distill_apply_opts(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __dsj.data, __dsb, sizeof __dsb) != 0) gmb_raise_error(5, __dsb, ");
    sb_printf(&c->out, "%d); free(__dsj.data); gmb_string_alloc(__dsb); })", line);
    return 1;
}
static int aip_distill_restore(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1) return 0;
    sb_put(&c->out, "({ char __dsb[8192]; if (gmb_distill_restore_opts(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __dsb, sizeof __dsb) != 0) gmb_raise_error(5, __dsb, ");
    sb_printf(&c->out, "%d); gmb_string_alloc(__dsb); })", line);
    return 1;
}
/* Stream D — AI DISTILL TRAIN (dataset [, opts]) + LOSS + HOLD + CKPT + DONE. */
static int aip_distill_train(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) return 0;
    sb_put(&c->out, "({ char __dsb[8192]; gmb_string_t __dsj = ");
    if (n >= 2) { sb_put(&c->out, "gmb_val_to_json("); emit_expr(c, a[1]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\")");
    sb_put(&c->out, "; if (gmb_distill_train_opts(");
    emit_str_operand(c, a[0]);
    sb_put(&c->out, ".data, __dsj.data, __dsb, sizeof __dsb) != 0) gmb_raise_error(5, __dsb, ");
    sb_printf(&c->out, "%d); free(__dsj.data); gmb_string_alloc(__dsb); })", line);
    return 1;
}
static int aip_distill_loss(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_loss())"); return 1;
}
static int aip_distill_hold(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_hold())"); return 1;
}
static int aip_distill_ckpt(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc((char*)gmb_distill_ckpt())"); return 1;
}
static int aip_distill_done(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)line;
    if (n != 0) return 0;
    sb_put(&c->out, "((gmb_double_t)gmb_distill_done())"); return 1;
}
static int aip_ft_status(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc((char*)gmb_ai_finetune_status())"); return 1;
}
static int aip_runs_json(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_runs_json())"); return 1;
}
static int aip_runs_count(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_runs_count()"); return 1;
}
static int aip_runs_clear(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_runs_clear()"); return 1;
}
static int aip_train_config(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_train_config_json())"); return 1;
}
static int aip_train_config_set(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "LLM.TRAINCONFIGSET: attesi (key, value)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_train_config_set(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_runs_item(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1) { c99_fail(c, line, "LLM.RUNSITEM: atteso (id)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_runs_item((int)atof(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data)))");
    return 1;
}
static int aip_dataset_stats(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1) { c99_fail(c, line, "LLM.DATASETSTATS: atteso (path)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_dataset_stats(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    return 1;
}
static int aip_lora_list(ctx_t* c, gmb_expr_t** a, int n, int line) {
    (void)a; (void)n; (void)line;
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_lora_list())"); return 1;
}
static int aip_train_export(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 1) { c99_fail(c, line, "LLM.TRAINEXPORT: atteso (out [, run])"); return 1; }
    if (n >= 2) {
        sb_put(&c->out, "(gmb_double_t)gmb_ai_train_export_run(");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, (int)atof(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data))");
    } else {
        sb_put(&c->out, "(gmb_double_t)gmb_ai_train_export("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data)");
    }
    return 1;
}
static int aip_runs_stop(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 1) { c99_fail(c, line, "LLM.RUNSTOP: atteso (id)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)gmb_ai_runs_stop((int)atof("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data))"); return 1;
}
/* LLM.EVALGATE(ref, cand) — verdetto gate (TRAIN EVAL GATE) dal runtime condiviso gmbllm. */
static int aip_lorasave(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "LLM.LORASAVE: attesi (ckpt, out)"); return 1; }
    sb_put(&c->out, "(({ int _ms = llmmodelio_model_adapter_save(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data); if (_ms != 0) gmb_raise_error(5, \"LLM.LORASAVE: fallito\", 0); (gmb_double_t)_ms; }))"); return 1;
}
static int aip_loraload(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "LLM.LORALOAD: attesi (adapter, out)"); return 1; }
    sb_put(&c->out, "(({ int _ms = llmmodelio_model_adapter_load(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data); if (_ms != 0) gmb_raise_error(5, \"LLM.LORALOAD: fallito\", 0); (gmb_double_t)_ms; }))"); return 1;
}
static int aip_evalgate(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "LLM.EVALGATE: attesi (ref, cand)"); return 1; }
    sb_put(&c->out, "(({ char _g[1024]; llmmodelio_model_gate(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, \"\", _g, (int)sizeof _g); gmb_string_alloc(_g); }))");
    return 1;
}
static int aip_ft_ckpt(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "gmb_string_alloc((char*)gmb_ai_finetune_ckpt())"); return 1;
}
static int aip_ft_loss(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_loss()"); return 1;
}
static int aip_ft_hold(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_hold()"); return 1;
}
static int aip_ft_done(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_done()"); return 1;
}
static int aip_ft_greedy(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_greedy()"); return 1;
}
static int aip_ft_greedy_after(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_greedy_after()"); return 1;
}
static int aip_ft_demo_changed(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_demo_changed()"); return 1;
}
static int aip_ft_base_train(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_base_train()"); return 1;
}
static int aip_ft_base_hold(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n != 0) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_ai_finetune_base_hold()"); return 1;
}

static const aip_row AIP_TBL[] = {
    { GMB_TOK_ProviderRegister, aip_register },
    { GMB_TOK_ProviderSetActive, aip_set_active },
    { GMB_TOK_AiActive, aip_active },
    { GMB_TOK_AiRotate, aip_rotate },
    { GMB_TOK_ProviderTest, aip_test },
    { GMB_TOK_ProviderMarkFailed, aip_fail },
    { GMB_TOK_ProviderResetFailed, aip_reset_failed },
    { GMB_TOK_ProviderSetPriority, aip_set_priority },
    { GMB_TOK_AiPriority, aip_priority },
    { GMB_TOK_ProviderSetWeight, aip_set_weight },
    { GMB_TOK_AiWeight, aip_weight },
    { GMB_TOK_ProviderUnregister, aip_unregister },
    { GMB_TOK_ProviderSetStatus, aip_set_status },
    { GMB_TOK_AiStatus, aip_status },
    { GMB_TOK_ProviderConfigure, aip_configure },
    { GMB_TOK_ProviderSave, aip_save },
    { GMB_TOK_ProviderLoad, aip_load },
    { GMB_TOK_AiSaveProv, aip_save },
    { GMB_TOK_AiLoadProv, aip_load },
    { GMB_TOK_ProviderSetCredits, aip_set_credits },
    { GMB_TOK_AiCredits, aip_credits },
    { GMB_TOK_ProviderStartHealthCheck, aip_start_hc },
    { GMB_TOK_ProviderStopHealthCheck, aip_stop_hc },
    { GMB_TOK_ProviderHealthCheckInterval, aip_hc_interval },
    { GMB_TOK_ProviderRunHealthCheck, aip_run_hc },
    { GMB_TOK_AiGetKey, aip_getkey },
    { GMB_TOK_AiSetKey, aip_setkey },
    { GMB_TOK_AiSetOption, aip_setoption },
    { GMB_TOK_AiOption, aip_option },
    { GMB_TOK_AiGetModel, aip_getmodel },
    { GMB_TOK_AiSetModel, aip_setmodel },
    { GMB_TOK_AiGetTemp, aip_gettemp },
    { GMB_TOK_AiSetTemp, aip_settemp },
    { GMB_TOK_AiGetBase, aip_getbase },
    { GMB_TOK_AiSetBase, aip_setbase },
    { GMB_TOK_AiInfo, aip_info },
    { GMB_TOK_AiKeys, aip_providers },
    { GMB_TOK_AiList, aip_list },
    { GMB_TOK_AiPluginInfo, aip_pluginsinfo },
    { GMB_TOK_AiPluginCall, aip_plugincall },
    { GMB_TOK_AiPluginsList, aip_pluginslist },
    { GMB_TOK_AiPluginList, aip_pluginlist },
    { GMB_TOK_ProviderDiscover, aip_discover },
    { GMB_TOK_AiOcr, aip_ocr },
    { GMB_TOK_AiAnalyzeImage, aip_analyzeimage },
    { GMB_TOK_Process, aip_process },
    { GMB_TOK_AiTranscribe, aip_transcribe },
    /* F3 multimodale (2026-09-11). */
    { GMB_TOK_AiImageInfo, aip_imageinfo },
    { GMB_TOK_AiImageResize, aip_imageresize },
    { GMB_TOK_AiImageCrop, aip_imagecrop },
    { GMB_TOK_AiImageConvert, aip_imageconvert },
    /* F2 multimodale (2026-09-11). */
    { GMB_TOK_AiDocFields, aip_docfields },
    { GMB_TOK_AiDocExtract, aip_docextract },
    /* F4 multimodale (2026-09-11). */
    { GMB_TOK_AiSynthesize, aip_synthesize },
    /* F5 multimodale (2026-09-11). */
    { GMB_TOK_AiImageEmbed, aip_imageembed },
    { GMB_TOK_AiImageCompare, aip_imagecompare },
    { GMB_TOK_AiImageDetect, aip_imagedetect },
    { GMB_TOK_AiImageClassify, aip_imageclassify },
    /* F6 multimodale (2026-09-11). */
    { GMB_TOK_AiAgentAskFile, aip_agentaskfile },
    /* F7 multimodale (2026-09-11). */
    { GMB_TOK_AiVideoInfo, aip_videoinfo },
    { GMB_TOK_AiVideoFrame, aip_videoframe },
    { GMB_TOK_AiVideoSnapshot, aip_videosnapshot },
    { GMB_TOK_McpListTools, aip_mcplisttools },
    { GMB_TOK_McpPrompts, aip_mcpprompts },
    { GMB_TOK_McpResources, aip_mcpresources },
    { GMB_TOK_McpCall, aip_mcpcall },
    { GMB_TOK_AiVectorListCollections, aip_listcollections },
    /* F1-followup (2026-09-11). */
    { GMB_TOK_AiVectorEnsureCollection, aip_ensurecollection },
    { GMB_TOK_AiVectorIndexText, aip_indextext },
    { GMB_TOK_AiVectorIndexMeta, aip_indexmeta },
    { GMB_TOK_AiVectorRemoveSource, aip_removesource },
    { GMB_TOK_AiVectorLoadDocuments, aip_loaddocuments },
    { GMB_TOK_AiVectorCountEmbeddings, aip_count },
    { GMB_TOK_AiVectorPurgeDeprecated, aip_purgedeprecated },
    { GMB_TOK_AiVectorSearch, aip_search },
    { GMB_TOK_AiVectorRagContext, aip_ragcontext },
    { GMB_TOK_AiVectorRerank, aip_rerank },
    { GMB_TOK_AiVectorAsk, aip_ask },
    { GMB_TOK_AiVectorEmbed, aip_embed },
    { GMB_TOK_WorkflowSteps, aip_workflowsteps },
    { GMB_TOK_WorkflowStatus, aip_workflowstatus },
    { GMB_TOK_WorkflowList, aip_workflowlist },
    { GMB_TOK_McpServerStatus, aip_mcpserverstatus },
    { GMB_TOK_McpServerList, aip_mcpserverlist },
    { GMB_TOK_McpServerMarketplaceList, aip_mcpmarketlist },
    { GMB_TOK_McpServerMarketplaceGet, aip_mcpmarketget },
    { GMB_TOK_McpListToolsServer, aip_mcplisttoolsserver },
    { GMB_TOK_AiGetDefault, aip_getdefault },
    { GMB_TOK_AiSetDefault, aip_setdefault },
    { GMB_TOK_AiDistillBuild, aip_distill_build },
    { GMB_TOK_AiDistillStatus, aip_distill_status },
    { GMB_TOK_AiDistillSample, aip_distill_sample },
    { GMB_TOK_AiDistillSampleN, aip_distill_samplen },
    { GMB_TOK_AiDistillSampleFail, aip_distill_samplefail },
    { GMB_TOK_AiDistillSampleDone, aip_distill_sampledone },
    { GMB_TOK_AiDistillEval, aip_distill_eval },
    { GMB_TOK_AiDistillEvalTok, aip_distill_evaltok },
    { GMB_TOK_AiDistillEvalPass, aip_distill_evalpass },
    { GMB_TOK_AiDistillEvalAcc, aip_distill_evalacc },
    { GMB_TOK_AiDistillApply, aip_distill_apply },
    { GMB_TOK_AiDistillRestore, aip_distill_restore },
    { GMB_TOK_AiDistillTrain, aip_distill_train },
    { GMB_TOK_AiDistillLoss, aip_distill_loss },
    { GMB_TOK_AiDistillHold, aip_distill_hold },
    { GMB_TOK_AiDistillCkpt, aip_distill_ckpt },
    { GMB_TOK_AiDistillDone, aip_distill_done },
    { GMB_TOK_AiFinetuneStatus, aip_ft_status },
    { GMB_TOK_LlmRunsJson, aip_runs_json },
    { GMB_TOK_LlmRunsCount, aip_runs_count },
    { GMB_TOK_LlmRunsClear, aip_runs_clear },
    { GMB_TOK_LlmTrainConfig, aip_train_config },
    { GMB_TOK_LlmTrainConfigSet, aip_train_config_set },
    { GMB_TOK_LlmRunsItem, aip_runs_item },
    { GMB_TOK_LlmDatasetStats, aip_dataset_stats },
    { GMB_TOK_LlmLoraList, aip_lora_list },
    { GMB_TOK_LlmTrainExport, aip_train_export },
    { GMB_TOK_LlmRunStop, aip_runs_stop },
    { GMB_TOK_LlmEvalGate, aip_evalgate },
    { GMB_TOK_LlmLoraSave, aip_lorasave },
    { GMB_TOK_LlmLoraLoad, aip_loraload },
    { GMB_TOK_AiFinetuneCkpt, aip_ft_ckpt },
    { GMB_TOK_AiFinetuneLoss, aip_ft_loss },
    { GMB_TOK_AiFinetuneHold, aip_ft_hold },
    { GMB_TOK_AiFinetuneDone, aip_ft_done },
    { GMB_TOK_AiFinetuneGreedy, aip_ft_greedy },
    { GMB_TOK_AiFinetuneGreedyAfter, aip_ft_greedy_after },
    { GMB_TOK_AiFinetuneDemoChanged, aip_ft_demo_changed },
    { GMB_TOK_AiFinetuneBaseTrain, aip_ft_base_train },
    { GMB_TOK_AiFinetuneBaseHold, aip_ft_base_hold },
};
int emit_bf_ai_provider(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(AIP_TBL) / sizeof(AIP_TBL[0]); i++)
        if (AIP_TBL[i].tk == tk) return AIP_TBL[i].sp(c, args, nargs, line);
    return 0;
}

/* ── dominio LLM/moestream (M14 MoE out-of-core, GGUF): LLM.STREAMSTATS →
 * gmb_moestream_streamstats_json (runtime condiviso, dlopen libgmb_ai_moestream). Ritorna la
 * stringa JSON (statement-expression). (Ripristinato 2026-09-25.) */
static int aimo_tokencount(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* B2 — LLM.TOKENCOUNT(model, testo) → llmmodelio_token_count (fonte unica llmtoken). */
    if (n < 2) { c99_fail(c, line, "LLM.TOKENCOUNT richiede (model, testo)"); return 1; }
    sb_put(&c->out, "(gmb_double_t)llmmodelio_token_count(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
    return 1;
}
static int aimo_llm_tokens(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* B2 — LLM.TOKENS(model, testo) → gmb_llm_tokens (runtime condiviso, ritorno come dato). */
    if (n < 2) { c99_fail(c, line, "LLM.TOKENS richiede (model, testo)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_tokens(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data), \"LLM.TOKENS\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_detokenize(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "LLM.DETOKENIZE richiede (model, ids)"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_detokenize(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data), \"LLM.DETOKENIZE\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_tensor_out(ctx_t* c, gmb_expr_t** a, int n, int line) {
    if (n < 2) { c99_fail(c, line, "LLM.TENSOROUT richiede (sub, a[, b[, c]])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_tensor_out(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", \"LLM.TENSOR\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_policy_out(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* LLM.POLICYOUT(sub[, a[, b[, c]]]) → gmb_llm_cmd_out (famiglia 1): output catturato. */
    if (n < 1) { c99_fail(c, line, "LLM.POLICY richiede almeno il sotto-verbo"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_cmd_out(1, ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", \"LLM.POLICYOUT\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_arch_out(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* LLM.ARCHOUT(sub[, a[, b[, c]]]) → gmb_llm_cmd_out (famiglia 2): output catturato. */
    if (n < 1) { c99_fail(c, line, "LLM.ARCH richiede almeno il sotto-verbo"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_cmd_out(2, ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", \"LLM.POLICYOUT\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_module_out(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* LLM.MODULEOUT(sub[, a[, b[, c]]]) → gmb_llm_cmd_out (famiglia 3): output catturato. */
    if (n < 1) { c99_fail(c, line, "LLM.MODULE richiede almeno il sotto-verbo"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_cmd_out(3, ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", \"LLM.POLICYOUT\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_kernel_out(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* LLM.KERNELOUT(sub[, a[, b[, c]]]) → gmb_llm_cmd_out (famiglia 4): output catturato. */
    if (n < 1) { c99_fail(c, line, "LLM.KERNEL richiede almeno il sotto-verbo"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_cmd_out(4, ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", \"LLM.POLICYOUT\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}
static int aimo_llm_kv_out(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* LLM.KVOUT(sub[, a[, b[, c]]]) → gmb_llm_cmd_out (famiglia 5): output catturato. */
    if (n < 1) { c99_fail(c, line, "LLM.KV richiede almeno il sotto-verbo"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_llm_out_or_raise(gmb_llm_cmd_out(5, ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_str_operand(c, a[1]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 4) { emit_str_operand(c, a[3]); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "\"\"");
    sb_put(&c->out, ", \"LLM.POLICYOUT\", ");
    { char lb[16]; snprintf(lb, sizeof lb, "%d", line); sb_put(&c->out, lb); }
    sb_put(&c->out, "))");
    return 1;
}static int aasr_transcribe(ctx_t* c, gmb_expr_t** a, int n, int line) {
    /* ASR.RECOGNIZE(model, wav[, outPath]) → gmb_asr_cmd (engine ASR nativo in gmb_runtime). Ritorna stringa. */
    if (n < 2) { c99_fail(c, line, "ASR.RECOGNIZE richiede (model, wav[, outPath])"); return 1; }
    sb_put(&c->out, "gmb_string_alloc(gmb_asr_cmd(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
    emit_str_operand(c, a[1]); sb_put(&c->out, ".data, ");
    /* Due parentesi: chiude gmb_asr_cmd( e gmb_string_alloc( (prima ne mancava una
     * → il C generato non compilava: "expected ')'" su ogni ASR.RECOGNIZE). */
    if (n >= 3) { emit_str_operand(c, a[2]); sb_put(&c->out, ".data))"); }
    else sb_put(&c->out, "\"\"))");
    return 1;
}
static int aimo_streamstats(ctx_t* c, gmb_expr_t** a, int n, int line) {    if (n != 1) return 0;
    sb_put(&c->out, "({ char __msbuf[2048]; if (gmb_moestream_streamstats_json(");
    emit_str_operand(c, a[0]); sb_put(&c->out, ".data");
    sb_printf(&c->out, ", __msbuf, sizeof(__msbuf)) != 0) gmb_raise_error(0, __msbuf, %d); gmb_string_alloc(__msbuf); })", line);
    return 1;
}
typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, int); } gmb_embedded_row;
static const gmb_embedded_row AIMO_TBL[] = {
    { GMB_TOK_AiStreamStats, aimo_streamstats },
    { GMB_TOK_LlmTokenCount, aimo_tokencount },
    { GMB_TOK_LlmTokens, aimo_llm_tokens },
    { GMB_TOK_LlmDetokenize, aimo_llm_detokenize },
    { GMB_TOK_LlmTensorValue, aimo_llm_tensor_out },
    { GMB_TOK_LlmPolicyOut, aimo_llm_policy_out },
    { GMB_TOK_LlmArchOut, aimo_llm_arch_out },
    { GMB_TOK_LlmModuleOut, aimo_llm_module_out },
    { GMB_TOK_LlmKernelOut, aimo_llm_kernel_out },
    { GMB_TOK_LlmKvOut, aimo_llm_kv_out },
    { GMB_TOK_AsrTranscribe, aasr_transcribe },
};
int emit_bf_ai_embedded(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(AIMO_TBL) / sizeof(AIMO_TBL[0]); i++)
        if (AIMO_TBL[i].tk == tk) return AIMO_TBL[i].sp(c, args, nargs, line);
    return 0;
}

/* ── dominio DATETIME (port gmb_dt_* — 2026-09-06) ── */
typedef struct { gmb_tokenkind_t tk; int (*h)(ctx_t*, gmb_expr_t**, int); } dt_row;
static int dt_now(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_dt_now()"); return 1; }
static int dt_today(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_dt_today()"); return 1; }
#define DT_P1(NAME, FN) static int dt_##NAME(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_dt_" FN "("); emit_expr(c, a[0]); sb_put(&c->out, ")"); return 1; }
DT_P1(day, "day")
DT_P1(month, "month")
DT_P1(year, "year")
DT_P1(hour, "hour")
DT_P1(minute, "minute")
DT_P1(second, "second")
DT_P1(weekday, "weekday")
#define DT_D1(NAME, FN) static int dt_##NAME(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "(gmb_double_t)gmb_dt_" FN "("); emit_expr(c, a[0]); sb_put(&c->out, ")"); return 1; }
#define DT_S1(NAME, FN) static int dt_##NAME(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "gmb_dt_" FN "("); emit_str_operand(c, a[0]); sb_put(&c->out, ")"); return 1; }
#define DT_SS1(NAME, FN) static int dt_##NAME(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "gmb_dt_" FN "("); emit_expr(c, a[0]); sb_put(&c->out, ")"); return 1; }
DT_D1(datevalue, "datevalue")
DT_D1(ctod, "ctod")
DT_D1(stod, "stod")
DT_SS1(dtoc, "dtoc")
DT_SS1(dtos, "dtos")
DT_SS1(cdow, "cdow")
DT_SS1(cmonth, "cmonth")
DT_D1(dow, "dow")
/* SECONDS(): secondi correnti (0..59), 0 argomenti — distinto da SECOND(date) (1 arg). */
static int dt_seconds(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "(gmb_double_t)gmb_dt_seconds()"); return 1; }
DT_D1(daysinmonth, "daysinmonth")
static int dt_isleapyear(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "(gmb_dt_isleapyear("); emit_expr(c, a[0]); sb_put(&c->out, ") ? -1.0 : 0.0)"); return 1; }
DT_D1(isoweek, "isoweek")
DT_D1(firstdayofmonth, "firstdayofmonth")
DT_D1(datefromunix, "datefromunix")
static int dt_isdate(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "(gmb_dt_isdate("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data) ? -1.0 : 0.0)"); return 1; }
static int dt_dateserial(ctx_t* c, gmb_expr_t** a, int n) { if (n != 3) return 0; sb_put(&c->out, "gmb_dt_dateserial((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "))"); return 1; }
static int dt_timeserial(ctx_t* c, gmb_expr_t** a, int n) { if (n != 3) return 0; sb_put(&c->out, "gmb_dt_timeserial((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "))"); return 1; }
static int dt_eomonth(ctx_t* c, gmb_expr_t** a, int n) { if (n < 1 || n > 2) return 0; sb_put(&c->out, "gmb_dt_eomonth("); emit_expr(c, a[0]); sb_put(&c->out, ", (int)("); if (n == 2) emit_expr(c, a[1]); else sb_put(&c->out, "0"); sb_put(&c->out, "))"); return 1; }
static int dt_dateadd(ctx_t* c, gmb_expr_t** a, int n) { if (n != 3) return 0; sb_put(&c->out, "gmb_dt_dateadd("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_expr(c, a[2]); sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ")"); return 1; }
static int dt_datediff(ctx_t* c, gmb_expr_t** a, int n) { if (n != 3) return 0; sb_put(&c->out, "gmb_dt_datediff("); emit_str_operand(c, a[0]); sb_put(&c->out, ".data, "); emit_expr(c, a[1]); sb_put(&c->out, ", "); emit_expr(c, a[2]); sb_put(&c->out, ")"); return 1; }
static int dt_unix(ctx_t* c, gmb_expr_t** a, int n) { if (n != 1) return 0; sb_put(&c->out, "(gmb_int_t)gmb_dt_unix("); emit_expr(c, a[0]); sb_put(&c->out, ", 1)"); return 1; }
static int dt_strtime(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_dt_timedollar(gmb_dt_now())"); return 1; }
/* Canone data/ora (UTC): DATE() = seriale giorno, STRDATE()/DATE$ = "dd/MM/yyyy". */
static int dt_dayserial(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_dt_dayserial()"); return 1; }
static int dt_strdate(ctx_t* c, gmb_expr_t** a, int n) { (void)a; if (n != 0) return 0; sb_put(&c->out, "gmb_dt_date_string()"); return 1; }

static const dt_row DT_TBL[] = {
    { GMB_TOK_Now, dt_now }, { GMB_TOK_Today, dt_today }, { GMB_TOK_Day, dt_day },
    { GMB_TOK_Month, dt_month }, { GMB_TOK_Year, dt_year }, { GMB_TOK_Hour, dt_hour },
    { GMB_TOK_Minute, dt_minute }, { GMB_TOK_Second, dt_second }, { GMB_TOK_Weekday, dt_weekday },
    { GMB_TOK_DateValue, dt_datevalue }, { GMB_TOK_Ctod, dt_ctod }, { GMB_TOK_Stod, dt_stod },
    { GMB_TOK_Dtoc, dt_dtoc }, { GMB_TOK_Dtos, dt_dtos }, { GMB_TOK_Cdow, dt_cdow },
    { GMB_TOK_Cmonth, dt_cmonth }, { GMB_TOK_Dow, dt_dow }, { GMB_TOK_Seconds, dt_seconds },
    { GMB_TOK_DaysInMonth, dt_daysinmonth }, { GMB_TOK_IsLeapYear, dt_isleapyear },
    { GMB_TOK_IsoWeekOfYear, dt_isoweek }, { GMB_TOK_FirstDayOfMonth, dt_firstdayofmonth },
    { GMB_TOK_DateFromUnix, dt_datefromunix }, { GMB_TOK_IsDate, dt_isdate },
    { GMB_TOK_DateSerial, dt_dateserial }, { GMB_TOK_TimeSerial, dt_timeserial },
    { GMB_TOK_Eomonth, dt_eomonth }, { GMB_TOK_DateAdd, dt_dateadd }, { GMB_TOK_DateDiff, dt_datediff },
    { GMB_TOK_UnixTimestamp, dt_unix }, { GMB_TOK_StrTime, dt_strtime },
    { GMB_TOK_Date, dt_dayserial }, { GMB_TOK_Time, dt_now }, { GMB_TOK_StrDate, dt_strdate },
};
int emit_bf_datetime(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(DT_TBL) / sizeof(DT_TBL[0]); i++)
        if (DT_TBL[i].tk == tk) return DT_TBL[i].h(c, args, nargs);
    return 0;
}
/* ── dominio: grep (delegazione §0bis — motore C condiviso gmb_grep) ── */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } gp_row;
static int gp_matches(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_grep_matches_str((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data)");
    return 1;
}
static int gp_files(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_grep_files_str((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data)");
    return 1;
}
static int gp_count(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_grep_count_path((");
    emit_str_operand(c, a[0]); sb_put(&c->out, ").data, (");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").data)");
    return 1;
}
static const gp_row GP_TBL[] = {
    { GMB_TOK_GrepMatches, gp_matches },
    { GMB_TOK_GrepFiles, gp_files },
    { GMB_TOK_GrepCount, gp_count },
};
int emit_bf_grep(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(GP_TBL) / sizeof(GP_TBL[0]); i++)
        if (GP_TBL[i].tk == tk) return GP_TBL[i].sp(c, args, nargs);
    return 0;
}
/* ── dominio: message (namespace MESSAGE — funzione MESSAGE.DELIVERY) ──
 * Contatori di consegna del bus: il testo arriva dal runtime C condiviso (§0bis)
 * gmb_msg_delivery_stats, la STESSA funzione usata dalla VM/C (gmb_bf_3815). Su C99Native
 * l'handle del bus è NUMERICO (MESSAGE CONNECT lo assegna), come su IL. */
typedef struct { gmb_tokenkind_t tk; ma_sp sp; } msg_row;
static int ms_delivery(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    /* statement-expression: il buffer vive nello scopo del chiamante, la stringa è gestita
     * dal runtime (gmb_string_alloc) — stesso schema di sx_* (ESCAPE) e gp_*. */
    sb_put(&c->out, "({ char __md[192]; gmb_msg_delivery_stats((int64_t)(");
    emit_expr(c, a[0]);
    sb_put(&c->out, "), __md, sizeof(__md)); gmb_string_alloc(__md); })");
    return 1;
}
static const msg_row MSG_TBL[] = {
    { GMB_TOK_MessageDelivery, ms_delivery },
};
int emit_bf_message(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    for (size_t i = 0; i < sizeof(MSG_TBL) / sizeof(MSG_TBL[0]); i++)
        if (MSG_TBL[i].tk == tk) return MSG_TBL[i].sp(c, args, nargs);
    return 0;
}
