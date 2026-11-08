/* ui_click_check.c — TEST FUNZIONALE dei clic sui controlli (headless, deterministico).
 *
 * Per ogni controllo: lo crea con la STESSA factory usata dall'emitter, renderizza un
 * frame (il renderer fissa il rect pixel), poi inietta DOWN+UP al centro con
 * `gmb_form_dispatch_mouse_px` e verifica che lo stato cambi.
 *
 * Serve a rispondere a "i clic non funzionano" con un numero, non con un'impressione.
 * Uscita: 0 = tutti PASS, N = numero di FAIL.
 *
 * Build/run: tools/sh/ui_click_check.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gmb_runtime.h"
#include "gmb_graphics.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_form.h"
#include "gmb_gfx_input.h"

/* Dichiarazioni del runtime non presenti negli header usati qui. */
void gmb_tui_draw_px_all(void);
void gmb_ui_set_theme_name(const char* name);

static int g_fail = 0;
static int g_pass = 0;

static void report(const char* name, int ok, const char* detail)
{
    printf("%-6s %-14s %s\n", ok ? "PASS" : "FAIL", name, detail ? detail : "");
    if (ok) g_pass++; else g_fail++;
}

/* Un frame completo: il renderer calcola i rect pixel usati dall'hit-test. */
static void render_frame(void)
{
    extern void gmb_tui_mark_dirty(void);
    gmb_tui_mark_dirty();          // forza il layer pixel (px_*) come un frame vero
    gmb_form_render_if_dirty();
    gmb_tui_draw_px_all();
}

static int g_btn_fired = 0;
static void btn_cb(void) { g_btn_fired = 1; }

/* Evento CHANGE (usato dalle schede): il gestore deve essere CHIAMATO dal runtime. */
static int g_chg_fired = 0;
static void chg_cb(void) { g_chg_fired = 1; }

static void click_at(int x, int y)
{
    gmb_form_dispatch_mouse_px(GMB_GFX_MOUSE_DOWN, x, y, 1, 0);
    gmb_form_dispatch_mouse_px(GMB_GFX_MOUSE_UP, x, y, 1, 0);
    render_frame();
}

int main(void)
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    gmb_gfx_screen(12, 900, 640, 32);
    gmb_ui_set_theme_name("glass");

    printf("== clic sui controlli (grafico, tema glass) ==\n");

    /* ── TOGGLE ────────────────────────────────────────────────────────── */
    {
        double v = 0;
        gmb_control_add_toggle(40, 40, 220, "live", &v);
        render_frame();
        click_at(40 + 30, 40 + 13);
        report("toggle", v != 0, v != 0 ? "ON" : "resta OFF");
    }

    /* ── CHECKBOX ──────────────────────────────────────────────────────── */
    {
        double v = 0;
        gmb_control_add_checkbox(40, 100, 220, "auto", &v);
        render_frame();
        click_at(40 + 11, 100 + 11);
        report("checkbox", v != 0, v != 0 ? "spuntato" : "resta vuoto");
    }

    /* ── SLIDER (click al 75% della barra) ─────────────────────────────── */
    {
        double v = 0;
        gmb_form_add_track(40, 160, 220, 0, 100, &v);
        render_frame();
        click_at(40 + 165, 160 + 12);
        report("slider", v > 40 && v < 95, "valore dopo click a 75%");
    }

    /* ── LISTBOX (click sulla seconda riga) ────────────────────────────── */
    {
        double v = 0;
        gmb_form_add_listbox(40, 210, 220, 96, "uno|due|tre", &v);
        render_frame();
        click_at(40 + 20, 210 + 40);
        report("listbox", v == 1, "selezione dopo click su 'due'");
    }

    /* ── TABS (click sulla seconda scheda) ─────────────────────────────── */
    {
        double v = 0;
        gmb_form_add_tabs(40, 330, "A|B|C", &v);
        gmb_form_set_event(GMB_EV_CHANGE, (gmb_form_cb)chg_cb);   /* come ON CHANGE */
        render_frame();
        click_at(40 + 110, 330 + 13);
        report("tabs", v != 0, "scheda selezionata");
        report("tabs-event", g_chg_fired != 0, "gestore CHANGE chiamato dal click");
    }

    /* ── BUTTON (callback) ─────────────────────────────────────────────── */
    {
        gmb_form_add_button(40, 380, 200, 40, "OK", (gmb_form_cb)btn_cb);
        render_frame();
        click_at(40 + 100, 380 + 20);
        report("button", g_btn_fired != 0, "callback CLICK");
    }

    /* ── GRID (click su una riga) ──────────────────────────────────────── */
    {
        double v = 0;
        gmb_form_add_grid(40, 440, 400, 140, "N|Q", "a|1;b|2;c|3", &v);
        render_frame();
        click_at(40 + 40, 440 + 26 + 2 * 26 + 10);
        report("grid", v == 2, "riga 3 selezionata (valore atteso 2)");
    }

    /* ── STARRATING (click sul 2o pip) ─────────────────────────────────── */
    {
        double v = 0;
        gmb_control_add_starrating(500, 40, 200, 5, 3, &v);
        render_frame();
        click_at(500 + 42, 40 + 13);
        report("starrating", v != 3, "valore dopo click");
    }

    /* ── NUMERICUPDOWN (click sul '+') ─────────────────────────────────── */
    {
        double v = 5;
        gmb_control_add_numericupdown(500, 100, 160, 0, 100, 1, &v);
        render_frame();
        click_at(500 + 160 - 14, 100 + 12);
        report("numericupdown", v == 6, "valore dopo click su +");
        { char b[64]; snprintf(b, sizeof b, "  (v=%.0f, atteso 6)", v); printf("%s\n", b); }
    }

    /* ── SCROLLBAR (click a metà) ──────────────────────────────────────── */
    {
        double v = 0;
        gmb_control_add_scrollbar(500, 220, 120, 0, 100, 10, &v);
        render_frame();
        click_at(500 + 60, 220 + 12);
        report("scrollbar", v >= 40, "valore dopo click a meta barra");
    }

    /* ── COMBOBOX: apre e seleziona la voce 2 ──────────────────────────── */
    {
        double v = 0;
        gmb_form_add_dropdown(40, 520, 220, "uno|due|tre", &v);
        render_frame();
        click_at(40 + 60, 520 + 12);              /* apre la lista */
        click_at(40 + 60, 520 + 28 + 28 + 12);    /* voce 2 = "due" (righe alte come il campo) */
        { char b[64]; snprintf(b, sizeof b, "  (v=%.0f)", v); printf("%s\n", b); }
        report("combobox", v == 1, "voce 2 selezionata dopo apertura+click");
    }

    /* ── TREEVIEW: selezione di un nodo ────────────────────────────────── */
    {
        double v = -1;
        gmb_form_add_tree(500, 380, 260, 160, "root/api|root/api/v1|root/db", &v);
        render_frame();
        { gmb_widget_style_t st = gmb_widget_style_current();
          int rh = st.row_h > 0 ? st.row_h : 24;
          click_at(500 + 120, 380 + rh * 2 + rh / 2); }   /* terza riga visibile */
        { char b[64]; snprintf(b, sizeof b, "  (v=%.0f)", v); printf("%s\n", b); }
        report("treeview", v == 2, "terzo nodo selezionato");
    }

    /* ── COLORPICKER: click sul terzo campione ─────────────────────────── */
    {
        double v = -1;
        gmb_form_add_colorpicker(500, 560, "", &v);
        render_frame();
        click_at(500 + 2 * 26 + 12, 560 + 12);
        { char b[64]; snprintf(b, sizeof b, "  (v=%.0f)", v); printf("%s\n", b); }
        report("colorpicker", v == 2, "campione 3 selezionato");
    }

    /* ── CONTEXT MENU: il click sul trigger apre il menu ───────────────── */
    {
        static gmb_context_menu_t cm;
        const char* its[] = { "Copy", "Paste", "Delete" };
        gmb_context_menu_init(&cm, 40, 600, 200, 3);
        gmb_context_menu_set_items(&cm, its, 3);
        gmb_tui_control_register(&cm.ctrl);
        render_frame();
        click_at(40 + 100, 600 + 12);
        report("contextmenu", cm.open != 0, "menu aperto dal click");
    }

    /* ── MODALE: velo + input catturato + chiusura dal pulsante ─────────── */
    {
        static gmb_file_dialog_t fd;
        gmb_file_dialog_init(&fd, 0, 0, 0, 0);        /* 0,0 = centrato, 520x420 */
        gmb_tui_control_register(&fd.ctrl);
        gmb_form_modal_open(&fd.ctrl);
        render_frame();
        int opened = gmb_form_modal_active();
        click_at(4, 4);                               /* FUORI dal dialogo */
        int blocked = gmb_form_modal_active();        /* deve restare aperto */
        int x = (fd.ctrl.px_w > 0) ? fd.ctrl.px_x : fd.ctrl.x;
        int y = (fd.ctrl.px_h > 0) ? fd.ctrl.px_y : fd.ctrl.y;
        int w = (fd.ctrl.px_w > 0) ? fd.ctrl.px_w : fd.ctrl.width;
        int h = (fd.ctrl.px_h > 0) ? fd.ctrl.px_h : fd.ctrl.height;
        click_at(x + w - 60, y + h - 26);             /* pulsante Annulla */
        int closed = !gmb_form_modal_active();
        report("modal", opened && blocked && closed, "aperto, click fuori ignorato, chiuso da Annulla");
    }

    /* ── TBROWSE: click su una riga (geometria PIXEL del renderer) ─────── */
    {
        static gmb_vec_ds_t ds;
        static gmb_browse_datasource_t dsrc;
        gmb_rect_t area = { 40, 700, 260, 150 };
        gmb_vec_ds_make(&ds, 3, 2);
        gmb_vec_ds_set(&ds, 0, 0, "alpha"); gmb_vec_ds_set(&ds, 0, 1, "1");
        gmb_vec_ds_set(&ds, 1, 0, "beta");  gmb_vec_ds_set(&ds, 1, 1, "2");
        gmb_vec_ds_set(&ds, 2, 0, "gamma"); gmb_vec_ds_set(&ds, 2, 1, "3");
        gmb_vec_ds_datasource(&ds, &dsrc);
        gmb_browse_t* tb = gmb_browse_new(&area, 0, NULL, &dsrc);
        gmb_browse_column_t c1, c2;
        memset(&c1, 0, sizeof(c1)); snprintf(c1.header, sizeof(c1.header), "Nome"); c1.width = 12; c1.visible = true;
        memset(&c2, 0, sizeof(c2)); snprintf(c2.header, sizeof(c2.header), "Qta");  c2.width = 6;  c2.visible = true;
        gmb_browse_add_column(tb, &c1);
        gmb_browse_add_column(tb, &c2);
        gmb_tui_control_register(&tb->ctrl);
        render_frame();
        { gmb_widget_style_t st = gmb_widget_style_current();
          int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 16;
          int rh = st.row_h > 0 ? st.row_h : 24; if (rh < th + 6) rh = th + 6;
          int rows_top = area.y + rh + 2;                 /* header alto rh */
          click_at(area.x + 30, rows_top + rh + rh / 2);  /* riga 2 */
          report("tbrowse", gmb_browse_row_pos(tb) == 1, "riga 2 selezionata col click");
        }
    }

    /* ── ALERTBOX: click sul pulsante OK (geometria pixel) ─────────────── */
    {
        static gmb_alert_box_t ab;
        gmb_alert_box_init(&ab, 600, 40, 220, 110);
        gmb_alert_box_with_title(&ab, "Avviso");
        gmb_alert_box_with_message(&ab, "Fatto");
        gmb_tui_control_register(&ab.ctrl);
        render_frame();
        int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 14;
        int bh = th + 8, bw = gmb_gfx_text_width_logical("OK") + 24; if (bw < 60) bw = 60;
        click_at(600 + 220 - bw - 10 + bw / 2, 40 + 110 - bh - 8 + bh / 2);
        report("alertbox", ab.result == 1, "pulsante OK premuto");
    }

    /* ── CONFIRMATIONBOX: click su Cancel ─────────────────────────────── */
    {
        static gmb_confirmation_box_t cb;
        gmb_confirmation_box_init(&cb, 600, 200, 240, 120);
        gmb_confirmation_box_with_message(&cb, "Procedere?");
        gmb_tui_control_register(&cb.ctrl);
        render_frame();
        int th = gmb_gfx_text_height_logical(); if (th <= 0) th = 14;
        int bh = th + 8, gap = 8;
        int w0 = gmb_gfx_text_width_logical("OK") + 24;     if (w0 < 60) w0 = 60;
        int w1 = gmb_gfx_text_width_logical("Cancel") + 24; if (w1 < 60) w1 = 60;
        int totw = w0 + w1 + gap;
        int bx = 600 + 240 - totw - 10, by = 200 + 120 - bh - 8;
        click_at(bx + w0 + gap + w1 / 2, by + bh / 2);
        report("confirmationbox", cb.result == 1, "pulsante Cancel premuto");
    }

    printf("\nTOTALE: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail;
}
