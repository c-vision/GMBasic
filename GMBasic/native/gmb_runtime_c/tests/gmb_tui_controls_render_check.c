/* gmb_tui_controls_render_check.c — HEADLESS: rende OGNI controllo del registry in un
 * buffer pulito (tema `turbovision`) e verifica:
 *   1. nessun carattere fuori dal set atteso (ASCII, box, blocchi, frecce, geometria,
 *      stelle/bullet);
 *   2. nessuna cella con testo in cui fg == bg (testo invisibile, es. sel_fg == sel_bg).
 *
 * Uso:  gmb_tui_controls_render_check [--verbose]
 * Exit: 0 = tutti i controlli puliti, 1 = almeno una violazione.
 *
 * Non apre finestre e non tocca SDL: only cell buffer (headless puro). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "gmb_tui_app.h"
#include "gmb_tui_widgets.h"

static int fails = 0;
static int verbose = 0;

static int emit_cp(unsigned cp)
{
    if (cp < 0x80) { putchar((int)cp); return 1; }
    if (cp < 0x800) { putchar(0xC0 | (cp >> 6)); putchar(0x80 | (cp & 0x3F)); return 2; }
    putchar(0xE0 | (cp >> 12)); putchar(0x80 | ((cp >> 6) & 0x3F)); putchar(0x80 | (cp & 0x3F)); return 3;
}

/* Caratteri ammessi nell'output TUI: ASCII stampabile, box-drawing, blocchi,
 * frecce/geometria, stelle/bullet. */
static int cp_ok(unsigned cp)
{
    if (cp >= 0x20 && cp < 0x7F) return 1;
    if (cp >= 0x2500 && cp <= 0x259F) return 1;
    if (cp >= 0x2190 && cp <= 0x21FF) return 1;
    if (cp >= 0x25A0 && cp <= 0x25FF) return 1;
    if (cp == 0x2022 || cp == 0x25CF || cp == 0x25CB || cp == 0x2605 || cp == 0x2606) return 1;
    return 0;
}

static void check(const char* name, gmb_control_t* c)
{
    if (!c) { printf("[MISS] %-14s NULL\n", name); fails++; return; }
    gmb_screen_buffer_t* b = gmb_screen_new(80, 25);
    gmb_screen_clear(b, crgb(0, 0, 0), crgb(0, 0, 0));
    gmb_control_draw(c, b);
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (w > 78) w = 78;
    if (h > 23) h = 23;
    int bad = 0, invisible = 0;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
        {
            gmb_screen_cell_t* cl = &gmb_screen_cells(b)[yy * 80 + xx];
            unsigned cp = cl->character ? cl->character : ' ';
            if (!cp_ok(cp)) { bad++; if (verbose) printf("  BAD  %s (%d,%d) cp=U+%04X\n", name, xx, yy, cp); }
            /* Il filler grafico (blocchi/box) e' spesso fg==bg per costruzione: conta solo
             * il TESTO, cioè i caratteri non grafici. */
            if (cp < 0x2500 || cp > 0x259F)
                if (cp > ' ' && cl->fg_rgb.r == cl->bg_rgb.r && cl->fg_rgb.g == cl->bg_rgb.g
                    && cl->fg_rgb.b == cl->bg_rgb.b)
                { invisible++; if (verbose) printf("  INVIS %s (%d,%d) cp=%u\n", name, xx, yy, cp); }
        }
    printf("[%s] %-14s kind=%-2d %dx%d bad=%d invis=%d\n", (bad || invisible) ? "FAIL" : "ok  ",
           name, c->ctrl_kind, c->width, c->height, bad, invisible);
    if (bad || invisible) fails++;
    if (verbose)
        for (int yy = y; yy < y + h; yy++)
        {
            printf("  |");
            for (int xx = x; xx < x + w; xx++)
            { unsigned cp = gmb_screen_cells(b)[yy * 80 + xx].character; emit_cp(cp ? cp : ' '); }
            printf("|\n");
        }
    gmb_screen_free(b);
}

int main(int argc, char** argv)
{
    verbose = (argc > 1 && strcmp(argv[1], "--verbose") == 0);
    gmb_ui_set_theme_name("turbovision");
    gmb_tui_show_markers = 0;
    gmb_tui_controls_reset();

    check("button",       gmb_tui_add_button(2, 1, 14, "OK"));
    check("button-def",   gmb_tui_add_button(2, 1, 14, "Cancel"));
    check("checkbox",     gmb_tui_add_checkbox(2, 1, 24, "Abilita notifiche"));
    check("toggle",       gmb_tui_add_toggle(2, 1, 24, "Modalita esperto"));
    check("listbox",      gmb_tui_add_listbox(2, 1, 20, 3, "alpha|beta|gamma"));
    check("combobox",     gmb_tui_add_combobox(2, 1, 20, "uno|due|tre"));
    check("radio",        gmb_tui_add_radiogroup(2, 1, 20, 3, "Testo|Grafico|Auto"));
    check("tab",          gmb_tui_add_tab(2, 1, 30, "File|Modifica"));
    check("menu",         gmb_tui_add_menu(0, 0, 40, "File|Edit"));
    check("spinner",      gmb_tui_add_spinner(2, 1, 12, "line"));
    check("slider",       gmb_tui_add_slider(2, 1, 20, 0, 100, 65));
    check("scrollbar",    gmb_tui_add_scrollbar(2, 1, 20, 0, 100, 40));
    check("label",        gmb_tui_add_label(2, 1, 20, "Etichetta"));
    check("textinput",    gmb_tui_add_textinput(2, 1, 20, "Mario Rossi"));
    check("logview",      gmb_tui_add_logview(2, 1, 20, 3, "uno|due|tre"));
    check("hexview",      gmb_tui_add_hexview(2, 1, 20, 2, "AB"));
    check("bigtext",      gmb_tui_add_bigtext(2, 1, "42"));
    check("codearea",     gmb_tui_add_codearea(2, 1, 24, 3, "riga uno|riga due"));
    check("alertbox",     gmb_tui_add_alertbox(2, 1, 24, 5, "Messaggio"));
    check("confbox",      gmb_tui_add_confirmationbox(2, 1, 24, 5, "Sicuro?"));
    check("statusbar",    gmb_tui_add_statusbar(0, 1, 40, "F1 Aiuto|F2 Salva"));
    check("starrating",   gmb_tui_add_starrating(2, 1, 16, 5, 3));
    check("numupdown",    gmb_tui_add_numericupdown(2, 1, 12, 0, 100, 10));
    check("grid",         gmb_tui_add_grid(2, 1, 24, 3, 2, "a|b|c|d"));
    check("treeview",     gmb_tui_add_treeview(2, 1, 20, 3, "root|figlio"));
    check("datepicker",   gmb_tui_add_datepicker(2, 1, 20));
    check("colorpicker",  gmb_tui_add_colorpicker(2, 1, 20, 3));
    check("dashboard",    gmb_tui_add_dashboard(2, 1, 30, 4, "CPU|42|100|%"));
    check("splitter",     gmb_tui_add_splitter(2, 1, 20, 3, 0));
    check("propertygrid", gmb_tui_add_propertygrid(2, 1, 24, 3, "a|1|b|2"));
    check("reorderlist",  gmb_tui_add_reorderlistbox(2, 1, 20, 3, "x|y|z"));
    check("scrollview",   gmb_tui_add_scrollview(2, 1, 20, 3, "uno|due|tre"));
    check("contextmenu",  gmb_tui_add_contextmenu(2, 1, 16, 3, "Apri|Salva"));

    printf("\nTOTALE: %d controlli con violazioni\n", fails);
    return fails ? 1 : 0;
}
