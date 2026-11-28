/* ============================================================================
 * gmb_color_filter_selfcheck.c — filtro daltonia (parte deterministica).
 *
 * Verifica: parsing dei nomi, interruttore dal registro, e la PURA trasformazione
 * (matrici): identità quando spento, luminanza conservata per l'acromatopsia,
 * rosso/verde resi distinguibili per deuteranopia e protanopia, clamp nei bordi.
 * Tutto senza disegnare: i punti di uscita (celle TUI, sink pixel, testo gfx,
 * ruoli del tema) chiamano esattamente questa funzione.
 * ============================================================================ */
#include "gmb_color_filter.h"
#include "gmb_runtime.h"     /* gmb_opt_set: registro opzioni */

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

static int near8(int a, int b, int tol) { int d = a - b; if (d < 0) d = -d; return d <= tol; }

static int col_eq(gmb_cell_color_t a, gmb_cell_color_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.is_default == b.is_default;
}

/* Distanza percettiva grezza: usata solo per dire "ora i due colori si
 * distinguono", non per validare una resa clinica. */
static int col_dist(gmb_cell_color_t a, gmb_cell_color_t b)
{
    int dr = (int)a.r - (int)b.r, dg = (int)a.g - (int)b.g, db = (int)a.b - (int)b.b;
    return dr * dr + dg * dg + db * db;
}

int main(void)
{
    /* ---- parsing dei nomi ------------------------------------------------ */
    check(gmb_color_filter_parse(NULL) == GMB_CVF_NONE, "parse: NULL = none");
    check(gmb_color_filter_parse("") == GMB_CVF_NONE, "parse: vuoto = none");
    check(gmb_color_filter_parse("none") == GMB_CVF_NONE, "parse: none");
    check(gmb_color_filter_parse("0") == GMB_CVF_NONE, "parse: 0 = spento");
    check(gmb_color_filter_parse("off") == GMB_CVF_NONE, "parse: off = spento");
    check(gmb_color_filter_parse("Deuteranopia") == GMB_CVF_DEUTERANOPIA, "parse: case-insensitive");
    check(gmb_color_filter_parse("deut") == GMB_CVF_DEUTERANOPIA, "parse: prefisso deut");
    check(gmb_color_filter_parse("prot") == GMB_CVF_PROTANOPIA, "parse: prefisso prot");
    check(gmb_color_filter_parse("trit") == GMB_CVF_TRITANOPIA, "parse: prefisso trit");
    check(gmb_color_filter_parse("achrom") == GMB_CVF_ACHROMATOPSIA, "parse: prefisso achrom");
    check(gmb_color_filter_parse("gray") == GMB_CVF_ACHROMATOPSIA, "parse: gray = acromatopsia");
    check(gmb_color_filter_parse("verde") == GMB_CVF_DEUTERANOPIA, "parse: verde");
    check(gmb_color_filter_parse("sconosciuto") == -1, "parse: sconosciuto = -1 (non si indovina)");
    check(!strcmp(gmb_color_filter_name(GMB_CVF_TRITANOPIA), "tritanopia"), "name: tritanopia");
    check(!strcmp(gmb_color_filter_name(99), "none"), "name: fuori intervallo = none");

    /* ---- interruttore dal registro --------------------------------------- */
    check(gmb_color_filter_active() == 0, "default: filtro spento");
    check(gmb_opt_set(GMB_COLOR_FILTER_OPT, "deuteranopia") == 0, "opt: set deuteranopia");
    check(gmb_color_filter_active() == 1, "opt: filtro acceso");
    check(gmb_color_filter_mode() == GMB_CVF_DEUTERANOPIA, "opt: modo deuteranopia");
    gmb_opt_set(GMB_COLOR_FILTER_OPT, "spazzatura");
    check(gmb_color_filter_active() == 0, "opt: valore sconosciuto = spento (mai a caso)");
    gmb_opt_set(GMB_COLOR_FILTER_OPT, "achromatopsia");

    /* ---- spento: identità ------------------------------------------------- */
    {
        gmb_cell_color_t c = { 200, 30, 90, 0 };
        gmb_cell_color_t o = gmb_color_filter_cell(c, GMB_CVF_NONE);
        check(col_eq(c, o), "none: colore invariato");
        check(gmb_color_filter_rgba(0x123456, GMB_CVF_NONE) == 0x123456, "none: rgba invariato");
    }

    /* ---- default del terminale non si filtra ------------------------------ */
    {
        gmb_cell_color_t d = GMB_CELLCOLOR_DEFAULT;
        gmb_cell_color_t o = gmb_color_filter_cell(d, GMB_CVF_DEUTERANOPIA);
        check(col_eq(d, o), "default: is_default non trasformato");
    }

    /* ---- acromatopsia: luminanza Rec.709, canali uguali ------------------- */
    {
        gmb_cell_color_t c = { 255, 0, 0, 0 };
        gmb_cell_color_t o = gmb_color_filter_cell(c, GMB_CVF_ACHROMATOPSIA);
        check(o.r == o.g && o.g == o.b, "achrom: canali uguali");
        check(near8(o.r, 54, 1), "achrom: luminanza Rec.709 del rosso (0.2126*255 = 54)");
        gmb_cell_color_t w = { 255, 255, 255, 0 };
        gmb_cell_color_t ow = gmb_color_filter_cell(w, GMB_CVF_ACHROMATOPSIA);
        check(ow.r == 255 && ow.g == 255 && ow.b == 255, "achrom: bianco resta bianco");
    }

    /* ---- deuteranopia / protanopia: rosso e verde si distinguono --------- */
    {
        gmb_cell_color_t red   = { 220, 20, 20, 0 };
        gmb_cell_color_t green = { 20, 180, 40, 0 };
        gmb_cell_color_t dr = gmb_color_filter_cell(red,   GMB_CVF_DEUTERANOPIA);
        gmb_cell_color_t dg = gmb_color_filter_cell(green, GMB_CVF_DEUTERANOPIA);
        gmb_cell_color_t pr = gmb_color_filter_cell(red,   GMB_CVF_PROTANOPIA);
        gmb_cell_color_t pg = gmb_color_filter_cell(green, GMB_CVF_PROTANOPIA);
        /* Il filtro NON li rende uguali (non è un "grigio tutto"): la matrice
         * sposta il rosso del verde simulando la perdita, e la differenza resta. */
        check(col_dist(dr, dg) > 300, "deut: rosso e verde restano distinguibili");
        check(col_dist(pr, pg) > 300, "prot: rosso e verde restano distinguibili");
        check(!col_eq(dr, red), "deut: la matrice agisce (rosso trasformato)");
        check(!col_eq(pg, green), "prot: la matrice agisce (verde trasformato)");
    }

    /* ---- portata: nessun canale fuori 0..255 ------------------------------ */
    {
        int ok = 1;
        for (int m = GMB_CVF_DEUTERANOPIA; m <= GMB_CVF_ACHROMATOPSIA; m++)
            for (int v = 0; v < 256; v += 17) {
                gmb_cell_color_t c = { (uint8_t)v, (uint8_t)(255 - v), (uint8_t)((v * 7) & 0xFF), 0 };
                (void)gmb_color_filter_cell(c, m);   /* uint8_t: illimitato è già impossibile */
                gmb_cell_color_t s = { 255, 255, 255, 0 };
                gmb_cell_color_t os = gmb_color_filter_cell(s, m);
                if (os.r > 255) ok = 0;   /* promemoria: il clamp è nel tipo e in clamp255 */
            }
        check(ok, "clamp: nessun canale fuori intervallo");
    }

    /* ---- rgba: stesso risultato del percorso cella ------------------------ */
    {
        gmb_cell_color_t c = { 10, 200, 60, 0 };
        gmb_cell_color_t o = gmb_color_filter_cell(c, GMB_CVF_TRITANOPIA);
        uint32_t rgb = gmb_color_filter_rgba(0x0AC83C, GMB_CVF_TRITANOPIA);
        check(((rgb >> 16) & 0xFF) == o.r && ((rgb >> 8) & 0xFF) == o.g && (rgb & 0xFF) == o.b,
              "rgba: coerente con cell");
        /* 0xAARRGGBB: l'alpha/flag dei colori grafici NON si perde (le superfici
         * traslucide dei temi restano tali) — è il caso del bug "chrome sparita". */
        check((gmb_color_filter_rgba(0x4012AB34, GMB_CVF_DEUTERANOPIA) & 0xFF000000u) == 0x40000000u,
              "rgba: byte alto (alpha/flag) conservato");
    }

    /* ---- set forzato (test): prevale finché l'opzione non cambia ---------- */
    {
        gmb_opt_set(GMB_COLOR_FILTER_OPT, "achromatopsia");
        (void)gmb_color_filter_mode();
        gmb_color_filter_set(GMB_CVF_DEUTERANOPIA);
        check(gmb_color_filter_mode() == GMB_CVF_DEUTERANOPIA, "set: modo forzato");
        gmb_opt_set(GMB_COLOR_FILTER_OPT, "protanopia");
        check(gmb_color_filter_mode() == GMB_CVF_PROTANOPIA, "set: il registro cambia → torna la verità del registro");
        gmb_color_filter_set(-1);
        check(gmb_color_filter_mode() == GMB_CVF_PROTANOPIA, "set(-1): rilegge il registro");
        gmb_opt_set(GMB_COLOR_FILTER_OPT, "none");
        check(gmb_color_filter_active() == 0, "fine: filtro spento");
    }

    if (g_fails) { printf("%d verifiche fallite\n", g_fails); return 1; }
    printf("gmb_color_filter_selfcheck: OK\n");
    return 0;
}