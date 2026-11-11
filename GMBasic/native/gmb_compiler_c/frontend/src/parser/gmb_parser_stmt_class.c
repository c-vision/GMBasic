/* gmb_parser_stmt_class.c — parser CLASS...END CLASS / TYPE...END TYPE (2.7/2.9).
 * Estratto da gmb_parser_statements.c (2026-08-30 — split, file <= 1200 righe). */
#include "gmb_parser_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const gmb_tokenkind_t gmb_member_mod_toks[] = {
    GMB_TOK_Public, GMB_TOK_Private, GMB_TOK_Static, GMB_TOK_Abstract,
    GMB_TOK_Async, GMB_TOK_Virtual, GMB_TOK_Override, GMB_TOK_Protected
};
static int gmb_member_mod_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_member_mod_toks) / sizeof(gmb_member_mod_toks[0]); i++)
        if (gmb_member_mod_toks[i] == k) return 1;
    return 0;
}

/* OPERATOR <simbolo|CTYPE>(params) AS <tipo> ... END OPERATOR — metodo sintetico OP_<simbolo>
 * (cursore DOPO il token OPERATOR). Estratto dal parser di classe per riuso a livello MODULO. */
gmb_stmt_t* gmb_p_parse_operator_method(gmb_parser_t* p, int line)
{
            gmb_stmt_t* m = gmb_stmt_new(GMB_STMT_FUNCTION, line);
            const char* opname = NULL;
            const gmb_token_t* op = gmb_p_cur(p);
            if (op) {
                switch (op->kind) {
                case GMB_TOK_Plus: opname = "OP_PLUS"; break;
                case GMB_TOK_Minus: opname = "OP_MINUS"; break;
                case GMB_TOK_Multiply: opname = "OP_MUL"; break;
                case GMB_TOK_Divide: opname = "OP_DIV"; break;
                case GMB_TOK_IntegerDivide: opname = "OP_IDIV"; break;
                case GMB_TOK_Mod: opname = "OP_MOD"; break;
                case GMB_TOK_Power: opname = "OP_POW"; break;
                case GMB_TOK_Equal: opname = "OP_EQ"; break;
                case GMB_TOK_NotEqual: opname = "OP_NE"; break;
                case GMB_TOK_LessThan: opname = "OP_LT"; break;
                case GMB_TOK_LessEqual: opname = "OP_LE"; break;
                case GMB_TOK_GreaterThan: opname = "OP_GT"; break;
                case GMB_TOK_GreaterEqual: opname = "OP_GE"; break;
                case GMB_TOK_And: opname = "OP_AND"; break;
                case GMB_TOK_Or: opname = "OP_OR"; break;
                default:
                    if (gmb_p_tok_is_keyword(op, "Not")) opname = "OP_NOT";
                    /* 2.7(d)p2c: OPERATOR CTYPE — conversione utente (parity C99CodeEmitter.cs
                     * _classCTypeTargets/SafeCast): nome canonico OP_CTYPE + il bare-assign
                     * "OPERATOR = expr" dentro il metodo risolve a __ret (assign a func_name). */
                    else if (op->kind == GMB_TOK_CType || gmb_p_tok_is_keyword(op, "Ctype")) opname = "OP_CTYPE";
                    break;
                }
                if (opname) gmb_p_adv(p);
            }
            if (!opname) opname = "OP_UNKNOWN";
            m->func_name = strdup(opname);
            /* parametri (a AS T, b AS T) — stesso formato degli accessori SET */
            int opcap = 0;
            if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                    int is_byref = 0;
                    if (gmb_p_check(p, GMB_TOK_ByRef) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref")) is_byref = 1;
                    if (gmb_p_check(p, GMB_TOK_ByVal) || gmb_p_check(p, GMB_TOK_ByRef)
                        || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byval") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref")
                        || gmb_p_tok_is_keyword(gmb_p_cur(p), "Optional") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Paramarray")) gmb_p_adv(p);
                    const gmb_token_t* prm = gmb_p_cur(p);
                    if (!prm || prm->kind == GMB_TOK_RightParen || prm->kind == GMB_TOK_Newline) break;
                    if (m->n_params >= opcap) {
                        opcap = opcap ? opcap * 2 : 4;
                        m->func_params = (char**)realloc(m->func_params, (size_t)opcap * sizeof(char*));
                        m->param_byref = (uint8_t*)realloc(m->param_byref, (size_t)opcap);
                        m->param_is_array = (uint8_t*)realloc(m->param_is_array, (size_t)opcap); m->param_ndims = (uint8_t*)realloc(m->param_ndims, (size_t)opcap);
                        m->param_types = (char**)realloc(m->param_types, (size_t)opcap * sizeof(char*));
                        for (int z = m->n_params; z < opcap; z++) { m->param_types[z] = NULL; m->param_is_array[z] = 0; m->param_ndims[z] = 0; }
                    }
                    m->func_params[m->n_params] = gmb_p_tok_strdup(prm);
                    m->param_byref[m->n_params] = (uint8_t)is_byref;
                    m->param_is_array[m->n_params] = 0; m->param_ndims[m->n_params] = 0;
                    m->param_types[m->n_params] = NULL;
                    m->n_params++;
                    gmb_p_adv(p);
                    if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                        gmb_p_adv(p);
                        const gmb_token_t* ty = gmb_p_cur(p);
                        if (ty && ty->kind != GMB_TOK_RightParen && ty->kind != GMB_TOK_Newline) {
                            m->param_types[m->n_params - 1] = gmb_p_tok_strdup(ty);
                            gmb_p_adv(p);
                            if (gmb_p_check(p, GMB_TOK_LessThan)) { while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_GreaterThan); }
                        }
                    }
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                }
                gmb_p_match(p, GMB_TOK_RightParen);
            }
            if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                gmb_p_adv(p);
                const gmb_token_t* ty = gmb_p_cur(p);
                if (ty && ty->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { m->func_ret_type = gmb_p_tok_strdup(ty); gmb_p_adv(p); }
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
            gmb_stmt_list_t body = gmb_p_stmt_list_new();
            for (;;) {   /* corpo fino a END OPERATOR; OPERATOR = expr → LET <OP_x> */
                gmb_p_skip_eol(p);
                if (gmb_p_at_end(p)) break;
                const gmb_token_t* bt = gmb_p_cur(p);
                if (bt->kind == GMB_TOK_EndOperator || gmb_p_tok_is_keyword(bt, "EndOperator")) { gmb_p_adv(p); break; }
                if (bt->kind == GMB_TOK_End && gmb_p_peek(p, 1)
                    && (gmb_p_peek(p, 1)->kind == GMB_TOK_Operator || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Operator"))) {
                    gmb_p_adv(p); gmb_p_adv(p); break;
                }
                if (bt->kind == GMB_TOK_End && gmb_p_peek(p, 1)
                    && (gmb_p_peek(p, 1)->kind == GMB_TOK_Class || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Class"))) break;
                if (gmb_p_tok_is_keyword(bt, "Operator") && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Equal) {
                    gmb_p_adv(p); gmb_p_adv(p);
                    gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_LET, bt->line);
                    a->target = strdup(opname);
                    a->value = gmb_p_parse_expr(p);
                    gmb_stmt_add(&body.items, &body.n, a, &body.cap);
                    continue;
                }
                bool bml = false;
                gmb_stmt_t* st = gmb_p_parse_stmt(p, &bml);
                if (st) gmb_stmt_add(&body.items, &body.n, st, &body.cap);
                else break;
            }
            m->func_body = body.items; m->n_funcbody = body.n;
    return m;
}

gmb_stmt_t* gmb_p_parse_class(gmb_parser_t* p, int line, int is_udt, int is_packed) {
    gmb_p_adv(p);   /* CLASS / TYPE */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CLASS, line);
    s->class_is_udt = is_udt;
    s->class_is_packed = is_packed;
    const gmb_token_t* nm = gmb_p_cur(p);
    if (nm && !gmb_p_at_end(p) && nm->kind != GMB_TOK_Newline) { s->class_name = gmb_p_tok_strdup(nm); gmb_p_adv(p); }
    else s->class_name = strdup("_class_");
    /* generico Box<T> / Pair<T1, T2>: cattura i NOMI dei type-param (CSV) così l'emitter può
     * specializzare le istanze (`Pair<INTEGER,STRING>` → struct con First=INTEGER, Second=STRING). */
    if (gmb_p_check(p, GMB_TOK_LessThan)) {
        gmb_p_adv(p);   /* '<' */
        char tp[128]; size_t tpl = 0; tp[0] = 0;
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) {
            const gmb_token_t* tt = gmb_p_cur(p);
            if (tt && tt->lexeme && tt->length && tt->kind != GMB_TOK_Comma) {
                if (tpl && tpl < sizeof tp - 2) tp[tpl++] = ',';
                size_t cl = (size_t)tt->length; if (tpl + cl >= sizeof tp) cl = sizeof tp - tpl - 1;
                memcpy(tp + tpl, tt->lexeme, cl); tpl += cl; tp[tpl] = 0;
            }
            gmb_p_adv(p);
        }
        gmb_p_match(p, GMB_TOK_GreaterThan);
        if (tpl) s->class_type_params = strdup(tp);
    }
    /* INHERITS base */
    if (gmb_p_cur(p) && gmb_p_tok_is_keyword(gmb_p_cur(p), "Inherits")) {
        gmb_p_adv(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline) {
            /* Nome base QUALIFICATO (MVC.HttpController): il lexer dà il primo segmento come
             * token e "." + segmenti successivi separati. Prima si prendeva UN SOLO token →
             * base="MVC" → la classe base non era trovabile ("ArticoliController INHERITS
             * MVC.HttpController" perdeva l'ereditarietà: i campi ereditati Request/Response
             * non esistevano → `SELF.Response.SendJSON` dava "metodo non definito nella
             * classe"). Concatena i segmenti dopo il punto (stesso schema del nome funzione). */
            const gmb_token_t* bt = gmb_p_cur(p);
            size_t bl = bt->length;
            char* b = (char*)malloc(bl + 1);
            if (b) { memcpy(b, bt->lexeme, bl); b[bl] = 0; }
            gmb_p_adv(p);
            while (b && gmb_p_check(p, GMB_TOK_Dot)) {
                const gmb_token_t* seg = gmb_p_peek(p, 1);
                if (!seg || seg->kind == GMB_TOK_Newline || seg->kind == GMB_TOK_Eof) break;
                gmb_p_adv(p);   /* '.' */
                size_t sl = seg->length;
                char* nb = (char*)realloc(b, strlen(b) + 1 + sl + 1);
                if (!nb) { free(b); b = NULL; break; }
                b = nb;
                strcat(b, ".");
                strncat(b, seg->lexeme, sl);
                gmb_p_adv(p);
            }
            s->class_base = b;
        }
        if (gmb_p_check(p, GMB_TOK_LessThan)) { while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_GreaterThan); }
    }
    /* 2.7(d)p2b: IMPLEMENTS If1, If2 — cattura le interfacce implementate */
    {
        int capif = 0;
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) {
            const gmb_token_t* tk = gmb_p_cur(p);
            if (tk && (tk->kind == GMB_TOK_Implements || gmb_p_tok_is_keyword(tk, "Implements"))) {
                gmb_p_adv(p);
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) {
                    const gmb_token_t* ifn = gmb_p_cur(p);
                    if (ifn->kind == GMB_TOK_Comma) { gmb_p_adv(p); continue; }
                    if (ifn->lexeme && ifn->length > 0 && ifn->length < 40) {
                        if (s->n_cls_implements >= capif) {
                            capif = capif ? capif * 2 : 4;
                            s->cls_implements = (char**)realloc(s->cls_implements, (size_t)capif * sizeof(char*));
                        }
                        s->cls_implements[s->n_cls_implements++] = gmb_p_tok_strdup(ifn);
                    }
                    gmb_p_adv(p);
                }
                break;
            }
            gmb_p_adv(p);
        }
    }
    if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
    int capf = 0, caps = 0, capm = 0, cape = 0;
    int member_static = 0;
    int member_abstract = 0;
    for (;;) {
        gmb_p_skip_eol(p);
        while (!gmb_p_at_end(p) && gmb_p_cur(p)->kind == GMB_TOK_LineNumber) gmb_p_adv(p);
        if (gmb_p_at_end(p)) break;
        const gmb_token_t* mt = gmb_p_cur(p);
        if (mt->kind == GMB_TOK_EndClass || mt->kind == GMB_TOK_EndType) { gmb_p_adv(p); break; }
        if (mt->kind == GMB_TOK_End && gmb_p_peek(p, 1)
            && (gmb_p_peek(p, 1)->kind == GMB_TOK_Class || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Class")
                || gmb_p_peek(p, 1)->kind == GMB_TOK_Type || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Type")
                || gmb_p_peek(p, 1)->kind == GMB_TOK_UnionType || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Union"))) {
            gmb_p_adv(p); gmb_p_adv(p); break;
        }
        if (gmb_member_mod_in(mt->kind)) {
            if (mt->kind == GMB_TOK_Static) member_static = 1;
            if (mt->kind == GMB_TOK_Abstract) member_abstract = 1;
            gmb_p_adv(p); continue;
        }
        if (mt->kind == GMB_TOK_Dim) {
            gmb_p_adv(p);
            /* 2.7(d): DIM a AS T, b AS T — loop sui campi separati da virgola */
            for (;;) {
                const gmb_token_t* fn = gmb_p_cur(p);
                char* fname = NULL; char* ftype = NULL;
                if (fn && fn->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { fname = gmb_p_tok_strdup(fn); gmb_p_adv(p); }
                int fdims = gmb_p_match(p, GMB_TOK_LeftParen) ? 1 : 0;
                long fdimbound = -1;   /* limite SUPERIORE della PRIMA dim (DIM arr(N)[,M...]) */
                long fdimbounds[8]; int nfdims = 0;   /* TUTTE le dim (DIM m(N,M) → [N][M]) */
                if (fdims) {
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Number || gmb_p_cur(p)->kind == GMB_TOK_Integer)) {
                            if (nfdims < 8) fdimbounds[nfdims++] = strtol(gmb_p_cur(p)->lexeme, NULL, 10);
                        }
                        gmb_p_adv(p);
                    }
                    gmb_p_match(p, GMB_TOK_RightParen);
                    if (nfdims > 0) fdimbound = fdimbounds[0];
                }
                if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                    gmb_p_adv(p);
                    const gmb_token_t* ty = gmb_p_cur(p);
                    if (ty && ty->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
                        /* LIST OF <Tipo>: campo collezione tipizzata (gmb_list_t di boxed) */
                        if ((ty->kind == GMB_TOK_List || gmb_p_tok_is_keyword(ty, "List"))
                            && gmb_p_peek(p, 1) && gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Of")) {
                            gmb_p_adv(p); gmb_p_adv(p);   /* LIST OF */
                            const gmb_token_t* el = gmb_p_cur(p);
                            if (el && el->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
                                char* en = gmb_p_tok_strdup(el);
                                size_t nl = strlen("LIST OF ") + strlen(en) + 1;
                                ftype = (char*)malloc(nl);
                                snprintf(ftype, nl, "LIST OF %s", en);
                                free(en);
                                gmb_p_adv(p);
                            } else {
                                ftype = strdup("LIST");
                            }
                        } else {
                            ftype = gmb_p_tok_strdup(ty); gmb_p_adv(p);
                            /* Lunghezza fissa `STRING * N` (DBF/ISAM): va CONSERVATA nel tipo,
                             * altrimenti DBF CREATE costruisce defs con una lunghezza di
                             * default e RECSIZE non torna (§ MiniTests/dbf/dbf_state.bas:
                             * recsize 128 invece di 20). Si registra come "STRING * 20". */
                            if (ftype && gmb_p_check(p, GMB_TOK_Multiply) && gmb_p_peek(p, 1)
                                && (gmb_p_peek(p, 1)->kind == GMB_TOK_Number
                                    || gmb_p_peek(p, 1)->kind == GMB_TOK_Integer)) {
                                char tmp[80];
                                snprintf(tmp, sizeof(tmp), "%s * %.*s", ftype,
                                         (int)gmb_p_peek(p, 1)->length, gmb_p_peek(p, 1)->lexeme);
                                free(ftype);
                                ftype = strdup(tmp);
                                gmb_p_adv(p);
                                gmb_p_adv(p);
                            }
                        }
                    }
                    if (gmb_p_check(p, GMB_TOK_LessThan)) { while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_GreaterThan); }
                }
                /* 2.7(d): STATIC DIM <n> AS <t> [= init] → campo statico della classe
                 * (parity PCodeVM; l'oracolo C# è in gap: letterale 0) */
                gmb_expr_t* finit = NULL;
                if (gmb_p_match(p, GMB_TOK_Equal)) finit = gmb_p_parse_expr(p);
                if (fname && member_static) {
                    if (s->n_cls_statics >= caps) {
                        caps = caps ? caps * 2 : 8;
                        s->cls_static_names = (char**)realloc(s->cls_static_names, (size_t)caps * sizeof(char*));
                        s->cls_static_types = (char**)realloc(s->cls_static_types, (size_t)caps * sizeof(char*));
                        s->cls_static_inits = (gmb_expr_t**)realloc(s->cls_static_inits, (size_t)caps * sizeof(gmb_expr_t*));
                    }
                    s->cls_static_names[s->n_cls_statics] = fname;
                    s->cls_static_types[s->n_cls_statics] = ftype;
                    s->cls_static_inits[s->n_cls_statics] = finit;
                    s->n_cls_statics++;
                } else if (fname) {
                    /* Array-campo di classe: marchia con "[N]" anche gli elementi STRING
                     * (`DIM arr(2) AS STRING`), prima esclusi → non erano indicizzabili.
                     * LIST resta escluso (collezione, non array). */
                    if (fdims && ftype && strncasecmp(ftype, "LIST", 4) != 0) {
                        /* Multi-dim (DIM m(2,2) AS INTEGER): marchia TUTTE le dim ("INTEGER[2][2]")
                         * — l'allocazione e l'indice flat usano il prodotto dei bound. */
                        char marked[160];
                        if (nfdims > 0) {
                            size_t mp = 0;
                            mp += (size_t)snprintf(marked + mp, sizeof marked - mp, "%s", ftype);
                            for (int di = 0; di < nfdims && mp < sizeof marked - 8; di++)
                                mp += (size_t)snprintf(marked + mp, sizeof marked - mp, "[%ld]", fdimbounds[di]);
                        } else {
                            snprintf(marked, sizeof marked, "%s[]", ftype);
                        }
                        free(ftype); ftype = strdup(marked);
                    }
                    if (s->n_cls_fields >= capf) {
                        capf = capf ? capf * 2 : 8;
                        s->cls_field_names = (char**)realloc(s->cls_field_names, (size_t)capf * sizeof(char*));
                        s->cls_field_types = (char**)realloc(s->cls_field_types, (size_t)capf * sizeof(char*));
                    }
                    s->cls_field_names[s->n_cls_fields] = fname;
                    s->cls_field_types[s->n_cls_fields] = ftype;
                    s->n_cls_fields++;
                    if (finit) gmb_expr_free(finit);
                } else { free(fname); if (ftype) free(ftype); if (finit) gmb_expr_free(finit); }
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
            }
            member_static = 0;
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);   /* residui riga */
            continue;
        }
        if (mt->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(mt, "Sub")) {
            /* 2.7(d): ABSTRACT FUNCTION/SUB — solo header, nessun corpo né END FUNCTION:
             * delegare a parse_stmt consumerebbe il blocco fino all'END CLASS. Creo il
             * metodo con corpo vuoto (le derivate lo sovrascrivono con OVERRIDE). */
            if (member_abstract) {
                gmb_stmt_t* m = gmb_stmt_new(GMB_STMT_FUNCTION, mt->line);
                gmb_p_adv(p);   /* FUNCTION/SUB */
                const gmb_token_t* mn = gmb_p_cur(p);
                if (mn && mn->kind != GMB_TOK_Newline) { m->func_name = gmb_p_tok_strdup(mn); gmb_p_adv(p); }
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                s->cls_methods[s->n_cls_methods++] = m;
                member_abstract = 0; member_static = 0;
                continue;
            }
            bool ml = false;
            gmb_stmt_t* m = gmb_p_parse_stmt(p, &ml);
            if (m && m->kind == GMB_STMT_FUNCTION) {
                if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                s->cls_methods[s->n_cls_methods++] = m;
            } else if (m) gmb_stmt_free(m);
            continue;
        }
        /* 2.7(b): PROPERTY <nome> AS <tipo> → metodi sintetici PropGet_/PropSet_ */
        if (mt->kind == GMB_TOK_Property || gmb_p_tok_is_keyword(mt, "Property")) {
            gmb_p_adv(p);   /* PROPERTY */
            char* prop_name = NULL; char* prop_type = NULL;
            const gmb_token_t* pn = gmb_p_cur(p);
            if (pn && pn->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { prop_name = gmb_p_tok_strdup(pn); gmb_p_adv(p); }
            if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                gmb_p_adv(p);
                const gmb_token_t* ty = gmb_p_cur(p);
                if (ty && ty->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { prop_type = gmb_p_tok_strdup(ty); gmb_p_adv(p); }
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)
                   && !gmb_p_check(p, GMB_TOK_FatArrow) && !gmb_p_check(p, GMB_TOK_Arrow)) gmb_p_adv(p);
            /* 2.7(d): PROPERTY <nome> AS <tipo> => expr — PropGet expression-bodied,
             * riga singola senza END PROPERTY (parity PCodeVM) */
            if (gmb_p_check(p, GMB_TOK_FatArrow) || gmb_p_check(p, GMB_TOK_Arrow)) {
                gmb_p_adv(p);
                gmb_stmt_t* m = gmb_stmt_new(GMB_STMT_FUNCTION, mt->line);
                char nm[160]; snprintf(nm, sizeof(nm), "PropGet_%s", prop_name ? prop_name : "?");
                m->func_name = strdup(nm);
                if (prop_type) m->func_ret_type = strdup(prop_type);
                gmb_stmt_list_t body = gmb_p_stmt_list_new();
                gmb_stmt_t* r = gmb_stmt_new(GMB_STMT_RETURN, mt->line);
                r->ret_value = gmb_p_parse_expr(p);
                gmb_stmt_add(&body.items, &body.n, r, &body.cap);
                m->func_body = body.items; m->n_funcbody = body.n;
                if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                s->cls_methods[s->n_cls_methods++] = m;
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                free(prop_name); free(prop_type);
                continue;
            }
            for (;;) {
                gmb_p_skip_eol(p);
                if (gmb_p_at_end(p)) break;
                const gmb_token_t* gt = gmb_p_cur(p);
                if (gt->kind == GMB_TOK_EndProperty || gmb_p_tok_is_keyword(gt, "EndProperty")) { gmb_p_adv(p); break; }
                if (gt->kind == GMB_TOK_End && gmb_p_peek(p, 1)
                    && (gmb_p_peek(p, 1)->kind == GMB_TOK_Property || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Property"))) {
                    gmb_p_adv(p); gmb_p_adv(p); break;
                }
                if (gt->kind == GMB_TOK_Get || gmb_p_tok_is_keyword(gt, "Get")) {
                    gmb_p_adv(p);
                    /* 2.7(d): GET => expr — accessor expression-bodied */
                    if (gmb_p_check(p, GMB_TOK_FatArrow) || gmb_p_check(p, GMB_TOK_Arrow)) {
                        gmb_p_adv(p);
                        gmb_stmt_t* m = gmb_stmt_new(GMB_STMT_FUNCTION, gt->line);
                        char nm[160]; snprintf(nm, sizeof(nm), "PropGet_%s", prop_name ? prop_name : "?");
                        m->func_name = strdup(nm);
                        if (prop_type) m->func_ret_type = strdup(prop_type);
                        gmb_stmt_list_t body = gmb_p_stmt_list_new();
                        gmb_stmt_t* r = gmb_stmt_new(GMB_STMT_RETURN, gt->line);
                        r->ret_value = gmb_p_parse_expr(p);
                        gmb_stmt_add(&body.items, &body.n, r, &body.cap);
                        m->func_body = body.items; m->n_funcbody = body.n;
                        if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                        s->cls_methods[s->n_cls_methods++] = m;
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                        continue;
                    }
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                    gmb_stmt_t* m = gmb_stmt_new(GMB_STMT_FUNCTION, gt->line);
                    char nm[160]; snprintf(nm, sizeof(nm), "PropGet_%s", prop_name ? prop_name : "?");
                    m->func_name = strdup(nm);
                    if (prop_type) m->func_ret_type = strdup(prop_type);
                    gmb_stmt_list_t body = gmb_p_stmt_list_new();
                    gmb_p_parse_block(p, &body, GMB_TOK_EndGet, "EndGet", gt->line);
                    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndGet || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndGet"))) gmb_p_adv(p);
                    else if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End && gmb_p_peek(p, 1)
                             && gmb_p_peek(p, 1)->kind == GMB_TOK_Get) { gmb_p_adv(p); gmb_p_adv(p); }
                    m->func_body = body.items; m->n_funcbody = body.n;
                    if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                    s->cls_methods[s->n_cls_methods++] = m;
                    continue;
                }
                if (gt->kind == GMB_TOK_Set || gmb_p_tok_is_keyword(gt, "Set")) {
                    gmb_p_adv(p);
                    gmb_stmt_t* m = gmb_stmt_new(GMB_STMT_FUNCTION, gt->line);
                    char nm[160]; snprintf(nm, sizeof(nm), "PropSet_%s", prop_name ? prop_name : "?");
                    m->func_name = strdup(nm);
                    int pcap = 0;
                    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                            int is_byref = 0;
                            if (gmb_p_check(p, GMB_TOK_ByRef) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref")) is_byref = 1;
                            if (gmb_p_check(p, GMB_TOK_ByVal) || gmb_p_check(p, GMB_TOK_ByRef)
                                || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byval") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref")
                                || gmb_p_tok_is_keyword(gmb_p_cur(p), "Optional") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Paramarray")) gmb_p_adv(p);
                            const gmb_token_t* prm = gmb_p_cur(p);
                            if (!prm || prm->kind == GMB_TOK_RightParen || prm->kind == GMB_TOK_Newline) break;
                            if (m->n_params >= pcap) {
                                pcap = pcap ? pcap * 2 : 4;
                                m->func_params = (char**)realloc(m->func_params, (size_t)pcap * sizeof(char*));
                                m->param_byref = (uint8_t*)realloc(m->param_byref, (size_t)pcap);
                                m->param_is_array = (uint8_t*)realloc(m->param_is_array, (size_t)pcap); m->param_ndims = (uint8_t*)realloc(m->param_ndims, (size_t)pcap);
                                m->param_types = (char**)realloc(m->param_types, (size_t)pcap * sizeof(char*));
                                for (int z = m->n_params; z < pcap; z++) { m->param_types[z] = NULL; m->param_is_array[z] = 0; m->param_ndims[z] = 0; }
                            }
                            m->func_params[m->n_params] = gmb_p_tok_strdup(prm);
                            m->param_byref[m->n_params] = (uint8_t)is_byref;
                            m->param_is_array[m->n_params] = 0; m->param_ndims[m->n_params] = 0;
                            m->param_types[m->n_params] = NULL;
                            m->n_params++;
                            gmb_p_adv(p);
                            if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                                gmb_p_adv(p);
                                const gmb_token_t* ty = gmb_p_cur(p);
                                if (ty && ty->kind != GMB_TOK_RightParen && ty->kind != GMB_TOK_Newline) {
                                    m->param_types[m->n_params - 1] = gmb_p_tok_strdup(ty);
                                    gmb_p_adv(p);
                                    if (gmb_p_check(p, GMB_TOK_LessThan)) { while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_GreaterThan); }
                                }
                            }
                            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        }
                        gmb_p_match(p, GMB_TOK_RightParen);
                    }
                    /* 2.7(d): SET(...) => <istruzione singola> — accessor expression-bodied */
                    if (gmb_p_check(p, GMB_TOK_FatArrow) || gmb_p_check(p, GMB_TOK_Arrow)) {
                        gmb_p_adv(p);
                        gmb_stmt_list_t body = gmb_p_stmt_list_new();
                        bool ml2 = false;
                        gmb_stmt_t* one = gmb_p_parse_stmt(p, &ml2);
                        if (one) gmb_stmt_add(&body.items, &body.n, one, &body.cap);
                        m->func_body = body.items; m->n_funcbody = body.n;
                        if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                        s->cls_methods[s->n_cls_methods++] = m;
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                        continue;
                    }
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                    gmb_stmt_list_t body = gmb_p_stmt_list_new();
                    gmb_p_parse_block(p, &body, GMB_TOK_EndSet, "EndSet", gt->line);
                    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndSet || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndSet"))) gmb_p_adv(p);
                    else if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End && gmb_p_peek(p, 1)
                             && gmb_p_peek(p, 1)->kind == GMB_TOK_Set) { gmb_p_adv(p); gmb_p_adv(p); }
                    m->func_body = body.items; m->n_funcbody = body.n;
                    if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
                    s->cls_methods[s->n_cls_methods++] = m;
                    continue;
                }
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
            }
            free(prop_name); free(prop_type);
            continue;
        }
        /* 2.7(d)p2b: OPERATOR <simbolo>(params) AS <tipo> ... END OPERATOR —
         * metodo sintetico OP_<simbolo> (STATIC: dispatch esplicito nell'emitter) */
        if (gmb_p_tok_is_keyword(mt, "Operator")) {
            gmb_p_adv(p);   /* OPERATOR */
            gmb_stmt_t* m = gmb_p_parse_operator_method(p, mt->line);
            if (s->n_cls_methods >= capm) { capm = capm ? capm * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)capm * sizeof(gmb_stmt_t*)); }
            s->cls_methods[s->n_cls_methods++] = m;
            member_static = 0; member_abstract = 0;
            continue;
        }
        /* EVENT: dichiarazione singola (EVENT Nome(args) senza END EVENT) — cattura solo
         * il NOME (2.9: usato per inizializzare il registro handler per-istanza al
         * costruttore — vedi cc_class_register/cc_class_member), poi consuma il resto
         * della riga (parametri dichiarati non verificati da RAISE EVENT). */
        if (gmb_p_tok_is_keyword(mt, "Event")) {
            gmb_p_adv(p);   /* EVENT */
            const gmb_token_t* en = gmb_p_cur(p);
            if (en && en->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
                if (s->n_cls_events >= cape) { cape = cape ? cape * 2 : 4; s->cls_event_names = (char**)realloc(s->cls_event_names, (size_t)cape * sizeof(char*)); }
                s->cls_event_names[s->n_cls_events++] = gmb_p_tok_strdup(en);
            }
            for (;;) {
                if (gmb_p_at_end(p)) break;
                if (gmb_p_check(p, GMB_TOK_Newline)) break;
                gmb_p_adv(p);
            }
            continue;
        }
        /* 2.7(d)p2b UDT: campi nudi nel corpo TYPE (X AS DOUBLE — senza DIM).
         * Il nome campo può essere un token keyword usato come identificatore (es. Zip).
         * Seconda grafia: campo DIMensionato "arr(4) AS LONG" — qui l'AS viene DOPO le
         * parensi, quindi il guard sotto delega a gmb_p_udt_field_dims_ahead (senza di ciò
         * il campo era silenziosamente SCARTATO: SIZEOF/UDTPACKED perdevano un campo). */
        if (is_udt && mt->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)
            && ((gmb_p_peek(p, 1)->kind == GMB_TOK_As || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "As"))
                || (gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen && gmb_p_udt_field_dims_ahead(p)))) {
            for (;;) {
                const gmb_token_t* fn = gmb_p_cur(p);
                char* fname = NULL; char* ftype = NULL;
                if (fn && fn->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { fname = gmb_p_tok_strdup(fn); gmb_p_adv(p); }
                int fdims = gmb_p_match(p, GMB_TOK_LeftParen) ? 1 : 0;
                long fdimbound = -1;   /* limite SUPERIORE della PRIMA dim (DIM arr(N)[,M...]) */
                long fdimbounds[8]; int nfdims = 0;   /* TUTTE le dim (DIM m(N,M) → [N][M]) */
                if (fdims) {
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Number || gmb_p_cur(p)->kind == GMB_TOK_Integer)) {
                            if (nfdims < 8) fdimbounds[nfdims++] = strtol(gmb_p_cur(p)->lexeme, NULL, 10);
                        }
                        gmb_p_adv(p);
                    }
                    gmb_p_match(p, GMB_TOK_RightParen);
                    if (nfdims > 0) fdimbound = fdimbounds[0];
                }
                if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                    gmb_p_adv(p);
                    const gmb_token_t* ty = gmb_p_cur(p);
                    if (ty && ty->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
                        if ((ty->kind == GMB_TOK_List || gmb_p_tok_is_keyword(ty, "List"))
                            && gmb_p_peek(p, 1) && gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Of")) {
                            gmb_p_adv(p); gmb_p_adv(p);
                            const gmb_token_t* el = gmb_p_cur(p);
                            if (el && el->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
                                char* en = gmb_p_tok_strdup(el);
                                size_t nl = strlen("LIST OF ") + strlen(en) + 1;
                                ftype = (char*)malloc(nl);
                                snprintf(ftype, nl, "LIST OF %s", en);
                                free(en);
                                gmb_p_adv(p);
                            } else ftype = strdup("LIST");
                        } else {
                            ftype = gmb_p_tok_strdup(ty); gmb_p_adv(p);
                            /* Lunghezza fissa `STRING * N` (DBF/ISAM) — conservata nel tipo:
                             * serve a DBF CREATE (defs) e quindi a RECSIZE. */
                            if (ftype && gmb_p_check(p, GMB_TOK_Multiply) && gmb_p_peek(p, 1)
                                && (gmb_p_peek(p, 1)->kind == GMB_TOK_Number
                                    || gmb_p_peek(p, 1)->kind == GMB_TOK_Integer)) {
                                char tmp[80];
                                snprintf(tmp, sizeof(tmp), "%s * %.*s", ftype,
                                         (int)gmb_p_peek(p, 1)->length, gmb_p_peek(p, 1)->lexeme);
                                free(ftype);
                                ftype = strdup(tmp);
                                gmb_p_adv(p);
                                gmb_p_adv(p);
                            }
                        }
                    }
                }
                /* BITFIELD (2026-11-11): clausola `WIDTH n` dopo il tipo del campo (1..64 bit).
                 * La larghezza viaggia in cls_field_bits[] accanto a nomi/tipi; l'emitter C99
                 * genera un vero bitfield C (`int64_t A : 3`) e il layout (SIZEOF) resta
                 * calcolato dal canone TypeLayout. */
                int fbits = 0;
                if (ftype && gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Width || gmb_p_tok_is_keyword(gmb_p_cur(p), "Width"))) {
                    gmb_p_adv(p);   /* WIDTH */
                    const gmb_token_t* wn = gmb_p_cur(p);
                    if (wn && (wn->kind == GMB_TOK_Number || wn->kind == GMB_TOK_Integer)) {
                        fbits = (int)strtol(wn->lexeme, NULL, 10);
                        gmb_p_adv(p);
                        /* Solo tipi INTERI (parità col parser C#): un bitfield C non può essere
                         * double/float. Il marcatore -1 fa fallire l'emissione con un errore
                         * esplicito invece di un campo silenziosamente ignorato. */
                        if (strncasecmp(ftype, "BYTE", 4) != 0 && strncasecmp(ftype, "SBYTE", 5) != 0
                            && strncasecmp(ftype, "INTEGER", 7) != 0 && strncasecmp(ftype, "UINTEGER", 8) != 0
                            && strncasecmp(ftype, "LONG", 4) != 0 && strncasecmp(ftype, "ULONG", 5) != 0
                            && strncasecmp(ftype, "QUAD", 4) != 0 && strncasecmp(ftype, "UQUAD", 5) != 0)
                            fbits = -1;
                    }
                }
                if (fname) {
                    /* Campo DIMensionato in un TYPE (X(n) AS T): gli elementi NON sono inline,
                     * il campo è un riferimento da 8 byte nel layout (canone SIZEOF/TypeLayout).
                     * Il marcatore "[]" è letto da gmb_cls_field_bytes; STRING/LIST già contano
                     * come riferimento e restano invariati (non perdere la fissità DBF). */
                    /* Un campo con dims è un ARRAY anche se STRING (`items(5) AS STRING * 10`):
                     * serve il marker "[N]" per allocarlo/iterarlo (parità C# `gmb_string_t
                     * items[6]`). Prima STRING era escluso → `items` restava scalare. */
                    if (fdims && ftype && strncasecmp(ftype, "LIST", 4) != 0) {
                        /* Marca con i BOUND reali ("INTEGER[1]", multi-dim "[2][2]"), non "[]":
                         * il C99Native usa il bound per ALLOCARE il campo-array (cls_emit_arr_alloc)
                         * e per serializzarlo in JSON. Prima "[]" perdeva il bound -> allocava 1
                         * slot e ToJson leggeva il puntatore come scalare
                         * (bug C99Native-UDT-Field-Array-Bound-Marker). Il campo resta comunque un
                         * RIFERIMENTO da 8 byte (gmb_cls_field_bytes riconosce qualsiasi "[...]"). */
                        char marked[160]; size_t mp = 0;
                        mp += (size_t)snprintf(marked + mp, sizeof marked - mp, "%s", ftype);
                        if (nfdims > 0) {
                            for (int di = 0; di < nfdims && mp < sizeof marked - 8; di++)
                                mp += (size_t)snprintf(marked + mp, sizeof marked - mp, "[%ld]", fdimbounds[di]);
                        } else {
                            snprintf(marked + mp, sizeof marked - mp, "[]");
                        }
                        free(ftype); ftype = strdup(marked);
                    }
                    if (s->n_cls_fields >= capf) {
                        capf = capf ? capf * 2 : 8;
                        s->cls_field_names = (char**)realloc(s->cls_field_names, (size_t)capf * sizeof(char*));
                        s->cls_field_types = (char**)realloc(s->cls_field_types, (size_t)capf * sizeof(char*));
                        s->cls_field_bits = (int*)realloc(s->cls_field_bits, (size_t)capf * sizeof(int));
                    }
                    s->cls_field_names[s->n_cls_fields] = fname;
                    s->cls_field_types[s->n_cls_fields] = ftype;
                    s->cls_field_bits[s->n_cls_fields] = fbits;
                    s->n_cls_fields++;
                } else { if (ftype) free(ftype); }
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);   /* residui riga */
            continue;
        }
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
    }
    return s;
}

static gmb_stmt_t* gmb_p_parse_stmt_impl(gmb_parser_t* p, bool* multiline);
