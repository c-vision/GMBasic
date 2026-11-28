/* ============================================================================
 * gmb_color_filter.h — filtro colore per la DALTONIA (color vision deficiency).
 *
 * È una voce del registro opzioni, come i flag di accessibilità:
 *
 *     AI SET OPTION "ui.color_filter", "deuteranopia"
 *
 * Valori: "" / "none" (default, nessuna trasformazione), "deuteranopia",
 * "protanopia", "tritanopia", "achromatopsia".
 *
 * La trasformazione è una matrice 3x3 applicata all'RGB (matrici di Machado,
 * Oliveira & Fernandes 2009, al grado 1.0): è una funzione PURA e deterministica,
 * quindi verificabile senza disegnare nulla. Si applica a TUTTI i colori che il
 * runtime emette — celle TUI, sink pixel, ruoli del tema, primitive grafiche —
 * così l'interfaccia resta leggibile anche dove il programma ha scelto i colori
 * a mano.
 *
 * Con il filtro spento non si perde nulla: `gmb_color_filter_active()` = 0 e i
 * chiamanti non fanno niente (nessun costo nel percorso comune). Una volta
 * attivo, `gmb_color_filter_rgb()` è O(1) e senza allocazioni.
 *
 * Nota: il colore "default" del terminale (is_default) NON si filtra — non è un
 * valore RGB noto, è la scelta del terminale, e alterarlo cambierebbe la
 * leggibilità di ciò che il runtime non ha deciso.
 * ============================================================================ */
#ifndef GMB_COLOR_FILTER_H
#define GMB_COLOR_FILTER_H

#include <stdint.h>
#include "gmb_tui.h"   /* gmb_cell_color_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Voce del registro opzioni (`AI LIST OPTIONS`). */
#define GMB_COLOR_FILTER_OPT "ui.color_filter"

/* Modi canonici. La numerazione NON è un contratto: si usa il nome. */
enum {
    GMB_CVF_NONE = 0,       /* default: nessuna trasformazione */
    GMB_CVF_DEUTERANOPIA,   /* verde (deficiency più diffusa) */
    GMB_CVF_PROTANOPIA,     /* rosso */
    GMB_CVF_TRITANOPIA,     /* blu */
    GMB_CVF_ACHROMATOPSIA   /* nessun colore (luminanza) */
};

/* Nome → modo (case-insensitive; accetta i prefissi "deut", "prot", "trit",
 * "achrom", "gray"/"grey"/"mono"). Ritorna -1 se il nome è sconosciuto. */
int gmb_color_filter_parse(const char* name);

/* Nome canonico del modo ("none", "deuteranopia", ...). */
const char* gmb_color_filter_name(int mode);

/* Modo ATTIVO adesso (letto dal registro con cache sull'ultimo valore visto). */
int gmb_color_filter_mode(void);

/* 1 se il filtro è acceso (modo != none). È il test da mettere nel percorso caldo. */
int gmb_color_filter_active(void);

/* Hook opzionale "il modo è cambiato": il layer UI ridisegna il frame. Con
 * NULL (nessuno registrato) il filtro funziona lo stesso, solo al frame dopo. */
void gmb_color_filter_on_change(void (*fn)(void));

/* Forza il modo senza passare dal registro (per test e per chi imposta l'opzione
 * a runtime senza thread-safe). mode<0 = torna a leggere il registro. */
void gmb_color_filter_set(int mode);

/* Trasformazione PURA: applica la matrice del modo a un colore RGB.
 * `mode` fuori intervallo o NONE = copia; `is_default` = copia. */
gmb_cell_color_t gmb_color_filter_cell(gmb_cell_color_t c, int mode);

/* Come sopra in formato 0xRRGGBB (le palette gfx e gli stili del tema). */
uint32_t gmb_color_filter_rgba(uint32_t rgb, int mode);
uint32_t gmb_color_filter_u32(unsigned int rgb, int mode);

/* Versione "col modo attivo" (zero rami di lettura registro nei chiamanti):
 * usano la cache interna. */
gmb_cell_color_t gmb_color_filter_cell_now(gmb_cell_color_t c);
uint32_t gmb_color_filter_rgba_now(uint32_t rgb);

#ifdef __cplusplus
}
#endif
#endif /* GMB_COLOR_FILTER_H */