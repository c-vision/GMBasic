/* ============================================================================
 * gmb_vm_tui.c — Controlli Clipper TUI nella VM/C (syscall 273–289).
 * Semantica: PCodeVM deprecato di gmo (PCodeProcessor.Tui.cs), input REALE.
 * Per ogni controllo: costruisce il widget runtime (gmb_tui_*), lo esegue in un
 * loop modale (render + input), poi spinge il risultato (item/index/value/bool).
 * ============================================================================ */
#include "gmb_vm_tui.h"
#include "gmb_syscall_codes.h"   /* GMB_SC_<NOME> = valore dell'enum SysCall (generato) */
#include "gmb_form.h"
#include "gmb_vm_events.h"
#include "gmb_gpm.h"
#include "gmb_vm_value.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_app.h"
#include "gmb_read.h"
#include "gmb_dscope.h"
#include "gmb_adapter.h"
#include "gmb_tui_io.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "gmb_vm_tui_internal.h"

/* ---------------------------------------------------------------------------
 * Contesto TUI per-VM (lazy)
 * ------------------------------------------------------------------------- */

static int tui_default_read(void* user, int timeout_ms, gmb_ui_event_t* out)
{
    gmb_clipper_state_t* st = (gmb_clipper_state_t*)user;
    /* typeahead iniettata (test/host) ha priorità; poi tty reale. */
    if (st && st->typeahead_count > 0)
    {
        int k = gmb_clipper_input_inkey(st);
        if (k != 0)
        {
            out->type = GMB_UIEVENT_KEY;
            out->key = k;
            out->mods = GMB_MODKEY_NONE;
            return 1;
        }
    }
    return gmb_raw_input_reader_read_event_core(st, out);
}

static void tui_default_render(gmb_screen_buffer_t* buf, void* ctx)
{
    (void)ctx;
    char out[65536];
    size_t n = gmb_render_screen(buf, out, sizeof(out));
    fwrite(out, 1, n, stdout);
    fflush(stdout);
}

void tui_ensure(gmb_vm_t* vm)
{
    if (vm->tui) return;
    gmb_vm_tui_t* t = (gmb_vm_tui_t*)calloc(1, sizeof(*t));
    t->st = gmb_clipper_state_new(80, 25);
    t->buf = gmb_screen_new(80, 25);
    t->input.user = t->st;
    t->input.read = tui_default_read;
    t->render = tui_default_render;
    t->render_ctx = NULL;
    gmb_modal_host_init(&t->host, &t->input);
    t->ready = true;
    vm->tui = t;
}

void gmb_vm_tui_free(gmb_vm_t* vm)
{
    if (!vm->tui) return;
    if (vm->tui->st) gmb_clipper_state_free(vm->tui->st);
    if (vm->tui->buf) gmb_screen_free(vm->tui->buf);
    free(vm->tui);
    vm->tui = NULL;
}

void gmb_vm_tui_inject_keys(gmb_vm_t* vm, const char* s)
{
    if (!vm->tui) return;
    gmb_clipper_input_keyboard(vm->tui->st, s);
}

void gmb_vm_tui_set_render(gmb_vm_t* vm, void (*render)(gmb_screen_buffer_t*, void*), void* ctx)
{
    if (!vm->tui) return;
    vm->tui->render = render ? render : tui_default_render;
    vm->tui->render_ctx = ctx;
}

/* ---------------------------------------------------------------------------
 * Helper stack (argomenti e risultati)
 * ------------------------------------------------------------------------- */
double tui_pop_num(gmb_task_t* task)
{
    gmb_stack_entry_t e = gmb_task_pop(task);
    gmb_obj_release(e.ref);
    return e.type == 1 ? e.num : 0.0;
}

const char* tui_pop_str(gmb_task_t* task)
{
    gmb_stack_entry_t e = gmb_task_pop(task);
    const char* s = (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)e.ref)->data : "";
    gmb_task_defer_release(task, e.ref);   /* il chiamante usa `s` (puntatore ai dati): rilascio a fine istruzione */
    return s;
}

/* Legge una lista di stringhe dallo stack. Ritorna count (<=cap). */
int tui_pop_str_list(gmb_task_t* task, char out[][256], int cap)
{
    gmb_stack_entry_t e = gmb_task_pop(task);
    int n = 0;
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)e.ref;
        gmb_list_t* l = sv->list;
        for (size_t i = 0; i < l->count && n < cap; i++)
        {
            gmb_val_t v = l->items[i];
            if (v.kind == GMB_TYPE_STRING && v.v_string.data) snprintf(out[n++], 256, "%s", v.v_string.data);
            else snprintf(out[n++], 256, "");
        }
    }
    else if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING)
    {
        /* CSV "a,b,c" (forma del minitest): split su virgola. Senza voci alcuni modali
         * ignorano ENTER (nessuna selezione possibile) → hang in harness. */
        const char* csv = ((gmb_obj_string_t*)e.ref)->data;
        if (csv && *csv)
        {
            char tmp[1024];
            snprintf(tmp, sizeof(tmp), "%s", csv);
            for (char* tok = strtok(tmp, ","); tok && n < cap; tok = strtok(NULL, ","))
            {
                while (*tok == ' ') tok++;
                if (*tok) snprintf(out[n++], 256, "%s", tok);
            }
        }
    }
    gmb_obj_release(e.ref);
    return n;
}

void tui_push_num(gmb_task_t* task, double v, uint8_t bt)
{
    gmb_task_push_num(task, v, bt);
}
void tui_push_bool(gmb_task_t* task, bool b)
{
    gmb_task_push_num(task, b ? 1.0 : 0.0, GMB_BT_BOOL);
}
void tui_push_str(gmb_task_t* task, const char* s)
{
    gmb_obj_string_t* o = gmb_str_cstr(s ? s : "");
    gmb_task_push_ref(task, (gmb_obj_t*)o, GMB_BT_STRING);
}

/* ---------------------------------------------------------------------------
 * Loop modale generico per widget (gmb_control_t)
 * ------------------------------------------------------------------------- */
static int tui_modal_cb(const gmb_ui_event_t* ev, void* ctx)
{
    tui_modal_ctx_t* mc = (tui_modal_ctx_t*)ctx;
    if (ev->type == GMB_UIEVENT_KEY)
    {
        if (ev->key == GMB_K_ENTER || ev->key == GMB_K_ESC)
        {
            mc->exit_key = ev->key;
            mc->done = true;
            return 0;
        }
        gmb_control_handle_key(mc->ctrl, ev->key);
    }
    else if (ev->type == GMB_UIEVENT_MOUSE)
    {
        gmb_control_handle_mouse(mc->ctrl, &ev->mouse);
    }
    gmb_screen_clear(mc->tui->buf, GMB_CELLCOLOR_BLACK, GMB_CELLCOLOR_BLACK);
    gmb_control_draw(mc->ctrl, mc->tui->buf);
    if (mc->tui->render) mc->tui->render(mc->tui->buf, mc->tui->render_ctx);
    return 1;
}

/* Esegue il loop modale di un widget; ritorna il tasto di uscita. */
void tui_join_csv(const char items[][256], int n, char* out, int cap)
{
    int o = 0;
    for (int i = 0; i < n; i++) { if (i && o < cap - 1) out[o++] = '|'; for (const char* q = items[i]; *q && o < cap - 1; q++) out[o++] = *q; }
    out[o] = 0;
}

static int tui_run_modal(gmb_vm_tui_t* tui, gmb_control_t* ctrl)
{
    tui_modal_ctx_t mc;
    memset(&mc, 0, sizeof(mc));
    mc.tui = tui;
    mc.ctrl = ctrl;
    gmb_modal_host_run_modal(&tui->host, tui_modal_cb, &mc);
    return mc.exit_key;
}

/* ---------------------------------------------------------------------------
 * Handler dei singoli syscall (semantica PCodeVM)
 * ------------------------------------------------------------------------- */
static void tui_statusbar(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* text = tui_pop_str(task);
    gmb_vm_tui_t* t = vm->tui;
    gmb_tui_add_statusbar(0, gmb_screen_height(t->buf) - 1, gmb_screen_width(t->buf), text);   /* registry */
}

static void tui_spinner(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* preset = tui_pop_str(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_vm_tui_t* t = vm->tui;
    gmb_tui_add_spinner(col, row, 4, preset);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

static void tui_control_apply_props(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm;
    const char* colorerr = tui_pop_str(task); const char* colorsel = tui_pop_str(task);
    const char* colorfocus = tui_pop_str(task); const char* color = tui_pop_str(task);
    const char* style = tui_pop_str(task); double focus = tui_pop_num(task);
    double tabstop = tui_pop_num(task), enabled = tui_pop_num(task), visible = tui_pop_num(task), tabindex = tui_pop_num(task);
    const char* tooltip = tui_pop_str(task); const char* tag = tui_pop_str(task); const char* name = tui_pop_str(task);
    unsigned flags = 1u|2u|4u|8u|16u|32u|64u; /* name/tag/tooltip/tabindex/visible/enabled/tabstop */
    if (focus != 0.0) flags |= 128u;
    gmb_control_apply_props_handle(gmb_tui_last_handle(), flags, name, tag, tooltip, (int)tabindex, (int)visible, (int)enabled, (int)tabstop);
    int h = gmb_tui_last_handle();
    char buf[160];
    if (style && *style) gmb_control_apply_style_handle(h, style);
    if (color && *color) { snprintf(buf, sizeof(buf), "color=%s", color); gmb_control_apply_style_handle(h, buf); }
    if (colorfocus && *colorfocus) { snprintf(buf, sizeof(buf), "focuscolor=%s", colorfocus); gmb_control_apply_style_handle(h, buf); }
    if (colorsel && *colorsel) { snprintf(buf, sizeof(buf), "selcolor=%s", colorsel); gmb_control_apply_style_handle(h, buf); }
    if (colorerr && *colorerr) { snprintf(buf, sizeof(buf), "errorcolor=%s", colorerr); gmb_control_apply_style_handle(h, buf); }
    tui_push_num(task, 1.0, GMB_BT_NUMBER);
}
void tui_last_handle(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; tui_push_num(task, (double)gmb_tui_last_handle(), GMB_BT_NUMBER); }
static const char* tui_val_cstr(gmb_val_t v, char* buf, int cap)
{
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    if (v.kind == GMB_TYPE_BOOL) { snprintf(buf, cap, "%s", v.v_bool ? "TRUE" : "FALSE"); return buf; }
    if (v.kind == GMB_TYPE_INT) { snprintf(buf, cap, "%d", (int)v.v_int); return buf; }
    if (v.kind == GMB_TYPE_DOUBLE) { snprintf(buf, cap, "%g", v.v_double); return buf; }
    snprintf(buf, cap, "");
    return buf;
}

static void tui_tbrowse(gmb_vm_t* vm, gmb_task_t* task)
{
    /* TBROWSE var, r, c, w, h, cols(), data() [, opts()] — tabella completa:
     * colonne (header/width/type/align/editable), righe (data), opzioni
     * (freeze/lock/footer/colors/border/header/highlight/mouse/autolite/colsep/
     * headsep/footsep/bookmark — validate/onkey: callback BASIC, non disponibili
     * senza closure nel C; qui ignorate, come i campi bool direct struct). */
    gmb_stack_entry_t optsRaw = gmb_task_pop(task);
    gmb_stack_entry_t dataRaw = gmb_task_pop(task);
    gmb_stack_entry_t colsRaw = gmb_task_pop(task);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);

    /* colonne: lista di [header, width, type, align, editable] */
    gmb_browse_column_t cols[GMB_BROWSE_MAX_COLS];
    int ncols = 0;
    if (colsRaw.type == 2 && colsRaw.ref && colsRaw.ref->kind == GMB_OBJ_LIST)
    {
        gmb_list_t cl = *((gmb_obj_shared_val_t*)colsRaw.ref)->list;
        for (size_t i = 0; i < cl.count && ncols < GMB_BROWSE_MAX_COLS; i++)
        {
            gmb_val_t cv = cl.items[i];
            gmb_list_t cdef = gmb_val_as_list(cv);
            if (cdef.count >= 2)
            {
                gmb_browse_column_t bc;
                memset(&bc, 0, sizeof(bc));
                gmb_string_t hv = gmb_val_to_string(gmb_list_get(cdef, 0));
                gmb_val_t wv = gmb_list_get(cdef, 1);
                snprintf(bc.header, sizeof(bc.header), "%s", hv.data ? hv.data : "");
                bc.width = wv.kind == GMB_TYPE_INT ? (int)wv.v_int : wv.kind == GMB_TYPE_DOUBLE ? (int)wv.v_double : 10;
                if (cdef.count >= 3)
                {
                    gmb_string_t ts = gmb_val_to_string(gmb_list_get(cdef, 2));
                    const char* t = ts.data;
                    if (t && *t)
                    {
                        if (strncmp(t, "N", 1) == 0 || strncmp(t, "I", 1) == 0) bc.data_type = GMB_DT_INT;
                        else if (strncmp(t, "F", 1) == 0) bc.data_type = GMB_DT_FLOAT;
                        else if (strncmp(t, "L", 1) == 0 || strncmp(t, "B", 1) == 0) bc.data_type = GMB_DT_BOOL;
                        else if (strncmp(t, "D", 1) == 0) bc.data_type = GMB_DT_DATE;
                        else if (strncmp(t, "X", 1) == 0) bc.data_type = GMB_DT_CHOICE;
                        else bc.data_type = GMB_DT_TEXT;
                    }
                }
                if (cdef.count >= 4)
                {
                    gmb_string_t as = gmb_val_to_string(gmb_list_get(cdef, 3));
                    const char* a = as.data;
                    if (a && strncmp(a, "R", 1) == 0) bc.align = GMB_ALIGN_RIGHT;
                    else if (a && strncmp(a, "C", 1) == 0) bc.align = GMB_ALIGN_CENTER;
                    else bc.align = GMB_ALIGN_LEFT;
                }
                if (cdef.count >= 5)
                {
                    gmb_val_t ev = gmb_list_get(cdef, 4);
                    gmb_string_t es = gmb_val_to_string(ev);
                    bc.editable = ev.kind == GMB_TYPE_BOOL ? ev.v_bool : (es.data && strcmp(es.data, "TRUE") == 0);
                }
                cols[ncols++] = bc;
            }
        }
    }
    if (ncols == 0)
    {
        memset(&cols[0], 0, sizeof(cols[0]));
        snprintf(cols[0].header, sizeof(cols[0].header), "%s", "Field");
        cols[0].width = 10;
        ncols = 1;
    }

    /* data: lista di righe (lista di stringhe) */
    char rows_cells[64][GMB_BROWSE_MAX_COLS][256];
    int nrows = 0;
    if (dataRaw.type == 2 && dataRaw.ref && dataRaw.ref->kind == GMB_OBJ_LIST)
    {
        gmb_list_t dl = *((gmb_obj_shared_val_t*)dataRaw.ref)->list;
        for (size_t i = 0; i < dl.count && nrows < 64; i++)
        {
            gmb_list_t rl = gmb_val_as_list(dl.items[i]);
            for (int c = 0; c < rl.count && c < GMB_BROWSE_MAX_COLS; c++)
            {
                gmb_string_t cs = gmb_val_to_string(gmb_list_get(rl, c));
                snprintf(rows_cells[nrows][c], 256, "%s", cs.data ? cs.data : "");
            }
            nrows++;
        }
    }

    static gmb_vec_ds_t vd;
    gmb_vec_ds_make(&vd, nrows, ncols);
    for (int rr = 0; rr < nrows && rr < vd.rows; rr++)
        for (int cc = 0; cc < ncols && cc < vd.cols; cc++)
            gmb_vec_ds_set(&vd, rr, cc, rows_cells[rr][cc]);

    static gmb_browse_datasource_t ds;
    gmb_vec_ds_datasource(&vd, &ds);

    gmb_rect_t area = { col, row, w > 0 ? w : 40, h > 0 ? h : 6 };
    gmb_browse_t* b = gmb_browse_new(&area, ncols, cols, &ds);
    gmb_obj_release(optsRaw.ref);
    gmb_obj_release(dataRaw.ref);
    gmb_obj_release(colsRaw.ref);
    if (!b) { gmb_vec_ds_make(&vd, 0, 0); tui_push_num(task, -1.0, GMB_BT_NUMBER); return; }

    /* opzioni */
    if (optsRaw.type == 2 && optsRaw.ref && optsRaw.ref->kind == GMB_OBJ_LIST)
    {
        gmb_list_t ol = *((gmb_obj_shared_val_t*)optsRaw.ref)->list;
        for (size_t i = 0; i < ol.count; i++)
        {
            gmb_list_t pair = gmb_val_as_list(ol.items[i]);
            if (pair.count < 2) continue;
            char keybuf[64];
            const char* key = tui_val_cstr(gmb_list_get(pair, 0), keybuf, sizeof(keybuf));
            gmb_val_t val = gmb_list_get(pair, 1);
            if (!key) continue;
            char tmpbuf[256];
            if (strcmp(key, "freeze") == 0) b->frozen_cols = val.kind == GMB_TYPE_INT ? (int)val.v_int : (int)val.v_double;
            else if (strcmp(key, "footer") == 0) gmb_browse_set_footer(b, tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)));
            else if (strcmp(key, "colors") == 0 || strcmp(key, "colorspec") == 0) gmb_browse_set_color_spec(b, tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)));
            else if (strcmp(key, "border") == 0 || strcmp(key, "showborder") == 0)
                b->show_border = val.kind == GMB_TYPE_BOOL ? val.v_bool : strcmp(tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)), "TRUE") == 0;
            else if (strcmp(key, "header") == 0 || strcmp(key, "showheader") == 0)
                b->show_header = val.kind == GMB_TYPE_BOOL ? val.v_bool : strcmp(tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)), "TRUE") == 0;
            else if (strcmp(key, "highlight") == 0 || strcmp(key, "highlightrow") == 0)
                b->highlight_row = val.kind == GMB_TYPE_BOOL ? val.v_bool : strcmp(tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)), "TRUE") == 0;
            else if (strcmp(key, "mouse") == 0 || strcmp(key, "mouseenabled") == 0)
                b->mouse_enabled = val.kind == GMB_TYPE_BOOL ? val.v_bool : strcmp(tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)), "TRUE") == 0;
            else if (strcmp(key, "autolite") == 0)
                b->auto_lite = val.kind == GMB_TYPE_BOOL ? val.v_bool : strcmp(tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)), "TRUE") == 0;
            else if (strcmp(key, "colsep") == 0) gmb_browse_set_col_sep(b, tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)));
            else if (strcmp(key, "headsep") == 0) gmb_browse_set_head_sep(b, tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)));
            else if (strcmp(key, "footsep") == 0) gmb_browse_set_foot_sep(b, tui_val_cstr(val, tmpbuf, sizeof(tmpbuf)));
            else if (strcmp(key, "lock") == 0)
            {
                gmb_list_t locks = gmb_val_as_list(val);
                for (size_t j = 0; j < locks.count; j++)
                {
                    gmb_list_t p2 = gmb_val_as_list(locks.items[j]);
                    if (p2.count >= 2)
                    {
                        gmb_val_t rv = gmb_list_get(p2, 0);
                        gmb_val_t cv2 = gmb_list_get(p2, 1);
                        gmb_browse_lock_cell(b, rv.kind == GMB_TYPE_INT ? (int)rv.v_int : (int)rv.v_double,
                                               cv2.kind == GMB_TYPE_INT ? (int)cv2.v_int : (int)cv2.v_double);
                    }
                }
            }
            else if (strcmp(key, "bookmark") == 0)
                gmb_browse_set_row_pos(b, val.kind == GMB_TYPE_INT ? (int)val.v_int : (int)val.v_double);
            /* validate / onkey: callback BASIC (closure): non disponibili in C senza
             * meccanismo di valutazione — ignorate (come il gap C# documentato). */
        }
    }

    /* non-bloccante: registra nel registry (frame-driven), niente loop modale. */
    if (b) gmb_tui_control_register(&b->ctrl);
    tui_push_num(task, -1.0, GMB_BT_NUMBER);
}

/* ---------------------------------------------------------------------------
 * Dispatch
 * ------------------------------------------------------------------------- */
/* TUI syscall per-code -> tabella {code, handler} (refactor §18.5). Le voci sono i
 * comandi TUI (278-294) emessi dal compilatore: dispatch del linguaggio, non opcode
 * interni. I sconosciuti/295 consumano gli argomenti (default). */
typedef struct { int code; void (*fn)(gmb_vm_t*, gmb_task_t*); } tui_sys_row;
static const tui_sys_row TUI_SYS_TBL[] = {
    { GMB_TUI_CHECKBOX, tui_checkbox },
    { GMB_TUI_LISTBOX, tui_listbox },
    { GMB_TUI_COMBOBOX, tui_combobox },
    { GMB_TUI_MENU, tui_menu },
    { GMB_TUI_STATUSBAR, tui_statusbar },
    { GMB_TUI_FILEDIALOG, tui_filedialog },
    { GMB_TUI_PRINTER, tui_printer },
    { GMB_TUI_TOGGLE, tui_toggle },
    { GMB_TUI_RADIOGROUP, tui_radiogroup },
    { GMB_TUI_SLIDER, tui_slider },
    { GMB_TUI_NUMERICUPDOWN, tui_numericupdown },
    { GMB_TUI_STARRATING, tui_starrating },
    { GMB_TUI_SCROLLBAR, tui_scrollbar },
    { GMB_TUI_SCROLLVIEW, tui_scrollview },
    { GMB_TUI_LABEL, tui_label },
    { GMB_TUI_DATALIST, tui_datalist },
    { GMB_TUI_PROPERTYGRID, tui_propertygrid },
    { GMB_TUI_LOGVIEW, tui_logview },
    { GMB_TUI_HEXVIEW, tui_hexview },
    { GMB_TUI_BIGTEXT, tui_bigtext },
    { GMB_TUI_CODEAREA, tui_codearea },
    { GMB_TUI_REORDERLISTBOX, tui_reorderlistbox },
    { GMB_TUI_ALERTBOX, tui_alertbox },
    { GMB_TUI_CONFIRMATIONBOX, tui_confirmationbox },
    { GMB_TUI_FILEEXPLORER, tui_fileexplorer },
    { GMB_TUI_DASHBOARD, tui_dashboard },
    { GMB_TUI_SPLITTER, tui_splitter },
    { GMB_TUI_CONTEXTMENU, tui_contextmenu },
    { GMB_TUI_TAB, tui_tab },
    { GMB_TUI_GRID, tui_grid },
    { GMB_TUI_DATEPICKER, tui_datepicker },
    { GMB_TUI_COLORPICKER, tui_colorpicker },
    { GMB_TUI_TREEVIEW, tui_treeview },
    { GMB_TUI_SPINNER, tui_spinner },
    { GMB_TUI_TBROWSE, tui_tbrowse },
    { GMB_TUI_LAST_HANDLE, tui_last_handle },
    { GMB_CONTROL_APPLY_PROPS, tui_control_apply_props },
};
/* ---- Controlli pixel + form (syscall 400-409; bridge gmb_form_* condiviso) ---- */
void tui_pop_str_into(gmb_task_t* task, char* dst, size_t cap)
{
    gmb_stack_entry_t e = gmb_task_pop(task);
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING && ((gmb_obj_string_t*)e.ref)->data)
        snprintf(dst, cap, "%s", ((gmb_obj_string_t*)e.ref)->data);
    else dst[0] = 0;
    gmb_obj_release(e.ref);
}
static void vm_pixel_ctrl(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm;
    gmb_stack_entry_t le = gmb_task_pop(task);
    char spec[512], name[128], kind[40];
    tui_pop_str_into(task, spec, sizeof(spec));
    tui_pop_str_into(task, name, sizeof(name));
    tui_pop_str_into(task, kind, sizeof(kind));
    double av[8]; char ab[8][256]; int ac = 0;
    for (int i = 0; i < 8; i++) { av[i] = 0; ab[i][0] = 0; }
    if (le.type == 2 && le.ref && le.ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)le.ref; gmb_list_t* l = sv->list;
        for (size_t i = 0; i < l->count && ac < 8; i++)
        {
            gmb_val_t v = l->items[i];
            if (v.kind == GMB_TYPE_STRING) { av[ac] = 0; snprintf(ab[ac], 256, "%s", v.v_string.data ? v.v_string.data : ""); }
            else { av[ac] = v.v_double; }
            ac++;
        }
    }
    gmb_obj_release(le.ref);
#define AI(i) ((i) < ac ? (int)av[i] : 0)
#define AD(i) ((i) < ac ? av[i] : 0.0)
#define AS(i) ((i) < ac ? ab[i] : "")
    if      (!strcasecmp(kind, "CHECK") || !strcasecmp(kind, "CHECKBOX")) gmb_form_add_checkbox(AI(0), AI(1), AS(2), NULL);
    else if (!strcasecmp(kind, "RADIO") || !strcasecmp(kind, "RADIOGROUP")) gmb_form_add_radio(AI(0), AI(1), AS(2), NULL);
    else if (!strcasecmp(kind, "TRACK") || !strcasecmp(kind, "SLIDER"))     gmb_form_add_track(AI(0), AI(1), AI(2), AD(3), AD(4), NULL);
    else if (!strcasecmp(kind, "DROPDOWN") || !strcasecmp(kind, "COMBOBOX")) gmb_form_add_dropdown(AI(0), AI(1), AI(2), AS(3), NULL);
    else if (!strcasecmp(kind, "PROGRESS"))    gmb_form_add_progress_v(AI(0), AI(1), AI(2), AD(3));
    else if (!strcasecmp(kind, "TOGGLE"))      gmb_form_add_toggle(AI(0), AI(1), AS(2), NULL);
    else if (!strcasecmp(kind, "SPINNER"))     gmb_form_add_spinner(AI(0), AI(1), AI(2), AD(3), AD(4), AD(5), NULL);
    else if (!strcasecmp(kind, "TAB"))         gmb_form_add_tabs(AI(0), AI(1), AS(2), NULL);
    else if (!strcasecmp(kind, "LISTBOX"))     gmb_form_add_listbox(AI(0), AI(1), AI(2), AI(3), AS(4), NULL);
    else if (!strcasecmp(kind, "COLORPICKER")) gmb_form_add_colorpicker(AI(0), AI(1), NULL, NULL);
    else if (!strcasecmp(kind, "GRID") || !strcasecmp(kind, "TBROWSE")) gmb_form_add_grid(AI(0), AI(1), AI(2), AI(3), AS(4), AS(5), NULL);
    else if (!strcasecmp(kind, "TREEVIEW"))    gmb_form_add_tree(AI(0), AI(1), AI(2), AI(3), AS(4), NULL);
    else if (!strcasecmp(kind, "STATUSBAR"))   gmb_form_add_statusbar(AI(0), AI(1), AI(2), AS(3));
    else if (!strcasecmp(kind, "MENU"))        gmb_form_add_menubar(AI(0), AI(1), AS(2), NULL);
    else if (!strcasecmp(kind, "DATEPICKER"))  gmb_form_add_datepicker(AI(0), AI(1), NULL, ac > 2 ? ab[2] : "dd/mm/yyyy");
    else if (!strcasecmp(kind, "FILEDIALOG"))  gmb_form_add_filedialog(AI(0), AI(1), AI(2), AI(3), AS(4), NULL);
    else if (!strcasecmp(kind, "TEXTBOX"))   gmb_form_add_textbox(AI(0), AI(1), AI(2), AI(3), NULL);
    else if (!strcasecmp(kind, "BUTTON"))      gmb_form_add_button(AI(0), AI(1), AI(2), AI(3), AS(4), NULL);
    else if (!strcasecmp(kind, "CANVAS"))      gmb_tui_add_canvas(AI(0), AI(1), AI(2), AI(3));
    else if (!strcasecmp(kind, "IMAGE"))       gmb_tui_add_image(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "IMAGEBUTTON")) gmb_tui_add_imagebutton(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "QR"))          gmb_tui_add_qr(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "RICHTEXT"))    gmb_tui_add_richtext(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "HALFBLOCK"))   gmb_tui_add_halfblock(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "TILING"))      gmb_tui_add_tiling(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "PANEL"))       gmb_form_add_panel(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "FUNCTIONKEYBAR")) gmb_tui_add_fkeybar(AI(0), AI(1), AI(2), AS(3));
    else if (!strcasecmp(kind, "CANDLESTICK")) gmb_tui_add_candlestick(AI(0), AI(1), AI(2), AI(3), AS(4));
    else if (!strcasecmp(kind, "CHART"))       gmb_tui_add_chart(AI(0), AI(1), AI(2), AI(3), AS(4));
#undef AI
#undef AD
#undef AS
    if (spec[0]) gmb_form_set_last_style(spec);
    else if (name[0]) { char b[200]; snprintf(b, sizeof(b), "name=%s", name); gmb_form_set_last_style(b); }
}
static gmb_vm_t* g_ctrl_vm = NULL;
static gmb_task_t* g_ctrl_task = NULL;
static double vm_ctrl_thunk(void)
{
    if (g_ctrl_vm && g_ctrl_task)
        gmb_vm_control_fire(g_ctrl_vm, g_ctrl_task, gmb_form_firing_control(), gmb_form_firing_event());
    return 0;
}

/* UserControl: thunk di paint — invoca il metodo BASIC Paint dell'istanza (run-to-completion)
 * passando l'handle del GraphicsContext. Chiamato durante gmb_form_frame (case 401), quindi
 * g_ctrl_vm/g_ctrl_task sono validi. */
extern int gmb_vm_invoke_paint(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_t* inst, double handle);
static void vm_uc_paint(void* inst, gmb_gfx_context_t* g)
{
    if (!g_ctrl_vm || !g_ctrl_task || !inst) return;
    int hh = gmb_gfxctx_open(g);
    gmb_vm_invoke_paint(g_ctrl_vm, g_ctrl_task, (gmb_obj_t*)inst, (double)hh);
    gmb_gfxctx_close(hh);
}
static void vm_form_frame(gmb_vm_t* vm, gmb_task_t* task) { int ms = (int)tui_pop_num(task); g_ctrl_vm = vm; g_ctrl_task = task; gmb_ui_frame(ms); g_ctrl_vm = NULL; g_ctrl_task = NULL; }
static void vm_form_closed(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; tui_push_num(task, gmb_form_closed() ? 1.0 : 0.0, GMB_BT_BOOL); }
static void vm_form_close(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; (void)task; gmb_form_request_close(); }
static void vm_form_ctrl_get(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char prop[64], name[128];
    tui_pop_str_into(task, prop, sizeof(prop)); tui_pop_str_into(task, name, sizeof(name));
    tui_push_num(task, gmb_form_control_get(name, prop), GMB_BT_NUMBER);
}
static void vm_form_ctrl_gets(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char prop[64], name[128];
    tui_pop_str_into(task, prop, sizeof(prop)); tui_pop_str_into(task, name, sizeof(name));
    tui_push_str(task, gmb_form_control_get_str(name, prop));
}
static void vm_form_ctrl_set(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char str[256], prop[64], name[128];
    tui_pop_str_into(task, str, sizeof(str)); double num = tui_pop_num(task);
    tui_pop_str_into(task, prop, sizeof(prop)); tui_pop_str_into(task, name, sizeof(name));
    gmb_form_control_set(name, prop, num, str);
}
static void vm_chart_create(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; tui_push_num(task, (double)gmb_gfx_chart_create(), GMB_BT_NUMBER); }
static void vm_chart_clear(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; double h = tui_pop_num(task); gmb_gfx_chart_clear((int)h); }
static void vm_chart_close(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; double h = tui_pop_num(task); gmb_gfx_chart_close((int)h); }
static void vm_chart_plot(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char legend[128], type[64];
    tui_pop_str_into(task, legend, sizeof(legend));
    tui_pop_str_into(task, type, sizeof(type));
    gmb_stack_entry_t yl = gmb_task_pop(task);
    gmb_stack_entry_t xl = gmb_task_pop(task);
    double h = tui_pop_num(task);
    int nx = 0, ny = 0;
    double* xs = gmb_vm_list_to_dbl(&xl, &nx);
    double* ys = gmb_vm_list_to_dbl(&yl, &ny);
    int n = nx < ny ? nx : ny;
    gmb_gfx_chart_plot((int)h, xs, ys, n, legend);
    free(xs); free(ys);
    if (xl.ref) gmb_obj_release(xl.ref);
    if (yl.ref) gmb_obj_release(yl.ref);
}
static void vm_chart_financial(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char legend[128], type[64];
    tui_pop_str_into(task, legend, sizeof(legend));
    tui_pop_str_into(task, type, sizeof(type));
    gmb_stack_entry_t ol = gmb_task_pop(task);
    double h = tui_pop_num(task);
    int n = 0;
    double* ohlc = gmb_vm_list_to_dbl(&ol, &n);
    gmb_gfx_chart_plot_financial((int)h, ohlc, n / 5, type, legend);
    free(ohlc);
    if (ol.ref) gmb_obj_release(ol.ref);
}
static void vm_chart_set(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char str[256], prop[64];
    tui_pop_str_into(task, str, sizeof(str)); double num = tui_pop_num(task);
    tui_pop_str_into(task, prop, sizeof(prop)); double h = tui_pop_num(task);
    gmb_gfx_chart_set((int)h, prop, num, str);
}
static void vm_chart_get(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char prop[64];
    tui_pop_str_into(task, prop, sizeof(prop)); double h = tui_pop_num(task);
    tui_push_num(task, gmb_gfx_chart_get((int)h, prop), GMB_BT_NUMBER);
}
static void vm_chart_gets(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; char prop[64];
    tui_pop_str_into(task, prop, sizeof(prop)); double h = tui_pop_num(task);
    tui_push_str(task, gmb_gfx_chart_get_str((int)h, prop));
}
static void vm_chart_draw(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm;
    gmb_stack_entry_t le = gmb_task_pop(task);          /* lista argomenti */
    char op[64]; tui_pop_str_into(task, op, sizeof(op));
    double h = tui_pop_num(task);
    double av[8]; int ac = 0;
    if (le.type == 2 && le.ref && le.ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)le.ref; gmb_list_t* l = sv->list;
        for (size_t i = 0; i < l->count && ac < 8; i++)
        {
            gmb_val_t v = l->items[i];
            av[ac++] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : (v.kind == GMB_TYPE_INT ? (double)v.v_int : 0);
        }
    }
    gmb_obj_release(le.ref);
    tui_push_num(task, gmb_gfx_chart_draw((int)h, op, av, ac), GMB_BT_NUMBER);
}
static void vm_form_ctrl_move(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm; int h = (int)tui_pop_num(task), w = (int)tui_pop_num(task), y = (int)tui_pop_num(task), x = (int)tui_pop_num(task);
    char name[128]; tui_pop_str_into(task, name, sizeof(name));
    gmb_form_control_move(name, x, y, w, h);
}
static void vm_form_ctrl_focus(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; char name[128]; tui_pop_str_into(task, name, sizeof(name)); gmb_form_control_focus(name); }
static void vm_form_ctrl_zorder(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; int front = (int)tui_pop_num(task); char name[128]; tui_pop_str_into(task, name, sizeof(name)); gmb_form_control_zorder(name, front); }
static void vm_form_ctrl_invalidate(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; char name[128]; tui_pop_str_into(task, name, sizeof(name)); gmb_form_control_invalidate(name); }
static void vm_form_ctrl_refresh(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; char name[128]; tui_pop_str_into(task, name, sizeof(name)); gmb_form_control_refresh(name); }
/* Polling di stato controlli (stessa semantica delle gambe C#). */
static void vm_form_clicked(gmb_vm_t* vm, gmb_task_t* task)     { (void)vm; int h = (int)tui_pop_num(task); tui_push_num(task, gmb_form_clicked(h), GMB_BT_NUMBER); }
static void vm_form_dblclicked(gmb_vm_t* vm, gmb_task_t* task)  { (void)vm; int h = (int)tui_pop_num(task); tui_push_num(task, gmb_form_dblclicked(h), GMB_BT_NUMBER); }
static void vm_form_changed(gmb_vm_t* vm, gmb_task_t* task)     { (void)vm; int h = (int)tui_pop_num(task); tui_push_num(task, gmb_form_changed(h), GMB_BT_NUMBER); }
static void vm_form_gotfocused(gmb_vm_t* vm, gmb_task_t* task)  { (void)vm; int h = (int)tui_pop_num(task); tui_push_num(task, gmb_form_gotfocused(h), GMB_BT_NUMBER); }
static void vm_form_lostfocused(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; int h = (int)tui_pop_num(task); tui_push_num(task, gmb_form_lostfocused(h), GMB_BT_NUMBER); }
static void vm_mouse_clicked(gmb_vm_t* vm, gmb_task_t* task)    { (void)vm; tui_push_num(task, gmb_mouse_clicked(), GMB_BT_NUMBER); }
static void vm_mouse_dblclicked(gmb_vm_t* vm, gmb_task_t* task) { (void)vm; tui_push_num(task, gmb_mouse_dblclicked(), GMB_BT_NUMBER); }
static void vm_mouse_pressed(gmb_vm_t* vm, gmb_task_t* task)    { (void)vm; tui_push_num(task, gmb_mouse_pressed(), GMB_BT_NUMBER); }
static void vm_key_pressed(gmb_vm_t* vm, gmb_task_t* task)      { (void)vm; tui_push_num(task, gmb_key_pressed(), GMB_BT_NUMBER); }
/* ON <EV> CONTROL <h> <azione>: registra per (handle, evento). Il firing arrivera' col bridge. */
static void vm_form_set_event_id(gmb_vm_t* vm, gmb_task_t* task)
{
    /* stack: [handle, evIndex, actionType, targetAddr] (target ultimo pushato).
     * Il dispatch nativo VM->sub (rientranza) non e' ancora implementato: no-op come
     * ON_WINDOW_EVENT (301). Si popano tutti i valori per mantenere lo stack allineato. */
    (void)vm; int target = (int)tui_pop_num(task);
    int actionType = (int)tui_pop_num(task);
    int evIndex = (int)tui_pop_num(task);
    int h = (int)tui_pop_num(task);
    gmb_vm_control_register(h, evIndex, actionType, target);
    gmb_form_set_event_id(h, evIndex, actionType == 0 ? NULL : (gmb_form_cb)vm_ctrl_thunk);
}

void gmb_vm_exec_tui_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    /* SET SHELL ON|OFF (subcode 21 del SET_CLIPPER): il VM/C instrada i SET_CLIPPER al
     * delegate TUI → senza questo case il flag restava OFF (divergenza 4 backend).
     * Parità col runtime condiviso (gmb_console.c case 21 → gmb_shell_set_enabled). */
    if (code == GMB_SC_SET_CLIPPER || code == 21) {
        /* SET_CLIPPER (295): stack [valore, subcode] (EmitConstant(value); EmitConstant(sub); argc=2).
         * subcode 21 = SET SHELL ON|OFF → gmb_shell_set_enabled (parità runtime/C99). */
        int sub = 0, v = 0;
        if (argc >= 2 && task->sp >= 2) { sub = (int)tui_pop_num(task); v = (int)tui_pop_num(task); }
        else if (argc >= 1 && task->sp >= 1) { v = (int)tui_pop_num(task); sub = code; }
        if (sub == 21) gmb_shell_set_enabled(v);
        return;
    }
    switch (code)
    {
        case 400: vm_pixel_ctrl(vm, task); return;
        case 401: vm_form_frame(vm, task); return;
        case 402: vm_form_closed(vm, task); return;
        case 403: vm_form_close(vm, task); return;
        case 404: vm_form_ctrl_get(vm, task); return;
        case 405: vm_form_ctrl_gets(vm, task); return;
        case 406: vm_form_ctrl_set(vm, task); return;
        case 407: vm_form_ctrl_move(vm, task); return;
        case 408: vm_form_ctrl_focus(vm, task); return;
        case 409: vm_form_ctrl_zorder(vm, task); return;
        case 448: vm_form_ctrl_invalidate(vm, task); return;
        case 449: vm_form_ctrl_refresh(vm, task); return;
        case GMB_SC_CHART_CREATE: vm_chart_create(vm, task); return;
        case GMB_SC_CHART_PLOT: vm_chart_plot(vm, task); return;
        case GMB_SC_CHART_FINANCIAL: vm_chart_financial(vm, task); return;
        case GMB_SC_CHART_CLEAR: vm_chart_clear(vm, task); return;
        case GMB_SC_CHART_CLOSE: vm_chart_close(vm, task); return;
        case 444: vm_chart_set(vm, task); return;
        case 445: vm_chart_get(vm, task); return;
        case 446: vm_chart_gets(vm, task); return;
        case 447: vm_chart_draw(vm, task); return;
        case 410: tui_push_num(task, (double)gmb_form_last_handle(), GMB_BT_NUMBER); return;
        case 411: vm_form_clicked(vm, task); return;
        case 412: vm_form_dblclicked(vm, task); return;
        case 413: vm_form_changed(vm, task); return;
        case 414: vm_form_gotfocused(vm, task); return;
        case 415: vm_form_lostfocused(vm, task); return;
        case 416: vm_mouse_clicked(vm, task); return;
        case 417: vm_mouse_dblclicked(vm, task); return;
        case 418: vm_mouse_pressed(vm, task); return;
        case 419: vm_key_pressed(vm, task); return;
        case 420: vm_form_set_event_id(vm, task); return;
        case 421: {
            /* FIRE EVENT: esecuzione SINCRONA -> GOSUB nel task corrente (ritorna dopo il comando). */
            char ev[64]; tui_pop_str_into(task, ev, sizeof(ev)); int h = (int)tui_pop_num(task);
            int evIndex = gmb_form_ev_index(ev);
            gmb_form_set_last_event(h, evIndex);
            /* §11.7-3: se è un metodo-evento UserControl → invoca il metodo-istanza (SELF),
             * nel contesto del task GPM corrente (mai thread esterno/.NET). */
            { void* uinst = NULL; char umeth[64];
              if (gmb_vm_control_inst_of(h, evIndex, &uinst, umeth, (int)sizeof(umeth))) {
                  extern int gmb_vm_invoke_uc_event(gmb_vm_t*, gmb_task_t*, gmb_obj_t*, const char*);
                  gmb_vm_invoke_uc_event(vm, task, (gmb_obj_t*)uinst, umeth);
                  return;
              } }
            int addr = gmb_vm_control_target(h, evIndex);
            if (addr >= 0 && task->gosub_count < GMB_GOSUB_CAP) {
                gmb_gosub_cont_t* c = &task->gosub_stack[task->gosub_count++];
                c->return_ip = task->ip; c->sp = task->sp; c->try_depth = task->try_count;
                c->cleanup_depth = task->cleanup_count; c->frame = task->frame;
                task->ip = addr;
            }
            return; }
        case 441: { /* CONTROL ADD instance AT x,y SIZE w,h [PARENT container] [+ name] → UserControl */
            char cname441[128], container[128];
            tui_pop_str_into(task, cname441, sizeof(cname441));      /* nome del controllo (istanza) */
            tui_pop_str_into(task, container, sizeof(container));    /* PARENT (contenitore) */
            int h = (int)tui_pop_num(task), w = (int)tui_pop_num(task), y = (int)tui_pop_num(task), x = (int)tui_pop_num(task);
            gmb_stack_entry_t e441 = gmb_task_pop(task);
            gmb_tui_add_usercontrol(x, y, w, h, vm_uc_paint, e441.ref, container, cname441);
            return; }
        case 450: { /* FORM_BIND_UC_EVENT: handle, evIndex, method$, symbolId, instance (§11.7-3).
                     * Cabla un metodo-evento della classe → eseguito nel task GPM (SELF=istanza). */
            gmb_stack_entry_t e450 = gmb_task_pop(task);           /* instance */
            (void)tui_pop_num(task);                                /* symbolId (non usato in VM-C: risolve via VTable) */
            char meth450[64]; tui_pop_str_into(task, meth450, sizeof(meth450));
            int evIdx450 = (int)tui_pop_num(task);
            int h450 = (int)tui_pop_num(task);
            gmb_vm_control_register_inst(h450, evIdx450, meth450, e450.ref);
            gmb_form_set_event_id(h450, evIdx450, (gmb_form_cb)vm_ctrl_thunk);
            return; }
        case 442: { /* CONTROL REMOVE instance */
            gmb_stack_entry_t e442 = gmb_task_pop(task); if (e442.ref) gmb_obj_release(e442.ref);
            return; }
        case 443: { /* CONTROL DRAW handle, op, a, b, c, d, str$ */
            char s443[256]; tui_pop_str_into(task, s443, sizeof(s443));
            double d443 = tui_pop_num(task), c443 = tui_pop_num(task), b443 = tui_pop_num(task), a443 = tui_pop_num(task);
            int op443 = (int)tui_pop_num(task);
            gmb_stack_entry_t hv443 = gmb_task_pop(task);
            int hnd443 = (int)gmb_val_to_double(gmb_adapter_stack_to_val(hv443));
            if (hv443.ref) gmb_obj_release(hv443.ref);
            gmb_gfxctx_h_op(hnd443, op443, a443, b443, c443, d443, s443);
            return; }
        default: break;
    }
    (void)argc;
    tui_ensure(vm);
    for (size_t i = 0; i < sizeof(TUI_SYS_TBL) / sizeof(TUI_SYS_TBL[0]); i++)
        if (TUI_SYS_TBL[i].code == code) { TUI_SYS_TBL[i].fn(vm, task); return; }
    /* SET_CLIPPER (295) e sconosciuti: consuma gli argomenti */
    for (int i = 0; i < argc && task->sp > 0; i++)
    {
        gmb_stack_entry_t a = gmb_task_pop(task);
        gmb_obj_release(a.ref);
    }
}

/* ---------------------------------------------------------------------------
 * gmb_vm_exec_console_syscall — syscall console/Clipper NON widget (input, schermo,
 * impostazioni). Headless-safe: input → stringa vuota/0, video/set → no-op, ma
 * consuma SEMPRE gli argomenti dallo stack (il compilatore emette AssignVariable
 * o OP_POP dopo la syscall, quindi lo stack deve restare coerente).
 *
 * I codici sono quelli di common/Compiler/SysCalls.cs, riferiti PER NOME (GMB_SC_<NOME>,
 * gmb_syscall_codes.h generato da tools/check_enum_ids --gen): mai numeri scritti a mano.
 * Famiglie coperte: INPUT/INKEY/GET_KEY/WAITKEY, CCLS, CURSOR_STATE, SCREEN*, SAVE/RESTORE_SCREEN,
 * SET_CLIPPER, WINDOW_*, CONSOLE_CAPTION/SENDKEY, PALETTE_*.
 * --------------------------------------------------------------------------- */
/* Window manager statico condiviso tra WINDOW_DEFINE e WINDOW_ACTION della VM/C.
 * Le finestre vivono nel modulo (nessun rendering modale richiesto dai sample
 * headless), le azioni raise/remove/minimize/restore aggiornano lo stato. */
gmb_window_manager_t* gmb_vm_tui_static_wm(void)
{
    static gmb_window_manager_t wm;
    static bool inited = false;
    if (!inited)
    {
        gmb_window_manager_init(&wm, gmb_rect_new(1, 1, 80, 24));
        inited = true;
    }
    return &wm;
}

/* console/Clipper syscall per-code -> tabella {code, handler} (refactor §18.5).
 * I code non in tabella (PALETTE 324/325/326/335/336 e range non implementati) cadono
 * nel default storico: consuma gli argomenti. WINDOW_ACTION (297) mantiene lo switch
 * interno su action (azione del window-manager, non dispatch del linguaggio). */
