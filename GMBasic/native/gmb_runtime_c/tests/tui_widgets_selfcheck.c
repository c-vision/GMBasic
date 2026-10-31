/* ============================================================================
 * tui_widgets_selfcheck.c — parita' TUI C vs C# (Control/ApplyCase, Button,
 * TextInput, ComboBox box/popup/tasti/click, FocusManager.HandleEvent).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#ifdef GMB_GFX_SDL_PRESENT
#include "gmb_gfx_input.h"
#endif
#include "gmb_tui_tbrowse.h"
#include "gmb_tui_windowstmt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
                              else printf("ok: %s\n", msg); fflush(stdout); } while (0)

typedef struct { int x, y; uint32_t ch; int puts; } rec_sink_t;
static void rec_put(void* u, int x, int y, uint32_t ch, gmb_cell_color_t fg, gmb_cell_color_t bg)
{ rec_sink_t* r = (rec_sink_t*)u; if (r->puts == 0) { r->x = x; r->y = y; r->ch = ch; } r->puts++; (void)fg; (void)bg; }
static void rec_fill(void* u, int x, int y, int w, int h, gmb_cell_color_t bg)
{ (void)u; (void)x; (void)y; (void)w; (void)h; (void)bg; }

static int g_click_n = 0;
static void click_counter(void* ctx) { (*(int*)ctx)++; }

static uint32_t cell_ch(gmb_screen_buffer_t* b, int x, int y)
{
    return gmb_screen_cells(b)[y * gmb_screen_width(b) + x].character;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    gmb_surface_set_autoattach(0);
    /* 1) ApplyCase (Control.Uppercase) */
    gmb_control_t ctl;
    gmb_control_init(&ctl, "t", 0, 0, 10, 1, NULL, NULL);
    char out[32];
    ctl.uppercase = false;
    gmb_control_apply_case(&ctl, "aBc", out, (int)sizeof(out));
    CHECK(strcmp(out, "aBc") == 0, "apply_case lowercase off");
    ctl.uppercase = true;
    gmb_control_apply_case(&ctl, "aBc", out, (int)sizeof(out));
    CHECK(strcmp(out, "ABC") == 0, "apply_case uppercase on");

    /* 2) Button: draw applica Uppercase al RenderLine */
    gmb_button_t btn; memset(&btn, 0, sizeof(btn));
    gmb_button_init(&btn, 0, 0, 10, "ok");
    btn.ctrl.uppercase = true;
    gmb_screen_buffer_t* buf = gmb_screen_new(20, 4);
    gmb_screen_clear(buf, (gmb_cell_color_t){0, 0, 0}, (gmb_cell_color_t){0, 0, 0});
    gmb_control_draw(&btn.ctrl, buf);
    CHECK(cell_ch(buf, 0, 0) == '[', "button draw '['");
    /* "ok" centrato in width 10 -> uppercase "OK" */
    char line[32];
    gmb_button_render_line(&btn, line, (int)sizeof(line));
    CHECK(strstr(line, "ok") != NULL, "button render_line raw");

    /* 3) TextInput: Uppercase sul testo mostrato */
    gmb_text_input_t ti; memset(&ti, 0, sizeof(ti));
    gmb_text_input_init(&ti, 0, 1, 10);
    ti.ctrl.uppercase = true;
    gmb_text_input_set_text(&ti, "ab");
    gmb_screen_clear(buf, (gmb_cell_color_t){0, 0, 0}, (gmb_cell_color_t){0, 0, 0});
    gmb_control_draw(&ti.ctrl, buf);
    CHECK(cell_ch(buf, 0, 1) == 'A' && cell_ch(buf, 1, 1) == 'B', "textinput uppercase shown");

    /* 4) ComboBox: box, popup, tasti, click */
    gmb_combobox_t cb; memset(&cb, 0, sizeof(cb));
    gmb_combobox_init(&cb, 0, 0, 10);
    static const char* items[] = { "alpha", "beta", "gamma" };
    gmb_combobox_set_items(&cb, items, 3);
    CHECK(cb.selected == 0 && cb.drop_down_height == 5, "combobox defaults (selected=0, h=5)");
    gmb_screen_clear(buf, (gmb_cell_color_t){0, 0, 0}, (gmb_cell_color_t){0, 0, 0});
    gmb_control_draw(&cb.ctrl, buf);
    CHECK(cell_ch(buf, 0, 0) == 'a', "combobox box shows selected");
    CHECK(cell_ch(buf, 9, 0) == 0x25BC, "combobox arrow at X+Width-1");
    /* Apre con ENTER; highlight = Selected */
    CHECK(gmb_control_handle_key(&cb.ctrl, GMB_K_ENTER), "combobox ENTER opens");
    CHECK(cb.open && cb.highlight == 0, "combobox open+highlight=0");
    CHECK(gmb_control_handle_key(&cb.ctrl, GMB_K_DOWN), "combobox DOWN");
    CHECK(cb.highlight == 1, "combobox highlight=1 after DOWN");
    gmb_screen_clear(buf, (gmb_cell_color_t){0, 0, 0}, (gmb_cell_color_t){0, 0, 0});
    gmb_control_draw(&cb.ctrl, buf);
    CHECK(cell_ch(buf, 0, 1) == 'a', "popup row0=alpha");
    CHECK(cell_ch(buf, 0, 2) == 'b', "popup row1=beta");
    /* ESC chiude; riapri e ENTER seleziona highlight */
    CHECK(gmb_control_handle_key(&cb.ctrl, GMB_K_ESC) && !cb.open, "combobox ESC closes");
    gmb_control_handle_key(&cb.ctrl, GMB_K_ENTER);
    gmb_control_handle_key(&cb.ctrl, GMB_K_DOWN);
    gmb_control_handle_key(&cb.ctrl, GMB_K_ENTER);
    CHECK(!cb.open && cb.selected == 1, "combobox ENTER selects highlight");
    CHECK(strcmp(gmb_combobox_selected_value(&cb), "beta") == 0, "combobox selected value");
    /* click open-aware: apre ad (0,0), poi seleziona item a y+2 */
    gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
    ma.kind = GMB_MA_CLICK; ma.x = 0; ma.y = 0;
    gmb_control_handle_mouse(&cb.ctrl, &ma);
    CHECK(cb.open, "combobox click opens");
    ma.y = 2;
    gmb_control_handle_mouse(&cb.ctrl, &ma);
    CHECK(!cb.open && cb.selected == 1, "combobox click selects item");

    /* 5) FocusManager.HandleEvent: Tab/BackTab e mouse */
    gmb_focus_manager_t fm;
    gmb_focus_manager_init(&fm);
    gmb_focus_manager_register_rect(&fm, "a", (gmb_rect_t){0, 0, 5, 1});
    gmb_focus_manager_register_rect(&fm, "b", (gmb_rect_t){0, 2, 5, 1});
    gmb_ui_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = GMB_UIEVENT_KEY;
    ev.key = GMB_K_TAB;
    const char* id = gmb_focus_manager_handle_event(&fm, &ev);
    CHECK(id && strcmp(id, "b") == 0, "handle_event TAB -> next");
    ev.key = GMB_K_BTAB;
    id = gmb_focus_manager_handle_event(&fm, &ev);
    CHECK(id && strcmp(id, "a") == 0, "handle_event BTAB -> prev");
    ev.type = GMB_UIEVENT_MOUSE; ev.mouse.kind = GMB_MA_CLICK; ev.mouse.x = 1; ev.mouse.y = 2;
    id = gmb_focus_manager_handle_event(&fm, &ev);
    CHECK(id && strcmp(id, "b") == 0, "handle_event click focuses");

    /* 6) ListBox: rotella mouse + click con EnsureVisible */
    gmb_listbox_t lb; memset(&lb, 0, sizeof(lb));
    gmb_listbox_init(&lb, 0, 0, 10, 3);
    static const char* items2[] = { "a","b","c","d","e","f","g","h","i","j" };
    gmb_listbox_set_items(&lb, items2, 10);
    lb.selected = 0;
    gmb_mouse_action_t m2; memset(&m2, 0, sizeof(m2));
    m2.kind = GMB_MA_SCROLLDOWN; gmb_control_handle_mouse(&lb.ctrl, &m2);
    CHECK(lb.selected == 1, "listbox wheel down");
    m2.kind = GMB_MA_SCROLLUP; gmb_control_handle_mouse(&lb.ctrl, &m2);
    CHECK(lb.selected == 0, "listbox wheel up");
    m2.kind = GMB_MA_CLICK; m2.x = 0; m2.y = 2;
    gmb_control_handle_mouse(&lb.ctrl, &m2);
    CHECK(lb.selected == 2, "listbox click row2");

    /* 7) Slider: tasti (incl. UP/DOWN), click/drag/release, render ─/█ */
    gmb_slider_t sl; memset(&sl, 0, sizeof(sl));
    gmb_slider_init(&sl, 0, 0, 11);
    gmb_slider_set_range(&sl, 0, 100);
    gmb_control_handle_key(&sl.ctrl, GMB_K_RIGHT); CHECK(sl.value == 1, "slider RIGHT");
    gmb_control_handle_key(&sl.ctrl, GMB_K_UP);    CHECK(sl.value == 2, "slider UP");
    gmb_control_handle_key(&sl.ctrl, GMB_K_DOWN);  CHECK(sl.value == 1, "slider DOWN");
    gmb_control_handle_key(&sl.ctrl, GMB_K_LEFT);  CHECK(sl.value == 0, "slider LEFT");
    gmb_control_handle_key(&sl.ctrl, GMB_K_END);   CHECK(sl.value == 100, "slider END");
    gmb_control_handle_key(&sl.ctrl, GMB_K_HOME);  CHECK(sl.value == 0, "slider HOME");
    gmb_mouse_action_t m3; memset(&m3, 0, sizeof(m3));
    m3.kind = GMB_MA_CLICK; m3.x = 6; m3.y = 0;
    gmb_control_handle_mouse(&sl.ctrl, &m3);
    CHECK(sl.value == 56 && sl.dragging, "slider click -> round(5/9*100)=56, dragging");
    m3.kind = GMB_MA_DRAG; m3.x = 10;
    gmb_control_handle_mouse(&sl.ctrl, &m3);
    CHECK(sl.value == 100, "slider drag -> 100");
    m3.kind = GMB_MA_RELEASE;
    gmb_control_handle_mouse(&sl.ctrl, &m3);
    CHECK(!sl.dragging, "slider release");
    gmb_slider_set_value(&sl, 0);
    char rl[64]; int rln = gmb_slider_render_line(&sl, rl, (int)sizeof(rl));
    CHECK(rl[0] == '[' && (unsigned char)rl[1] == 0xE2, "slider render starts [ then █");
    CHECK(rln == 31, "slider render len 31 (no closing ], C# trunc to Width)");

    /* 8) Tab: HOME/END/LEFT/RIGHT, click, render_line */
    gmb_tab_t tb; memset(&tb, 0, sizeof(tb));
    gmb_tab_init(&tb, 0, 0, 20);
    static const char* tabs3[] = { "one", "two", "three" };
    gmb_tab_set_tabs(&tb, tabs3, 3);
    gmb_control_handle_key(&tb.ctrl, GMB_K_END);   CHECK(tb.selected == 2, "tab END");
    gmb_control_handle_key(&tb.ctrl, GMB_K_HOME);  CHECK(tb.selected == 0, "tab HOME");
    gmb_control_handle_key(&tb.ctrl, GMB_K_RIGHT); CHECK(tb.selected == 1, "tab RIGHT");
    gmb_control_handle_key(&tb.ctrl, GMB_K_LEFT);  CHECK(tb.selected == 0, "tab LEFT");
    gmb_mouse_action_t m4; memset(&m4, 0, sizeof(m4));
    m4.kind = GMB_MA_CLICK; m4.y = 0;
    m4.x = 7; gmb_control_handle_mouse(&tb.ctrl, &m4);
    CHECK(tb.selected == 1, "tab click on 'two'");
    m4.x = 1; gmb_control_handle_mouse(&tb.ctrl, &m4);
    CHECK(tb.selected == 0, "tab click on 'one'");
    char tl2[64]; int tln = gmb_tab_render_line(&tb, tl2, (int)sizeof(tl2));
    CHECK(tln == 20 && strncmp(tl2, " one ", 5) == 0, "tab render_line padded");

    /* 9) ImageWidget: Control + half-block draw + click mapping cella->pixel */
    gmb_image_widget_t iw; memset(&iw, 0, sizeof(iw));
    gmb_image_widget_init(&iw);
    static gmb_cell_color_t ipx[4];
    ipx[0] = (gmb_cell_color_t){255,0,0}; ipx[1] = (gmb_cell_color_t){0,255,0};
    ipx[2] = (gmb_cell_color_t){0,0,255}; ipx[3] = (gmb_cell_color_t){255,255,255};
    gmb_image_widget_set_pixels(&iw, 2, 2, ipx);
    gmb_image_widget_set_bounds(&iw, 0, 3, 2, 1);
    gmb_screen_clear(buf, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_control_draw(&iw.ctrl, buf);
    CHECK(cell_ch(buf, 0, 3) == 0x2588, "imagewidget half-block drawn");
    gmb_mouse_action_t m5; memset(&m5, 0, sizeof(m5));
    m5.kind = GMB_MA_CLICK; m5.x = 1; m5.y = 3;
    CHECK(gmb_control_handle_mouse(&iw.ctrl, &m5), "imagewidget click inside");
    CHECK(gmb_image_widget_last_pixel_x(&iw) == 1 && gmb_image_widget_last_pixel_y(&iw) == 0,
          "imagewidget cell->pixel mapping");
    m5.x = 5; m5.y = 3;
    CHECK(!gmb_control_handle_mouse(&iw.ctrl, &m5), "imagewidget click outside");

    /* 10) FileExplorer: Control, nav, enter, search, draw */
    system("mkdir -p /tmp/gmb_fe_test; printf x > /tmp/gmb_fe_test/a.txt; printf x > /tmp/gmb_fe_test/b.bas");
    gmb_file_explorer_t fe; memset(&fe, 0, sizeof(fe));
    gmb_file_explorer_init(&fe, "/tmp/gmb_fe_test");
    gmb_file_explorer_set_bounds(&fe, 0, 0, 20, 4);
    CHECK(gmb_file_explorer_entry_count(&fe) >= 3, "fe entries (.. + 2 files)");
    gmb_screen_clear(buf, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_control_draw(&fe.ctrl, buf);
    CHECK(cell_ch(buf, 0, 1) == '>', "fe draw selected marker");
    gmb_control_handle_key(&fe.ctrl, GMB_K_DOWN);
    CHECK(gmb_file_explorer_selected_index(&fe) == 1, "fe DOWN");
    gmb_control_handle_key(&fe.ctrl, GMB_K_ENTER);
    CHECK(gmb_file_explorer_last_action(&fe) == 1, "fe ENTER selects file");
    CHECK(gmb_file_explorer_selected_path(&fe) && strstr(gmb_file_explorer_selected_path(&fe), "a.txt") != NULL,
          "fe selected path a.txt");
    gmb_control_handle_key(&fe.ctrl, '/');
    CHECK(gmb_file_explorer_search_mode(&fe), "fe search mode on");
    gmb_control_handle_key(&fe.ctrl, GMB_K_ESC);
    CHECK(!gmb_file_explorer_search_mode(&fe), "fe search mode off (ESC)");

    /* 11) PrinterDialog: Control, default printers, border/draw, tasti, click */
    gmb_printer_dialog_t pd; memset(&pd, 0, sizeof(pd));
    gmb_printer_dialog_init(&pd);
    CHECK(pd.printers_count == 4, "pd default printers");
    gmb_printer_dialog_set_bounds(&pd, 0, 0, 40, 8);
    gmb_screen_buffer_t* b2 = gmb_screen_new(50, 12);
    gmb_screen_clear(b2, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_control_draw(&pd.ctrl, b2);
    CHECK(gmb_screen_cells(b2)[0].character == 0x2554, "pd double border corner");
    gmb_control_handle_key(&pd.ctrl, GMB_K_TAB); CHECK(pd.btn_focus == 1, "pd TAB -> OK");
    gmb_control_handle_key(&pd.ctrl, GMB_K_TAB); CHECK(pd.btn_focus == 2, "pd TAB -> Cancel");
    gmb_control_handle_key(&pd.ctrl, GMB_K_ENTER); CHECK(pd.confirmed == false, "pd ENTER on Cancel");
    gmb_control_handle_key(&pd.ctrl, 'd'); CHECK(pd.duplex, "pd D toggles duplex");
    gmb_control_handle_key(&pd.ctrl, '+'); CHECK(pd.copies == 2, "pd + copies");
    gmb_control_handle_key(&pd.ctrl, GMB_K_DOWN); CHECK(pd.selected_index == 1, "pd DOWN");
    gmb_mouse_action_t m6; memset(&m6, 0, sizeof(m6));
    m6.kind = GMB_MA_CLICK; m6.x = 3; m6.y = 6;   /* OK button */
    CHECK(gmb_control_handle_mouse(&pd.ctrl, &m6) && pd.confirmed, "pd click OK -> confirmed");
    gmb_screen_free(b2);

    /* 12) TBrowse: Control, key nav, ApplyCase */
    gmb_vec_ds_t v; gmb_vec_ds_make(&v, 5, 2);
    gmb_vec_ds_set(&v, 0, 0, "aa"); gmb_vec_ds_set(&v, 0, 1, "bb");
    gmb_browse_datasource_t dsx; gmb_vec_ds_datasource(&v, &dsx);
    gmb_browse_column_t bcols[2]; memset(bcols, 0, sizeof(bcols));
    snprintf(bcols[0].header, sizeof(bcols[0].header), "c1"); bcols[0].width = 4; bcols[0].visible = true;
    snprintf(bcols[1].header, sizeof(bcols[1].header), "c2"); bcols[1].width = 4; bcols[1].visible = true;
    gmb_rect_t bar = { 0, 0, 20, 6 };
    gmb_browse_t* br = gmb_browse_new(&bar, 2, bcols, &dsx);
    CHECK(br && br->ctrl.width == 20 && br->ctrl.height == 6, "browse control bounds");
    int br0 = br->current_row;
    gmb_control_handle_key(&br->ctrl, GMB_K_DOWN);
    CHECK(br->current_row == br0 + 1, "browse key DOWN via control");
    br->ctrl.uppercase = true;
    gmb_screen_buffer_t* b3 = gmb_screen_new(24, 8);
    gmb_screen_clear(b3, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_control_draw(&br->ctrl, b3);
    int hasA = 0, hasa = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 24; x++) {
        uint32_t ch = gmb_screen_cells(b3)[y * 24 + x].character;
        if (ch == 'A') hasA = 1; if (ch == 'a') hasa = 1;
    }
    CHECK(hasA && !hasa, "browse ApplyCase uppercased");
    gmb_screen_free(b3);
    gmb_browse_free(br);

    /* 13) Surface abstraction: sink dirotta i widget; attach_gfx non rompe */
    gmb_button_t sb; memset(&sb, 0, sizeof(sb));
    gmb_button_init(&sb, 0, 0, 10, "ok");
    rec_sink_t rec; memset(&rec, 0, sizeof(rec));
    gmb_screen_sink_t rs; memset(&rs, 0, sizeof(rs));
    rs.user = &rec; rs.put_cell = rec_put; rs.fill = rec_fill;
    gmb_screen_set_sink(buf, &rs);
    CHECK(gmb_screen_has_sink(buf), "surface: sink attached");
    gmb_control_draw(&sb.ctrl, buf);
    CHECK(rec.puts > 0 && rec.x == 0 && rec.y == 0 && rec.ch == '[', "surface: widget drawn via sink");
    gmb_screen_detach_sink(buf);
    CHECK(!gmb_screen_has_sink(buf), "surface: sink detached");
    gmb_screen_attach_gfx(buf, 0, 0, 8, 16);
    CHECK(gmb_screen_has_sink(buf), "surface: gfx sink attached");
    gmb_control_draw(&sb.ctrl, buf);   /* non deve crashare (font/screen opzionali) */
    gmb_screen_detach_sink(buf);
    CHECK(!gmb_screen_has_sink(buf), "surface: gfx sink detached");

#ifdef GMB_GFX_SDL_PRESENT
    /* 14) Input bridge: pump gfx input -> control / event handler */
    gmb_button_t ib; memset(&ib, 0, sizeof(ib));
    gmb_button_init(&ib, 0, 0, 10, "go");
    g_click_n = 0;
    gmb_button_set_onclick(&ib, click_counter, &g_click_n);
    gmb_gfx_input_reset();
    gmb_gfx_input_push_key(" ");
    CHECK(gmb_tui_pump_control(&ib.ctrl, 8, 16) == 1 && g_click_n == 1, "pump key -> button click");
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, 3 * 8, 0, 1, 0);
    CHECK(gmb_tui_pump_control(&ib.ctrl, 8, 16) == 1 && g_click_n == 2, "pump mouse -> button click");

    gmb_ctrl_event_handler_t eh; memset(&eh, 0, sizeof(eh));
    gmb_ctrl_event_handler_init(&eh);
    gmb_button_t pb1, pb2; memset(&pb1, 0, sizeof(pb1)); memset(&pb2, 0, sizeof(pb2));
    gmb_button_init(&pb1, 0, 0, 10, "a"); gmb_button_init(&pb2, 0, 1, 10, "b");
    gmb_ctrl_event_handler_register(&eh, "b1", &pb1.ctrl);
    gmb_ctrl_event_handler_register(&eh, "b2", &pb2.ctrl);
    gmb_ctrl_event_handler_set_focus_by_id(&eh, "b1");
    gmb_gfx_input_reset();
    gmb_gfx_input_push_key("\t");
    gmb_tui_pump_event_handler(&eh, 8, 16);
    CHECK(gmb_focus_manager_current_id(&eh.focus) && strcmp(gmb_focus_manager_current_id(&eh.focus), "b2") == 0,
          "pump TAB -> focus next");
#endif

    /* 15) Chrome pixel nativo (rounded/gradient/shadow) sulla pagina gfx */
    gmb_gfx_init();
    gmb_gfx_screen(13, 64, 64, 32);
    int gw = 0, gh = 0;
    const uint32_t* gp = gmb_gfx_visible_pixels(&gw, &gh);
    gmb_gfx_clear_rect(0, 0, gw - 1, gh - 1, (int)(0x40000000u | 0x000000u));
    gmb_gfx_fill_rounded_rect(0, 0, 32, 32, 8, 0xFFFF0000u);
    CHECK(gp && gw >= 32 && gp[16 * gw + 16] == 0x00FF0000u, "chrome: rounded fill center red");
    CHECK(gp[0] != 0x00FF0000u, "chrome: rounded corner not filled (AA edge)");
    gmb_gfx_stroke_rounded_rect(0, 0, 32, 32, 8, 0xFF00FF00u);
    CHECK(gp[0 * gw + 16] == 0x0000FF00u, "chrome: stroke top edge green");
    gmb_gfx_gradient_rect(0, 40, 32, 1, 0xFF0000FFu, 0xFFFFFFFFu, 0);
    CHECK(gp[40 * gw + 0] == 0x000000FFu && gp[40 * gw + 32] == 0x00FFFFFFu, "chrome: gradient endpoints");
    gmb_gfx_drop_shadow(4, 4, 16, 16, 2, 0x80000000u, 6);

    /* 16) Panel chrome nativo su sink gfx (rounded/stroke) */
    gmb_gfx_init();
    gmb_gfx_screen(13, 128, 64, 32);
    int gwp = 0, ghp = 0;
    const uint32_t* gpp = gmb_gfx_visible_pixels(&gwp, &ghp);
    gmb_gfx_clear_rect(0, 0, gwp - 1, ghp - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* pb = gmb_screen_new(16, 8);
    gmb_screen_attach_gfx(pb, 0, 0, 8, 8);
    gmb_panel_t pan; memset(&pan, 0, sizeof(pan));
    gmb_panel_init(&pan, 0, 0, 10, 4, "");
    pan.ctrl.style.fg = (gmb_cell_color_t){255, 0, 0};
    pan.ctrl.style.has_fg = true;
    gmb_control_draw(&pan.ctrl, pb);
    CHECK(gpp[0 * gwp + 5 * 8] == 0x00FF0000u, "panel chrome: gfx stroke red at top edge");
    gmb_screen_detach_sink(pb);
    gmb_screen_free(pb);

    /* 17) Button + Grid chrome nativo su sink gfx */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwb = 0, ghb = 0;
    const uint32_t* gpb = gmb_gfx_visible_pixels(&gwb, &ghb);
    gmb_gfx_clear_rect(0, 0, gwb - 1, ghb - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* fb = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(fb, 0, 0, 8, 8);
    gmb_button_t nb; memset(&nb, 0, sizeof(nb));
    gmb_button_init(&nb, 0, 0, 10, "ok");
    nb.ctrl.style.fg = (gmb_cell_color_t){255, 0, 0}; nb.ctrl.style.has_fg = true;
    gmb_control_draw(&nb.ctrl, fb);
    CHECK(gpb[0 * gwb + 5 * 8] == 0x00FF0000u, "button chrome: gfx stroke red");
    gmb_grid_t ng; memset(&ng, 0, sizeof(ng));
    gmb_grid_init(&ng, 0, 2, 10, 3);
    gmb_grid_set_cols(&ng, 2);
    static const char* gcells[] = { "a", "b", "c", "d" };
    gmb_grid_set_cells(&ng, gcells, 4);
    ng.ctrl.style.sel_bg = (gmb_cell_color_t){0, 255, 0}; ng.ctrl.style.has_sel_bg = true;
    gmb_control_draw(&ng.ctrl, fb);
    int grid_green = 0;
    for (int yy = 2 * 8; yy < 3 * 8; yy++)
        for (int xx = 0; xx < 10 * 8; xx++)
            if (gpb[yy * gwb + xx] == 0x0000FF00u) grid_green = 1;
    CHECK(grid_green, "grid chrome: cursor row highlight green");
    gmb_screen_detach_sink(fb);
    gmb_screen_free(fb);

    /* 18) Chrome nativo CheckBox/Toggle/ListBox */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwc = 0, ghc = 0;
    const uint32_t* gpc = gmb_gfx_visible_pixels(&gwc, &ghc);
    gmb_gfx_clear_rect(0, 0, gwc - 1, ghc - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* cb2 = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(cb2, 0, 0, 8, 8);
    gmb_checkbox_t ncb; memset(&ncb, 0, sizeof(ncb));
    gmb_checkbox_init(&ncb, 0, 0, 10, "chk");
    ncb.ctrl.style.fg = (gmb_cell_color_t){255, 0, 0}; ncb.ctrl.style.has_fg = true;
    gmb_control_draw(&ncb.ctrl, cb2);
    CHECK(gpc[0 * gwc + 5 * 8] == 0x00FF0000u, "checkbox chrome: stroke red");
    gmb_toggle_button_t ntg; memset(&ntg, 0, sizeof(ntg));
    gmb_toggle_button_init(&ntg, 0, 1, 10, "tgl");
    ntg.ctrl.style.fg = (gmb_cell_color_t){255, 0, 0}; ntg.ctrl.style.has_fg = true;
    gmb_control_draw(&ntg.ctrl, cb2);
    CHECK(gpc[(1 * 8) * gwc + 5 * 8] == 0x00FF0000u, "toggle chrome: stroke red");
    gmb_listbox_t nlb; memset(&nlb, 0, sizeof(nlb));
    gmb_listbox_init(&nlb, 0, 2, 10, 3);
    static const char* litems[] = { "x", "y", "z" };
    gmb_listbox_set_items(&nlb, litems, 3);
    nlb.ctrl.style.sel_bg = (gmb_cell_color_t){0, 255, 0}; nlb.ctrl.style.has_sel_bg = true;
    gmb_control_draw(&nlb.ctrl, cb2);
    int lb_green = 0;
    for (int yy = 2 * 8; yy < 3 * 8; yy++) for (int xx = 0; xx < 10 * 8; xx++) if (gpc[yy * gwc + xx] == 0x0000FF00u) lb_green = 1;
    CHECK(lb_green, "listbox chrome: selected row green");
    gmb_screen_detach_sink(cb2);
    gmb_screen_free(cb2);

    /* 19) Chrome nativo ComboBox/Tab/Radio/Slider/NumericUpDown/Progress/StatusBar */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwx = 0, ghx = 0;
    const uint32_t* gpx = gmb_gfx_visible_pixels(&gwx, &ghx);
    gmb_gfx_clear_rect(0, 0, gwx - 1, ghx - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* xb = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(xb, 0, 0, 8, 8);
    int found;
    gmb_tab_t xt; memset(&xt, 0, sizeof(xt));
    gmb_tab_init(&xt, 0, 0, 10);
    static const char* xtabs[] = { "a", "b" };
    gmb_tab_set_tabs(&xt, xtabs, 2);
    xt.ctrl.style.sel_bg = (gmb_cell_color_t){0,255,0}; xt.ctrl.style.has_sel_bg = true;
    gmb_control_draw(&xt.ctrl, xb);
    found = 0; for (int yy=0; yy<8; yy++) for (int xx=0; xx<80; xx++) if (gpx[yy*gwx+xx]==0x0000FF00u) found=1;
    CHECK(found, "tab chrome: active highlight green");
    gmb_radio_group_t xr; memset(&xr, 0, sizeof(xr));
    gmb_radio_group_init(&xr, 0, 1, 10, 2);
    static const char* xopts[] = { "x", "y" };
    gmb_radio_group_set_options(&xr, xopts, 2);
    xr.selected = 1;
    xr.ctrl.style.sel_bg = (gmb_cell_color_t){0,255,0}; xr.ctrl.style.has_sel_bg = true;
    gmb_control_draw(&xr.ctrl, xb);
    found = 0; for (int yy=2*8; yy<3*8; yy++) for (int xx=0; xx<80; xx++) if (gpx[yy*gwx+xx]==0x0000FF00u) found=1;
    CHECK(found, "radio chrome: selected highlight green");
    gmb_slider_t xs; memset(&xs, 0, sizeof(xs));
    gmb_slider_init(&xs, 0, 3, 10);
    gmb_slider_set_range(&xs, 0, 100); gmb_slider_set_value(&xs, 50);
    xs.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; xs.ctrl.style.has_fg = true;
    gmb_control_draw(&xs.ctrl, xb);
    found = 0; for (int yy=3*8; yy<4*8; yy++) for (int xx=0; xx<80; xx++) if (gpx[yy*gwx+xx]==0x00FF0000u) found=1;
    CHECK(found, "slider chrome: filled bar red");
    gmb_combobox_t xc; memset(&xc, 0, sizeof(xc));
    gmb_combobox_init(&xc, 0, 4, 10);
    static const char* xitems[] = { "p", "q" };
    gmb_combobox_set_items(&xc, xitems, 2);
    xc.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; xc.ctrl.style.has_fg = true;
    gmb_control_draw(&xc.ctrl, xb);
    CHECK(gpx[(4*8)*gwx + 5*8] == 0x00FF0000u, "combobox chrome: stroke red");
    gmb_numeric_up_down_t xn; memset(&xn, 0, sizeof(xn));
    gmb_numeric_up_down_init(&xn, 0, 5, 10);
    xn.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; xn.ctrl.style.has_fg = true;
    gmb_control_draw(&xn.ctrl, xb);
    found = 0; for (int yy=5*8; yy<6*8; yy++) for (int xx=0; xx<80; xx++) if (gpx[yy*gwx+xx]==0x00FF0000u) found=1;
    CHECK(found, "numeric chrome: stroke red");
    gmb_status_bar_t xsb; memset(&xsb, 0, sizeof(xsb));
    gmb_status_bar_init(&xsb, 0, 6, 10); gmb_status_bar_set_text(&xsb, "s");
    xsb.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; xsb.ctrl.style.has_fg = true;
    gmb_control_draw(&xsb.ctrl, xb);
    CHECK(gpx[(6*8)*gwx + 5*8] == 0x00FF0000u, "statusbar chrome: stroke red");
    gmb_screen_detach_sink(xb);
    gmb_screen_free(xb);

    /* 20) Chrome nativo ultimo batch (spinner/starrating/logview/hexview/scrollbar/bigtext) */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwz = 0, ghz = 0;
    const uint32_t* gpz = gmb_gfx_visible_pixels(&gwz, &ghz);
    gmb_gfx_clear_rect(0, 0, gwz - 1, ghz - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* zb = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(zb, 0, 0, 8, 8);
    int red;
#define REDBAND(y0,y1) do { red=0; for (int yy=(y0); yy<(y1); yy++) for (int xx=0; xx<200; xx++) if (gpz[yy*gwz+xx]==0x00FF0000u) red=1; } while(0)
    gmb_spinner_t zsp; memset(&zsp, 0, sizeof(zsp));
    gmb_spinner_init(&zsp, 0, 0, 10);
    zsp.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zsp.ctrl.style.has_fg = true;
    gmb_control_draw(&zsp.ctrl, zb);
    REDBAND(0, 8); CHECK(red, "spinner chrome: stroke red");
    gmb_star_rating_t zsr; memset(&zsr, 0, sizeof(zsr));
    gmb_star_rating_init(&zsr, 0, 1, 10);
    zsr.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zsr.ctrl.style.has_fg = true;
    gmb_control_draw(&zsr.ctrl, zb);
    REDBAND(8, 16); CHECK(red, "starrating chrome: stroke red");
    gmb_log_view_t zlv; memset(&zlv, 0, sizeof(zlv));
    gmb_log_view_init(&zlv, 0, 2, 10, 3);
    zlv.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zlv.ctrl.style.has_fg = true;
    gmb_control_draw(&zlv.ctrl, zb);
    REDBAND(16, 40); CHECK(red, "logview chrome: stroke red");
    gmb_hex_view_t zhv; memset(&zhv, 0, sizeof(zhv));
    gmb_hex_view_init(&zhv, 0, 5, 10, 2);
    zhv.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zhv.ctrl.style.has_fg = true;
    gmb_control_draw(&zhv.ctrl, zb);
    REDBAND(40, 56); CHECK(red, "hexview chrome: stroke red");
    gmb_scrollbar_t zsb; memset(&zsb, 0, sizeof(zsb));
    gmb_scrollbar_init(&zsb, 0, 7, 10, 0);
    zsb.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zsb.ctrl.style.has_fg = true;
    gmb_control_draw(&zsb.ctrl, zb);
    REDBAND(56, 64); CHECK(red, "scrollbar chrome: stroke red");
    gmb_big_text_t zbt; memset(&zbt, 0, sizeof(zbt));
    gmb_big_text_init(&zbt, 0, 8);
    gmb_big_text_set_text(&zbt, "I");
    zbt.has_color = true; zbt.color = (gmb_cell_color_t){255,0,0};
    gmb_control_draw(&zbt.ctrl, zb);
    REDBAND(64, 120); CHECK(red, "bigtext chrome: red blocks");
    gmb_menu_t zm; memset(&zm, 0, sizeof(zm));
    gmb_menu_init(&zm, 0, 15, 20); gmb_menu_add_item(&zm, "file");
    gmb_control_draw(&zm.ctrl, zb);
    gmb_color_picker_t zcp; memset(&zcp, 0, sizeof(zcp));
    gmb_color_picker_init(&zcp, 0, 14, 10, 1);
    gmb_control_draw(&zcp.ctrl, zb);
    gmb_date_picker_t zdp; memset(&zdp, 0, sizeof(zdp));
    gmb_date_picker_init(&zdp, 0, 13, 10);
    gmb_control_draw(&zdp.ctrl, zb);
    CHECK(1, "menu/colorpicker/datepicker draw no crash");
#undef REDBAND
    gmb_screen_detach_sink(zb);
    gmb_screen_free(zb);

    /* 21) Chrome nativo TreeView + FileExplorer */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwt = 0, ght = 0;
    const uint32_t* gpt = gmb_gfx_visible_pixels(&gwt, &ght);
    gmb_gfx_clear_rect(0, 0, gwt - 1, ght - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* tvb = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(tvb, 0, 0, 8, 8);
    gmb_tree_view_t tv; memset(&tv, 0, sizeof(tv));
    gmb_tree_view_init(&tv, 0, 0, 12, 4);
    gmb_tree_view_add_node(&tv, "root", 0, true);
    gmb_tree_view_add_node(&tv, "child", 1, false);
    tv.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; tv.ctrl.style.has_fg = true;
    gmb_control_draw(&tv.ctrl, tvb);
    int tred = 0;
    for (int yy = 0; yy < 4*8; yy++) for (int xx = 0; xx < 12*8; xx++) if (gpt[yy*gwt+xx] == 0x00FF0000u) tred = 1;
    CHECK(tred, "treeview chrome: stroke red");
    gmb_file_explorer_t xfe; memset(&xfe, 0, sizeof(xfe));
    gmb_file_explorer_init(&xfe, "/tmp/gmb_fe_test");
    gmb_file_explorer_set_bounds(&xfe, 0, 5, 16, 4);
    xfe.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; xfe.ctrl.style.has_fg = true;
    gmb_control_draw(&xfe.ctrl, tvb);
    int fred = 0;
    for (int yy = 5*8; yy < 9*8; yy++) for (int xx = 0; xx < 16*8; xx++) if (gpt[yy*gwt+xx] == 0x00FF0000u) fred = 1;
    CHECK(fred, "fileexplorer chrome: stroke red");
    gmb_screen_detach_sink(tvb);
    gmb_screen_free(tvb);

    /* 22) Chrome nativo Dashboard/DataList/ReorderListBox/ScrollView */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwd = 0, ghd = 0;
    const uint32_t* gpd = gmb_gfx_visible_pixels(&gwd, &ghd);
    gmb_gfx_clear_rect(0, 0, gwd - 1, ghd - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* db = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(db, 0, 0, 8, 8);
    int dred;
#define REDBAND2(y0,y1) do { dred=0; for (int yy=(y0); yy<(y1); yy++) for (int xx=0; xx<200; xx++) if (gpd[yy*gwd+xx]==0x00FF0000u) dred=1; } while(0)
    gmb_dashboard_t zdb; memset(&zdb, 0, sizeof(zdb));
    gmb_dashboard_init(&zdb, 0, 0, 12, 3);
    gmb_dashboard_add_metric(&zdb, "cpu", 50, 100, "%");
    zdb.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zdb.ctrl.style.has_fg = true;
    gmb_control_draw(&zdb.ctrl, db);
    REDBAND2(0, 24); CHECK(dred, "dashboard chrome: stroke red");
    gmb_data_list_t zdl; memset(&zdl, 0, sizeof(zdl));
    gmb_data_list_init(&zdl, 0, 3, 12, 3);
    zdl.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zdl.ctrl.style.has_fg = true;
    gmb_control_draw(&zdl.ctrl, db);
    REDBAND2(24, 48); CHECK(dred, "datalist chrome: stroke red");
    gmb_reorder_listbox_t zrl; memset(&zrl, 0, sizeof(zrl));
    gmb_reorder_listbox_init(&zrl, 0, 6, 12, 3);
    zrl.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zrl.ctrl.style.has_fg = true;
    gmb_control_draw(&zrl.ctrl, db);
    REDBAND2(48, 72); CHECK(dred, "reorderlist chrome: stroke red");
    gmb_scroll_view_t zsv; memset(&zsv, 0, sizeof(zsv));
    gmb_scroll_view_init(&zsv, 0, 9, 12, 3);
    zsv.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zsv.ctrl.style.has_fg = true;
    gmb_control_draw(&zsv.ctrl, db);
    REDBAND2(72, 96); CHECK(dred, "scrollview chrome: stroke red");
#undef REDBAND2
    gmb_screen_detach_sink(db);
    gmb_screen_free(db);

    /* 23) Chrome nativo PrinterDialog */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gpw2 = 0, gph2 = 0;
    const uint32_t* gpp2 = gmb_gfx_visible_pixels(&gpw2, &gph2);
    gmb_gfx_clear_rect(0, 0, gpw2 - 1, gph2 - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* pdb = gmb_screen_new(24, 10);
    gmb_screen_attach_gfx(pdb, 0, 0, 8, 8);
    gmb_printer_dialog_t zpd; memset(&zpd, 0, sizeof(zpd));
    gmb_printer_dialog_init(&zpd);
    gmb_printer_dialog_set_bounds(&zpd, 0, 0, 20, 8);
    zpd.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zpd.ctrl.style.has_fg = true;
    gmb_control_draw(&zpd.ctrl, pdb);
    int pdred = 0;
    for (int yy = 0; yy < 8*8; yy++) for (int xx = 0; xx < 160; xx++) if (gpp2[yy*gpw2+xx] == 0x00FF0000u) pdred = 1;
    CHECK(pdred, "printerdialog chrome: stroke red");
    gmb_screen_detach_sink(pdb);
    gmb_screen_free(pdb);

    /* 24) Chrome nativo TextInput/TextArea/PropertyGrid/Line/ImageButton */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwi = 0, ghi = 0;
    const uint32_t* gpi = gmb_gfx_visible_pixels(&gwi, &ghi);
    gmb_gfx_clear_rect(0, 0, gwi - 1, ghi - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* ib2 = gmb_screen_new(32, 16);
    gmb_screen_attach_gfx(ib2, 0, 0, 8, 8);
    int ired;
#define REDBAND3(y0,y1) do { ired=0; for (int yy=(y0); yy<(y1); yy++) for (int xx=0; xx<200; xx++) if (gpi[yy*gwi+xx]==0x00FF0000u) ired=1; } while(0)
    gmb_text_input_t zti; memset(&zti, 0, sizeof(zti));
    gmb_text_input_init(&zti, 0, 0, 12);
    zti.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zti.ctrl.style.has_fg = true;
    gmb_control_draw(&zti.ctrl, ib2);
    REDBAND3(0, 8); CHECK(ired, "textinput chrome: stroke red");
    gmb_text_area_t zta; memset(&zta, 0, sizeof(zta));
    gmb_text_area_init(&zta, 0, 1, 12, 3);
    zta.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zta.ctrl.style.has_fg = true;
    gmb_control_draw(&zta.ctrl, ib2);
    REDBAND3(8, 32); CHECK(ired, "textarea chrome: stroke red");
    gmb_property_grid_t zpg; memset(&zpg, 0, sizeof(zpg));
    gmb_property_grid_init(&zpg, 0, 4, 12, 3);
    zpg.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zpg.ctrl.style.has_fg = true;
    gmb_control_draw(&zpg.ctrl, ib2);
    REDBAND3(32, 56); CHECK(ired, "propertygrid chrome: stroke red");
    gmb_image_button_t zib; memset(&zib, 0, sizeof(zib));
    gmb_image_button_init(&zib, 0, 7, 10);
    zib.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zib.ctrl.style.has_fg = true;
    gmb_control_draw(&zib.ctrl, ib2);
    REDBAND3(56, 64); CHECK(ired, "imagebutton chrome: stroke red");
    gmb_hline_t zhl; memset(&zhl, 0, sizeof(zhl));
    gmb_hline_init(&zhl, 0, 8, 12, "");
    zhl.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zhl.ctrl.style.has_fg = true;
    gmb_control_draw(&zhl.ctrl, ib2);
    REDBAND3(64, 72); CHECK(ired, "line chrome: red horizontal line");
#undef REDBAND3
    gmb_screen_detach_sink(ib2);
    gmb_screen_free(ib2);

    /* 25) Chrome nativo TBrowse */
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwb2 = 0, ghb2 = 0;
    const uint32_t* gpb2 = gmb_gfx_visible_pixels(&gwb2, &ghb2);
    gmb_gfx_clear_rect(0, 0, gwb2 - 1, ghb2 - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* bbuf = gmb_screen_new(24, 8);
    gmb_screen_attach_gfx(bbuf, 0, 0, 8, 8);
    gmb_vec_ds_t bv; gmb_vec_ds_make(&bv, 5, 2);
    gmb_vec_ds_set(&bv, 0, 0, "aa"); gmb_vec_ds_set(&bv, 0, 1, "bb");
    gmb_browse_datasource_t bds; gmb_vec_ds_datasource(&bv, &bds);
    gmb_browse_column_t bcols2[2]; memset(bcols2, 0, sizeof(bcols2));
    snprintf(bcols2[0].header, sizeof(bcols2[0].header), "c1"); bcols2[0].width = 6; bcols2[0].visible = true;
    snprintf(bcols2[1].header, sizeof(bcols2[1].header), "c2"); bcols2[1].width = 6; bcols2[1].visible = true;
    gmb_rect_t barea = { 0, 0, 20, 6 };
    gmb_browse_t* bbr = gmb_browse_new(&barea, 2, bcols2, &bds);
    bbr->header_colors.foreground = (gmb_cell_color_t){255,0,0};
    gmb_control_draw(&bbr->ctrl, bbuf);
    int bred = 0;
    for (int yy = 0; yy < 6*8; yy++) for (int xx = 0; xx < 20*8; xx++) if (gpb2[yy*gwb2+xx] == 0x00FF0000u) bred = 1;
    CHECK(bred, "tbrowse chrome: stroke red");
    gmb_screen_detach_sink(bbuf);
    gmb_screen_free(bbuf);
    gmb_browse_free(bbr);

    /* 26) Finestra PIXEL: render nativo sul page gfx */
    gmb_winstmt_reset();
    gmb_gfx_init();
    gmb_gfx_screen(13, 640, 400, 32);
    int gww = 0, ghw = 0;
    const uint32_t* gpw = gmb_gfx_visible_pixels(&gww, &ghw);
    gmb_gfx_clear_rect(0, 0, gww - 1, ghw - 1, (int)(0x40000000u | 0x0000FFu));
    /* define/content con screen gfx attivo -> auto-render nativo (ws_draw). */
    gmb_winstmt_define("w1", "T", 0, 0, 5, 10, GMB_WINSTMT_PIXEL);
    gmb_winstmt_content("w1", "hello");
    int cyan = 0;
    for (int yy = 0; yy < gww * ghp / (gww ? gww : 1) && yy < 400; yy++)
        for (int xx = 0; xx < gww; xx++) if (gpw[yy*gww+xx] == 0x0000FFFFu) cyan = 1;
    CHECK(cyan, "window PIXEL: auto-render frame cyan nativo");
    gmb_winstmt_reset();

    /* 27) Candlestick (widget nuovo): TUI + gfx */
    gmb_candlestick_t zck; memset(&zck, 0, sizeof(zck));
    gmb_candlestick_init(&zck, 0, 0, 6, 8);
    static gmb_candle_t zcd[3] = { {1.0,2.0,0.5,1.8}, {1.8,2.5,1.5,1.6}, {1.6,3.0,1.4,2.8} };
    gmb_candlestick_set_candles(&zck, zcd, 3);
    gmb_screen_buffer_t* ctb = gmb_screen_new(6, 8);
    gmb_screen_clear(ctb, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_control_draw(&zck.ctrl, ctb);
    int cbody = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 6; x++) if (cell_ch(ctb, x, y) == 0x2588) cbody = 1;
    CHECK(cbody, "candlestick TUI: corpo candela");
    gmb_screen_free(ctb);
    gmb_gfx_init();
    gmb_gfx_screen(13, 128, 128, 32);
    int gwc3 = 0, ghc3 = 0;
    const uint32_t* gpc3 = gmb_gfx_visible_pixels(&gwc3, &ghc3);
    gmb_gfx_clear_rect(0, 0, gwc3 - 1, ghc3 - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* ckb = gmb_screen_new(8, 10);
    gmb_screen_attach_gfx(ckb, 0, 0, 8, 8);
    gmb_control_draw(&zck.ctrl, ckb);
    int cgreen = 0, cred = 0;
    for (int yy = 0; yy < 8*8; yy++) for (int xx = 0; xx < 6*8; xx++) {
        if (gpc3[yy*gwc3+xx] == 0x0000A000u) cgreen = 1;
        if (gpc3[yy*gwc3+xx] == 0x00D00000u) cred = 1;
    }
    CHECK(cgreen && cred, "candlestick gfx: candele up/down colorate");
    gmb_screen_detach_sink(ckb);
    gmb_screen_free(ckb);
    gmb_control_handle_key(&zck.ctrl, GMB_K_RIGHT);
    CHECK(gmb_candlestick_cursor(&zck) == 1, "candlestick key RIGHT");

    /* 28) CodeArea (Control): gutter/marker/syntax + chrome gfx */
    gmb_code_area_t zca; memset(&zca, 0, sizeof(zca));
    gmb_code_area_init(&zca, 0, 0, 20, 4);
    static const char* zcl[] = { "PRINT x", "y = 1" };
    gmb_code_area_set_lines(&zca, zcl, 2);
    gmb_code_area_set_breakpoint(&zca, 0, true);
    gmb_code_area_add_keyword(&zca, "PRINT");
    gmb_screen_clear(buf, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_control_draw(&zca.ctrl, buf);
    int cgw = gmb_code_area_gutter(&zca);
    CHECK(cell_ch(buf, 0, 0) == 0x25CF, "codearea gutter: marker breakpoint");
    CHECK(cell_ch(buf, cgw, 0) == 'P', "codearea code starts after gutter");
    gmb_control_handle_key(&zca.ctrl, GMB_K_DOWN);
    CHECK(gmb_code_area_cursor_line(&zca) == 1, "codearea DOWN");
    gmb_gfx_init();
    gmb_gfx_screen(13, 256, 128, 32);
    int gwc4 = 0, ghc4 = 0;
    const uint32_t* gpc4 = gmb_gfx_visible_pixels(&gwc4, &ghc4);
    gmb_gfx_clear_rect(0, 0, gwc4 - 1, ghc4 - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* cab = gmb_screen_new(24, 6);
    gmb_screen_attach_gfx(cab, 0, 0, 8, 8);
    zca.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; zca.ctrl.style.has_fg = true;
    gmb_control_draw(&zca.ctrl, cab);
    int cared = 0;
    for (int yy = 0; yy < 4*8; yy++) for (int xx = 0; xx < 20*8; xx++) if (gpc4[yy*gwc4+xx] == 0x00FF0000u) cared = 1;
    CHECK(cared, "codearea chrome: stroke red");
    gmb_screen_detach_sink(cab);
    gmb_screen_free(cab);

    /* 29) Auto-attach surface: controllo reso su target grafico -> per-pixel */
    gmb_surface_set_autoattach(1);
    gmb_gfx_init();
    gmb_gfx_screen(13, 128, 128, 32);
    int gwa = 0, gha = 0;
    const uint32_t* gpa = gmb_gfx_visible_pixels(&gwa, &gha);
    gmb_gfx_clear_rect(0, 0, gwa - 1, gha - 1, (int)(0x40000000u | 0x0000FFu));
    gmb_screen_buffer_t* ab = gmb_screen_new(16, 4);
    gmb_button_t eb; memset(&eb, 0, sizeof(eb));
    gmb_button_init(&eb, 0, 0, 10, "x");
    eb.ctrl.style.fg = (gmb_cell_color_t){255,0,0}; eb.ctrl.style.has_fg = true;
    gmb_control_draw(&eb.ctrl, ab);   /* nessun sink esplicito: lo aggancia autoattach */
    int ared = 0;
    for (int yy = 0; yy < 2*8; yy++) for (int xx = 0; xx < 12*8; xx++) if (gpa[yy*gwa+xx] == 0x00FF0000u) ared = 1;
    CHECK(ared, "autoattach: controllo per-pixel su target grafico");
    gmb_screen_detach_sink(ab);
    gmb_screen_free(ab);
    gmb_surface_set_autoattach(0);

    /* 30) Finestre: mouse (HandleMouse) in coordinate raw*scala -> logiche (headless) */
    gmb_winstmt_reset();
    gmb_gfx_init();
    gmb_gfx_screen(13, 640, 480, 32);
    gmb_gfx_set_render_scale(2.0f);   /* simula Retina: pagina nativa, input in pixel nativi */
    gmb_winstmt_define("w", "T", 0, 0, 10, 30, 0);
    CHECK(gmb_winstmt_count() == 1, "window definita");
    {
        float sc = gmb_gfx_render_scale();
        int cw = (int)(gmb_gfx_text_width("M") / sc + 0.5f); if (cw <= 0) cw = 8;
        int ch2 = (int)(gmb_gfx_text_height() / sc + 0.5f);  if (ch2 <= 0) ch2 = 16;
        int wx = 0, wy = 0, ww = 31 * cw;              /* rect.w = 31 celle */
        int lx = wx + ww - 15, ly = wy + 12;           /* zona bottone chiudi (logico) */
        gmb_gfx_input_reset();
        gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, (int)(lx * sc), (int)(ly * sc), 1, 0);
        gmb_winstmt_handle_mouse_events();
        CHECK(gmb_winstmt_count() == 0, "click chiudi (coordinate raw*scala) -> finestra chiusa");
        (void)ch2;
    }
    gmb_winstmt_reset();
    /* drag titlebar: DOWN + MOVE (raw) spostano la finestra */
    gmb_winstmt_define("d", "T", 0, 0, 10, 30, 0);
    {
        float sc = gmb_gfx_render_scale();
        int cw = (int)(gmb_gfx_text_width("M") / sc + 0.5f); if (cw <= 0) cw = 8;
        int ch2 = (int)(gmb_gfx_text_height() / sc + 0.5f);  if (ch2 <= 0) ch2 = 16;
        int x0 = 0, y0 = 0;
        gmb_gfx_input_reset();
        gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, (int)((x0 + 10) * sc), (int)((y0 + 12) * sc), 1, 0);
        gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_MOVE, (int)((x0 + 80) * sc), (int)((y0 + 52) * sc), 1, 0);
        gmb_winstmt_handle_mouse_events();
        gmb_managed_window_t* mw2 = gmb_window_manager_get(gmb_window_manager_static(), gmb_winstmt_id("d"));
        CHECK(mw2 && (mw2->rect.x > x0 || mw2->rect.y > y0), "drag titlebar -> finestra spostata");
        (void)cw; (void)ch2;
    }
    gmb_winstmt_reset();
    /* minimize: click zona gialla (raw*scala) */
    gmb_winstmt_define("m", "T", 0, 0, 10, 30, 0);
    {
        float sc = gmb_gfx_render_scale();
        int cw = (int)(gmb_gfx_text_width("M") / sc + 0.5f); if (cw <= 0) cw = 8;
        int ww = 31 * cw;
        int lx = ww - 62, ly = 12;   /* centro bottone minimizza (minX+10) */
        gmb_gfx_input_reset();
        gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, (int)(lx * sc), (int)(ly * sc), 1, 0);
        gmb_winstmt_handle_mouse_events();
        CHECK(gmb_winstmt_is_minimized("m") == 1, "click bottone minimizza");
        /* ripristino dal chip taskbar */
        int barY = gmb_gfx_height() - 28 + 5;
        gmb_gfx_input_reset();
        gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, (int)((6 + 10) * sc), (int)(barY * sc), 1, 0);
        gmb_winstmt_handle_mouse_events();
        CHECK(gmb_winstmt_is_minimized("m") == 0, "click chip taskbar -> finestra ripristinata");
    }
    gmb_winstmt_reset();

    /* 31) Probe font (headless): monospace? box-drawing? */
    {
        const char* fp = getenv("GMB_FONT_PATH");
        gmb_gfx_init();
        gmb_gfx_screen(13, 640, 480, 32);
        gmb_gfx_set_render_scale(1.0f);
        if (fp && *fp) gmb_gfx_font(fp, 16);
        fprintf(stderr, "FONT <%s>: M=%d MM=%d i=%d W=%d box(-)=%d box(\xE2\x96\x88)=%d h=%d\n",
                fp ? fp : "(none)", gmb_gfx_text_width("M"), gmb_gfx_text_width("MM"),
                gmb_gfx_text_width("i"), gmb_gfx_text_width("W"),
                gmb_gfx_text_width("\xE2\x94\x80"), gmb_gfx_text_width("\xE2\x96\x88"), gmb_gfx_text_height());
        CHECK(1, "font probe");
    }

    /* 32) Probe render celle->pixel: span glifi e gap */
    {
        const char* fp = getenv("GMB_FONT_PATH");
        gmb_gfx_init();
        gmb_gfx_screen(13, 128, 64, 32);
        gmb_gfx_set_render_scale(1.0f);
        if (fp && *fp) gmb_gfx_font(fp, 16);
        int chars[3] = { 'A', 'B', 'A' };
        unsigned int fg[3] = { 0x40000000u | 0xFFFFFFu, 0x40000000u | 0xFFFFFFu, 0x40000000u | 0xFFFFFFu };
        unsigned int bg[3] = { 0, 0, 0 };
        int w = 0, h = 0; (void)gmb_gfx_visible_pixels(&w, &h);
        gmb_gfx_render_cells(chars, fg, bg, 3, 1);
        const uint32_t* px = gmb_gfx_visible_pixels(&w, &h);
        int colink[256];
        for (int x = 0; x < 128; x++) { int ink = 0; for (int y = 0; y < 32; y++) if (px[y * w + x] != 0) ink = 1; colink[x] = ink; }
        int start = -1; char rng[512]; rng[0] = 0;
        for (int x = 0; x <= 128; x++) {
            if (x < 128 && colink[x] && start < 0) start = x;
            if ((x == 128 || !colink[x]) && start >= 0) { char t[32]; snprintf(t, sizeof(t), "[%d-%d]", start, x - 1); strncat(rng, t, sizeof(rng) - strlen(rng) - 1); start = -1; }
        }
        fprintf(stderr, "RENDER span glifi (A B A): %s  (cw=%d ch=%d)\n", rng, gmb_gfx_text_width("M"), gmb_gfx_text_height());
        CHECK(1, "render probe");
    }

    /* 33) Dump PNG del render celle->pixel (ispezione visiva headless) */
    {
        const char* fp = getenv("GMB_FONT_PATH");
        gmb_gfx_init();
        gmb_gfx_screen(13, 640, 480, 32);
        gmb_gfx_set_render_scale(2.0f);           /* come il run reale su Retina */
        if (fp && *fp) gmb_gfx_font(fp, 32);
        int Wsc = 80, Hsc = 25;
        gmb_screen_buffer_t* sb = gmb_screen_new(Wsc, Hsc);
        gmb_screen_clear(sb, (gmb_cell_color_t){255,255,255}, (gmb_cell_color_t){0,0,170});
        gmb_screen_setstring(sb, 0, 0, "PROGRAM TurboVisionDemo", (gmb_cell_color_t){255,255,255}, (gmb_cell_color_t){0,0,170});
        gmb_screen_setstring(sb, 0, 1, "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x90\xE2\x94\x82\xE2\x94\x94\xE2\x94\x98 \xE2\x96\x88\xE2\x96\x80\xE2\x96\x84 WB", (gmb_cell_color_t){255,255,255}, (gmb_cell_color_t){0,0,170});
        int chars[80*25]; unsigned int fg[80*25], bg[80*25];
        gmb_screen_cell_t* cells = gmb_screen_cells(sb);
        for (int i = 0; i < Wsc*Hsc; i++) {
            chars[i] = (int)cells[i].character;
            fg[i] = 0x40000000u | ((unsigned)cells[i].fg_rgb.r<<16)|((unsigned)cells[i].fg_rgb.g<<8)|cells[i].fg_rgb.b;
            bg[i] = 0x40000000u | ((unsigned)cells[i].bg_rgb.r<<16)|((unsigned)cells[i].bg_rgb.g<<8)|cells[i].bg_rgb.b;
        }
        fprintf(stderr, "PROBE33 cw=%d ch=%d (buffer %dx%d, page %dx%d)\n", gmb_gfx_text_width("M"), gmb_gfx_text_height(), Wsc, Hsc, gmb_gfx_width(), gmb_gfx_height());
        gmb_gfx_render_cells_ex(chars, fg, bg, Wsc, Hsc, gmb_gfx_text_width("M"), gmb_gfx_text_height());
        gmb_gfx_save_screen_png("/tmp/sdlcon.png");
        fprintf(stderr, "PNG scritto /tmp/sdlcon.png\n");
        gmb_screen_free(sb);
        CHECK(1, "png dump");
    }

    /* 34) Content-rect della finestra renderizzato DENTRO il body dal compositor:
     *     le celle di st->buffer (blu), incluso un controllo, devono comparire nel body
     *     della finestra (non solo chrome). Regressione della separazione controlli/finestra. */
    {
        extern void gmb_tui_screen_enable(void);
        gmb_gfx_init();
        gmb_gfx_screen(12, 640, 480, 32);
        gmb_gfx_set_render_scale(1.0f);
        gmb_tui_screen_enable();
        gmb_winstmt_theme_name("Turbo");
        gmb_winstmt_define("W34", "T34", 1, 1, 18, 60, GMB_WINSTMT_DOUBLE | GMB_WINSTMT_SHADOW);
        gmb_winstmt_select("W34");
        gmb_winstmt_append("W34", "content dentro la finestra");
        gmb_clipper_state_t* st34 = gmb_clipper_global_state();
        gmb_surface_set_autoattach(0);
        gmb_checkbox_t cb34; gmb_checkbox_init(&cb34, 2, 3, 20, "opt");
        if (st34 && st34->buffer) gmb_control_draw(&cb34.ctrl, st34->buffer);
        gmb_winstmt_compose_all_pixel();
        int cw34 = gmb_gfx_text_width("M"); if (cw34 < 1) cw34 = 8;
        int ch34 = gmb_gfx_text_height();   if (ch34 < 1) ch34 = 16;
        int blue = 0;
        gmb_managed_window_t* mw34 = gmb_window_manager_get(gmb_window_manager_static(), gmb_winstmt_id("W34"));
        if (mw34) {
            int px0 = mw34->rect.x * cw34, py0 = mw34->rect.y * ch34;
            int px1 = px0 + mw34->rect.w * cw34, py1 = py0 + mw34->rect.h * ch34;
            for (int y = py0 + ch34; y < py1 && y < gmb_gfx_height(); y++)
                for (int x = px0 + 1; x < px1 && x < gmb_gfx_width(); x++)
                    if ((gmb_gfx_point(x, y) & 0xFFFFFFu) == 0x0000AAu) blue++;
        }
        fprintf(stderr, "PROBE34 pixel-blu-nel-body=%d\n", blue);
        CHECK(blue > 100, "content-rect (controlli+testo) renderizzato DENTRO la finestra");
        gmb_gfx_save_screen_png("/tmp/window_content.png");
    }

    gmb_screen_free(buf);
    printf(g_fail == 0 ? "ALL PASS\n" : "FAILURES: %d\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
