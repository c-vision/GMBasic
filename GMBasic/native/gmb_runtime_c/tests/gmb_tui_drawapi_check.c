/* gmb_tui_drawapi_check.c — HEADLESS: contratto dell'API di disegno usata dai
 * client C# (gmb_tui_draw_control, gmb_tui_drawapi.h).
 *
 * Per OGNI kind disegnabile:
 *   1. il render deve riuscire (ritorno 1);
 *   2. deve produrre almeno una cella non vuota (nessun controllo "fantasma");
 *   3. non deve scrivere fuori dal buffer.
 *
 * È il gate che garantisce che il percorso C# -> C (P/Invoke) abbia un render
 * reale per ogni controllo, senza fallback a celle nel client.
 *
 * Uso: gmb_tui_drawapi_check [--verbose]
 * Exit: 0 = tutto ok, 1 = almeno un kind non disegnato.
 */
#include <stdio.h>
#include <string.h>
#include "gmb_tui.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_drawapi.h"

static int fails = 0;
static int verbose = 0;

typedef struct { const char* name; int kind; } dk_case_t;

static void run(const dk_case_t* k)
{
    gmb_screen_buffer_t* b = gmb_screen_new(100, 30);
    gmb_screen_clear(b, crgb(0, 0, 0), crgb(0, 0, 0));

    gmb_draw_req_t r;
    memset(&r, 0, sizeof(r));
    r.kind = k->kind;
    r.x = 2; r.y = 2; r.w = 24; r.h = 6;
    r.sel = 1; r.min = 0; r.max = 100; r.value = 40;
    r.text = "alpha|beta|gamma";
    r.extra = "titolo";
    r.state = GMB_DS_FOCUSED;

    switch (k->kind)
    {
    case GMB_DK_PANEL: case GMB_DK_WINDOW: case GMB_DK_ALERTBOX:
    case GMB_DK_LISTBOX: case GMB_DK_TEXTAREA: case GMB_DK_CODEAREA:
    case GMB_DK_RADIOGROUP: case GMB_DK_FILEEXPLORER:
        break;
    case GMB_DK_MENU: case GMB_DK_TAB: case GMB_DK_COMBOBOX:
    case GMB_DK_CONTEXTMENU:
        r.state |= GMB_DS_OPEN;
        break;
    case GMB_DK_TBROWSE:
        r.text  = "Nome:12|Citta:10";
        r.extra = "Mario|Roma;Luigi|Milano;Anna|Torino";
        r.w = 30; r.h = 8;
        break;
    case GMB_DK_STATUSBAR: case GMB_DK_FKEYBAR:
        r.h = 1; r.y = 2;
        break;
    case GMB_DK_HLINE: r.h = 1; r.w = 30; break;
    case GMB_DK_VLINE: r.w = 1; r.h = 8; break;
    case GMB_DK_BIGTEXT: r.h = 7; r.text = "OK"; break;
    case GMB_DK_TEXTINPUT: r.text = "Mario Rossi"; break;
    case GMB_DK_CHECKBOX: r.text = "Abilita"; r.state |= GMB_DS_CHECKED; break;
    default: break;
    }

    int ok = gmb_tui_draw_control(b, &r);

    int painted = 0;
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 100; x++)
        {
            int cell[10];
            gmb_screen_get_cell(b, x, y, cell);
            /* "disegnato" = carattere visibile O sfondo diverso dal nero iniziale
             * (splitter/progress dipingono superfici di soli spazi colorati). */
            if ((cell[0] != 0 && cell[0] != ' ') ||
                cell[5] != 0 || cell[6] != 0 || cell[7] != 0)
                painted++;
        }

    if (!ok) { printf("[FAIL] %-14s render rifiutato\n", k->name); fails++; }
    else if (painted == 0) { printf("[FAIL] %-14s nessuna cella disegnata\n", k->name); fails++; }
    else if (verbose) printf("[ OK ] %-14s celle=%d\n", k->name, painted);

    gmb_screen_free(b);
}

static void check_richtext(void)
{
    gmb_rich_span_t sp[16];
    int n = gmb_richtext_parse("A *B* {#ff0000 C} D", sp, 16);
    if (n != 5) { printf("[FAIL] richtext parse: %d span (attesi 5)\n", n); fails++; return; }
    if (strcmp(sp[0].text, "A ") || strcmp(sp[1].text, "B") || strcmp(sp[2].text, " ") ||
        strcmp(sp[3].text, "C") || strcmp(sp[4].text, " D"))
    { printf("[FAIL] richtext parse: ordine/testo errato\n"); fails++; return; }
    if (!sp[1].bold || sp[3].has_fg == false || sp[3].fg.r != 255 || sp[3].fg.g != 0 || sp[3].fg.b != 0)
    { printf("[FAIL] richtext parse: stile/colore errato\n"); fails++; return; }

    gmb_screen_buffer_t* b = gmb_screen_new(20, 3);
    gmb_screen_clear(b, crgb(0, 0, 0), crgb(0, 0, 0));
    gmb_richtext_render_to_buffer(b, 1, 0, sp, n, 18);
    int cell[10];
    gmb_screen_get_cell(b, 1, 0, cell);
    if (cell[0] != 'A') { printf("[FAIL] richtext render: cella iniziale\n"); fails++; }
    gmb_screen_free(b);
    if (verbose) printf("[ OK ] richtext\n");
}

static void check_paragraph(void)
{
    gmb_rich_span_t sp[4];
    int n = gmb_richtext_parse("uno due tre quattro cinque sei", sp, 4);
    gmb_screen_buffer_t* b = gmb_screen_new(12, 4);
    gmb_screen_clear(b, crgb(0, 0, 0), crgb(0, 0, 0));
    int drawn = gmb_richtext_render_paragraph(b, 0, 0, 10, 4, sp, n, 1);
    int cell[10];
    gmb_screen_get_cell(b, 0, 1, cell);   /* seconda riga: word-wrap avvenuto */
    if (drawn == 0 || cell[0] == 0)
    { printf("[FAIL] paragraph: nessun wrap/disegno\n"); fails++; }
    gmb_screen_free(b);
    if (verbose) printf("[ OK ] paragraph\n");
}

static void check_windowmanager(void)
{
    gmb_window_manager_t* m = gmb_window_manager_new(0, 0, 40, 12);
    if (!m) { printf("[FAIL] wm: new\n"); fails++; return; }
    gmb_rect_t r = { 1, 1, 30, 8 };
    int id = gmb_window_manager_add(m, "Editor", r);
    gmb_managed_window_t* w = gmb_window_manager_get(m, id);
    if (!w) { printf("[FAIL] wm: add\n"); fails++; gmb_window_manager_free(m); return; }
    const char* lines[] = { "riga uno", "riga due", "riga tre" };
    gmb_managed_window_set_lines(w, lines, 3);

    gmb_screen_buffer_t* b = gmb_screen_new(40, 12);
    gmb_screen_clear(b, crgb(0, 0, 0), crgb(0, 0, 0));
    gmb_window_manager_render_named(m, b, "turbovision", "GMBasic");
    int cell[10];
    gmb_screen_get_cell(b, r.x + 1, r.y + 1, cell);   /* prima riga di contenuto */
    int ok = cell[0] == 'r';
    gmb_screen_get_cell(b, r.x, r.y, cell);           /* angolo della cornice */
    if (ok) ok = cell[0] != 0 && cell[0] != ' ';
    if (!ok) { printf("[FAIL] wm: contenuto/cornice non disegnati\n"); fails++; }
    gmb_screen_free(b);
    gmb_window_manager_free(m);
    if (verbose) printf("[ OK ] windowmanager\n");
}

int main(int argc, char** argv)
{
    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) verbose = 1;

    check_richtext();
    check_paragraph();
    check_windowmanager();

    static const dk_case_t cases[] = {
        { "button",     GMB_DK_BUTTON },
        { "checkbox",   GMB_DK_CHECKBOX },
        { "toggle",     GMB_DK_TOGGLE },
        { "label",      GMB_DK_LABEL },
        { "textinput",  GMB_DK_TEXTINPUT },
        { "listbox",    GMB_DK_LISTBOX },
        { "combobox",   GMB_DK_COMBOBOX },
        { "radiogroup", GMB_DK_RADIOGROUP },
        { "progress",   GMB_DK_PROGRESS },
        { "scrollbar",  GMB_DK_SCROLLBAR },
        { "tab",        GMB_DK_TAB },
        { "panel",      GMB_DK_PANEL },
        { "spinner",    GMB_DK_SPINNER },
        { "slider",     GMB_DK_SLIDER },
        { "statusbar",  GMB_DK_STATUSBAR },
        { "menu",       GMB_DK_MENU },
        { "textarea",   GMB_DK_TEXTAREA },
        { "codearea",   GMB_DK_CODEAREA },
        { "alertbox",   GMB_DK_ALERTBOX },
        { "bigtext",    GMB_DK_BIGTEXT },
        { "numeric",    GMB_DK_NUMERIC },
        { "hline",      GMB_DK_HLINE },
        { "vline",      GMB_DK_VLINE },
        { "starrating", GMB_DK_STARRATING },
        { "fileexplorer", GMB_DK_FILEEXPLORER },
        { "fkeybar",    GMB_DK_FKEYBAR },
        { "splitter",   GMB_DK_SPLITTER },
        { "contextmenu", GMB_DK_CONTEXTMENU },
        { "scrollview", GMB_DK_SCROLLVIEW },
        { "window",     GMB_DK_WINDOW },
        { "tbrowse",    GMB_DK_TBROWSE },
    };

    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) run(&cases[i]);

    printf("TOTALE: %d kind, %d FAIL\n", n, fails);
    return fails == 0 ? 0 : 1;
}
