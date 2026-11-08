/* Runner di test NATIVO per il runtime C (gmb_runtime_c).
 * Compila i sorgenti C e verifica le funzioni direttamente, senza .NET. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gmb_tui.h"
#include "gmb_tui_input.h"
#include "gmb_tui_datatypes.h"
#include "gmb_tui_numfmt.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_tui_clipper.h"
#include "gmb_tui_text.h"
#include "gmb_tui_canvas.h"
#include "gmb_tui_mouse.h"
#include "gmb_tui_flex.h"
#include "gmb_tui_console.h"
#include "gmb_tui_form.h"
#include "gmb_tui_render.h"
#include "gmb_tui_cache.h"
#include "gmb_tui_emu.h"
#include "gmb_tui_psf.h"
#include "gmb_tui_yaml.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"

/* base64 canonica (gmb_encoding.c / gmb_commonlib.h) — forward decl per evitare
   di includere gmb_runtime.h che collide con gmb_tui_widgets.h su gmb_event_handler. */
char* gmb_base64_encode(const uint8_t* data, size_t len);
uint8_t* gmb_base64_decode(const char* input, size_t* len_out);
#include "gmb_tui_concodes.h"
#include "gmb_tui_events.h"
#include "gmb_tui_dateformat.h"
#include "gmb_tui_lastkey.h"
#include "gmb_tui_keys.h"
#include "gmb_tui_inputmask.h"
#include "gmb_tui_support.h"
#include "gmb_tui_datetimeinput.h"
#include "gmb_tui_history.h"
#include "gmb_tui_hinter.h"
#include "gmb_tui_inputmanager.h"
#include "gmb_tui_pathcompleter.h"
#include "gmb_tui_io.h"
#include "gmb_tui_colors.h"
#include "gmb_tui_charts.h"

static int failures = 0;
static int checks = 0;
static char g_apply1[256], g_apply2[256];
static void apply1(void* ctx, const char* v) { (void)ctx; snprintf(g_apply1, sizeof(g_apply1), "%s", v); }
static void apply2(void* ctx, const char* v) { (void)ctx; snprintf(g_apply2, sizeof(g_apply2), "%s", v); }
static int g_fired_key, g_fired_win, g_fired_mouse;
static void key_h(void* c) { (void)c; g_fired_key++; }
static void win_h(void* c) { (void)c; g_fired_win++; }
static void mouse_h(void* c, int x, int y) { (void)c; (void)x; (void)y; g_fired_mouse++; }
static int g_modal_calls;
static bool g_drew;
static void ctrl_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{ (void)u; (void)c; (void)buf; g_drew = true; }
static bool ctrl_key(void* u, gmb_control_t* c, int key)
{ (void)u; (void)c; return key == 'x'; }
static int g_clicked;
static void on_click(void* ctx) { (void)ctx; g_clicked++; }
static int modal_fn(const gmb_ui_event_t* ev, void* ctx)
{
    (void)ev; (void)ctx;
    g_modal_calls++;
    return g_modal_calls < 3;
}
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_STR(a, b) do { checks++; if (strcmp((a), (b)) != 0) { failures++; printf("FAIL %s:%d: [%s] != [%s]\n", __FILE__, __LINE__, (a), (b)); } } while (0)

static gmb_fsm_transition_t on_evt(void* ctx, int st, int ev)
{
    (void)ctx;
    if (ev == 9) return gmb_fsm_transition_abort(GMB_ABORT_USER);
    return gmb_fsm_transition_change(st + ev);
}

typedef struct fake_app_ctx { int draw_calls; int close; int key_seen; } fake_app_ctx_t;
static const char* fake_app_name(void* u) { (void)u; return "fake"; }
static bool fake_app_close(void* u) { fake_app_ctx_t* c = (fake_app_ctx_t*)u; return c->close != 0; }
static void fake_app_resize(void* u, int w, int h) { (void)u; (void)w; (void)h; }
static void fake_app_draw(void* u, gmb_screen_buffer_t* b) { (void)b; ((fake_app_ctx_t*)u)->draw_calls++; }
static void fake_app_handle(void* u, const gmb_ui_event_t* ev)
{
    (void)ev;
    ((fake_app_ctx_t*)u)->close = 1;
}
static void fake_key_handler(void* app_ctx) { ((fake_app_ctx_t*)app_ctx)->key_seen++; }
static void fake_render(gmb_screen_buffer_t* sb, void* ctx) { (void)sb; (void)ctx; }
static int fake_input_read(void* user, int timeout_ms, gmb_ui_event_t* out)
{
    (void)timeout_ms;
    static int n = 0;
    memset(out, 0, sizeof(*out));
    if (n == 0) { n = 1; out->type = GMB_UIEVENT_KEY; out->key = 'x'; return 1; }
    out->type = GMB_UIEVENT_NONE;
    return 0;
}

int main(void)
{
    /* ---- gmb_tui_text: style patch ---- */
    {
        gmb_style_t base = gmb_style_new();
        gmb_style_t fg = gmb_style_with_fg(base, GMB_CELLCOLOR_RED);
        gmb_style_t bg = gmb_style_with_bg(base, GMB_CELLCOLOR_BLUE);
        gmb_style_t patched = gmb_style_patch(fg, bg);
        CHECK(patched.has_fg && gmb_cellcolor_eq(patched.fg, GMB_CELLCOLOR_RED));
        CHECK(patched.has_bg && gmb_cellcolor_eq(patched.bg, GMB_CELLCOLOR_BLUE));
        gmb_style_t add = gmb_style_with_add_modifier(base, GMB_MOD_BOLD);
        CHECK(add.add_mod == GMB_MOD_BOLD);
        gmb_style_t rm = gmb_style_with_remove_modifier(add, GMB_MOD_BOLD);
        CHECK(rm.add_mod == GMB_MOD_NONE);
        CHECK(rm.sub_mod & GMB_MOD_BOLD);
        gmb_style_t reset = gmb_style_reset();
        CHECK(reset.sub_mod == 0x1FF);
    }

    /* ---- gmb_tui_text: line / text ---- */
    {
        gmb_line_t line;
        gmb_line_raw(&line, "hello world");
        CHECK(gmb_line_width(&line) == 11);
        char buf[128];
        CHECK(gmb_line_text(&line, buf, sizeof(buf)) == 11);
        CHECK_STR(buf, "hello world");
        gmb_text_t txt;
        gmb_text_raw(&txt, "a\nbb\nccc");
        CHECK(gmb_text_height(&txt) == 3);
        CHECK(gmb_text_width(&txt) == 3);
    }

    /* ---- gmb_tui_clipper: numbers / set / dates ---- */
    {
        CHECK(gmb_clipper_num_mod(-7.0, 3.0) == 2.0);
        CHECK(gmb_clipper_num_mod(7, -3) == -2);
        CHECK(gmb_clipper_num_int(-3.7) == -3);
        CHECK(fabs(gmb_clipper_num_round(2.345, 2) - 2.35) < 1e-9);
        CHECK(gmb_clipper_num_abs_int(-5) == 5);

        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        CHECK(st != NULL);
        char out[256];
        gmb_clipper_set_format_number(st, 1234.567, out, sizeof(out));
        CHECK_STR(out, " 1234.567");
        gmb_clipper_set_decimals(st, 2);
        gmb_clipper_set_fixed(st, 0);
        gmb_clipper_set_format_number(st, 1234.567, out, sizeof(out));
        CHECK_STR(out, " 1234.57");
        gmb_clipper_set_fixed(st, 1);
        gmb_clipper_set_format_number(st, 1234.5, out, sizeof(out));
        CHECK_STR(out, " 1234.50");

        /* date */
        gmb_clipper_dates_dtoc(st, 2024, 12, 25, out, sizeof(out));
        CHECK_STR(out, "12-25-2024");
        gmb_clipper_set_date_format(st, "dd/mm/yyyy");
        gmb_clipper_dates_dtoc(st, 2024, 12, 25, out, sizeof(out));
        CHECK_STR(out, "25/12/2024");
        gmb_clipper_date_t d;
        CHECK(gmb_clipper_dates_ctod(st, "25/12/2024", &d) == 1);
        CHECK(d.y == 2024 && d.m == 12 && d.d == 25);
        CHECK(gmb_clipper_dates_dow(2024, 12, 25) == 4); /* mercoledì */
        gmb_clipper_dates_cdow(2024, 12, 25, out, sizeof(out));
        CHECK_STR(out, "Wednesday");
        gmb_clipper_dates_cmonth(2024, 12, 25, out, sizeof(out));
        CHECK_STR(out, "December");

        /* output */
        gmb_clipper_output_qout(st, "hello");
        CHECK(gmb_clipper_output_row(st) == 1);
        CHECK(gmb_clipper_output_col(st) == 0);
        gmb_clipper_output_set_pos(st, 0, 0);
        gmb_clipper_output_say(st, 3, 5, "testo");
        CHECK(gmb_clipper_output_row(st) == 3);
        CHECK(gmb_clipper_output_col(st) == 10);

        /* read headless */
        gmb_clipper_read_reset_gets(st);
        gmb_clipper_read_add_field(st, 1, 1, "", NULL, false, 0, 0);
        gmb_clipper_read_add_field(st, 2, 1, "", NULL, false, 0, 0);
        gmb_clipper_input_keyboard(st, "\t\r");
        CHECK(gmb_clipper_read_read(st) == 0);
        CHECK(gmb_clipper_read_get_active(st) == -1);
        gmb_clipper_read_reset_gets(st);
        gmb_clipper_read_add_field(st, 1, 1, "", NULL, false, 0, 0);
        gmb_clipper_input_keyboard(st, "\x1b");
        CHECK(gmb_clipper_read_read(st) == 6);
        gmb_clipper_state_free(st);

        /* accept */
        gmb_clipper_state_t* st2 = gmb_clipper_state_new(80, 24);
        gmb_clipper_input_keyboard(st2, "abc\r");
        char buf2[64];
        CHECK(gmb_clipper_input_accept(st2, buf2, sizeof(buf2)) == 1);
        CHECK_STR(buf2, "abc");
        gmb_clipper_state_free(st2);
    }

    /* ---- gmb_tui_confun ---- */
    {
        char out[256];
        gmb_confun_center("abc", 10, out, sizeof(out));
        CHECK_STR(out, "   abc    ");
        gmb_confun_right("abc", 6, out, sizeof(out));
        CHECK_STR(out, "   abc");
        gmb_confun_left("abc", 6, out, sizeof(out));
        CHECK_STR(out, "abc   ");
        gmb_confun_clip("abcdef", 3, out, sizeof(out));
        CHECK_STR(out, "abc");
        int n = gmb_confun_boxed("hi", out, sizeof(out));
        CHECK(n > 0);
    }

    /* ---- gmb_tui_canvas ---- */
    {
        gmb_screen_buffer_t* buffer = gmb_screen_new(20, 10);
        gmb_canvas_t canvas;
        gmb_rect_t frame = { 2, 2, 10, 5 };
        gmb_canvas_new(&canvas, buffer, frame);
        gmb_canvas_pixel(&canvas, 0, 0, 'A', NULL, NULL);
        gmb_canvas_pixel(&canvas, 5, 3, 'B', NULL, NULL);
        gmb_canvas_pixel(&canvas, 20, 0, 'X', NULL, NULL); /* fuori frame: ignorato */
        gmb_screen_cell_t* cells = gmb_screen_cells(buffer);
        int idx = 2 * 20 + 2;
        CHECK(cells[idx].character == 'A');
        gmb_canvas_draw_string(&canvas, 0, 1, "abc", NULL, NULL);
        CHECK(cells[3 * 20 + 2].character == 'a');
        gmb_canvas_hline(&canvas, 0, 2, 5, '-', NULL, NULL);
        CHECK(cells[4 * 20 + 3].character == '-');
        gmb_canvas_box(&canvas, (gmb_rect_t){1, 1, 6, 3}, NULL, NULL);
        CHECK(cells[3 * 20 + 3].character == 0x250C); /* ┌ */
        gmb_screen_free(buffer);
    }

    /* ---- gmb_tui_tbrowse ---- */
    {
        gmb_vec_ds_t v;
        gmb_vec_ds_make(&v, 4, 3);
        gmb_vec_ds_set(&v, 0, 0, "aa");
        gmb_vec_ds_set(&v, 0, 1, "bb");
        gmb_browse_datasource_t ds;
        gmb_vec_ds_datasource(&v, &ds);
        gmb_rect_t area = { 0, 0, 32, 10 };
        gmb_browse_column_t cols[3];
        for (int i = 0; i < 3; i++)
        {
            memset(&cols[i], 0, sizeof(cols[i]));
            snprintf(cols[i].header, sizeof(cols[i].header), "H%d", i);
            cols[i].width = 10;
            cols[i].visible = true;
        }
        gmb_browse_t* b = gmb_browse_new(&area, 3, cols, &ds);
        gmb_browse_move_down(b);
        CHECK(gmb_browse_row_pos(b) == 1);
        gmb_browse_move_up(b);
        CHECK(gmb_browse_row_pos(b) == 0);
        char frame[8192];
        int fn = gmb_browse_render(b, frame, sizeof(frame));
        CHECK(fn > 0);
        CHECK(strstr(frame, "H0") != NULL);
        gmb_browse_free(b);
    }

    /* ---- gmb_tui_datatypes ---- */
    {
        char out[256];
        gmb_locale_t it = gmb_locale_italian();
        gmb_numbers_format_int(1234567, &it, out, sizeof(out));
        CHECK_STR(out, "1.234.567");
        gmb_numbers_format_decimal(1234.5, 2, &it, out, sizeof(out));
        CHECK_STR(out, "1.234,50");
        gmb_numbers_format_bytes(1536, out, sizeof(out));
        CHECK_STR(out, "1.50 KB");
        CHECK(gmb_business_is_valid_luhn("79927398713"));
        CHECK(!gmb_business_is_valid_luhn("79927398710"));
        CHECK(gmb_dates_is_leap_year(2024));
        CHECK(!gmb_dates_is_leap_year(2023));
        CHECK(gmb_dates_days_in_month(2024, 2) == 29);
    }

    /* ---- gmb_tui_numfmt ---- */
    {
        char out[256];
        gmb_number_format_t nf;
        gmb_numfmt_init(&nf, '.', ',');
        gmb_numfmt_format(&nf, ",.2f", 1234.567, out, sizeof(out));
        CHECK_STR(out, "1,234.57");
        gmb_numfmt_format(&nf, "010d", 42, out, sizeof(out));
        CHECK_STR(out, "0000000042");
        gmb_numfmt_format(&nf, "x", 255, out, sizeof(out));
        CHECK_STR(out, "ff");
        gmb_numfmt_format(&nf, "X", 255, out, sizeof(out));
        CHECK_STR(out, "FF");
        gmb_numfmt_format(&nf, "e", 12345.678, out, sizeof(out));
        CHECK_STR(out, "1.234568e+004");
    }

    /* ---- gmb_tui_input ---- */
    {
        char out[256];
        gmb_currency_format(1234.5, 2, '.', ',', out, sizeof(out));
        CHECK_STR(out, "1.234,50");
        gmb_currency_format(42, 0, '.', ',', out, sizeof(out));
        CHECK_STR(out, "42");
        double v;
        int dec = 2;
        CHECK(gmb_currency_tryparse("1.234,50", &v, &dec, ','));
        CHECK(fabs(v - 1234.50) < 1e-6);
        gmb_textutil_allowed_chars("a1b2c3", "abc", out, sizeof(out));
        CHECK_STR(out, "abc");
        gmb_textutil_apply_case("aBc", true, false, out, sizeof(out));
        CHECK_STR(out, "ABC");
    }

    /* ---- gmb_tui_tbrowse: editing inline ---- */
    {
        gmb_vec_ds_t v;
        gmb_vec_ds_make(&v, 3, 3);
        gmb_browse_datasource_t ds;
        gmb_vec_ds_datasource(&v, &ds);
        gmb_rect_t area = { 0, 0, 40, 10 };
        gmb_browse_column_t cols[3];
        for (int i = 0; i < 3; i++)
        {
            memset(&cols[i], 0, sizeof(cols[i]));
            snprintf(cols[i].header, sizeof(cols[i].header), "H%d", i);
            cols[i].width = 10;
            cols[i].visible = true;
            cols[i].editable = true;
        }
        gmb_browse_t* b = gmb_browse_new(&area, 3, cols, &ds);
        CHECK(gmb_browse_begin_edit(b));
        CHECK(gmb_browse_apply_edit_key(b, 'A') == GMB_BA_NONE);
        CHECK(gmb_browse_apply_edit_key(b, 'B') == GMB_BA_NONE);
        CHECK(gmb_browse_apply_edit_key(b, 'C') == GMB_BA_NONE);
        CHECK(gmb_browse_apply_edit_key(b, GMB_K_ENTER) == GMB_BA_SELECTED);
        char cell[512];
        CHECK(gmb_browse_cell_value(b, 0, 0, cell, sizeof(cell)) == 3);
        CHECK_STR(cell, "ABC");
        gmb_browse_free(b);
    }

    /* ---- gmb_tui_clipper: save/rest screen ---- */
    {
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_clipper_output_say(st, 2, 5, "testo");
        char saved[4096];
        int sn = gmb_clipper_scrn_save_screen(st, 0, 0, 5, 10, saved, sizeof(saved));
        CHECK(sn > 0);
        CHECK(strncmp(saved, "S11;6|", 6) == 0);
        gmb_clipper_output_say(st, 2, 5, "     ");
        CHECK(gmb_clipper_scrn_rest_screen(st, saved, 0, 0) == 1);
        char saved2[4096];
        gmb_clipper_scrn_save_screen(st, 0, 0, 5, 10, saved2, sizeof(saved2));
        CHECK(strcmp(saved, saved2) == 0);
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_confun: bold / fgrgb ---- */
    {
        char out[256];
        int n = gmb_confun_bold("x", out, sizeof(out));
        CHECK(n > 0);
        CHECK(strncmp(out, "\x1b[1m", 4) == 0);
        CHECK(strstr(out, "x") != NULL);
        n = gmb_confun_fgrgb("x", 255, 0, 128, out, sizeof(out));
        CHECK(n > 0);
        CHECK(strstr(out, "38;2") != NULL);
    }

    /* ---- gmb_tui_text: styles ---- */
    {
        char out[256];
        CHECK(gmb_styles_style_code("bold") == 1);
        CHECK(gmb_styles_style_code("underline") == 4);
        CHECK(gmb_styles_style_code("nope") == 0);
        int n = gmb_styles_bold("x", out, sizeof(out));
        CHECK(n > 0);
        CHECK(strncmp(out, "\x1b[1m", 4) == 0);
        CHECK(strcmp(out + n - 4, "\x1b[0m") == 0);
        n = gmb_styles_dim("y", out, sizeof(out));
        CHECK(strncmp(out, "\x1b[2m", 4) == 0);
    }

    /* ---- gmb_tui_input: currency range/round ---- */
    {
        CHECK(gmb_currency_range_coerce(5, 0, 10) == 5.0);
        CHECK(gmb_currency_range_coerce(-1, 0, 10) == 0.0);
        CHECK(gmb_currency_range_coerce(15, 0, 10) == 10.0);
        double adjusted = gmb_currency_adjust_to_space(1234.5678, 8, 0, 1e9);
        CHECK(adjusted > 0);
    }

    /* ---- gmb_tui_datatypes: strings/natural compare ---- */
    {
        CHECK(gmb_strings_natural_compare("rfc1", "rfc9") < 0);
        CHECK(gmb_strings_natural_compare("rfc9", "rfc10") < 0);
        CHECK(gmb_strings_natural_compare("abc", "abc") == 0);
        char out[512];
        int lines;
        int n = gmb_strings_word_wrap("the quick brown fox", 10, out, sizeof(out), &lines);
        CHECK(n > 0);
        CHECK(lines == 2);
        CHECK(gmb_strings_is_printable("abc"));
        CHECK(!gmb_strings_is_printable("a\x01b"));
    }

    /* ---- gmb_tui_clipper: memo edit ---- */
    {
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_clipper_input_keyboard(st, "ab\x1b");
        char out[1024];
        gmb_clipper_memo_edit(st, "start", 1, 1, 30, 5, out, sizeof(out));
        CHECK_STR(out, "abstart");
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_clipper: achoice ---- */
    {
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        const char* items[] = { "Uno", "Due", "Tre" };
        gmb_clipper_input_keyboard(st, "\r");
        CHECK(gmb_clipper_achoice(st, 2, 2, 8, 30, items, 3) == 1);
        gmb_clipper_input_keyboard(st, "\x1b");
        CHECK(gmb_clipper_achoice(st, 2, 2, 8, 30, items, 3) == 0);
        gmb_clipper_input_keyboard(st, "\x1b[5~");
        CHECK(gmb_clipper_input_inkey(st) != 0 || 1); /* nessun crash */
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_mouse: decoder ---- */
    {
        gmb_mouse_event_t ev;
        gmb_mouse_action_t act;
        /* SGR click sinistro a (10,5) → 0-based (9,4) */
        CHECK(gmb_mouse_decode_sequence("\x1b[<0;10;5M", &ev) == 1);
        CHECK(ev.button == GMB_MB_LEFT && ev.x == 9 && ev.y == 4);
        CHECK(!ev.released && !ev.drag && ev.scroll == GMB_SCROLL_NONE);
        gmb_mouse_reset_click_state();
        CHECK(gmb_mouse_process_event(&ev, &act) == 1);
        CHECK(act.kind == GMB_MA_CLICK && act.x == 9 && act.y == 4);
        CHECK(gmb_mouse_action_inkey(&act) == GMB_MOUSE_BASE - GMB_MA_CLICK);
        /* X10: "\x1b[M !! " (6 byte) → cb=32-32=0(left), x=33-32=1, y=33-32=1 → 0-based (0,0) */
        CHECK(gmb_mouse_decode_sequence("\x1b[M\x20!!", &ev) == 1);
        CHECK(ev.button == GMB_MB_LEFT && ev.x == 0 && ev.y == 0);
        /* wheel su */
        CHECK(gmb_mouse_decode_sequence("\x1b[<64;5;5M", &ev) == 1);
        CHECK(ev.scroll == GMB_SCROLL_UP);
        gmb_mouse_reset_click_state();
        CHECK(gmb_mouse_process_event(&ev, &act) == 1);
        CHECK(act.kind == GMB_MA_SCROLLUP);
        /* release */
        CHECK(gmb_mouse_decode_sequence("\x1b[<0;5;5m", &ev) == 1);
        CHECK(ev.released);
        gmb_mouse_process_event(&ev, &act);
        CHECK(act.kind == GMB_MA_RELEASE);
        /* non-mouse */
        CHECK(gmb_mouse_decode_sequence("\x1b[A", &ev) == 0);
        /* doppio click ravvicinato */
        gmb_mouse_reset_click_state();
        CHECK(gmb_mouse_decode_sequence("\x1b[<0;3;3M", &ev) == 1);
        gmb_mouse_process_event(&ev, &act);
        CHECK(act.kind == GMB_MA_CLICK);
        gmb_mouse_process_event(&ev, &act);
        CHECK(act.kind == GMB_MA_DOUBLECLICK);
    }

    /* ---- gmb_tui_flex ---- */
    {
        gmb_flex_item_t items[3];
        for (int i = 0; i < 3; i++) gmb_flex_item_init(&items[i]);
        gmb_flex_item_set_size(&items[0], 10, 20, 0);
        gmb_flex_item_set_size(&items[1], 20, 10, 1);
        gmb_flex_item_set_size(&items[2], 10, 15, 0);
        gmb_rect_t c = { 10, 5, 50, 20 };
        gmb_flex_generate(c, items, 3, GMB_FLEX_ROW, GMB_FLEX_NOWRAP,
                          GMB_JUSTIFY_START, GMB_ALIGN_ITEMS_STRETCH, GMB_ALIGN_CONTENT_START);
        CHECK(items[0].rect.x == 10 && items[0].rect.y == 5 && items[0].rect.w == 10 && items[0].rect.h == 20);
        CHECK(items[1].rect.x == 21 && items[1].rect.y == 5 && items[1].rect.w == 30 && items[1].rect.h == 20);
        CHECK(items[2].rect.x == 52 && items[2].rect.y == 5 && items[2].rect.w == 10 && items[2].rect.h == 20);
        /* alignItems Start: cross len = crossBasis */
        for (int i = 0; i < 3; i++) { gmb_flex_item_init(&items[i]); }
        gmb_flex_item_set_size(&items[0], 10, 20, 0);
        gmb_flex_item_set_size(&items[1], 20, 10, 1);
        gmb_flex_item_set_size(&items[2], 10, 15, 0);
        gmb_flex_generate(c, items, 3, GMB_FLEX_ROW, GMB_FLEX_NOWRAP,
                          GMB_JUSTIFY_START, GMB_ALIGN_ITEMS_START, GMB_ALIGN_CONTENT_START);
        CHECK(items[1].rect.w == 30 && items[1].rect.h == 10);
        /* column center/center */
        for (int i = 0; i < 3; i++) { gmb_flex_item_init(&items[i]); }
        gmb_flex_item_set_size(&items[0], 10, 20, 0);
        gmb_flex_item_set_size(&items[1], 20, 10, 1);
        gmb_flex_item_set_size(&items[2], 10, 15, 0);
        gmb_flex_generate(c, items, 3, GMB_FLEX_COLUMN, GMB_FLEX_NOWRAP,
                          GMB_JUSTIFY_CENTER, GMB_ALIGN_ITEMS_CENTER, GMB_ALIGN_CONTENT_START);
        CHECK(items[0].rect.x == 15 && items[0].rect.y == 5 && items[0].rect.w == 10 && items[0].rect.h == 20);
        CHECK(items[1].rect.x == 10 && items[1].rect.y == 26 && items[1].rect.w == 20 && items[1].rect.h == 0);
        CHECK(items[2].rect.x == 15 && items[2].rect.y == 27 && items[2].rect.w == 10 && items[2].rect.h == 15);
    }

    /* ---- gmb_tui_console: TermUtils / Consts ---- */
    {
        char out[256];
        int n = gmb_termutils_show_cursor(true, out, sizeof(out));
        CHECK_STR(out, "\x1b[?25h");
        n = gmb_termutils_show_cursor(false, out, sizeof(out));
        CHECK_STR(out, "\x1b[?25l");
        gmb_termutils_alternate_screen(true, out, sizeof(out));
        CHECK_STR(out, "\x1b[?1049h");
        gmb_termutils_alternate_screen(false, out, sizeof(out));
        CHECK_STR(out, "\x1b[?1049l");
        gmb_termutils_set_title("ciao", out, sizeof(out));
        CHECK_STR(out, "\x1b]2;ciao\x1b\\");
        gmb_termutils_move_to(3, 5, out, sizeof(out));
        CHECK_STR(out, "\x1b[5;3H");
        gmb_consts_fg256(200, out, sizeof(out));
        CHECK_STR(out, "\x1b[38;5;200m");
        gmb_consts_bg_rgb(1, 2, 3, out, sizeof(out));
        CHECK_STR(out, "\x1b[48;2;1;2;3m");
        gmb_consts_cursor_goto(10, 20, out, sizeof(out));
        CHECK_STR(out, "\x1b[10;20H");
        int w, h;
        gmb_termutils_query_size(&w, &h);
        CHECK(w == 80 && h == 24); /* default senza env */
        /* inkey adapter: mouse → inkey */
        gmb_mouse_action_t ma;
        ma.kind = GMB_MA_CLICK; ma.x = 3; ma.y = 4; ma.mods = GMB_MODKEY_NONE;
        gmb_ui_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = GMB_UIEVENT_MOUSE;
        ev.mouse = ma;
        CHECK(gmb_inkey_to_event(&ev) == GMB_MOUSE_BASE - GMB_MA_CLICK);
        ev.type = GMB_UIEVENT_KEY;
        ev.key = GMB_K_ENTER;
        CHECK(gmb_inkey_to_event(&ev) == GMB_K_ENTER);
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_ui_event_t outev;
        gmb_inkey_to_uievent(GMB_MOUSE_BASE - GMB_MA_SCROLLUP, st, &outev);
        CHECK(outev.type == GMB_UIEVENT_MOUSE && outev.mouse.kind == GMB_MA_SCROLLUP);
        gmb_inkey_to_uievent(65, st, &outev);
        CHECK(outev.type == GMB_UIEVENT_KEY && outev.key == 65);
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_clipper: ClipperMouse ---- */
    {
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        CHECK(!gmb_clipper_mouse_is_mouse(st));
        gmb_mouse_action_t ma;
        ma.kind = GMB_MA_CLICK; ma.x = 7; ma.y = 3; ma.mods = GMB_MODKEY_NONE;
        gmb_clipper_mouse_update_from_event(st, &ma);
        CHECK(gmb_clipper_mouse_is_mouse(st));
        CHECK(gmb_clipper_mouse_mleft(st));
        CHECK(gmb_clipper_mouse_mleftpress(st));
        CHECK(gmb_clipper_mouse_mrow(st) == 3);
        CHECK(gmb_clipper_mouse_mcol(st) == 7);
        CHECK(gmb_clipper_mouse_mousex(st) == 7);
        CHECK(gmb_clipper_mouse_mousey(st) == 3);
        CHECK(gmb_clipper_mouse_mousebutton(st, 0, 0) == 1);
        CHECK(gmb_clipper_mouse_mousebutton(st, 1, 0) == 1);
        CHECK(gmb_clipper_mouse_mousebutton(st, 1, 1) == 0);
        int row, col, mods;
        CHECK(gmb_clipper_mouse_moustat(st, 1, &row, &col, &mods));
        CHECK(row == 3 && col == 7);
        CHECK(gmb_clipper_mouse_mouseclick_area(st, 5, 2, 9, 4));
        CHECK(!gmb_clipper_mouse_mouseclick_area(st, 10, 10, 20, 20));
        char rel[8];
        gmb_clipper_mouse_mrelease(st, rel, sizeof(rel));
        CHECK_STR(rel, "L");
        CHECK(!gmb_clipper_mouse_mleft(st));
        /* scroll */
        ma.kind = GMB_MA_SCROLLDOWN; ma.x = 1; ma.y = 1;
        gmb_clipper_mouse_update_from_event(st, &ma);
        CHECK(gmb_clipper_mouse_mousewheel(st) == -1);
        ma.kind = GMB_MA_SCROLLUP;
        gmb_clipper_mouse_update_from_event(st, &ma);
        CHECK(gmb_clipper_mouse_mousewheel(st) == 1);
        /* mouse cursor: "hidden" → non visibile, "on" → visibile */
        int r = 10, c = 20;
        CHECK(gmb_clipper_mouse_mousecursor(st, &r, &c, "hidden") == false);
        CHECK(gmb_clipper_mouse_mousecursor(st, NULL, NULL, "on") == true);
        CHECK(gmb_clipper_mouse_mrow(st) == 10 && gmb_clipper_mouse_mcol(st) == 20);
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_form: ClipperForm headless ---- */
    {
        g_apply1[0] = '\0'; g_apply2[0] = '\0';
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_clipper_form_t form;
        gmb_clipper_form_init(&form, st);
        gmb_form_input_t in0, in1;
        gmb_form_input_init(&in0);
        gmb_form_input_set_value(&in0, "");
        in0.ctrl.row = 1; in0.ctrl.col = 1; in0.ctrl.width = 10; in0.ctrl.height = 1;
        in0.ctrl.visible = true; in0.ctrl.enabled = true;
        gmb_form_input_init(&in1);
        gmb_form_input_set_value(&in1, "");
        in1.ctrl.row = 2; in1.ctrl.col = 1; in1.ctrl.width = 10; in1.ctrl.height = 1;
        in1.ctrl.visible = true; in1.ctrl.enabled = true;
        gmb_clipper_form_add(&form, GMB_FIELD_INPUT, &in0);
        gmb_clipper_form_add(&form, GMB_FIELD_INPUT, &in1);
        gmb_clipper_form_set_field_apply(&form, 0, apply1, NULL);
        gmb_clipper_form_set_field_apply(&form, 1, apply2, NULL);
        gmb_clipper_input_keyboard(st, "ab\tc\r");
        int rc = gmb_clipper_form_run(&form);
        CHECK(rc == GMB_K_ENTER);
        CHECK_STR(g_apply1, "ab");
        CHECK_STR(g_apply2, "c");
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_form: FormDatePicker ---- */
    {
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_form_datepicker_t dp;
        gmb_form_datepicker_init(&dp);
        dp.ctrl.row = 1; dp.ctrl.col = 1; dp.ctrl.width = 12; dp.ctrl.height = 1;
        dp.day = 25; dp.month = 12; dp.year = 2024;
        char out[64];
        gmb_form_datepicker_format_date(&dp, st, out, sizeof(out));
        CHECK_STR(out, "12-25-2024");
        gmb_clipper_set_date_format(st, "dd/mm/yyyy");
        gmb_form_datepicker_format_date(&dp, st, out, sizeof(out));
        CHECK_STR(out, "25/12/2024");
        CHECK(gmb_form_datepicker_is_valid_date(&dp));
        /* SPACE apre il popup; LEFT sposta il giorno indietro */
        CHECK(gmb_form_datepicker_process_key(&dp, st, ' '));
        CHECK(dp.open);
        CHECK(gmb_form_datepicker_process_key(&dp, st, GMB_K_LEFT));
        CHECK(dp.day == 24 && dp.month == 12 && dp.year == 2024);
        /* RIGHT torna a 25 */
        CHECK(gmb_form_datepicker_process_key(&dp, st, GMB_K_RIGHT));
        CHECK(dp.day == 25);
        /* ESC chiude senza modifiche */
        CHECK(gmb_form_datepicker_process_key(&dp, st, GMB_K_ESC));
        CHECK(!dp.open);
        /* field key: digiti e UP/DOWN */
        CHECK(gmb_form_datepicker_process_key(&dp, st, GMB_K_UP));
        CHECK(dp.day == 26);
        CHECK(gmb_form_datepicker_process_key(&dp, st, GMB_K_DOWN));
        CHECK(dp.day == 25);
        /* draw e draw_popup non crashano */
        gmb_screen_buffer_t* buf = gmb_screen_new(80, 24);
        gmb_form_datepicker_draw(&dp, buf, GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK, true, true, true);
        dp.open = true;
        gmb_form_datepicker_draw_popup(&dp, buf, GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK);
        gmb_form_datepicker_draw_popup(&dp, buf, GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK);
        gmb_screen_free(buf);
        gmb_form_datepicker_free(&dp);
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_render: Colorize ---- */
    {
        char out[128];
        int n = gmb_colorize_sgr(true, false, false, false, false, GMB_CELLCOLOR_DEFAULT,
                                 false, GMB_CELLCOLOR_DEFAULT, out, sizeof(out));
        CHECK_STR(out, "\x1b[1m");
        n = gmb_colorize_sgr(false, false, false, false, true, (gmb_cell_color_t){255,0,0,0},
                             false, GMB_CELLCOLOR_DEFAULT, out, sizeof(out));
        CHECK_STR(out, "\x1b[38;2;255;0;0m");
        n = gmb_colorize_sgr(false, false, false, false, false, GMB_CELLCOLOR_DEFAULT,
                             false, GMB_CELLCOLOR_DEFAULT, out, sizeof(out));
        CHECK_STR(out, "\x1b[0m");
        /* ansi256 roundtrip */
        int idx = gmb_colorize_rgb_to_ansi256((gmb_cell_color_t){200, 30, 30, 0});
        gmb_cell_color_t rgb = gmb_colorize_ansi256_to_rgb(idx);
        CHECK(rgb.r >= 0 && rgb.r <= 255);
        /* rosso puro → vicino a 196 */
        int red = gmb_colorize_rgb_to_ansi256((gmb_cell_color_t){255,0,0,0});
        CHECK(red >= 16 && red <= 255);
    }

    /* ---- gmb_tui_render: DoubleBuffer ---- */
    {
        gmb_screen_buffer_t* a = gmb_screen_new(4, 2);
        gmb_screen_buffer_t* b = gmb_screen_new(4, 2);
        gmb_double_buffer_t db;
        gmb_double_buffer_new(&db);
        gmb_screen_setchar(a, 0, 0, 'X', GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK);
        gmb_screen_setchar(b, 0, 0, 'Y', GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLACK);
        /* primo diff: tutte le celle (prev null) */
        gmb_cell_change_t changes[GMB_DIFF_MAX];
        int cnt = gmb_double_buffer_compute_diff(&db, a, changes, GMB_DIFF_MAX);
        CHECK(cnt == 8);
        /* present aggiorna prev */
        char out[4096];
        int n = gmb_double_buffer_present(&db, a, out, sizeof(out));
        CHECK(n > 0);
        /* dopo present, diff tra a e a = 0 */
        cnt = gmb_double_buffer_compute_diff(&db, a, changes, GMB_DIFF_MAX);
        CHECK(cnt == 0);
        /* diff b vs a: solo (0,0) */
        cnt = gmb_double_buffer_compute_diff(&db, b, changes, GMB_DIFF_MAX);
        CHECK(cnt == 1);
        CHECK(changes[0].x == 0 && changes[0].y == 0);
        gmb_double_buffer_invalidate(&db);
        cnt = gmb_double_buffer_compute_diff(&db, b, changes, GMB_DIFF_MAX);
        CHECK(cnt == 8);
        gmb_double_buffer_free(&db);
        gmb_screen_free(a);
        gmb_screen_free(b);
    }

    /* ---- gmb_tui_cache: TinyCache ---- */
    {
        gmb_tiny_cache_t cache;
        gmb_tiny_cache_init(&cache, 3);
        CHECK(gmb_tiny_cache_capacity(&cache) == 3);
        gmb_tiny_cache_set(&cache, 1, 10);
        gmb_tiny_cache_set(&cache, 2, 20);
        gmb_tiny_cache_set(&cache, 3, 30);
        int v;
        CHECK(gmb_tiny_cache_try_get(&cache, 1, &v) && v == 10);
        CHECK(gmb_tiny_cache_is_valid(&cache, 2));
        /* superamento capacità: evicta il più vecchio (1) */
        gmb_tiny_cache_set(&cache, 4, 40);
        CHECK(!gmb_tiny_cache_is_valid(&cache, 1));
        CHECK(gmb_tiny_cache_is_valid(&cache, 4));
        gmb_tiny_cache_invalidate(&cache, 2);
        CHECK(!gmb_tiny_cache_is_valid(&cache, 2));
        gmb_tiny_cache_clear(&cache);
        CHECK(!gmb_tiny_cache_is_valid(&cache, 3));
    }

    /* ---- gmb_tui_emu: emulatore ---- */
    {
        gmb_emulator_t emu;
        gmb_emulator_init(&emu, 20, 6);
        gmb_emulator_feed(&emu, "hello\r\nworld");
        int cx, cy;
        gmb_emulator_cursor(&emu, &cx, &cy);
        CHECK(cx == 5 && cy == 1);
        CHECK(emu.grid.cells[0][0].ch == 'h');
        CHECK(emu.grid.cells[1][0].ch == 'w');
        /* CSI color */
        gmb_emulator_feed(&emu, "\x1b[31mred");
        CHECK(emu.grid.cells[1][5].ch == 'r');
        CHECK(emu.grid.cells[1][5].style.has_fg && emu.grid.cells[1][5].style.fg.r == 255);
        /* erase line */
        gmb_emulator_feed(&emu, "\x1b[2K");
        CHECK(emu.grid.cells[1][0].ch == ' ');
        /* clear */
        gmb_emulator_feed(&emu, "\x1b[2J");
        CHECK(emu.grid.cells[0][0].ch == ' ');
        /* cursor position: H → (4,2), poi Z */
        gmb_emulator_feed(&emu, "\x1b[3;5H");
        gmb_emulator_cursor(&emu, &cx, &cy);
        CHECK(cx == 4 && cy == 2);
        gmb_emulator_feed(&emu, "Z");
        CHECK(emu.grid.cells[2][4].ch == 'Z');
        /* render: la Z resta */
        gmb_screen_buffer_t* buf = gmb_screen_new(20, 6);
        gmb_emu_render_to_buffer(&emu, buf, false, 0, 0);
        CHECK(gmb_screen_cells(buf)[2 * 20 + 4].character == 'Z');
        gmb_screen_free(buf);
        /* scroll region: 2;5r = [1..4], S scrolla su di 2 → Z (riga 2) sovrascritta */
        gmb_emulator_feed(&emu, "\x1b[2;5r\x1b[2S");
        gmb_emulator_feed(&emu, "\x1b[5;5H");
        gmb_emulator_feed(&emu, "Y");
        CHECK(emu.grid.cells[4][4].ch == 'Y');
    }

    /* ---- gmb_tui_psf: font PSF1 ---- */
    {
        /* costruisci un font PSF1: header 4 byte + 256 glyph (8x8) */
        unsigned char font_data[4 + 256 * 8];
        font_data[0] = 0x36; font_data[1] = 0x04; font_data[2] = 0; font_data[3] = 8;
        for (int i = 0; i < 256 * 8; i++) font_data[4 + i] = 0;
        /* glyph 'A' (65): ogni riga ha solo il MSB acceso → pixel(0,0)=1, pixel(7,7)=0 */
        for (int r = 0; r < 8; r++) font_data[4 + 65 * 8 + r] = 0x80;
        gmb_psf_font_t font;
        CHECK(gmb_psf_from_bytes(&font, font_data, (int)sizeof(font_data)) == GMB_PSF_OK);
        CHECK(font.width == 8 && font.height == 8 && font.count == 256);
        unsigned char glyph[8];
        CHECK(gmb_psf_get_glyph(&font, 65, glyph, 8) == 8);
        CHECK(gmb_psf_get_pixel(&font, 65, 0, 0));
        CHECK(!gmb_psf_get_pixel(&font, 65, 7, 7));
        CHECK(!gmb_psf_get_pixel(&font, 200, 0, 0)); /* glyph non definito: tutto 0 */
        /* render glyph */
        gmb_screen_buffer_t* buf = gmb_screen_new(20, 10);
        gmb_psf_render_glyph(&font, 65, buf, 1, 1, NULL, NULL);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[1 * 20 + 1].character == 0x2588); /* pixel (0,0) del glyph A acceso */
        gmb_fonts_set_default(&font);
        CHECK(gmb_fonts_render_text("A", buf, 0, 0, NULL, NULL) == 1);
        gmb_screen_free(buf);
    }

    /* ---- gmb_tui_yaml: parser + config ---- */
    {
        const char* yaml =
            "title: Test Form\n"
            "window:\n"
            "  x: 2\n"
            "  y: 3\n"
            "  width: 40\n"
            "  height: 12\n"
            "  border: single\n"
            "  shadow: true\n"
            "fields:\n"
            "  - type: text\n"
            "    name: nome\n"
            "    label: Nome\n"
            "    x: 1\n"
            "    y: 1\n"
            "    max_len: 20\n"
            "    case: upper\n"
            "  - type: currency\n"
            "    name: prezzo\n"
            "    decimals: 2\n"
            "    min: 0\n"
            "  - type: list\n"
            "    name: scelta\n"
            "    items:\n"
            "      - Uno\n"
            "      - Due\n"
            "mouse:\n"
            "  enabled: true\n";
        gmb_yaml_doc_t doc;
        CHECK(gmb_yaml_parse(&doc, yaml) == 0);
        int title = gmb_yaml_node_get(&doc, doc.root, "title");
        CHECK(title >= 0);
        CHECK_STR(gmb_yaml_scalar(&doc, title), "Test Form");
        gmb_form_config_t cfg;
        gmb_yaml_form_config_from_node(&doc, doc.root, &cfg);
        CHECK_STR(cfg.title, "Test Form");
        CHECK(cfg.window.x == 2 && cfg.window.y == 3 && cfg.window.width == 40 && cfg.window.height == 12);
        CHECK(cfg.window.border == GMB_BORDER_SINGLE);
        CHECK(cfg.window.shadow);
        CHECK(cfg.field_count == 3);
        CHECK_STR(cfg.fields[0].type, "text");
        CHECK_STR(cfg.fields[0].name, "nome");
        CHECK(cfg.fields[0].max_len == 20);
        CHECK_STR(cfg.fields[0].case_, "upper");
        CHECK_STR(cfg.fields[1].type, "currency");
        CHECK(cfg.fields[1].decimals == 2);
        CHECK_STR(cfg.fields[2].type, "list");
        CHECK(cfg.fields[2].items_count == 2);
        CHECK_STR(cfg.fields[2].items[0], "Uno");
        CHECK(cfg.has_mouse && cfg.mouse.enabled);
    }

    /* ---- gmb_tui_app: BlinkClock / EventLoop / Dispatcher / ModalHost ---- */
    {
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_blink_clock_t clk;
        gmb_blink_clock_init(&clk);
        gmb_blink_clock_reset(&clk);
        CHECK(clk.blink_on);
        /* tick immediato non scatta (next_flip in futuro) */
        CHECK(!gmb_blink_clock_tick(&clk, st));

        /* EventLoop con ClipperInputSource headless */
        gmb_input_source_t src;
        gmb_clipper_input_source_init(&src, st);
        gmb_event_loop_t loop;
        gmb_event_loop_init(&loop, &src);
        gmb_clipper_input_keyboard(st, "x");
        gmb_ui_event_t ev;
        CHECK(gmb_event_loop_next(&loop, -1, &ev) == 1);
        CHECK(ev.type == GMB_UIEVENT_KEY && ev.key == 'x');

        /* EventDispatcher */
        g_fired_key = g_fired_win = g_fired_mouse = 0;
        gmb_event_dispatcher_t disp;
        gmb_event_dispatcher_init(&disp);
        gmb_event_dispatcher_register_key(&disp, 65, key_h, NULL);
        gmb_event_dispatcher_register_window(&disp, 1, win_h, NULL);
        gmb_event_dispatcher_register_mouse(&disp, 2, mouse_h, NULL);
        CHECK(gmb_event_dispatcher_dispatch_key(&disp, 65));
        CHECK(g_fired_key == 1);
        CHECK(!gmb_event_dispatcher_dispatch_key(&disp, 66));
        gmb_event_dispatcher_dispatch_window(&disp, 1);
        CHECK(g_fired_win == 1);
        gmb_event_dispatcher_dispatch_mouse(&disp, 2, 5, 6);
        CHECK(g_fired_mouse == 1);

        /* ModalHost: run_modal finché un evento invia false */
        g_modal_calls = 0;
        gmb_modal_host_t host;
        gmb_modal_host_init(&host, &src);
        gmb_modal_host_run_modal(&host, modal_fn, NULL);
        CHECK(g_modal_calls == 3);
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_widgets: Theme / FocusManager / Control / EventHandler ---- */
    {
        /* Theme */
        gmb_theme_t t = gmb_theme_named("dracula");
        CHECK(t.text.r == 248);
        CHECK(gmb_theme_named("nope").accent.r == GMB_CELLCOLOR_YELLOW.r); /* fallback turbo */
        /* ControlStyle */
        gmb_control_style_t st = gmb_control_style_from_theme(&t);
        gmb_cell_color_t fg, bg;
        gmb_control_style_effective(&st, true, false, false, &fg, &bg);
        CHECK(gmb_cellcolor_eq(fg, t.window_bg));
        gmb_control_style_effective(&st, false, true, false, &fg, &bg);
        CHECK(gmb_cellcolor_eq(bg, GMB_CELLCOLOR_RED));
        /* FocusManager */
        gmb_focus_manager_t fm;
        gmb_focus_manager_init(&fm);
        gmb_focus_manager_register_rect(&fm, "a", (gmb_rect_t){0, 0, 10, 1});
        gmb_focus_manager_register_rect(&fm, "b", (gmb_rect_t){0, 2, 10, 1});
        gmb_focus_manager_register_rect(&fm, "c", (gmb_rect_t){0, 4, 10, 1});
        CHECK_STR(gmb_focus_manager_current_id(&fm), "a");
        CHECK_STR(gmb_focus_manager_next(&fm), "b");
        CHECK_STR(gmb_focus_manager_previous(&fm), "a");
        CHECK(gmb_focus_manager_set_focus_by_point(&fm, 5, 4));
        CHECK_STR(gmb_focus_manager_current_id(&fm), "c");
        gmb_focus_manager_bind_key(&fm, "ctrl+s", "save");
        CHECK_STR(gmb_focus_manager_resolve_key(&fm, "CTRL+S"), "save");
        CHECK(gmb_focus_manager_handle_key(&fm, 9)); /* Tab */
        /* Damage */
        gmb_damage_tracker_t dt;
        gmb_damage_tracker_init(&dt, 10);
        gmb_damage_tracker_mark(&dt, 3);
        gmb_damage_tracker_mark(&dt, 5);
        int s_, e_;
        gmb_damage_tracker_dirty_rows(&dt, &s_, &e_);
        CHECK(s_ == 3 && e_ == 6);
        gmb_damage_tracker_clear(&dt);
        CHECK(!gmb_damage_tracker_is_dirty(&dt));
        /* EventHandler con un controllo semplice */
        g_drew = false;
        gmb_control_t ctrl;
        gmb_control_init(&ctrl, "ctrl", 0, 0, 10, 1, ctrl_draw, ctrl_key);
        gmb_ctrl_event_handler_t h;
        gmb_ctrl_event_handler_init(&h);
        gmb_ctrl_event_handler_register(&h, "ctrl", &ctrl);
        gmb_ui_event_t ev;
        ev.type = GMB_UIEVENT_KEY;
        ev.key = 'x';
        CHECK(gmb_ctrl_event_handler_dispatch(&h, &ev));
        ev.key = 'y';
        CHECK(!gmb_ctrl_event_handler_dispatch(&h, &ev));
        CHECK(gmb_ctrl_event_handler_is_quit(&ev) == false);
        ev.key = GMB_K_ESC;
        CHECK(gmb_ctrl_event_handler_is_quit(&ev));
        CHECK(gmb_ctrl_event_handler_event_to_navigation(&ev) == 8); /* Cancel */
    }

    /* ---- gmb_tui_concodes: encoder ---- */
    {
        char out[256];
        gmb_concodes_cuu(3, out, sizeof(out));
        CHECK_STR(out, "\x1b[3A");
        gmb_concodes_cup(5, 7, out, sizeof(out));
        CHECK_STR(out, "\x1b[5;7H");
        gmb_concodes_ed(2, out, sizeof(out));
        CHECK_STR(out, "\x1b[2J");
        gmb_concodes_sgr_fg256(200, out, sizeof(out));
        CHECK_STR(out, "\x1b[38;5;200m");
        gmb_concodes_sgr_fg24(10, 20, 30, out, sizeof(out));
        CHECK_STR(out, "\x1b[38;2;10;20;30m");
        gmb_concodes_sgr_fg((gmb_cell_color_t){255,0,0,0}, out, sizeof(out));
        CHECK_STR(out, "\x1b[31m");
        gmb_concodes_sgr_fg((gmb_cell_color_t){200,10,10,0}, out, sizeof(out));
        CHECK_STR(out, "\x1b[38;2;200;10;10m");
        gmb_concodes_set_title("ciao", out, sizeof(out));
        CHECK_STR(out, "\x1b]2;ciao\x07");
        gmb_concodes_save_cursor(out, sizeof(out));
        CHECK_STR(out, "\x1b" "7");
        gmb_concodes_scroll_region(2, 20, out, sizeof(out));
        CHECK_STR(out, "\x1b[2;20r");
        gmb_concodes_delete_chars(3, out, sizeof(out));
        CHECK_STR(out, "\x1b[3P");
    }

    /* ---- gmb_tui_concodes: parser (state machine DEC) ---- */
    {
        gmb_parser_t p;
        gmb_parser_init(&p, true);
        gmb_sequence_t seqs[32];
        gmb_parser_advance(&p, "ab", false);
        CHECK(gmb_parser_pending(&p) == 2);
        int n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 2);
        CHECK(seqs[0].type == GMB_SEQ_PRINT && seqs[0].ch == 'a');
        CHECK(seqs[1].type == GMB_SEQ_PRINT && seqs[1].ch == 'b');
        /* CSI SGR */
        gmb_parser_advance(&p, "\x1b[31;1m", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 1 && seqs[0].type == GMB_SEQ_SGR && seqs[0].sgr_count == 2);
        CHECK(seqs[0].sgr[0].type == GMB_SA_FG && seqs[0].sgr[0].color.type == GMB_SGRCOL_INDEXED && seqs[0].sgr[0].color.index == 1);
        CHECK(seqs[0].sgr[1].type == GMB_SA_BOLD);
        /* SGR 256 */
        gmb_parser_advance(&p, "\x1b[38;5;196m", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 1 && seqs[0].sgr_count == 1 && seqs[0].sgr[0].color.index == 196);
        /* SGR RGB */
        gmb_parser_advance(&p, "\x1b[38;2;1;2;3m", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 1 && seqs[0].sgr[0].color.type == GMB_SGRCOL_RGB && seqs[0].sgr[0].color.r == 1 && seqs[0].sgr[0].color.b == 3);
        /* cursor position */
        gmb_parser_advance(&p, "\x1b[10;20H", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 1 && seqs[0].type == GMB_SEQ_CURSOR_POSITION && seqs[0].x == 20 && seqs[0].y == 10);
        /* erase + scroll region */
        gmb_parser_advance(&p, "\x1b[2J\x1b[3;8r", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 2);
        CHECK(seqs[0].type == GMB_SEQ_ERASE_IN_DISPLAY && seqs[0].n1 == 2);
        CHECK(seqs[1].type == GMB_SEQ_SET_SCROLL_REGION && seqs[1].n1 == 3 && seqs[1].n2 == 8);
        /* save/restore cursore + soft reset */
        gmb_parser_advance(&p, "\x1b" "7" "\x1b" "8" "\x1b" "c", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 3);
        CHECK(seqs[0].type == GMB_SEQ_SAVE_CURSOR);
        CHECK(seqs[1].type == GMB_SEQ_RESTORE_CURSOR);
        CHECK(seqs[2].type == GMB_SEQ_SOFT_RESET);
        /* priv mode */
        gmb_parser_advance(&p, "\x1b[?25l", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 1 && seqs[0].type == GMB_SEQ_RESET_MODE && seqs[0].mode == GMB_TM_CURSOR_VISIBLE);
        /* OSC */
        gmb_parser_advance(&p, "\x1b]2;titolo\x07", false);
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 1 && seqs[0].type == GMB_SEQ_OSC && seqs[0].osc.type == GMB_OSC_SET_TITLE);
        CHECK_STR(seqs[0].osc.text, "titolo");
        /* UTF-8 multibyte */
        gmb_parser_advance(&p, "cè", false); /* 'c' + 'è' */
        n = gmb_parser_drain(&p, seqs, 32);
        CHECK(n == 2);
        CHECK(seqs[1].type == GMB_SEQ_PRINT && seqs[1].ch == (char)0xE8);
    }

    /* ---- gmb_tui_datatypes: Iters ---- */
    {
        int src[5] = {1, 2, 3, 4, 5};
        int out[8];
        int n = gmb_iters_take_until_last_n(src, 5, 2, out, 8);
        CHECK(n == 3 && out[0] == 1 && out[2] == 3);
        n = gmb_iters_take_until_last_n(src, 5, 0, out, 8);
        CHECK(n == 5);
        n = gmb_iters_take_until_last_n(src, 3, 5, out, 8);
        CHECK(n == 0);
        int a[2] = {10, 20}, b[1] = {30};
        const int* lists[2] = { a, b };
        int lens[2] = { 2, 1 };
        n = gmb_iters_flatten_results(lists, lens, 2, out, 8);
        CHECK(n == 3 && out[0] == 10 && out[1] == 20 && out[2] == 30);
    }

    /* ---- gmb_tui_datatypes: Regexes ---- */
    {
        char out[4][128];
        int n = gmb_regexes_matches(GMB_REGEX_IPV4, "ip 192.168.1.1 e 10.0.0.1", out, 4);
        CHECK(n == 2);
        CHECK_STR(out[0], "192.168.1.1");
        CHECK_STR(out[1], "10.0.0.1");
        n = gmb_regexes_matches(GMB_REGEX_HEXCOLOR, "colori #ff0000 e #0f0", out, 4);
        CHECK(n == 2);
        n = gmb_regexes_matches(GMB_REGEX_EMAIL, "scrivi a pippo@ex.com", out, 4);
        CHECK(n == 1);
        CHECK_STR(out[0], "pippo@ex.com");
    }

    /* ---- gmb_tui_events / GetBuilder ---- */
    {
        gmb_form_numeric_event_args_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.current_value = 3.5;
        ev.min = 0;
        ev.max = 10;
        CHECK(ev.current_value == 3.5 && ev.max == 10);
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        int g = gmb_clipper_read_add_field(st, 2, 3, "abc", NULL, false, 0, 0);
        CHECK(g == 0);
        gmb_clipper_read_set_field_colors(st, 0, "W/N", "N/W", "R/W", "ciao");
        gmb_get_t* gp = gmb_clipper_read_get_ptr(st, 0);
        CHECK_STR(gp->color, "W/N");
        CHECK_STR(gp->message, "ciao");
        char out[256];
        gmb_clipper_get_builder(2, 3, "abc", out, sizeof(out));
        CHECK_STR(out, "2;3;abc");
        gmb_clipper_state_free(st);
    }

    /* ---- gmb_tui_inputs: DateFormat / Keys / InputMask / Support / DateTime / History / Manager ---- */
    {
        char out[128];
        gmb_input_dateformat_template(GMB_DF_ITALIAN, true, out, sizeof(out));
        CHECK_STR(out, "dd-mm-yyyy");
        gmb_input_dateformat_template(GMB_DF_AMERICAN, false, out, sizeof(out));
        CHECK_STR(out, "mm/dd/yy");
        gmb_input_dateformat_chrono(GMB_DF_JAPAN, true, out, sizeof(out));
        CHECK_STR(out, "%Y.%m.%d");
        CHECK(gmb_input_dateformat_separator(GMB_DF_ITALIAN) == '-');
        CHECK(gmb_input_dateformat_separator(GMB_DF_BRITISH) == '/');
        gmb_last_key_set(65);
        CHECK(gmb_last_key_get() == 65);
        CHECK(gmb_keys_key_name("Enter") == GMB_K_ENTER);
        CHECK(gmb_keys_key_name("esc") == GMB_K_ESC);
        CHECK(gmb_keys_matches_char(65, 'A'));
        CHECK(gmb_keys_matches_name(GMB_K_UP, "up"));
        CHECK((gmb_inputmask_default() & GMB_IM_EXIT_ON_ENTER) != 0);
        CHECK(gmb_support_adjust_text_len(5, 0) == 20);
        CHECK(gmb_support_adjust_text_len(25, 0) == 25);
        CHECK(gmb_support_adjust_text_len(5, 8) == 8);
        CHECK(gmb_support_adjust_key_mask(GMB_IM_NONE) != GMB_IM_NONE);
        /* DateTimeInput */
        gmb_datetime_parse_long_date("2024-12-25", out, sizeof(out));
        CHECK_STR(out, "25/12/2024");
        gmb_datetime_parse_short_date("2024-12-25", out, sizeof(out));
        CHECK_STR(out, "25/12/24");
        int maxlen;
        gmb_datetime_parse_date_and_max_len("2024-12-25", false, out, sizeof(out), &maxlen);
        CHECK(maxlen == 8);
        gmb_datetime_now_string("HH:MM:ss", out, sizeof(out));
        CHECK(strlen(out) == 8);
        /* History */
        gmb_basic_history_t hist;
        gmb_basic_history_init(&hist, 0);
        gmb_basic_history_add(&hist, "primo");
        gmb_basic_history_add(&hist, "secondo");
        CHECK(gmb_basic_history_count(&hist) == 2);
        char e[GMB_HISTORY_ENTRY];
        gmb_basic_history_previous(&hist, e, sizeof(e));
        CHECK_STR(e, "secondo");
        gmb_basic_history_previous(&hist, e, sizeof(e));
        CHECK_STR(e, "primo");
        gmb_basic_history_next(&hist, e, sizeof(e));
        CHECK_STR(e, "secondo");
        char m[4][GMB_HISTORY_ENTRY];
        CHECK(gmb_basic_history_search(&hist, "pr", m, 4) == 1);
        CHECK_STR(m[0], "primo");
        /* InputField / InputManager */
        gmb_input_field_t f;
        gmb_input_field_text(&f, 1, 2, "abc", "abc", 1, 10, true, GMB_IM_EXIT_ON_ENTER, true);
        CHECK(f.kind == GMB_IF_TEXT && f.x == 1 && f.y == 2);
        CHECK_STR(f.value, "abc");
        gmb_input_manager_t man;
        gmb_input_manager_init(&man);
        gmb_input_manager_add(&man, &f);
        gmb_input_field_date(&f, 3, 4, "25/12/2024", true, false, GMB_IM_NONE, false);
        gmb_input_manager_add(&man, &f);
        CHECK(man.field_count == 2);
        CHECK(man.fields[1].kind == GMB_IF_DATE);
    }

    /* ---- gmb_tui_io: RawTerminal / DecodeCsiKey ---- */
    {
        CHECK(gmb_raw_terminal_is_platform_supported());
        gmb_raw_terminal_enable_raw();
        gmb_raw_terminal_disable_raw();
        CHECK(gmb_terminal_reader_decode_csi_key("\x1b[A") == GMB_K_UP);
        CHECK(gmb_terminal_reader_decode_csi_key("\x1b[C") == GMB_K_RIGHT);
        CHECK(gmb_terminal_reader_decode_csi_key("\x1b[Z") == GMB_K_BTAB);
        CHECK(gmb_terminal_reader_decode_csi_key("\x1b[H") == GMB_K_HOME);
        CHECK(gmb_terminal_reader_decode_csi_key("x") == 0);
        gmb_raw_input_reader_shutdown();
    }

    /* ---- gmb_tui_widgets: widget ---- */
    {
        gmb_button_t btn;
        gmb_button_init(&btn, 0, 0, 12, "OK");
        char line[128];
        gmb_button_render_line(&btn, line, sizeof(line));
        CHECK_STR(line, "[    OK    ]");
        g_clicked = 0;
        gmb_button_set_onclick(&btn, on_click, NULL);
        gmb_button_t* b = &btn;
        CHECK(gmb_control_handle_key(&b->ctrl, GMB_K_ENTER));
        CHECK(g_clicked == 1);
        CHECK(!gmb_control_handle_key(&b->ctrl, 'x'));
        CHECK(gmb_control_handle_input(&b->ctrl, ' '));
        CHECK(g_clicked == 2);

        gmb_checkbox_t cb;
        gmb_checkbox_init(&cb, 0, 0, 10, "Tutto");
        gmb_checkbox_render_line(&cb, line, sizeof(line));
        CHECK_STR(line, "[ ] Tutto ");
        gmb_checkbox_set_checked(&cb, true);
        gmb_checkbox_render_line(&cb, line, sizeof(line));
        CHECK_STR(line, "[X] Tutto ");
        CHECK(gmb_control_handle_key(&cb.ctrl, ' '));
        CHECK(cb.ival == 0);

        gmb_label_t lbl;
        gmb_label_init(&lbl, 0, 0, 10, "abc");
        lbl.align = 1;
        gmb_label_render_line(&lbl, line, sizeof(line));
        CHECK_STR(line, "   abc    ");

        gmb_progress_bar_t pb;
        gmb_progress_bar_init(&pb, 0, 0, 12);
        gmb_progress_bar_set(&pb, 50);
        gmb_progress_bar_render_line(&pb, line, sizeof(line));
        CHECK(strchr(line, '[') != NULL && strchr(line, ']') != NULL);

        gmb_numeric_updown_t nu;
        gmb_numeric_updown_init(&nu, 0, 0, 10);
        gmb_numeric_updown_set_range(&nu, 0, 100);
        gmb_numeric_updown_set_value(&nu, 5);
        CHECK((int)nu.ival == 5);
        CHECK(gmb_control_handle_key(&nu.ctrl, GMB_K_UP));
        CHECK((int)nu.ival == 6);
        CHECK(gmb_control_handle_key(&nu.ctrl, GMB_K_LEFT));
        CHECK((int)nu.ival == 5);
        CHECK(gmb_control_handle_key(&nu.ctrl, '5'));
        CHECK((int)nu.ival == 55);

        gmb_radio_group_t rg;
        gmb_radio_group_init(&rg, 0, 0, 12, 3);
        const char* opts[3] = { "Uno", "Due", "Tre" };
        gmb_radio_group_set_options(&rg, opts, 3);
        CHECK_STR(gmb_radio_group_selected_value(&rg), "Uno");
        gmb_radio_group_render_line(&rg, 0, line, sizeof(line));
        CHECK(strstr(line, "(•)") != NULL);
        CHECK(gmb_control_handle_key(&rg.ctrl, GMB_K_DOWN));
        CHECK_STR(gmb_radio_group_selected_value(&rg), "Due");

        gmb_combobox_t co;
        gmb_combobox_init(&co, 0, 0, 10);
        gmb_combobox_set_items(&co, opts, 3);
        CHECK_STR(gmb_combobox_selected_value(&co), "Uno");
        CHECK(gmb_control_handle_key(&co.ctrl, GMB_K_DOWN));
        CHECK_STR(gmb_combobox_selected_value(&co), "Due");

        gmb_listbox_t lb;
        gmb_listbox_init(&lb, 0, 0, 10, 3);
        gmb_listbox_set_items(&lb, opts, 3);
        CHECK(gmb_control_handle_key(&lb.ctrl, GMB_K_DOWN));
        CHECK_STR(gmb_listbox_selected_value(&lb), "Due");
        CHECK(gmb_control_handle_key(&lb.ctrl, GMB_K_END));
        CHECK_STR(gmb_listbox_selected_value(&lb), "Tre");

        gmb_tab_t tab;
        gmb_tab_init(&tab, 0, 0, 20);
        gmb_tab_set_tabs(&tab, opts, 3);
        CHECK(gmb_control_handle_key(&tab.ctrl, GMB_K_RIGHT));
        CHECK((int)tab.ival == 1);

        gmb_panel_t panel;
        gmb_panel_init(&panel, 1, 1, 10, 5, "Titolo");
        CHECK(panel.ctrl.width == 10 && panel.ctrl.height == 5);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&panel.ctrl, buf);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[1 * 40 + 1].character == 0x250C);
        gmb_screen_free(buf);

        /* render dei widget su buffer */
        gmb_screen_buffer_t* buf2 = gmb_screen_new(40, 10);
        gmb_control_draw(&btn.ctrl, buf2);
        gmb_control_draw(&cb.ctrl, buf2);
        gmb_control_draw(&lbl.ctrl, buf2);
        gmb_control_draw(&pb.ctrl, buf2);
        gmb_control_draw(&co.ctrl, buf2);
        gmb_control_draw(&lb.ctrl, buf2);
        gmb_control_draw(&tab.ctrl, buf2);
        gmb_screen_free(buf2);
    }

    /* ---- gmb_tui_widgets: ScrollView / Form / StatusBar / Clipboard ---- */
    {
        gmb_scroll_view_t sv;
        gmb_scroll_view_init(&sv, 0, 0, 10, 3);
        const char* lines[5] = { "l0", "l1", "l2", "l3", "l4" };
        gmb_scroll_view_set_content(&sv, lines, 5);
        CHECK(gmb_control_handle_key(&sv.ctrl, GMB_K_DOWN));
        CHECK(sv.offset == 1);
        CHECK(gmb_control_handle_key(&sv.ctrl, GMB_K_END));
        CHECK(sv.offset == 2);
        CHECK(gmb_control_handle_key(&sv.ctrl, GMB_K_HOME));
        CHECK(sv.offset == 0);
        CHECK(gmb_scroll_view_scroll_fraction(&sv) == 0.0);
        char line[64];
        gmb_scroll_view_render_line(&sv, 0, line, sizeof(line));
        CHECK(strstr(line, "l0") != NULL);

        gmb_form_widget_t form;
        gmb_form_widget_init(&form, 0, 0, 20, 4);
        gmb_form_widget_add_field(&form, "Nome", "Gino");
        gmb_form_widget_add_field(&form, "Citta", "");
        CHECK_STR(gmb_form_widget_get_value(&form, 0), "Gino");
        CHECK(gmb_control_handle_key(&form.ctrl, GMB_K_DOWN));
        CHECK(form.selected_index == 1);
        CHECK(gmb_control_handle_key(&form.ctrl, GMB_K_ENTER));
        CHECK(form.fields[1].editing);
        CHECK(gmb_control_handle_key(&form.ctrl, 'R'));
        CHECK(gmb_control_handle_key(&form.ctrl, 'M'));
        CHECK_STR(gmb_form_widget_get_value(&form, 1), "RM");
        gmb_form_widget_render_line(&form, 1, line, sizeof(line));
        CHECK(strstr(line, "RM") != NULL);

        gmb_status_bar_t sb;
        gmb_status_bar_init(&sb, 0, 0, 12);
        gmb_status_bar_set_text(&sb, "pronto");
        gmb_status_bar_render_line(&sb, line, sizeof(line));
        CHECK(strstr(line, "pronto") != NULL);

        CHECK(gmb_clipboard_is_image_path("logo.png"));
        CHECK(gmb_clipboard_is_image_path("foto.JPG"));
        CHECK(!gmb_clipboard_is_image_path("doc.txt"));
        CHECK(gmb_clipboard_try_get_text() == NULL);

        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&sv.ctrl, buf);
        gmb_control_draw(&form.ctrl, buf);
        gmb_control_draw(&sb.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- gmb_tui_widgets: AnsiStr ---- */
    {
        char out[256];
        CHECK(gmb_ansistr_char_width('A') == 1);
        CHECK(gmb_ansistr_char_width(0x4E00) == 2);
        CHECK(gmb_ansistr_width("abc") == 3);
        CHECK(gmb_ansistr_width("\x1b[31mabc\x1b[0m") == 3);
        gmb_ansistr_truncate("abcdef", 3, out, sizeof(out));
        CHECK_STR(out, "abc\x1b[0m");
        gmb_ansistr_center("ab", 8, out, sizeof(out));
        CHECK(strlen(out) == 12); /* 3+2+reset(4)+3 */
        gmb_ansistr_cut("abcdef", 1, 4, out, sizeof(out));
        CHECK_STR(out, "bcd\x1b[0m");
    }

    /* ---- gmb_tui_widgets: Menu ---- */
    {
        gmb_menu_t menu;
        gmb_menu_init(&menu, 0, 0, 20);
        gmb_menu_item_t* file = gmb_menu_add_item(&menu, "File");
        gmb_menu_item_t* edit = gmb_menu_add_item(&menu, "Edit");
        gmb_menu_sub_item_t* open_ = gmb_menu_item_add_sub(file, "Open");
        gmb_menu_item_add_sub(file, "Save");
        gmb_menu_item_add_sub(edit, "Cut");
        CHECK(menu.nitems == 2);
        CHECK(file->sub_count == 2);
        CHECK(gmb_menu_compute_width(&menu) >= 4);
        CHECK(gmb_control_handle_key(&menu.ctrl, GMB_K_ENTER));
        CHECK(menu.open);
        CHECK(gmb_control_handle_key(&menu.ctrl, GMB_K_DOWN));
        CHECK(menu.sub_sel == 1);
        CHECK(gmb_control_handle_key(&menu.ctrl, GMB_K_RIGHT));
        CHECK((int)menu.ival == 1);
        CHECK(gmb_control_handle_key(&menu.ctrl, GMB_K_DOWN));
        CHECK_STR(gmb_menu_get_selected_label(&menu), "Cut");
        CHECK(gmb_control_handle_key(&menu.ctrl, GMB_K_ESC));
        CHECK(!menu.open);
        gmb_menu_sub_item_set_action(open_, NULL, NULL);
        /* draw senza crash */
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 12);
        menu.open = 1;
        menu.ival = 0;
        gmb_control_draw(&menu.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- gmb_tui_widgets: BoxDrawing ---- */
    {
        gmb_box_char_t cross = gmb_box_char_cross(GMB_W_NORMAL);
        CHECK(gmb_box_char_to_char(cross) == 0x253C); /* ┼ */
        gmb_box_char_t ul = gmb_box_char_upper_left(GMB_W_NORMAL);
        CHECK(gmb_box_char_to_char(ul) == 0x250C);    /* ┌ */
        gmb_box_char_t h = gmb_box_char_horizontal(GMB_W_NORMAL);
        CHECK(gmb_box_char_to_char(h) == 0x2500);     /* ─ */
        gmb_box_char_t v = gmb_box_char_vertical(GMB_W_NORMAL);
        CHECK(gmb_box_char_to_char(v) == 0x2502);     /* │ */
        gmb_box_char_t round = gmb_box_char_round();
        gmb_box_char_t rnd_ul = gmb_box_char_upper_left(GMB_W_NORMAL);
        rnd_ul.style = GMB_BS_CURVED;
        CHECK(gmb_box_char_to_char(rnd_ul) == 0x256D); /* ╭ */
        gmb_box_char_t tk = gmb_box_char_thick();
        CHECK(gmb_box_char_to_char(tk) == 0x254B);     /* ╋ */
        gmb_box_char_t ascii = gmb_box_char_upper_left(GMB_W_NORMAL);
        ascii.ascii = true;
        ascii.style = GMB_BS_CURVED;
        CHECK(gmb_box_char_to_ascii(ascii) == '.');
        ascii = gmb_box_char_cross(GMB_W_NORMAL);
        ascii.ascii = true;
        CHECK(gmb_box_char_to_ascii(ascii) == '+');
        gmb_screen_buffer_t* buf = gmb_screen_new(20, 8);
        gmb_box_drawing_draw_box(buf, 1, 1, 10, 5, gmb_box_char_thin(), NULL, NULL);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[1 * 20 + 1].character == 0x250C);
        gmb_box_widget_t bw;
        gmb_box_widget_init(&bw, 2, 2, 12, 6);
        gmb_rect_t inner = gmb_box_widget_inner(&bw);
        CHECK(inner.x == 3 && inner.y == 3 && inner.w == 10 && inner.h == 4);
        gmb_box_widget_draw(&bw, buf);
        gmb_screen_free(buf);
    }

    /* ---- gmb_tui_colors ---- */
    {
        /* ColorMath: HSL roundtrip */
        gmb_rgb_t red = gmb_rgb_from_u8(255, 0, 0);
        double h, s, l;
        gmb_colormath_rgb_to_hsl(red, &h, &s, &l);
        CHECK(fabs(h - 0.0) < 1e-9 && fabs(s - 1.0) < 1e-9 && fabs(l - 0.5) < 1e-9);
        gmb_rgb_t back = gmb_colormath_hsl_to_rgb(h, s, l);
        CHECK(fabs(back.r - 1.0) < 1e-9 && fabs(back.g) < 1e-9);
        /* CMYK */
        double cc, cm, cy, ck;
        gmb_colormath_rgb_to_cmyk(red, &cc, &cm, &cy, &ck);
        CHECK(fabs(ck) < 1e-9 && fabs(cc) < 1e-9 && fabs(cm - 1.0) < 1e-9);
        /* Mix */
        gmb_rgb_t black = gmb_rgb_from_u8(0, 0, 0);
        gmb_rgb_t mid = gmb_colormath_mix(black, red, 0.5);
        CHECK(fabs(mid.r - 0.5) < 1e-9);
        /* Gradient */
        gmb_gradient_t g;
        gmb_gradient_grayscale(&g);
        gmb_cell_color_t at0 = gmb_gradient_at(&g, 0.0);
        CHECK(at0.r == 0);
        gmb_cell_color_t at1 = gmb_gradient_at(&g, 1.0);
        CHECK(at1.r == 255);
        gmb_gradient_rainbow(&g);
        gmb_cell_color_t midc = gmb_gradient_at(&g, 0.5);
        CHECK(midc.g == 255); /* verde al centro */
        /* NamedColors */
        gmb_cell_color_t nc;
        CHECK(gmb_named_colors_get("Red", &nc));
        CHECK(nc.r == 255 && nc.g == 0);
        CHECK(gmb_named_colors_get("GOLD", &nc));
        CHECK(nc.r == 255 && nc.g == 215);
        CHECK(!gmb_named_colors_get("nonesiste", &nc));
        /* ColorDetection */
        CHECK(gmb_color_detection_from_env("xterm-256color", NULL, false, 0) == GMB_COLOR_ANSI256);
        CHECK(gmb_color_detection_from_env(NULL, "truecolor", false, 0) == GMB_COLOR_TRUECOLOR);
        CHECK(gmb_color_detection_from_env(NULL, NULL, true, 0) == GMB_COLOR_NONE);
        CHECK(gmb_color_detection_from_env(NULL, NULL, false, 4) == GMB_COLOR_TRUECOLOR);
        /* gradient samples */
        gmb_gradient_t g2;
        gmb_gradient_heat(&g2);
        gmb_cell_color_t samples[4];
        gmb_gradient_samples(&g2, 4, samples);
        CHECK(samples[0].r == 0);
        CHECK(samples[3].r == 255);
    }

    /* ---- gmb_tui_widgets: BigText ---- */
    {
        char rows[7][32];
        CHECK(gmb_bigtext_get('A', rows) == 1);
        CHECK((unsigned char)rows[0][2] == 0xE2 && (unsigned char)rows[0][3] == 0x96 && (unsigned char)rows[0][4] == 0x88);
        CHECK(gmb_bigtext_width('A') == 5);
        char block[7][512];
        gmb_bigtext_to_block("HI", block);
        CHECK(strlen(block[0]) >= 9);
        gmb_big_text_t bt;
        gmb_big_text_init(&bt, 0, 0);
        gmb_big_text_set_text(&bt, "OK");
        int w, h;
        gmb_big_text_dimensions(&bt, &w, &h);
        CHECK(h == 7 && w >= 5);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&bt.ctrl, buf);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        bool any_block = false;
        for (int i = 0; i < 400; i++) if (cells[i].character == 0x2588) { any_block = true; break; }
        CHECK(any_block);
        gmb_screen_free(buf);
    }

    /* ---- StarRating / Spinner / Slider / Dashboard / Validator ---- */
    {
        char line[256];
        gmb_star_rating_t sr;
        gmb_star_rating_init(&sr, 0, 0, 8);
        sr.vmax = 5;
        gmb_star_rating_render_line(&sr, line, sizeof(line));
        CHECK((unsigned char)line[0] == 0xE2);
        CHECK(gmb_control_handle_key(&sr.ctrl, GMB_K_RIGHT));
        CHECK((int)sr.ival == 1);
        CHECK(gmb_control_handle_key(&sr.ctrl, GMB_K_RIGHT));
        CHECK((int)sr.ival == 2);

        gmb_spinner_t spin;
        gmb_spinner_init(&spin, 0, 0, 4);
        CHECK(spin.frames_count == 4);
        CHECK_STR(spin.frames[0], "|");
        gmb_spinner_tick(&spin);
        CHECK((int)spin.ival == 1);
        gmb_spinner_set_preset(&spin, "dots");
        CHECK(spin.frames_count == 6);
        gmb_spinner_tick(&spin);
        CHECK((int)spin.ival == 1);

        gmb_slider_t sl;
        gmb_slider_init(&sl, 0, 0, 14);
        gmb_slider_set_range(&sl, 0, 100);
        gmb_slider_set_value(&sl, 50);
        CHECK((int)sl.ival == 50);
        CHECK(gmb_control_handle_key(&sl.ctrl, GMB_K_RIGHT));
        CHECK((int)sl.ival == 51);
        CHECK(gmb_control_handle_key(&sl.ctrl, GMB_K_HOME));
        CHECK((int)sl.ival == 0);
        gmb_slider_render_line(&sl, line, sizeof(line));
        CHECK(strchr(line, '[') != NULL);
        gmb_mouse_action_t ma;
        ma.kind = GMB_MA_CLICK; ma.x = 8; ma.y = 0;
        CHECK(gmb_control_handle_mouse(&sl.ctrl, &ma));
        CHECK((int)sl.ival > 0);

        gmb_dashboard_t dash;
        gmb_dashboard_init(&dash, 0, 0, 20, 3);
        gmb_dashboard_add_metric(&dash, "CPU", 50, 100, "%");
        gmb_dashboard_add_metric(&dash, "RAM", 30, 100, "%");
        CHECK(dash.metrics_count == 2);
        CHECK(gmb_control_handle_key(&dash.ctrl, GMB_K_DOWN));
        CHECK(dash.selected_index == 1);
        gmb_dashboard_render_line(&dash, 0, line, sizeof(line));
        CHECK(strstr(line, "CPU") != NULL);

        gmb_validator_t v;
        gmb_validator_int(&v, 1, 10);
        CHECK(gmb_validator_is_valid(&v, "5"));
        CHECK(!gmb_validator_is_valid(&v, "50"));
        CHECK(gmb_validator_error(&v, "abc") != NULL);
        gmb_validator_required(&v);
        CHECK(gmb_validator_is_valid(&v, "x"));
        CHECK(!gmb_validator_is_valid(&v, "  "));
        const char* opts[2] = {"a", "b"};
        gmb_validator_oneof(&v, opts, 2);
        CHECK(gmb_validator_is_valid(&v, "a"));
        CHECK(!gmb_validator_is_valid(&v, "c"));

        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&sr.ctrl, buf);
        gmb_control_draw(&spin.ctrl, buf);
        gmb_control_draw(&sl.ctrl, buf);
        gmb_control_draw(&dash.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- StyleBox / DataList ---- */
    {
        char line[256];
        gmb_border_style_t b = gmb_border_style_default();
        CHECK(b.top_left == 0x250C && b.horizontal == 0x2500);
        gmb_border_style_t thick = gmb_border_style_thick();
        CHECK(thick.top_left == 0x250F);
        gmb_box_style_t bs = gmb_box_style_new();
        bs = gmb_box_style_with_padding_all(bs, 1);
        bs = gmb_box_style_with_border(bs, b);
        bs = gmb_box_style_with_align(bs, GMB_SALIGN_CENTER);
        CHECK(bs.pad_left == 1 && bs.has_border && bs.align == GMB_ALIGN_CENTER);
        CHECK(gmb_text_measurer_measure("ab\ncd", 2) == 2);
        CHECK(gmb_text_measurer_measure("abcdef", 2) == 3);
        char lines[8][256];
        int n = gmb_text_measurer_prepare_text("abcdef", 3, lines, 8);
        CHECK(n == 2);
        CHECK_STR(lines[0], "abc");
        gmb_screen_buffer_t* buf = gmb_screen_new(20, 8);
        gmb_text_measurer_render_boxed(bs, lines, n, buf, 1, 1, 10, 5);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[1 * 20 + 1].character == 0x250C);

        gmb_data_list_t dl;
        gmb_data_list_init(&dl, 0, 0, 12, 3);
        const char* items[4] = { "alfa", "beta", "gamma", "delta" };
        gmb_data_list_set_items(&dl, items, 4);
        dl.multi_select = true;
        CHECK(gmb_control_handle_key(&dl.ctrl, GMB_K_DOWN));
        CHECK(dl.selected_index == 1);
        CHECK(gmb_control_handle_key(&dl.ctrl, ' '));
        CHECK(dl.selected_items[1]);
        CHECK(gmb_control_handle_key(&dl.ctrl, GMB_K_END));
        CHECK(dl.selected_index == 3);
        gmb_data_list_render_line(&dl, 0, line, sizeof(line));
        CHECK(strstr(line, "alfa") != NULL);
        gmb_control_draw(&dl.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- gmb_tui_charts ---- */
    {
        char out[256];
        int vals[6] = { 0, 2, 4, 6, 8, 10 };
        int n = gmb_sparkline_render(vals, 6, 6, false, 0, false, 0, out, sizeof(out));
        CHECK(n > 0);
        gmb_chartutil_format_value(5.0, out, sizeof(out));
        CHECK_STR(out, "5");
        gmb_chartutil_format_value(5.5, out, sizeof(out));
        CHECK_STR(out, "5.5");

        gmb_bar_chart_t bc;
        gmb_bar_chart_init(&bc);
        gmb_bar_chart_add(&bc, "A", 5, false, GMB_CELLCOLOR_DEFAULT);
        gmb_bar_chart_add(&bc, "B", 10, false, GMB_CELLCOLOR_DEFAULT);
        CHECK(bc.bars_count == 2);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 12);
        gmb_bar_chart_render(&bc, buf, 0, 0);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        bool has_bar = false;
        for (int i = 0; i < 480; i++) if (cells[i].character == 0x2588 || cells[i].character == 0x258F) { has_bar = true; break; }
        CHECK(has_bar);

        gmb_gauge_t g;
        gmb_gauge_init(&g, 50);
        g.show_percent = true;
        gmb_gauge_render(&g, buf, 0, 2);
        bool has_gauge = false;
        for (int i = 0; i < 480; i++) if (cells[i].character == 0x2588) { has_gauge = true; break; }
        CHECK(has_gauge);

        gmb_line_chart_t lc;
        gmb_line_chart_init(&lc);
        double sv[4] = { 1, 3, 2, 4 };
        gmb_line_chart_add_series(&lc, "s1", sv, 4, false, GMB_CELLCOLOR_DEFAULT);
        CHECK(lc.series_count == 1);
        gmb_line_chart_render(&lc, buf, 0, 4);
        bool has_braille = false;
        for (int i = 0; i < 480; i++) if (cells[i].character >= 0x2800 && cells[i].character <= 0x28FF) { has_braille = true; break; }
        CHECK(has_braille);

        gmb_histogram_t hist;
        gmb_histogram_init(&hist);
        double data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        gmb_histogram_from_data(&hist, data, 8, 4);
        CHECK(hist.bins_count == 4);
        gmb_histogram_render(&hist, buf, 0, 8);
        gmb_screen_free(buf);
    }

    /* ---- Grid / LogView / TextArea / TreeView ---- */
    {
        char line[256];
        gmb_grid_t grid;
        gmb_grid_init(&grid, 0, 0, 20, 3);
        gmb_grid_set_cols(&grid, 2);
        const char* cells[6] = { "a", "b", "c", "d", "e", "f" };
        gmb_grid_set_cells(&grid, cells, 6);
        CHECK(gmb_control_handle_key(&grid.ctrl, GMB_K_DOWN));
        CHECK(grid.cursor_row == 1);
        CHECK(gmb_control_handle_key(&grid.ctrl, GMB_K_RIGHT));
        CHECK(grid.cursor_col == 1);
        CHECK(gmb_control_handle_key(&grid.ctrl, GMB_K_ENTER));
        CHECK(grid.has_selected && grid.sel_col == 1);
        gmb_grid_render_line(&grid, 0, line, sizeof(line));
        CHECK(strstr(line, "a") != NULL);

        gmb_log_view_t lv;
        gmb_log_view_init(&lv, 0, 0, 30, 4);
        gmb_log_view_push(&lv, GMB_LOG_ERROR, "net", "timeout");
        gmb_log_view_push(&lv, GMB_LOG_INFO, "app", "ok");
        CHECK(lv.records_count == 2);
        CHECK(gmb_log_view_filtered_count(&lv) == 2);
        lv.min_level = GMB_LOG_WARN;
        CHECK(gmb_log_view_filtered_count(&lv) == 1);
        lv.min_level = GMB_LOG_TRACE;
        gmb_log_view_render_line(&lv, 0, line, sizeof(line));
        CHECK(strstr(line, "ERROR") != NULL);

        gmb_text_area_t ta;
        gmb_text_area_init(&ta, 0, 0, 20, 5);
        const char* tlines[2] = { "riga1", "riga2" };
        gmb_text_area_set_lines(&ta, tlines, 2);
        CHECK(gmb_control_handle_key(&ta.ctrl, GMB_K_DOWN));
        CHECK(ta.cursor_line == 1);
        CHECK(gmb_control_handle_key(&ta.ctrl, 'X'));
        CHECK_STR(ta.lines[1], "Xriga2");
        CHECK(ta.modified);
        gmb_text_area_render_line(&ta, 0, line, sizeof(line));
        CHECK(strstr(line, "riga1") != NULL);

        gmb_tree_view_t tv;
        gmb_tree_view_init(&tv, 0, 0, 20, 6);
        gmb_tree_view_add_node(&tv, "root", 0, true);
        gmb_tree_view_add_node(&tv, "  child1", 1, false);
        gmb_tree_view_add_node(&tv, "  child2", 1, false);
        gmb_tree_view_add_node(&tv, "altro", 0, false);
        CHECK(tv.nodes_count == 4);
        CHECK(gmb_control_handle_key(&tv.ctrl, GMB_K_DOWN));
        CHECK(tv.selected_index == 1);
        gmb_tree_view_render_line(&tv, 0, line, sizeof(line));
        CHECK(strstr(line, "root") != NULL);

        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&grid.ctrl, buf);
        gmb_control_draw(&lv.ctrl, buf);
        gmb_control_draw(&ta.ctrl, buf);
        gmb_control_draw(&tv.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- ColorPicker / TableModel ---- */
    {
        char line[256];
        gmb_color_picker_t cp;
        gmb_color_picker_init(&cp, 0, 0, 16, 4);
        CHECK(gmb_color_picker_row_count(&cp) == 16);
        CHECK(gmb_color_picker_cell_at(&cp, 0, 0) != NULL);
        CHECK(gmb_color_picker_is_cell_selected(&cp, 0, 0));
        CHECK(gmb_control_handle_key(&cp.ctrl, GMB_K_RIGHT));
        CHECK((int)cp.ival == 1);
        CHECK(gmb_control_handle_key(&cp.ctrl, GMB_K_DOWN));
        CHECK((int)cp.ival == 17);
        CHECK(gmb_control_handle_key(&cp.ctrl, GMB_K_HOME));
        CHECK((int)cp.ival == 0);
        gmb_color_picker_render_line(&cp, 0, line, sizeof(line));

        gmb_text_align_align("ab", 6, GMB_HALIGN_RIGHT, line, sizeof(line));
        CHECK_STR(line, "    ab");
        gmb_text_align_align("ab", 6, GMB_HALIGN_CENTER, line, sizeof(line));
        CHECK_STR(line, "  ab  ");

        gmb_table_columns_t cols;
        gmb_table_columns_init(&cols);
        CHECK(gmb_table_columns_add(&cols, "A") == 0);
        CHECK(gmb_table_columns_add(&cols, "B") == 1);
        CHECK(gmb_table_columns_add(&cols, "C") == 2);
        CHECK(cols.count == 3);
        CHECK(gmb_table_columns_remove(&cols, 1));
        CHECK(cols.count == 2);
        CHECK(cols.cols[1].index == 1);
        CHECK(gmb_table_columns_insert(&cols, 1, "X"));
        CHECK(cols.count == 3);
        CHECK_STR(cols.cols[1].name, "X");
        CHECK(gmb_table_columns_reorder(&cols, 2, 0));
        CHECK_STR(cols.cols[0].name, "C");

        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&cp.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- HexView / RichText ---- */
    {
        char line[256];
        unsigned char data[80];
        for (int i = 0; i < 80; i++) data[i] = (unsigned char)i;
        gmb_hex_view_t hv;
        gmb_hex_view_init(&hv, 0, 0, 50, 4);
        gmb_hex_view_set_data(&hv, data, 80);
        gmb_hex_view_render_line(&hv, 0, line, sizeof(line));
        CHECK(strstr(line, "00") != NULL);
        CHECK(gmb_control_handle_key(&hv.ctrl, GMB_K_DOWN));
        CHECK(hv.offset == 1);
        CHECK(gmb_control_handle_key(&hv.ctrl, GMB_K_HOME));
        CHECK(hv.offset == 0);

        gmb_rich_span_t spans[8];
        int n = gmb_richtext_parse("ciao *mondo*", spans, 8);
        CHECK(n == 2);
        CHECK_STR(spans[0].text, "ciao ");
        CHECK_STR(spans[1].text, "mondo");
        CHECK(spans[1].bold);
        n = gmb_richtext_parse("{red pericolo}", spans, 8);
        CHECK(n == 1);
        CHECK(spans[0].has_fg);
        CHECK(spans[0].fg.r == 255 && spans[0].fg.g == 0);
        gmb_cell_color_t pc;
        CHECK(gmb_richtext_parse_color("#10a0c0", &pc));
        CHECK(pc.r == 0x10 && pc.g == 0xa0 && pc.b == 0xc0);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_richtext_render_to_buffer(buf, 0, 0, spans, n, 20);
        gmb_control_draw(&hv.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- KeyMap / FunctionKeyBar / ImageButton / ProgressBuilder / PropertyGrid ---- */
    {
        char line[256];
        gmb_key_map_t km;
        gmb_key_map_init(&km);
        gmb_key_map_bind(&km, "ctrl+s", 42);
        CHECK(gmb_key_map_resolve(&km, "CTRL+S") == 42);
        CHECK(gmb_key_map_contains(&km, "ctrl+s"));
        CHECK(gmb_key_map_count(&km) == 1);
        gmb_key_map_clear(&km);
        CHECK(gmb_key_map_is_empty(&km));

        gmb_function_key_bar_t fkb;
        gmb_function_key_bar_init(&fkb, 0, 0, 40);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 5);
        gmb_function_key_bar_draw(&fkb, buf);
        CHECK(gmb_function_key_bar_handle_key(&fkb, GMB_K_F1));
        CHECK(gmb_function_key_bar_get_command(&fkb, GMB_K_F1 + 2) == 2);

        gmb_image_button_t ib;
        gmb_image_button_init(&ib, 0, 0, 12);
        snprintf(ib.icon, sizeof(ib.icon), "*");
        snprintf(ib.label, sizeof(ib.label), "Go");
        gmb_image_button_render_line(&ib, line, sizeof(line));
        CHECK(strstr(line, "Go") != NULL);

        gmb_bar_builder_t bb = gmb_bar_builder_new();
        bb = gmb_bar_builder_with_width(bb, 10);
        gmb_bar_render(bb, 0.5, line, sizeof(line));
        CHECK(strchr(line, '[') != NULL && strchr(line, ']') != NULL);
        gmb_bar_render_with_percent(bb, 0.5, line, sizeof(line));
        CHECK(strstr(line, "50%") != NULL);
        gmb_byte_bar_render(5, 10, 10, line, sizeof(line));
        CHECK(strchr(line, '|') != NULL);

        gmb_property_grid_t pg;
        gmb_property_grid_init(&pg, 0, 0, 20, 4);
        gmb_property_grid_add(&pg, "nome", "Gino");
        gmb_property_grid_add(&pg, "citta", "Roma");
        CHECK(pg.items_count == 2);
        CHECK(gmb_control_handle_key(&pg.ctrl, GMB_K_DOWN));
        CHECK(pg.selected_index == 1);
        CHECK(gmb_control_handle_key(&pg.ctrl, GMB_K_ENTER));
        CHECK(pg.editing);
        CHECK(gmb_control_handle_key(&pg.ctrl, 'X'));
        CHECK(gmb_control_handle_key(&pg.ctrl, GMB_K_ENTER));
        CHECK_STR(pg.values[1], "RomaX");
        gmb_property_grid_render_line(&pg, 0, line, sizeof(line));
        CHECK(strstr(line, "nome") != NULL);

        gmb_control_draw(&ib.ctrl, buf);
        gmb_control_draw(&pg.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- DatePicker ---- */
    {
        char line[256];
        gmb_date_picker_t dp;
        gmb_date_picker_init(&dp, 0, 0, 12);
        dp.day = 15; dp.month = 6; dp.year = 2024;
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_ENTER));
        CHECK(dp.open);
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_LEFT));
        CHECK(dp.day == 14);
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_RIGHT));
        CHECK(dp.day == 15);
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_ESC));
        CHECK(!dp.open);
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_ENTER));
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_DOWN));
        CHECK(dp.day == 22);
        CHECK(gmb_control_handle_key(&dp.ctrl, GMB_K_ENTER));
        CHECK(!dp.open);
        CHECK(gmb_date_picker_contains_point(&dp, 0, 0) == false);
        gmb_clipper_state_t* st = gmb_clipper_state_new(80, 24);
        gmb_clipper_set_date_format(st, "dd/mm/yyyy");
        gmb_date_picker_get_formatted(&dp, st, line, sizeof(line));
        CHECK_STR(line, "22/06/2024");
        gmb_clipper_state_free(st);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 12);
        dp.open = true;
        gmb_control_draw(&dp.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- Panel / Window / AlertBox / ConfirmationBox / ReorderListBox ---- */
    {
        char line[256];
        gmb_panel_w_t pan;
        gmb_panel_w_init(&pan, 1, 1, 10, 5);
        snprintf(pan.title, sizeof(pan.title), "P");
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&pan.ctrl, buf);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[1 * 40 + 1].character == 0x250C);
        gmb_rect_t inner = gmb_panel_inner(&pan.ctrl, GMB_PB_SINGLE);
        CHECK(inner.x == 2 && inner.y == 2 && inner.w == 8 && inner.h == 3);

        gmb_window_t win;
        gmb_window_init(&win, 1, 1, 12, 6);
        snprintf(win.title, sizeof(win.title), "Win");
        CHECK(gmb_window_title_bar_contains(&win, 3, 1));
        CHECK(gmb_window_close_button_contains(&win, 11, 1));
        gmb_control_draw(&win.ctrl, buf);

        char wrapped[4][256];
        int wn = gmb_alert_wrap_text("parola parola parola", 10, wrapped, 4);
        CHECK(wn == 3);

        gmb_confirmation_box_t conf;
        gmb_confirmation_box_init(&conf, 1, 1, 20, 6);
        CHECK(conf.focused_choice == GMB_CONF_CONFIRM);

        gmb_reorder_listbox_t rlb;
        gmb_reorder_listbox_init(&rlb, 0, 0, 12, 3);
        const char* ritems[3] = { "a", "b", "c" };
        gmb_reorder_listbox_set_items(&rlb, ritems, 3);
        CHECK(gmb_control_handle_key(&rlb.ctrl, GMB_K_DOWN));
        CHECK(rlb.selected == 1);
        rlb.ctrl_held = true;
        CHECK(gmb_control_handle_key(&rlb.ctrl, GMB_K_UP));
        CHECK_STR(rlb.items[0], "b");
        CHECK_STR(rlb.items[1], "a");
        rlb.ctrl_held = false;
        rlb.multi_select = true;
        CHECK(gmb_control_handle_key(&rlb.ctrl, ' '));
        CHECK(rlb.checked[rlb.selected]);
        gmb_reorder_listbox_render_line(&rlb, 0, line, sizeof(line));

        gmb_control_draw(&rlb.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- TextInput / HalfBlock / Base64 / Graphics ---- */
    {
        char line[256];
        gmb_text_input_t ti;
        gmb_text_input_init(&ti, 0, 0, 12);
        gmb_text_input_set_text(&ti, "abc");
        CHECK(ti.cursor == 3);
        CHECK(gmb_control_handle_key(&ti.ctrl, 'D'));
        CHECK_STR(ti.text, "abcD");
        CHECK(gmb_control_handle_key(&ti.ctrl, GMB_K_BS));
        CHECK_STR(ti.text, "abc");
        CHECK(gmb_control_handle_key(&ti.ctrl, GMB_K_LEFT));
        CHECK(ti.cursor == 2);
        gmb_text_input_render_line(&ti, line, sizeof(line));
        CHECK(strstr(line, "abc") != NULL);

        gmb_half_block_canvas_t hb;
        gmb_half_block_canvas_init(&hb, 4, 4, ' ');
        gmb_half_block_canvas_set(&hb, 0, 0, GMB_CELLCOLOR_RED);
        gmb_screen_buffer_t* buf = gmb_screen_new(20, 8);
        gmb_half_block_canvas_render(&hb, buf, 0, 0);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[0].character == 0x2580);
        gmb_half_block_canvas_free(&hb);

        unsigned char data[3] = { 0xFF, 0x00, 0x80 };
        char* enc = gmb_base64_encode(data, 3);
        CHECK(enc != NULL && strlen(enc) == 4);
        size_t dl = 0;
        uint8_t* dec = gmb_base64_decode(enc, &dl);
        CHECK(dl == 3);
        CHECK(dec && dec[0] == 0xFF && dec[2] == 0x80);
        free(enc); free(dec);
        CHECK(gmb_graphics_detect() >= GMB_GFX_NONE);
        int n = gmb_graphics_delete_image(5, line, sizeof(line));
        CHECK(n > 0);
        gmb_screen_free(buf);
    }

    /* ---- DialogBuilder / Feeder ---- */
    {
        gmb_question_t q;
        gmb_question_init(&q, "eta", GMB_QK_INT);
        gmb_answer_t ans;
        CHECK(gmb_question_resolve(&q, "25", &ans));
        CHECK(ans.has_int && ans.int_ == 25);
        gmb_question_init(&q, "nome", GMB_QK_INPUT);
        snprintf(q.default_str, sizeof(q.default_str), "Gino");
        CHECK(gmb_question_resolve(&q, "", &ans));
        CHECK(ans.has_string && strcmp(ans.str, "Gino") == 0);
        gmb_question_init(&q, "scelta", GMB_QK_SELECT);
        gmb_question_add_choice(&q, "a");
        gmb_question_add_choice(&q, "b");
        CHECK(gmb_question_resolve(&q, "b", &ans));
        CHECK(ans.int_ == 1);
        gmb_question_init(&q, "confirm", GMB_QK_CONFIRM);
        CHECK(gmb_question_resolve(&q, "y", &ans));
        CHECK(ans.bool_);
        CHECK(!gmb_question_resolve(&q, "x", &ans));

        const char* items[3] = { "Apple", "Banana", "Cherry" };
        char filtered[3][256];
        int n = gmb_feeder_filter(items, 3, "an", filtered, 3);
        CHECK(n == 1);
        CHECK_STR(filtered[0], "Banana");
        gmb_feeder_t f;
        gmb_feeder_init(&f, ".");
        CHECK(f.directory[0] == '.');
    }

    /* ---- SpreadsheetModel / TilingModel / FileDialog ---- */
    {
        char line[256];
        gmb_column_width_t cw = gmb_column_width_bound(5, 10);
        CHECK(gmb_column_width_resolve(cw, 7) == 7);
        CHECK(gmb_column_width_resolve(cw, 3) == 5);
        CHECK(gmb_column_width_resolve(cw, 20) == 15);
        gmb_cell_cursor_t cur;
        gmb_cell_cursor_init(&cur);
        gmb_cell_cursor_move_right(&cur, 3, 10, 5);
        CHECK(cur.col == 3);
        gmb_cell_cursor_toggle_select(&cur);
        CHECK(gmb_cell_cursor_is_selected(&cur, 0, 3));
        gmb_cell_cursor_column_select_all(&cur, 10);
        CHECK(gmb_cell_cursor_is_selected(&cur, 5, 3));

        gmb_tiling_model_t tm;
        gmb_tiling_model_init(&tm);
        int c1 = gmb_tiling_model_split_leaf(&tm, 0, GMB_SPLIT_HORIZONTAL, 0.5);
        CHECK(c1 == 2);
        int rects_n = 0;
        gmb_rect_t area = { 0, 0, 20, 10 };
        int ids[4];
        gmb_rect_t rects[4];
        rects_n = gmb_tiling_model_layout(&tm, area, ids, rects, 4);
        CHECK(rects_n == 2);
        CHECK(rects[0].w == 10 && rects[1].w == 10);
        gmb_tiling_model_set_view(&tm, 1, 42);
        CHECK(gmb_tiling_model_view_of(&tm, 1) == 42);
        tm.nodes[0].rect = area;
        gmb_tiling_model_move_offset(&tm, 1, 2);
        rects_n = gmb_tiling_model_layout(&tm, area, ids, rects, 4);
        CHECK(rects[0].w == 12);

        gmb_file_dialog_t fd;
        gmb_file_dialog_init(&fd, 0, 0, 20, 4);
        gmb_file_entry_t entries[3];
        snprintf(entries[0].name, sizeof(entries[0].name), "src");
        entries[0].is_dir = true;
        snprintf(entries[1].name, sizeof(entries[1].name), "readme.txt");
        entries[1].is_dir = false;
        gmb_file_dialog_set_entries(&fd, entries, 2);
        CHECK(gmb_control_handle_key(&fd.ctrl, GMB_K_DOWN));
        CHECK(fd.selected_index == 1);
        gmb_file_dialog_render_line(&fd, 0, line, sizeof(line));
        CHECK(strstr(line, "src") != NULL);
        gmb_screen_buffer_t* buf = gmb_screen_new(40, 10);
        gmb_control_draw(&fd.ctrl, buf);
        gmb_screen_free(buf);
    }

    /* ---- Pretty ---- */
    {
        char line[256];
        gmb_doc_t* d1 = gmb_doc_text("hello");
        gmb_doc_t* d2 = gmb_doc_space();
        gmb_doc_t* d3 = gmb_doc_text("world");
        gmb_doc_t* concat = gmb_doc_append(gmb_doc_append(d1, d2), d3);
        gmb_doc_render_string(concat, 80, line, sizeof(line));
        CHECK_STR(line, "hello world");
        gmb_doc_free(concat);
        /* group + soft line */
        gmb_doc_t* a = gmb_doc_text("foo");
        gmb_doc_t* soft = gmb_doc_soft_line();
        gmb_doc_t* b = gmb_doc_text("bar");
        gmb_doc_t* g = gmb_doc_group(gmb_doc_append(gmb_doc_append(a, soft), b));
        gmb_doc_render_string(g, 20, line, sizeof(line));
        CHECK_STR(line, "foo bar");
        gmb_doc_render_string(g, 5, line, sizeof(line));
        CHECK(strchr(line, '\n') != NULL);
        gmb_doc_free(g);
    }

    /* ---- BufferWidgets / FileIcons ---- */
    {
        gmb_screen_buffer_t* buf = gmb_screen_new(30, 12);
        gmb_cell_color_t cyan = GMB_CELLCOLOR_CYAN;
        gmb_border_box_t bb;
        memset(&bb, 0, sizeof(bb));
        bb.border_color = cyan;
        snprintf(bb.title, sizeof(bb.title), "Titolo");
        gmb_rect_t area = { 1, 1, 12, 6 };
        gmb_border_box_draw(&bb, buf, area);
        gmb_screen_cell_t* cells = gmb_screen_cells(buf);
        CHECK(cells[1 * 30 + 1].character == 0x250C);

        gmb_bw_list_t lst;
        memset(&lst, 0, sizeof(lst));
        snprintf(lst.items[0], 256, "uno");
        snprintf(lst.items[1], 256, "due");
        lst.items_count = 2;
        lst.selected = 1;
        lst.sel_fg = GMB_CELLCOLOR_BLACK;
        lst.sel_bg = GMB_CELLCOLOR_CYAN;
        gmb_rect_t larea = { 1, 1, 10, 4 };
        gmb_bw_list_draw(&lst, buf, larea);

        gmb_bw_tabs_t tabs;
        memset(&tabs, 0, sizeof(tabs));
        snprintf(tabs.titles[0], 64, "File");
        snprintf(tabs.titles[1], 64, "Edit");
        tabs.titles_count = 2;
        tabs.selected = 0;
        tabs.sel_fg = GMB_CELLCOLOR_BLACK;
        tabs.sel_bg = GMB_CELLCOLOR_CYAN;
        gmb_rect_t tarea = { 1, 8, 12, 1 };
        gmb_bw_tabs_draw(&tabs, buf, tarea);

        gmb_bw_table_t table;
        memset(&table, 0, sizeof(table));
        snprintf(table.headers[0], 64, "A");
        snprintf(table.headers[1], 64, "B");
        table.headers_count = 2;
        snprintf(table.rows[0][0], 128, "1");
        snprintf(table.rows[0][1], 128, "2");
        table.rows_count = 1;
        table.row_cols = 2;
        table.border_color = cyan;
        gmb_rect_t tarea2 = { 1, 9, 10, 3 };
        gmb_bw_table_draw(&table, buf, tarea2);

        gmb_icon_info_t ic = gmb_file_theme_icon_for("main.c", false, false, false);
        CHECK(ic.has_color);
        ic = gmb_file_theme_icon_for("doc.md", false, false, false);
        CHECK(ic.icon[0] != 0);
        ic = gmb_file_theme_icon_for("run", false, false, true);
        CHECK(ic.color.r == GMB_CELLCOLOR_GREEN.r);
        gmb_screen_free(buf);
    }

    /* ---- Tiling ---- */
    {
        gmb_tiling_stack_t st;
        gmb_tiling_stack_init(&st);
        gmb_window_id_t w1 = { 1 }, w2 = { 2 }, w3 = { 3 };
        gmb_tiling_stack_add_window(&st, w1, true);
        gmb_tiling_stack_add_window(&st, w2, false);
        CHECK(gmb_tiling_stack_focused_index(&st) == 0);
        int idx = gmb_tiling_stack_add_window(&st, w3, true);
        CHECK(idx == 2 && gmb_tiling_stack_focused_index(&st) == 2);
        CHECK(gmb_tiling_stack_cycle_focus(&st, 1) == 0);
        CHECK(gmb_tiling_stack_contains(&st, w2));
        gmb_tiling_stack_remove_window(&st, w2);
        CHECK(!gmb_tiling_stack_contains(&st, w2) && st.windows_count == 2);
        gmb_tiling_stack_t st2;
        gmb_tiling_stack_init(&st2);
        gmb_tiling_stack_insert_after(&st2, w1, (gmb_window_id_t){ 99 }, true);
        gmb_tiling_stack_insert_after(&st2, w3, w1, false);
        CHECK(st2.windows_count == 2 && st2.windows[1].id == 3);
        gmb_rect_t r = { 0, 0, 10, 10 }, a, b;
        gmb_tiling_split_rect(r, GMB_TILING_H, 0.3f, &a, &b);
        CHECK(a.w == 3 && b.x == 3 && b.w == 7);
        gmb_tiling_split_rect(r, GMB_TILING_V, 0.5f, &a, &b);
        CHECK(a.h == 5 && b.y == 5 && b.h == 5);
        CHECK(gmb_tiling_direction_opposite(GMB_DIR_LEFT) == GMB_DIR_RIGHT);
        gmb_tiler_t tl;
        gmb_tiler_init(&tl);
        gmb_tiler_add(&tl, w1);
        gmb_tiler_add(&tl, w2);
        CHECK(gmb_tiler_count(&tl) == 2 && gmb_tiler_contains(&tl, w2));
        CHECK(!gmb_tiler_add(&tl, w1));
        gmb_tiler_remove(&tl, w1);
        CHECK(!gmb_tiler_contains(&tl, w1) && gmb_tiler_count(&tl) == 1);
    }

    /* ---- Fuzzy ---- */
    {
        gmb_fuzzy_result_t r = gmb_fuzzy_score("fuzzy", "FuzzyText");
        CHECK(r.matched && r.score > 0);
        gmb_fuzzy_result_t exact = gmb_fuzzy_score("fuzzy", "fuzzy");
        CHECK(exact.matched && exact.score > r.score);
        gmb_fuzzy_result_t r2 = gmb_fuzzy_score("ft", "FuzzyText");
        CHECK(r2.matched && r2.score > 0);
        gmb_fuzzy_result_t r3 = gmb_fuzzy_score("fz", "FuzzyText");
        CHECK(r3.matched);
        gmb_fuzzy_result_t r4 = gmb_fuzzy_score("xyz", "FuzzyText");
        CHECK(!r4.matched);
        gmb_fuzzy_result_t r5 = gmb_fuzzy_score("", "anything");
        CHECK(r5.matched && r5.score >= 50000);
        const char* items[4] = { "main.c", "readme.md", "MainTest.c", "build.sh" };
        int idx[4];
        int n = gmb_fuzzy_filter("main.c", items, 4, idx, 4);
        CHECK(n >= 1 && strcmp(items[idx[0]], "main.c") == 0);
        int n2 = gmb_fuzzy_filter("mc", items, 4, idx, 4);
        CHECK(n2 >= 1);
        CHECK(gmb_fuzzy_matches("md", "readme.md"));
        CHECK(!gmb_fuzzy_matches("zz", "readme.md"));
    }

    /* ---- Qr ---- */
    {
        gmb_qr_matrix_t* m = gmb_qr_encode("HELLO", GMB_QR_ECC_M);
        CHECK(m && m->width == 21 && m->height == 21);
        CHECK(gmb_qr_get(m, 0, 0));
        CHECK(!gmb_qr_get(m, 3, 3));
        CHECK(gmb_qr_get(m, 0, 3) && gmb_qr_get(m, 3, 0));
        CHECK(gmb_qr_get(m, m->width - 8, 8));
        CHECK(gmb_qr_get(m, 0, 6) == gmb_qr_get(m, 2, 6));
        char hb[2048];
        gmb_qr_matrix_to_halfblock(m, hb, sizeof(hb));
        CHECK(hb[0] != 0 && hb[0] == (char)0xE2);
        CHECK((unsigned char)hb[1] == 0x96);
        gmb_qr_matrix_free(m);
        char qs[2048];
        gmb_qr_string("abc", GMB_QR_ECC_L, qs, sizeof(qs));
        CHECK(strlen(qs) > 40);
        gmb_qr_matrix_t* m2 = gmb_qr_encode("this is a longer text to push the version boundary", GMB_QR_ECC_H);
        CHECK(m2 && m2->width > 21);
        gmb_qr_matrix_free(m2);
    }

    /* ---- Repl ---- */
    {
        static int called = 0;
        gmb_repl_t r;
        gmb_repl_init(&r, "> ");
        CHECK(strcmp(r.prompt, "> ") == 0);
        gmb_repl_register(&r, "help", "aiuto", NULL);
        gmb_repl_register(&r, "exit", "esci", NULL);
        char comp[8][256];
        int n = gmb_repl_complete(&r, "he", 2, comp, 8);
        CHECK(n == 1 && strcmp(comp[0], "help") == 0);
        n = gmb_repl_complete(&r, "h", 1, comp, 8);
        CHECK(n == 1 && strcmp(comp[0], "help") == 0);
        n = gmb_repl_complete(&r, "help x", 6, comp, 8);
        CHECK(n == 0);
        CHECK(gmb_repl_dispatch(&r, "exit now") == true);
        CHECK(gmb_repl_dispatch(&r, "unknown") == false);
        gmb_repl_history_add(&r, "print 1");
        gmb_repl_history_add(&r, "print 2");
        CHECK(r.history_count == 2);
        char prev[256];
        int h = gmb_repl_history_prev(&r, "", prev, sizeof(prev));
        CHECK(h == 1 && strcmp(prev, "print 2") == 0);
        h = gmb_repl_history_prev(&r, "print 2", prev, sizeof(prev));
        CHECK(h == 0 && strcmp(prev, "print 1") == 0);
        const char* hp = "/tmp/gmb_repl_hist.txt";
        CHECK(gmb_repl_save_history(&r, hp));
        gmb_repl_t r2;
        gmb_repl_init(&r2, "> ");
        CHECK(gmb_repl_load_history(&r2, hp) == 2);
        CHECK(strcmp(r2.history[1], "print 2") == 0);
    }

    /* ---- CodeArea ext ---- */
    {
        char buf[32];
        gmb_codearea_format_line_number(3, 5, buf, sizeof(buf));
        CHECK(strcmp(buf, "    3|") == 0);
        gmb_codearea_format_line_number(12, 2, buf, sizeof(buf));
        CHECK(strcmp(buf, "12|") == 0);
        CHECK(gmb_codearea_gutter_inner_width(1) == 2);
        CHECK(gmb_codearea_gutter_inner_width(100) == 3);
        CHECK(gmb_codearea_gutter_width(100) == 4);
        gmb_text_selection_t sel;
        gmb_text_selection_clear(&sel, 0, 0);
        CHECK(!gmb_text_selection_has(&sel));
        gmb_text_selection_select_line(&sel, 2, 5);
        CHECK(gmb_text_selection_has(&sel));
        CHECK(sel.anchor_line == 2 && sel.end_line == 2 && sel.end_col == 5);
    }

    /* ---- WindowManager ---- */
    {
        gmb_tile_grid_t g;
        gmb_tile_grid_init(&g, 2, 3);
        CHECK(g.rows == 2 && g.cols == 3 && gmb_tile_grid_cells(&g) == 6);
        int r, c;
        gmb_tile_grid_rowcol(&g, 4, &r, &c);
        CHECK(r == 1 && c == 1);
        gmb_rect_t work = { 0, 0, 40, 20 };
        gmb_window_manager_t m;
        gmb_window_manager_init(&m, work);
        gmb_rect_t r1 = { 2, 2, 10, 10 }, r2 = { 5, 5, 8, 8 };
        int id1 = gmb_window_manager_add(&m, "Alpha", r1);
        int id2 = gmb_window_manager_add(&m, "Beta", r2);
        CHECK(id1 == 1 && id2 == 2 && gmb_window_manager_focused(&m) == 2);
        CHECK(gmb_window_manager_find_by_title(&m, "ALPHA") == 1);
        CHECK(gmb_window_manager_find_by_title(&m, "gamma") == -1);
        CHECK(gmb_window_manager_topmost_at(&m, 6, 6) == 2);
        gmb_window_manager_raise(&m, id1);
        CHECK(gmb_window_manager_focused(&m) == 1);
        CHECK(gmb_window_manager_topmost_at(&m, 6, 6) == 1);
        gmb_window_manager_move_by(&m, id2, 3, -1);
        gmb_managed_window_t* w = gmb_window_manager_get(&m, id2);
        CHECK(w->rect.x == 8 && w->rect.y == 4);
        gmb_window_manager_deactivate(&m, id1);
        CHECK(gmb_window_manager_focused(&m) == -1);
        gmb_window_manager_minimize(&m, id2);
        CHECK(w->minimized && w->state == GMB_WSTATE_MINIMIZED);
        gmb_window_manager_maximize(&m, id1);
        gmb_managed_window_t* w1 = gmb_window_manager_get(&m, id1);
        CHECK(w1->rect.w == 40 && w1->rect.h == 20);
        gmb_window_manager_restore(&m, id1);
        CHECK(w1->rect.w == 10 && w1->rect.h == 10);
        gmb_rect_t cr = gmb_managed_window_content_rect(w1);
        CHECK(cr.x == 3 && cr.w == 8);
        gmb_window_manager_tile(&m, 2, 2);
        gmb_managed_window_t* a = gmb_window_manager_get(&m, 1);
        gmb_managed_window_t* b = gmb_window_manager_get(&m, 2);
        CHECK(a->rect.w == 20 && a->rect.h == 10);
        CHECK(b->rect.x == 20);
        gmb_window_manager_remove(&m, 1);
        CHECK(gmb_window_manager_get(&m, 1) == NULL && m.count == 1);
    }

    /* ---- PrinterDialog / DialogAdapter / FileExplorer ---- */
    {
        gmb_printer_dialog_t pd;
        gmb_printer_dialog_init(&pd);
        CHECK(pd.printers_count == 4);   /* C# PrinterDialog.Printers default */
        gmb_printer_dialog_add_printer(&pd, "Default");
        gmb_printer_dialog_add_printer(&pd, "PDF");
        CHECK(pd.printers_count == 6);
        CHECK(strcmp(gmb_printer_dialog_selected_printer(&pd), "Default") == 0);
        pd.selected_index = 1;
        CHECK(strcmp(gmb_printer_dialog_selected_printer(&pd), "PDF") == 0);
        CHECK(pd.copies == 1 && !pd.duplex);

        gmb_question_t q;
        gmb_question_init(&q, "name", GMB_QK_INPUT);
        snprintf(q.default_str, sizeof(q.default_str), "World");
        char qout[128];
        gmb_dialog_build_input(&q, qout, sizeof(qout));
        CHECK(strcmp(qout, "World") == 0);
        gmb_question_t qc;
        gmb_question_init(&qc, "ok", GMB_QK_CONFIRM);
        qc.has_default_bool = true; qc.default_bool = true;
        CHECK(gmb_dialog_build_confirm(&qc) == true);
        gmb_question_t qs;
        gmb_question_init(&qs, "pick", GMB_QK_SELECT);
        gmb_question_add_choice(&qs, "uno");
        gmb_question_add_choice(&qs, "due");
        CHECK(gmb_dialog_build_select(&qs) == 0);
        qs.has_default_index = true; qs.default_index = 1;
        CHECK(gmb_dialog_build_select(&qs) == 1);

        gmb_file_explorer_t fe;
        gmb_file_explorer_init(&fe, "/tmp");
        CHECK(fe.entry_count >= 1);
        CHECK(gmb_file_explorer_get_selected(&fe) != NULL);
        int fe_sel_after_next = fe.entry_count > 1 ? 1 : 0;
        gmb_file_explorer_next(&fe);
        CHECK(gmb_file_explorer_selected_index(&fe) == fe_sel_after_next);
        gmb_file_explorer_prev(&fe);
        CHECK(gmb_file_explorer_selected_index(&fe) == 0);
    }

    /* ---- Fsm / ImageWidget / NumericUpDown ---- */
    {
        gmb_fsm_t f;
        gmb_fsm_init(&f, 0, on_evt, NULL);
        CHECK(f.running && f.current_state == 0);
        CHECK(gmb_fsm_fire(&f, 3) == 3);
        CHECK(gmb_fsm_fire(&f, 2) == 5);
        gmb_fsm_fire(&f, 9);
        CHECK(!f.running);
        gmb_fsm_t f2;
        gmb_fsm_init(&f2, 1, NULL, NULL);
        CHECK(gmb_fsm_fire(&f2, 5) == 1);

        gmb_image_widget_t iw;
        gmb_image_widget_init(&iw);
        gmb_cell_color_t px[4] = { GMB_CELLCOLOR_RED, GMB_CELLCOLOR_GREEN, GMB_CELLCOLOR_BLUE, GMB_CELLCOLOR_WHITE };
        CHECK(gmb_image_widget_set_pixels(&iw, 2, 2, px));
        gmb_cell_color_t out;
        CHECK(gmb_image_widget_pixel_at(&iw, 1, 1, &out) && out.r == GMB_CELLCOLOR_WHITE.r);
        CHECK(!gmb_image_widget_pixel_at(&iw, 5, 0, NULL));
        gmb_image_widget_click(&iw, 7, 9);
        CHECK(iw.last_click_x == 7 && iw.last_click_y == 9);
        CHECK(!gmb_image_widget_set_pixels(&iw, 300, 300, px));

        gmb_numeric_up_down_t nu;
        gmb_numeric_up_down_init(&nu, 0, 0, 8);
        gmb_numeric_up_down_set_range(&nu, -5, 5, 1);
        gmb_numeric_up_down_set_value(&nu, 10);
        CHECK(gmb_numeric_up_down_value(&nu) == 5);
        gmb_numeric_up_down_increment(&nu);
        CHECK(gmb_numeric_up_down_value(&nu) == 5);
        gmb_numeric_up_down_decrement(&nu);
        CHECK(gmb_numeric_up_down_value(&nu) == 4);
        gmb_numeric_up_down_set_value(&nu, -99);
        CHECK(gmb_numeric_up_down_value(&nu) == -5);
    }

    /* ---- Config ---- */
    {
        gmb_config_t c;
        gmb_config_init_default(&c);
        CHECK(!c.debug_mode && strcmp(c.decimal_separator, ",") == 0 && strcmp(c.thousands_separator, ".") == 0);
        c.debug_mode = true;
        snprintf(c.decimal_separator, sizeof(c.decimal_separator), ".");
        snprintf(c.thousands_separator, sizeof(c.thousands_separator), ",");
        gmb_config_make_current(&c);
        gmb_config_t* cur = gmb_config_current();
        CHECK(cur && cur->debug_mode && strcmp(cur->decimal_separator, ".") == 0);
    }

    /* ---- Geometry ---- */
    {
        gmb_rect_t a = { 0, 0, 10, 10 }, b = { 5, 5, 10, 10 };
        gmb_point_t ctr = gmb_rect_center(a);
        CHECK(ctr.x == 5 && ctr.y == 5);
        gmb_rect_t inter;
        CHECK(gmb_rect_intersect(a, b, &inter));
        CHECK(inter.x == 5 && inter.y == 5 && inter.w == 5 && inter.h == 5);
        gmb_rect_t uni = gmb_rect_union(a, b);
        CHECK(uni.x == 0 && uni.y == 0 && uni.w == 15 && uni.h == 15);
        gmb_rect_t sep = { 20, 0, 2, 2 };
        gmb_rect_t no;
        CHECK(!gmb_rect_intersect(a, sep, &no));
        gmb_point_t p1 = { 1, 5 }, p2 = { 8, 5 }, p3 = { 5, 5 };
        gmb_rect_t screen = { 0, 0, 10, 10 };
        CHECK(gmb_geometry_snap_zone(p1, screen, 2) == GMB_SNAP_LEFT);
        CHECK(gmb_geometry_snap_zone(p2, screen, 2) == GMB_SNAP_RIGHT);
        CHECK(gmb_geometry_snap_zone(p3, screen, 2) == GMB_SNAP_NONE);
        gmb_rect_t snapped;
        gmb_geometry_snapped_rect(GMB_SNAP_LEFT, screen, &snapped);
        CHECK(snapped.w == 5);
        gmb_geometry_snapped_rect(GMB_SNAP_RIGHT, screen, &snapped);
        CHECK(snapped.x == 5 && snapped.w == 5);
    }

    /* ---- PathCompleter ---- */
    {
        gmb_path_completer_result_t pc;
        gmb_path_completer_complete("/tmp/", 5, &pc);
        CHECK(pc.count > 0);
        CHECK(pc.replacement_start == 0 && pc.replacement_len == 5);
        gmb_path_completer_result_t pc2;
        gmb_path_completer_complete("cd /tmp", 8, &pc2);
        CHECK(pc2.count > 0 && pc2.replacement_len == 4);
        gmb_path_completer_result_t pc3;
        gmb_path_completer_complete("", 0, &pc3);
        CHECK(pc3.count == 0);
    }

    /* ---- AppRunner / IApp ---- */
    {
        gmb_app_runner_t r;
        gmb_app_runner_init(&r);
        gmb_app_runner_on_key(&r, 'x', fake_key_handler);
        gmb_app_runner_on_mouse(&r, NULL);
        gmb_app_runner_on_resize(&r, NULL);
        gmb_app_runner_on_event(&r, NULL);
        gmb_app_runner_set_render(&r, fake_render, NULL);
        CHECK(r.key_count == 1 && r.key_codes[0] == 'x');
        gmb_app_runner_on_key(&r, 'x', fake_key_handler);
        CHECK(r.key_count == 1);

        fake_app_ctx_t ctx = { 0, 0, 0 };
        gmb_app_t app;
        memset(&app, 0, sizeof(app));
        app.user = &ctx;
        app.name = fake_app_name;
        app.close_requested = fake_app_close;
        app.resize = fake_app_resize;
        app.draw = fake_app_draw;
        app.handle_event = fake_app_handle;
        gmb_input_source_t input;
        memset(&input, 0, sizeof(input));
        input.user = &r;
        input.read = fake_input_read;
        gmb_app_runner_run(&r, &app, &input, NULL, 80, 24);
        CHECK(ctx.key_seen == 1);
        CHECK(ctx.draw_calls >= 2);
    }

    /* ---- MouseCapture motion ---- */
    {
        char esc[64];
        int n = gmb_termutils_enable_mouse_capture_motion(esc, sizeof(esc));
        CHECK(n > 0 && strstr(esc, "?1003h") != NULL);
    }

    if (failures == 0)
        printf("OK: %d checks superati.\n", checks);
    else
        printf("FAIL: %d/%d checks falliti.\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
