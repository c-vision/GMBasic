/* gmb_vm_tui_controls.c — controlli/widget/dialoghi TUI della VM/C (syscall 273-289).
 * Estratto da gmb_vm_tui.c (§0quater: soglia 1200 righe). Nessuna modifica di logica.
 */
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

void tui_checkbox(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* caption = tui_pop_str(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    int w = (int)strlen(caption) + 6; if (w < 12) w = 12;
    gmb_tui_add_checkbox(col, row, w, caption);   /* non-bloccante: registra nel registry */
    tui_push_bool(task, 0);
}

void tui_listbox(gmb_vm_t* vm, gmb_task_t* task)
{
    char items[64][256];
    int n = tui_pop_str_list(task, items, 64);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[4096]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_tui_add_listbox(col, row, w, h > 1 ? h : 1, csv);   /* registry */
    tui_push_str(task, "");
}

void tui_combobox(gmb_vm_t* vm, gmb_task_t* task)
{
    char items[64][256];
    int n = tui_pop_str_list(task, items, 64);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[4096]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_tui_add_combobox(col, row, w, csv);   /* registry */
    tui_push_str(task, n > 0 ? items[0] : "");   /* label iniziale (parità IL/C99) */
}

void tui_toggle(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* caption = tui_pop_str(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    int w = (int)strlen(caption) + 6; if (w < 12) w = 12;
    gmb_tui_add_toggle(col, row, w, caption);   /* registry */
    tui_push_bool(task, 0);
}

void tui_radiogroup(gmb_vm_t* vm, gmb_task_t* task)
{
    char items[64][256];
    int n = tui_pop_str_list(task, items, 64);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[4096]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_tui_add_radiogroup(col, row, 20, n > 0 ? n : 1, csv);   /* registry */
    tui_push_str(task, n > 0 ? items[0] : "");   /* label iniziale (parità IL/C99) */
}

void tui_slider(gmb_vm_t* vm, gmb_task_t* task)
{
    int step = (int)tui_pop_num(task);
    int max = (int)tui_pop_num(task);
    int min = (int)tui_pop_num(task);
    int len = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    (void)step;
    gmb_tui_add_slider(col, row, len > 2 ? len : 10, min, max, min);   /* registry */
    tui_push_num(task, (double)min, GMB_BT_NUMBER);
}

void tui_numericupdown(gmb_vm_t* vm, gmb_task_t* task)
{
    int step = (int)tui_pop_num(task);
    int max = (int)tui_pop_num(task);
    int min = (int)tui_pop_num(task);
    int len = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    (void)step;
    gmb_tui_add_numericupdown(col, row, len > 2 ? len : 10, min, max, min);   /* registry */
    tui_push_num(task, (double)min, GMB_BT_NUMBER);
}

void tui_starrating(gmb_vm_t* vm, gmb_task_t* task)
{
    int value = (int)tui_pop_num(task);
    int max = (int)tui_pop_num(task);
    int len = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_starrating(col, row, len > 2 ? len : 10, max, value);   /* registry */
    tui_push_num(task, (double)value, GMB_BT_NUMBER);
}

void tui_scrollbar(gmb_vm_t* vm, gmb_task_t* task)
{
    int value = (int)tui_pop_num(task);
    int max = (int)tui_pop_num(task);
    int min = (int)tui_pop_num(task);
    int len = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_scrollbar(col, row, len > 2 ? len : 10, min, max, value);   /* registry */
    tui_push_num(task, (double)value, GMB_BT_NUMBER);
}

void tui_scrollview(gmb_vm_t* vm, gmb_task_t* task)
{
    char lines[128][256];
    int n = tui_pop_str_list(task, lines, 128);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[8192]; tui_join_csv(lines, n, csv, (int)sizeof(csv));
    gmb_tui_add_scrollview(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_label(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* text = tui_pop_str(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_label(col, row, w, text);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_datalist(gmb_vm_t* vm, gmb_task_t* task)
{
    char items[128][256];
    int n = tui_pop_str_list(task, items, 128);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[8192]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_tui_add_datalist(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_propertygrid(gmb_vm_t* vm, gmb_task_t* task)
{
    char items[128][256];
    int n = tui_pop_str_list(task, items, 128);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[8192]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_tui_add_propertygrid(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_logview(gmb_vm_t* vm, gmb_task_t* task)
{
    char records[128][256];
    int n = tui_pop_str_list(task, records, 128);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[8192]; tui_join_csv(records, n, csv, (int)sizeof(csv));
    gmb_tui_add_logview(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_hexview(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* data = tui_pop_str(task);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_hexview(col, row, w, h, data);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_bigtext(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* text = tui_pop_str(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_bigtext(col, row, text);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_codearea(gmb_vm_t* vm, gmb_task_t* task)
{
    char lines[128][256];
    int n = tui_pop_str_list(task, lines, 128);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[8192]; tui_join_csv(lines, n, csv, (int)sizeof(csv));
    gmb_tui_add_codearea(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_reorderlistbox(gmb_vm_t* vm, gmb_task_t* task)
{
    char items[128][256];
    int n = tui_pop_str_list(task, items, 128);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[8192]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_tui_add_reorderlistbox(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_alertbox(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* text = tui_pop_str(task);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_alertbox(col, row, w, h, text);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_confirmationbox(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* text = tui_pop_str(task);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_confirmationbox(col, row, w, h, text);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_fileexplorer(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* path = tui_pop_str(task);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_fileexplorer(col, row, w, h, path);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_dashboard(gmb_vm_t* vm, gmb_task_t* task)
{
    char metrics[256][256];
    int n = tui_pop_str_list(task, metrics, 256);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[16384]; tui_join_csv(metrics, n, csv, (int)sizeof(csv));
    gmb_tui_add_dashboard(col, row, w, h, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_splitter(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm;
    int orient = (int)tui_pop_num(task);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_control_t* c = gmb_tui_add_splitter(col, row, w, h, orient);   /* registry */
    /* INTO = HANDLE del controllo (parità 4 backend). */
    tui_push_num(task, c ? (double)c->handle : 0.0, GMB_BT_NUMBER);
}

void tui_contextmenu(gmb_vm_t* vm, gmb_task_t* task)
{
    (void)vm;
    char items[16][64];
    int n = tui_pop_str_list(task, items, 16);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char csv[2048]; tui_join_csv(items, n, csv, (int)sizeof(csv));
    gmb_control_t* c = gmb_tui_add_contextmenu(col, row, w, h, csv);   /* registry */
    /* INTO = HANDLE del controllo (parità 4 backend; non la lista items). */
    tui_push_num(task, c ? (double)c->handle : 0.0, GMB_BT_NUMBER);
}

void tui_tab(gmb_vm_t* vm, gmb_task_t* task)
{
    char tabs[64][256];
    int n = tui_pop_str_list(task, tabs, 64);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    (void)h;
    char csv[4096]; tui_join_csv(tabs, n, csv, (int)sizeof(csv));
    gmb_tui_add_tab(col, row, w, csv);   /* registry */
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
}

void tui_grid(gmb_vm_t* vm, gmb_task_t* task)
{
    char data[256][256];
    int dn = tui_pop_str_list(task, data, 256);
    char headers[64][256];
    int hn = tui_pop_str_list(task, headers, 64);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    (void)h;
    int cols = hn > 0 ? hn : 1;
    char allc[512][256]; int cc = 0;
    for (int i = 0; i < hn && cc < 512; i++) snprintf(allc[cc++], 256, "%s", headers[i]);
    for (int i = 0; i < dn && cc < 512; i++) snprintf(allc[cc++], 256, "%s", data[i]);
    char gcsv[65536]; tui_join_csv((const char (*)[256])allc, cc, gcsv, (int)sizeof(gcsv));
    gmb_tui_add_grid(col, row, w, h > 1 ? h : 1, cols, gcsv);   /* registry */
    tui_push_num(task, -1.0, GMB_BT_NUMBER);
}

void tui_datepicker(gmb_vm_t* vm, gmb_task_t* task)
{
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_datepicker(col, row, 22);   /* registry */
    tui_push_str(task, "");
}

void tui_colorpicker(gmb_vm_t* vm, gmb_task_t* task)
{
    int cols = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    gmb_tui_add_colorpicker(col, row, cols > 0 ? cols : 16, 8);   /* registry */
    tui_push_num(task, -1.0, GMB_BT_NUMBER);
}

void tui_treeview(gmb_vm_t* vm, gmb_task_t* task)
{
    char raw[64][256];
    int n = tui_pop_str_list(task, raw, 64);
    int h = (int)tui_pop_num(task);
    int w = (int)tui_pop_num(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    char tcsv[8192]; tui_join_csv((const char (*)[256])raw, n, tcsv, (int)sizeof(tcsv));
    gmb_tui_add_treeview(col, row, w, h > 1 ? h : 1, tcsv);   /* registry */
    tui_push_str(task, "");
}

/* ---- Menu (custom modal: ENTER apre/sottomenu, ESC chiude) ---- */
static int tui_menu_cb(const gmb_ui_event_t* ev, void* ctx)
{
    tui_modal_ctx_t* mc = (tui_modal_ctx_t*)ctx;
    gmb_menu_t* m = (gmb_menu_t*)mc->ctrl;
    if (ev->type == GMB_UIEVENT_KEY)
    {
        if (ev->key == GMB_K_ENTER)
        {
            gmb_menu_t* mm = (gmb_menu_t*)mc->ctrl;
            if (mm->open)
            {
                const char* lbl = gmb_menu_get_selected_label(mm);
                if (lbl && lbl[0])
                {
                    int si = (int)mm->ival;
                    gmb_menu_item_t* it = gmb_menu_item_at(mm, si);
                    if (it && it->sub_count == 0)
                    {
                        mc->exit_key = GMB_K_ENTER;
                        mc->result = si;
                        mc->done = true;
                        return 0;
                    }
                }
            }
            gmb_control_handle_key(&mm->ctrl, ev->key);
            return 1;
        }
        if (ev->key == GMB_K_ESC)
        {
            gmb_menu_t* mm = (gmb_menu_t*)mc->ctrl;
            if (mm->open) { gmb_control_handle_key(&mm->ctrl, ev->key); return 1; }
            mc->exit_key = GMB_K_ESC;
            mc->result = -1;
            mc->done = true;
            return 0;
        }
        gmb_control_handle_key(mc->ctrl, ev->key);
    }
    gmb_screen_clear(mc->tui->buf, GMB_CELLCOLOR_BLACK, GMB_CELLCOLOR_BLACK);
    gmb_control_draw(mc->ctrl, mc->tui->buf);
    if (mc->tui->render) mc->tui->render(mc->tui->buf, mc->tui->render_ctx);
    return 1;
}

void tui_menu(gmb_vm_t* vm, gmb_task_t* task)
{
    gmb_stack_entry_t itemsRaw = gmb_task_pop(task);
    int col = (int)tui_pop_num(task);
    int row = (int)tui_pop_num(task);
    static gmb_menu_t m;
    gmb_menu_init(&m, col, row, 20);
    /* items: lista di (stringa | lista [label, sub...]) OPPURE stringa CSV "a,b,c"
     * (forma del minitest e di C#: PopStrArr su stringa). Senza voci il modale ignora
     * ENTER (nessuna selezione possibile) → hang in harness: mai menu vuoto. */
    if (itemsRaw.type == 2 && itemsRaw.ref && itemsRaw.ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)itemsRaw.ref;
        gmb_list_t* l = sv->list;
        for (size_t i = 0; i < l->count; i++)
        {
            gmb_val_t v = l->items[i];
            if (v.kind == GMB_TYPE_STRING && v.v_string.data)
            {
                gmb_menu_item_t* it = gmb_menu_add_item(&m, v.v_string.data);
                if (it) gmb_menu_item_set_action(it, NULL, NULL);
            }
            else if (v.kind == GMB_TYPE_OBJECT && v.v_ptr &&
                     ((gmb_obj_t*)v.v_ptr)->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sub = (gmb_obj_shared_val_t*)v.v_ptr;
                if (sub->list && sub->list->count >= 1 && sub->list->items[0].kind == GMB_TYPE_STRING && sub->list->items[0].v_string.data)
                {
                    gmb_menu_item_t* it = gmb_menu_add_item(&m, sub->list->items[0].v_string.data);
                    for (size_t j = 1; it && j < sub->list->count; j++)
                    {
                        gmb_val_t sv2 = sub->list->items[j];
                        if (sv2.kind == GMB_TYPE_STRING && sv2.v_string.data)
                            gmb_menu_item_add_sub(it, sv2.v_string.data);
                    }
                }
            }
        }
    }
    else if (itemsRaw.type == 2 && itemsRaw.ref && itemsRaw.ref->kind == GMB_OBJ_STRING)
    {
        /* CSV "a,b,c": split manuale (non alloca dal runtime condiviso per non
         * legare il ciclo di vita; max 16 voci come il buffer del menu). */
        const char* csv = ((gmb_obj_string_t*)itemsRaw.ref)->data;
        if (csv && *csv)
        {
            char tmp[1024];
            snprintf(tmp, sizeof(tmp), "%s", csv);
            for (char* tok = strtok(tmp, ","); tok; tok = strtok(NULL, ","))
            {
                while (*tok == ' ') tok++;
                if (*tok) gmb_menu_add_item(&m, tok);
            }
        }
    }
    gmb_obj_release(itemsRaw.ref);
    gmb_tui_control_register(&m.ctrl);   /* non-bloccante: registry (conserva i sottomenu) */
    tui_push_num(task, -1.0, GMB_BT_NUMBER);
}

/* ---- FileDialog (custom modal: enter entra nelle dir / ritorna path, esc annulla) ---- */
static int tui_filedialog_cb(const gmb_ui_event_t* ev, void* ctx)
{
    tui_modal_ctx_t* mc = (tui_modal_ctx_t*)ctx;
    gmb_file_dialog_t* fd = (gmb_file_dialog_t*)mc->ctrl;
    if (ev->type == GMB_UIEVENT_KEY)
    {
        if (ev->key == GMB_K_ENTER)
        {
            int idx = fd->selected_index;
            if (idx >= 0 && idx < fd->entries_count)
            {
                mc->result = idx;
                mc->exit_key = GMB_K_ENTER;
                mc->done = true;
                return 0;
            }
            return 1;
        }
        if (ev->key == GMB_K_ESC)
        {
            mc->exit_key = GMB_K_ESC;
            mc->result = -1;
            mc->done = true;
            return 0;
        }
        gmb_control_handle_key(&fd->ctrl, ev->key);
    }
    gmb_screen_clear(mc->tui->buf, GMB_CELLCOLOR_BLACK, GMB_CELLCOLOR_BLACK);
    gmb_control_draw(&fd->ctrl, mc->tui->buf);
    if (mc->tui->render) mc->tui->render(mc->tui->buf, mc->tui->render_ctx);
    return 1;
}

void tui_filedialog(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* title = tui_pop_str(task);
    const char* mask = tui_pop_str(task);
    (void)title;
    static gmb_file_dialog_t fd;
    gmb_file_dialog_init(&fd, 2, 2, 60, 12);
    gmb_file_entry_t entries[512];
    int n = 0;
    DIR* d = opendir(".");
    if (d)
    {
        struct dirent* de;
        while ((de = readdir(d)) != NULL && n < 512)
        {
            if (de->d_name[0] == '.') continue;
            snprintf(entries[n].name, sizeof(entries[n].name), "%s", de->d_name);
            entries[n].is_dir = de->d_type == DT_DIR;
            n++;
        }
        closedir(d);
    }
    gmb_file_dialog_set_entries(&fd, entries, n);
    gmb_tui_control_register(&fd.ctrl);   /* non-bloccante */
    tui_push_str(task, "");
}

void tui_printer(gmb_vm_t* vm, gmb_task_t* task)
{
    const char* title = tui_pop_str(task);
    (void)title;
    static gmb_printer_dialog_t pd;
    gmb_printer_dialog_init(&pd);
    gmb_printer_dialog_add_printer(&pd, "Default");
    gmb_printer_dialog_add_printer(&pd, "PDF");
    gmb_printer_dialog_add_printer(&pd, "Office Laser");
    gmb_tui_control_register(&pd.ctrl);   /* non-bloccante */
    tui_push_str(task, "");
    tui_push_num(task, 0.0, GMB_BT_NUMBER);
    tui_push_bool(task, 0);
}

