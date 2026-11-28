/* ============================================================================
 * gmb_gfx_textscreen_selfcheck.c — P7 blocco 4: schermo di testo 80×25 a celle.
 *
 * Ogni check è una regola del contratto di `gmb_gfx_textscreen_*`
 * (include/gmb_graphics.h): griglia 80×25 di celle 8×12, penna e carta PER CELLA,
 * cursore con a capo automatico, TAB a zone di 8, scroll in fondo, `render`
 * deterministico dal bitmap ASCII 8×16 e modalità spenta che non scrive e non disegna.
 *
 * Niente schermo e niente SDL per la parte di modello/DISEGNO: il render va su un
 * canvas offscreen 640×300 (le celle sono 1:1). L'ultimo check usa `gmb_gfx_screen`
 * solo per la regola di ridimensionamento della pagina.
 *
 * Eseguito da: ctest -R gmb_gfx_textscreen_selfcheck
 * ============================================================================ */
#include "gmb_graphics.h"

#include <stdio.h>

/* Colori del linguaggio: 0x40000000 = RGB diretto (vedi resolve_color). */
#define C_RGB(rgb) ((int)(0x40000000u | (unsigned)(rgb)))

#define WHITE 0xFFFFFFu
#define BLACK 0x000000u
#define RED   0xFF0000u
#define GREEN 0x00FF00u
#define BLUE  0x0000FFu

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

static unsigned rgb_of(int packed) { return (unsigned)packed & 0xFFFFFFu; }

/* Cella: carattere e colori in un colpo solo (0 = fuori griglia). */
static int cell_at(int col, int row, int* ch, unsigned* fg, unsigned* bg)
{
    *ch = -1;
    *fg = *bg = 0xDEADu;
    return gmb_gfx_textscreen_cell(col, row, ch, fg, bg);
}

int main(void)
{
    int cvs;

    /* ---- 1. sorgente e geometria: spento, 80×25 di celle 8×12 ---- */
    CHECK(gmb_gfx_textscreen_on() == 0, "la modalità è spenta finché non la si accende");
    CHECK(gmb_gfx_textscreen_cols() == 80, "80 colonne");
    CHECK(gmb_gfx_textscreen_rows() == 25, "25 righe");
    CHECK(gmb_gfx_textscreen_cell_w() == 8, "cella larga 8 px");
    CHECK(gmb_gfx_textscreen_cell_h() == 12, "cella alta 12 px");
    CHECK(gmb_gfx_textscreen_write("A") == 0, "a modo spento non si scrive (nessun no-op silenzioso: ritorna 0)");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 0, &ch, &fg, &bg) == 1, "la griglia si può leggere anche a modo spento");
        CHECK(ch == ' ' && fg == WHITE && bg == BLACK, "griglia iniziale bianca su nero");
    }
    CHECK(gmb_gfx_textscreen_cell(80, 0, NULL, NULL, NULL) == 0, "colonna 80 fuori griglia");
    CHECK(gmb_gfx_textscreen_cell(0, 25, NULL, NULL, NULL) == 0, "riga 25 fuori griglia");
    CHECK(gmb_gfx_textscreen_cell(-1, 0, NULL, NULL, NULL) == 0, "colonna negativa fuori griglia");

    /* ---- 2. accensione: griglia azzerata, cursore in alto a sinistra, 2000 celle ---- */
    CHECK(gmb_gfx_textscreen_set(1) == 1, "set(1) accende la modalità");
    CHECK(gmb_gfx_textscreen_on() == 1, "ON dopo set(1)");
    CHECK(gmb_gfx_textscreen_cursor_x() == 0 && gmb_gfx_textscreen_cursor_y() == 0, "cursore in 0,0");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(79, 24, &ch, &fg, &bg) == 1, "l'ultima cella esiste");
        CHECK(ch == ' ' && fg == WHITE && bg == BLACK, "ultima cella vuota, carta nera");
    }

    /* ---- 3. scrittura al cursore, con penna/carta PER CELLA ---- */
    CHECK(gmb_gfx_textscreen_write("AB") == 2, "due caratteri scritti");
    CHECK(gmb_gfx_textscreen_cursor_x() == 2 && gmb_gfx_textscreen_cursor_y() == 0, "il cursore è avanzato di 2");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 0, &ch, &fg, &bg) == 1 && ch == 'A' && fg == WHITE && bg == BLACK,
              "cella 0 = 'A' coi colori correnti");
        CHECK(cell_at(1, 0, &ch, &fg, &bg) == 1 && ch == 'B', "cella 1 = 'B'");
    }
    gmb_gfx_textscreen_pen(C_RGB(RED), C_RGB(BLUE));
    CHECK(gmb_gfx_textscreen_write("C") == 1, "un carattere coi nuovi colori");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(2, 0, &ch, &fg, &bg) == 1 && ch == 'C' && fg == RED && bg == BLUE,
              "penna rossa su carta blu");
        CHECK(cell_at(0, 0, &ch, &fg, &bg) == 1 && fg == WHITE && bg == BLACK,
              "le celle già scritte NON cambiano colore (penna/carta per cella)");
    }
    gmb_gfx_textscreen_pen(-1, -1);
    CHECK(gmb_gfx_textscreen_write("D") == 1, "-1 lascia la penna com'era");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(3, 0, &ch, &fg, &bg) == 1 && ch == 'D' && fg == RED && bg == BLUE, "'D' rosso su blu");
    }

    /* ---- 4. controlli: \r ignorato, \n a capo in colonna 0 ---- */
    CHECK(gmb_gfx_textscreen_write("\rE") == 1, "\\r non produce caratteri");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(4, 0, &ch, &fg, &bg) == 1 && ch == 'E', "\\r ignorato: 'E' resta sulla riga 0");
    }
    CHECK(gmb_gfx_textscreen_write("\nF") == 1, "a capo più un carattere");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 1, &ch, &fg, &bg) == 1 && ch == 'F', "\\n porta in colonna 0 della riga dopo");
        CHECK(gmb_gfx_textscreen_cursor_x() == 1 && gmb_gfx_textscreen_cursor_y() == 1, "cursore dopo 'F'");
    }

    /* ---- 5. a capo automatico a fine riga ---- */
    gmb_gfx_textscreen_locate(79, 0);
    CHECK(gmb_gfx_textscreen_write("XY") == 2, "due caratteri a fine riga");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(79, 0, &ch, &fg, &bg) == 1 && ch == 'X', "'X' nell'ultima colonna");
        CHECK(cell_at(0, 1, &ch, &fg, &bg) == 1 && ch == 'Y', "a capo automatico: 'Y' in colonna 0 della riga 1");
        CHECK(gmb_gfx_textscreen_cursor_x() == 1 && gmb_gfx_textscreen_cursor_y() == 1, "cursore dopo il capo automatico");
    }

    /* ---- 6. TAB = zona di 8 colonne, riempita di spazi ---- */
    gmb_gfx_textscreen_locate(1, 2);
    CHECK(gmb_gfx_textscreen_write("\tZ") == 8, "TAB dalla colonna 1 = 7 spazi + 'Z'");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(1, 2, &ch, &fg, &bg) == 1 && ch == ' ', "il TAB riempie di spazi");
        CHECK(cell_at(7, 2, &ch, &fg, &bg) == 1 && ch == ' ', "fino alla colonna 7");
        CHECK(cell_at(8, 2, &ch, &fg, &bg) == 1 && ch == 'Z', "'Z' alla colonna 8");
        CHECK(gmb_gfx_textscreen_cursor_x() == 9, "cursore dopo 'Z'");
    }
    gmb_gfx_textscreen_locate(8, 2);
    CHECK(gmb_gfx_textscreen_write("\t") == 8, "TAB su colonna già multipla di 8 = 8 spazi");

    /* ---- 7. scroll automatico arrivando in fondo ---- */
    gmb_gfx_textscreen_locate(0, 24);
    CHECK(gmb_gfx_textscreen_write("BOTTOM") == 6, "scrittura sull'ultima riga");
    CHECK(gmb_gfx_textscreen_write("\nNEXT") == 4, "a capo dall'ultima riga: scroll");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 23, &ch, &fg, &bg) == 1 && ch == 'B', "la riga 24 è scorsa in 23");
        CHECK(cell_at(0, 24, &ch, &fg, &bg) == 1 && ch == 'N', "il testo nuovo è sull'ultima riga");
        CHECK(cell_at(10, 24, &ch, &fg, &bg) == 1 && ch == ' ', "la coda dell'ultima riga è vuota");
        CHECK(bg == BLUE, "le righe liberate prendono la carta corrente");
        CHECK(gmb_gfx_textscreen_cursor_y() == 24 && gmb_gfx_textscreen_cursor_x() == 4, "cursore dopo lo scroll");
    }

    /* ---- 8. clear: celle vuote coi colori correnti, cursore a casa ---- */
    CHECK(gmb_gfx_textscreen_clear() == 1, "clear in modalità accesa");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 0, &ch, &fg, &bg) == 1 && ch == ' ' && fg == RED && bg == BLUE,
              "clear usa penna/carta correnti");
        CHECK(cell_at(79, 24, &ch, &fg, &bg) == 1 && ch == ' ', "anche l'ultima cella è vuota");
        CHECK(gmb_gfx_textscreen_cursor_x() == 0 && gmb_gfx_textscreen_cursor_y() == 0, "cursore a casa dopo clear");
    }

    /* ---- 9. scroll esplicito ---- */
    gmb_gfx_textscreen_locate(0, 0);
    CHECK(gmb_gfx_textscreen_write("Q") == 1, "'Q' in alto a sinistra");
    CHECK(gmb_gfx_textscreen_scroll(1) == 1, "scroll(1) di una riga");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 0, &ch, &fg, &bg) == 1 && ch == ' ', "dopo lo scroll la riga 0 è vuota");
        CHECK(cell_at(0, 24, &ch, &fg, &bg) == 1 && ch == ' ' && bg == BLUE, "l'ultima riga è carta corrente");
    }
    CHECK(gmb_gfx_textscreen_scroll(0) == 0, "scroll(0) non fa nulla e lo dice");
    CHECK(gmb_gfx_textscreen_scroll(999) == 25, "scroll oltre il fondo si ferma a 25 righe");

    /* ---- 10. render: 2000 celle su canvas, glifi dal bitmap ---- */
    gmb_gfx_textscreen_pen(C_RGB(RED), C_RGB(BLUE));
    CHECK(gmb_gfx_textscreen_set(1) == 1, "set(1) riazzera la griglia coi colori correnti");
    CHECK(gmb_gfx_textscreen_write("H") == 1, "'H' nella prima cella");
    cvs = gmb_gfx_canvas_new(640, 300);
    CHECK(cvs > 0, "canvas offscreen 640×300");
    CHECK(gmb_gfx_canvas_use(cvs) == 0, "canvas attivo");
    CHECK(gmb_gfx_textscreen_render() == 2000, "render di 2000 celle");
    CHECK(rgb_of(gmb_gfx_canvas_get(cvs, 0, 0)) == BLUE, "la cella 0 parte dalla carta");
    CHECK(rgb_of(gmb_gfx_canvas_get(cvs, 8, 0)) == BLUE, "la cella 1 (vuota) è tutta carta");
    CHECK(rgb_of(gmb_gfx_canvas_get(cvs, 639, 299)) == BLUE, "l'ultima cella della griglia è disegnata");
    {
        int pen_px = 0, other_px = 0;
        for (int y = 0; y < 12; y++)
            for (int x = 0; x < 8; x++)
            {
                if (rgb_of(gmb_gfx_canvas_get(cvs, x, y)) == RED) pen_px++;
                if (rgb_of(gmb_gfx_canvas_get(cvs, 8 + x, y)) == RED) other_px++;
            }
        CHECK(pen_px > 0, "il glifo 'H' ha pixel di penna nella cella 0");
        CHECK(other_px == 0, "la cella vuota accanto non ha pixel di penna");
    }
    /* il rendering è idempotente: rifarlo non cambia i pixel */
    CHECK(gmb_gfx_textscreen_render() == 2000, "secondo render");
    CHECK(rgb_of(gmb_gfx_canvas_get(cvs, 0, 0)) == BLUE, "il secondo render lascia la stessa carta");

    /* ---- 11. modalità spenta: niente scritture, niente disegno, griglia conservata ---- */
    CHECK(gmb_gfx_textscreen_set(0) == 0, "set(0) spegne");
    CHECK(gmb_gfx_textscreen_on() == 0, "OFF dopo set(0)");
    CHECK(gmb_gfx_textscreen_write("X") == 0, "a modo spento non si scrive");
    CHECK(gmb_gfx_textscreen_clear() == 0, "a modo spento clear non fa nulla");
    CHECK(gmb_gfx_textscreen_scroll(1) == 0, "a modo spento scroll non fa nulla");
    {
        int ch; unsigned fg, bg;
        CHECK(cell_at(0, 0, &ch, &fg, &bg) == 1 && ch == 'H', "la griglia resta: 'H' è ancora lì");
    }
    CHECK(gmb_gfx_canvas_clear(cvs, 0) == 0, "canvas ripulito a nero");
    CHECK(gmb_gfx_textscreen_render() == 0, "a modo spento render non disegna");
    CHECK(rgb_of(gmb_gfx_canvas_get(cvs, 0, 0)) == 0, "la pagina è rimasta intatta (nero)");
    gmb_gfx_canvas_use(0);

    /* ---- 12. con uno schermo, la pagina diventa 640×300 nativi ---- */
    gmb_gfx_screen(13, 320, 200, 32);
    CHECK(gmb_gfx_textscreen_set(1) == 1, "set(1) con schermo attivo");
    {
        float sc = gmb_gfx_render_scale();
        if (sc < 1.0f) sc = 1.0f;
        CHECK((int)(gmb_gfx_width() * sc + 0.5f) == 640, "larghezza pagina = 80×8");
        CHECK((int)(gmb_gfx_height() * sc + 0.5f) == 300, "altezza pagina = 25×12");
    }
    CHECK(gmb_gfx_textscreen_set(0) == 0, "chiusura della modalità");

    printf("P7-TEXTSCREEN ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
