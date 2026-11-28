/* ============================================================================
 * gmb_gfx_design_selfcheck.c — P7 blocco 5: `DESIGN`, immagini da righe ASCII.
 *
 * Verifica il contratto di `gmb_gfx_design` (include/gmb_graphics.h): righe separate da
 * '\\n', indici di palette base 36, '.'/' ' trasparenti, CRLF e newline finale tollerati,
 * righe disuguali/vuote/caratteri non validi/dimensioni eccessive RIFIUTATI con messaggio,
 * e lo sprite risultante è una texture normale (disegno su canvas, gruppi, collisione).
 *
 * Eseguito da: ctest -R gmb_gfx_design_selfcheck
 * ============================================================================ */
#include "gmb_graphics.h"

#include <stdio.h>

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

static void expect_error_at(const char* rows, const char* what, int line)
{
    int r = gmb_gfx_design(1, rows);
    g_checks++;
    if (r != -1 || gmb_gfx_design_error()[0] == 0)
    {
        printf("FAIL riga %d: %s (r=%d, errore \"%s\")\n", line, what, r, gmb_gfx_design_error());
        g_fails++;
    }
}
#define EXPECT_ERROR(rows, what) expect_error_at((rows), (what), __LINE__)

int main(void)
{
    int cvs;

    /* il runtime grafico va inizializzato PRIMA della palette: gmb_gfx_init azzera lo
       stato, quindi una palette impostata prima verrebbe persa al primo sprite_def */
    gmb_gfx_init();

    /* palette: 0 nero, 1 rosso, 2 verde, 3 blu, 4 giallo */
    gmb_gfx_palette(0, 0, 0, 0);
    gmb_gfx_palette(1, 255, 0, 0);
    gmb_gfx_palette(2, 0, 255, 0);
    gmb_gfx_palette(3, 0, 0, 255);
    gmb_gfx_palette(4, 255, 255, 0);
    /* servono almeno 36 voci perché gli indici base 36 tornino identici da
       sprite_pixels_to_array (che rilegge la palette, non il carattere) */
    for (int i = 5; i < 36; i++)
        gmb_gfx_palette(i, i * 5, i * 9, i * 11);

    /* ---- 1. definizione e fedeltà dei pixel ---- */
    CHECK(gmb_gfx_design(1, "12\n34") == 0, "DESIGN 2×2 da due righe");
    {
        int out[6];
        CHECK(gmb_gfx_sprite_pixels_to_array(1, out, 6) == 6, "lo sprite ha 2×2 pixel");
        CHECK(out[0] == 2 && out[1] == 2, "dimensioni ricavate dalle righe");
        CHECK(out[2] == 1 && out[3] == 2 && out[4] == 3 && out[5] == 4,
              "gli indici di palette sono quelli dei caratteri");
    }

    /* ---- 2. trasparenza: '.' e ' ' non producono pixel ---- */
    CHECK(gmb_gfx_design(2, ".1\n2 ") == 0, "DESIGN con trasparenze");
    {
        int out[6];
        CHECK(gmb_gfx_sprite_pixels_to_array(2, out, 6) == 6, "2×2 con trasparenze");
        CHECK(out[2] == -1, "'.' è trasparente");
        CHECK(out[3] == 1, "cifra accanto al trasparente");
        CHECK(out[4] == 2, "riga sotto");
        CHECK(out[5] == -1, "' ' è trasparente");
    }

    /* ---- 3. base 36: lettere come indici sopra 9 ---- */
    CHECK(gmb_gfx_design(3, "AZ\n9a") == 0, "DESIGN con indici base 36");
    {
        int out[6];
        CHECK(gmb_gfx_sprite_pixels_to_array(3, out, 6) == 6, "2×2 base 36");
        CHECK(out[2] == 10 && out[3] == 35, "A = 10, Z = 35");
        CHECK(out[4] == 9 && out[5] == 10, "9 = 9, 'a' = 10 (minuscole come maiuscole)");
    }

    /* ---- 4. righe disuguali, vuote, caratteri non validi, dimensioni ---- */
    EXPECT_ERROR("12\n345", "righe di larghezza diversa rifiutate");
    EXPECT_ERROR("", "stringa vuota rifiutata");
    EXPECT_ERROR("\n12", "prima riga vuota rifiutata");
    EXPECT_ERROR("1#", "carattere non valido rifiutato");
    EXPECT_ERROR("12\n\n34", "riga vuota in mezzo rifiutata");
    {
        char big[300];
        int i;
        for (i = 0; i < 257; i++) big[i] = '1';
        big[257] = 0;
        EXPECT_ERROR(big, "257 colonne rifiutate (max 256)");
    }
    CHECK(gmb_gfx_design(9999, "1") == -1, "id non valido rifiutato");
    CHECK(gmb_gfx_design_error()[0] != 0, "l'errore di id ha un messaggio");

    /* ---- 5. CRLF e newline finale ---- */
    CHECK(gmb_gfx_design(4, "12\r\n34\r\n") == 0, "CRLF e newline finale tollerati");
    {
        int out[6];
        CHECK(gmb_gfx_sprite_pixels_to_array(4, out, 6) == 6, "CRLF: sempre 2×2");
        CHECK(out[2] == 1 && out[5] == 4, "CRLF: indici corretti");
    }
    CHECK(gmb_gfx_design(5, "1\n2\n") == 0, "newline finale senza riga vuota");

    /* ---- 6. lo sprite è una texture normale: si disegna ---- */
    cvs = gmb_gfx_canvas_new(8, 8);
    CHECK(cvs > 0, "canvas offscreen");
    CHECK(gmb_gfx_canvas_use(cvs) == 0, "canvas attivo");
    gmb_gfx_sprite_set_position(1, 2, 2);
    gmb_gfx_sprite_draw(1);
    CHECK((gmb_gfx_canvas_get(cvs, 1, 1) & 0xFFFFFF) == 0xFF0000, "pixel 1 = rosso di palette");
    CHECK((gmb_gfx_canvas_get(cvs, 2, 1) & 0xFFFFFF) == 0x00FF00, "pixel 2 = verde di palette");
    CHECK((gmb_gfx_canvas_get(cvs, 1, 2) & 0xFFFFFF) == 0x0000FF, "pixel 3 = blu di palette");
    CHECK((gmb_gfx_canvas_get(cvs, 2, 2) & 0xFFFFFF) == 0xFFFF00, "pixel 4 = giallo di palette");
    gmb_gfx_canvas_use(0);

    /* ---- 7. e partecipa ai gruppi come qualunque sprite ---- */
    CHECK(gmb_gfx_sprite_set_size(1, 2, 1) == 0, "blocco 2×1 con lo sprite DESIGN come testa");
    {
        int out[6];
        CHECK(gmb_gfx_sprite_pixels_to_array(2, out, 6) == 6, "il membro mantiene la sua texture");
        CHECK(out[2] == -1 && out[3] == 1, "il membro è lo sprite 2 (con trasparenze)");
    }
    CHECK(gmb_gfx_sprite_set_size(1, 1, 1) == 0, "gruppo sciolto");

    printf("P7-DESIGN ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
