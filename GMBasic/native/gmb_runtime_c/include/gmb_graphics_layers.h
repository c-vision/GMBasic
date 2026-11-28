/* gmb_graphics_layers.h — P7: i layer 2D del riferimento (Engine1999 §graphics-subsystem).
 *
 * Il riferimento compone lo schermo da cinque layer in priorita' — background, tile, text,
 * graphics, sprites — ognuno un bitmap di INDICI di palette con una palette ARGB propria,
 * priorita' scambiabili, backdrop sotto tutto e ridisegno dei soli layer modificati.
 *
 * PROPRIETA' DELLA COMPOSIZIONE (deciso il 2026-11-28). Le PAGINE sono l'unico proprietario
 * della composizione: primitive, sprite, SCENE PRESENT, CANVAS BLIT e SCREENSHOT convergono
 * tutti in `g_gfx.pages[]` / `g_gfx.canvases[]`. I layer NON compongono lo schermo: sono una
 * superficie INDICIZZATA della famiglia grafica che, come un canvas, deposita il risultato in
 * una pagina con `gmb_layers_blit_page()`. Un solo percorso: un layer si vede solo passando
 * da una pagina.
 *
 * Perche' non "le pagine diventano backend dei layer": le pagine sono ARGB a colore diretto
 * (ogni primitiva risolve il colore in `resolve_color`), mentre un layer e' un bitmap di
 * indici con palette PROPRIA. Fare dei layer il backend delle pagine costringerebbe o a un
 * modo indice in tutte le primitive (due rappresentazioni nello stesso percorso) o a perdere
 * la palette per layer, cioe' il motivo per cui il layer esiste. La pagina resta il target;
 * il layer e' una superficie che ci scrive sopra, come un canvas.
 *
 * Formato: ARGB 0x00RRGGBB come le pagine 2D e il target 3D (specifica 3D §11). L'alpha
 * della palette non si usa: la trasparenza e' l'indice 0 di un layer non opaco.
 *
 * Handle: 1..GMB_LAYERS_MAX (`0` non e' un layer). `gmb_layers_create` prende il primo
 * libero, quindi gli id restano stabili finche' quel layer non viene liberato.
 */
#ifndef GMB_GRAPHICS_LAYERS_H
#define GMB_GRAPHICS_LAYERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_LAYERS_MAX 8
#define GMB_LAYERS_PALETTE 256

/* I cinque kind del riferimento, nello stesso ordine di priorita' di default:
 * background disegnato per primo, sprites per ultimo (sopra). */
typedef enum {
    GMB_LAYER_BACKGROUND = 0,
    GMB_LAYER_TILE       = 1,
    GMB_LAYER_TEXT       = 2,
    GMB_LAYER_GRAPHICS   = 3,
    GMB_LAYER_SPRITES    = 4,
    GMB_LAYER_KINDS      = 5
} gmb_layer_kind_t;

/* Libera tutti i layer. Idempotente: 0 = fatto. */
int gmb_layers_reset(void);

/* Crea un layer `kind` di w×h indici.
 * `opaque` = 1: l'indice 0 e' un colore come gli altri (palette[0]);
 *           0: l'indice 0 e' trasparente e lascia vedere i layer sotto.
 * Priorita' iniziale = kind (0..4), cosi' lo stack di default e' quello del riferimento.
 * Ritorna l'handle (>=1) o -1 (kind fuori intervallo, dimensioni <= 0, nessuno slot libero). */
int gmb_layers_create(int kind, int w, int h, int opaque);

/* Libera un layer. 0 = fatto, -1 = handle ignoto. */
int gmb_layers_free(int id);

int gmb_layers_count(void);
int gmb_layers_kind(int id);     /* kind del layer, -1 se ignoto */
int gmb_layers_width(int id);    /* -1 se ignoto */
int gmb_layers_height(int id);   /* -1 se ignoto */

/* Palette del layer: `rgb` = n colori 0x00RRGGBB, presi da index 0 in su (n <= 256).
 * La palette e' PER LAYER: due layer possono avere lo stesso indice con colori diversi.
 * Il default e' la scala di grigi 0x000000..0xFFFFFF (i tre canali uguali all'indice),
 * cosi' un layer senza palette dichiarata resta deterministico e leggibile.
 * Ritorna il numero di voci scritte, -1 su argomenti non validi. */
int gmb_layers_set_palette(int id, const uint32_t* rgb, int n);

/* Una voce di palette. index 0..255, rgb 0x00RRGGBB. 0 = fatto, -1 = argomenti invalidi. */
int gmb_layers_set_palette_entry(int id, int index, uint32_t rgb);
uint32_t gmb_layers_palette_entry(int id, int index);   /* 0xFFFFFFFF se ignoto */

/* Riempie il layer con `index` (0..255). 0 = fatto, -1 = argomenti invalidi. */
int gmb_layers_clear(int id, int index);

/* Scrive/legge un indice. Fuori dal rettangolo: set -1 (nessuna scrittura), get -1.
 * Una scrittura che non cambia il valore NON marca il layer come modificato. */
int gmb_layers_set(int id, int x, int y, int index);
int gmb_layers_get(int id, int x, int y);

/* Priorita': si disegna in ordine CRESCENTE (0 sotto, numeri alti sopra). A parita' di
 * priorita' vince l'ordine di creazione (il piu' vecchio primo: composizione stabile). */
int gmb_layers_set_priority(int id, int priority);
int gmb_layers_priority(int id);

/* Backdrop: il colore che riempie TUTTO il frame prima dei layer (0 di default = nero). */
int gmb_layers_set_backdrop(uint32_t rgb);
uint32_t gmb_layers_backdrop(void);

/* Marca il layer come modificato / dice se qualcuno lo e' (dal-l'ultima composizione). */
int gmb_layers_touch(int id);
int gmb_layers_touched(void);

/* Compone i layer in `out` (w*h pixel 0x00RRGGBB, riga 0 in alto).
 * `composite` compone sempre; `composite_dirty` compone solo se qualche layer e'
 * modificato e ritorna 0 senza toccare `out` se non c'e' niente da fare.
 * La composizione azzera i flag di modifica. Nessuna allocazione.
 * Ritorna 1 = composto, 0 = niente da fare (solo _dirty), -1 = argomenti invalidi. */
int gmb_layers_composite(uint32_t* out, int w, int h);
int gmb_layers_composite_dirty(uint32_t* out, int w, int h);

/* ---- Unico ponte verso lo schermo ----
 * Compone (solo se qualche layer e' sporco) nella pagina ATTIVA: il canvas attivo se c'e',
 * altrimenti la pagina dello schermo. Coordinate 1:1 come le primitive su canvas: niente
 * VIEW/WINDOW e niente render-scale, quindi su Retina la pagina e' nativa e il layer va
 * creato alla dimensione nativa (oppure si compone in un canvas e si fa CANVAS BLIT).
 * Il backdrop copre tutta la pagina: il blit SOSTITUISCE il contenuto dell'area, e' un
 * frame intero. 1 = composto, 0 = niente da fare (pagina intatta), -1 = nessuna pagina
 * attiva (SCREEN mai aperto) o pagina senza pixel. Nessuna allocazione.
 */
int gmb_layers_blit_page(void);

/* Compone e salva un PNG (rgb24, il codificatore di gmb_graphics_screenshot.c: una sola
 * implementazione, §0bis). E' il percorso di VERIFICA headless, non quello di presentazione:
 * per mostrare i layer si usa `gmb_layers_blit_page` + lo screenshot della pagina.
 * 0 = fatto, -1 = errore (argomenti o file). */
int gmb_layers_screenshot(const char* path, int w, int h);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GRAPHICS_LAYERS_H */
