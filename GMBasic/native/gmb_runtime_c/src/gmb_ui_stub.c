/* gmb_ui_stub.c — default HEADLESS (weak) degli entry point UI usati dal core.
 *
 * Eseguibili-C-Minimali (step 4 del piano): `gmb_console.c` (percorso PRINT) deve poter
 * funzionare senza trascinare gfx/iconsole/tui/SDL. Le chiamate restano invariate; se i
 * moduli UI NON sono linkati, il linker risolve qui (no-op). Se sono linkati (programma
 * grafico/TUI: il boot condizionale li tira in causa), le definizioni FORTI negli oggetti
 * gfx/iconsole/tui sostituiscono quelle debolissime di questo oggetto.
 *
 * Le firme DEVONO combaciare con gli header (il linker non controlla i tipi): qui si
 * includono gli stessi header delle implementazioni reali.
 */
#include <stddef.h>
#include "gmb_graphics.h"
#include "gmb_gfx_input.h"
#include "gmb_gfx_present.h"
#include "gmb_iconsole.h"
#include "gmb_tui.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"

#define GMB_WEAK __attribute__((weak))

GMB_WEAK void gmb_gfx_init(void) { }
GMB_WEAK int  gmb_gfx_screen_on(void) { return 0; }
GMB_WEAK void gmb_gfx_color(int fg, int bg) { (void)fg; (void)bg; }
GMB_WEAK void gmb_gfx_palette(int index, int r, int g, int b) { (void)index; (void)r; (void)g; (void)b; }
GMB_WEAK void gmb_gfx_cls(int color) { (void)color; }
GMB_WEAK void gmb_gfx_font(const char* path, int size) { (void)path; (void)size; }
GMB_WEAK void gmb_gfx_locate(int x, int y) { (void)x; (void)y; }
GMB_WEAK void gmb_gfx_print(const char* text) { (void)text; }
GMB_WEAK void gmb_gfx_present_tick(void) { }
GMB_WEAK int  gmb_gfx_input_poll_key(char* out, int maxlen) { (void)out; (void)maxlen; return 0; }
GMB_WEAK int  gmb_gfx_present_should_quit(void) { return 0; }
GMB_WEAK int  gmb_gfx_present_video_is_real(void) { return 0; }

GMB_WEAK int  gmb_iconsole_try_write(const char* text) { (void)text; return 0; }
GMB_WEAK int  gmb_iconsole_sdl_active(void) { return 0; }

GMB_WEAK int  gmb_tui_run_control_modal(gmb_control_t* ctrl) { (void)ctrl; return 0; }
GMB_WEAK int  gmb_tui_control_count(void) { return 0; }
GMB_WEAK gmb_control_t* gmb_tui_control_at(int i) { (void)i; return NULL; }
GMB_WEAK long long gmb_tui_control_value(int i) { (void)i; return 0; }
GMB_WEAK const char* gmb_tui_control_text_ptr(int i) { (void)i; return ""; }
GMB_WEAK int  gmb_tui_sdl_active(void) { return 0; }
GMB_WEAK void gmb_tui_menu_set(int idx, const char* spec) { (void)idx; (void)spec; }
GMB_WEAK long long gmb_tui_menu_poll(void) { return 0; }
GMB_WEAK gmb_control_t* gmb_tui_add_toggle(int x, int y, int w, const char* caption) { (void)x; (void)y; (void)w; (void)caption; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_menu(int x, int y, int w, const char* items_csv) { (void)x; (void)y; (void)w; (void)items_csv; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_spinner(int x, int y, int w, const char* preset) { (void)x; (void)y; (void)w; (void)preset; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_slider(int x, int y, int w, int mn, int mx, int val) { (void)x; (void)y; (void)w; (void)mn; (void)mx; (void)val; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_label(int x, int y, int w, const char* text) { (void)x; (void)y; (void)w; (void)text; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_textinput(int x, int y, int w, const char* initial) { (void)x; (void)y; (void)w; (void)initial; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_alertbox(int x, int y, int w, int h, const char* text) { (void)x; (void)y; (void)w; (void)h; (void)text; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_datepicker(int x, int y, int w) { (void)x; (void)y; (void)w; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_colorpicker(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_filedialog(const char* title, const char* mask) { (void)title; (void)mask; return NULL; }
GMB_WEAK void gmb_tui_inject_keys(const char* s) { (void)s; }
GMB_WEAK gmb_screen_buffer_t* gmb_tui_modal_buffer(void) { return NULL; }
GMB_WEAK int  gmb_tui_global_typeahead_read(void) { return 0; }
GMB_WEAK void gmb_tui_present_gfx(void) { }

/* ── Entry point UI referenziati da gmb_form.c / gmb_c99gpm.c / gmb_screen_config.c ──
 * (seconda ondata: il core form/GPM usava altre funzioni gfx/tui/iconsole). */
GMB_WEAK void gmb_gfx_draw_text(const char* text, int x, int y, int align, int color) { (void)text; (void)x; (void)y; (void)align; (void)color; }
GMB_WEAK void gmb_gfx_fill_circle(int cx, int cy, int d, unsigned int rgba) { (void)cx; (void)cy; (void)d; (void)rgba; }
GMB_WEAK void gmb_gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba) { (void)x; (void)y; (void)w; (void)h; (void)radius; (void)rgba; }
GMB_WEAK const char* gmb_gfx_font_path(void) { return ""; }
GMB_WEAK int  gmb_gfx_font_size(void) { return 0; }
GMB_WEAK void gmb_gfx_gradient_rect(int x, int y, int w, int h, unsigned int cstart, unsigned int cend, int vertical) { (void)x; (void)y; (void)w; (void)h; (void)cstart; (void)cend; (void)vertical; }
GMB_WEAK int  gmb_gfx_input_poll_mouse(int* type, int* x, int* y, int* button, int* wheel) { (void)type; (void)x; (void)y; (void)button; (void)wheel; return 0; }
GMB_WEAK void gmb_gfx_line(int x1, int y1, int x2, int y2, int color, const char* style) { (void)x1; (void)y1; (void)x2; (void)y2; (void)color; (void)style; }
GMB_WEAK float gmb_gfx_render_scale(void) { return 1.0f; }
GMB_WEAK void gmb_gfx_screen(int mode, int width, int height, int bpp) { (void)mode; (void)width; (void)height; (void)bpp; }
GMB_WEAK int  gmb_gfx_screen_height(void) { return 0; }
GMB_WEAK int  gmb_gfx_screen_width(void) { return 0; }
GMB_WEAK void gmb_gfx_stroke_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba) { (void)x; (void)y; (void)w; (void)h; (void)radius; (void)rgba; }
GMB_WEAK int  gmb_gfx_text_end_x(const char* text, int x, int y) { (void)text; (void)x; (void)y; return 0; }
GMB_WEAK int  gmb_gfx_text_height(void) { return 0; }
GMB_WEAK int  gmb_gfx_text_width(const char* text) { (void)text; return 0; }
GMB_WEAK int  gmb_gfx_present_on_main_thread(void) { return 0; }
GMB_WEAK void gmb_gfx_present_start(void) { }
GMB_WEAK void gmb_gfx_present_stop(void) { }

GMB_WEAK gmb_control_t* gmb_tui_add_button(int x, int y, int w, const char* label) { (void)x; (void)y; (void)w; (void)label; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_checkbox(int x, int y, int w, const char* label) { (void)x; (void)y; (void)w; (void)label; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_combobox(int x, int y, int w, const char* items_csv) { (void)x; (void)y; (void)w; (void)items_csv; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_grid(int x, int y, int w, int h, int cols, const char* data_csv) { (void)x; (void)y; (void)w; (void)h; (void)cols; (void)data_csv; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_listbox(int x, int y, int w, int h, const char* items_csv) { (void)x; (void)y; (void)w; (void)h; (void)items_csv; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_radiogroup(int x, int y, int w, int h, const char* items_csv) { (void)x; (void)y; (void)w; (void)h; (void)items_csv; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_statusbar(int x, int y, int w, const char* text) { (void)x; (void)y; (void)w; (void)text; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_tab(int x, int y, int w, const char* items_csv) { (void)x; (void)y; (void)w; (void)items_csv; return NULL; }
GMB_WEAK gmb_control_t* gmb_tui_add_treeview(int x, int y, int w, int h, const char* items_csv) { (void)x; (void)y; (void)w; (void)h; (void)items_csv; return NULL; }
GMB_WEAK void gmb_tui_bind_value(int handle, double* out) { (void)handle; (void)out; }
GMB_WEAK double gmb_tui_changed(int handle) { (void)handle; return 0; }
GMB_WEAK double gmb_tui_clicked(int handle) { (void)handle; return 0; }
GMB_WEAK gmb_control_t* gmb_tui_control_find_by_name(const char* name) { (void)name; return NULL; }
GMB_WEAK void gmb_tui_dispatch_mouse_px(int type, int x, int y, int button, int wheel) { (void)type; (void)x; (void)y; (void)button; (void)wheel; }
GMB_WEAK void gmb_tui_draw_px_all(void) { }
GMB_WEAK void gmb_tui_fire(int handle, int ev) { (void)handle; (void)ev; }
GMB_WEAK double gmb_tui_gotfocused(int handle) { (void)handle; return 0; }
GMB_WEAK double gmb_tui_lostfocused(int handle) { (void)handle; return 0; }
GMB_WEAK void gmb_tui_push_gfx_key(int k) { (void)k; }
GMB_WEAK void gmb_tui_screen_flush(void) { }
GMB_WEAK void gmb_tui_set_event_id(int handle, int ev, gmb_tui_cb cb) { (void)handle; (void)ev; (void)cb; }

GMB_WEAK int  gmb_iconsole_present_if_active(void) { return 0; }

GMB_WEAK void gmb_control_apply_style(gmb_control_t* c, const char* spec) { (void)c; (void)spec; }
