/* gmb_tui_controls_add.c — factory per i controlli TUI nel REGISTRY NON-bloccante
 * (mirror di gmb_form_add_* per il pixel). Alloca lo stato (persistente), registra e ritorna.
 * Ogni factory imposta ctrl.ctrl_kind (GMB_CK_*) per gli accessor generici CONTROL SET/GET. */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_form.h"
#include "gmb_graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

extern int  gmb_gfx_screen_on(void);
extern void gmb_form_frame(int frame_ms);

void gmb_tui_control_register(gmb_control_t* c) { gmb_tui_control_add(c); }

/* Pump unificato: GRAPHICS -> gmb_form_frame; TEXT -> gmb_tui_controls_frame. Stesso modello.
 *
 * GPM + SDL: il pump può girare su un WORKER GPM (WINDOW PUMP è BASIC). La UI (SDL) può
 * stare SOLO sul main thread: il main ha già il suo present (`gmb_tui_present_gfx`, che
 * renderizza e presenta ogni ms). Qui dal worker si fa solo l'evento/park e la
 * sincronizzazione; il present si fa solo se siamo sul main thread. Prima si chiamava
 * `gmb_gfx_present_tick()` sempre: SDL dal worker → segfault dopo pochi frame. */
void gmb_ui_frame(int ms)
{
    if (gmb_gfx_screen_on())
    {
        gmb_form_frame(ms);
        /* I tasti SDL vanno ai controlli anche in grafica (il frame TUI non gira lì). */
        gmb_tui_process_pending_keys();
        { extern void gmb_tui_sync_bindings(void); gmb_tui_sync_bindings(); }
        { extern int gmb_gfx_present_on_main_thread(void);
          if (gmb_gfx_present_on_main_thread())
          {
              extern void gmb_form_render_if_dirty(void);
              extern void gmb_gfx_present_tick(void);
              gmb_form_render_if_dirty();
              gmb_gfx_present_tick();
          } }
    }
    else gmb_tui_controls_frame(ms);
}

/* Split di una lista "a|b|c" in out[][]. Ritorna il numero di item. */
static int tui_split(const char* s, char out[][64], int cap)
{
    int n = 0; const char* p = s ? s : "";
    while (*p && n < cap)
    {
        int i = 0;
        while (*p && *p != '|' && i < 63) out[n][i++] = *p++;
        out[n][i] = 0; n++;
        if (*p == '|') p++;
    }
    if (n == 0 && cap > 0) { out[0][0] = 0; n = 0; }
    return n;
}
static void tui_set_arr(const char (*tmp)[64], int n, const char* arr[64])
{ for (int i = 0; i < n && i < 64; i++) arr[i] = tmp[i]; }

gmb_control_t* gmb_tui_add_button(int x, int y, int w, const char* label)
{ gmb_button_t* c = (gmb_button_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_button_init(c, x, y, w, label); c->ctrl.ctrl_kind = GMB_CK_BUTTON; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_checkbox(int x, int y, int w, const char* label)
{ gmb_checkbox_t* c = (gmb_checkbox_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_checkbox_init(c, x, y, w, label); c->ctrl.ctrl_kind = GMB_CK_CHECKBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_statusbar(int x, int y, int w, const char* text)
{ gmb_widget_t* c = (gmb_widget_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_status_bar_init(c, x, y, w); gmb_status_bar_set_text(c, text ? text : ""); c->ctrl.ctrl_kind = GMB_CK_STATUSBAR; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_toggle(int x, int y, int w, const char* caption)
{ gmb_toggle_button_t* c = (gmb_toggle_button_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_toggle_button_init(c, x, y, w, caption); c->ctrl.ctrl_kind = GMB_CK_TOGGLE; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_listbox(int x, int y, int w, int h, const char* csv)
{ gmb_listbox_t* c = (gmb_listbox_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_listbox_init(c, x, y, w, h); char tmp[64][64]; int n = tui_split(csv, tmp, 64); const char* arr[64]; tui_set_arr(tmp, n, arr); gmb_listbox_set_items(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_LISTBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_combobox(int x, int y, int w, const char* csv)
{ gmb_combobox_t* c = (gmb_combobox_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_combobox_init(c, x, y, w); char tmp[64][64]; int n = tui_split(csv, tmp, 64); const char* arr[64]; tui_set_arr(tmp, n, arr); gmb_combobox_set_items(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_COMBOBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_radiogroup(int x, int y, int w, int h, const char* csv)
{ gmb_radio_group_t* c = (gmb_radio_group_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_radio_group_init(c, x, y, w, h); char tmp[64][64]; int n = tui_split(csv, tmp, 64); const char* arr[64]; tui_set_arr(tmp, n, arr); gmb_radio_group_set_options(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_RADIO; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_tab(int x, int y, int w, const char* csv)
{ gmb_tab_t* c = (gmb_tab_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_tab_init(c, x, y, w); char tmp[64][64]; int n = tui_split(csv, tmp, 64); const char* arr[64]; tui_set_arr(tmp, n, arr); gmb_tab_set_tabs(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_TAB; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_menu(int x, int y, int w, const char* csv)
{ gmb_menu_t* c = (gmb_menu_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_menu_init(c, x, y, w); char tmp[64][64]; int n = tui_split(csv, tmp, 64); for (int i = 0; i < n; i++) gmb_menu_add_item(c, tmp[i]); c->ctrl.ctrl_kind = GMB_CK_MENU; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_spinner(int x, int y, int w, const char* preset)
{ gmb_spinner_t* c = (gmb_spinner_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_spinner_init(c, x, y, w); if (preset && *preset) gmb_spinner_set_preset(c, preset); c->ctrl.ctrl_kind = GMB_CK_SPINNER; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_slider(int x, int y, int w, int mn, int mx, int val)
{ gmb_slider_t* c = (gmb_slider_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_slider_init(c, x, y, w); gmb_slider_set_range(c, mn, mx); gmb_slider_set_value(c, val); c->ctrl.ctrl_kind = GMB_CK_SLIDER; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_scrollview(int x, int y, int w, int h, const char* csv)
{ gmb_scroll_view_t* c = (gmb_scroll_view_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_scroll_view_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); const char* arr[128]; tui_set_arr(tmp, n, arr); gmb_scroll_view_set_content(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_SCROLLVIEW; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_label(int x, int y, int w, const char* text)
{ gmb_label_t* c = (gmb_label_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_label_init(c, x, y, w, text ? text : ""); c->ctrl.ctrl_kind = GMB_CK_LABEL; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_textinput(int x, int y, int w, const char* initial)
{ gmb_text_input_t* c = (gmb_text_input_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_text_input_init(c, x, y, w); if (initial) gmb_text_input_set_text(c, initial); c->ctrl.ctrl_kind = GMB_CK_TEXTBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_datalist(int x, int y, int w, int h, const char* csv)
{ gmb_data_list_t* c = (gmb_data_list_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_data_list_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); const char* arr[128]; tui_set_arr(tmp, n, arr); gmb_data_list_set_items(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_DATALIST; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_propertygrid(int x, int y, int w, int h, const char* csv)
{ gmb_property_grid_t* c = (gmb_property_grid_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_property_grid_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); const char* arr[128]; tui_set_arr(tmp, n, arr); for (int i = 0; i + 1 < n; i += 2) gmb_property_grid_add(c, arr[i], arr[i + 1]); c->ctrl.ctrl_kind = GMB_CK_PROPERTYGRID; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_logview(int x, int y, int w, int h, const char* csv)
{ gmb_log_view_t* c = (gmb_log_view_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_log_view_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); const char* arr[128]; tui_set_arr(tmp, n, arr); for (int i = 0; i < n; i++) gmb_log_view_push(c, 0, "", arr[i]); c->ctrl.ctrl_kind = GMB_CK_LOGVIEW; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_hexview(int x, int y, int w, int h, const char* data)
{ gmb_hex_view_t* c = (gmb_hex_view_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_hex_view_init(c, x, y, w, h); gmb_hex_view_set_data(c, (const unsigned char*)(data ? data : ""), data ? (int)strlen(data) : 0); c->ctrl.ctrl_kind = GMB_CK_HEXVIEW; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_bigtext(int x, int y, const char* text)
{ gmb_big_text_t* c = (gmb_big_text_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_big_text_init(c, x, y); gmb_big_text_set_text(c, text ? text : ""); c->ctrl.ctrl_kind = GMB_CK_BIGTEXT; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_codearea(int x, int y, int w, int h, const char* csv)
{ gmb_code_area_t* c = (gmb_code_area_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_code_area_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); const char* arr[128]; tui_set_arr(tmp, n, arr); gmb_code_area_set_lines(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_CODEAREA; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_reorderlistbox(int x, int y, int w, int h, const char* csv)
{ gmb_reorder_listbox_t* c = (gmb_reorder_listbox_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_reorder_listbox_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); const char* arr[128]; tui_set_arr(tmp, n, arr); gmb_reorder_listbox_set_items(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_REORDERLISTBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_alertbox(int x, int y, int w, int h, const char* text)
{ gmb_alert_box_t* c = (gmb_alert_box_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_alert_box_init(c, x, y, w, h); if (text) snprintf(c->message, sizeof(c->message), "%s", text); c->ctrl.ctrl_kind = GMB_CK_ALERTBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_confirmationbox(int x, int y, int w, int h, const char* text)
{ gmb_confirmation_box_t* c = (gmb_confirmation_box_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_confirmation_box_init(c, x, y, w, h); if (text) gmb_confirmation_box_with_message(c, text); c->ctrl.ctrl_kind = GMB_CK_CONFIRMATIONBOX; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_fileexplorer(int x, int y, int w, int h, const char* path)
{ gmb_file_explorer_t* c = (gmb_file_explorer_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_file_explorer_init(c, path ? path : "."); gmb_file_explorer_set_bounds(c, x, y, w, h); c->ctrl.ctrl_kind = GMB_CK_FILEEXPLORER; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_dashboard(int x, int y, int w, int h, const char* csv)
{ gmb_dashboard_t* c = (gmb_dashboard_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_dashboard_init(c, x, y, w, h); char tmp[128][64]; int n = tui_split(csv, tmp, 128); for (int i = 0; i + 3 < n; i += 4) gmb_dashboard_add_metric(c, tmp[i], (float)atof(tmp[i + 1]), (float)atof(tmp[i + 2]), tmp[i + 3]); c->ctrl.ctrl_kind = GMB_CK_DASHBOARD; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }


/* Getter testo (CodeArea/TextField) del controllo registrato all'indice i. Ritorna len. */
int gmb_tui_control_text(int i, char* out, int cap)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || !out || cap <= 0) return 0;
    out[0] = 0; int w = 0;
    if (c->ctrl_kind == GMB_CK_CODEAREA)
    {
        gmb_code_area_t* ca = (gmb_code_area_t*)c;
        for (int k = 0; k < ca->lines_count; k++)
        {
            int ln = (int)strlen(ca->lines[k]);
            if (w + ln + 2 < cap) { memcpy(out + w, ca->lines[k], (size_t)ln); w += ln; out[w++] = '\n'; }
        }
    }
    else if (c->ctrl_kind == GMB_CK_LISTBOX)
    {
        const char* sv = gmb_listbox_selected_value((gmb_listbox_t*)c);
        if (sv) { int ln = (int)strlen(sv); if (ln < cap) { memcpy(out, sv, (size_t)ln); w = ln; } }
    }
    else if (c->ctrl_kind == GMB_CK_TEXTBOX)
    {
        const char* tv = ((gmb_widget_t*)c)->itext;
        if (tv) { int ln = (int)strlen(tv); if (ln < cap) { memcpy(out, tv, (size_t)ln); w = ln; } }
    }
    out[w] = 0;
    return w;
}

/* Valore/selezione generica di un controllo registrato (TAB: indice tab attivo). -1 se n/d. */
long long gmb_tui_control_value(int i)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c) return -1;
    if (c->ctrl_kind == GMB_CK_TAB) return (int)((gmb_widget_t*)c)->ival;
    return -1;
}

/* Marca/toglie un errore di lint su una riga del CodeArea registrato (squiggle nel gutter). */
void gmb_tui_control_error(int i, int line, int on)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (c && c->ctrl_kind == GMB_CK_CODEAREA) gmb_code_area_set_error((gmb_code_area_t*)c, line, on ? true : false);
}
void gmb_tui_control_clear_errors(int i)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (c && c->ctrl_kind == GMB_CK_CODEAREA) gmb_code_area_clear_errors((gmb_code_area_t*)c);
}

/* ---- FFI-friendly (per il BASIC via DECLARE ... LIB "gmb_gfx") ----
 * L'IDE TUI in BASIC legge/scrive i controlli per indice con ritorni `const char*` (buffer
 * statico condiviso) invece che con out-buffer (che il FFI BASIC non può passare). */
static char g_tui_textbuf[1 << 20];

/* Testo del controllo i come stringa: CodeArea -> righe unite con '\n'; ListBox -> valore
 * selezionato. Valido fino alla chiamata successiva. */
const char* gmb_tui_control_text_ptr(int i)
{
    gmb_tui_control_text(i, g_tui_textbuf, (int)sizeof(g_tui_textbuf));
    return g_tui_textbuf;
}

/* Imposta il contenuto del controllo i (split su '\n'): CodeArea -> righe; ListBox -> voci. */
void gmb_tui_control_set_text(int i, const char* text)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || !text) return;
    static char tmp[GMB_CODE_MAX_LINES][GMB_CODE_MAX_COLS];
    int n = 0;
    const char* p = text;
    while (n < GMB_CODE_MAX_LINES)
    {
        int col = 0;
        while (*p && *p != '\n' && col < GMB_CODE_MAX_COLS - 1) tmp[n][col++] = *p++;
        tmp[n][col] = 0; n++;
        if (*p == '\n') p++; else break;
    }
    if (c->ctrl_kind == GMB_CK_CODEAREA)
    {
        const char* arr[GMB_CODE_MAX_LINES];
        for (int k = 0; k < n; k++) arr[k] = tmp[k];
        gmb_code_area_set_lines((gmb_code_area_t*)c, arr, n);
    }
    else if (c->ctrl_kind == GMB_CK_LISTBOX)
    {
        const char* arr[GMB_CODE_MAX_LINES];
        for (int k = 0; k < n; k++) arr[k] = tmp[k];
        gmb_listbox_set_items((gmb_listbox_t*)c, arr, n);
    }
    else if (c->ctrl_kind == GMB_CK_TEXTBOX)
    {
        gmb_text_input_set_text((gmb_text_input_t*)c, tmp[0]);
    }
    else if (c->ctrl_kind == GMB_CK_STATUSBAR)
    {
        snprintf(((gmb_widget_t*)c)->label, sizeof(((gmb_widget_t*)c)->label), "%s", text ? text : "");
    }
    else if (c->ctrl_kind == GMB_CK_LABEL)
    {
        gmb_label_set_text((gmb_label_t*)c, text);
    }
}

/* Posiziona il cursore del CodeArea i sulla riga (0-based) e scrolla per renderla visibile.
 * Ritorna la riga cursore finale, -1 se il controllo non è un CodeArea. */
long long gmb_code_area_goto_line(int i, int line)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return -1;
    gmb_code_area_t* ca = (gmb_code_area_t*)c;
    if (line < 0) line = 0;
    if (line >= ca->lines_count) line = ca->lines_count > 0 ? ca->lines_count - 1 : 0;
    ca->cursor_line = line; ca->cursor_col = 0;
    if (line < ca->scroll_offset) ca->scroll_offset = line;
    if (line >= ca->scroll_offset + c->height) ca->scroll_offset = line - c->height + 1;
    if (ca->scroll_offset < 0) ca->scroll_offset = 0;
    return ca->cursor_line;
}

/* Posizione (cella) del cursore di un CodeArea registrato: utile per il caret della console.
 * Ritorna 1 se il controllo e' un CodeArea (row/col = coordinate 0-based nel buffer). */
int gmb_tui_control_cursor(int i, int* row, int* col)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return 0;
    gmb_code_area_t* ca = (gmb_code_area_t*)c;
    int gw = ca->show_line_numbers ? gmb_code_area_gutter(ca) : 0;
    if (row) *row = c->y + (ca->cursor_line - ca->scroll_offset);
    if (col) *col = c->x + gw + ca->cursor_col;
    return 1;
}

/* FFI per l'IDE: riga del cursore (0-based) di un CodeArea, o -1 se non e' un CodeArea. */
long long gmb_tui_code_cursor_line(int i)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return -1;
    return ((gmb_code_area_t*)c)->cursor_line;
}

/* FFI per l'IDE: numero di righe di un CodeArea, o -1 se non e' un CodeArea. */
long long gmb_tui_code_line_count(int i)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return -1;
    return ((gmb_code_area_t*)c)->lines_count;
}

/* FFI per l'IDE: inserisce `text` alla posizione del cursore del CodeArea i (riusa il
 * key handler dei caratteri/Enter, cosi' la semantica di editing resta UNA). Ritorna 1. */
long long gmb_tui_code_insert(int i, const char* text)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA || !text) return 0;
    for (const char* p = text; *p; p++)
    {
        int k = (*p == '\n') ? GMB_K_ENTER : (unsigned char)*p;
        gmb_control_handle_key(c, k);
    }
    return 1;
}

/* FFI per l'IDE: testo della riga n (0-based). Puntatore INTERNO (l'FFI legge la stringa). */
const char* gmb_tui_code_line_text(int i, int n)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return "";
    gmb_code_area_t* ca = (gmb_code_area_t*)c;
    if (n < 0 || n >= ca->lines_count) return "";
    return ca->lines[n];
}

/* FFI per l'IDE: cancella la riga n (0-based). Ritorna 1 se cancellata. */
long long gmb_tui_code_delete_line(int i, int n)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return 0;
    gmb_code_area_t* ca = (gmb_code_area_t*)c;
    if (n < 0 || n >= ca->lines_count || ca->lines_count <= 1) return 0;
    for (int k = n; k < ca->lines_count - 1; k++) strcpy(ca->lines[k], ca->lines[k + 1]);
    ca->lines_count--;
    if (ca->cursor_line >= ca->lines_count) ca->cursor_line = ca->lines_count - 1;
    return 1;
}

/* FFI per l'IDE: prima riga (>= from, con wrap) contenente `needle`. Numero riga o -1. */
long long gmb_tui_code_find(int i, const char* needle, int from)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA || !needle || !*needle) return -1;
    gmb_code_area_t* ca = (gmb_code_area_t*)c;
    if (from < 0 || from >= ca->lines_count) from = 0;
    for (int n = from; n < ca->lines_count; n++) if (strstr(ca->lines[n], needle)) return n;
    for (int n = 0; n < from; n++) if (strstr(ca->lines[n], needle)) return n;
    return -1;
}

/* FFI per l'IDE: sostituisce TUTTE le occorrenze di `find` con `repl` (per riga). Ritorna il
 * numero di sostituzioni. */
long long gmb_tui_code_replace(int i, const char* find, const char* repl)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA || !find || !*find || !repl) return 0;
    gmb_code_area_t* ca = (gmb_code_area_t*)c;
    int fl = (int)strlen(find), rl = (int)strlen(repl), count = 0;
    for (int n = 0; n < ca->lines_count; n++)
    {
        char* p = ca->lines[n];
        while ((p = strstr(p, find)) != NULL)
        {
            int cur_len = (int)strlen(ca->lines[n]);
            if (cur_len - fl + rl >= GMB_CODE_MAX_COLS) break;
            memmove(p + rl, p + fl, (size_t)(cur_len - fl - (int)(p - ca->lines[n])) + 1);
            memcpy(p, repl, (size_t)rl);
            p += rl; count++;
        }
    }
    return count;
}

/* Cancella gli errori (squiggle) del CodeArea i e marca le righe indicate da `csv` (1-based,
 * separatore '|' o ';'). Comodo per rifare lo squiggle dopo ogni lint. */
void gmb_tui_control_set_errors(int i, const char* lines_1based)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_CODEAREA) return;
    gmb_code_area_clear_errors((gmb_code_area_t*)c);
    const char* p = lines_1based ? lines_1based : "";
    while (*p)
    {
        while (*p == '|' || *p == ';') p++;
        int v = 0; bool any = false;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = true; }
        if (any && v >= 1) gmb_code_area_set_error((gmb_code_area_t*)c, v - 1, true);
        while (*p && *p != '|' && *p != ';') p++;
    }
}

gmb_control_t* gmb_tui_add_datepicker(int x, int y, int w)
{ gmb_date_picker_t* c = (gmb_date_picker_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_date_picker_init(c, x, y, w); c->ctrl.ctrl_kind = GMB_CK_DATEPICKER; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_colorpicker(int x, int y, int w, int h)
{ gmb_color_picker_t* c = (gmb_color_picker_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_color_picker_init(c, x, y, w, h); c->ctrl.ctrl_kind = GMB_CK_COLORPICKER; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_grid(int x, int y, int w, int h, int cols, const char* data_csv)
{ gmb_grid_t* c = (gmb_grid_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_grid_init(c, x, y, w, h); gmb_grid_set_cols(c, cols < 1 ? 1 : cols); char tmp[128][64]; int n = tui_split(data_csv, tmp, 128); const char* arr[128]; for (int i = 0; i < n && i < 128; i++) arr[i] = tmp[i]; gmb_grid_set_cells(c, arr, n); c->ctrl.ctrl_kind = GMB_CK_GRID; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }

gmb_control_t* gmb_tui_add_treeview(int x, int y, int w, int h, const char* csv)
{
    gmb_tree_view_t* c = (gmb_tree_view_t*)calloc(1, sizeof(*c)); if (!c) return NULL;
    gmb_tree_view_init(c, x, y, w, h);
    char tmp[64][64]; int n = tui_split(csv, tmp, 64);
    for (int i = 0; i < n; i++)
    {
        const char* sd = tmp[i]; const char* lab = sd; while (*lab == ' ') lab++;
        int depth = (int)(lab - sd) / 2;
        char lbl[64]; snprintf(lbl, sizeof(lbl), "%s", lab);
        size_t ll = strlen(lbl); int hc = (ll > 0 && lbl[ll - 1] == '+');
        if (hc) lbl[ll - 1] = 0;
        gmb_tree_view_add_node(c, lbl, depth, hc);
    }
    c->ctrl.ctrl_kind = GMB_CK_TREE;
    gmb_tui_control_add(&c->ctrl); return &c->ctrl;
}

gmb_control_t* gmb_tui_add_filedialog(const char* title, const char* mask)
{
    (void)title; (void)mask;
    gmb_file_dialog_t* fd = (gmb_file_dialog_t*)calloc(1, sizeof(*fd)); if (!fd) return NULL;
    gmb_file_dialog_init(fd, 2, 2, 60, 12);
    gmb_file_entry_t entries[64]; int n = 0;
    DIR* d = opendir(".");
    if (d) { struct dirent* de; while ((de = readdir(d)) && n < 64) { if (de->d_name[0]=='.') continue; snprintf(entries[n].name, sizeof(entries[n].name), "%s", de->d_name); entries[n].is_dir = (de->d_type == DT_DIR); n++; } closedir(d); }
    gmb_file_dialog_set_entries(fd, entries, n);
    fd->ctrl.ctrl_kind = GMB_CK_FILEDIALOG;
    gmb_tui_control_add(&fd->ctrl); return &fd->ctrl;
}

gmb_control_t* gmb_tui_add_printerdialog(const char* title)
{
    (void)title;
    gmb_printer_dialog_t* pd = (gmb_printer_dialog_t*)calloc(1, sizeof(*pd)); if (!pd) return NULL;
    gmb_printer_dialog_init(pd);
    gmb_printer_dialog_add_printer(pd, "Default");
    gmb_printer_dialog_add_printer(pd, "PDF");
    gmb_printer_dialog_add_printer(pd, "Office Laser");
    pd->ctrl.ctrl_kind = GMB_CK_PRINTERDIALOG;
    gmb_tui_control_add(&pd->ctrl); return &pd->ctrl;
}

gmb_control_t* gmb_tui_add_tbrowse(int row, int col, int w, int h, const char* cols_csv, const char* data_csv)
{
    static gmb_vec_ds_t vd;
    static gmb_browse_datasource_t ds;
    static char cells[64][16][64];
    gmb_browse_column_t cols[16]; int ncols = 0;
    memset(cols, 0, sizeof cols);   /* solo header/width sono impostati sotto: il resto (data_type, align, editable, puntatori) era SPAZZATURA -> segfault con righe non vuote */
    char ct[1024]; snprintf(ct, sizeof(ct), "%s", cols_csv ? cols_csv : "");
    { char* csave = NULL;
      for (char* t = strtok_r(ct, "|", &csave); t && ncols < 16; t = strtok_r(NULL, "|", &csave))
      {
        /* campo colonna: "header[:width[:type[:align]]]" */
        char* f[4]; int nf = 0; char* fsave = NULL;
        for (char* g = strtok_r(t, ":", &fsave); g && nf < 4; g = strtok_r(NULL, ":", &fsave)) f[nf++] = g;
        snprintf(cols[ncols].header, sizeof(cols[ncols].header), "%s", nf > 0 ? f[0] : "");
        cols[ncols].width = nf > 1 ? atoi(f[1]) : 10;
        if (cols[ncols].width < 2) cols[ncols].width = 10;
        if (nf > 2 && f[2] && *f[2])
        {
          char tp = f[2][0];
          if (tp == 'N' || tp == 'I') cols[ncols].data_type = GMB_DT_INT;
          else if (tp == 'F') cols[ncols].data_type = GMB_DT_FLOAT;
          else if (tp == 'L' || tp == 'B') cols[ncols].data_type = GMB_DT_BOOL;
          else if (tp == 'D') cols[ncols].data_type = GMB_DT_DATE;
          else if (tp == 'X') cols[ncols].data_type = GMB_DT_CHOICE;
          else cols[ncols].data_type = GMB_DT_TEXT;
        }
        if (nf > 3 && f[3] && *f[3])
        {
          char al = f[3][0];
          if (al == 'R') cols[ncols].align = GMB_ALIGN_RIGHT;
          else if (al == 'C') cols[ncols].align = GMB_ALIGN_CENTER;
          else cols[ncols].align = GMB_ALIGN_LEFT;
        }
        ncols++;
      } }
    if (ncols == 0) { cols[0].header[0] = 0; cols[0].width = 10; ncols = 1; }
    int nrows = 0;
    char dt[8192]; snprintf(dt, sizeof(dt), "%s", data_csv ? data_csv : "");
    { char* rsave = NULL;
      for (char* rr = strtok_r(dt, ";", &rsave); rr && nrows < 64; rr = strtok_r(NULL, ";", &rsave))
      { int c = 0; char* clsave = NULL;
        for (char* cl = strtok_r(rr, "|", &clsave); cl && c < 16; cl = strtok_r(NULL, "|", &clsave)) { snprintf(cells[nrows][c], 64, "%s", cl); c++; } nrows++; } }
    gmb_vec_ds_make(&vd, nrows, ncols);
    for (int r2 = 0; r2 < nrows && r2 < vd.rows; r2++)
        for (int c2 = 0; c2 < ncols && c2 < vd.cols; c2++)
            gmb_vec_ds_set(&vd, r2, c2, cells[r2][c2]);
    gmb_vec_ds_datasource(&vd, &ds);
    gmb_rect_t area = { col, row, w > 0 ? w : 40, h > 0 ? h : 6 };
    gmb_browse_t* b = gmb_browse_new(&area, ncols, cols, &ds);
    if (!b) return NULL;
    b->ctrl.ctrl_kind = GMB_CK_TBROWSE;
    gmb_tui_control_add(&b->ctrl);
    return &b->ctrl;
}

/* ---- Controlli SOLO-GRAFICA nella famiglia UNICA gmb_control_t ----
 * Pixel-only (no-op in TEXT: draw_fn NULL). Registrati nel registry TUI con un draw_px_fn
 * che delega al renderer pixel condiviso gmb_px_draw_ex → stesso path (gmb_tui_draw_px_all)
 * degli altri controlli, invece del vecchio gmb_form_add_*. */
typedef struct { gmb_control_t ctrl; int pxkind; char text[256]; } gmb_gfx_control_t;

static void gfx_control_draw_px(void* u, gmb_control_t* c)
{
    gmb_gfx_control_t* g = (gmb_gfx_control_t*)u;
    /* Rect PIXEL (px_*): è lì che il renderer ha applicato lo spazio coordinate. */
    int x = (c->px_w > 0) ? c->px_x : c->x;
    int y = (c->px_h > 0) ? c->px_y : c->y;
    int w = (c->px_w > 0) ? c->px_w : c->width;
    int h = (c->px_h > 0) ? c->px_h : c->height;
    /* RICHTEXT: righe separate da '|' (prima finivano su una riga sola, con i '|'). */
    if (g->pxkind == GMB_PX_RICHTEXT && g->text[0] && strchr(g->text, '|'))
    {
        gmb_widget_style_t st = gmb_widget_style_current();
        int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;
        char buf[512]; snprintf(buf, sizeof(buf), "%s", g->text);
        char* s1 = NULL; int line = 0;
        for (char* ln = strtok_r(buf, "|", &s1); ln; ln = strtok_r(NULL, "|", &s1), line++)
        {
            if (4 + line * (th + 4) > h) break;
            gmb_gfx_draw_text(ln, x + 4, y + 4 + line * (th + 4), 0,
                              (int)(0x40000000u | (st.text & 0xFFFFFFu)));
        }
        return;
    }
    gmb_px_draw_ex(g->pxkind, x, y, w, h, g->text, NULL, 0, 0, 0, 0, NULL, 0, NULL, 0);
}

static gmb_control_t* gfx_control_add(int ck, int px, int x, int y, int w, int h, const char* text)
{
    gmb_gfx_control_t* g = (gmb_gfx_control_t*)calloc(1, sizeof(*g));
    if (!g) return NULL;
    g->pxkind = px;
    snprintf(g->text, sizeof(g->text), "%s", text ? text : "");
    gmb_control_init(&g->ctrl, "", x, y, w, h, NULL, NULL);
    g->ctrl.user = g;
    g->ctrl.draw_px_fn = gfx_control_draw_px;
    g->ctrl.ctrl_kind = ck;
    gmb_tui_control_add(&g->ctrl);
    return &g->ctrl;
}

gmb_control_t* gmb_tui_add_canvas(int x, int y, int w, int h)
{ return gfx_control_add(GMB_CK_CANVAS, GMB_PX_CANVAS, x, y, w, h, NULL); }
gmb_control_t* gmb_tui_add_image(int x, int y, int w, int h, const char* text)
{ return gfx_control_add(GMB_CK_IMAGE, GMB_PX_IMAGE, x, y, w, h, text); }
gmb_control_t* gmb_tui_add_imagebutton(int x, int y, int w, int h, const char* text)
{ return gfx_control_add(GMB_CK_IMAGEBUTTON, GMB_PX_IMAGEBUTTON, x, y, w, h, text); }
gmb_control_t* gmb_tui_add_qr(int x, int y, int w, int h, const char* text)
{ return gfx_control_add(GMB_CK_QR, GMB_PX_QR, x, y, w, h, text); }
gmb_control_t* gmb_tui_add_richtext(int x, int y, int w, int h, const char* text)
{ return gfx_control_add(GMB_CK_RICHTEXT, GMB_PX_RICHTEXT, x, y, w, h, text); }
gmb_control_t* gmb_tui_add_halfblock(int x, int y, int w, int h, const char* text)
{ return gfx_control_add(GMB_CK_HALFBLOCK, GMB_PX_HALFBLOCK, x, y, w, h, text); }
gmb_control_t* gmb_tui_add_tiling(int x, int y, int w, int h, const char* text)
{ return gfx_control_add(GMB_CK_TILING, GMB_PX_TILING, x, y, w, h, text); }

gmb_control_t* gmb_tui_add_splitter(int x, int y, int w, int h, int orient)
{ gmb_splitter_t* s = (gmb_splitter_t*)calloc(1, sizeof(*s)); if (!s) return NULL; gmb_splitter_init(s, x, y, w, h, orient); gmb_tui_control_add(&s->ctrl); return &s->ctrl; }
gmb_control_t* gmb_tui_add_contextmenu(int x, int y, int w, int h, const char* csv)
{ gmb_context_menu_t* m = (gmb_context_menu_t*)calloc(1, sizeof(*m)); if (!m) return NULL; gmb_context_menu_init(m, x, y, w, h); char tmp[16][64]; int n = tui_split(csv, tmp, 16); const char* arr[16]; tui_set_arr(tmp, n, arr); gmb_context_menu_set_items(m, arr, n); gmb_tui_control_add(&m->ctrl); return &m->ctrl; }
