/* tui_visual_gallery.c — giro visivo dei controlli TUI (headless, nessuna finestra).
 *
 * Rende la STESSA galleria di controlli su due percorsi e salva un PNG per ciascuno:
 *   tui  -> buffer a celle + SDLConsole (glifi/geometria Turbo Vision)
 *   gfx  -> schermo grafico, renderer pixel (draw_px_fn)
 *
 * Uso:  tui_visual_gallery <tui|gfx> <output.png> [dump]
 *       `dump` stampa su stderr la zona del TBrowse (verifica del buffer, non del PNG).
 *
 * Build/run: tools/sh/tui_visual_gallery.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gmb_runtime.h"
#include "gmb_graphics.h"
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_drawapi.h"
#include "gmb_form.h"
#include "gmb_gfx_present.h"
#include "gmb_sdl_console.h"
#include "gmb_screen_config.h"

static gmb_screen_buffer_t* g_buf;

static void dk(int kind, int x, int y, int w, int h, const char* text, const char* extra,
               int sel, double value, int min, int max, int state)
{
    gmb_draw_req_t r;
    memset(&r, 0, sizeof(r));
    r.kind = kind; r.x = x; r.y = y; r.w = w; r.h = h;
    r.text = text; r.extra = extra;
    r.sel = sel; r.value = value; r.min = min; r.max = max; r.state = state;
    gmb_tui_draw_control(g_buf, &r);
}

/* Galleria in CELLE (TUI): usa gmb_tui_draw_control, la stessa API dei client C#. */
static void tui_gallery(void)
{
    gmb_theme_paint_desktop(g_buf);
    dk(GMB_DK_WINDOW, 1, 1, 118, 37, "GMBasic - Galleria controlli (Turbo Vision)", NULL, 0, 0, 0, 0, GMB_DS_OPEN);
    dk(GMB_DK_MENU, 2, 2, 112, 1, "File|Modifica|Vista|Aiuto", NULL, 0, 0, 0, 0, 0);

    dk(GMB_DK_LABEL, 3, 3, 10, 1, "Nome:", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_TEXTINPUT, 14, 3, 32, 1, "Mario Rossi", NULL, 5, 0, 0, 0, GMB_DS_FOCUSED);
    dk(GMB_DK_CHECKBOX, 3, 5, 40, 1, "Abilita notifiche", NULL, 0, 0, 0, 0, GMB_DS_CHECKED);
    dk(GMB_DK_TOGGLE, 3, 6, 40, 1, "Modalita esperto", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_LABEL, 3, 8, 10, 1, "Tema:", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_COMBOBOX, 14, 8, 30, 1, "turbovision|clipper|nord|glass", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_LABEL, 3, 10, 10, 1, "Layout:", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_RADIOGROUP, 14, 10, 34, 3, "Testo|Grafico|Automatico", NULL, 1, 0, 0, 0, 0);
    dk(GMB_DK_LABEL, 3, 14, 20, 1, "Elementi:", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_LISTBOX, 3, 15, 34, 6, "alpha|beta|gamma|delta|epsilon|zeta", NULL, 2, 0, 0, 0, GMB_DS_FOCUSED);
    dk(GMB_DK_TAB, 3, 22, 34, 1, "Dati|Opzioni|Info", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_PROGRESS, 3, 24, 34, 1, NULL, NULL, 0, 65, 0, 100, 0);
    dk(GMB_DK_SLIDER, 3, 26, 34, 1, NULL, NULL, 0, 65, 0, 100, GMB_DS_FOCUSED);
    dk(GMB_DK_SPINNER, 3, 28, 12, 1, "line", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_STARRATING, 18, 28, 20, 1, NULL, NULL, 0, 3, 0, 5, 0);
    dk(GMB_DK_TEXTAREA, 3, 30, 34, 3, "riga uno\nriga due\nriga tre", NULL, 0, 0, 0, 0, 0);

    dk(GMB_DK_PANEL, 52, 3, 56, 6, "", "Pannello", 0, 0, 0, 0, 0);
    dk(GMB_DK_CODEAREA, 52, 10, 56, 5, "int main(void)\n{\n  return 0;\n}", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_TBROWSE, 52, 16, 56, 8, "Nome:12|Citta:14",
       "Mario|Roma;Luigi|Milano;Anna|Torino;Sara|Napoli", 1, 0, 0, 0, 0);
    dk(GMB_DK_HLINE, 52, 24, 56, 1, "separatore", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_BIGTEXT, 52, 26, 20, 7, "OK", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_VLINE, 49, 3, 1, 6, NULL, NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_SCROLLBAR, 52, 33, 56, 1, NULL, NULL, 0, 40, 0, 100, 0);
    dk(GMB_DK_FKEYBAR, 3, 34, 105, 1, "1Aiuto|2Salva|3Carica|4Cerca|5Esegui|6Modifica", NULL, 0, 0, 0, 0, 0);
    dk(GMB_DK_STATUSBAR, 3, 36, 105, 1, "F1 Aiuto   F2 Salva   F10 Menu   Alt+X Esci", NULL, 0, 0, 0, 0, 0);
}

static gmb_control_t* px(gmb_control_t* c, int w, int h)
{
    if (c) { c->width = w; c->height = h; }
    return c;
}

/* La TextArea non ha una factory nel registry: la si crea e registra qui. */
static gmb_control_t* add_textarea(int x, int y, int w, int h, const char* csv)
{
    gmb_text_area_t* t = (gmb_text_area_t*)calloc(1, sizeof(*t));
    if (!t) return NULL;
    gmb_text_area_init(t, x, y, w, h);
    char tmp[64][256]; int n = 0; const char* p = csv;
    while (*p && n < 64)
    {
        int i = 0;
        while (*p && *p != '|' && i < 255) tmp[n][i++] = *p++;
        tmp[n][i] = '\0'; n++;
        if (*p == '|') p++;
    }
    const char* arr[64];
    for (int k = 0; k < n; k++) arr[k] = tmp[k];
    gmb_text_area_set_lines(t, arr, n);
    gmb_tui_control_add(&t->ctrl);
    return &t->ctrl;
}

/* Galleria in PIXEL (grafica): registry TUI + gmb_tui_draw_px_all. */
static void gfx_gallery(void)
{
    gmb_tui_controls_reset();

    px(gmb_tui_add_menu(0, 0, 1280, "File|Modifica|Vista|Aiuto"), 1280, 32);
    px(gmb_tui_add_label(30, 44, 90, "Nome:"), 90, 24);
    px(gmb_tui_add_textinput(30, 64, 340, "Mario Rossi"), 340, 34);
    px(gmb_tui_add_checkbox(30, 120, 340, "Abilita notifiche"), 340, 28);
    px(gmb_tui_add_toggle(30, 160, 340, "Modalita esperto"), 340, 28);
    px(gmb_tui_add_combobox(30, 210, 340, "turbovision|clipper|nord|glass"), 340, 34);
    px(gmb_tui_add_radiogroup(30, 260, 340, 100, "Testo|Grafico|Automatico"), 340, 100);
    px(gmb_tui_add_listbox(30, 380, 340, 150, "alpha|beta|gamma|delta|epsilon|zeta"), 340, 150);
    px(gmb_tui_add_tab(30, 550, 340, "Dati|Opzioni|Info"), 340, 34);
    px(gmb_tui_add_spinner(30, 600, 140, "line"), 140, 32);
    px(gmb_tui_add_starrating(190, 600, 180, 5, 3), 180, 32);
    px(gmb_tui_add_slider(30, 650, 340, 0, 100, 65), 340, 28);
    px(gmb_tui_add_scrollbar(30, 690, 340, 0, 100, 40), 340, 18);

    /* pannello-card: contiene editor di codice e area di testo */
    gmb_px_draw_ex(GMB_PX_PANEL, 430, 40, 820, 300, "Pannello", NULL, 0, 0, 0, 0, NULL, 0, NULL, 0);
    px(gmb_tui_add_codearea(450, 70, 780, 110, "int main(void)|{|  return 0;|}"), 780, 110);
    px(add_textarea(450, 195, 780, 120, "Area di testo multiriga|con caret e scroll|nel percorso pixel"), 780, 120);

    px(gmb_tui_add_tbrowse(360, 430, 820, 200, "Nome:12|Citta:14",
                           "Mario|Roma;Luigi|Milano;Anna|Torino;Sara|Napoli"), 820, 200);
    px(gmb_tui_add_bigtext(430, 580, "OK"), 200, 80);
    px(gmb_tui_add_fkeybar(30, 760, 560, "1Aiuto|2Salva|3Carica|4Cerca|5Esegui|6Modifica"), 560, 24);
    px(gmb_tui_add_statusbar(30, 800, 1220, "F1 Aiuto    F2 Salva    F10 Menu    Alt+X Esci"), 1220, 28);

    gmb_tui_px_invalidate();
    gmb_tui_draw_px_all();
}

/* Barra di avanzamento: nessuna factory nel registry, la si crea qui. */
static gmb_control_t* add_progress(int x, int y, int w, int h, int value)
{
    gmb_progress_bar_t* p = (gmb_progress_bar_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    gmb_progress_bar_init(p, x, y, w);
    gmb_progress_bar_set(p, value);
    p->ctrl.width = w; p->ctrl.height = h;
    gmb_tui_control_add(&p->ctrl);
    return &p->ctrl;
}

/* Finestra GRAFICA: due zone, "Clipper / Turbo Vision" e "moderni (non Clipper)". */
static void gfx_window(void)
{
    gmb_tui_controls_reset();

    gmb_px_draw_ex(GMB_PX_PANEL, 16, 16, 1368, 868, "GMBasic - finestra grafica", NULL, 0, 0, 0, 0, NULL, 0, NULL, 0);
    px(gmb_tui_add_menu(36, 40, 1328, "File|Modifica|Vista|Aiuto"), 1328, 28);
    gmb_px_draw_ex(GMB_PX_PANEL, 36, 76, 650, 744, "Clipper / Turbo Vision", NULL, 0, 0, 0, 0, NULL, 0, NULL, 0);
    gmb_px_draw_ex(GMB_PX_PANEL, 706, 76, 658, 744, "Moderni (non Clipper)", NULL, 0, 0, 0, 0, NULL, 0, NULL, 0);

    /* ---- zona Clipper ---- */
    px(gmb_tui_add_label(56, 110, 70, "Nome:"), 70, 24);
    px(gmb_tui_add_textinput(130, 106, 300, "Mario Rossi"), 300, 30);
    px(gmb_tui_add_checkbox(56, 155, 300, "Abilita notifiche"), 300, 26);
    px(gmb_tui_add_radiogroup(56, 190, 300, 90, "Testo|Grafico|Automatico"), 300, 90);
    px(gmb_tui_add_combobox(56, 295, 300, "turbovision|clipper|nord|glass"), 300, 32);
    px(gmb_tui_add_listbox(56, 340, 300, 150, "alpha|beta|gamma|delta|epsilon|zeta"), 300, 150);
    px(gmb_tui_add_button(56, 505, 140, "OK"), 140, 40);
    px(gmb_tui_add_button(210, 505, 140, "Annulla"), 140, 40);
    px(gmb_tui_add_slider(56, 565, 300, 0, 100, 65), 300, 28);
    px(gmb_tui_add_scrollbar(56, 605, 300, 0, 100, 40), 300, 18);
    px(gmb_tui_add_tab(56, 640, 300, "Dati|Opzioni|Info"), 300, 34);
    px(gmb_tui_add_bigtext(380, 560, "OK"), 200, 70);
    px(gmb_tui_add_tbrowse(690, 56, 600, 130, "Nome:12|Citta:14",
                           "Mario|Roma;Luigi|Milano;Anna|Torino;Sara|Napoli"), 600, 130);

    /* ---- zona moderna (non Clipper) ---- */
    px(gmb_tui_add_toggle(726, 110, 220, "Modalita esperto"), 220, 28);
    px(add_progress(726, 150, 300, 22, 72), 300, 22);
    px(gmb_tui_add_spinner(1050, 106, 100, "line"), 100, 32);
    px(gmb_tui_add_starrating(1050, 150, 160, 5, 3), 160, 30);
    px(gmb_tui_add_datepicker(726, 190, 180), 180, 32);
    px(gmb_tui_add_logview(930, 190, 340, 70, "avvio ok|tema=turbovision|grafico|controlli attivi"), 340, 70);
    px(gmb_tui_add_treeview(726, 280, 290, 140,
                            "Documenti|  Foto|  Musica|Progetti+|  note.txt|  todo.txt"), 290, 140);
    px(gmb_tui_add_grid(726, 440, 290, 120, 3, "Mario|Roma|42|Luigi|Milano|38|Anna|Torino|29"), 290, 120);
    px(gmb_tui_add_chart(1040, 280, 300, 140, "12;18;15;22;19;25;21;28;24;30"), 300, 140);
    px(gmb_tui_add_candlestick(1040, 440, 300, 120,
                               "10,12,9,11;11,14,10,13;13,15,12,14;14,16,13,15;15,17,14,16"), 300, 120);
    px(gmb_tui_add_qr(726, 580, 100, 100, "https://gmbasic.dev"), 100, 100);
    px(gmb_tui_add_imagebutton(850, 580, 160, 40, "Immagine"), 160, 40);
    px(gmb_tui_add_dashboard(1040, 580, 300, 120, "Utenti|1280|12|verde|Ricavi|42.5|8|verde"), 300, 120);
    px(gmb_tui_add_splitter(726, 700, 614, 90, 0), 614, 90);
    px(gmb_tui_add_fkeybar(36, 830, 1328, "1Aiuto|2Salva|3Carica|4Cerca|5Esegui|6Modifica"), 1328, 24);
    px(gmb_tui_add_statusbar(36, 858, 1328, "F1 Aiuto    F2 Salva    F10 Menu    Alt+X Esci"), 1328, 24);

    gmb_tui_px_invalidate();
    gmb_tui_draw_px_all();
}

int main(int argc, char** argv)
{
    const char* mode = argc > 1 ? argv[1] : "tui";
    const char* out = argc > 2 ? argv[2] : "tui_gallery.png";
    const char* theme = (argc > 3 && strcmp(argv[3], "dump") != 0) ? argv[3] : "turbovision";
    int dump = (argc > 3 && strcmp(argv[3], "dump") == 0);

    /* `show`: FINESTRA REALE (nessun dummy). Le altre modalità sono headless. */
    if (strcmp(mode, "show") == 0)
    {
        int seconds = argc > 2 ? atoi(argv[2]) : 45;
        if (seconds <= 0) seconds = 45;
        gmb_ui_set_theme_name(theme);
        gmb_gfx_present_init_main_thread();
        gmb_gfx_screen(12, 1400, 900, 32);
        gmb_theme_t th = gmb_ui_theme();
        gmb_gfx_cls((th.desktop_bg.r << 16) | (th.desktop_bg.g << 8) | th.desktop_bg.b);
        gfx_window();
        gmb_gfx_present_tick();
        int frames = seconds * 60;   /* ~16 ms per frame */
        for (int i = 0; i < frames; i++)
        {
            if (gmb_gfx_present_should_quit()) break;
            gmb_tui_draw_px_all();
            gmb_gfx_present_tick();
            gmb_sleep_ms(16);
        }
        gmb_gfx_present_stop();
        return 0;
    }

    setenv("SDL_VIDEODRIVER", "dummy", 1);   /* headless: nessuna finestra reale */
    gmb_ui_set_theme_name(theme);

    if (strcmp(mode, "tui") == 0)
    {
        gmb_screen_config_t cfg = gmb_screen_config_get_default(GMB_SM_TEXT);
        cfg.char_grid_width = 120;
        cfg.char_grid_height = 39;
        gmb_sdl_console_t* con = gmb_sdl_console_new(&cfg, 120, 39);
        if (!con) { fprintf(stderr, "console NULL\n"); return 1; }
        g_buf = gmb_sdl_console_screen(con);
        tui_gallery();
        if (dump)
        {
            for (int yy = 16; yy <= 24; yy++)
            {
                for (int xx = 50; xx < 110; xx++)
                {
                    int cc[10]; gmb_screen_get_cell(g_buf, xx, yy, cc);
                    fputc(cc[0] ? (cc[0] < 128 ? cc[0] : '|') : ' ', stderr);
                }
                fputc('\n', stderr);
            }
        }
        int rc = gmb_sdl_console_save_png(con, out);
        fprintf(stderr, "TUI png rc=%d %s\n", rc, out);
        return rc == 0 ? 0 : 1;
    }

    gmb_gfx_present_init_main_thread();
    gmb_gfx_screen(12, 1400, 900, 32);
    gmb_theme_t th = gmb_ui_theme();
    gmb_gfx_cls((th.desktop_bg.r << 16) | (th.desktop_bg.g << 8) | th.desktop_bg.b);
    if (strcmp(mode, "window") == 0) gfx_window();
    else gfx_gallery();
    int rc = gmb_gfx_save_screen_png(out);
    fprintf(stderr, "GFX png rc=%d %s\n", rc, out);
    return rc == 0 ? 0 : 1;
}
