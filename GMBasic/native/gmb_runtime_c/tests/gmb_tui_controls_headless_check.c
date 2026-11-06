/* gmb_tui_controls_headless_check.c — HEADLESS: controlli DENTRO UNA WINDOW su SDLConsole
 * (driver SDL dummy, nessuna finestra) e StandardConsole. Per ogni caso inietta tasti e
 * mouse e verifica lo stato; l HASH del buffer (solo area finestra) deve essere IDENTICO
 * fra le due console: la TUI a celle ha un unico output, il render SDL cambia solo i glifi. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "gmb_runtime.h"
#include "gmb_iconsole.h"
#include "gmb_screen_config.h"
#include "gmb_sdl_console.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_clipper.h"
#include "gmb_gfx_input.h"

extern void gmb_tui_handle_mouse_cell(int, int, int);
static int fails = 0, checks = 0;
#define CHECK(c, m) do { checks++; if (!(c)) { fprintf(stderr, "  FAIL: %s\n", m); fails++; } } while (0)

static gmb_screen_buffer_t* B = NULL;
static gmb_iconsole_t* CON = NULL;

static void key(const char* k) { gmb_tui_push_gfx_key((int)(unsigned char)k[0]); gmb_tui_process_pending_keys(); }
static void click(int x, int y) { gmb_tui_handle_mouse_cell(x, y, 1); gmb_tui_handle_mouse_cell(x, y, 2); }
static gmb_control_t* at(int i) { return gmb_tui_control_at(i); }

static void case_begin(int mode, const char* name)
{
    static int seq = 0;
    char wn[32];
    gmb_tui_controls_reset();
    gmb_tui_process_pending_keys();   /* svuota la coda tasti (nessun controllo: scartati) */
    gmb_winstmt_reset();
    snprintf(wn, sizeof wn, "w%d", ++seq);
    gmb_winstmt_define(wn, name, 1, 1, 22, 77, GMB_WINSTMT_DOUBLE);
    gmb_theme_paint_desktop(B);
    gmb_winstmt_render_all();
}

static uint32_t hash_buf(void)
{
    uint32_t h = 2166136261u;
    int w = gmb_screen_width(B);
    gmb_screen_cell_t* c = gmb_screen_cells(B);
    for (int y = 1; y <= 22; y++)
        for (int x = 1; x <= 77; x++)
        {
            int i = y * w + x;
            h = (h ^ c[i].character) * 16777619u;
            h = (h ^ (c[i].fg_rgb.r << 16 | c[i].fg_rgb.g << 8 | c[i].fg_rgb.b)) * 16777619u;
            h = (h ^ (c[i].bg_rgb.r << 16 | c[i].bg_rgb.g << 8 | c[i].bg_rgb.b)) * 16777619u;
        }
    return h;
}

/* ---- casi ---- */
static void case_input(int mode, uint32_t* h)
{
    case_begin(mode, "Input");
    gmb_tui_add_label(3, 3, 8, "Nome:");
    gmb_control_t* c = gmb_tui_add_textinput(12, 3, 20, "");
    gmb_tui_control_focus(1);
    gmb_tui_draw_controls(B);
    key("a"); key("b"); key("c");
    { char t[64]; gmb_tui_control_text(1, t, sizeof t); CHECK(strcmp(t, "abc") == 0, "input: tasti abc"); }
    /* cursore in fondo: Backspace cancella l'ultimo carattere */
    key("\x7f");   /* backspace */
    { char t[64]; gmb_tui_control_text(1, t, sizeof t); CHECK(strcmp(t, "ab") == 0, "input: backspace"); }
    /* click sul campo: deve prendere il focus senza alterare il testo */
    click(c->x + 2, c->y);
    { char t[64]; gmb_tui_control_text(1, t, sizeof t); CHECK(strcmp(t, "ab") == 0, "input: click non altera il testo"); }
    CHECK(gmb_tui_control_focused() == 1, "input: click -> focus");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static int clicks = 0;
static void on_click(void* ctx) { (void)ctx; clicks++; }
static void case_button(int mode, uint32_t* h)
{
    case_begin(mode, "Button");
    clicks = 0;
    gmb_control_t* b = gmb_tui_add_button(10, 5, 12, "OK");
    { gmb_button_t* bt = (gmb_button_t*)b->user; gmb_button_set_onclick(bt, on_click, NULL); }
    gmb_tui_draw_controls(B);
    click(b->x + b->width / 2, b->y);
    CHECK(clicks == 1, "button: click -> callback");
    gmb_tui_control_focus(0);
    key("\r");
    CHECK(clicks == 2, "button: Enter -> callback");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_checkbox(int mode, uint32_t* h)
{
    case_begin(mode, "Checkbox");
    gmb_control_t* c = gmb_tui_add_checkbox(3, 3, 26, "Abilita");
    gmb_checkbox_t* cb = (gmb_checkbox_t*)c->user;
    gmb_tui_draw_controls(B);
    click(c->x + 2, c->y);
    CHECK(cb->checked == true, "checkbox: click attiva");
    gmb_tui_control_focus(0);
    key(" ");
    CHECK(cb->checked == false, "checkbox: Space disattiva");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_toggle(int mode, uint32_t* h)
{
    case_begin(mode, "Toggle");
    gmb_control_t* c = gmb_tui_add_toggle(3, 3, 26, "Esperto");
    gmb_toggle_button_t* t = (gmb_toggle_button_t*)c->user;
    gmb_tui_draw_controls(B);
    click(c->x + 2, c->y);
    CHECK(t->pressed == true, "toggle: click");
    gmb_tui_control_focus(0);
    key(" ");
    CHECK(t->pressed == false, "toggle: Space");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_radio(int mode, uint32_t* h)
{
    case_begin(mode, "Radio");
    gmb_control_t* c = gmb_tui_add_radiogroup(3, 3, 24, 3, "Testo|Grafico|Auto");
    gmb_radio_group_t* r = (gmb_radio_group_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x18");  /* DOWN */
    CHECK(r->selected == 1, "radio: Down -> 1");
    click(c->x + 2, c->y + 2);
    CHECK(r->selected == 2, "radio: click riga 3 -> 2");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_listbox(int mode, uint32_t* h)
{
    case_begin(mode, "Listbox");
    gmb_control_t* c = gmb_tui_add_listbox(3, 3, 24, 4, "alpha|beta|gamma|delta");
    gmb_listbox_t* l = (gmb_listbox_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x18"); key("\x18");
    CHECK(l->selected == 2, "listbox: Down x2 -> 2");
    click(c->x + 1, c->y + 3);
    CHECK(l->selected == 3, "listbox: click riga 4 -> 3");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_combo(int mode, uint32_t* h)
{
    case_begin(mode, "Combo");
    gmb_control_t* c = gmb_tui_add_combobox(3, 3, 20, "uno|due|tre");
    gmb_combobox_t* cb = (gmb_combobox_t*)c->user;
    gmb_tui_draw_controls(B);
    click(c->x + c->width - 2, c->y);
    CHECK(cb->open == true, "combo: click apre");
    key("\x18");   /* DOWN */
    key("\r");     /* Enter */
    CHECK(cb->selected == 1 && !cb->open, "combo: Down+Enter seleziona e chiude");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_slider(int mode, uint32_t* h)
{
    case_begin(mode, "Slider");
    gmb_control_t* c = gmb_tui_add_slider(3, 3, 20, 0, 100, 50);
    gmb_slider_t* s = (gmb_slider_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    { int v0 = s->value; key("\x04"); CHECK(s->value > v0, "slider: Right aumenta"); }
    click(c->x + 18, c->y);
    CHECK(s->value > 50, "slider: click a destra");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_numupdown(int mode, uint32_t* h)
{
    case_begin(mode, "NumUpDown");
    gmb_control_t* c = gmb_tui_add_numericupdown(3, 3, 12, 0, 100, 10);
    gmb_numeric_updown_t* n = (gmb_numeric_updown_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x05");  /* UP */
    CHECK(n->value > 10, "numupdown: Up aumenta");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_tab(int mode, uint32_t* h)
{
    case_begin(mode, "Tab");
    gmb_control_t* c = gmb_tui_add_tab(3, 3, 30, "File|Modifica");
    gmb_tab_t* t = (gmb_tab_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x04");  /* RIGHT */
    CHECK(t->selected == 1, "tab: Right -> 1");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_menu(int mode, uint32_t* h)
{
    case_begin(mode, "Menu");
    gmb_tui_add_menu(0, 0, 60, "File|Edit");
    gmb_tui_menu_set(0, "File:Apri|Salva;Edit:Copia");
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x18");  /* DOWN apre */
    { gmb_menu_t* m = (gmb_menu_t*)at(0)->user; CHECK(m->is_open == true, "menu: Down apre"); }
    key("\x18");  /* DOWN voce */
    key("\r");    /* Enter */
    { gmb_menu_t* m = (gmb_menu_t*)at(0)->user; CHECK(m->is_open == false, "menu: Enter chiude"); }
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_scrollbar(int mode, uint32_t* h)
{
    case_begin(mode, "Scrollbar");
    gmb_control_t* c = gmb_tui_add_scrollbar(3, 3, 20, 0, 100, 10);
    gmb_scrollbar_t* s = (gmb_scrollbar_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x18");
    CHECK(s->value >= 10, "scrollbar: Down/tasto");
    click(c->x + c->width - 1, c->y);
    CHECK(s->value > 10, "scrollbar: click freccia destra");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_stars(int mode, uint32_t* h)
{
    case_begin(mode, "Stars");
    gmb_control_t* c = gmb_tui_add_starrating(3, 3, 16, 5, 3);
    gmb_star_rating_t* s = (gmb_star_rating_t*)c->user;
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x04");
    CHECK(s->value == 4, "stars: Right -> 4");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_spinner(int mode, uint32_t* h)
{
    case_begin(mode, "Spinner");
    gmb_control_t* c = gmb_tui_add_spinner(3, 3, 12, "line");
    gmb_spinner_t* s = (gmb_spinner_t*)c->user;
    gmb_tui_draw_controls(B);
    { int i0 = s->index; click(c->x + c->width - 1, c->y); CHECK(s->index != i0, "spinner: click avanza"); }
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

static void case_static(int mode, uint32_t* h)
{
    case_begin(mode, "Label+Status");
    gmb_tui_add_label(3, 3, 20, "Etichetta");
    gmb_tui_add_statusbar(0, 24, 80, "F1 Aiuto|F2 Salva");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

/* TBROWSE: griglia con colonne/righe, navigazione tastiera e click. */
static void case_tbrowse(int mode, uint32_t* h)
{
    case_begin(mode, "TBrowse");
    gmb_control_t* c = gmb_tui_add_tbrowse(3, 3, 44, 6, "Nome:12:0:l|Eta:6:N",
                                           "Mario|42;Luigi|31;Anna|28");
    gmb_tui_draw_controls(B);
    gmb_tui_control_focus(0);
    key("\x18");  /* DOWN */
    CHECK(gmb_tui_control_selrow(0) == 1, "tbrowse: Down -> riga 1");
    key("\x18");
    CHECK(gmb_tui_control_selrow(0) == 2, "tbrowse: Down -> riga 2");
    key("\x05");  /* UP */
    CHECK(gmb_tui_control_selrow(0) == 1, "tbrowse: Up -> riga 1");
    key("\x01");  /* HOME */
    CHECK(gmb_tui_control_selrow(0) == 0, "tbrowse: Home -> riga 0");
    key("\x06");  /* END */
    CHECK(gmb_tui_control_selrow(0) == 2, "tbrowse: End -> ultima riga");
    /* END porta in fondo e scorre (top_row>0): HOME riporta in cima, poi il click sulla
     * 2a riga dati (bordo+header+separatore => +3, quindi +3 e +4 per la riga 1) deve
     * selezionare la riga 1. */
    key("\x01");  /* HOME: top_row = 0 */
    click(c->x + 2, c->y + 4);
    CHECK(gmb_tui_control_selrow(0) == 1, "tbrowse: click riga 2 -> 1");
    gmb_tui_draw_controls(B);
    *h = hash_buf();
}

typedef struct { const char* name; void (*fn)(int, uint32_t*); } Case;

int main(int argc, char** argv)
{
    freopen("/dev/null", "w", stdout);
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    gmb_ui_set_theme_name("turbovision");
    gmb_tui_show_markers = 0;

    Case cases[] = {
        {"input", case_input}, {"button", case_button}, {"checkbox", case_checkbox},
        {"toggle", case_toggle}, {"radio", case_radio}, {"listbox", case_listbox},
        {"combo", case_combo}, {"slider", case_slider}, {"numupdown", case_numupdown},
        {"tab", case_tab}, {"menu", case_menu}, {"scrollbar", case_scrollbar},
        {"stars", case_stars}, {"spinner", case_spinner}, {"static", case_static},
        {"tbrowse", case_tbrowse},
    };
    int nc = (int)(sizeof cases / sizeof cases[0]);

    for (int mode_i = 0; mode_i < 2; mode_i++)
    {
        int mode = mode_i == 0 ? 0 : 2;   /* 0 = SDLConsole, 2 = StandardConsole */
        gmb_screen_config_t cfg = gmb_screen_config_get_default(GMB_SM_TEXT);
        CON = gmb_iconsole_create(mode, &cfg);
        if (!CON) { fprintf(stderr, "console mode %d: creazione FALLITA\n", mode); return 2; }
        gmb_iconsole_set_active(CON);
        struct gmb_sdl_console* m = gmb_iconsole_model(CON);
        B = gmb_sdl_console_screen(m);
        gmb_clipper_global_state()->buffer = B;
        fprintf(stderr, "\n===== CONSOLE %s (buffer %dx%d) =====\n", mode == 0 ? "SDLConsole" : "StandardConsole",
               gmb_screen_width(B), gmb_screen_height(B));
        int only = argc > 1 ? atoi(argv[1]) : -1;
        for (int i = 0; i < nc; i++)
        {
            if (only >= 0 && i != only) continue;
            uint32_t h = 0;
            int before = fails;
            cases[i].fn(mode, &h);
            fprintf(stderr, "%-10s %s  hash=%08X\n", cases[i].name, fails == before ? "PASS" : "FAIL", h);
        }
        CON->dispose(CON); CON = NULL;
    }
    fprintf(stderr, "\nTOTALE: %d checks, %d FAIL\n", checks, fails);
    return fails ? 1 : 0;
}
