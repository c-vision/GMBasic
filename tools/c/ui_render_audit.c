/* ui_render_audit.c — INVENTARIO: nessun controllo deve finire nel percorso a CELLE
 * quando c'e' una pagina grafica.
 *
 * Regola verificata (una sola, quella che conta):
 *   - i controlli FORM sono disegnati da draw_one_widget() = renderer pixel: OK;
 *   - i controlli creati nel registry TUI DEVONO avere draw_px_fn != NULL.
 * Se un controllo finisce nel registry TUI senza draw_px_fn, in grafica verrebbe
 * blittato a celle: e' esattamente il "TUI copiato" che non deve esistere.
 *
 * Uscita: 0 = tutto ok, N = controlli senza renderer pixel.
 * Build/run: tools/sh/ui_render_audit.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gmb_runtime.h"
#include "gmb_graphics.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_tui_datatypes.h"
#include "gmb_form.h"

static int fails = 0, total = 0;

int gmb_form_last_is_tui(void);

/* Controllo creato da una factory (handle intero). FORM = pixel per costruzione;
 * TUI = deve avere draw_px_fn. */
static void chk(const char* name, int handle)
{
    total++;
    int ok = 0;
    if (handle < 0) { printf("%-18s MANCA (factory fallita)\n", name); fails++; return; }
    if (gmb_form_last_is_tui())
    {
        gmb_control_t* c = gmb_tui_control_at(handle);
        ok = (c && c->draw_px_fn != NULL);
        printf("%-18s %s (TUI)\n", name, ok ? "PIXEL" : "MANCA RENDERER PIXEL");
    }
    else { ok = 1; printf("%-18s PIXEL (form)\n", name); }
    if (!ok) fails++;
}

/* Controllo creato direttamente nel registry TUI. */
static void chk_tui(const char* name, gmb_control_t* c)
{
    total++;
    int ok = (c && c->draw_px_fn != NULL);
    printf("%-18s %s\n", name, ok ? "PIXEL" : "MANCA RENDERER PIXEL");
    if (!ok) fails++;
}

int main(void)
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    gmb_gfx_screen(12, 1280, 940, 32);
    gmb_ui_set_theme_name("glass");
    setvbuf(stdout, NULL, _IONBF, 0);
    double v = 0; gmb_string_t s = gmb_string_alloc("x");

    printf("== controlli in GRAFICA: renderer pixel ==\n");

    chk("PANEL",         gmb_form_add_panel(20, 20, 200, 100, "p"));
    chk("BUTTON",        gmb_form_add_button(20, 140, 160, 34, "ok", NULL));
    chk("CHECKBOX",      gmb_control_add_checkbox(20, 200, 160, "c", &v));
    chk("TOGGLE",        gmb_control_add_toggle(20, 240, 160, "t", &v));
    chk("RADIO",         gmb_control_add_radio(20, 280, 160, "a|b", &v));
    chk("SLIDER",        gmb_form_add_track(20, 340, 160, 0, 100, &v));
    chk("PROGRESS",      gmb_form_add_progress(20, 380, 160, &v));
    chk("NUMERICUPDOWN", gmb_control_add_numericupdown(20, 420, 160, 0, 10, 1, &v));
    chk("TAB",           gmb_control_add_tab(20, 460, 160, "a|b", &v));
    chk("LISTBOX",       gmb_control_add_listbox(20, 500, 160, 80, "a|b", &v));
    chk("COMBOBOX",      gmb_form_add_dropdown(20, 600, 160, "a|b", &v));
    chk("GRID",          gmb_form_add_grid(300, 20, 200, 100, "a|b", "1|2", &v));
    chk("TREEVIEW",      gmb_control_add_tree(300, 140, 200, 100, "a/b", &v));
    chk("TEXTBOX",       gmb_form_add_textbox(300, 260, 200, 34, &s));
    chk("COLORPICKER",   gmb_control_add_colorpicker(300, 310, 200, "", &v));
    chk("STATUSBAR",     gmb_form_add_statusbar(300, 380, 200, "st"));
    chk("SCROLLBAR",     gmb_control_add_scrollbar(300, 420, 200, 0, 100, 0, &v));
    chk("STARRATING",    gmb_control_add_starrating(300, 460, 200, 5, 3, &v));
    chk("QR",            gmb_form_add_qr(300, 500, 90, 90, "x"));

    /* registry TUI: ognuno DEVE avere il renderer pixel */
    { static gmb_vec_ds_t ds; static gmb_browse_datasource_t dsrc; gmb_rect_t r = { 520, 20, 240, 120 };
      gmb_vec_ds_make(&ds, 2, 1); gmb_vec_ds_set(&ds, 0, 0, "a");
      gmb_vec_ds_datasource(&ds, &dsrc);
      gmb_browse_t* b = gmb_browse_new(&r, 0, NULL, &dsrc);
      gmb_browse_column_t cc; memset(&cc, 0, sizeof cc);
      snprintf(cc.header, sizeof cc.header, "H"); cc.width = 10; cc.visible = true;
      gmb_browse_add_column(b, &cc); chk_tui("TBROWSE", &b->ctrl); }
    { static gmb_data_list_t d; gmb_data_list_init(&d, 520, 160, 240, 80);
      const char* it[] = { "a", "b" }; gmb_data_list_set_items(&d, it, 2); chk_tui("DATALIST", &d.ctrl); }
    { static gmb_property_grid_t p; gmb_property_grid_init(&p, 520, 260, 240, 60);
      gmb_property_grid_add(&p, "k", "v"); chk_tui("PROPERTYGRID", &p.ctrl); }
    { static gmb_scroll_view_t sv; gmb_scroll_view_init(&sv, 520, 340, 240, 80);
      const char* it[] = { "a", "b" }; gmb_scroll_view_set_content(&sv, it, 2); chk_tui("SCROLLVIEW", &sv.ctrl); }
    chk_tui("LOGVIEW", gmb_tui_add_logview(800, 20, 240, 80, "a|b"));
    chk_tui("HEXVIEW", gmb_tui_add_hexview(800, 120, 240, 80, "474D"));
    chk_tui("CODEAREA", gmb_tui_add_codearea(800, 220, 240, 80, "x=1|y=2"));
    chk_tui("DASHBOARD", gmb_tui_add_dashboard(800, 320, 240, 80, "CPU|50|100|%"));
    chk_tui("CHART", gmb_tui_add_chart(1060, 20, 200, 120, "1,1;2,2"));
    chk_tui("CANDLESTICK", gmb_tui_add_candlestick(1060, 160, 200, 100, "1,2,1,2;2,3,2,3"));
    chk_tui("FILEEXPLORER", gmb_tui_add_fileexplorer(800, 420, 240, 100, "."));
    { static gmb_context_menu_t cm; gmb_context_menu_init(&cm, 520, 440, 200, 3);
      const char* it[] = { "a", "b" }; gmb_context_menu_set_items(&cm, it, 2); chk_tui("CONTEXTMENU", &cm.ctrl); }
    { static gmb_menu_t mn; gmb_menu_init(&mn, 800, 520, 200); gmb_menu_add_item(&mn, "File"); chk_tui("MENU", &mn.ctrl); }
    { static gmb_date_picker_t dp; gmb_date_picker_init(&dp, 800, 560, 160); chk_tui("DATEPICKER", &dp.ctrl); }
    { static gmb_alert_box_t ab; gmb_alert_box_init(&ab, 800, 620, 200, 90); chk_tui("ALERTBOX", &ab.ctrl); }
    { static gmb_confirmation_box_t cb; gmb_confirmation_box_init(&cb, 1020, 620, 200, 90); chk_tui("CONFIRMATIONBOX", &cb.ctrl); }
    { static gmb_file_dialog_t fd; gmb_file_dialog_init(&fd, 20, 700, 0, 0); chk_tui("FILEDIALOG", &fd.ctrl); }
    { static gmb_printer_dialog_t pd; gmb_printer_dialog_init(&pd); chk_tui("PRINTERDIALOG", &pd.ctrl); }
    chk_tui("FUNCTIONKEYBAR", gmb_tui_add_fkeybar(20, 560, 400, "F1|F2"));
    { static gmb_splitter_t sp; gmb_splitter_init(&sp, 20, 600, 200, 20, 0); chk_tui("SPLITTER", &sp.ctrl); }
    { static gmb_label_t lb; gmb_label_init(&lb, 20, 640, 200, "testo"); chk_tui("LABEL", &lb.ctrl); }

    printf("\nTOTALE: %d controlli, %d senza renderer pixel\n", total, fails);
    return fails;
}
