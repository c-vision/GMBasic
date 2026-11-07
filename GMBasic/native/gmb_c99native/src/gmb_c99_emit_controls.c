/* gmb_c99_emit_controls.c — emitter C99 NATIVO dei widget TUI senza-namespace
 * (COMBOBOX/RADIO/SLIDER/TAB/GRID/DATEPICKER/COLORPICKER/TREEVIEW/STATUSBAR/
 * SPINNER + CHECKBOX/LISTBOX/MENU/TOGGLE). Estratto da gmb_c99_emit.c (§0quater:
 * dominio → file dedicato). §0bis: chiama i runtime condivisi gmb_*_* di
 * gmb_tui_widgets.c, MAI logica inline. Parità emitter C# (C99CodeEmitter.cs).
 *
 * Semantica per widget (dal parser Parser.Stmt.Controls.cs e dal runtime TUI):
 *   COMBOBOX  row, col, width, itemsCSV            INTO var   (label selezionata / "")
 *   GRID      row, col, width, height, headers, data INTO var  (indice selezionato / -1)
 *   SLIDER    row, col, length, min, max, step     INTO var   (valore / min)
 *   SPINNER   row, col, preset                     [INTO var]  (indice / 0)
 *   RADIO row, col, itemsCSV                  INTO var   (label / "")
 *   STATUSBAR text                                             (nessun INTO)
 *   TREEVIEW  row, col, width, height, treeCSV     INTO var   (label / "")
 *   COLORPICKER row, col, columns                  INTO var   (indice / -1)
 *   DATEPICKER row, col                            INTO var$  (data formattata)
 *   TAB       row, col, width, height, tabsCSV     INTO var   (indice / 0)
 *   FILEDIALOG mask, title                         INTO var$  (path / "")
 *   PRINTERDIALOG title?                           INTO p$, copies, duplex
 */
#include "gmb_c99_emit_internal.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* Elenco di stringhe → `const char* arr[]` + `int cnt`. Tre forme:
 *  1) letterale array ["A","B"] (parser: CALL "_array_") → elementi diretti;
 *  2) array nativo di stringhe (DIM x(n) AS STRING / DIM x = {...}) → x[i].data con
 *     lunghezza emit_arr_len_of (parity del ramo array di ctl_as_list);
 *  3) fallback: stringa CSV → gmb_str_split_strings (temp unico per call).
 * Ritorna il nome del temp gmb_string_t* da liberare DOPO l'uso (o NULL se non serve). */
static const char* ctl_split_csv(ctx_t* c, gmb_expr_t* e, const char* arr, const char* cnt,
                                 const char* free_arr)
{
    (void)free_arr;
    if (e && e->kind == GMB_EXPR_CALL && e->var && strcmp(e->var, "_array_") == 0) {
        sb_printf(&c->out, "int %s = %d; const char* %s[64]; ", cnt, e->nargs, arr);
        for (int i = 0; i < e->nargs && i < 64; i++) {
            sb_printf(&c->out, "%s[%d] = ", arr, i);
            emit_str_operand(c, e->args[i]);
            sb_put(&c->out, ".data; ");
        }
        return NULL;
    }
    if (e && e->kind == GMB_EXPR_VAR && e->var) {
        var_t* v = lookup_var(c, e->var);
        if (v && v->is_array && !v->matrix && v->gkind != GK_LIST && v->type == TY_STR
            && !(v->byref && !v->captured)) {
            sb_printf(&c->out, "int %s = (int)(", cnt);
            emit_arr_len_of(c, v);
            sb_printf(&c->out, "); const char* %s[64]; for (int __ctl_i = 0; __ctl_i < %s && __ctl_i < 64; __ctl_i++) ", arr, cnt);
            sb_printf(&c->out, "%s[__ctl_i] = %s[__ctl_i].data ? %s[__ctl_i].data : \"\"; ", arr, v->name, v->name);
            return NULL;
        }
    }
    static int csv_uid = 0;
    static char retbuf[8][32];
    int id = ++csv_uid;
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "__ctl_tmp%d", id);
    sb_printf(&c->out, "int %s = 0; gmb_string_t* %s = gmb_str_split_strings(", cnt, tmp);
    emit_str_operand(c, e);
    sb_printf(&c->out, ".data, \"|\", &%s); ", cnt);
    sb_printf(&c->out, "const char* %s[32]; int __ctl_i%d = 0; for (; __ctl_i%d < %s && __ctl_i%d < 32; __ctl_i%d++) %s[__ctl_i%d] = %s[__ctl_i%d].data; ",
              arr, id, id, cnt, id, id, arr, id, tmp, id);
    char* slot = retbuf[csv_uid % 8];
    snprintf(slot, 32, "%s", tmp);
    return slot;
}

/* Unisce gli elementi (arr[0..cnt-1]) in una stringa CSV '|' in un buffer LOCALE
 * (usato dalle factory polimorfiche che accettano items come stringa). */
static void ctl_join_csv_from(ctx_t* c, const char* arr, const char* cnt, const char* out)
{
    sb_printf(&c->out, "char %s[4096]; { int __o = 0; for (int __i = 0; __i < %s; __i++) { if (__i && __o < 4095) %s[__o++] = '|'; const char* __s = %s[__i] ? %s[__i] : \"\"; while (*__s && __o < 4095) %s[__o++] = *__s++; } %s[__o] = 0; } ",
              out, cnt, out, arr, arr, out, out);
}

static void ctl_free_csv(const char* cnt)
{
    /* placeholder: la free è inline dopo l'uso in ogni ramo (vedi emit_control). */
    (void)cnt;
}

/* Assegna il target INTO da un valore stringa o numerico. */
static void ctl_assign_str(ctx_t* c, var_t* iv, const char* strexpr)
{
    if (!iv) return;
    emit_net_lhs(c, iv);
    if (iv->type == TY_STR) { sb_put(&c->out, " = gmb_string_alloc("); sb_put(&c->out, strexpr); sb_put(&c->out, " ? "); sb_put(&c->out, strexpr); sb_put(&c->out, " : \"\"); "); }
    else sb_put(&c->out, " = 0.0; ");
}
static void ctl_assign_num(ctx_t* c, var_t* iv, const char* numexpr)
{
    if (!iv) return;
    emit_net_lhs(c, iv);
    sb_put(&c->out, " = ("); sb_put(&c->out, numexpr); sb_put(&c->out, "); ");
}

/* Emette un'espressione come gmb_list_t: se l'arg è un array NATIVO (STRING/DOUBLE)
 * lo converte con gmb_list_from_strings/doubles + lunghezza; altrimenti assume un
 * valore già boxed lista (gmb_val_as_list). Usato da TBROWSE. */
/* Letterale `_array_` ANNIDATO ({{"Nome",10},{"Qta",6}}: colonne/dati di TBROWSE) → lista di liste di gmb_val_t:
 * ({ gmb_list_t __l = gmb_list_new(); gmb_list_add(&__l, gmb_make_val_list(<interno>)); ... __l; }). Gli elementi scalari sono boxati
 * per tipo (stringa → gmb_make_val_string, numero → gmb_make_val_double) cosi' la larghezza colonna resta numerica. */
static int ctl_is_array_lit(gmb_expr_t* e) { return e && e->kind == GMB_EXPR_CALL && e->var && strcmp(e->var, "_array_") == 0; }

/* Codici delle operazioni GraphicsContext (CONTROL DRAW g, "op", ...). */
static int gfxctx_op_code(const char* n)
{
    if (!n) return -1;
    if (!strcasecmp(n, "clear")) return 0;
    if (!strcasecmp(n, "setcolor")) return 1;
    if (!strcasecmp(n, "line")) return 2;
    if (!strcasecmp(n, "rect")) return 3;
    if (!strcasecmp(n, "fillrect")) return 4;
    if (!strcasecmp(n, "circle")) return 5;
    if (!strcasecmp(n, "fillcircle")) return 6;
    if (!strcasecmp(n, "text")) return 7;
    if (!strcasecmp(n, "plot")) return 8;
    return -1;
}
static void ctl_emit_val_list(ctx_t* c, gmb_expr_t* e)
{
    static int uid = 0;
    int id = ++uid;
    sb_printf(&c->out, "({ gmb_list_t __vl%d = gmb_list_new(); ", id);
    for (int i = 0; i < e->nargs; i++) {
        gmb_expr_t* el = e->args[i];
        sb_printf(&c->out, "gmb_list_add(&__vl%d, ", id);
        if (ctl_is_array_lit(el)) { sb_put(&c->out, "gmb_make_val_list("); ctl_emit_val_list(c, el); sb_put(&c->out, ")"); }
        else if (etype(c, el) == TY_STR) { sb_put(&c->out, "gmb_make_val_string(("); emit_str_operand(c, el); sb_put(&c->out, ").data)"); }
        else { sb_put(&c->out, "gmb_make_val_double((gmb_double_t)("); emit_expr(c, el); sb_put(&c->out, "))"); }
        sb_put(&c->out, "); ");
    }
    sb_printf(&c->out, "__vl%d; })", id);
}
/* Wrapper pubblico per l'emissione di un letterale annidato come lista di liste
 * (usato dall'emitter principale per le variabili ANY/TY_VAL). */
void gmb_c99_emit_nested_list(ctx_t* c, gmb_expr_t* e);
static int ctl_has_nested_array(gmb_expr_t* e)
{
    if (!ctl_is_array_lit(e)) return 0;
    for (int i = 0; i < e->nargs; i++) if (ctl_is_array_lit(e->args[i])) return 1;
    return 0;
}

void gmb_c99_emit_nested_list(ctx_t* c, gmb_expr_t* e) { ctl_emit_val_list(c, e); }
static void ctl_as_list(ctx_t* c, gmb_expr_t* e)
{
    /* Letterale array ["A","B"] (parser: CALL "_array_") → lista via compound literal
     * C99 (prima cadeva nel ramo gmb_val_as_list → "_array_ non definita", TBROWSE). */
    if (ctl_has_nested_array(e)) { ctl_emit_val_list(c, e); return; }
    if (e && e->kind == GMB_EXPR_CALL && e->var && strcmp(e->var, "_array_") == 0) {
        if (e->nargs == 0) { sb_put(&c->out, "gmb_list_from_strings(NULL, 0)"); return; }
        sb_put(&c->out, "gmb_list_from_strings((const gmb_string_t[]){ ");
        for (int i = 0; i < e->nargs; i++) {
            if (i) sb_put(&c->out, ", ");
            emit_str_operand(c, e->args[i]);
        }
        sb_printf(&c->out, " }, %d)", e->nargs);
        return;
    }
    var_t* v = (e && e->kind == GMB_EXPR_VAR && e->var) ? lookup_var(c, e->var) : NULL;
    if (v && v->is_array && !v->matrix && v->gkind != GK_LIST)
    {
        sb_put(&c->out, v->type == TY_STR ? "gmb_list_from_strings((const gmb_string_t*)" : "gmb_list_from_doubles((const gmb_double_t*)");
        emit_var_ref(c, v);
        sb_put(&c->out, ", (gmb_int_t)(");
        emit_arr_len_of(c, v);
        sb_put(&c->out, "))");
        return;
    }
    /* Stringa (letterale o variabile) come argomento-lista: lista di UNA voce.
     * Prima finiva in gmb_val_as_list(gmb_string_t) → errore di compilazione
     * ("passing 'gmb_string_t' to parameter of type 'gmb_val_t'"). */
    if (e && (e->kind == GMB_EXPR_STR ||
              (e->kind == GMB_EXPR_VAR && e->var && (lookup_var(c, e->var) == NULL || lookup_var(c, e->var)->type == TY_STR)))) {
        sb_put(&c->out, "gmb_list_from_strings((const gmb_string_t[]){ ");
        emit_str_operand(c, e);
        sb_put(&c->out, " }, 1)");
        return;
    }
    sb_put(&c->out, "gmb_val_as_list(");
    emit_expr(c, e);
    sb_put(&c->out, ")");
}

/* Dispatch per nome: ritorna 1 se gestito. */
static const char* ev_const(const char* e)
{
    if (!strcasecmp(e, "CLICK")) return "GMB_EV_CLICK";
    if (!strcasecmp(e, "DBLCLICK")) return "GMB_EV_DBLCLICK";
    if (!strcasecmp(e, "CHANGE")) return "GMB_EV_CHANGE";
    if (!strcasecmp(e, "GOTFOCUS")) return "GMB_EV_GOTFOCUS";
    if (!strcasecmp(e, "LOSTFOCUS")) return "GMB_EV_LOSTFOCUS";
    if (!strcasecmp(e, "MOUSEDOWN")) return "GMB_EV_MOUSEDOWN";
    if (!strcasecmp(e, "MOUSEUP")) return "GMB_EV_MOUSEUP";
    if (!strcasecmp(e, "MOUSEMOVE")) return "GMB_EV_MOUSEMOVE";
    if (!strcasecmp(e, "KEYDOWN")) return "GMB_EV_KEYDOWN";
    if (!strcasecmp(e, "KEYPRESS")) return "GMB_EV_KEYPRESS";
    if (!strcasecmp(e, "KEYUP")) return "GMB_EV_KEYUP";
    if (!strcasecmp(e, "DRAGDROP")) return "GMB_EV_DRAGDROP";
    if (!strcasecmp(e, "DRAGOVER")) return "GMB_EV_DRAGOVER";
    if (!strcasecmp(e, "VALIDATE")) return "GMB_EV_VALIDATE";
    if (!strcasecmp(e, "SCROLL")) return "GMB_EV_SCROLL";
    if (!strcasecmp(e, "DROPDOWN")) return "GMB_EV_DROPDOWN";
    if (!strcasecmp(e, "UPCLICK")) return "GMB_EV_UPCLICK";
    if (!strcasecmp(e, "DOWNCLICK")) return "GMB_EV_DOWNCLICK";
    if (!strcasecmp(e, "NODECLICK")) return "GMB_EV_NODECLICK";
    if (!strcasecmp(e, "EXPAND")) return "GMB_EV_EXPAND";
    if (!strcasecmp(e, "COLLAPSE")) return "GMB_EV_COLLAPSE";
    if (!strcasecmp(e, "SELCHANGE")) return "GMB_EV_SELCHANGE";
    if (!strcasecmp(e, "ROWCOLCHANGE")) return "GMB_EV_ROWCOLCHANGE";
    return NULL;
}
static void emit_ctrl_events(ctx_t* c, gmb_stmt_t* s)
{
    if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
    for (int i = 0; i + 1 < s->n_ev; i += 2) {
        const char* ec = ev_const(s->ev_pairs[i]);
        if (!ec) continue;
        const char* act = s->ev_pairs[i + 1];
        if (!act || !*act) { sb_printf(&c->out, "gmb_form_set_event(%s, NULL); ", ec); continue; }   /* STOP */
        const char* name = ((act[0] == 'L' || act[0] == 'G') && act[1] == ':') ? act + 2 : act;
        sb_printf(&c->out, "gmb_form_set_event(%s, (gmb_form_cb)gmb_user_", ec);
        emit_ident(c, name);
        sb_put(&c->out, "); ");
    }
}
static int tui_spec_val(const char* spec, const char* key, char* out, int cap)
{
    out[0]=0; if (!spec) return 0;
    size_t kl=strlen(key); const char* p=spec;
    while (*p) {
        while (*p==' ') p++;
        if (!*p) break;
        if (strncmp(p,key,kl)==0 && p[kl]=='=' ) {
            p+=kl+1; int n=0;
            if (*p=='"') { p++; while (*p && *p!='"' && n<cap-1) out[n++]=*p++; if (*p=='"') p++; }
            else { while (*p && *p!=' ' && n<cap-1) out[n++]=*p++; }
            out[n]=0; return 1;
        }
        while (*p && *p!=' ') p++;
    }
    return 0;
}
static void emit_tui_props(ctx_t* c, gmb_stmt_t* s)
{    if (!s->set_attr || s->n_set_attr<=0 || !s->set_attr[0] || !*s->set_attr[0]) return;
    const char* spec=s->set_attr[0];
    char nm[64]={0}, tg[64]={0}, tp[128]={0}, ti[32]={0}, vis[32]={0}, en[32]={0}, ts[32]={0};
    unsigned flags=0;
    if (tui_spec_val(spec,"NAME",nm,sizeof nm)) flags|=1;
    if (tui_spec_val(spec,"TAG",tg,sizeof tg)) flags|=2;
    if (tui_spec_val(spec,"TOOLTIP",tp,sizeof tp)) flags|=4;
    if (tui_spec_val(spec,"TABINDEX",ti,sizeof ti)) flags|=8;
    if (tui_spec_val(spec,"VISIBLE",vis,sizeof vis)) flags|=16;
    if (tui_spec_val(spec,"ENABLED",en,sizeof en)) flags|=32;
    if (tui_spec_val(spec,"TABSTOP",ts,sizeof ts)) flags|=64;
    if (strstr(spec, "focus")) flags|=128;
    if (!flags) return;
    ind(c);
    sb_printf(&c->out, "gmb_control_apply_props_handle(gmb_form_last_handle(), %uu, ", flags);
    if (flags&1) sb_printf(&c->out,"\"%s\"",nm); else sb_put(&c->out,"(const char*)0");
    sb_put(&c->out,", ");
    if (flags&2) sb_printf(&c->out,"\"%s\"",tg); else sb_put(&c->out,"(const char*)0");
    sb_put(&c->out,", ");
    if (flags&4) sb_printf(&c->out,"\"%s\"",tp); else sb_put(&c->out,"(const char*)0");
    sb_printf(&c->out, ", %d, %d, %d, %d);\n", (flags&8)?atoi(ti):0, (flags&16)?atoi(vis):0, (flags&32)?atoi(en):0, (flags&64)?atoi(ts):0);
    { char st[256]; if (tui_spec_val(spec,"style",st,sizeof st) || tui_spec_val(spec,"STYLE",st,sizeof st)) { ind(c); sb_printf(&c->out, "gmb_control_apply_style_handle(gmb_form_last_handle(), \"%s\");\n", st); } }
}

int emit_control_stmt(ctx_t* c, gmb_stmt_t* s)
{
    if (s->n_control_args < 1 || !s->control_args[0] || !s->control_args[0]->var) {
        c99_fail(c, s->line, "CONTROL: atteso nome widget");
        return 1;
    }
    const char* cname = s->control_args[0]->var;
    int na = s->n_control_args - 1;
    gmb_expr_t** a = s->control_args + 1;

    /* Alias canonici: DROPDOWN = COMBOBOX (stesso controllo, stesso renderer). */
    if (strcasecmp(cname, "DROPDOWN") == 0) cname = "COMBOBOX";

    /* FIRE EVENT <handle>, "<EVENTO>" — comando a due token: a[0]="EVENT", a[1]=handle, a[2]=ev. */
    if (strcasecmp(cname, "FIRE") == 0 && na >= 3) {
        c99_mark_tui(c); ind(c);
        sb_put(&c->out, "gmb_form_fire_event((int)(");
        emit_expr(c, a[1]);
        sb_put(&c->out, "), gmb_form_ev_index((");
        emit_str_operand(c, a[2]);
        sb_put(&c->out, ").data));\n");
        return 0;
    }

    /* CONTROL <SUB> ... : metodi a runtime per nome (due token, convenzione GMBasic).
     *   CONTROL SET "n","prop",val[,str$] | CONTROL GET "n","prop" INTO var |
     *   CONTROL GETS "n","prop" INTO var$ | CONTROL MOVE "n",x,y[,w,h] |
     *   CONTROL FOCUS "n" | CONTROL ZORDER "n",front. */
    if (strcasecmp(cname, "CONTROL") == 0 && na >= 1 && a[0] && a[0]->var) {
        const char* sub = a[0]->var;
        c99_mark_tui(c); ind(c);
        if (!strcasecmp(sub, "SET") && na >= 4) {
            sb_put(&c->out, "((void)gmb_form_control_set((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (");
            emit_str_operand(c, a[2]); sb_put(&c->out, ").data, (double)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ");
            if (na >= 5) { sb_put(&c->out, "("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data"); } else sb_put(&c->out, "\"\"");
            sb_put(&c->out, "), 0.0);\n");
            return 1;
        }
        if (!strcasecmp(sub, "GET") && na >= 3) {
            var_t* iv = (a[3] && a[3]->kind == GMB_EXPR_VAR && a[3]->var) ? emit_net_target(c, a[3]->var) : NULL;
            if (iv) { emit_net_lhs(c, iv); sb_put(&c->out, " = (gmb_double_t)"); }
            sb_put(&c->out, "gmb_form_control_get((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (");
            emit_str_operand(c, a[2]); sb_put(&c->out, ").data);\n");
            return 1;
        }
        if (!strcasecmp(sub, "GETS") && na >= 3) {
            var_t* iv = (a[3] && a[3]->kind == GMB_EXPR_VAR && a[3]->var) ? emit_net_target(c, a[3]->var) : NULL;
            if (iv) { emit_net_lhs(c, iv); sb_put(&c->out, " = gmb_string_alloc("); }
            sb_put(&c->out, "gmb_form_control_get_str((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (");
            emit_str_operand(c, a[2]); sb_put(&c->out, ").data)");
            if (iv) sb_put(&c->out, ")");
            sb_put(&c->out, ";\n");
            return 1;
        }
        if (!strcasecmp(sub, "MOVE") && na >= 3) {
            sb_put(&c->out, "gmb_form_control_move((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (int)(");
            emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), (int)(");
            if (na >= 5) emit_expr(c, a[4]); else sb_put(&c->out, "0");
            sb_put(&c->out, "), (int)(");
            if (na >= 6) emit_expr(c, a[5]); else sb_put(&c->out, "0");
            sb_put(&c->out, "));\n");
            return 1;
        }
        if (!strcasecmp(sub, "FOCUS") && na >= 2) {
            sb_put(&c->out, "gmb_form_control_focus((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data);\n");
            return 1;
        }
        if (!strcasecmp(sub, "ZORDER") && na >= 3) {
            sb_put(&c->out, "gmb_form_control_zorder((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data, (int)(");
            emit_expr(c, a[2]); sb_put(&c->out, "));\n");
            return 1;
        }
        if (!strcasecmp(sub, "INVALIDATE") && na >= 2) {
            sb_put(&c->out, "gmb_form_control_invalidate((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data);\n");
            return 1;
        }
        if (!strcasecmp(sub, "REFRESH") && na >= 2) {
            sb_put(&c->out, "gmb_form_control_refresh((");
            emit_str_operand(c, a[1]); sb_put(&c->out, ").data);\n");
            return 1;
        }
        if (!strcasecmp(sub, "ADD") && na >= 6) {
            sb_put(&c->out, "gmb_tui_add_usercontrol((int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), (int)("); emit_expr(c, a[4]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[5]); sb_put(&c->out, "), ");
            const char* _inst = (a[1] && a[1]->var) ? a[1]->var : NULL;
            {
                var_t* _iv = _inst ? lookup_var(c, _inst) : NULL;
                if (_iv && _iv->vclass[0]) { sb_printf(&c->out, "(void*)_uc_paint_%s, (void*)&", _iv->vclass); emit_ident(c, _inst); }
                else sb_put(&c->out, "NULL, NULL");
            }
            sb_put(&c->out, ", ");
            /* PARENT: stringa (nome window) oppure variabile (nome di un controllo/UserControl). */
            if (na >= 7 && a[6] && a[6]->kind == GMB_EXPR_STR) { sb_put(&c->out, "("); emit_str_operand(c, a[6]); sb_put(&c->out, ").data"); }
            else if (na >= 7 && a[6] && a[6]->var) sb_printf(&c->out, "\"%s\"", a[6]->var);
            else sb_put(&c->out, "\"\"");
            sb_put(&c->out, ", ");
            /* nome del controllo = nome-variabile dell'istanza (per la composizione annidata). */
            if (_inst) sb_printf(&c->out, "\"%s\"", _inst); else sb_put(&c->out, "\"\"");
            sb_put(&c->out, ");\n");
            /* §11.7-3: cabla i metodi-evento della classe sul firing dell'evento del controllo. */
            if (_inst) { var_t* _iv2 = lookup_var(c, _inst); if (_iv2 && _iv2->vclass[0]) { sb_printf(&c->out, "_uc_ev_bind_%s((void*)&", _iv2->vclass); emit_ident(c, _inst); sb_put(&c->out, ", gmb_form_last_handle());\n"); } }
            /* INTO h: handle del controllo appena aggiunto. */
            if (na >= 8 && a[7] && a[7]->var) { emit_ident(c, a[7]->var); sb_put(&c->out, " = gmb_form_last_handle();\n"); }
            return 1;
        }
        if (!strcasecmp(sub, "DRAW") && na >= 2) {
            /* CONTROL DRAW g, "op", ... → gmb_gfxctx_h_op(handle, CODE, a,b,c,d, s).
             * L'op è un CODICE (0..8), non la stringa: prima si emetteva
             * (int)(gmb_string_alloc("op")) = puntatore troncato → nessun disegno. */
            int code = (na >= 3 && a[2] && a[2]->kind == GMB_EXPR_STR) ? gfxctx_op_code(a[2]->str) : -1;
            sb_put(&c->out, "gmb_gfxctx_h_op((int)gmb_val_to_double(("); emit_expr(c, a[1]); sb_put(&c->out, ")), (int)(");
            sb_printf(&c->out, "%d", code); sb_put(&c->out, ")");
            /* argomenti per operazione (indici 3..6 = argomenti dopo op). */
            int need4 = (code == 2 || code == 3 || code == 4);
            int need3 = (code == 5 || code == 6);
            int needTxt = (code == 7);
            int need2 = (code == 1 || code == 8);
            for (int k = 0; k < 4; k++)
            {
                int src = 3 + k;
                sb_put(&c->out, ", ");
                if (needTxt)
                {
                    if (k == 0) { if (na > src && a[src] && a[src]->kind == GMB_EXPR_STR) { sb_put(&c->out, "(double)0"); } else sb_put(&c->out, "(double)0"); }
                    else if ((k == 1 || k == 2) && na > src) { sb_put(&c->out, "(double)("); emit_expr(c, a[src]); sb_put(&c->out, ")"); }
                    else sb_put(&c->out, "(double)0");
                }
                else if (need4 || need3 || need2)
                {
                    int lim = need4 ? 4 : (need3 ? 3 : 2);
                    if (k < lim && na > src && a[src]) { sb_put(&c->out, "(double)("); emit_expr(c, a[src]); sb_put(&c->out, ")"); }
                    else sb_put(&c->out, "(double)0");
                }
                else sb_put(&c->out, "(double)0");
            }
            sb_put(&c->out, ", ");
            if (needTxt && na >= 4 && a[3] && a[3]->kind == GMB_EXPR_STR) { sb_put(&c->out, "("); emit_str_operand(c, a[3]); sb_put(&c->out, ").data"); }
            else if (na >= 8 && a[7] && a[7]->kind == GMB_EXPR_STR) { sb_put(&c->out, "("); emit_str_operand(c, a[7]); sb_put(&c->out, ").data"); }
            else sb_put(&c->out, "\"\"");
            sb_put(&c->out, ");\n");
            return 1;
        }
        if (!strcasecmp(sub, "REMOVE") && na >= 2) { sb_put(&c->out, "/* CONTROL REMOVE (P0) */\n"); return 1; }
        c99_fail(c, s->line, "CONTROL: sottocomando non supportato");
        return 1;
    }

    /* BUTTON x, y, w, h, "label" ON CLICK <sub> [STYLE "spec"] — bottone non-modale:
     * registra la callback (SUB GPM). Il fire avviene in gmb_form_poll() sul worker,
     * mai sul thread SDL (regola GPM). Non ha INTO. */
    if (strcasecmp(cname, "BUTTON") == 0 && na >= 6) {
        c99_mark_tui(c);
        ind(c);
        sb_put(&c->out, a[6] ? "{ gmb_form_add_button_css((int)(" : "{ gmb_control_add_button((int)(");
        emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
        emit_expr(c, a[3]); sb_put(&c->out, "), (");
        emit_str_operand(c, a[4]);
        sb_put(&c->out, ").data, ");
        if (a[5] && a[5]->var) { sb_put(&c->out, "(gmb_form_cb)gmb_user_"); emit_ident(c, a[5]->var); }
        else sb_put(&c->out, "(gmb_form_cb)0");
        if (a[6]) { sb_put(&c->out, ", ("); emit_str_operand(c, a[6]); sb_put(&c->out, ").data"); }
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        sb_put(&c->out, "}\n");
        return 0;
    }

    /* Widget PIXEL: CHECK/RADIO/TRACK/DROPDOWN/PROGRESS ... INTO var. L'ultimo arg è
     * la variabile (double*) in cui il widget scrive il valore. */
    {
        gmb_expr_t* _v = (s->n_control_args > 0) ? s->control_args[s->n_control_args - 1] : NULL;
        const char* _vn = (_v && _v->var) ? _v->var : NULL;
        if (strcasecmp(cname, "PROGRESS") == 0 && na >= 3) {
            c99_mark_tui(c); ind(c);
            if (na >= 4 && _vn) {
                sb_put(&c->out, "{ gmb_form_add_progress((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
                emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), &(");
                emit_ident(c, _vn);        sb_put(&c->out, ")); ");
            } else {
                sb_put(&c->out, "{ gmb_form_add_progress_v((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
                emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (double)(");
                if (na >= 4) emit_expr(c, a[3]); else sb_put(&c->out, "0");
                sb_put(&c->out, ")); ");
            }
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXTOGGLE") == 0 && na >= 4 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_toggle((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), ("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXSPINNER") == 0 && na >= 7 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_spinner((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (double)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), (double)("); emit_expr(c, a[4]); sb_put(&c->out, "), (double)(");
            emit_expr(c, a[5]); sb_put(&c->out, "), &("); emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXSTATUSBAR") == 0 && na >= 4) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_statusbar((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (");
            emit_str_operand(c, a[3]); sb_put(&c->out, ").data); }\n"); return 0;
        }
        if (strcasecmp(cname, "PIXTAB") == 0 && na >= 4 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_tabs((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), ("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXLISTBOX") == 0 && na >= 6 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_listbox((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXCOLORPICKER") == 0 && na >= 3 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_colorpicker((int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[0]); sb_put(&c->out, "), NULL, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        /* Alias TUI (row,col,...) -> widget pixel (x=col, y=row). */
        if (strcasecmp(cname, "PIXSLIDER") == 0 && na >= 6 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_track((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (double)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), (double)("); emit_expr(c, a[4]); sb_put(&c->out, "), &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXRADIO") == 0 && na >= 4 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_radio((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), ("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXCOMBOBOX") == 0 && na >= 5 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_dropdown((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (");
            emit_str_operand(c, a[3]); sb_put(&c->out, ").data, &("); emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXMENU") == 0 && na >= 4 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_menubar((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), ("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXPRINTERDIALOG") == 0) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ static gmb_form_printerdialog(");
            /* target INTO trailingi: (p$, copies, duplex) */
            gmb_expr_t* t1 = (s->n_control_args >= 2) ? s->control_args[s->n_control_args - 3 > 0 ? s->n_control_args - 3 : 1] : NULL;
            int base = s->n_control_args - 3; if (base < 1) base = 1;
            gmb_expr_t* v1 = (s->n_control_args > base) ? s->control_args[base] : NULL;
            gmb_expr_t* v2 = (s->n_control_args > base + 1) ? s->control_args[base + 1] : NULL;
            gmb_expr_t* v3 = (s->n_control_args > base + 2) ? s->control_args[base + 2] : NULL;
            (void)t1;
            if (v1 && v1->var) { var_t* vv = lookup_var(c, v1->var); if (vv && vv->type == TY_STR) { sb_put(&c->out, "(void*)&("); emit_ident(c, v1->var); sb_put(&c->out, "), "); } else sb_put(&c->out, "NULL, "); } else sb_put(&c->out, "NULL, ");
            if (v2 && v2->var) { sb_put(&c->out, "&("); emit_ident(c, v2->var); sb_put(&c->out, "), "); } else sb_put(&c->out, "NULL, ");
            if (v3 && v3->var) { sb_put(&c->out, "&("); emit_ident(c, v3->var); sb_put(&c->out, ")"); } else sb_put(&c->out, "NULL");
            sb_put(&c->out, "); }\n"); return 0;
        }
        if (strcasecmp(cname, "PIXFILEDIALOG") == 0 && na >= 1 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_filedialog(40, 40, 320, 220, \".\", (void*)&(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXTREEVIEW") == 0 && na >= 6 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_tree((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data, &(");
            emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXTBROWSE") == 0 && na >= 7 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_grid((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data, (");
            emit_str_operand(c, a[5]); sb_put(&c->out, ").data, &("); emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXGRID") == 0 && na >= 7 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_grid((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data, (");
            emit_str_operand(c, a[5]); sb_put(&c->out, ").data, &("); emit_ident(c, _vn);        sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
        emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "PIXDATEPICKER") == 0 && na >= 3 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_datepicker((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (void*)&("); emit_ident(c, _vn); sb_put(&c->out, "), ");
            if (na >= 4 && a[2] && a[2]->kind == GMB_EXPR_STR) { sb_put(&c->out, "("); emit_str_operand(c, a[2]); sb_put(&c->out, ").data"); }
            else sb_put(&c->out, "\"dd/mm/yyyy\"");
            sb_put(&c->out, "); ");
            if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }

        if ((strcasecmp(cname, "TEXTBOX") == 0 || strcasecmp(cname, "PIXTEXTBOX") == 0) && na >= 5 && _vn) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_form_add_textbox((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), (void*)&("); emit_ident(c, _vn); sb_put(&c->out, ")); ");
 if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0]) { sb_put(&c->out, "gmb_form_set_last_style(\""); for (const char* _pp = s->set_attr[0]; *_pp; _pp++) { if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\"); char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch); } sb_put(&c->out, "\"); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }

        if (strcasecmp(cname, "CANVAS") == 0 && na >= 4) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_tui_add_canvas((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, ")); ");
            if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if ((strcasecmp(cname, "IMAGE") == 0 || strcasecmp(cname, "IMAGEBUTTON") == 0 || strcasecmp(cname, "QR") == 0 || strcasecmp(cname, "RICHTEXT") == 0) && na >= 5) {
            const char* fn = strcasecmp(cname, "IMAGE") == 0 ? "gmb_tui_add_image"
                           : strcasecmp(cname, "IMAGEBUTTON") == 0 ? "gmb_tui_add_imagebutton"
                           : strcasecmp(cname, "QR") == 0 ? "gmb_tui_add_qr"
                           : "gmb_tui_add_richtext";
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ "); sb_put(&c->out, fn); sb_put(&c->out, "((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data); ");
            if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "FUNCTIONKEYBAR") == 0 && na >= 3) {
            c99_mark_tui(c); ind(c);
            /* argomenti BASIC: row, col → factory (x=col, y=row). */
            sb_put(&c->out, "{ gmb_tui_add_fkeybar((int)("); emit_expr(c, a[1]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[0]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), ");
            if (na >= 4) { sb_put(&c->out, "("); emit_str_operand(c, a[3]); sb_put(&c->out, ").data); "); }
            else sb_put(&c->out, "\"\"); ");
            if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "CANDLESTICK") == 0 && na >= 4) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_tui_add_candlestick((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ");
            if (na >= 5) { sb_put(&c->out, "("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data); "); }
            else sb_put(&c->out, "\"\"); ");
            if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if (strcasecmp(cname, "CHART") == 0 && na >= 4) {
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ gmb_tui_add_chart((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ");
            if (na >= 5) { sb_put(&c->out, "("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data); "); }
            else sb_put(&c->out, "\"\"); ");
            if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
        if ((strcasecmp(cname, "HALFBLOCK") == 0 || strcasecmp(cname, "TILING") == 0) && na >= 4) {
            const char* fn = strcasecmp(cname, "HALFBLOCK") == 0 ? "gmb_tui_add_halfblock" : "gmb_tui_add_tiling";
            c99_mark_tui(c); ind(c);
            sb_put(&c->out, "{ "); sb_put(&c->out, fn); sb_put(&c->out, "((int)("); emit_expr(c, a[0]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[1]); sb_put(&c->out, "), (int)("); emit_expr(c, a[2]); sb_put(&c->out, "), (int)(");
            emit_expr(c, a[3]); sb_put(&c->out, "), ");
            if (na >= 5) { sb_put(&c->out, "("); emit_str_operand(c, a[4]); sb_put(&c->out, ").data"); }
            else sb_put(&c->out, "\"\"");
            sb_put(&c->out, "); ");
            if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
            emit_ctrl_events(c, s); sb_put(&c->out, "}\n"); return 0;
        }
    }
    c99_mark_tui(c);   /* prologo: #include widget + modal runner (setter: ctx_t opaco) */
    const char* into_nm = NULL;
    if (s->control_into > 0 && s->control_into < s->n_control_args) {
        gmb_expr_t* _t = s->control_args[s->control_into];
        if (_t && _t->var) into_nm = _t->var;
    }
    var_t* _iv = into_nm ? emit_net_target(c, into_nm) : NULL;
    /* nr = argomenti POSIZIONALI (INTO escluso, anche multi-target). */
    int nr = (s->control_into > 0) ? s->control_into - 1 : na;

    if (strcasecmp(cname, "CHECKBOX") == 0 && na >= 3) {
        /* Forme: CHECKBOX x, y, label  |  CHECKBOX x, y, w, label. */
        c99_mark_tui(c); ind(c);
        sb_put(&c->out, "{ gmb_control_add_checkbox((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)(");
        if (nr >= 4) emit_expr_dflt(c, a[2], "20"); else sb_put(&c->out, "20");
        sb_put(&c->out, "), "); emit_str_operand(c, nr >= 4 ? a[3] : a[2]); sb_put(&c->out, ".data, ");
        if (into_nm) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "CONTEXTMENU") == 0 && na >= 4) {
        /* Context menu: x, y, w, items (altezza = righe voci). */
        ind(c); sb_put(&c->out, "{ static gmb_context_menu_t _tui_cm; ");
        ctl_split_csv(c, a[3], "__cmi", "__cmn", NULL);
        sb_put(&c->out, "gmb_context_menu_init(&_tui_cm, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "200"); sb_put(&c->out, "), (int)__cmn); ");
        sb_put(&c->out, "gmb_context_menu_set_items(&_tui_cm, __cmi, __cmn); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_cm.ctrl); ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "gmb_tui_bind_value(_tui_cm.ctrl.handle, (double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")); "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "LISTBOX") == 0 && na >= 5) {
        /* Factory polimorfica. Argomenti: x, y, w, h, items. Items: letterale/array/CSV. */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, a[4], "__lbiv", "__lbic", NULL);
        ctl_join_csv_from(c, "__lbiv", "__lbic", "__lbcsv");
        sb_put(&c->out, "int __tui_h = gmb_control_add_listbox((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "20"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "5"); sb_put(&c->out, "), __lbcsv, ");
        if (_iv && _iv->type != TY_STR && into_nm) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (_iv && _iv->type == TY_STR && into_nm) {
            sb_put(&c->out, "if (__tui_h >= 0) gmb_tui_bind_str(__tui_h, (gmb_string_t*)&(");
            emit_ident(c, (char*)into_nm);
            sb_put(&c->out, ")); ");
        }
        /* Clausole (NAME/TAG/TOOLTIP/VISIBLE/…/STYLE e COLOR/COLORFOCUS/COLORSEL/COLORERROR):
         * il parser le fonde in set_attr[0]; qui si applicano al controllo appena creato.
         * Senza, `LISTBOX ... COLOR "W+/B" COLORFOCUS "GR+/B"` perdeva i colori
         * (tui/listbox_color_states_parity: il PARSE ora passa, ma la spec non era emessa). */
        emit_tui_props(c, s);
        if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0] && *s->set_attr[0]) {
            sb_put(&c->out, "gmb_form_set_last_style(\"");
            for (const char* _pp = s->set_attr[0]; *_pp; _pp++) {
                if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\");
                char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch);
            }
            sb_put(&c->out, "\"); ");
        }
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "MENU") == 0 && na >= 4) {
        /* Menubar = widget form (pixel): x, y, items, INTO. */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, a[3], "__mn", "__mnn", NULL);
        ctl_join_csv_from(c, "__mn", "__mnn", "__mncsv");
        sb_put(&c->out, "int __tui_h = gmb_form_add_menubar((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), __mncsv, ");
        if (_iv && _iv->type != TY_STR && into_nm) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (_iv && _iv->type == TY_STR && into_nm) {
            sb_put(&c->out, "if (__tui_h >= 0) gmb_tui_bind_str(__tui_h, (gmb_string_t*)&(");
            emit_ident(c, (char*)into_nm);
            sb_put(&c->out, ")); ");
        }
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "TOGGLE") == 0 && na >= 3) {
        /* Forme: TOGGLE x, y, label  |  TOGGLE x, y, w, label. */
        ind(c);
        sb_put(&c->out, "{ gmb_control_add_toggle((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)(");
        if (nr >= 4) emit_expr_dflt(c, a[2], "20"); else sb_put(&c->out, "20");
        sb_put(&c->out, "), "); emit_str_operand(c, nr >= 4 ? a[3] : a[2]); sb_put(&c->out, ".data, ");
        if (into_nm) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "COMBOBOX") == 0 && na >= 4) {
        /* Factory polimorfica. Argomenti: x, y, w, items.
         * INTO: numerico → indice (double*); stringa → testo selezionato (bind_str). */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, a[3], "__cbiv", "__cbic", NULL);
        ctl_join_csv_from(c, "__cbiv", "__cbic", "__cbcsv");
        sb_put(&c->out, "int __tui_h = gmb_control_add_combobox((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "20"); sb_put(&c->out, "), __cbcsv, ");
        if (_iv && _iv->type != TY_STR && into_nm) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (_iv && _iv->type == TY_STR && into_nm) {
            sb_put(&c->out, "if (__tui_h >= 0) gmb_tui_bind_str(__tui_h, (gmb_string_t*)&(");
            emit_ident(c, (char*)into_nm);
            sb_put(&c->out, ")); ");
        }
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if ((strcasecmp(cname, "RADIO") == 0) && na >= 3) {
        /* Forme: RADIO x, y, items  |  RADIO x, y, w, items. */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, nr >= 4 ? a[3] : a[2], "__rgiv", "__rgic", NULL);
        ctl_join_csv_from(c, "__rgiv", "__rgic", "__rgcsv");
        sb_put(&c->out, "int __tui_h = gmb_control_add_radio((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); if (nr >= 4) emit_expr_dflt(c, a[2], "20"); else sb_put(&c->out, "20"); sb_put(&c->out, "), __rgcsv, ");
        if (_iv && _iv->type != TY_STR && into_nm) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (_iv && _iv->type == TY_STR && into_nm) {
            sb_put(&c->out, "if (__tui_h >= 0) gmb_tui_bind_str(__tui_h, (gmb_string_t*)&(");
            emit_ident(c, (char*)into_nm);
            sb_put(&c->out, ")); ");
        }
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "SLIDER") == 0 && na >= 6) {
        /* Track polimorfico (form in grafica). Argomenti: x, y, w, min, max, valore. */
        ind(c);
        sb_put(&c->out, "{ gmb_form_add_track((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "200"); sb_put(&c->out, "), (double)("); emit_expr_dflt(c, a[3], "0");
        sb_put(&c->out, "), (double)("); emit_expr_dflt(c, a[4], "100"); sb_put(&c->out, "), ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); gmb_form_set_last_value((double)("); emit_expr_dflt(c, a[5], "0"); sb_put(&c->out, ")); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "NUMERICUPDOWN") == 0 && na >= 6) {
        /* Factory polimorfica. Argomenti: x, y, w, min, max, step. */
        ind(c);
        sb_put(&c->out, "{ gmb_control_add_numericupdown((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "20"); sb_put(&c->out, "), (double)("); emit_expr_dflt(c, a[3], "0");
        sb_put(&c->out, "), (double)("); emit_expr_dflt(c, a[4], "100"); sb_put(&c->out, "), (double)("); emit_expr_dflt(c, a[5], "1"); sb_put(&c->out, "), ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "STARRATING") == 0 && na >= 5) {
        /* Argomenti: x, y, w, max, valore. */
        ind(c);
        sb_put(&c->out, "{ gmb_control_add_starrating((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "20"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "5");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[4], "0"); sb_put(&c->out, "), ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "SCROLLBAR") == 0 && na >= 6) {
        /* Argomenti: x, y, len, min, max, valore. */
        ind(c);
        sb_put(&c->out, "{ gmb_control_add_scrollbar((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "20"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[4], "100"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[5], "0"); sb_put(&c->out, "), ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "SCROLLVIEW") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_scroll_view_t _tui_sv; ");
        sb_printf(&c->out, "gmb_scroll_view_init(&_tui_sv, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _sv_tmp = ctl_split_csv(c, a[4], "__svi", "__svn", NULL);
        sb_put(&c->out, "gmb_scroll_view_set_content(&_tui_sv, __svi, __svn); ");
        if (_sv_tmp) sb_printf(&c->out, "for (int __i3 = 0; __i3 < __svn; __i3++) gmb_string_free(&%s[__i3]); free(%s); ", _sv_tmp, _sv_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_sv.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "LABEL") == 0 && na >= 4) {
        ind(c); sb_put(&c->out, "{ static gmb_label_t _tui_lb; ");
        sb_printf(&c->out, "gmb_label_init(&_tui_lb, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), "); emit_str_operand(c, a[3]); sb_put(&c->out, ".data); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_lb.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "DATALIST") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_data_list_t _tui_dl; ");
        sb_printf(&c->out, "gmb_data_list_init(&_tui_dl, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _dl_tmp = ctl_split_csv(c, a[4], "__dli", "__dln", NULL);
        sb_put(&c->out, "gmb_data_list_set_items(&_tui_dl, __dli, __dln); ");
        if (_dl_tmp) sb_printf(&c->out, "for (int __i4 = 0; __i4 < __dln; __i4++) gmb_string_free(&%s[__i4]); free(%s); ", _dl_tmp, _dl_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_dl.ctrl); ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "gmb_tui_bind_value(_tui_dl.ctrl.handle, (double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")); "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "PROPERTYGRID") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_property_grid_t _tui_pg; ");
        sb_printf(&c->out, "gmb_property_grid_init(&_tui_pg, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _pg_tmp = ctl_split_csv(c, a[4], "__pgi", "__pgn", NULL);
        sb_put(&c->out, "for (int __i5 = 0; __i5 < __pgn; __i5++) gmb_property_grid_add_kv(&_tui_pg, __pgi[__i5]); ");
        if (_pg_tmp) sb_printf(&c->out, "for (int __i6 = 0; __i6 < __pgn; __i6++) gmb_string_free(&%s[__i6]); free(%s); ", _pg_tmp, _pg_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_pg.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "LOGVIEW") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_log_view_t _tui_lv; ");
        sb_printf(&c->out, "gmb_log_view_init(&_tui_lv, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _lv_tmp = ctl_split_csv(c, a[4], "__lvi", "__lvn", NULL);
        sb_put(&c->out, "for (int __i7 = 0; __i7 < __lvn; __i7++) gmb_log_view_push(&_tui_lv, 0, \"\", __lvi[__i7]); ");
        if (_lv_tmp) sb_printf(&c->out, "for (int __i8 = 0; __i8 < __lvn; __i8++) gmb_string_free(&%s[__i8]); free(%s); ", _lv_tmp, _lv_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_lv.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "HEXVIEW") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_hex_view_t _tui_hv; ");
        sb_printf(&c->out, "gmb_hex_view_init(&_tui_hv, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        sb_put(&c->out, "gmb_hex_view_set_data(&_tui_hv, (const unsigned char*)("); emit_str_operand(c, a[4]); sb_put(&c->out, ".data), (int)("); emit_str_operand(c, a[4]); sb_put(&c->out, ".length)); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_hv.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "BIGTEXT") == 0 && na >= 3) {
        ind(c); sb_put(&c->out, "{ static gmb_big_text_t _tui_bt; ");
        sb_printf(&c->out, "gmb_big_text_init(&_tui_bt, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, ")); ");
        sb_put(&c->out, "gmb_big_text_set_text(&_tui_bt, "); emit_str_operand(c, a[2]); sb_put(&c->out, ".data); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_bt.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "CODEAREA") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_code_area_t _tui_ca; ");
        sb_printf(&c->out, "gmb_code_area_init(&_tui_ca, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _ca_tmp = ctl_split_csv(c, a[4], "__cai", "__can", NULL);
        sb_put(&c->out, "gmb_code_area_set_lines(&_tui_ca, __cai, __can); ");
        if (_ca_tmp) sb_printf(&c->out, "for (int __i9 = 0; __i9 < __can; __i9++) gmb_string_free(&%s[__i9]); free(%s); ", _ca_tmp, _ca_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_ca.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "REORDERLISTBOX") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_reorder_listbox_t _tui_rl; ");
        sb_printf(&c->out, "gmb_reorder_listbox_init(&_tui_rl, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _rl_tmp = ctl_split_csv(c, a[4], "__rli", "__rln", NULL);
        sb_put(&c->out, "gmb_reorder_listbox_set_items(&_tui_rl, __rli, __rln); ");
        if (_rl_tmp) sb_printf(&c->out, "for (int __i10 = 0; __i10 < __rln; __i10++) gmb_string_free(&%s[__i10]); free(%s); ", _rl_tmp, _rl_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_rl.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "ALERTBOX") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_alert_box_t _tui_ab; ");
        sb_printf(&c->out, "gmb_alert_box_init(&_tui_ab, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        sb_put(&c->out, "snprintf(_tui_ab.message, sizeof(_tui_ab.message), \"%s\", "); emit_str_operand(c, a[4]); sb_put(&c->out, ".data); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_ab.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "CONFIRMATIONBOX") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_confirmation_box_t _tui_cb; ");
        sb_printf(&c->out, "gmb_confirmation_box_init(&_tui_cb, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        sb_put(&c->out, "gmb_confirmation_box_with_message(&_tui_cb, "); emit_str_operand(c, a[4]); sb_put(&c->out, ".data); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_cb.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); sb_put(&c->out, " = (gmb_double_t)_tui_cb.result; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "FILEEXPLORER") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_file_explorer_t _tui_fe; ");
        sb_put(&c->out, "gmb_file_explorer_init(&_tui_fe, "); emit_str_operand(c, a[4]); sb_put(&c->out, ".data); ");
        sb_printf(&c->out, "gmb_file_explorer_set_bounds(&_tui_fe, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_fe.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "DASHBOARD") == 0 && na >= 5) {
        ind(c); sb_put(&c->out, "{ static gmb_dashboard_t _tui_db; ");
        sb_printf(&c->out, "gmb_dashboard_init(&_tui_db, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ")); ");
        const char* _db_tmp = ctl_split_csv(c, a[4], "__dbi", "__dbn", NULL);
        sb_put(&c->out, "for (int __i11 = 0; __i11 + 3 < __dbn; __i11 += 4) gmb_dashboard_add_metric(&_tui_db, __dbi[__i11], (float)atof(__dbi[__i11 + 1]), (float)atof(__dbi[__i11 + 2]), __dbi[__i11 + 3]); ");
        if (_db_tmp) sb_printf(&c->out, "for (int __i12 = 0; __i12 < __dbn; __i12++) gmb_string_free(&%s[__i12]); free(%s); ", _db_tmp, _db_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_db.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv); if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = (gmb_double_t)0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "TAB") == 0 && nr >= 3 && (na < 6 || s->control_into > 0)) {
        /* Factory polimorfica. Forme: x,y,items  |  x,y,w,items  (h default). */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, nr >= 4 ? a[3] : a[2], "__tb", "__tbn", NULL);
        ctl_join_csv_from(c, "__tb", "__tbn", "__tbcsv");
        sb_put(&c->out, "gmb_control_add_tab((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); if (nr >= 4) emit_expr_dflt(c, a[2], "200"); else sb_put(&c->out, "200"); sb_put(&c->out, "), __tbcsv, ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "TAB") == 0 && na >= 6) {
        ind(c); sb_put(&c->out, "{ static gmb_tab_t _tui_tb; ");
        sb_printf(&c->out, "gmb_tab_init(&_tui_tb, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, ")); ");
        const char* _tb_tmp = ctl_split_csv(c, a[4], "__tbi", "__tbn", NULL);
        sb_put(&c->out, "gmb_tab_set_tabs(&_tui_tb, __tbi, __tbn); ");
        if (_tb_tmp) sb_printf(&c->out, "for (int __i2 = 0; __i2 < __tbn; __i2++) gmb_string_free(&%s[__i2]); free(%s); ", _tb_tmp, _tb_tmp);
        sb_put(&c->out, "gmb_tui_control_register(&_tui_tb.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv);
            if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"0\"); ");
            else sb_put(&c->out, " = 0.0; ");
        }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "GRID") == 0 && na >= 6) {
        /* Factory polimorfica. Argomenti: x, y, w, h, headers, data. */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, a[4], "__gh", "__ghn", NULL);
        ctl_join_csv_from(c, "__gh", "__ghn", "__ghcsv");
        ctl_split_csv(c, a[5], "__gd", "__gdn", NULL);
        ctl_join_csv_from(c, "__gd", "__gdn", "__gdcsv");
        sb_put(&c->out, "gmb_control_add_grid((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "200"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "100");
        sb_put(&c->out, "), __ghcsv, __gdcsv, ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "DATEPICKER") == 0 && na >= 2) {
        ind(c); sb_put(&c->out, "{ static gmb_date_picker_t _tui_dp; ");
        sb_printf(&c->out, "gmb_date_picker_init(&_tui_dp, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), 22); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_dp.ctrl); char __dps[64]; gmb_date_picker_get_formatted(&_tui_dp, NULL, __dps, (int)sizeof(__dps)); ");
        if (_iv) { emit_net_lhs(c, _iv);
            if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(__dps); ");
            else sb_put(&c->out, " = 0.0; ");
        }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "COLORPICKER") == 0 && na >= 3) {
        /* Polimorfico: widget PIXEL in grafica (palette di colori), colorpicker a
         * celle nel TUI. Prima passava sempre dal controllo a celle: in grafica
         * appariva come una griglia di riquadri VUOTI (nessuna palette). */
        ind(c);
        sb_put(&c->out, "{ gmb_control_add_colorpicker((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)(");
        emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "208");
        sb_put(&c->out, "), \"\", ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "TREEVIEW") == 0 && na >= 5) {
        /* Factory polimorfica. Argomenti: x, y, w, h, items. */
        ind(c);
        sb_put(&c->out, "{ ");
        ctl_split_csv(c, a[4], "__tv", "__tvn", NULL);
        ctl_join_csv_from(c, "__tv", "__tvn", "__tvcsv");
        sb_put(&c->out, "gmb_control_add_tree((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "200"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "100");
        sb_put(&c->out, "), __tvcsv, ");
        if (_iv && _iv->type != TY_STR) { sb_put(&c->out, "(double*)&("); emit_ident(c, (char*)into_nm); sb_put(&c->out, ")"); }
        else sb_put(&c->out, "(double*)0");
        sb_put(&c->out, "); ");
        if (s->id_var && *s->id_var) { emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle(); "); }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "TBROWSE") == 0 && na >= 6) {
        /* TBROWSE row, col, width, height, cols, data [, opts] INTO var — porting
         * fedele dell'emissione emitter C# (C99CodeEmitter.cs case TBrowseStmt):
         * colonne da lista di tuple [header,width,type,align], righe da lista di liste,
         * loop modale con gmb_browse_apply_key, assegnazione dell'indice riga. */
        var_t* _iv_tb = NULL;
        { int _n2 = s->n_control_args; gmb_expr_t* _t2 = (_n2 > 0) ? s->control_args[_n2 - 1] : NULL;
          if (_n2 > 7 && _t2 && _t2->kind == GMB_EXPR_VAR && _t2->var) _iv_tb = emit_net_target(c, _t2->var); }
        ind(c); sb_put(&c->out, "{\n");
        ind(c); sb_put(&c->out, "gmb_rect_t _tb_area = { (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)(("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, ") > 0 ? ("); emit_expr_dflt(c, a[2], "0"); sb_put(&c->out, ") : 40), (int)(("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ") > 0 ? ("); emit_expr_dflt(c, a[3], "0"); sb_put(&c->out, ") : 6) };\n");
        ind(c); sb_put(&c->out, "gmb_browse_t* _tb = gmb_browse_new(&_tb_area, 0, NULL, NULL); int _tbRow = -1;\n");
        ind(c); sb_put(&c->out, "if (_tb) {\n");
        ind(c); sb_put(&c->out, "gmb_list_t _tb_cols = "); ctl_as_list(c, a[4]); sb_put(&c->out, ";\n");
        ind(c); sb_put(&c->out, "for (size_t _ti = 0; _ti < gmb_list_count(_tb_cols) && _ti < 32; _ti++) { gmb_list_t _tci = gmb_val_as_list(gmb_list_get(_tb_cols, (int)_ti)); if (gmb_list_count(_tci) >= 2) { gmb_browse_column_t _tcol; memset(&_tcol, 0, sizeof(_tcol)); gmb_string_t _tcs = gmb_val_to_string(gmb_list_get(_tci, 0)); snprintf(_tcol.header, sizeof(_tcol.header), \"%s\", _tcs.data ? _tcs.data : \"\"); gmb_val_t _twv = gmb_list_get(_tci, 1); _tcol.width = _twv.kind == GMB_TYPE_INT ? (int)_twv.v_int : (int)_twv.v_double; if (gmb_list_count(_tci) >= 3) { gmb_string_t _tts = gmb_val_to_string(gmb_list_get(_tci, 2)); const char* _tt = _tts.data; if (_tt && *_tt) { if (!strncmp(_tt,\"N\",1)||!strncmp(_tt,\"I\",1)) _tcol.data_type = GMB_DT_INT; else if (!strncmp(_tt,\"F\",1)) _tcol.data_type = GMB_DT_FLOAT; else if (!strncmp(_tt,\"L\",1)||!strncmp(_tt,\"B\",1)) _tcol.data_type = GMB_DT_BOOL; else if (!strncmp(_tt,\"D\",1)) _tcol.data_type = GMB_DT_DATE; else if (!strncmp(_tt,\"X\",1)) _tcol.data_type = GMB_DT_CHOICE; else _tcol.data_type = GMB_DT_TEXT; } } if (gmb_list_count(_tci) >= 4) { gmb_string_t _tas = gmb_val_to_string(gmb_list_get(_tci, 3)); const char* _ta = _tas.data; if (_ta && strncmp(_ta,\"R\",1)==0) _tcol.align = GMB_ALIGN_RIGHT; else if (_ta && strncmp(_ta,\"C\",1)==0) _tcol.align = GMB_ALIGN_CENTER; else _tcol.align = GMB_ALIGN_LEFT; } gmb_browse_add_column(_tb, &_tcol); } }\n");
        ind(c); sb_put(&c->out, "if (gmb_browse_col_count(_tb) == 0) { gmb_browse_column_t _tdfl; memset(&_tdfl, 0, sizeof(_tdfl)); snprintf(_tdfl.header, sizeof(_tdfl.header), \"Field\"); _tdfl.width = 10; gmb_browse_add_column(_tb, &_tdfl); }\n");
        ind(c); sb_put(&c->out, "gmb_list_t _tb_rows = "); ctl_as_list(c, a[5]); sb_put(&c->out, "; int _tbNRows = (int)gmb_list_count(_tb_rows);\n");
        ind(c); sb_put(&c->out, "static gmb_vec_ds_t _tvd; gmb_vec_ds_make(&_tvd, _tbNRows > 64 ? 64 : _tbNRows, gmb_browse_col_count(_tb));\n");
        ind(c); sb_put(&c->out, "for (size_t _tr = 0; _tr < gmb_list_count(_tb_rows) && _tr < 64; _tr++) { gmb_list_t _trl = gmb_val_as_list(gmb_list_get(_tb_rows, (int)_tr)); for (int _tc2 = 0; _tc2 < (int)gmb_list_count(_trl) && _tc2 < gmb_browse_col_count(_tb); _tc2++) { gmb_string_t _tcv = gmb_val_to_string(gmb_list_get(_trl, _tc2)); gmb_vec_ds_set(&_tvd, (int)_tr, _tc2, _tcv.data ? _tcv.data : \"\"); } }\n");
        ind(c); sb_put(&c->out, "static gmb_browse_datasource_t _tbds; gmb_vec_ds_datasource(&_tvd, &_tbds); gmb_browse_t* _tbReal = gmb_browse_new(&_tb_area, gmb_browse_col_count(_tb), NULL, &_tbds);\n");
        ind(c); sb_put(&c->out, "if (_tbReal) { for (int _tc3 = 0; _tc3 < gmb_browse_col_count(_tb); _tc3++) { gmb_browse_column_t* _tcw = gmb_browse_get_column(_tb, _tc3); if (_tcw) gmb_browse_set_column(_tbReal, _tc3, _tcw); } gmb_browse_free(_tb); _tb = _tbReal; }\n");
        /* Non-bloccante (parità VM-C/IL/PCode): registra il browse nel registry TUI (niente
         * loop modale) — così CONTROL SET/GET (showheader/toprow/sort/filter…) e NAME lo trovano. */
        ind(c); sb_put(&c->out, "gmb_tui_control_register(&_tb->ctrl); _tbRow = gmb_browse_row_pos(_tb); }\n");
        if (_iv_tb) { emit_net_lhs(c, _iv_tb); if (_iv_tb->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"0\"); "); else sb_put(&c->out, " = (gmb_double_t)_tbRow; "); }
        ind(c); sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "PRINTERDIALOG") == 0 && na >= 3) {
        /* PRINTERDIALOG [titolo,] stampante$, copie, duplex: non bloccante (parità
         * emitter C#: stampante "", copie 1, duplex 0). I 3 target sono a[0],a[1],a[2]. */
        ind(c); sb_put(&c->out, "{ static gmb_printer_dialog_t _tui_pd; gmb_printer_dialog_init(&_tui_pd); gmb_tui_control_register(&_tui_pd.ctrl); ");
        var_t* pv = a[0] && a[0]->kind == GMB_EXPR_VAR ? emit_net_target(c, a[0]->var) : NULL;
        var_t* cv = a[1] && a[1]->kind == GMB_EXPR_VAR ? emit_net_target(c, a[1]->var) : NULL;
        var_t* dv = a[2] && a[2]->kind == GMB_EXPR_VAR ? emit_net_target(c, a[2]->var) : NULL;
        if (pv) { emit_net_lhs(c, pv); if (pv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); "); else sb_put(&c->out, " = 0.0; "); }
        if (cv) { emit_net_lhs(c, cv); if (cv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"1\"); "); else sb_put(&c->out, " = 1.0; "); }
        if (dv) { emit_net_lhs(c, dv); if (dv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"0\"); "); else sb_put(&c->out, " = 0.0; "); }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "FILEDIALOG") == 0 && na >= 2) {
        ind(c); sb_put(&c->out, "{ static gmb_file_dialog_t _tui_fd; ");
        sb_put(&c->out, "gmb_file_dialog_init(&_tui_fd, 0, 0, 0, 0);   /* 0 -> dimensioni/posizione proprie (centrata) */ ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_fd.ctrl); ");
        sb_put(&c->out, "gmb_file_dialog_load(&_tui_fd, \".\", "); emit_str_operand(c, a[0]); sb_put(&c->out, ".data); ");
        if (_iv) { emit_net_lhs(c, _iv);
            if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"\"); ");
            else sb_put(&c->out, " = 0.0; ");
        }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "STATUSBAR") == 0 && na >= 1) {
        /* Forme: STATUSBAR "testo"  |  STATUSBAR x, y, w, "testo". */
        ind(c);
        sb_put(&c->out, "{ ");
        if (na >= 4) {
            sb_put(&c->out, "gmb_form_add_statusbar((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
            sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "200"); sb_put(&c->out, "), "); emit_str_operand(c, a[3]); sb_put(&c->out, ".data); ");
        } else {
            sb_put(&c->out, "gmb_form_add_statusbar(0, 0, (int)(gmb_gfx_width() * gmb_gfx_render_scale()), "); emit_str_operand(c, a[na - 1]); sb_put(&c->out, ".data); ");
        }
        emit_ctrl_events(c, s);
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "SPINNER") == 0 && na >= 3) {
        /* Forme: SPINNER x, y, preset  |  SPINNER x, y, w, preset. */
        ind(c);
        sb_put(&c->out, "{ static gmb_spinner_t _tui_sp; gmb_spinner_init(&_tui_sp, (int)(");
        emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0"); sb_put(&c->out, "), (int)(");
        if (nr >= 4) emit_expr_dflt(c, a[2], "4"); else sb_put(&c->out, "4");
        sb_put(&c->out, ")); ");
        sb_put(&c->out, "gmb_spinner_set_preset(&_tui_sp, "); emit_str_operand(c, nr >= 4 ? a[3] : a[2]); sb_put(&c->out, ".data); ");
        sb_put(&c->out, "gmb_tui_control_register(&_tui_sp.ctrl); ");
        if (_iv) { emit_net_lhs(c, _iv);
            if (_iv->type == TY_STR) sb_put(&c->out, " = gmb_string_alloc(\"0\"); ");
            else sb_put(&c->out, " = (gmb_double_t)_tui_sp.index; ");
        }
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "SPLITTER") == 0 && na >= 4) {
        /* Splitter: x, y, w, h [, orientamento]. */
        ind(c); sb_put(&c->out, "{ static gmb_splitter_t _sp; ");
        sb_put(&c->out, "gmb_splitter_init(&_sp, (int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "8"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "200");
        sb_put(&c->out, "), (int)("); if (na >= 5) emit_expr_dflt(c, a[4], "0"); else sb_put(&c->out, "0"); sb_put(&c->out, ")); ");
        sb_put(&c->out, "gmb_tui_control_register(&_sp.ctrl); ");
        sb_put(&c->out, "}\n");
    } else if (strcasecmp(cname, "PANEL") == 0 && na >= 4) {
        ind(c); sb_put(&c->out, "{ gmb_form_add_panel((int)("); emit_expr_dflt(c, a[0], "0"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[1], "0");
        sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[2], "200"); sb_put(&c->out, "), (int)("); emit_expr_dflt(c, a[3], "100"); sb_put(&c->out, "), ");
        if (na >= 5) { emit_str_operand(c, a[4]); sb_put(&c->out, ".data); "); } else sb_put(&c->out, "\"\"); ");
        sb_put(&c->out, "}\n");
    } else {
        c99_fail(c, s->line, "CONTROL '%s' non supportato dall'emitter nativo", cname);
    }
    if (s->id_var && *s->id_var) { ind(c); emit_ident(c, s->id_var); sb_put(&c->out, " = (gmb_double_t)gmb_form_last_handle();\n"); }
    emit_tui_props(c, s);
    /* Clausole del controllo (NAME/TAG/TOOLTIP/VISIBLE/ENABLED/TABINDEX/TABSTOP/STYLE/
     * COLOR... e `metric`) per OGNI tipo: i rami che non le emettono direttamente le
     * applicano qui, per HANDLE (una sola fonte: il runtime). Senza, `USE CELLS` su
     * CHECKBOX/TOGGLE/COMBOBOX/PROGRESS/SLIDER/PANEL/... veniva perso e il controllo
     * finiva disegnato alle coordinate grezze (residui/sovrapposizioni). */
    if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0] && *s->set_attr[0]) {
        ind(c);
        sb_put(&c->out, "gmb_form_apply_last_style(\"");
        for (const char* _pp = s->set_attr[0]; *_pp; _pp++) {
            if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\");
            char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch);
        }
        sb_put(&c->out, "\");\n");
    }
    (void)ctl_free_csv;
    return 1;
}

/* Clausole dichiarate del controllo (proprietà base + spec CSS-like/metric): le applica
 * l'EMITTER CHIAMANTE dopo emit_control_stmt, così valgono anche per i rami che fanno
 * `return 0` prima della coda (CHART, CANDLESTICK, QR, FKEYBAR, TBROWSE, ...).
 * Prima quelle clausole (compreso NAME) andavano perse → `CONTROL SET "nome"` non
 * trovava il controllo e la visibilità per scheda non funzionava. */
void emit_control_clauses(ctx_t* c, gmb_stmt_t* s)
{
    if (!c || !s || s->kind != GMB_STMT_CONTROL) return;
    emit_tui_props(c, s);
    if (s->set_attr && s->n_set_attr > 0 && s->set_attr[0] && *s->set_attr[0]) {
        ind(c);
        sb_put(&c->out, "gmb_form_apply_last_style(\"");
        for (const char* _pp = s->set_attr[0]; *_pp; _pp++) {
            if (*_pp == '"' || *_pp == '\\') sb_put(&c->out, "\\");
            char _ch[2] = { *_pp, 0 }; sb_put(&c->out, _ch);
        }
        sb_put(&c->out, "\");\n");
    }
}