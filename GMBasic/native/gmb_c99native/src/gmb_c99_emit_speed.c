/* gmb_c99_emit_speed.c — emitter C99 nativo del dominio SPEED (§0quater).
 * Statement legacy a spazio (OPEN/USE/PUT/GET/GO/SKIP/SET FILTER/SET ORDER/CLOSE/
 * SELECT/CREATE INDEX/DROP INDEX) → chiamate dirette al runtime C condiviso
 * gmb_db_speed_* (gmb_db_c, §0bis): MAI logica inline. Parità emitter C# Db.cs
 * (SpeedOpenStmt/SpeedUseStmt/SpeedPutStmt/SpeedGetStmt/SpeedGoStmt/...).
 * SEEK/DELETE/VIEWS/RELATIONS/SETMANY: errore esplicito (follow-up, mai stub).
 * Usa l'area corrente (gmb_db_current_speed, impostata da OPEN come ISAM/DBF).
 * Chiamato da st_domain (gmb_c99_emit.c) su win_tk == GMB_TOK_Speed. */
#include "gmb_c99_emit_internal.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* valore dopo una keyword negli args: KEY <val> → <val> (come ident o expr). */
static gmb_expr_t* sp_after_kw(gmb_expr_t** a, int na, const char* kw) {
    int i;
    for (i = 0; i + 1 < na; i++) {
        const char* nm = dom_ident(a[i]);
        if (nm && strcasecmp(nm, kw) == 0) return a[i + 1];
    }
    return NULL;
}

/* nome dopo una keyword (ALIAS/OF/ON/...): NULL se assente. */
static const char* sp_after_kw_name(gmb_expr_t** a, int na, const char* kw) {
    gmb_expr_t* e = sp_after_kw(a, na, kw);
    const char* nm = e ? dom_ident(e) : NULL;
    return (nm && nm[0]) ? nm : NULL;
}

/* nome da espressione: stringa letterale O identificatore (nomi indice/vista possono
 * essere scritti come "Nome" o Nome). */
static const char* sp_name(gmb_expr_t* e) {
    if (!e) return NULL;
    if (e->kind == GMB_EXPR_STR && e->str) return e->str;
    return dom_ident(e);
}

/* Risolve la variabile UDT (bagai ISAM GET): cerca nei locals, poi nei globals. */
static struct cls_info* sp_rec_class(ctx_t* c, const char* vn) {
    var_t* gv = NULL;
    if (vn && vn[0]) {
        if (c->cur_fn) gv = vars_find(&c->cur_fn->locals, vn);
        if (!gv) gv = vars_find(&c->globals, vn);
    }
    if (gv && gv->vclass[0]) return cls_find(c, gv->vclass);
    /* Locale di TIPO (UDT/TYPE) — es. `DIM c AS Cliente` con Cliente = TYPE: il DIM non
     * popola vclass (riservato alle CLASSI) ma registra il tipo UDT nella mappa dei
     * nome-tipo → recuperalo da lì. Senza, `SPEED PUT tabella, c` dava "record 'c' non
     * risolvibile come UDT" (DbVmMarshallerSpeed/DbSpeedFullTests). */
    { char ut[64]; ut[0] = 0;
      if (gmb_udt_var_type(c, vn, ut, (int)sizeof ut) && ut[0]) return cls_find(c, ut); }
    return NULL;
}

/* SPEED OPEN "dir" [ALIAS x] → gmb_db_speed_open (imposta la corrente, come ISAM). */
static void sp_open(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    if (na < 1) { c99_fail(c, s->line, "SPEED OPEN: atteso il path"); return; }
    ind(c); sb_put(&c->out, "{ gmb_string_t _sp_p = ");
    emit_str_operand(c, a[0]); sb_put(&c->out, ";\n");
    ind(c); sb_put(&c->out, "(void)gmb_db_speed_open(_sp_p.data ? _sp_p.data : \"\");\n");
    ind(c); sb_put(&c->out, "}\n");
}

/* SPEED USE "tab" FROM db OF Tipo ALIAS T [INDEXED BY f1, f2]
 * (parity VMC gmb_sys_GMB_SYS_SPEED_USE: crea la tabella con l'ALIAS, id reale,
 * registra alias→id, use. Senza tabella nel Manifest i cursori falliscono.) */
static void sp_use(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* tn = sp_after_kw_name(a, na, "OF");
    const char* an = sp_after_kw_name(a, na, "ALIAS");
    char defs[2048];
    int i;
    if (!tn || !tn[0]) { c99_fail(c, s->line, "SPEED USE: clausola OF <tipo> mancante"); return; }
    if (!an || !an[0]) {
        /* fallback: nome tabella (primo string literal) */
        an = NULL;
        for (i = 0; i < na; i++)
            if (a[i] && a[i]->kind == GMB_EXPR_STR && a[i]->str && a[i]->str[0]) { an = a[i]->str; break; }
        if (!an) { c99_fail(c, s->line, "SPEED USE: clausola ALIAS <nome> mancante"); return; }
    }
    dom_schema_defs(c, tn, defs, (int)sizeof(defs));
    if (!defs[0]) { c99_fail(c, s->line, "SPEED USE: tipo '%s' senza campi", tn); return; }
    ind(c); sb_printf(&c->out, "{ int _sp_tid = gmb_db_speed_create_table(gmb_db_current_speed(), \"%s\", \"%s\");\n", an, defs);
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_register_table_alias(\"%s\", _sp_tid);\n", an);
    ind(c); sb_put(&c->out, "(void)gmb_db_speed_use(gmb_db_current_speed(), _sp_tid);\n");
    /* INDEXED BY f1, f2 → un indice per campo (come emitter C# CreateIndex). */
    for (i = 0; i < na; i++) {
        const char* nm = dom_ident(a[i]);
        if (nm && strcasecmp(nm, "INDEXED") == 0) {
            int j;
            for (j = i + 1; j < na; j++) {
                const char* fn = dom_ident(a[j]);
                if (!fn || !fn[0]) continue;
                if (strcasecmp(fn, "BY") == 0 || strcasecmp(fn, "ON") == 0 ||
                    strcasecmp(fn, "TO") == 0 || strcasecmp(fn, "ALIAS") == 0) continue;
                ind(c); sb_printf(&c->out, "(void)gmb_db_speed_create_index(gmb_db_current_speed(), _sp_tid, \"%s_%s\", \"%s\", 0);\n", an, fn, fn);
            }
            break;
        }
    }
    ind(c); sb_put(&c->out, "}\n");
}

/* SPEED PUT T, rec — rec UDT → JSON {campo:valore} → gmb_db_speed_set.
 * Formato JSON (parità VMC/C#: la PUT VM/C scrive gmb_dict_to_json e il filtro
 * speed_filter_get ri-parsa JSON). Chiave = primo campo (pk tipico), come VMC. */
static void sp_put(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* an, *vn;
    struct cls_info* ci;
    int i;
    if (na < 2) { c99_fail(c, s->line, "SPEED PUT: attesi alias e record (PUT alias, rec)"); return; }
    an = dom_ident(a[0]);
    if (!an || !an[0]) { c99_fail(c, s->line, "SPEED PUT: alias mancante"); return; }
    /* record letterale {k: v, ...} → _dict_(k1,v1,...): serializza a JSON (stessa forma
     * della PUT da UDT). Prima l'emitter C# lo saltava in silenzio (stub) e C99Native
     * dava errore: i record letterali non venivano MAI inseriti. */
    {
        gmb_expr_t* rec = a[na - 1];
        if (rec && rec->kind == GMB_EXPR_CALL && rec->var && strcmp(rec->var, "_dict_") == 0) {
            int np = rec->nargs / 2, j;
            if (np < 1) { c99_fail(c, s->line, "SPEED PUT: record letterale vuoto"); return; }
            ind(c); sb_put(&c->out, "{ gmb_dict_t _sp_d = gmb_dict_new();\n");
            for (j = 0; j < np; j++) {
                gmb_expr_t* ke = rec->args[j * 2];
                gmb_expr_t* ve = rec->args[j * 2 + 1];
                const char* kn = dom_ident(ke);
                if (!kn && ke && ke->kind == GMB_EXPR_STR) kn = ke->str;
                ind(c); sb_printf(&c->out, "gmb_dict_set(&_sp_d, \"%s\", ", kn ? kn : "");
                if (ve && ve->kind == GMB_EXPR_STR) {
                    sb_put(&c->out, "gmb_make_val_string("); emit_str_operand(c, ve);
                    sb_put(&c->out, ".data ? "); emit_str_operand(c, ve); sb_put(&c->out, ".data : \"\"));\n");
                } else {
                    sb_put(&c->out, "gmb_make_val_double((gmb_double_t)("); emit_expr(c, ve); sb_put(&c->out, ")));\n");
                }
            }
            {
                gmb_expr_t* fv = rec->args[1];
                ind(c); sb_put(&c->out, "char _sp_k[64];\n");
                ind(c);
                if (fv && fv->kind == GMB_EXPR_STR) {
                    sb_put(&c->out, "snprintf(_sp_k, sizeof(_sp_k), \"%s\", ");
                    emit_str_operand(c, fv); sb_put(&c->out, ".data ? "); emit_str_operand(c, fv); sb_put(&c->out, ".data : \"\");\n");
                } else {
                    sb_put(&c->out, "snprintf(_sp_k, sizeof(_sp_k), \"%g\", (double)("); emit_expr(c, fv); sb_put(&c->out, "));\n");
                }
            }
            ind(c); sb_put(&c->out, "gmb_string_t _sp_js = gmb_dict_to_json(&_sp_d);\n");
            ind(c); sb_printf(&c->out, "(void)gmb_db_speed_set(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), _sp_k, (int)strlen(_sp_k), (const uint8_t*)_sp_js.data, (int)_sp_js.length);\n", an);
            ind(c); sb_put(&c->out, "gmb_string_free(&_sp_js); gmb_dict_free(&_sp_d); }\n");
            return;
        }
    }
    vn = NULL;
    for (i = na - 1; i >= 0; i--) { const char* nm = dom_ident(a[i]); if (nm && nm[0]) { vn = nm; break; } }
    if (!vn || !vn[0]) { c99_fail(c, s->line, "SPEED PUT: record non risolto"); return; }
    ci = sp_rec_class(c, vn);
    if (!ci) { c99_fail(c, s->line, "SPEED PUT: record '%s' non risolvibile come UDT", vn); return; }
    ind(c); sb_put(&c->out, "{ gmb_dict_t _sp_d = gmb_dict_new();\n");
    for (i = 0; i < ci->nfields; i++) {
        ind(c); sb_printf(&c->out, "gmb_dict_set(&_sp_d, \"%s\", ", ci->fname[i]);
        if (cls_field_is_str(ci, ci->fname[i])) {
            sb_put(&c->out, "gmb_make_val_string(");
            sb_cls_field(&c->out, ci, vn, ci->fname[i]);
            sb_put(&c->out, ".data ? ");
            sb_cls_field(&c->out, ci, vn, ci->fname[i]);
            sb_put(&c->out, ".data : \"\"));\n");
        } else {
            sb_put(&c->out, "gmb_make_val_double((gmb_double_t)(");
            sb_cls_field(&c->out, ci, vn, ci->fname[i]);
            sb_put(&c->out, ")));\n");
        }
    }
    /* chiave record = primo campo (pk tipico), come VMC (INDEXED BY / fallback ID). */
    ind(c); sb_put(&c->out, "char _sp_k[64];\n");
    ind(c);
    if (cls_field_is_str(ci, ci->fname[0])) {
        sb_put(&c->out, "snprintf(_sp_k, sizeof(_sp_k), \"%s\", ");
        sb_cls_field(&c->out, ci, vn, ci->fname[0]);
        sb_put(&c->out, ".data ? ");
        sb_cls_field(&c->out, ci, vn, ci->fname[0]);
        sb_put(&c->out, ".data : \"\");\n");
    } else {
        sb_put(&c->out, "snprintf(_sp_k, sizeof(_sp_k), \"%g\", (double)(");
        sb_cls_field(&c->out, ci, vn, ci->fname[0]);
        sb_put(&c->out, "));\n");
    }
    ind(c); sb_put(&c->out, "gmb_string_t _sp_js = gmb_dict_to_json(&_sp_d);\n");
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_set(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), _sp_k, (int)strlen(_sp_k), (const uint8_t*)_sp_js.data, (int)_sp_js.length);\n", an);
    ind(c); sb_put(&c->out, "gmb_string_free(&_sp_js); gmb_dict_free(&_sp_d); }\n");
}

/* SPEED GET T, rec — get_current (capacità!) + parse JSON nei campi UDT.
 * Il formato è JSON (parità VMC/C#). NOTA ABI: gmb_db_speed_get_current copia SOLO se
 * *vlen (capacità in ingresso) >= lunghezza reale → va inizializzata alla dimensione. */
static void sp_get(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* an, *vn;
    struct cls_info* ci;
    int i;
    if (na < 2) { c99_fail(c, s->line, "SPEED GET: attesi alias e variabile (GET alias, rec)"); return; }
    an = dom_ident(a[0]);
    vn = NULL;
    for (i = na - 1; i >= 0; i--) { const char* nm = dom_ident(a[i]); if (nm && nm[0]) { vn = nm; break; } }
    if (!an || !an[0] || !vn || !vn[0]) { c99_fail(c, s->line, "SPEED GET: alias/variabile non risolti"); return; }
    ci = sp_rec_class(c, vn);
    if (!ci) { c99_fail(c, s->line, "SPEED GET: record '%s' non risolvibile come UDT", vn); return; }
    ind(c); sb_put(&c->out, "{ char _sp_g[16384]; int _sp_glen = (int)sizeof(_sp_g);\n");
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_get_current(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), _sp_g, &_sp_glen);\n", an);
    ind(c); sb_put(&c->out, "if (_sp_glen < 0) _sp_glen = 0; if (_sp_glen >= (int)sizeof(_sp_g)) _sp_glen = (int)sizeof(_sp_g) - 1; _sp_g[_sp_glen] = 0;\n");
    ind(c); sb_put(&c->out, "gmb_dict_t _sp_d = gmb_dict_new(); gmb_json_parse_to_dict(_sp_g, &_sp_d);\n");
    for (i = 0; i < ci->nfields; i++) {
        ind(c); sb_printf(&c->out, "{ gmb_val_t _sp_v = gmb_dict_get(_sp_d, \"%s\"); ", ci->fname[i]);
        sb_cls_field(&c->out, ci, vn, ci->fname[i]);
        if (cls_field_is_str(ci, ci->fname[i]))
            sb_put(&c->out, " = gmb_string_alloc(_sp_v.kind == GMB_TYPE_STRING && _sp_v.v_string.data ? _sp_v.v_string.data : \"\");");
        else
            sb_put(&c->out, " = (gmb_int_t)gmb_val_to_double(_sp_v);");
        sb_put(&c->out, " }\n");
    }
    ind(c); sb_put(&c->out, "gmb_dict_free(&_sp_d); }\n");
}

/* SPEED GO T, TOP|BOTTOM (parity emitter C#: TOP; BOTTOM come VMC). */
static void sp_go(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* an = (na >= 1) ? dom_ident(a[0]) : NULL;
    const char* loc = NULL;
    int i;
    if (!an || !an[0]) { c99_fail(c, s->line, "SPEED GO: alias mancante"); return; }
    for (i = na - 1; i >= 0; i--) {
        const char* nm = dom_ident(a[i]);
        if (nm && (strcasecmp(nm, "TOP") == 0 || strcasecmp(nm, "BOTTOM") == 0)) { loc = nm; break; }
    }
    ind(c);
    if (loc && strcasecmp(loc, "BOTTOM") == 0)
        sb_printf(&c->out, "(void)gmb_db_speed_go_bottom(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"));\n", an);
    else
        sb_printf(&c->out, "(void)gmb_db_speed_go_top(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"));\n", an);
}

/* SPEED SKIP T [, n] → skip (default 1, come ISAM). */
static void sp_skip(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* an = (na >= 1) ? dom_ident(a[0]) : NULL;
    if (!an || !an[0]) { c99_fail(c, s->line, "SPEED SKIP: alias mancante"); return; }
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_skip(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), (int)(", an);
    if (na >= 2) emit_expr(c, a[1]); else sb_put(&c->out, "1");
    sb_put(&c->out, "));\n");
}

/* SPEED SET FILTER TO <cond> ON <alias> / SET ORDER TO <idx> ON <alias>
 * (parity ISAM: act_tk=Set, primo arg ORDER/FILTER). */
static void sp_set(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* sub = (na >= 1) ? dom_ident(a[0]) : NULL;
    const char* an = sp_after_kw_name(a, na, "ON");
    if (sub && strcasecmp(sub, "FILTER") == 0) {
        if (na < 3) { c99_fail(c, s->line, "SPEED SET FILTER: atteso TO <condizione> ON <alias>"); return; }
        if (!an || !an[0]) { c99_fail(c, s->line, "SPEED SET FILTER: clausola ON <alias> mancante"); return; }
        ind(c); sb_put(&c->out, "(void)gmb_db_speed_set_filter(gmb_db_current_speed(), (");
        emit_str_operand(c, a[2]); sb_put(&c->out, ").data);\n");
        /* parity emitter C#: dopo il filtro si riposiziona in testa. */
        ind(c); sb_printf(&c->out, "(void)gmb_db_speed_go_top(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"));\n", an);
        return;
    }
    if (sub && strcasecmp(sub, "ORDER") == 0) {
        if (na < 3) { c99_fail(c, s->line, "SPEED SET ORDER: atteso TO <indice> ON <alias>"); return; }
        if (!an || !an[0]) { c99_fail(c, s->line, "SPEED SET ORDER: clausola ON <alias> mancante"); return; }
        ind(c); sb_printf(&c->out, "(void)gmb_db_speed_set_order(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), (", an);
        emit_str_operand(c, a[2]); sb_put(&c->out, ").data);\n");
        return;
    }
    if (sub && strcasecmp(sub, "RELATION") == 0) {
        /* SET RELATION ON <parent> [TO <key> INTO <child>]; senza INTO rimuove. */
        const char* parent = sp_after_kw_name(a, na, "ON");
        const char* child = sp_after_kw_name(a, na, "INTO");
        if (!parent || !parent[0]) {
            c99_fail(c, s->line, "SPEED SET RELATION: atteso SET RELATION ON <padre> [TO <key> INTO <figlio>]");
            return;
        }
        ind(c); sb_printf(&c->out, "(void)gmb_db_speed_set_relation_table(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), ", parent);
        if (child && child[0]) sb_printf(&c->out, "gmb_db_speed_table_by_alias(\"%s\")", child);
        else sb_put(&c->out, "-1");
        sb_put(&c->out, ");\n");
        return;
    }
    c99_fail(c, s->line, "SPEED SET %s: non supportato dall'emitter nativo", sub ? sub : "?");
}

/* SPEED CLOSE → chiude l'area corrente. */
static void sp_close(ctx_t* c, gmb_stmt_t* s) {
    (void)s;
    ind(c); sb_put(&c->out, "(void)gmb_db_speed_close(gmb_db_current_speed());\n");
}

/* ── SPEED SELECT (query engine JSON condiviso, gmb_db_speed_select) ──────────────── */

/* Indice del primo argomento, da `from`, che è la keyword <kw> (-1 se assente). */
static int sp_kw_idx(gmb_expr_t** a, int na, const char* kw, int from) {
    int i;
    for (i = from; i < na; i++) { const char* nm = dom_ident(a[i]); if (nm && strcasecmp(nm, kw) == 0) return i; }
    return -1;
}

/* Ricostruisce il testo sorgente di un'espressione (per il piano JSON del motore). */
static void sp_expr_text(sb_t* b, gmb_expr_t* e) {
    int i;
    if (!e) return;
    switch (e->kind) {
        case GMB_EXPR_NUM: { char n[64]; snprintf(n, sizeof(n), "%g", e->num); sb_put(b, n); break; }
        case GMB_EXPR_STR: sb_put(b, "\""); if (e->str) sb_put(b, e->str); sb_put(b, "\""); break;
        case GMB_EXPR_VAR: if (e->var) sb_put(b, e->var); break;
        case GMB_EXPR_BINARY:
            sp_expr_text(b, e->left); if (e->op) sb_printf(b, " %s ", e->op); sp_expr_text(b, e->right);
            break;
        case GMB_EXPR_UNARY:
            if (e->op) sb_put(b, e->op); sp_expr_text(b, e->left);
            break;
        case GMB_EXPR_CALL:
            if (e->var) sb_put(b, e->var);
            sb_put(b, "(");
            for (i = 0; i < e->nargs; i++) { if (i) sb_put(b, ","); sp_expr_text(b, e->args[i]); }
            sb_put(b, ")");
            break;
        default: break;
    }
}

/* Testo di un argomento (termine singolo): VAR/numero/stringa. */
static void sp_arg_text(sb_t* b, gmb_expr_t* e) {
    if (!e) return;
    if (e->kind == GMB_EXPR_STR) { sb_put(b, "\""); if (e->str) sb_put(b, e->str); sb_put(b, "\""); }
    else sp_expr_text(b, e);
}

/* Concatena gli argomenti [lo, hi) nel testo dell'espressione (es. SUM ( Ordini.Importo ) →
 * "SUM(Ordini.Importo)"). */
static void sp_args_text(sb_t* b, gmb_expr_t** a, int lo, int hi) {
    int i;
    for (i = lo; i < hi; i++) sp_arg_text(b, a[i]);
}

/* Scrive una stringa JSON con escape (per i valori del piano). */
static void sp_json_str(sb_t* b, const char* s) {
    sb_put(b, "\"");
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') { char e[3] = { '\\', *s, 0 }; sb_put(b, e); }
        else sb_put(b, (char[2]){ *s, 0 });
    }
    sb_put(b, "\"");
}

/* SPEED SELECT <campi> INTO <arr>() OF <tipo> FROM <a> [JOIN <b> ON <cond>] [WHERE <c>]
 * [GROUP BY <g>] [HAVING <h>] [ORDER BY <o>] — esegue gmb_db_speed_select e riempie l'array
 * UDT di destinazione (parity PCode SpeedExecuteSelect). */
static void sp_select(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    int iInto, iOf, iFrom, iJoin, iOn, iWhere, iGroup, iBy, iHaving, iOrder, iOrderBy;
    int i;
    const char* an;
    const char* arrName;
    const char* typeName;
    struct cls_info* ci;
    sb_t plan;
    char planEsc[8192];
    int pe = 0;

    iInto = sp_kw_idx(a, na, "INTO", 0);
    iFrom = sp_kw_idx(a, na, "FROM", 0);
    if (iFrom < 0) { c99_fail(c, s->line, "SPEED SELECT: clausola FROM <alias> mancante"); return; }
    an = (iFrom + 1 < na) ? dom_ident(a[iFrom + 1]) : NULL;
    if (!an || !an[0]) { c99_fail(c, s->line, "SPEED SELECT: FROM <alias> non risolto"); return; }
    iOf = (iInto >= 0) ? sp_kw_idx(a, na, "OF", iInto) : -1;
    arrName = (iInto >= 0 && iInto + 1 < na) ? dom_ident(a[iInto + 1]) : NULL;
    typeName = (iOf >= 0 && iOf + 1 < na) ? dom_ident(a[iOf + 1]) : NULL;
    if (!arrName || !arrName[0]) { c99_fail(c, s->line, "SPEED SELECT: INTO <array>() mancante"); return; }
    if (!typeName || !typeName[0]) { c99_fail(c, s->line, "SPEED SELECT: OF <tipo> mancante"); return; }
    ci = cls_find(c, typeName);
    if (!ci) { c99_fail(c, s->line, "SPEED SELECT: tipo '%s' non trovato", typeName); return; }

    iJoin = sp_kw_idx(a, na, "JOIN", iFrom);
    iOn = (iJoin >= 0) ? sp_kw_idx(a, na, "ON", iJoin) : -1;
    iWhere = sp_kw_idx(a, na, "WHERE", iFrom);
    iGroup = sp_kw_idx(a, na, "GROUP", iFrom);
    iBy = (iGroup >= 0) ? sp_kw_idx(a, na, "BY", iGroup) : -1;
    iHaving = sp_kw_idx(a, na, "HAVING", iFrom);
    iOrder = sp_kw_idx(a, na, "ORDER", iFrom);
    iOrderBy = (iOrder >= 0) ? sp_kw_idx(a, na, "BY", iOrder) : -1;

    /* costruisce il piano JSON in un buffer sb_t */
    memset(&plan, 0, sizeof(plan));
    sb_put(&plan, "{");
    sb_put(&plan, "\"from\":"); sp_json_str(&plan, an);
    if (iJoin >= 0 && iJoin + 1 < na) {
        const char* jn = dom_ident(a[iJoin + 1]);
        sb_put(&plan, ",\"join\":"); sp_json_str(&plan, jn ? jn : "");
        if (iOn >= 0) {
            int hi = na;
            if (iWhere >= 0 && iWhere < hi) hi = iWhere;
            if (iGroup >= 0 && iGroup < hi) hi = iGroup;
            if (iOrder >= 0 && iOrder < hi) hi = iOrder;
            if (iHaving >= 0 && iHaving < hi) hi = iHaving;
            sb_put(&plan, ",\"joinCond\":");
            { sb_t t; memset(&t, 0, sizeof(t)); sp_args_text(&t, a, iOn + 1, hi); sp_json_str(&plan, t.s ? t.s : ""); free(t.s); }
        }
    }
    if (iWhere >= 0) {
        int hi = na;
        if (iGroup >= 0 && iGroup < hi) hi = iGroup;
        if (iOrder >= 0 && iOrder < hi) hi = iOrder;
        if (iHaving >= 0 && iHaving < hi) hi = iHaving;
        sb_put(&plan, ",\"where\":");
        { sb_t t; memset(&t, 0, sizeof(t)); sp_args_text(&t, a, iWhere + 1, hi); sp_json_str(&plan, t.s ? t.s : ""); free(t.s); }
    }
    if (iGroup >= 0 && iBy >= 0) {
        int hi = na;
        if (iOrder >= 0 && iOrder < hi) hi = iOrder;
        if (iHaving >= 0 && iHaving < hi) hi = iHaving;
        sb_put(&plan, ",\"groupBy\":[");
        { sb_t t; memset(&t, 0, sizeof(t)); sp_args_text(&t, a, iBy + 1, hi); sp_json_str(&plan, t.s ? t.s : ""); free(t.s); }
        sb_put(&plan, "]");
    }
    if (iHaving >= 0) {
        int hi = na;
        if (iOrder >= 0 && iOrder < hi) hi = iOrder;
        sb_put(&plan, ",\"having\":");
        { sb_t t; memset(&t, 0, sizeof(t)); sp_args_text(&t, a, iHaving + 1, hi); sp_json_str(&plan, t.s ? t.s : ""); free(t.s); }
    }
    if (iOrder >= 0 && iOrderBy >= 0) {
        int hi = na;
        sb_put(&plan, ",\"orderBy\":[");
        { sb_t t; memset(&t, 0, sizeof(t)); sp_args_text(&t, a, iOrderBy + 1, hi); sp_json_str(&plan, t.s ? t.s : ""); free(t.s); }
        sb_put(&plan, "]");
    }
    /* select: campi [expr] [AS alias], separati da AS (le virgole non sopravvivono al parser) */
    {
        int hi = (iInto >= 0) ? iInto : na;
        int star = (hi == 1 && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var && strcmp(a[0]->var, "*") == 0);
        sb_put(&plan, ",\"star\":");
        sb_put(&plan, star ? "true" : "false");
        sb_put(&plan, ",\"selE\":[");
        {
            int lo = 0, first = 1;
            for (i = 0; i <= hi; i++) {
                int isAs = (i < hi && a[i] && a[i]->kind == GMB_EXPR_VAR && a[i]->var && strcasecmp(a[i]->var, "AS") == 0);
                if (isAs || (i == hi && lo < hi)) {
                    sb_t t; memset(&t, 0, sizeof(t));
                    sp_args_text(&t, a, lo, i);
                    if (!first) sb_put(&plan, ",");
                    sp_json_str(&plan, t.s ? t.s : "");
                    free(t.s);
                    first = 0;
                    lo = (isAs && i + 1 < hi) ? i + 2 : i + 1;
                    if (isAs) i++;
                }
            }
        }
        sb_put(&plan, "],\"selA\":[");
        {
            int lo = 0, first = 1;
            for (i = 0; i <= hi; i++) {
                int isAs = (i < hi && a[i] && a[i]->kind == GMB_EXPR_VAR && a[i]->var && strcasecmp(a[i]->var, "AS") == 0);
                if (isAs) {
                    if (!first) sb_put(&plan, ",");
                    sp_json_str(&plan, (i + 1 < hi) ? (dom_ident(a[i + 1]) ? dom_ident(a[i + 1]) : "") : "");
                    first = 0;
                }
            }
        }
        sb_put(&plan, "]");
    }
    sb_put(&plan, "}");

    /* escape per il letterale C */
    for (i = 0; plan.s && plan.s[i] && pe < (int)sizeof(planEsc) - 3; i++) {
        char ch = plan.s[i];
        if (ch == '"' || ch == '\\') { planEsc[pe++] = '\\'; planEsc[pe++] = ch; }
        else if (ch == '\n') { planEsc[pe++] = '\\'; planEsc[pe++] = 'n'; }
        else planEsc[pe++] = ch;
    }
    planEsc[pe] = 0;
    free(plan.s);

    ind(c); sb_put(&c->out, "{ gmb_string_t _sp_js = gmb_string_alloc(\"\");\n");
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_select(gmb_db_current_speed(), \"%s\", &_sp_js);\n", planEsc);
    ind(c); sb_put(&c->out, "gmb_val_t _sp_arr = gmb_json_parse(_sp_js.data ? _sp_js.data : \"[]\");\n");
    ind(c); sb_put(&c->out, "gmb_list_t _sp_l = gmb_val_as_list(_sp_arr);\n");
    ind(c); sb_printf(&c->out, "%s_len = (gmb_int_t)_sp_l.count;\n", arrName);
    ind(c); sb_printf(&c->out, "%s = (gmb_val_t*)realloc(%s, (size_t)(_sp_l.count > 0 ? _sp_l.count : 1) * sizeof(gmb_val_t));\n", arrName, arrName);
    ind(c); sb_put(&c->out, "{ size_t _sp_i; for (_sp_i = 0; _sp_i < _sp_l.count; _sp_i++) {\n");
    ind(c); sb_put(&c->out, "  gmb_dict_t* _sp_rd = (gmb_dict_t*)(((gmb_obj_t*)_sp_l.items[_sp_i].v_ptr)->data);\n");
    ind(c); sb_printf(&c->out, "  %s[_sp_i] = gmb_make_val_obj(gmb_obj_new_typed(calloc(1, sizeof(gmb_obj_%s)), \"%s\"));\n", arrName, ci->name, ci->name);
    {
        char base[128];
        snprintf(base, sizeof(base), "%s[_sp_i]", arrName);
        for (i = 0; i < ci->nfields; i++) {
            ind(c); sb_printf(&c->out, "  { gmb_val_t _sp_v = gmb_dict_get(*_sp_rd, \"%s\"); ", ci->fname[i]);
            sb_cls_field(&c->out, ci, base, ci->fname[i]);
            if (cls_field_is_str(ci, ci->fname[i]))
                sb_put(&c->out, " = gmb_string_alloc(_sp_v.kind == GMB_TYPE_STRING && _sp_v.v_string.data ? _sp_v.v_string.data : \"\");");
            else
                sb_put(&c->out, " = (gmb_int_t)gmb_val_to_double(_sp_v);");
            sb_put(&c->out, " }\n");
        }
    }
    ind(c); sb_put(&c->out, "} }\n");
    ind(c); sb_put(&c->out, "gmb_string_free(&_sp_js); }\n");
}

/* SPEED CREATE INDEX <n> ON <a> KEY (<k>) / DROP INDEX <n> ON <a>. */
static void sp_create(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* sub = (na >= 1) ? dom_ident(a[0]) : NULL;
    const char* nm, *an, *key;
    if (sub && strcasecmp(sub, "VIEW") == 0) {
        /* SPEED CREATE VIEW <nome> OF <tipo> AS ... → gmb_db_speed_create_view. */
        const char* vn = (na >= 2) ? sp_name(a[1]) : NULL;
        if (!vn || !vn[0]) { c99_fail(c, s->line, "SPEED CREATE VIEW: atteso il nome vista"); return; }
        ind(c); sb_printf(&c->out, "(void)gmb_db_speed_create_view(\"%s\", gmb_db_speed_curtable(gmb_db_current_speed()));\n", vn);
        return;
    }
    if (!sub || strcasecmp(sub, "INDEX") != 0) { c99_fail(c, s->line, "SPEED CREATE: solo CREATE INDEX/VIEW supportato"); return; }
    nm = (na >= 2) ? sp_name(a[1]) : NULL;
    an = sp_after_kw_name(a, na, "ON");
    /* KEY (campo): il parser domain produce per `KEY (...)` un unico arg CALL di nome KEY
     * con il campo come sotto-arg (non i token separati KEY,"(",campo,")" come ISAM).
     * La forma senza parentesi (KEY campo) resta valida (flat). Fix 2026-09-22:
     * DbSpeedFullTests.bas:203 usa KEY (Citta) e veniva rifiutato (MiniTest-Oracolo §0.47). */
    key = NULL;
    for (int ki = 0; ki < na; ki++) {
        gmb_expr_t* ke = a[ki];
        const char* kw = dom_ident(ke);
        if (kw && strcasecmp(kw, "KEY") == 0) {
            if (ke->kind == GMB_EXPR_CALL && ke->nargs >= 1) {
                key = dom_ident(ke->args[0]);
            } else if (ki + 1 < na) {
                int kj = ki + 1;
                const char* op = dom_ident(a[kj]);
                if (op && strcmp(op, "(") == 0) kj++;
                if (kj < na) key = dom_ident(a[kj]);
            }
            break;
        }
    }
    if (!nm || !nm[0] || !an || !an[0] || !key || !key[0]) {
        c99_fail(c, s->line, "SPEED CREATE INDEX: atteso CREATE INDEX <nome> ON <alias> KEY (<campo>)");
        return;
    }
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_create_index(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), \"%s\", \"%s\", 0);\n", an, nm, key);
}

static void sp_drop(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* sub = (na >= 1) ? dom_ident(a[0]) : NULL;
    const char* nm, *an;
    if (sub && strcasecmp(sub, "VIEW") == 0) {
        const char* vn = (na >= 2) ? sp_name(a[1]) : NULL;
        if (!vn || !vn[0]) { c99_fail(c, s->line, "SPEED DROP VIEW: atteso il nome vista"); return; }
        ind(c); sb_printf(&c->out, "(void)gmb_db_speed_drop_view(\"%s\");\n", vn);
        return;
    }
    if (!sub || strcasecmp(sub, "INDEX") != 0) { c99_fail(c, s->line, "SPEED DROP: solo DROP INDEX/VIEW supportato"); return; }
    nm = (na >= 2) ? sp_name(a[1]) : NULL;
    an = sp_after_kw_name(a, na, "ON");
    if (!nm || !nm[0] || !an || !an[0]) {
        c99_fail(c, s->line, "SPEED DROP INDEX: atteso DROP INDEX <nome> ON <alias>");
        return;
    }
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_drop_index(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), \"%s\");\n", an, nm);
}

/* SPEED DELETE alias, key|rec → gmb_db_speed_delete (runtime condiviso).
 * La chiave è quella della PUT: stringa/numero per espressione, oppure il PRIMO campo
 * (PK) se il 2° argomento è una variabile UDT. */
static void sp_delete(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* an = (na >= 1) ? dom_ident(a[0]) : NULL;
    gmb_expr_t* src = (na >= 2) ? a[1] : NULL;
    if (!an || !an[0]) { c99_fail(c, s->line, "SPEED DELETE: alias mancante"); return; }
    {
        const char* sn = src ? dom_ident(src) : NULL;
        struct cls_info* ci = sn ? sp_rec_class(c, sn) : NULL;
        if (ci && ci->nfields > 0) {
            /* chiave = primo campo (PK), stessa forma della PUT (snprintf %g / stringa). */
            ind(c); sb_put(&c->out, "{ char _sp_k[64];\n");
            ind(c);
            if (cls_field_is_str(ci, ci->fname[0])) {
                sb_put(&c->out, "snprintf(_sp_k, sizeof(_sp_k), \"%s\", ");
                sb_cls_field(&c->out, ci, sn, ci->fname[0]); sb_put(&c->out, ".data ? ");
                sb_cls_field(&c->out, ci, sn, ci->fname[0]); sb_put(&c->out, ".data : \"\");\n");
            } else {
                sb_put(&c->out, "snprintf(_sp_k, sizeof(_sp_k), \"%g\", (double)(");
                sb_cls_field(&c->out, ci, sn, ci->fname[0]); sb_put(&c->out, "));\n");
            }
            ind(c); sb_printf(&c->out, "(void)gmb_db_speed_delete(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), (const void*)_sp_k, (int)strlen(_sp_k));\n", an);
            ind(c); sb_put(&c->out, "}\n");
            return;
        }
    }
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_delete(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), ", an);
    if (src) {
        sb_put(&c->out, "("); emit_str_operand(c, src); sb_put(&c->out, ").data, (int)(");
        emit_str_operand(c, src); sb_put(&c->out, ").length");
    } else sb_put(&c->out, "NULL, 0");
    sb_put(&c->out, ");\n");
}

/* SPEED SEEK alias, key [USING index] → gmb_db_speed_seek. */
static void sp_seek(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* an = (na >= 1) ? dom_ident(a[0]) : NULL;
    const char* idx = sp_after_kw_name(a, na, "USING");
    if (!an || !an[0] || na < 2) { c99_fail(c, s->line, "SPEED SEEK: attesi alias e chiave"); return; }
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_seek(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\"), ", an);
    sb_put(&c->out, "("); emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (int)(");
    emit_str_operand(c, a[1]); sb_put(&c->out, ").length, ");
    if (idx && idx[0]) sb_printf(&c->out, "\"%s\"", idx); else sb_put(&c->out, "NULL");
    sb_put(&c->out, ");\n");
}

/* SPEED REFRESH VIEW name → gmb_db_speed_refresh_view. */
static void sp_refresh(ctx_t* c, gmb_stmt_t* s, gmb_expr_t** a, int na) {
    const char* sub = (na >= 1) ? dom_ident(a[0]) : NULL;
    const char* nm = (na >= 2) ? dom_ident(a[1]) : NULL;
    if (!sub || strcasecmp(sub, "VIEW") != 0 || !nm || !nm[0]) {
        c99_fail(c, s->line, "SPEED REFRESH VIEW: atteso REFRESH VIEW <nome>");
        return;
    }
    ind(c); sb_printf(&c->out, "(void)gmb_db_speed_refresh_view(\"%s\");\n", nm);
}

void emit_speed_domain(ctx_t* c, gmb_stmt_t* s) {
    gmb_expr_t** a = s->go_args;
    int na = s->n_go_args;
    switch (s->act_tk) {
    case GMB_TOK_Open: sp_open(c, s, a, na); return;
    case GMB_TOK_Use: sp_use(c, s, a, na); return;
    case GMB_TOK_Put: sp_put(c, s, a, na); return;
    case GMB_TOK_Get: sp_get(c, s, a, na); return;
    case GMB_TOK_Go:
    case GMB_TOK_Goto: sp_go(c, s, a, na); return;
    case GMB_TOK_Skip: sp_skip(c, s, a, na); return;
    case GMB_TOK_Seek: sp_seek(c, s, a, na); return;
    case GMB_TOK_Delete: sp_delete(c, s, a, na); return;
    case GMB_TOK_Set: sp_set(c, s, a, na); return;
    case GMB_TOK_Close: sp_close(c, s); return;
    case GMB_TOK_Select: sp_select(c, s, a, na); return;
    case GMB_TOK_Create: sp_create(c, s, a, na); return;
    case GMB_TOK_Drop: sp_drop(c, s, a, na); return;
    case GMB_TOK_Refresh: sp_refresh(c, s, a, na); return;
    default:
        c99_fail(c, s->line, "SPEED %s: verbo non supportato dall'emitter nativo",
                 s->mutex_action ? s->mutex_action : "?");
    }
}

/* SPEED.SETMANY(area, records) — bulk insert di una LIST OF record UDT.
 * Il 2° argomento è una variabile gmb_list_t (DIM x AS LIST OF <UDT>); l'alias (1° arg)
 * è ignorato come nell'emitter C# (NULL → tabella corrente). Runtime condiviso
 * gmb_db_speed_set_many (§0bis), che ritorna il numero di record inseriti. */
int emit_bf_speed(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line)
{
    (void)name; (void)ns;
    if (tk == GMB_TOK_SpeedSetDurability) {
        /* (mode) o (alias, mode): la durability è per-database → alias non serve. */
        gmb_expr_t* mode = (nargs >= 2) ? args[1] : (nargs >= 1 ? args[0] : NULL);
        if (!mode) { c99_fail(c, line, "SPEED.SETDURABILITY: atteso (mode)"); return 1; }
        sb_put(&c->out, "gmb_db_speed_set_durability_str(gmb_db_current_speed(), (");
        emit_str_operand(c, mode);
        sb_put(&c->out, ").data)");
        return 1;
    }
    if (tk == GMB_TOK_SpeedFlushDurability) {
        sb_put(&c->out, "((double)(gmb_db_speed_flush_durability(gmb_db_current_speed()) == 0 ? -1.0 : 0.0))");
        return 1;
    }
    if (tk == GMB_TOK_SpeedBeginGroupCommit) {
        sb_put(&c->out, "((double)(gmb_db_speed_begin_group_commit(gmb_db_current_speed()) == 0 ? -1.0 : 0.0))");
        return 1;
    }
    if (tk == GMB_TOK_SpeedEndGroupCommit) {
        sb_put(&c->out, "((double)(gmb_db_speed_end_group_commit(gmb_db_current_speed()) == 0 ? -1.0 : 0.0))");
        return 1;
    }
    if (tk == GMB_TOK_SpeedInfo) {
        sb_put(&c->out, "gmb_db_speed_info_str(gmb_db_current_speed())");
        return 1;
    }
    if (tk == GMB_TOK_SpeedSetMany) {
        if (nargs < 2) { c99_fail(c, line, "SPEED.SETMANY: attesi (area, records)"); return 1; }
        sb_put(&c->out, "(double)gmb_db_speed_set_many(gmb_db_current_speed(), ");
        emit_expr(c, args[1]);
        sb_put(&c->out, ", NULL)");
        return 1;
    }
    if (tk == GMB_TOK_EofFunc) {
        /* SPEED.EOF(alias) → BIT del cursore SPEED. Prima cadeva su IO.EOF (file) →
         * sempre true (i loop `DO WHILE NOT SPEED.EOF(T)` non partivano). Si usa
         * table_by_alias(alias) come GO/GET (curtable può essere stale). */
        const char* an = (nargs >= 1) ? dom_ident(args[0]) : NULL;
        if (an && an[0])
            sb_printf(&c->out, "((double)gmb_db_speed_eof(gmb_db_current_speed(), gmb_db_speed_table_by_alias(\"%s\")))", an);
        else
            sb_put(&c->out, "((double)gmb_db_speed_eof(gmb_db_current_speed(), gmb_db_speed_curtable(gmb_db_current_speed())))");
        return 1;
    }
    return 0;
}
