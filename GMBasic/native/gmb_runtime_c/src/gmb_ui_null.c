/* gmb_ui_null.c — NO-OP dell'API UI per la variante MINIMA del runtime (build testuale).
 *
 * Un eseguibile testuale (PRINT, calcoli, DB) non deve portarsi dietro SDL2/SDL2_ttf/MoltenVK.
 * `gmb_runtime_min` = core (senza GMB_WITH_UI) + questo file: i simboli UI referenziati dal core
 * esistono ma sono no-op con un avviso ESPLICITO alla prima chiamata. I programmi con FORM/READ/
 * grafica linkano il runtime COMPLETO (UI vera).
 *
 * Elenco generato da: nm -u libgmb_runtime_min.a ∩ nm -gU gmb_runtime_ui_obj (66 simboli).
 */
#include "gmb_runtime.h"
#include "gmb_tui.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_sdl_console.h"
#include "gmb_gfx_input.h"
#include "gmb_gfx_present.h"
#include "gmb_console_extra.h"
#include "gmb_tui_tbrowse.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void gmb_ui_null_warn(const char* fn)
{
    static int warned;
    if (!warned) {
        warned = 1;
        fprintf(stderr, "[ui] build testuale: API UI no-op (prima chiamata: %s).\n", fn);
    }
}

void gmb_control_apply_style(gmb_control_t* c, const char* spec) { gmb_ui_null_warn("gmb_control_apply_style"); }
void gmb_gfx_cls(int color) { gmb_ui_null_warn("gmb_gfx_cls"); }
void gmb_gfx_draw_text(const char* text, int x, int y, int align, int color) { gmb_ui_null_warn("gmb_gfx_draw_text"); }
void gmb_gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba) { gmb_ui_null_warn("gmb_gfx_fill_rounded_rect"); }
int gmb_gfx_present_on_main_thread(void) { return 0; }
void gmb_gfx_present_start(void) { gmb_ui_null_warn("gmb_gfx_present_start"); }
void gmb_gfx_present_stop(void) { gmb_ui_null_warn("gmb_gfx_present_stop"); }
void gmb_gfx_present_tick(void) { gmb_ui_null_warn("gmb_gfx_present_tick"); }
float gmb_gfx_render_scale(void) { return 1.0f; }
int gmb_gfx_screen_height(void) { return 25; }
int gmb_gfx_screen_on(void) { return 0; }
int gmb_gfx_screen_width(void) { return 80; }
void gmb_gfx_stroke_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba) { gmb_ui_null_warn("gmb_gfx_stroke_rounded_rect"); }
int gmb_iconsole_present_if_active(void) { return 0; }
int gmb_iconsole_sdl_active(void) { return 0; }
int gmb_iconsole_try_write(const char* text) { return 0; }
void gmb_label_set_text(gmb_label_t* l, const char* text) { gmb_ui_null_warn("gmb_label_set_text"); }
void gmb_text_input_set_text(gmb_text_input_t* t, const char* text) { gmb_ui_null_warn("gmb_text_input_set_text"); }
gmb_control_t* gmb_tui_add_alertbox(int x, int y, int w, int h, const char* text) { gmb_ui_null_warn("gmb_tui_add_alertbox"); return NULL; }
gmb_control_t* gmb_tui_add_button(int x, int y, int w, const char* label) { gmb_ui_null_warn("gmb_tui_add_button"); return NULL; }
gmb_control_t* gmb_tui_add_checkbox(int x, int y, int w, const char* label) { gmb_ui_null_warn("gmb_tui_add_checkbox"); return NULL; }
gmb_control_t* gmb_tui_add_colorpicker(int x, int y, int w, int h) { gmb_ui_null_warn("gmb_tui_add_colorpicker"); return NULL; }
gmb_control_t* gmb_tui_add_combobox(int x, int y, int w, const char* items_csv) { gmb_ui_null_warn("gmb_tui_add_combobox"); return NULL; }
gmb_control_t* gmb_tui_add_datepicker(int x, int y, int w) { gmb_ui_null_warn("gmb_tui_add_datepicker"); return NULL; }
gmb_control_t* gmb_tui_add_filedialog(const char* title, const char* mask) { gmb_ui_null_warn("gmb_tui_add_filedialog"); return NULL; }
gmb_control_t* gmb_tui_add_grid(int x, int y, int w, int h, int cols, const char* data_csv) { gmb_ui_null_warn("gmb_tui_add_grid"); return NULL; }
gmb_control_t* gmb_tui_add_label(int x, int y, int w, const char* text) { gmb_ui_null_warn("gmb_tui_add_label"); return NULL; }
gmb_control_t* gmb_tui_add_listbox(int x, int y, int w, int h, const char* items_csv) { gmb_ui_null_warn("gmb_tui_add_listbox"); return NULL; }
gmb_control_t* gmb_tui_add_menu(int x, int y, int w, const char* items_csv) { gmb_ui_null_warn("gmb_tui_add_menu"); return NULL; }
gmb_control_t* gmb_tui_add_numericupdown(int x, int y, int w, int mn, int mx, int val) { gmb_ui_null_warn("gmb_tui_add_numericupdown"); return NULL; }
gmb_control_t* gmb_tui_add_radiogroup(int x, int y, int w, int h, const char* items_csv) { gmb_ui_null_warn("gmb_tui_add_radiogroup"); return NULL; }
gmb_control_t* gmb_tui_add_scrollbar(int x, int y, int len, int mn, int mx, int val) { gmb_ui_null_warn("gmb_tui_add_scrollbar"); return NULL; }
gmb_control_t* gmb_tui_add_slider(int x, int y, int w, int mn, int mx, int val) { gmb_ui_null_warn("gmb_tui_add_slider"); return NULL; }
gmb_control_t* gmb_tui_add_spinner(int x, int y, int w, const char* preset) { gmb_ui_null_warn("gmb_tui_add_spinner"); return NULL; }
gmb_control_t* gmb_tui_add_starrating(int x, int y, int w, int mx, int val) { gmb_ui_null_warn("gmb_tui_add_starrating"); return NULL; }
gmb_control_t* gmb_tui_add_statusbar(int x, int y, int w, const char* text) { gmb_ui_null_warn("gmb_tui_add_statusbar"); return NULL; }
gmb_control_t* gmb_tui_add_tab(int x, int y, int w, const char* items_csv) { gmb_ui_null_warn("gmb_tui_add_tab"); return NULL; }
gmb_control_t* gmb_tui_add_textinput(int x, int y, int w, const char* initial) { gmb_ui_null_warn("gmb_tui_add_textinput"); return NULL; }
gmb_control_t* gmb_tui_add_toggle(int x, int y, int w, const char* caption) { gmb_ui_null_warn("gmb_tui_add_toggle"); return NULL; }
gmb_control_t* gmb_tui_add_treeview(int x, int y, int w, int h, const char* items_csv) { gmb_ui_null_warn("gmb_tui_add_treeview"); return NULL; }
void gmb_tui_bind_value(int handle, double* out) { gmb_ui_null_warn("gmb_tui_bind_value"); }
double gmb_tui_changed(int handle) { gmb_ui_null_warn("gmb_tui_changed"); return 0.0; }
double gmb_tui_clicked(int handle) { gmb_ui_null_warn("gmb_tui_clicked"); return 0.0; }
gmb_control_t* gmb_tui_control_at(int i) { gmb_ui_null_warn("gmb_tui_control_at"); return NULL; }
int gmb_tui_control_count(void) { return 0; }
gmb_control_t* gmb_tui_control_find_by_name(const char* name) { gmb_ui_null_warn("gmb_tui_control_find_by_name"); return NULL; }
void gmb_tui_control_focus(int i) { gmb_ui_null_warn("gmb_tui_control_focus"); }
const char* gmb_tui_control_text_ptr(int i) { gmb_ui_null_warn("gmb_tui_control_text_ptr"); return NULL; }
long long gmb_tui_control_value(int i) { gmb_ui_null_warn("gmb_tui_control_value"); return 0; }
void gmb_tui_dispatch_mouse_px(int type, int x, int y, int button, int wheel) { gmb_ui_null_warn("gmb_tui_dispatch_mouse_px"); }
void gmb_tui_draw_px_all(void) { gmb_ui_null_warn("gmb_tui_draw_px_all"); }
void gmb_tui_fire(int handle, int ev) { gmb_ui_null_warn("gmb_tui_fire"); }
int gmb_tui_global_typeahead_read(void) { return 0; }
double gmb_tui_gotfocused(int handle) { gmb_ui_null_warn("gmb_tui_gotfocused"); return 0.0; }
void gmb_tui_inject_keys(const char* s) { gmb_ui_null_warn("gmb_tui_inject_keys"); }
double gmb_tui_lostfocused(int handle) { gmb_ui_null_warn("gmb_tui_lostfocused"); return 0.0; }
long long gmb_tui_menu_poll(void) { return 0; }
void gmb_tui_menu_set(int idx, const char* spec) { gmb_ui_null_warn("gmb_tui_menu_set"); }
gmb_screen_buffer_t* gmb_tui_modal_buffer(void) { gmb_ui_null_warn("gmb_tui_modal_buffer"); return NULL; }
void gmb_tui_present_gfx(void) { gmb_ui_null_warn("gmb_tui_present_gfx"); }
void gmb_tui_push_gfx_key(int k) { gmb_ui_null_warn("gmb_tui_push_gfx_key"); }
int gmb_tui_run_control_modal(gmb_control_t* ctrl) { gmb_ui_null_warn("gmb_tui_run_control_modal"); return 0; }
int gmb_tui_sdl_active(void) { return 0; }
void gmb_tui_set_event_id(int handle, int ev, gmb_tui_cb cb) { gmb_ui_null_warn("gmb_tui_set_event_id"); }

/* --- seconda tornata (simboli emersi dopo i primi stub) --- */
void gmb_alert_box_centered(gmb_alert_box_t* a, int term_w, int term_h, const char* title, const char* message, int width, int align) { gmb_ui_null_warn("gmb_alert_box_centered"); }
int gmb_alert_box_show(gmb_alert_box_t* a) { gmb_ui_null_warn("gmb_alert_box_show"); return 0; }
void gmb_alert_box_with_border(gmb_alert_box_t* a, int b) { gmb_ui_null_warn("gmb_alert_box_with_border"); }
void gmb_alert_box_with_buttons(gmb_alert_box_t* a, const char* b0, const char* b1, const char* b2) { gmb_ui_null_warn("gmb_alert_box_with_buttons"); }
void gmb_alert_box_with_colors_rgb(gmb_alert_box_t* a, int has_fg, int fr, int fg, int fb, int has_bg, int br, int bg, int bb) { gmb_ui_null_warn("gmb_alert_box_with_colors_rgb"); }
int gmb_browse_csv(const gmb_browse_t* b, char* out, int cap) { gmb_ui_null_warn("gmb_browse_csv"); return 0; }
const char* gmb_browse_filter(const gmb_browse_t* b) { gmb_ui_null_warn("gmb_browse_filter"); return NULL; }
void gmb_browse_set_filter(gmb_browse_t* b, const char* text) { gmb_ui_null_warn("gmb_browse_set_filter"); }
void gmb_browse_set_sortkeys(gmb_browse_t* b, const char* spec) { gmb_ui_null_warn("gmb_browse_set_sortkeys"); }
void gmb_browse_sort(gmb_browse_t* b, int col, int desc) { gmb_ui_null_warn("gmb_browse_sort"); }
int gmb_browse_sort_col(const gmb_browse_t* b) { gmb_ui_null_warn("gmb_browse_sort_col"); return 0; }
int gmb_browse_sort_dir(const gmb_browse_t* b) { gmb_ui_null_warn("gmb_browse_sort_dir"); return 0; }
int gmb_browse_sortkeys(const gmb_browse_t* b, char* out, int cap) { gmb_ui_null_warn("gmb_browse_sortkeys"); return 0; }
gmb_clipper_state_t* gmb_clipper_global_state(void) { gmb_ui_null_warn("gmb_clipper_global_state"); return NULL; }
gmb_rect_t gmb_managed_window_content_rect(const gmb_managed_window_t* w) { gmb_ui_null_warn("gmb_managed_window_content_rect"); gmb_rect_t z; memset(&z,0,sizeof z); return z; }
void gmb_spinner_set_preset(gmb_spinner_t* s, const char* name) { gmb_ui_null_warn("gmb_spinner_set_preset"); }
gmb_widget_style_t gmb_widget_style_current(void) { gmb_ui_null_warn("gmb_widget_style_current"); gmb_widget_style_t z; memset(&z,0,sizeof z); return z; }
int gmb_window_manager_focused(gmb_window_manager_t* m) { gmb_ui_null_warn("gmb_window_manager_focused"); return 0; }
gmb_managed_window_t* gmb_window_manager_get(gmb_window_manager_t* m, int id) { gmb_ui_null_warn("gmb_window_manager_get"); return NULL; }
gmb_window_manager_t* gmb_window_manager_static(void) { gmb_ui_null_warn("gmb_window_manager_static"); return NULL; }
