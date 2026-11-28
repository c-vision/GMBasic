/* ============================================================================
 * gmb_graphics_layers_selfcheck.c — P7: layer 2D con palette propria, priorita', backdrop.
 *
 * Ogni check qui sotto e' una regola del contratto di include/gmb_graphics_layers.h (che a
 * sua volta e' la riga "Layer multipli con palette propria + priorita'" della checklist §5
 * del piano 3D). Niente schermo, niente SDL: si compone in un buffer e si guardano i
 * pixel, perche' e' cosi' che la regola diventa verificabile invece che "a occhio".
 *
 * La sezione 11 verifica la PROPRIETA' della composizione: i layer non compongono lo
 * schermo, scrivono in una pagina (qui un canvas headless) con `gmb_layers_blit_page`;
 * senza pagina attiva il ponte risponde -1 invece di toccare uno stato suo.
 *
 * Eseguito da: ctest -R gmb_graphics_layers_selfcheck
 * ============================================================================ */
#include "gmb_graphics_layers.h"
#include "gmb_graphics.h"   /* canvas headless: la pagina in cui i layer depositano */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

#define W 16
#define H 8

static uint32_t g_frame[W * H];

static uint32_t at(int x, int y) { return g_frame[y * W + x]; }

int main(void)
{
    /* ---- 1. creazione, handle stabili, proprieta' ---- */
    gmb_layers_reset();
    int bg = gmb_layers_create(GMB_LAYER_BACKGROUND, W, H, 1);
    int gr = gmb_layers_create(GMB_LAYER_GRAPHICS, W, H, 0);
    CHECK(bg == 1 && gr == 2, "handle 1 e 2 dai primi due slot");
    CHECK(gmb_layers_count() == 2, "due layer vivi");
    CHECK(gmb_layers_kind(gr) == GMB_LAYER_GRAPHICS, "kind conservato");
    CHECK(gmb_layers_width(gr) == W && gmb_layers_height(gr) == H, "dimensioni conservate");
    CHECK(gmb_layers_kind(0) == -1 && gmb_layers_kind(9) == -1, "handle fuori intervallo = -1");
    CHECK(gmb_layers_create(GMB_LAYER_KINDS, W, H, 0) == -1, "kind 5 non esiste");
    CHECK(gmb_layers_create(GMB_LAYER_TILE, 0, H, 0) == -1, "larghezza 0 rifiutata");
    CHECK(gmb_layers_create(GMB_LAYER_TILE, 16385, H, 0) == -1, "oltre il tetto rifiutato");
    CHECK(gmb_layers_free(gr) == 0 && gmb_layers_count() == 1, "free abbassa il conteggio");
    CHECK(gmb_layers_free(gr) == -1, "doppio free = -1");
    CHECK(gmb_layers_create(GMB_LAYER_GRAPHICS, W, H, 0) == gr, "lo slot liberato si riusa");
    gr = 2;

    /* ---- 2. palette di default: grigio pari all'indice ---- */
    CHECK(gmb_layers_palette_entry(bg, 0) == 0x000000u, "default indice 0 = nero");
    CHECK(gmb_layers_palette_entry(bg, 255) == 0xFFFFFFu, "default indice 255 = bianco");
    CHECK(gmb_layers_palette_entry(bg, 200) == 0xC8C8C8u, "default indice 200 = 0xC8C8C8");

    /* ---- 3. la palette e' PER LAYER (non globale come la VGA16) ---- */
    {
        uint32_t p[4] = { 0x000000u, 0xFF0000u, 0x00FF00u, 0x0000FFu };  /* R, G, B */
        CHECK(gmb_layers_set_palette(gr, p, 4) == 4, "palette di 4 voci accettata");
        CHECK(gmb_layers_palette_entry(gr, 1) == 0xFF0000u, "graphics: indice 1 rosso");
        CHECK(gmb_layers_palette_entry(bg, 1) == 0x010101u, "background: indice 1 resta il default");
        CHECK(gmb_layers_set_palette_entry(bg, 1, 0x123456u) == 0, "voce singola");
        CHECK(gmb_layers_palette_entry(bg, 1) == 0x123456u, "voce singola letta");
        CHECK(gmb_layers_palette_entry(bg, 300) == 0xFFFFFFFFu, "indice fuori = 0xFFFFFFFF");
        CHECK(gmb_layers_set_palette(gr, NULL, 4) == -1, "palette NULL rifiutata");
    }

    /* ---- 4. indice 0: trasparente se il layer non e' opaco, colore se lo e' ---- */
    gmb_layers_set_backdrop(0x707070u);
    gmb_layers_clear(bg, 0);          /* background opaco: indice 0 = colore 0 */
    gmb_layers_set_palette_entry(bg, 0, 0x000080u);
    gmb_layers_clear(gr, 0);          /* graphics trasparente: indice 0 lascia sotto */
    CHECK(gmb_layers_composite(g_frame, W, H) == 1, "composizione eseguita");
    CHECK(at(0, 0) == 0x000080u, "background opaco copre il backdrop");
    CHECK(at(W - 1, H - 1) == 0x000080u, "opaco vale su tutto il layer");

    gmb_layers_set(gr, 3, 2, 1);
    gmb_layers_composite(g_frame, W, H);
    CHECK(at(3, 2) == 0xFF0000u, "graphics scrive sopra il background");
    CHECK(at(4, 2) == 0x000080u, "l'indice 0 trasparente lascia il background");

    /* ---- 5. priorita': si disegna in ordine crescente, chi ha piu' vince ---- */
    CHECK(gmb_layers_priority(gr) == GMB_LAYER_GRAPHICS, "priorita' iniziale = kind");
    gmb_layers_set(gr, 5, 2, 2);
    gmb_layers_set(bg, 5, 2, 1);
    gmb_layers_composite(g_frame, W, H);
    CHECK(at(5, 2) == 0x00FF00u, "graphics (3) sopra background (0)");

    CHECK(gmb_layers_set_priority(bg, 10) == 0, "background portato a 10");
    gmb_layers_composite(g_frame, W, H);
    CHECK(at(5, 2) == 0x123456u, "background con priorita' 10 copre graphics");
    CHECK(at(3, 2) == 0x000080u, "e la cella con l'indice 0 del background ora vince su graphics");

    /* riporta l'ordine di default */
    gmb_layers_set_priority(bg, GMB_LAYER_BACKGROUND);

    /* ---- 6. spareggio stabile: stessa priorita' -> ordine di creazione ---- */
    {
        int a = gmb_layers_create(GMB_LAYER_TEXT, W, H, 1);
        int b = gmb_layers_create(GMB_LAYER_TEXT, W, H, 1);
        CHECK(a == 3 && b == 4, "due text negli slot 3 e 4");
        gmb_layers_set_priority(a, 7);
        gmb_layers_set_priority(b, 7);
        gmb_layers_set_palette_entry(a, 5, 0xAA0000u);
        gmb_layers_set_palette_entry(b, 5, 0x00AA00u);
        gmb_layers_set(a, 8, 4, 5);
        gmb_layers_set(b, 8, 4, 5);
        gmb_layers_composite(g_frame, W, H);
        CHECK(at(8, 4) == 0x00AA00u, "a parita' di priorita' vince il creato dopo");
        gmb_layers_set_priority(a, 8);   /* a ora sopra */
        gmb_layers_composite(g_frame, W, H);
        CHECK(at(8, 4) == 0xAA0000u, "alzare la priorita' cambia chi vince");
        gmb_layers_free(a);
        gmb_layers_free(b);
    }

    /* ---- 7. backdrop sotto tutto e clipping di un layer piu' grande del frame ---- */
    {
        int big = gmb_layers_create(GMB_LAYER_TILE, W * 2, H * 2, 0);
        CHECK(big == 3, "layer grande nello slot libero");
        gmb_layers_set_palette_entry(big, 9, 0x00FFFFu);
        gmb_layers_set(big, W + 3, H + 3, 9);   /* fuori dal frame 16x8 */
        gmb_layers_set(big, 1, 1, 9);
        gmb_layers_composite(g_frame, W, H);
        CHECK(at(1, 1) == 0x00FFFFu, "dentro il frame disegna");
        CHECK(at(0, 7) == 0x000080u, "angolo lontano resta il background");
        gmb_layers_free(big);
    }
    gmb_layers_clear(bg, 0);
    gmb_layers_clear(gr, 0);
    gmb_layers_composite(g_frame, W, H);
    CHECK(at(0, 0) == 0x000080u, "backdrop coperto solo da un layer opaco");

    /* ---- 8. dirty: non si ricompone se non e' cambiato niente ---- */
    {
        uint32_t sentinel[W * H];
        for (int i = 0; i < W * H; i++) sentinel[i] = 0xDEADBEu;
        gmb_layers_composite(g_frame, W, H);       /* pulisce i flag */
        CHECK(gmb_layers_touched() == 0, "dopo una composizione nessun layer e' sporco");
        memset(g_frame, 0x11, sizeof(g_frame));
        CHECK(gmb_layers_composite_dirty(g_frame, W, H) == 0, "niente da fare = 0");
        CHECK(at(0, 0) == 0x11111111u, "e il buffer non viene toccato");   /* memset, non colore */

        gmb_layers_set(gr, 2, 3, 1);              /* cambia davvero */
        CHECK(gmb_layers_composite_dirty(g_frame, W, H) == 1, "modifica = 1");
        CHECK(at(2, 3) == 0xFF0000u, "e il pixel nuovo c'e'");

        CHECK(gmb_layers_set(gr, 2, 3, 1) == 0, "riscrivere lo stesso indice e' lecito");
        CHECK(gmb_layers_touched() == 0, "ma non marca sporco (nessun cambio)");
        CHECK(gmb_layers_set(gr, 2, 3, 99) == 0, "indice generico accettato");
        CHECK(gmb_layers_touched() == 1, "un indice diverso marca sporco");
        CHECK(gmb_layers_set(gr, 2, 3, 300) == -1, "indice oltre 255 rifiutato");
        CHECK(gmb_layers_get(gr, 2, 3) == 99, "ovunque si legga l'indice");
        CHECK(gmb_layers_get(gr, -1, 3) == -1 && gmb_layers_set(gr, W, 3, 1) == -1,
              "fuori dal rettangolo = -1 senza effetto");
    }

    /* ---- 9. il cambio di palette o di referee sporca il frame ---- */
    gmb_layers_composite(g_frame, W, H);
    CHECK(gmb_layers_touched() == 0, "pulito");
    gmb_layers_set_palette_entry(bg, 0, 0x200020u);
    CHECK(gmb_layers_touched() == 1, "cambiare una voce di palette sporca");
    gmb_layers_composite(g_frame, W, H);
    CHECK(at(0, 0) == 0x200020u, "e il frame usa il colore nuovo");

    gmb_layers_set_backdrop(0x010203u);
    CHECK(gmb_layers_touched() == 1, "cambiare il backdrop sporca");
    CHECK(gmb_layers_backdrop() == 0x010203u, "backdrop letto");

    /* ---- 10. screenshot: PNG vero, composto come il frame ---- */
    {
        const char* path = "gmb_graphics_layers_selfcheck.png";
        gmb_layers_set_backdrop(0x000000u);
        CHECK(gmb_layers_screenshot(path, W, H) == 0, "screenshot scritto");
        FILE* f = fopen(path, "rb");
        CHECK(f != NULL, "il file esiste");
        if (f)
        {
            unsigned char sig[8] = {0};
            size_t got = fread(sig, 1, 8, f);
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fclose(f);
            CHECK(got == 8 && sig[0] == 137 && sig[1] == 'P' && sig[2] == 'N' && sig[3] == 'G',
                  "firma PNG");
            CHECK(len > 8, "file non vuoto");
            remove(path);
        }
        CHECK(gmb_layers_screenshot(NULL, W, H) == -1, "path NULL = -1");
        CHECK(gmb_layers_screenshot(path, 0, H) == -1, "larghezza 0 = -1");
    }

    /* ---- 11. PROPRIETA' DELLA COMPOSIZIONE: i layer scrivono in una PAGINA ---- */
    {
        int cvs = gmb_gfx_canvas_new(W, H);
        int L;
        CHECK(cvs >= 1, "canvas headless creato (la pagina in cui si compone)");
        CHECK(gmb_gfx_canvas_use(cvs) == 0, "canvas attivo = pagina attiva");
        gmb_layers_reset();
        gmb_layers_set_backdrop(0x102030u);
        L = gmb_layers_create(GMB_LAYER_GRAPHICS, W, H, 0);
        CHECK(L == 1, "layer creato dopo il reset");
        CHECK(gmb_layers_set_palette_entry(L, 1, 0x00FF00u) == 0, "palette del layer");
        CHECK(gmb_layers_set(L, 2, 3, 1) == 0, "indice scritto");
        CHECK(gmb_layers_blit_page() == 1, "blit: compone nella pagina attiva");
        CHECK((gmb_gfx_canvas_get(cvs, 0, 0) & 0xFFFFFF) == 0x102030,
              "backdrop finito nella pagina, non in uno stato parallelo");
        CHECK((gmb_gfx_canvas_get(cvs, 2, 3) & 0xFFFFFF) == 0x00FF00,
              "indice del layer risolto con la SUA palette, dentro la pagina");
        CHECK(gmb_layers_blit_page() == 0, "secondo blit: niente da fare (dirty azzerato)");
        CHECK((gmb_gfx_canvas_get(cvs, 2, 3) & 0xFFFFFF) == 0x00FF00, "la pagina resta quella");
        gmb_layers_set(L, 4, 4, 1);
        CHECK(gmb_layers_blit_page() == 1, "una scrittura sporca di nuovo il frame");
        gmb_gfx_canvas_use(0);
        CHECK(gmb_layers_blit_page() == -1, "senza pagina attiva (nessun SCREEN) = -1");
        gmb_gfx_canvas_free(cvs);
    }

    /* ---- 12. reset: tutto libero, riparte da zero ---- */
    CHECK(gmb_layers_reset() == 0, "reset");
    CHECK(gmb_layers_count() == 0, "nessun layer dopo il reset");
    CHECK(gmb_layers_touched() == 0, "e nessuno sporco");
    CHECK(gmb_layers_backdrop() == 0, "backdrop al default");

    printf("P7-LAYERS ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
