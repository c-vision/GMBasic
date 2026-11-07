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
        render_frame();
        click_at(40 + 110, 330 + 13);
        report("tabs", v != 0, "scheda selezionata");
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

    printf("\nTOTALE: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail;
}
